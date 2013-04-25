#ifndef BUILTIN_STACK_REP
#define BUILTIN_STACK_REP
#include "memprof-structs.h"

void stack_repartition_init();
void stack_repartition_parse(struct s* s);
void stack_repartition_show();

#endif
