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
#include "compatibility.h"
#include "compatibility-machine.h"

/*
 * compatibility.c
 * Enable Memprof to read old memprof dumps.
 */


struct s_v1 {
   uint32_t cpu;
   uint32_t ibs_dc_kmem_cache_offset;
   uint32_t ibs_op_data1_high;
   uint32_t ibs_op_data1_low;
   uint32_t ibs_op_data2_low;
   uint32_t ibs_op_data3_high;
   uint32_t ibs_op_data3_low;
   uint32_t hole; // odd number of 32, fill in hole
   uint64_t rip;
   uint64_t ibs_dc_linear;
   uint64_t ibs_dc_phys;
   uint64_t ibs_dc_kmem_cachep;
   uint64_t ibs_dc_kmem_cache_caller;
   uint64_t linear_valid;
   uint64_t overflow;
   char comm[32];
   uint32_t hole2; // odd number of 32, fill in hole
   unsigned long usersp;
   unsigned long rsp;
   void *stack;
   int tid;
};

struct s_v2 {
   uint32_t cpu;
   uint32_t ibs_dc_kmem_cache_offset;
   uint32_t ibs_op_data1_high;
   uint32_t ibs_op_data1_low;
   uint32_t ibs_op_data2_low;
   uint32_t ibs_op_data3_high;
   uint32_t ibs_op_data3_low;
   uint32_t hole; // odd number of 32, fill in hole
   uint64_t rip;
   uint64_t ibs_dc_linear;
   uint64_t ibs_dc_phys;
   uint64_t ibs_dc_kmem_cachep;
   uint64_t ibs_dc_kmem_cache_caller;
   uint64_t linear_valid;
   uint64_t overflow;
   char comm[32];
   uint32_t hole2; // odd number of 32, fill in hole
   unsigned long usersp;
   unsigned long rsp;
   void *stack;
   int tid;
   uint64_t rdt;
};

struct s_v3 { 
   uint32_t cpu;
   uint32_t ibs_dc_kmem_cache_offset;
   uint32_t ibs_op_data1_high;
   uint32_t ibs_op_data1_low;
   uint32_t ibs_op_data2_low;
   uint32_t ibs_op_data3_high;
   uint32_t ibs_op_data3_low;
   uint32_t hole; // odd number of 32, fill in hole
   uint64_t rip;
   uint64_t ibs_dc_linear;
   uint64_t ibs_dc_phys;
   uint64_t ibs_dc_kmem_cachep;
   uint64_t ibs_dc_kmem_cache_caller;
   uint64_t linear_valid;
   uint64_t overflow;
   char comm[32];
   uint32_t hole2; // odd number of 32, fill in hole
   unsigned long usersp;
   unsigned long rsp;
   void *stack;
   int tid;
   uint64_t rdt;
   uint64_t object_num;
};

/* V1 to V2 simply consists in adding a missing field to the struct */
size_t fread_v1(void *ptr, size_t size, size_t nitems, FILE *mmaped_file) {
   struct s_v1 s1;
   struct s *s = ptr;
   int ret = fread(&s1, sizeof(struct s_v1), nitems, mmaped_file);

   s->rdt = 0LL;
   s->cpu = s1.cpu;
   s->rip = s1.rip;
   s->ibs_op_data1_high = s1.ibs_op_data1_high;
   s->ibs_op_data1_low = s1.ibs_op_data1_low;
   s->ibs_op_data2_low = s1.ibs_op_data2_low;
   s->ibs_op_data3_high = s1.ibs_op_data3_high;
   s->ibs_op_data3_low = s1.ibs_op_data3_low;
   s->tid = s1.linear_valid/100;
   s->pid = s1.tid;
   s->kern = ((s1.linear_valid % 100) >= 10);
   memcpy(s->comm, s1.comm, sizeof(s1.comm));
   s->usersp = s1.usersp;
   s->stack = (long)s1.stack;
   return ret;
}

