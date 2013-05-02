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
