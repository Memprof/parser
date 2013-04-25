#ifndef BUILTIN_MEMORY_REP
#define BUILTIN_MEMORY_REP
#include "memprof-structs.h"

void memory_repartition_init();
void memory_repartition_parse(struct s* s);
void memory_repartition_show();

#endif
