#include "parse.h"
#include "builtin-memory-overlap.h"

/**
 * Overview: Show informations about pages accessed by multiple applications.
 * - overlap_add_application/tid: add an application/tid to monitor
 * - overlap_parse: construct and fill page_informations_t for each touched page.
 * - overlap_show: show informations: kernel vs user; top accessing functions, etc.
 */

static int nb_overlapping_apps = 0;              /* Apps to monitor */
static char **overlapping_apps = NULL;
static int nb_overlapping_tids = 0;              /* Tids to monitor */
static int  *overlapping_tids = NULL;
static int nb_overlapping_pids = 0;              /* Tids to monitor */
static int  *overlapping_pids = NULL;
static int unshared_tid = -1;

typedef struct page_informations {              /* Informations stored for each memory page : */
   int *value;                                  /* int[NB_STORED_FIELDS] : ++ on each occurence, see previous defines */
   struct list *ips;                            /* IP (Instruction Pointer) which accessed the page. Will be resolved later */
   int sum;
   char *name;
   int uid;
} page_informations_t;

static rbtree overlap_tree;                     /* Page -> page_informations_t */
static int nb_overlap_in_kernel, nb_overlap_total, nb_non_overlap; /* Obvious */
static int sum_overlap, sum_non_overlap;

/*********************************
 * Initialization
 *********************************/

/* Add an application to monitor */
void overlap_add_application(char *app) {
   overlapping_apps = realloc(overlapping_apps, (nb_overlapping_apps+1)*sizeof(*overlapping_apps));
   overlapping_apps[nb_overlapping_apps] = strdup(app); 
   nb_overlapping_apps++;
}

/* Add a tid to monitor */
void overlap_add_tid(int tid) {
   overlapping_tids = realloc(overlapping_tids, (nb_overlapping_tids+1)*sizeof(*overlapping_tids));
   overlapping_tids[nb_overlapping_tids] = tid; 
   nb_overlapping_tids++;
}

/* Add a pid to monitor */
void overlap_add_pid(int pid) {
   overlapping_pids = realloc(overlapping_pids, (nb_overlapping_pids+1)*sizeof(*overlapping_pids));
   overlapping_pids[nb_overlapping_pids] = pid; 
   nb_overlapping_pids++;
}


void overlap_add_tid_unshared(int tid) {
   if(unshared_tid != -1) {
      fprintf(stderr, "Only one -N option might be set\n");
      exit(-1);
   }
   unshared_tid = nb_overlapping_tids-1;
}

void overlap_init() {
   /*if(nb_overlapping_apps <= 1) {
      fprintf(stderr, "Please specify at least two applications, e.g. -O app1 -O app2\n");
      exit(-1);
   }*/
   overlap_tree = rbtree_create();
   nb_overlap_in_kernel = nb_overlap_total = nb_non_overlap = 0;
   sum_overlap = sum_non_overlap = 0;
}

int values_sum(int *v, int *non_null) {
   int i, sum = 0;
   *non_null = 0;
   for(i = 0; i < nb_overlapping_apps + nb_overlapping_tids + nb_overlapping_pids; i++) {                                                                                                            
      if(v[i] != 0) {
         (*non_null)++;
         sum += v[i];
      }
   }
   if(*non_null)
      (*non_null)--;
   return sum;
}

/************************
 * Parsing = storing data in a slightly more organised way, rbtree fun.
 ************************/
