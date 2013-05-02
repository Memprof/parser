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
#include "builtin-object.h"

#define SPACES 5
static int uid, nb_tids;
static int total_calls;
static rbtree tids, timeline, top;
void obj_set(int _uid) {
   uid = _uid;
}

void obj_init() {
   tids = rbtree_create();
   timeline = rbtree_create();
   top = rbtree_create();
   assert(uid != 0);
}

int callchain_printed = 0;
void print_callchain(struct s* s) {
   if(callchain_printed)
      return;
   callchain_printed = 1;
   
   struct symbol *ss = sample_to_variable2(s);

   int i;
   printf("Object was allocated from the following path:\n");
   printf("\t%s [%p]\n", ss->function, (void*)s->rip);
   for(i = 0; i < ss->callchain_size; i++) {
      printf("\t%*.*s%s [%p]\n", (i+1)*2,(i+1)*2, "", sample_to_callchain(s, ss, i)->function, (void*)ss->callchain[i]);
   }

   printf("Object is here: %lx-%lx\n",
        (long unsigned) ss->ip,
        (long unsigned) &(((char*)ss->ip)[ss->size]));
}

void obj_parse(struct s* s) {
   struct symbol *ob2 = sample_to_variable2(s);
   if(!ob2 || ob2->uid != uid)
      return;
   print_callchain(s);

   struct s *ss = malloc(sizeof(*ss));
   memcpy(ss, s, sizeof(*ss));

   rbtree_insert(timeline, (void*)(long)s->rdt, ss, pointer_cmp);
   int *value = rbtree_lookup(tids, (void*)(long)get_tid(s), int_cmp);
   if(!value) {
      value = calloc(2, sizeof(*value));
      value[0] = nb_tids;
      nb_tids++;
      rbtree_insert(tids, (void*)(long)get_tid(s), value, int_cmp);
   }
   value[1] = value[1] + 1;
}

int print_timeline(void *key, void *value) {
   int tid = get_tid(value);
   int *v = rbtree_lookup(tids, (void*)(long)tid, int_cmp);
   assert(v);

   char *func = get_symbol(value)->function;
   int i;
   for(i = 0; i < v[0]; i++) {
      printf("%*.*s", SPACES, SPACES, "");
   }
   if(is_load(value))
      printf("  x  ");
   else
      printf("  *  ");
   for(i = v[0]; i < nb_tids; i++) {
      printf("%*.*s", SPACES, SPACES, "");
   }
   printf("%s\n", func);

   int *nb_calls = rbtree_lookup(top, func, (compare_func)strcmp);
   if(!nb_calls) {
      nb_calls = calloc(1, sizeof(*nb_calls));
      rbtree_insert(top, func, nb_calls, (compare_func)strcmp);
   }
   (*nb_calls)++;
   total_calls++;

   return 0;
}

static __unused int top_cmp(const void *a, const void* b) {
   const rbtree_node _a = *(const rbtree_node*) a;
   const rbtree_node _b = *(const rbtree_node*) b;
   return *(int*)_b->value - *(int*)_a->value;
}

void obj_show() {
   int i;

   printf("Timeline of memory access: (x = load, * = write, one column per tid):\n"),
   rbtree_print(timeline, print_timeline);

   printf("\n\n\n");
   printf("Top functions accessing the object (#access, function):\n");
   rbtree_key_val_arr_t *sorted = rbtree_sort(top, top_cmp);
   for(i = 0; i < sorted->nb_elements; i++) { 
      int *vals = (int*)sorted->vals[i]->value;
      printf("%8d %5.2f%% %s\n",
            vals[0],
            100.*((float)vals[0])/((float)total_calls),
            (char*)sorted->vals[i]->key);
   }
}

void obj_modifier(int m) {
}

