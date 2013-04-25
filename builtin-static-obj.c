#include "parse.h"
#include "builtin-static-obj.h"

static int nb_plt = 0, nb_non_plt = 0;
static int nb_pages_plt_memcpy = 0;
static int nb_total_access = 0;
static void *last_memcpy = NULL;
static rbtree r,r2;
static compare_func cmp = pointer_cmp;
void static_obj_init() {
   r = rbtree_create();
   r2 = rbtree_create();
}

void static_obj_modifier(int m) {
   if(m == 1)
      cmp = pointer_cmp;
   else if(m == 2)
      cmp = (compare_func)strcmp;
   else
      die("Modifier %d not supported\n", m);
}

void static_obj_parse(struct s* s) {
   /*if(is_kernel(s))
      printf("%d -> %d\n", s->linear_valid & 3, s->object_num);*/
   struct symbol *sym = get_symbol(s);
   struct symbol *ob = sample_to_static_object(s); //unsafe, may return NULL ; static object
   struct symbol *ob2 = sample_to_variable2(s);
   if(strstr(sym->function, "plt")) {
      nb_plt++;
      if(strstr(sym->function, "memcpy")) {
         if((s->ibs_dc_phys & PAGE_MASK) != (uint64_t)last_memcpy) {
            last_memcpy = (void*)(s->ibs_dc_phys & PAGE_MASK);
            if(!rbtree_lookup(r2, last_memcpy, pointer_cmp)) {
               //printf("memcpy @ %p @ %p\n", last_memcpy, s->ibs_dc_linear);
               rbtree_insert(r2, last_memcpy, (void*)1, pointer_cmp);
               nb_pages_plt_memcpy++;
            }
         }
      }
   }
   // else
   {
      nb_non_plt++;
      struct dyn_lib* ob3 = sample_to_mmap(s);
      char *obj = NULL;
      if(ob2)
         obj = ob2->function;
      if(!obj && ob)
         obj = ob->function;
      if(!obj && strstr(sym->function, "@plt"))
         obj = sym->function;
       if(!obj && !strcmp(sym->function, "[vdso]"))
         obj = sym->function;
      if(!obj && ob3) {
         obj = ob3->name;
      }
      int *value = rbtree_lookup(r, obj, cmp);
      if(!value) {
         value = calloc(6 + 8 + 4 + 1,sizeof(int));
         rbtree_insert(r, obj, value, cmp);
      }
      value[0] = value[0] + 1;
      value[1] = value[1] + is_distant(s);
      if(ob2) {
         value[2] = value[2] + (is_distant(s) && (get_tid(s) == ob2->tid));
         value[3] = value[3] + (is_distant(s) && (get_tid(s) == ob2->tid) && (ob2->cpu != s->cpu));
         value[4] = value[4] + (is_distant(s) && (get_tid(s) == ob2->tid) && (ob2->cpu == s->cpu));
         value[5] = value[5] + (is_distant(s));
   
         value[14] = value[14] + ((get_tid(s) == ob2->tid));
         value[15] = value[15] + 1;

         value[16] = value[16] + (value[15] == 0)*((get_tid(s) == ob2->tid));
         value[17] = value[17] + (value[14] == 0);
         value[18] = ob2->uid;
      }
      value[6 + cpu_to_node(s->cpu)]++;
      value[10 + get_addr_node(s)]++;
      nb_total_access++;
   }
}

static int distant = 0, pin_avoid = 0, pin_ok = 0, bad_alloc = 0, alloc_self = 0, total_alloc = 0;
static int print_static(void *key, void *value) {
   int *v = (int*)value;
   printf("'%5d-%25s' - %d [%d %d%% local] [%.2f%% total] [%d %.2f pin] [%d %.2f badalloc]\n", 
         v[18], (char*)(key), v[0], //%d-%s = object uid + name, #accesses
         v[1], 100*(v[0]-v[1])/(v[0]), // #distants accesses (%)
         100*(float)((float)v[0])/((float)nb_total_access),  // % of all accesses performed on that object
         v[3], 100*(float)((float)v[3])/((float)v[2]), // #accesses by allocator  on a cpu != allocating cpu (%)
         v[4], 100*(float)((float)v[3])/((float)v[4])); // #accesses by allocator on cpu == allocating cpu (hence memory was allocated on a remote node) (%)

   printf("\tNODES [");
   int i;
   for(i = 0; i < 4; i++) {
      printf("%d ", v[6+i]);
   }
   printf("]\tHOSTED ON [");
   for(i = 0; i < 4; i++) {
      printf("%d ", v[10+i]);
   }
   printf("] ALLOCATED BY SELF [%2.f%% (%d/%d) (%d before)]\n\n", (float)100.*((float)v[14])/((float)v[15]), v[14], v[15], v[17]);

   distant += v[1];
   pin_avoid += v[3];
   pin_ok += v[5];
   bad_alloc += v[4];
   alloc_self += v[14];
   total_alloc += v[15];
   return 0;
}
static __unused int static_cmp(const void *a, const void* b) {
   const rbtree_node _a = *(const rbtree_node*) a;
   const rbtree_node _b = *(const rbtree_node*) b;
   return *(int*)_b->value - *(int*)_a->value;
}
void static_obj_show() {
   printf("PLT: %d\tNONPLT: %d\n", nb_plt, nb_non_plt);
   printf("NB_MEMCPY: %d\n", nb_pages_plt_memcpy);
   rbtree_key_val_arr_t *sorted = rbtree_sort(r, static_cmp);
   int i;
   for(i = 0; i < sorted->nb_elements; i++) {   
      print_static(sorted->vals[i]->key, sorted->vals[i]->value);
   }
   printf("Distant = %d (%.2f%%) avoidable by pining = %d (%.2f%%) and by alloc = %d (%.2f%%) %d dist ob2 ; %% of access to objects allocated by self=%5.2f%% (%d)\n", 
         distant,
         100.*(float)((float)distant)/((float)nb_total_access),
         pin_avoid,
         100.*(float)((float)pin_avoid)/((float)distant),
         bad_alloc,
         100.*(float)((float)bad_alloc)/((float)distant),
         pin_ok,
         100.*(float)((float)alloc_self)/((float)total_alloc),
         alloc_self
         );

}

