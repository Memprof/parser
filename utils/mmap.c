/*
Copyright (C) 2013  
Baptiste Lepers <baptiste.lepers@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "../parse.h"
#include "mmap.h"
/**
 * TODO: if some day we need performance, remove_overlap can be easily improved by using the
 * fact that existing mmaps don't overlap and are sorted by start ip => no need to
 * go through the whole tree.
 * We can also reduce the number of inserts/deletes and calloc by shrinking areas 
 * rather than deleting them.
 * But for now it is fine.
 */

/** Variables **/
static int mmap_evts = 0;
static __unused struct dyn_lib unknown_lib = {
   .name = "[unknown-lib]",
};
static __unused struct dyn_lib unknown_pid = {
   .name = "[unknown-pid]",
};
static struct symbol unknown = {
   .ip = NULL,
   .function_name = "[unknown]",
};
static struct sample_to_symbol_stats {
   int nb_success;
   int nb_fail_pid;
   int nb_fail_lib;
   int nb_fail_sym;
} sample_to_symbol_stats;
extern int processed_perf_samples;
extern int total_perf_samples;


/** Debug functions **/
int print_mmap(void *key, void *value) {
   printf("%p-%p off %p: %s\n", key,
         (void*)((struct mmapped_dyn_lib *)value)->end,
         (void*)((struct mmapped_dyn_lib *)value)->off,
         ((struct mmapped_dyn_lib *)value)->lib->name);
   return 0;
}


/** Prototypes **/
struct mmapped_dyn_lib* _ip_to_mmap(rbtree_node n, void *ip);



/** Code **/
static int mmap_overlap(struct mmapped_dyn_lib *m, struct mmapped_dyn_lib *n) {
   if(m->begin > n->begin) {
      struct mmapped_dyn_lib *t = m;
      m = n;
      n = t;
   }
   return m->end >= n->begin;
}

/* Insert a mmap in the tree while suppressing the overlaps.
 * E.g. [c d] inserted in [a b] -> [a c][c d][d b] */
static int remove_overlap(rbtree t, rbtree_node n, struct mmapped_dyn_lib *m) {
   if(!n)
      return 0;

   struct mmapped_dyn_lib *lib = n->value;

   if(!(m->begin > lib->begin))
      if(remove_overlap(t, n->left, m)) return 1;
   if(!(m->end < lib->begin))
      if(remove_overlap(t, n->right, m)) return 1;

   if(lib && !mmap_overlap(m, lib))
      return 0;

   // Case 1: We are adding //anon exactly on top of an existing library. Ignore because its probably data of the lib.
   if(m->begin - m->off == 0 && m->end == lib->end
         && !strcmp(m->lib->name, "//anon")) { 
      return 1;
   }
   if(m->end <= lib->end && !strcmp(m->lib->name, lib->lib->name)) { //Overload of the same lib, ignore.
      return 1;
   }

   rbtree_delete(t, (void*)lib->begin, pointer_cmp_reverse);
   struct mmapped_dyn_lib *old_lib;
   if(lib->begin < m->begin) {
      old_lib = calloc(1, sizeof(*old_lib));
      old_lib->begin = lib->begin;
      old_lib->end = m->begin - 1;
      old_lib->off = old_lib->off;
      old_lib->lib = lib->lib;
      if(verbose)
         fprintf(stdout, "-> (1) adding: from %p to %p\n", (void*)old_lib->begin, (void*)old_lib->end);
      rbtree_insert(t, (void*)old_lib->begin, old_lib, pointer_cmp_reverse);
   }
   if(lib->end > m->end) {
      old_lib = calloc(1, sizeof(*old_lib));
      old_lib->begin = m->end + 1;
      old_lib->end = lib->end;
      old_lib->off = lib->off + (m->end + 1 - lib->begin);
      old_lib->lib = lib->lib;
      if(verbose)
         fprintf(stdout, "-> (2) adding: from %p to %p with offset %p\n", (void*)old_lib->begin, (void*)old_lib->end, (void*)old_lib->off);
      rbtree_insert(t, (void*)old_lib->begin, old_lib, pointer_cmp_reverse);
   }
   free(lib);
   return 0;
}


