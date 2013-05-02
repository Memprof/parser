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
#include "data.h"
#include "list.h"
#include "malloc.h"
#include "symbols.h"
#include "process.h"

static struct list *data_ev_list = NULL;
extern int print_mmap(void *key, void *value);

void read_data_events(char *mmaped_file) {
   /** Header **/
   FILE *data = open_file(mmaped_file);
   if(!data) {
      printf("#Warning: data file %s not found\n", mmaped_file);
      return;
   }

   struct malloc_event *e;
   while(1) {
      e = malloc(sizeof(*e) + MAX_CALLCHAIN*sizeof(uint64_t));

      int n = fread(e, sizeof(*e), 1, data);
      if(!n)
         break;
      
      if(e->callchain_size) {
         n = fread(e->callchain, e->callchain_size*sizeof(uint64_t), 1, data);
         if(!n)
            die("Cannot read callchain (size=%lu)\n", (long unsigned)e->callchain_size);
      }
   
      //e->rdt = 0;
      switch(e->type) {
         case 0: //free
         case 2: //unmap
            //Currently free and unmap is not handled. We don't care.
            free(e);
            break;
         default:
            data_ev_list = list_add(data_ev_list, e);
      }
   }
}

/**
struct malloc_event {
   int ev_size;
   uint64_t rdt;
   uint64_t addr;
   int tid;
   int pid;
   int cpu;
   int alloc_size;
   int type;
   int callchain_size;
   uint64_t callchain[];
};
**/
void add_alloc_ev(struct malloc_event *m);
void process_data_samples(uint64_t time) {
   while(data_ev_list && (time >= ((struct malloc_event*)data_ev_list->data)->rdt)) {
      struct malloc_event *e = ((struct malloc_event*)data_ev_list->data);
      add_alloc_ev(e);
      data_ev_list = list_next(data_ev_list);
   }
}

static int alloc_evts = 0;
extern struct dyn_lib* anon_lib;
extern struct mmapped_dyn_lib* _ip_to_mmap(rbtree_node n, void *ip);
void add_alloc_ev(struct malloc_event *m) {
   process_init();

   int i;
   struct process *p = find_process_safe(m->pid);

   struct symbol *s = calloc(1, sizeof(*s) + m->callchain_size*sizeof(uint64_t));
   s->uid = alloc_evts;
   s->ip = (void*)m->addr;
   s->size = m->alloc_size;
   s->tid = m->tid;
   s->cpu = m->cpu;
   s->file = anon_lib;

   rbtree_node n = p->mmapped_dyn_lib->root;
   struct mmapped_dyn_lib *lib = _ip_to_mmap(n, (void*)m->alloc_ip);
   if(!lib || lib->end < (uint64_t)m->alloc_ip) {
      asprintf(&s->function, "[unknownlib]+%p", (void*) (m->alloc_ip) );
   } else {
      struct symbol *sym = ip_to_symbol(lib->lib, (void*) (m->alloc_ip - lib->begin + lib->off));
      if(!sym) {
         asprintf(&s->function, "%s:?+%p", lib->lib->name, (void*) (m->alloc_ip - lib->begin + lib->off) );
      } else {
         /*if(!strcmp(sym->function, "makeMatrix"))
            printf("IP=%p\n", (void*)m->alloc_ip);*/
         asprintf(&s->function, "%s+%p", sym->function, (void*) (m->alloc_ip - lib->begin + lib->off) - (uint64_t)sym->ip);
      }
   }

   s->callchain_size = m->callchain_size;
   for(i = 0; i < s->callchain_size; i++) {
      s->callchain[i] = m->callchain[i];
   }


   rbtree_insert(p->allocated_obj, s->ip, s, pointer_cmp_reverse);
   alloc_evts++;

   free(m);
}

static __unused int symb_print(void *key, void* value) {
   printf("%p-%p: %s\n", key, (void*)(uint64_t) key+((struct symbol*)value)->size, ((struct symbol*)value)->function);
   return 0;
}

struct symbol* _va_to_symbol(rbtree_node n, void *ip) {
   while (n != NULL) {
      if(n->left)
         assert(n->key > n->left->key);
      if(n->right)
         assert(n->key < n->right->key);

      if(ip < n->key) {
         n = n->left;
      } else {
         if(n->right && ip >= n->right->key) {
            n = n->right;
         } else {
            /* Look in the right subtree for a better candidate */
            struct symbol *best_son = _va_to_symbol(n->right, ip);
            return best_son?best_son:n->value;
         }
      }
   }
   return NULL;
}
struct symbol* sample_to_variable2(struct s* s) {
   struct process *p = find_process(get_pid(s));
   if(!p) {
      return NULL;
   }


   rbtree_node n = p->allocated_obj->root;
   struct symbol* ss = _va_to_symbol(n, (void*)s->ibs_dc_linear);

   /*if(1 && (!ss || ((uint64_t)ss->ip + ss->size < s->ibs_dc_linear))) {
      struct symbol *ss = get_symbol(s);
      struct dyn_lib *lib = sample_to_mmap(s);
      if(!strcmp(lib->name, "//anon")) {
         printf("Req %p in %s\n", s->ibs_dc_linear, ss->function);
         rbtree_print(p->allocated_obj, symb_print);
         printf("\n\n");
         rbtree_print(p->mmapped_dyn_lib, print_mmap);
      }
   }*/

   if(!ss || ((uint64_t)ss->ip + ss->size < s->ibs_dc_linear))
      return NULL;
   return ss;
}
char* sample_to_variable(struct s* s) {
   struct symbol* ss = sample_to_variable2(s);
   return ss?ss->function:"[unknown]";
}
void show_data_stats() {
}

