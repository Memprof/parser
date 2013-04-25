#include "../parse.h"
#include "machine.h"
#include "compatibility-machine.h"

extern int max_node;
extern uint64_t *memory_bounds;
static int mask_init = 0;
static cpu_set_t mask[4];

#define MEMORY_SIZE_BERTHA (32LL*ONE_GB)
static uint64_t memory_bounds_bertha[MAX_NODE] = {
   2LL*ONE_GB + MEMORY_SIZE_BERTHA/4LL,
   2LL*ONE_GB + 2LL*MEMORY_SIZE_BERTHA/4LL,
   2LL*ONE_GB + 3LL*MEMORY_SIZE_BERTHA/4LL,
   2LL*ONE_GB + 4LL*MEMORY_SIZE_BERTHA/4LL,
};
int cpu_to_node_bertha(int cpu) {
   int die = (cpu % 4);
   if(die == 1)
      die = 3;
   else if(die == 3)
      die = 1;
   return die;
}
cpu_set_t* die_cpu_set_bertha(int die) {
   if(mask_init)
      return &mask[die];
   mask_init = 1;

   /** INIT Cpuset ***/
   CPU_ZERO(&mask[0]);
   CPU_ZERO(&mask[1]);
   CPU_ZERO(&mask[2]);
   CPU_ZERO(&mask[3]);

   CPU_SET(0, &mask[0]);
   CPU_SET(4, &mask[0]);
   CPU_SET(8, &mask[0]);
   CPU_SET(12, &mask[0]);

   CPU_SET(3, &mask[1]);
   CPU_SET(7, &mask[1]);
   CPU_SET(11, &mask[1]);
   CPU_SET(15, &mask[1]);

   CPU_SET(2, &mask[2]);
   CPU_SET(6, &mask[2]);
   CPU_SET(10, &mask[2]);
   CPU_SET(14, &mask[2]);

   CPU_SET(1, &mask[3]);
   CPU_SET(5, &mask[3]);
   CPU_SET(9, &mask[3]);
   CPU_SET(13, &mask[3]);

   return &mask[die];
}

/* Parapluie */
#define MEMORY_SIZE_PARA (48LL*ONE_GB)
uint64_t memory_bounds_para[MAX_NODE] = {
   1LL*ONE_GB + MEMORY_SIZE_PARA/4LL,
   1LL*ONE_GB + 2LL*MEMORY_SIZE_PARA/4LL,
   1LL*ONE_GB + 3LL*MEMORY_SIZE_PARA/4LL,
   1LL*ONE_GB + 4LL*MEMORY_SIZE_PARA/4LL,
};
int cpu_to_node_para(int cpu) {
   if(cpu < 12)
      return cpu/6;
   if(cpu < 18)
      return 3;
   return 2;
}
cpu_set_t* die_cpu_set_para(int die) {
   if(mask_init)
      return &mask[die];
   mask_init = 1;

   /** INIT Cpuset ***/
   CPU_ZERO(&mask[0]);
   CPU_ZERO(&mask[1]);
   CPU_ZERO(&mask[2]);
   CPU_ZERO(&mask[3]);

   CPU_SET(0, &mask[0]);
   CPU_SET(1, &mask[0]);
   CPU_SET(2, &mask[0]);
   CPU_SET(3, &mask[0]);
   CPU_SET(4, &mask[0]);
   CPU_SET(5, &mask[0]);

   CPU_SET(6, &mask[1]);
   CPU_SET(7, &mask[1]);
   CPU_SET(8, &mask[1]);
   CPU_SET(9, &mask[1]);
   CPU_SET(10, &mask[1]);
   CPU_SET(11, &mask[1]);

   CPU_SET(12, &mask[3]);
   CPU_SET(13, &mask[3]);
   CPU_SET(14, &mask[3]);
   CPU_SET(15, &mask[3]);
   CPU_SET(16, &mask[3]);
   CPU_SET(17, &mask[3]);

   CPU_SET(18, &mask[2]);
   CPU_SET(19, &mask[2]);
   CPU_SET(20, &mask[2]);
   CPU_SET(21, &mask[2]);
   CPU_SET(22, &mask[2]);
   CPU_SET(23, &mask[2]);

   return &mask[die];
}


uint64_t get_memory_size_bertha() {
   return MEMORY_SIZE_BERTHA;
}

uint64_t get_memory_size_para() {
   return MEMORY_SIZE_PARA;
}

void set_machine_compat(char *hostname) {
   if(!strcmp(hostname, "sci100")) {
      cpu_to_node = cpu_to_node_bertha;
      die_cpu_set = die_cpu_set_bertha;
      memory_bounds = memory_bounds_bertha;
      get_memory_size = get_memory_size_bertha;
   } else if(!strncmp(hostname, "parapluie", sizeof("parapluie")-1)
               || !strncmp(hostname, "stremi", sizeof("stremi")-1)
               || !strncmp(hostname, "valencia", sizeof("valencia")-1)) {
      cpu_to_node = cpu_to_node_para;
      die_cpu_set = die_cpu_set_para;
      memory_bounds = memory_bounds_para;
      get_memory_size = get_memory_size_para;
   } else {
      die("Unknown machine '%s'\n", hostname);
   }
}
