#include "../parse.h"
#include "machine.h"
#include "compatibility-machine.h"

int (*cpu_to_node)(int cpu);
cpu_set_t* (*die_cpu_set)(int die);
uint64_t (*get_memory_size)(void);

int max_node;
int max_real_node;
uint64_t *memory_bounds;

int max_cpu;
static int *_cpu_to_node;

int phys_to_node(uint64_t addr) {
   int phys_node;
   for(phys_node = 0; phys_node < max_real_node; phys_node++) {
      if(addr < memory_bounds[phys_node]) {
         return phys_node;
      }
   }
   return phys_node;
}

static int cpu_to_node_generic(int cpu) {
   assert(cpu >= 0 && cpu < max_cpu);
   return _cpu_to_node[cpu];
}

static cpu_set_t* die_cpu_set_generic(int die) {
   int i;
   cpu_set_t *mask = malloc(sizeof(*mask));
   CPU_ZERO(mask);

   for(i = 0; i < max_cpu; i++) {
      if(_cpu_to_node[i] == die)
         CPU_SET(i, mask);
   }

   return mask;
}

uint64_t get_memory_size_generic() {
   return memory_bounds[max_real_node - 1];
}

void set_machine_full(struct i *i) {
   int j;

   if(!i->cpu_to_node) {
      set_machine_compat(i->hostname); //hardcoded machine information
   } else {
      max_cpu = i->max_cpu;
      _cpu_to_node = malloc(sizeof(*_cpu_to_node)*i->max_cpu);
      for(j = 0; j < i->max_cpu; j++) {
         _cpu_to_node[j] = i->cpu_to_node[j];
      }
      cpu_to_node = cpu_to_node_generic;
      die_cpu_set = die_cpu_set_generic;
   }

   max_node = i->max_nodes + 1;
   max_real_node = i->max_nodes;
   memory_bounds = malloc(sizeof(*memory_bounds)*max_real_node);
   for(j = 0; j < max_real_node; j++) {
      memory_bounds[j] = i->node_end[j]*(4LL*1024LL);
   }
   get_memory_size = get_memory_size_generic;
}
