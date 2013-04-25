#include "parse.h"
#include "builtin-stats.h"

static rbtree rbtree_stats;
typedef struct tid_stat {
   int samples_per_cpu[NB_STAT_VALUES];
   int remote_samples_per_cpu[MAX_NODE][MAX_CPU];
   char *app;
   int pid;
   int last_cpu;
   int nb_cpus;
} tid_stat_t;

void stats_init() {
   rbtree_stats = rbtree_create();
}

static int stat_tid_cmp(void *a, void *b) {
   return ((long)a)-((long)b);
}

void stats_parse(struct s* s) {
   tid_stat_t* value = rbtree_lookup(rbtree_stats, (void*)(long)get_tid(s), stat_tid_cmp);
   if(!value) {
      value = calloc(1, sizeof(*value));
      value->app = strdup(get_app(s));
      value->pid = get_pid(s);
      rbtree_insert(rbtree_stats, (void*)(long)get_tid(s), value, stat_tid_cmp);
      value->last_cpu = s->cpu;
      value->nb_cpus = 1;
   }
   assert(s->cpu < MAX_CPU);
   value->samples_per_cpu[s->cpu]++;
   value->samples_per_cpu[STAT_TID_SUM_SAMPLES]++;
   if(s->ibs_dc_phys != 0) {
      value->remote_samples_per_cpu[phys_to_node(s->ibs_dc_phys)][s->cpu]++;
   }
   if(value->last_cpu != s->cpu) {
      value->last_cpu = s->cpu;
      value->nb_cpus++;
   }
}

static int stat_tid_print(void *key, void* value) {
   int i, j;
   tid_stat_t *v = value;
   printf("%ld: %5d\t[", (long)key, v->samples_per_cpu[STAT_TID_SUM_SAMPLES]);
   for(i = 0; i < MAX_CPU; i++) 
      printf("%4d ", v->samples_per_cpu[i]);
   printf("]\n");
   for(i = 0; i < MAX_REAL_NODE; i++) {
      if(i == 0) 
         printf("%8.8s\t[", v->app);
      else if(i == 1) 
         printf("%8d\t[", v->pid);
      else
         printf("\t\t[");
      for(j = 0; j < MAX_CPU; j++) {
         if(cpu_to_node(j) != i && v->remote_samples_per_cpu[i][j]) {
            printf(RED "%4d " RESET, v->remote_samples_per_cpu[i][j]);
         } else if(cpu_to_node(j) == i && v->remote_samples_per_cpu[i][j]) {
            printf(GREEN "%4d " RESET, v->remote_samples_per_cpu[i][j]);
         } else {
            printf("%4d ", v->remote_samples_per_cpu[i][j]);
         }
      }
      printf("\n");
   }
   return 0;
}
void stats_show() {
   printf("Different TIDs: %d\n", rbtree_stats->nb_elements);
   rbtree_print(rbtree_stats, stat_tid_print);
}

