#ifndef MACHINE_H
#define MACHINE_H 1

#include "memprof-structs.h"

extern int max_node;
extern int max_real_node;
extern int max_cpu;

/* Machine specific */
extern uint64_t (*get_memory_size)(void);
int phys_to_node(uint64_t addr);
extern int (*cpu_to_node)(int cpu);
extern cpu_set_t* (*die_cpu_set)(int die);

/* V3 and beyond have a special header which includes machine specific information */
void set_machine_full(struct i *i);

#endif
