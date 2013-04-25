#include "parse.h"
#include "builtin-sched.h"


static int nb_sched_apps = 0;              
static char **sched_apps = NULL;
void sched_add_application(char *app) {
   sched_apps = realloc(sched_apps, (nb_sched_apps+1)*sizeof(*sched_apps));
   sched_apps[nb_sched_apps] = strdup(app); 
   nb_sched_apps++;
}

static rbtree sched_tree;
void sched_init() {
   sched_tree = rbtree_create();
}

void sched_parse(struct s* s) {
   if(!s->ibs_dc_phys)
      return;

   int app_num = -1, i;
   for(i = 0; i < nb_sched_apps; i++) {
      if(!strcmp(get_app(s), sched_apps[i])) {
         app_num = i;
         break;
      }
   }
   if(app_num == -1)
      return;

   int *v = rbtree_lookup(sched_tree, (void*)(long)get_tid(s), pointer_cmp);
   if(!v) {
      v = calloc(MAX_NODE, sizeof(*v));
      rbtree_insert(sched_tree, (void*)(long)get_tid(s), v, pointer_cmp);
   }
   v[phys_to_node(s->ibs_dc_phys)]++;
}


static int sched_do(void *key, void *value) {
   int max = 0, max_index = 0, i, *v = value;
   for(i = 0; i < MAX_NODE; i++) {
      if(v[i] > max) {
         max = v[i];
         max_index = i;
      }
   }
   if(sched_setaffinity((int)(long)key, sizeof(cpu_set_t), die_cpu_set(max_index)) < 0) {
      printf("Error when setting affinity of pid %d\n", (int)(long)key);
   }
   return 0;
}

void sched_show() {
   rbtree_print(sched_tree, sched_do);
}