void overlap_parse(struct s* s) {
   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   ibs_op_data3_t *data3 = (void*)&udata3;
   //if(!data3->ibsstop)
   //   return;

   /* If data does not come from a L3 (2) or DRAM (3), ignore */
   /*int status = s->ibs_op_data2_low & 7;
   if(status != 2 && status != 3) 
      return;*/

   /* Does the symbol belongs to the monitored set ? */
   int app_num = -1, i/*, non_null*/;
   for(i = 0; i < nb_overlapping_apps; i++) {
      if(!strcmp(get_app(s), overlapping_apps[i])) {
         app_num = i;
         break;
      }
   }
   for(i = nb_overlapping_apps; i < nb_overlapping_tids+nb_overlapping_apps; i++) {
      if(get_tid(s) == overlapping_tids[i-nb_overlapping_apps]) {
         app_num = i;
         break;
      }
   }
   for(i = nb_overlapping_apps + nb_overlapping_tids; i < nb_overlapping_tids+nb_overlapping_apps+nb_overlapping_pids; i++) {
      if(get_pid(s) == overlapping_pids[i-nb_overlapping_apps-nb_overlapping_tids]) {
         app_num = i;
         break;
      }
   }
   if(app_num == -1)
      return;

   /* Add page into rbtree */
   //void *addr = (void*)((s->ibs_dc_phys / 64) * (64));
   //void *addr = (void*)((s->ibs_dc_phys / PAGE_SIZE) * (PAGE_SIZE));
   //void *addr = (void*)((s->ibs_dc_linear / PAGE_SIZE) * (PAGE_SIZE));
   struct dyn_lib * l = sample_to_mmap(s);
   struct symbol *ob = sample_to_static_object(s); //unsafe, may return NULL ; static object
   struct symbol *ob2 = sample_to_variable2(s); //idem ; malloc object
   char *addr = NULL;
   if(ob2)
      addr = ob2->function;
   if(!addr && ob)
      addr = ob->function;
   //addr = (void*)((s->ibs_dc_phys / PAGE_SIZE) * (PAGE_SIZE));
   if(addr == NULL)
      return;

   page_informations_t *v = rbtree_lookup(overlap_tree, addr, pointer_cmp);
   if(!v) {
      v = calloc(1, sizeof(*v));
      v->value = calloc(NB_STORED_FIELDS, sizeof(*v->value));
      rbtree_insert(overlap_tree, addr, v, pointer_cmp);
      //v->name = l->name;
      v->name = addr;
      if(l->enclosing_lib)
         v->uid = l->enclosing_lib->uid;
   } 
#if 0
   else if(strcmp(v->name, l->name)) {
      /* Accesses to the same virtual address which do NOT correspond to the same type of data (e.g. //anon vs [stack])       */
      /* This is a problem because we cannot continue to count overlaps on this memory area => currently we reset the value   */
      /* We also do not care if xxx becomes [kernel]. That simply means that the kernel is accessing a user memory zone.      */
      if(!strcmp("[kernel]", v->name)) {
         v->name = l->name;
         if(l->enclosing_lib)
            v->uid = l->enclosing_lib->uid;
      } else if(!strcmp("[kernel]", l->name)) {
      } else {
         int discard = values_sum(v->value, &non_null);
         printf("#Mismatch: %s became %s [discarding %d samples, %d overlap]\n", v->name, l->name, discard, non_null);
         v->name = l->name;
         if(l->enclosing_lib)
            v->uid = l->enclosing_lib->uid;
         memset( v->value, 0, sizeof(*v->value));
      }
      //exit(0); //For now we exit but we should reset a counter to indicate that it is a new memory map.
   } else if(l->enclosing_lib && v->uid != l->enclosing_lib->uid) {
      /* Uid change = the previous mmaped area at this virtual address has been unmapped. For now we simply discard the old page */
      /* and use the new one. This is sound in our tests since really used mmaps are never unmapped & replaced.                  */
      /* -> Yet print a warning.                                                                                                 */
      int discard = values_sum(v->value, &non_null);
      printf("#Uid change %d to %d [discarding %d samples, %d overlaps]\n", v->uid, l->enclosing_lib->uid, discard, non_null);
      v->uid = l->enclosing_lib->uid;
      memset( v->value, 0, sizeof(*v->value));
      //exit(0); //For now we exit but we should reset a counter to indicate that it is a new memory map.
   }
#endif

   /* Basic: increment count for the app */
   v->value[app_num] = v->value[app_num] + 1;

   /* Load / Store */
   if(data3->ibsldop) { //Load
      v->value[LOAD_INDEX(app_num)]++;
   }
   if(data3->ibsstop) {
      v->value[STORE_INDEX(app_num)]++;
   }

   /* Kernel or not ?*/
   if(is_kernel(s)) { 
      v->value[IN_KERNEL]++;
   } else {
      v->value[USERLAND]++;
   }

   /* List of IPs */
   struct s *s_copy = malloc(sizeof(*s));
   memcpy(s_copy, s, sizeof(*s));
   v->ips = list_add(v->ips, s_copy);
}

/** Utility functions to free and sort the resolved IPs **/
static __unused int overlap_free(rbtree_node n) {
   free(n->value);
   return 0;
}
static __unused int overlap_get_sum_sons(int *vals) {
   int i;
   int sum = vals[MAX_CPU];
   if(!sum) {
      for(i = 0; i < MAX_CPU; i++) {
         sum += vals[i];
      }
      vals[MAX_CPU] = sum;
   }
   return sum;
}
static __unused int overlap_cmp_sons(const void *a, const void* b) {
   const rbtree_node _a = *(const rbtree_node*) a;
   const rbtree_node _b = *(const rbtree_node*) b;
   int a_sum = overlap_get_sum_sons((int*)_a->value);
   int b_sum = overlap_get_sum_sons((int*)_b->value);
   return b_sum - a_sum;
}

