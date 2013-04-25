#ifndef BUILTIN_STAT
#define BUILTIN_STAT 1

#define NB_STAT_VALUES (MAX_CPU+1)
#define STAT_TID_SUM_SAMPLES (NB_STAT_VALUES-1)

void stats_init();
void stats_parse(struct s* s);
void stats_show();

#endif
