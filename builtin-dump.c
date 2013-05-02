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
#include "builtin-dump.h"
#include <math.h>

/*
 * Various dumps of information
 * Some outputs are thought to be piped to gnuplot or to perl/python scripts
 */

#define LATENCY_STEP 20 // breakdown latencies in chucks of X cycles, e.g. 0-20, 20-40, etc.

static int nb_ignored, nb_printed;
static uint64_t min_page, max_page;
static int mode = 1;
extern uint64_t first_rdt;

static struct addresses {
   uint64_t rdt;
   uint64_t addr;
   int pid;
   int cpu;
   struct symbol *sym;
   char *app;
   struct dyn_lib* mmap;
   struct mmapped_dyn_lib* mmap2;
} **addresses;
static int nb_addresses_max, nb_addresses;
struct addr_arr {
   struct addresses **arr;
   int index, max_nb_elem;
};

static int nb_latencies;
static int total_latency;
static rbtree latencies;
static rbtree pids;
static struct {
   struct {
      uint64_t addr;
      uint64_t rdt;
   }* arr;
   size_t index;
   size_t size;
} loc_remote[2]; //remote = 0, local = 1

static double *latencies_list = NULL;
static int latencies_list_index = 0;
static int latencies_list_max = 0;
static uint64_t ffirst_rdt, llast_rdt;

void dump_modifier(int m) {
   if(m > 9 || m < 1)
      die("Mode %d is not supported by dumper\n", m);
   mode = m;
}

void dump_init() {
   nb_ignored = nb_printed = 0;
   min_page = -1;
   max_page = 0;

   nb_addresses_max = 500;
   nb_addresses = 0;
   addresses = malloc(nb_addresses_max*sizeof(*addresses));
   latencies = rbtree_create();
   pids = rbtree_create();
}


static int cmp_addresses(const void *_a, const void *_b) {
   struct addresses *a = *(struct addresses **)_a;
   struct addresses *b = *(struct addresses **)_b;
   if(a->rdt > b->rdt)
      return 1;
   else if(a->rdt < b->rdt)
      return -1;
   else
      return 0;
   //return a->rdt - b->rdt; //Nope because overflow
}

/*find and print the average*/
double average(double *nums, int maxNums) {
   double sum = 0;
   int i;
   for( i = 0; i < maxNums; i++)
      sum += nums[i];
   return sum/maxNums;
}


/*finds the standard deviation*/
double stdDev(double *nums, int maxNums) {
   double sum = 0;
   double mean = average(nums, maxNums);
   int i;

   for(i = 0; i < maxNums; i++)
      sum += (nums[i]-mean)*(nums[i]-mean);

   return sqrt(sum/maxNums);
}


