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
#include "parse.h"
#include "builtin-sched.h"

/*
 * Sets the affinity of threads.
 * Places threads close to their memory.
 */
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
      v = calloc(max_node, sizeof(*v));
      rbtree_insert(sched_tree, (void*)(long)get_tid(s), v, pointer_cmp);
   }
   v[phys_to_node(s->ibs_dc_phys)]++;
}


static int sched_do(void *key, void *value) {
   int max = 0, max_index = 0, i, *v = value;
   for(i = 0; i < max_node; i++) {
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