/* Real stuff */
static int load_overlaps, store_overlaps;
static rbtree rbtree_overlap_mmaped_zones;
static rbtree rbtree_nonoverlap_mmaped_zones;
struct mmaped_overlap_t {
   char *name;
   int value;
};
static int mmaped_overlap_sum = 0, mmaped_nonoverlap_sum = 0, mmaped_total_overlap_sum = 0;
static int localize_sort(const void *_a, const void *_b) {
   struct mmaped_overlap_t *a = (*(const rbtree_node*)_a)->value;
   struct mmaped_overlap_t *b = (*(const rbtree_node*)_b)->value;
   return b->value - a->value;
}


static int overlap_print(void *key, void *_value) {
   page_informations_t *value = _value;
   int *v = value->value, i, non_null = 0, sum = 0;
   for(i = 0; i < nb_overlapping_apps + nb_overlapping_tids + nb_overlapping_pids; i++) {
      if(v[i] != 0) {
         non_null++;
         sum += v[i];
      }
   }

   /* Show everything when only one -O is specified, whatever that means */
   if(nb_overlapping_apps + nb_overlapping_tids + nb_overlapping_pids == 1)
      non_null++;

   /* Only show pages manipulated by at least two apps */
   if((unshared_tid == -1 && non_null > 1 && sum > 0)
         || (unshared_tid > -1 && v[nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids+unshared_tid] > 0 && v[nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids+unshared_tid] == sum)) {

      struct mmaped_overlap_t* mo = rbtree_lookup(rbtree_overlap_mmaped_zones, value->name, (compare_func)strcmp);
      if(!mo) {
         mo = calloc(1, sizeof(*mo));
         mo->name = strdup(value->name);
         rbtree_insert(rbtree_overlap_mmaped_zones, value->name, mo, (compare_func)strcmp);
      }
      mo->value += sum;
      mmaped_overlap_sum += sum;
      mmaped_total_overlap_sum += sum;

      /* Basic information: who accessed, how much */
      //printf("%18lu ", (long unsigned)key);
      //printf("%015lx ", (long unsigned) key);
      printf("%s", (char*) value->name);
      for(i = 0; i < nb_overlapping_apps + nb_overlapping_tids + nb_overlapping_pids; i++) {
         printf("%8d [store %5d load %5d] ", v[i], v[STORE_INDEX(i)], v[LOAD_INDEX(i)]);
         load_overlaps += v[LOAD_INDEX(i)];
         store_overlaps += v[STORE_INDEX(i)];
      }
      printf("[%3d kernel, %3d user] %s\n", v[IN_KERNEL], v[USERLAND], value->name);

#if 0
      ip_t *ip;
      list_foreach(value->ips, ip) {
         printf("\t\t%p\n", (void*)ip->rip);
      }
#elsif 0
      /* Resolve IPs and store function count in a rbtree */
      struct s *ip;
      rbtree t = rbtree_create();
      list_foreach(value->ips, ip) {
         struct symbol *s = get_symbol(ip);
         if(s) {
            int *value = rbtree_lookup(t, s, pointer_cmp);
            if(!value) {
               value = calloc(MAX_CPU + 1,sizeof(*value));
               rbtree_insert(t, s, value, pointer_cmp);
            }
            assert(ip->cpu < MAX_CPU);
            value[ip->cpu] = value[ip->cpu] + 1;
         }
      }

      /* Sort the rbtree by count value */
      rbtree_key_val_arr_t *sorted = rbtree_sort(t, overlap_cmp_sons);
      int i, j;
      for(i = 0; i < sorted->nb_elements; i++) { /* and print */
         int *vals = (int*)sorted->vals[i]->value;
         printf("\t%5d %30s [", 
               overlap_get_sum_sons(vals),
               ((struct symbol*)sorted->vals[i]->key)->function);
         for(j = 0; j < MAX_CPU; j++) {
            printf("%3d ", vals[j]);
         }
         printf("]\n");
      }

      /* Clean */
      rbtree_free(t, overlap_free);
      rbtree_arr_free(sorted);
#endif
      nb_overlap_total++;
      sum_overlap += sum;
   } else {
      nb_non_overlap++;
      sum_non_overlap += sum;

      struct mmaped_overlap_t* mo = rbtree_lookup(rbtree_nonoverlap_mmaped_zones, value->name, (compare_func)strcmp);
      if(!mo) {
         mo = calloc(1, sizeof(*mo));
         mo->name = strdup(value->name);
         rbtree_insert(rbtree_nonoverlap_mmaped_zones, value->name, mo, (compare_func)strcmp);
      }
      mo->value += sum;
      mmaped_nonoverlap_sum += sum;
      mmaped_total_overlap_sum += sum;
   }

   return 0;
}


