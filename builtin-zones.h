#ifndef BUILTIN_zone
#define BUILTIN_zone
#include "memprof-structs.h"

void zone_init();
void zone_parse(struct s* s);
void zone_show();
void zone_modifier(int m);
void zone_set_cluster_size(int size);

#endif