void add_mmap_event(struct mmap_event *m) {
   struct process *p = find_process_safe(m->pid);

   struct mmapped_dyn_lib *lib = calloc(1, sizeof(*lib));
   lib->uid = mmap_evts;
   lib->begin = m->start;
   lib->end = m->start + m->len;
   lib->off = m->pgoff;
   if(m->len == 8392704) { /** Guess that all mmap of that size correspond to a stack. TO BE CONFIRMED. */
      lib->lib = find_lib("[stack]");
   } else
      lib->lib = find_lib(m->file_name);

   int dont_add = remove_overlap(p->mmapped_dyn_lib, p->mmapped_dyn_lib->root, lib);
   if(!dont_add)
      rbtree_insert(p->mmapped_dyn_lib, (void*)m->start, lib, pointer_cmp_reverse);

   mmap_evts++;
}

struct mmapped_dyn_lib* _ip_to_mmap(rbtree_node n, void *ip) {
   while (n != NULL) {
      if(n->left)
         assert(n->key > n->left->key);
      if(n->right)
         assert(n->key < n->right->key);

      if(ip < n->key) {
         //printf("%p < %p left\n", ip, n->key);
         n = n->left;
      } else {
         if(n->right && ip >= n->right->key) {
            //printf("%p >= %p right\n", ip, n->right->key);
            n = n->right;
         } else {
            //printf("%p >= %p bestson\n", ip, n->key);
            /* Look in the right subtree for a better candidate */
            struct mmapped_dyn_lib *best_son = _ip_to_mmap(n->right, ip);
            return best_son?best_son:n->value;
         }
      }
   }
   return NULL;
}

struct symbol* sample_to_function(struct s* s) {
   process_init();

   if(!is_kernel(s)) {
      struct process *p = find_process(get_pid(s));
      if(!p) {
         unknown.file = &unknown_pid;
         sample_to_symbol_stats.nb_fail_pid++;
         return &unknown;
      }

      rbtree_node n = p->mmapped_dyn_lib->root;
      struct mmapped_dyn_lib *lib = _ip_to_mmap(n, (void*)s->rip);
      if(!lib) {
         sample_to_symbol_stats.nb_fail_lib++;
         unknown.file = &unknown_lib;
         return &unknown;
      }
      if(lib->end < s->rip) {
         sample_to_symbol_stats.nb_fail_lib++;
         unknown.file = &unknown_lib;
         return &unknown;
      }
      
      struct symbol *sym = ip_to_symbol(lib->lib, (void*) (s->rip - lib->begin + lib->off));
      if(!sym) {
         sample_to_symbol_stats.nb_fail_sym++;
         unknown.file = lib->lib;
         return &unknown;   
      }

      /** SASHA: Here we want to record one more field in our symbol struct:
       * the offset into the function that performed the access. 
       */
      //sym->func_offset =  (s->rip - lib->begin + lib->off) - (uint64_t)sym->ip;

      sample_to_symbol_stats.nb_success++;
      return sym;
   } else {
      struct dyn_lib *lib = find_lib(KERNEL_NAME);
      struct symbol *sym = ip_to_symbol(lib, (void*)s->rip);
      if(!sym) {
         sample_to_symbol_stats.nb_fail_sym++;
         unknown.file = lib;
         return &unknown;   
      }
      sample_to_symbol_stats.nb_success++;
      return sym;
   }
}

struct mmapped_dyn_lib* sample_to_mmap2(struct s* s) {
   process_init();

   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   __unused ibs_op_data3_t *data3 = (void*)&udata3;
   if(!data3->ibsdclinaddrvalid)
      return NULL;
   
   if(!is_kernel(s)) {
      struct process *p = find_process(get_pid(s));
      if(!p)
         return NULL;
      
      rbtree_node n = p->mmapped_dyn_lib->root;
      struct mmapped_dyn_lib *lib = _ip_to_mmap(n, (void*)s->ibs_dc_linear);
      if(!lib) 
         return NULL;
      if(lib->end < s->ibs_dc_linear) 
         return NULL;
      return lib;
   } else
      return NULL;
}

