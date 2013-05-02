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
#include "builtin-top-functions.h"

/*
 * Outputs the top functions (sorted by # of samples)
 */

static int cluster_by_lib = 0;
void top_fun_modifier(int m) {
   cluster_by_lib = m;
}

static rbtree top_tree;
static int nb_samples;
void top_fun_init() {
   top_tree = rbtree_create();
   nb_samples = 0;
}

static int sym_cmp(void *a, void *b) {
   struct symbol *c = a, *d = b;
   if(cluster_by_lib && !strcmp(c->file->name, d->file->name))
      return 0;
   return pointer_cmp(a, b);
}

void top_fun_parse(struct s* s) {
   struct symbol *sym = get_symbol(s);
   if(sym) {
      int *value = rbtree_lookup(top_tree, sym, /*pointer_cmp*/sym_cmp);
      if(!value) {
         value = calloc(1+2+(max_node),sizeof(*value));
         rbtree_insert(top_tree, sym, value, pointer_cmp);
      }
      value[0]++;
      if(s->ibs_dc_phys) {
         if(cpu_to_node(s->cpu) != get_addr_node(s)) {
            value[1]++; //distant
         } else { 
            value[2]++; //local
         }
         value[3+get_addr_node(s)]++;
      }
      nb_samples++;
   }
}

static __unused int top_cmp(const void *a, const void* b) {
   const rbtree_node _a = *(const rbtree_node*) a;
   const rbtree_node _b = *(const rbtree_node*) b;
   return *(int*)_b->value - *(int*)_a->value;
}
void top_fun_show() {
   /* Sort the rbtree by count value */
   rbtree_key_val_arr_t *sorted = rbtree_sort(top_tree, top_cmp);
   int i;
   for(i = 0; i < sorted->nb_elements; i++) { /* and print */
      int *vals = (int*)sorted->vals[i]->value;
      char *color = RED;
      if((float)100.*((float)vals[2])/((float)vals[1]+vals[2]) > 60.) 
         color = "";
      printf("%8d %5.2f%% %50s [%30s] (%s%.2f%%"RESET" local) [accesses to", 
            vals[0],
            100.*((float)vals[0])/((float)nb_samples),
            cluster_by_lib?"-":(((struct symbol*)sorted->vals[i]->key)->function),
            short_name(((struct symbol*)sorted->vals[i]->key)->file->name),
            color,
            (float)100.*((float)vals[2])/((float)vals[1]+vals[2]));
      int j;
      for(j = 0; j < max_node; j++) {
         printf(" %6d", vals[3+j]);
      }
      printf("]\n");
   }
}

