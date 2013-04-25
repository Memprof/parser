#ifndef BUILTIN_DUMP
#define BUILTIN_DUMP
#include "memprof-structs.h"

void dump_init();
void dump_parse(struct s* s);
void dump_show();
void dump_modifier(int m);

#endif