void dump_parse(struct s* s) {
   if(mode == 7 || mode == 8) {
      struct addr_arr *a = rbtree_lookup(pids, (void*)(long)get_tid(s), int_cmp);
      if(!a) {
         a = malloc(sizeof(*a));
         a->arr = malloc(512*sizeof(*a->arr));
         a->index = 0;
         a->max_nb_elem = 512;
         rbtree_insert(pids, (void*)(long)get_tid(s), a, int_cmp);
      }
      if(a->index >= a->max_nb_elem) {
         a->max_nb_elem *= 2;
         a->arr = realloc(a->arr, a->max_nb_elem*sizeof(*a->arr));
      }

      struct addresses *addr = calloc(1, sizeof(*a));
      addr->rdt = s->rdt;
      if(mode == 7)
         addr->addr = s->ibs_dc_linear;
      else
         addr->addr = s->ibs_dc_phys;
      addr->pid = get_tid(s);
      a->arr[a->index] = addr;
      a->index++;
   } else if(mode == 9) {
      int local = get_addr_node(s) == cpu_to_node(s->cpu);
      if(loc_remote[local].index >= loc_remote[local].size) {
         loc_remote[local].size += 10000;
         loc_remote[local].arr = realloc(loc_remote[local].arr, (loc_remote[local].size)*sizeof(*loc_remote[local].arr));
      }
      loc_remote[local].arr[loc_remote[local].index].rdt = s->rdt;
      loc_remote[local].arr[loc_remote[local].index].addr = s->ibs_dc_linear;
      loc_remote[local].index++;
   } else if(mode==1) {
      char *var = sample_to_variable(s);
      struct symbol *fun = get_function(s);
      struct mmapped_dyn_lib *lib = sample_to_mmap2(s);
      printf("[PID %d TID %d (%s)] [RIP %p] [CPU %d] [ADDR LIN %p (PHYS %p)] [Cache %d Data2 %x] [%s+(0x%lx)] [%s (%s)]\n",
        get_pid(s),
        get_tid(s),
        get_app(s),
        (void*)s->rip,
        (int)s->cpu,
        (void*)s->ibs_dc_linear,
        (void*)s->ibs_dc_phys,
        (s->ibs_op_data3_low & (1<<7))>>7,
        s->ibs_op_data2_low,
        fun?fun->function_name:"unknown function",
        (fun && lib)?((s->rip - lib->begin + lib->off) - (uint64_t)fun->ip):0, // SASHA - offset of the access inside the function
        var?var:"unknown",
        sample_to_mmap(s)->name
     );
   } else if(mode==5) {
      long latency = get_latency(s) / LATENCY_STEP;
      if(latency > 800/LATENCY_STEP)
         latency = 800/LATENCY_STEP;
      else if(latency > 600/LATENCY_STEP)
         latency = 600/LATENCY_STEP;
      else if(latency > 400/LATENCY_STEP)
         latency = 400/LATENCY_STEP;
      else if(latency > 250/LATENCY_STEP)
         latency = 250/LATENCY_STEP;
      else if(latency > 150/LATENCY_STEP)
         latency = 150/LATENCY_STEP;
      int *value = rbtree_lookup(latencies, (void*)latency, pointer_cmp);
      if(!value) {
         value = calloc(1, sizeof(*value));
         rbtree_insert(latencies, (void*)latency, value, pointer_cmp);
      }
      value[0]++;
      nb_latencies++;
      total_latency += get_latency(s);

      if(latencies_list_max <= latencies_list_index) {
         latencies_list_max += 10000;
         latencies_list = realloc(latencies_list, sizeof(*latencies_list)*latencies_list_max);
      }
      latencies_list[latencies_list_index++] = get_latency(s);

      if(!ffirst_rdt)
         ffirst_rdt = s->rdt;
      llast_rdt = s->rdt;
   } else if(mode == 3) {
      printf("%ld %ld\n", s->rdt, (long)get_latency(s));
   } else {
      uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
      ibs_op_data3_t *data3 = (void*)&udata3;
      if(!data3->ibsdcphyaddrvalid)
         return;

      if(s->ibs_dc_phys > get_memory_size())
         return;

      if(nb_addresses >= nb_addresses_max) {
         nb_addresses_max*=2;
         addresses = realloc(addresses, nb_addresses_max*sizeof(*addresses));
      }
      addresses[nb_addresses] = malloc(sizeof(*addresses[nb_addresses]));
      addresses[nb_addresses]->rdt = s->rdt;
      if(mode != 4) {
         addresses[nb_addresses]->addr = s->ibs_dc_phys;
      } else {
         addresses[nb_addresses]->addr = s->ibs_dc_linear;
      }
         addresses[nb_addresses]->addr = s->ibs_dc_linear;
      addresses[nb_addresses]->cpu = s->cpu;
      addresses[nb_addresses]->pid = get_tid(s);
      addresses[nb_addresses]->sym = get_function(s);
      addresses[nb_addresses]->app = strdup(get_app(s));
      addresses[nb_addresses]->mmap = sample_to_mmap(s);
      addresses[nb_addresses]->mmap2 = sample_to_mmap2(s);
      nb_addresses++;
   }
}

static int print_latency(void *key, void *value) {
   printf("%4d-%4d: %d (%.2f%%)\n",
         ((int)(long)key)*LATENCY_STEP,
         (((int)(long)key)+1)*LATENCY_STEP,
         *(int*)value,
         100.*((float)*(int*)value)/((float)nb_latencies));
   return 0;
}

static rbtree rbtree_stats;
struct dump_stat {
   int last_cpu;
   int nb_cpus;
   char *app;
};
static int stat_tid_cmp(void *a, void *b) {
   return ((long)a)-((long)b);
}
static int nb_pid, nb_cpus;
static int print_rbstat(void *key, void* value) {
   struct dump_stat *v = value;
   printf("%d (%s): %d\n", (int)(long)key, v->app, v->nb_cpus);
   nb_pid++;
   nb_cpus += v->nb_cpus;
   return 0;
}

static int sort_pid(const void *a, const void *b) {
   const rbtree_node _a = *(const rbtree_node*) a;
   const rbtree_node _b = *(const rbtree_node*) b;
   return ((long)_a->key) - ((long)_b->key);
}

static void add_log_file_if_exist() {
   char buffer[512];
   FILE * pFile = fopen ("/tmp/log" , "r");
   if(!pFile)
      return;

   int i;
   struct addr_arr * addrs[2];
   for(i = 0; i < 2; i++) {
      addrs[i] = rbtree_lookup(pids, (void*)(long)(i+1), int_cmp);
      if(addrs[i])
         die("%d should not exist\n", i+1);

      addrs[i] = malloc(sizeof(*addrs[i]));
      addrs[i]->arr = malloc(512*sizeof(*addrs[i]->arr));
      addrs[i]->index = 0;
      addrs[i]->max_nb_elem = 512;
      rbtree_insert(pids, (void*)(long)(i+1), addrs[i], int_cmp);
   }

   while(fgets (buffer, sizeof(buffer), pFile)) {
      uint64_t address;
      int node, status;
      struct addr_arr * a;
      if(sscanf(buffer, "#%lx %d %di", &address, &node, &status) != 3) {
         printf("#Bad line: %s", buffer);
         continue;
      }

      a = addrs[(status<0)]; //0 = success, -x = error
      if(a->index >= a->max_nb_elem) {
         a->max_nb_elem *= 2;
         a->arr = realloc(a->arr, a->max_nb_elem*sizeof(*a->arr));
      }

      struct addresses *addr = calloc(1, sizeof(*a));
      addr->rdt = first_rdt;
      addr->addr = address;
      addr->pid = (status<0);
      a->arr[a->index] = addr;
      a->index++;
   }
   fclose (pFile);
}