size_t fread_v2(void *ptr, size_t size, size_t nitems, FILE *mmaped_file) {
   struct s_v2 s1;
   struct s *s = ptr;
   int ret = fread(&s1, sizeof(struct s_v2), nitems, mmaped_file);
   s->rdt = s1.rdt;
   s->cpu = s1.cpu;
   s->rip = s1.rip;
   s->ibs_op_data1_high = s1.ibs_op_data1_high;
   s->ibs_op_data1_low = s1.ibs_op_data1_low;
   s->ibs_op_data2_low = s1.ibs_op_data2_low;
   s->ibs_op_data3_high = s1.ibs_op_data3_high;
   s->ibs_op_data3_low = s1.ibs_op_data3_low;
   s->tid = s1.linear_valid/100;
   s->pid = s1.tid;
   s->kern = ((s1.linear_valid % 100) >= 10);
   memcpy(s->comm, s1.comm, sizeof(s1.comm));
   s->usersp = s1.usersp;
   s->stack = (long)s1.stack;
   return ret;
}

size_t fread_v3(void *ptr, size_t size, size_t nitems, FILE *mmaped_file) {
   struct s_v3 s1;
   struct s *s = ptr;
   int ret = fread(&s1, sizeof(struct s_v3), nitems, mmaped_file);
   s->rdt = s1.rdt;
   s->cpu = s1.cpu;
   s->rip = s1.rip;
   s->ibs_op_data1_high = s1.ibs_op_data1_high;
   s->ibs_op_data1_low = s1.ibs_op_data1_low;
   s->ibs_op_data2_low = s1.ibs_op_data2_low;
   s->ibs_op_data3_high = s1.ibs_op_data3_high;
   s->ibs_op_data3_low = s1.ibs_op_data3_low;
   s->tid = s1.linear_valid/100;
   s->pid = s1.tid;
   s->kern = ((s1.linear_valid % 100) >= 10);
   memcpy(s->comm, s1.comm, sizeof(s1.comm));
   s->usersp = s1.usersp;
   s->stack = (long)s1.stack;
   return ret;
}

/* V1 does not have perf samples */
static void read_perf_v1(char *mmaped_file) {
}

/* V1 and V2 do not have any header */
static void read_header_v1(FILE *stream, struct i *i) {
   if(!opt_machine) {
      printf("#No machine specified via --machine ; guess is sci100\n");
      opt_machine = "sci100";
   }
   set_machine_compat(opt_machine);

   memset(i, 0, sizeof(*i));
   strncpy(i->hostname, opt_machine, sizeof(i->hostname));
   i->sampling_rate = 0;
}

#define S_MAX_NODE 4
struct i_v3 {
   char hostname[32];
   int max_nodes;
   uint64_t node_begin[S_MAX_NODE];
   uint64_t node_end[S_MAX_NODE];
};
static void read_header_v3(FILE *stream, struct i *i) {
   struct i_v3 i_tmp;
   assert(fread(&i_tmp, sizeof(i_tmp), 1, stream) == 1);

   i->sorted_by_rdt = 0;
   i->max_nodes = i_tmp.max_nodes;
   i->sampling_rate = 0;
   memcpy(i->hostname, i_tmp.hostname, sizeof(i->hostname));
   i->max_nodes = S_MAX_NODE;
   i->node_begin = malloc(sizeof(i->node_begin)*i->max_nodes);
   memcpy(i->node_begin, i_tmp.node_begin, sizeof(i_tmp.node_begin));
   i->node_end = malloc(sizeof(i->node_end)*i->max_nodes);
   memcpy(i->node_end, i_tmp.node_end, sizeof(i_tmp.node_end));
   i->max_cpu = 0;
   i->cpu_to_node = 0;

   set_machine_full(i);
}

