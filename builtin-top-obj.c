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
#include "parse.h"
#include "builtin-top-obj.h"

/*
 * Show top objects 
 */
static int nb_plt = 0, nb_non_plt = 0;
static int nb_total_access = 0;
static rbtree r,r2;
static compare_func cmp = pointer_cmp;
void top_obj_init() {
   r = rbtree_create();
   r2 = rbtree_create();
   printf("#'<object uid>-<object allocator function>' - <#access-to-object> [<#distant-accesses> <%% local>] [<%%-of-total-access-that-are-on-the-obj>] [<#access-avoidable-by-pinning-threads> <%%-of access to the obj>] [<#access-avoidable-by-allocating on different node> <%%>]\n");
   printf("#\tNODES [<number of access performed FROM the node (one fig per node)>+]");
   printf("]\tHOSTED ON [<number of accesses performed TO the node (one fig per node)>+]");
   printf("] ALLOCATED BY SELF [<%% of accesses done by the thread that allocated the object> (#access/#total access to the obj) (<# access done by the allocating thread before any other thread accessed the object)]\n\n");
}

void top_obj_modifier(int m) {
   if(m == 1)
      cmp = pointer_cmp;
   else if(m == 2)
      cmp = (compare_func)strcmp;
   else
      die("Modifier %d not supported\n", m);
}

struct value {
   uint32_t uid;
   uint32_t accesses;
   uint32_t dist_accesses;
   uint32_t *from_accesses;
   uint32_t *to_accesses;
   uint32_t dist_by_allocator;
   uint32_t dist_by_allocator_remote_cpu;
   uint32_t dist_by_allocator_alloc_cpu;
   uint32_t dist_for_obj;
   uint32_t by_allocator;
   uint32_t by_everybody;
   uint32_t by_allocator_before_everybody;
};

void top_obj_parse(struct s* s) {
   struct symbol *sym = get_function(s);
   if(strstr(sym->function_name, "plt")) {
      nb_plt++;
   }
   else
   {
      nb_non_plt++;
      struct symbol *ob = get_object(s);
      struct dyn_lib* ob3 = sample_to_mmap(s);
      char *obj = NULL;
      if(ob)
         obj = ob->object_name;
      if(!obj && strstr(sym->function_name, "@plt"))
         obj = sym->function_name;
       if(!obj && !strcmp(sym->function_name, "[vdso]"))
         obj = sym->function_name;
      if(!obj && ob3) 
         obj = ob3->name;
      struct value *value = rbtree_lookup(r, obj, cmp);
      if(!value) {
         value = calloc(1, sizeof(*value));
         value->from_accesses = calloc(max_node, sizeof(*value->from_accesses));
         value->to_accesses = calloc(max_node, sizeof(*value->to_accesses));
         rbtree_insert(r, obj, value, cmp);
      }
      value->accesses++;
      value->dist_accesses += is_distant(s);
      value->from_accesses[cpu_to_node(s->cpu)]++;
      value->to_accesses[get_addr_node(s)]++;
      if(ob) {
         value->dist_by_allocator += (is_distant(s) && (get_tid(s) == ob->allocator_tid));
         value->dist_by_allocator_remote_cpu += (is_distant(s) && (get_tid(s) == ob->allocator_tid) && (ob->allocator_cpu != s->cpu));
         value->dist_by_allocator_alloc_cpu += (is_distant(s) && (get_tid(s) == ob->allocator_tid) && (ob->allocator_cpu == s->cpu));
         value->dist_for_obj += (is_distant(s));
   
         value->by_allocator += ((get_tid(s) == ob->allocator_tid));
         value->by_everybody += ((get_tid(s) != ob->allocator_tid));

         value->by_allocator_before_everybody += (value->by_everybody == 0);
         value->uid = ob->uid;
      }
      nb_total_access++;
   }
}

static int distant = 0, pin_avoid = 0, bad_alloc = 0, alloc_self = 0, total_alloc = 0;
static int print_static(void *key, void *value) {
   struct value *v = value;
   printf("'%5d-%25s' - %d [%d %d%% local] [%.2f%% total] [%d %.2f pin] [%d %.2f badalloc]\n", 
         v->uid, (char*)(key), v->accesses, //%d-%s = object uid + name, #accesses
         v->dist_accesses, 100*(v->accesses-v->dist_accesses)/(v->accesses), // #distants accesses (%local)
         100*(float)((float)v->accesses)/((float)nb_total_access),  // % of all accesses performed on that object
         v->dist_by_allocator_remote_cpu, 100*(float)((float)v->dist_by_allocator_remote_cpu)/((float)v->dist_by_allocator), // #accesses by allocator  on a cpu != allocating cpu (%)
         v->dist_by_allocator_alloc_cpu, 100*(float)((float)v->dist_by_allocator_remote_cpu)/((float)v->dist_by_allocator_alloc_cpu)); // #accesses by allocator on cpu == allocating cpu (hence memory was allocated on a remote node) (%)

   printf("\tNODES [");
   int i;
   for(i = 0; i < max_node; i++) {
      printf("%d ", v->from_accesses[i]); // accesses from node i to the object
   }
   printf("]\tHOSTED ON [");
   for(i = 0; i < max_node; i++) {
      printf("%d ", v->to_accesses[i]);   // accesses to node i (object may be on multiple nodes - because its big or because of migrations)
   }
   printf("] ALLOCATED BY SELF [%2.f%% (%d/%d) (%d before)]\n\n", (float)100.*((float)v->by_allocator)/((float)(v->by_everybody+v->by_allocator)), v->by_allocator, (v->by_everybody + v->by_allocator), v->by_allocator_before_everybody);  // % of accesses done by the thread that allocated the object (allocator / total accesses), % of accesses done by the allocator before any other thread accessed the object

   distant += v->dist_accesses;
   pin_avoid += v->dist_by_allocator;
   bad_alloc += v->dist_by_allocator_alloc_cpu;
   alloc_self += v->by_allocator;
   total_alloc += v->by_everybody + v->by_allocator;
   return 0;
}
static __unused int static_cmp(const void *a, const void* b) {
   const rbtree_node _a = *(const rbtree_node*) a;
   const rbtree_node _b = *(const rbtree_node*) b;
   return *(int*)_b->value - *(int*)_a->value;
}
void top_obj_show() {
   printf("PLT: %d\tNONPLT: %d\n", nb_plt, nb_non_plt);
   rbtree_key_val_arr_t *sorted = rbtree_sort(r, static_cmp);
   int i;
   for(i = 0; i < sorted->nb_elements; i++) {   
      print_static(sorted->vals[i]->key, sorted->vals[i]->value);
   }
   printf("Distant = %d (%.2f%%) avoidable by pining = %d (%.2f%%) and by alloc = %d (%.2f%%); %% of access to objects allocated by self=%5.2f%% (%d)\n", 
         distant,
         100.*(float)((float)distant)/((float)nb_total_access),
         pin_avoid,
         100.*(float)((float)pin_avoid)/((float)distant),
         bad_alloc,
         100.*(float)((float)bad_alloc)/((float)distant),
         100.*(float)((float)alloc_self)/((float)total_alloc),
         alloc_self
         );
}

