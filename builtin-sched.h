#ifndef BUILTIN_SCHED
#define BUILTIN_SCHED 1

void sched_init();
void sched_parse(struct s* s);
void sched_show();
void sched_add_application(char *app);
#endif