static int overlap_get_sum_pages(page_informations_t *val) {
   int i;
   int sum = val->sum;
   if(!sum) {
      for(i = 0; i < nb_overlapping_apps + nb_overlapping_tids + nb_overlapping_pids; i++) {
         sum += val->value[i];
      }
      val->sum = sum;
   }
   return sum;
}

static int overlap_cmp_pages(const void *a, const void *b) {
   const rbtree_node _a = *(const rbtree_node*) a;
   const rbtree_node _b = *(const rbtree_node*) b;
   int a_sum = overlap_get_sum_pages((page_informations_t*)_a->value);
   int b_sum = overlap_get_sum_pages((page_informations_t*)_b->value);
   return b_sum - a_sum;

}

void overlap_show() {
   int i;
   rbtree_overlap_mmaped_zones = rbtree_create();
   rbtree_nonoverlap_mmaped_zones = rbtree_create();

   if(nb_overlapping_apps + nb_overlapping_tids + nb_overlapping_pids == 1)
      printf("#Warn: showing all samples of 1 app (%d %d %d); to view overlaps specify at least two apps (e.g. -O app1 -O app2)\n",
            nb_overlapping_apps, nb_overlapping_tids, nb_overlapping_pids);
   else {
      printf("%*.*s ",15,15," ");
      for(i = 0; i < nb_overlapping_apps; i++) {
         printf("%8.8s ", overlapping_apps[i]);
      }
      for(i = 0; i < nb_overlapping_tids; i++) {
         printf("%8d ", overlapping_tids[i]);
      }
      for(i = 0; i < nb_overlapping_pids; i++) {
         printf("%8d ", overlapping_pids[i]);
      }
      printf("\n");
   }

   //rbtree_print(overlap_tree, overlap_print);
   rbtree_key_val_arr_t *sorted = rbtree_sort(overlap_tree, overlap_cmp_pages);
   for(i = 0; i < sorted->nb_elements; i++) { 
      rbtree_node n = sorted->vals[i];
      overlap_print(n->key, n->value);
   }

   printf("#Top libs inducing overlaps:\n");
   sorted = rbtree_sort(rbtree_overlap_mmaped_zones, localize_sort);
   for(i = 0; i < sorted->nb_elements; i++) { 
      struct mmaped_overlap_t* val = sorted->vals[i]->value;
      printf("%30s: %7d (%.2f%%) (%.2f%%)\n", short_name(val->name), val->value, 
            100.*((float)val->value)/((float)mmaped_overlap_sum),
            100.*((float)val->value)/((float)mmaped_total_overlap_sum));
   }

   printf("#Top libs not inducing overlaps:\n");
   sorted = rbtree_sort(rbtree_nonoverlap_mmaped_zones, localize_sort);
   for(i = 0; i < sorted->nb_elements; i++) { 
      struct mmaped_overlap_t* val = sorted->vals[i]->value;
      printf("%30s: %7d (%.2f%%) (%.2f%%)\n", short_name(val->name), val->value, 
            100.*((float)val->value)/((float)mmaped_nonoverlap_sum),
            100.*((float)val->value)/((float)mmaped_total_overlap_sum));
   }



   printf("%s: %d (%.2f%%)\n", (unshared_tid == -1)?"Overlapping pages":"Non shared pages:", nb_overlap_total, (float)100.*((float)nb_overlap_total)/((float)nb_overlap_total+nb_non_overlap));
   printf("%s: %d\n", (unshared_tid == -1)?"Non overlapping pages":"Shared pages:", nb_non_overlap);
   printf("%s: %d (%.2f%%)\n", (unshared_tid == -1)?"Operations touching overlapping pages":"Operations touching non shared pages:", sum_overlap, (float)100.*((float)sum_overlap)/((float)sum_non_overlap+sum_overlap));
   printf("Load overlaps: %d; Store overlaps: %d\n", load_overlaps, store_overlaps);
}

