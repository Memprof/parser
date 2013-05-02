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
#include "builtin-stats.h"

/*
 * Show number of accesses per tid on all cpu to all nodes
 * x = cpu, y = node
 */
static rbtree rbtree_stats;
typedef struct tid_stat {
   int *samples_per_cpu;
   int *remote_samples_per_cpu;
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
      value->remote_samples_per_cpu = calloc(1, sizeof(*value->remote_samples_per_cpu)*max_node*max_cpu);
      value->samples_per_cpu = calloc(1, sizeof(*value->samples_per_cpu)*max_cpu + 1);
   }
   assert(s->cpu < max_cpu);
   value->samples_per_cpu[s->cpu]++;
   value->samples_per_cpu[max_cpu]++;
   if(s->ibs_dc_phys != 0) {
      value->remote_samples_per_cpu[phys_to_node(s->ibs_dc_phys)*max_cpu + s->cpu]++;
   }
   if(value->last_cpu != s->cpu) {
      value->last_cpu = s->cpu;
      value->nb_cpus++;
   }
}

static int stat_tid_print(void *key, void* value) {
   int i, j;
   tid_stat_t *v = value;
   printf("%ld: %5d\t[", (long)key, v->samples_per_cpu[max_cpu]);
   for(i = 0; i < max_cpu; i++) 
      printf("%4d ", v->samples_per_cpu[i]);
   printf("]\n");
   for(i = 0; i < max_real_node; i++) {
      if(i == 0) 
         printf("%8.8s\t[", v->app);
      else if(i == 1) 
         printf("%8d\t[", v->pid);
      else
         printf("\t\t[");
      for(j = 0; j < max_cpu; j++) {
         if(cpu_to_node(j) != i && v->remote_samples_per_cpu[i*max_cpu + j]) {
            printf(RED "%4d " RESET, v->remote_samples_per_cpu[i*max_cpu + j]);
         } else if(cpu_to_node(j) == i && v->remote_samples_per_cpu[i*max_cpu + j]) {
            printf(GREEN "%4d " RESET, v->remote_samples_per_cpu[i*max_cpu + j]);
         } else {
            printf("%4d ", v->remote_samples_per_cpu[i*max_cpu + j]);
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