struct i_v4 {
   int sorted_by_rdt;
   char hostname[32];
   int max_nodes;
   uint64_t node_begin[S_MAX_NODE];
   uint64_t node_end[S_MAX_NODE];
};
static void read_header_v4(FILE *stream, struct i *i) {
   struct i_v4 i_tmp;
   assert(fread(&i_tmp, sizeof(i_tmp), 1, stream) == 1);

   i->sampling_rate = 0;
   i->sorted_by_rdt = i_tmp.sorted_by_rdt;
   i->max_nodes = S_MAX_NODE;
   i->node_begin = malloc(sizeof(i->node_begin)*i->max_nodes);
   memcpy(i->node_begin, i_tmp.node_begin, sizeof(i_tmp.node_begin));
   i->node_end = malloc(sizeof(i->node_end)*i->max_nodes);
   memcpy(i->node_end, i_tmp.node_end, sizeof(i_tmp.node_end));
   i->max_cpu = 0;
   i->cpu_to_node = 0;

   set_machine_full(i);
}

struct i_v5 {
   int sorted_by_rdt;
   char hostname[32];
   int max_nodes;
   int sampling_rate;
   uint64_t node_begin[S_MAX_NODE];
   uint64_t node_end[S_MAX_NODE];
};

static void read_header_v5(FILE *stream, struct i *i) {
   struct i_v5 i_tmp;
   assert(fread(&i_tmp, sizeof(i_tmp), 1, stream) == 1);

   i->sampling_rate = i_tmp.sampling_rate;
   i->sorted_by_rdt = i_tmp.sorted_by_rdt;
   i->max_nodes = S_MAX_NODE;
   i->node_begin = malloc(sizeof(i->node_begin)*i->max_nodes);
   memcpy(i->node_begin, i_tmp.node_begin, sizeof(i_tmp.node_begin));
   i->node_end = malloc(sizeof(i->node_end)*i->max_nodes);
   memcpy(i->node_end, i_tmp.node_end, sizeof(i_tmp.node_end));
   i->max_cpu = 0;
   i->cpu_to_node = 0;

   set_machine_full(i);
}

static void read_header_v6(FILE *stream, struct i *i) {
   assert(fread(i, sizeof(*i), 1, stream) == 1);
   i->node_begin = malloc(sizeof(*i->node_begin)*i->max_nodes);
   assert(fread(i->node_begin, sizeof(*i->node_begin)*i->max_nodes, 1, stream) == 1);
   i->node_end = malloc(sizeof(*i->node_end)*i->max_nodes);
   assert(fread(i->node_end, sizeof(*i->node_end)*i->max_nodes, 1, stream) == 1);
   i->cpu_to_node = malloc(sizeof(*i->cpu_to_node)*i->max_cpu);
   assert(fread(i->cpu_to_node, sizeof(*i->cpu_to_node)*i->max_cpu, 1, stream) == 1);
   set_machine_full(i);
}


sample_reader_f read_sample;
read_extra_header_f read_header;
read_perf_f read_perf_events;
void set_version(struct h *h) {
   if(h->version != S_VERSION) {
      switch(h->version) {
         case 1:
            read_sample = fread_v1;
            read_perf_events = read_perf_v1;
            read_header = read_header_v1;
            break;
         case 2:
            read_sample = fread_v2;
            read_perf_events = read_perf;
            read_header = read_header_v1;
            break;
         case 3:
            read_sample = fread_v2;
            read_perf_events = read_perf;
            read_header = read_header_v3;
            break;
         case 4:
            read_sample = fread_v2;
            read_perf_events = read_perf;
            read_header = read_header_v4;
            break;
         case 5:
            read_sample = fread_v3;
            read_perf_events = read_perf;
            read_header = read_header_v4;
            break;
         case 6:
         case 7:
            read_sample = fread_v3;
            read_perf_events = read_perf;
            read_header = read_header_v5;
            break;
         default:
            fprintf(stderr, "VERSION MISMATCH ! This parser is configured for version %d, seen version %d\n",
                  S_VERSION,
                  h->version);
            exit(-1);
      }
      if(verbose)
         printf("Warning: you are parsing a file from an old version of Memprof - compatibility mode enabled.\n");
   } else {
      read_sample = fread;
      read_perf_events = read_perf;
      read_header = read_header_v6;
   }
}
