#include "parse.h"
#include "builtin-memory-repartition.h"

#define SHOW_MIN_THRES 1

static int *memory_repartition_on_nodes;
static int *sample_repartition_on_cpus;
static int nb_seen_pages = 0;

static rbtree memory_repartition_tree;
static rbtree app_repartition_tree;
static __unused int memory_repartition_cmp(void *left, void *right) {
   if(left > right) {
      if(left - right < PAGE_SIZE)
         return 0;
      return -1;
   } else if(left < right) {
      if(right - left < PAGE_SIZE)
         return 0;
      return 1;
   } else if(left == right) {
      return 0;
   }
   return 0; //Pleases GCC
}
static __unused int memory_repartition_print(void *key, void *value) {
   if(*(int*)value >= SHOW_MIN_THRES)
      printf("%lu: %d\n", (long unsigned)key, *(int*)value);
   return 0;
}

void memory_repartition_parse(struct s* s) {
   if(!memory_repartition_on_nodes)
      memory_repartition_on_nodes = calloc(1, sizeof(*memory_repartition_on_nodes)*max_node);
   if(!sample_repartition_on_cpus)
      sample_repartition_on_cpus = calloc(1, sizeof(*sample_repartition_on_cpus)*max_cpu);

   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   __unused ibs_op_data3_t *data3 = (void*)&udata3;
   /*if(!data3->ibsldop) //Northbridge data is only valid for load operations
      return;*/

   if(s->ibs_dc_phys != 0) {
      memory_repartition_on_nodes[phys_to_node(s->ibs_dc_phys)]++;
   }
#if 1
   /* Store the number of loads occurring on each page */
   void *addr = (void*)((s->ibs_dc_phys / PAGE_SIZE) * (PAGE_SIZE));
   void *value = rbtree_lookup(memory_repartition_tree, addr, memory_repartition_cmp);
   if(!value) {
      value = calloc(1, sizeof(int));
      rbtree_insert(memory_repartition_tree, addr, value, memory_repartition_cmp);
      nb_seen_pages++;
   }
   *(int*)value = (*(int*) value) + 1;

   /* Detail by CPU */
   if(s->cpu >= max_cpu) {
      fprintf(stderr, "Seen CPU %d with MAX_CPU=%d\n", s->cpu, max_cpu);
      exit(-1);
   }
   sample_repartition_on_cpus[s->cpu]++;


   /* Detail by app */
   char *app = strdup(get_app(s));
   value = rbtree_lookup(app_repartition_tree, app, (compare_func)strcmp);
   if(!value) {
      value = calloc(1, sizeof(int));
      rbtree_insert(app_repartition_tree, app, value, (compare_func)strcmp);
   }
   *(int*)value = (*(int*) value) + 1;
#endif
}

static __unused int apps_print(void *key, void *value) {
   printf("#App %s: %d\n",
         (char*)key,
         *(int*)value);
   return 0;
}

void memory_repartition_show() {
   if(SHOW_MIN_THRES > 1)
      printf("#Warn: only showing pages with more than %d accesses\n", SHOW_MIN_THRES);
   //rbtree_print(memory_repartition_tree, memory_repartition_print);

   int i, sum = 0;
   for(i = 0; i < 4; i++) 
      sum += memory_repartition_on_nodes[i];
   for(i = 0; i < 4; i++) {
      printf("#Node %d: %d (%.2f%%)\n", i, memory_repartition_on_nodes[i], 100.*((float)memory_repartition_on_nodes[i])/((float)sum));
   }
#if 1
   for(i = 0; i < max_cpu; i++) {
      if(sample_repartition_on_cpus[i]) {
         printf("#CPU %d: %d samples\n", i, sample_repartition_on_cpus[i]);
      }
   }
   rbtree_print(app_repartition_tree, apps_print);
   printf("#Nb Touched pages: %d\n", nb_seen_pages);
#endif
}

void memory_repartition_init() {
   memory_repartition_tree = rbtree_create();
   app_repartition_tree = rbtree_create();
   memset(memory_repartition_on_nodes, 0, sizeof(memory_repartition_on_nodes));
   memset(sample_repartition_on_cpus, 0, sizeof(sample_repartition_on_cpus));
}
