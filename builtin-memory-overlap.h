#ifndef BUILTIN_MEMORY_OVERLAP
#define BUILTIN_MEMORY_OVERLAP 1

#define NB_STORED_FIELDS ((nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids)*3 + 2)
#define LOAD_INDEX(app) ((nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids) + app)
#define STORE_INDEX(app) ((nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids)*2 + app)
#define IN_KERNEL ((nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids)*3)
#define USERLAND (IN_KERNEL + 1)

void overlap_add_application(char *app);
void overlap_add_tid(int tid);
void overlap_add_pid(int pid);
void overlap_add_tid_unshared(int tid);
void overlap_init();
void overlap_parse(struct s* s);
void overlap_show();

#endif