void dump_show() {
   if(mode == 7 || mode == 8) {
      add_log_file_if_exist();

      int i, j, max = 0;
      rbtree_key_val_arr_t *sorted = rbtree_sort(pids, sort_pid);
      printf("#");
      for(i = 0; i < sorted->nb_elements; i++) { 
         struct addr_arr *a = (sorted->vals[i])->value;
         if(a->index > max)
            max = a->index;
         printf("%lu ", (long)(sorted->vals[i])->key);
      }
      printf("\n");
      for(j = 0; j < max; j++) {
         for(i = 0; i < sorted->nb_elements; i++) { 
            struct addr_arr *a = (sorted->vals[i])->value;
            if(j >= a->index)
               printf("-\t-\t");
            else
               printf("%lu\t%lu\t", a->arr[j]->rdt, a->arr[j]->addr);
         }
         printf("\n");
      }
   } else if(mode == 9) {
      printf("#remote (rdt, addr)\tlocal (rdt, addr)\n");
      int max = loc_remote[0].index>loc_remote[1].index?loc_remote[0].index:loc_remote[1].index;
      int i,j;
      for(i = 0; i < max; i++) {
         for(j = 0; j < 2; j++) { 
            if(i >= loc_remote[j].index)
                  printf("-\t-\t");
            else
                  printf("%lu\t%lu\t", loc_remote[j].arr[i].rdt, loc_remote[j].arr[i].addr);
         }
         printf("\n");
      }
   } else if((mode > 1 && mode < 5) || mode == 6) {
      switch(mode) {
         case 2:
         case 3:
         case 4:
            printf("RDT\t\tADDR (%s)\tPID\tCPU\n", (mode==4)?"linear":"phys");
      }

      qsort (addresses, nb_addresses, sizeof (*addresses), cmp_addresses);
      rbtree_stats = rbtree_create();

      int i;
      uint64_t last_rdt = 0;
      struct dump_stat* value;
      for(i = 0; i < nb_addresses; i++) {
         assert( addresses[i]->rdt >= last_rdt );
         last_rdt =  addresses[i]->rdt;

         switch(mode) {
            case 2:
               printf("%llu\t%llu\t%d\t%d\t%30s (%50s)\t%s\taccess %s %p\n", (long long unsigned) addresses[i]->rdt, (long long unsigned) addresses[i]->addr, addresses[i]->pid, addresses[i]->cpu, addresses[i]->sym->function_name, addresses[i]->sym->file->name, addresses[i]->app, addresses[i]->mmap->name, (void*)((addresses[i]->mmap2)?(addresses[i]->addr - addresses[i]->mmap2->begin + addresses[i]->mmap2->off):0));
               break;
            case 3:
            case 4:
               printf("%llu\t%llu\t%d\t%d\n", (long long unsigned) addresses[i]->rdt, (long long unsigned) addresses[i]->addr, addresses[i]->pid, addresses[i]->cpu);
               break;
            case 6:
               value = rbtree_lookup(rbtree_stats, (void*)(long)addresses[i]->pid, stat_tid_cmp);
               if(!value) {
                  value = calloc(1, sizeof(*value));
                  rbtree_insert(rbtree_stats, (void*)(long)addresses[i]->pid, value, stat_tid_cmp);
                  value->last_cpu = addresses[i]->cpu;
                  value->nb_cpus = 1;
                  value->app = addresses[i]->app;
               } else if(value->last_cpu != addresses[i]->cpu) {
                  value->last_cpu = addresses[i]->cpu;
                  value->nb_cpus++;
               }
               break;
         }
      }
      switch(mode) {
         case 6:
            rbtree_print(rbtree_stats, print_rbstat);
            printf("%d %d\n", nb_pid, nb_cpus);
      }
   } else if(mode == 5) {
      printf("Latency range - %%\n");
      rbtree_print(latencies, print_latency);
      printf("Avg latency: %.2f\n", ((float)total_latency)/((float)nb_latencies));

      printf("Avg latency(double): %.2f [stddev = %2.f = %2.f%%] [# = %d]\n", average(latencies_list, latencies_list_index), stdDev(latencies_list, latencies_list_index), stdDev(latencies_list, latencies_list_index)/average(latencies_list,latencies_list_index)*100., latencies_list_index);

      printf("MRper10^12cycle: %2.f (%.2f)\n", 1000000000000.*((double)latencies_list_index) / ((double)(llast_rdt-ffirst_rdt)),
            ((double)(llast_rdt-ffirst_rdt)));
      printf("MRperFreq10^12cycle: %x (%d) -> %2.f (%.2f)\n", i.sampling_rate, i.sampling_rate, 1000000000000.*((double)i.sampling_rate)*((double)latencies_list_index) / ((double)(llast_rdt-ffirst_rdt)),
            ((double)(llast_rdt-ffirst_rdt)));
   }
}