struct dyn_lib* sample_to_mmap(struct s* s) {
   process_init();

   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   __unused ibs_op_data3_t *data3 = (void*)&udata3;
   if(!data3->ibsdclinaddrvalid)
      return get_invalid_lib();
   
   if(!is_kernel(s)) {
      struct process *p = find_process(get_pid(s));
      if(!p)
         return &unknown_pid;
      
      rbtree_node n = p->mmapped_dyn_lib->root;
      struct mmapped_dyn_lib *lib = _ip_to_mmap(n, (void*)s->ibs_dc_linear);
      if(!lib) 
         return &unknown_lib;
      if(lib->end < s->ibs_dc_linear) 
         return &unknown_lib;

      lib->lib->enclosing_lib = lib;
      return lib->lib;
   } else {
      return find_lib(KERNEL_NAME);
   }
}

/**
 * Global static objects resolution: 
 */
struct symbol* sample_to_static_object(struct s* s) {
   process_init();

   if(!is_kernel(s)) {
      struct process *p = find_process(get_pid(s));
      if(!p) 
         return NULL;

      rbtree_node n = p->mmapped_dyn_lib->root;
      struct mmapped_dyn_lib *lib = _ip_to_mmap(n, (void*)s->ibs_dc_linear);
      if(!lib) 
         return NULL;

      struct symbol *sym = ip_to_symbol(lib->lib, (void*) (s->ibs_dc_linear - lib->begin + lib->off));
      if(!sym || !strcmp(sym->function_name, "//anon")) {
         struct mmapped_dyn_lib *new_lib = _ip_to_mmap(n, (void*)lib->begin - 1);
         if(!new_lib || s->ibs_dc_linear > new_lib->begin + new_lib->lib->real_size) 
            return NULL;
         sym = ip_to_symbol(new_lib->lib, (void*) (s->ibs_dc_linear - new_lib->begin + new_lib->off));
         if(!sym) 
            return NULL;
         lib = new_lib;
      }
      if((s->ibs_dc_linear - lib->begin + lib->off) - (uint64_t)sym->ip > sym->size) {
         return NULL;
      }

      return sym;
   } else {
      return NULL;
   }
}


struct symbol* sample_to_callchain(struct s* s, struct symbol *sym, int index) {
   process_init();

   if(!is_kernel(s)) {
      struct process *p = find_process(get_pid(s));
      rbtree_node n = p->mmapped_dyn_lib->root;
      struct mmapped_dyn_lib *lib = _ip_to_mmap(n, (void*)sym->callchain[index]);
      if(!lib)
         return &unknown;

      struct symbol *ret = ip_to_symbol(lib->lib, (void*) (sym->callchain[index] - lib->begin + lib->off));
      if(!ret)
         return &unknown;
      return ret;
   } else {
      return &unknown;   
   }
}


void show_mmap_stats() {
   if(!(sample_to_symbol_stats.nb_success+sample_to_symbol_stats.nb_fail_pid+sample_to_symbol_stats.nb_fail_lib+sample_to_symbol_stats.nb_fail_sym))
      return;

   if(sample_to_symbol_stats.nb_success == 0)
      sample_to_symbol_stats.nb_success++;

   printf("#Functions sucesses %d (%.2f%%) Fails %d [Unknown pids %d ; Unknown lib %d ; Unknown symbol in lib %d]\n",
         sample_to_symbol_stats.nb_success,
         100.*((float)sample_to_symbol_stats.nb_success)/((float)sample_to_symbol_stats.nb_success+sample_to_symbol_stats.nb_fail_pid+sample_to_symbol_stats.nb_fail_lib+sample_to_symbol_stats.nb_fail_sym),
         sample_to_symbol_stats.nb_fail_pid+sample_to_symbol_stats.nb_fail_lib+sample_to_symbol_stats.nb_fail_sym,
         sample_to_symbol_stats.nb_fail_pid,
         sample_to_symbol_stats.nb_fail_lib,
         sample_to_symbol_stats.nb_fail_sym);
   printf("#Processed perf events : %d / %d\n", processed_perf_samples, total_perf_samples);
}
