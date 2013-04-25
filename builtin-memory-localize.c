#include "parse.h"
#include "builtin-memory-localize.h"

static int nb_samples;
static rbtree touched_mmaped_zones;
struct mmaped_zone_stat {
   char *name;
   int nb_accesses;
   int nb_local_accesses;
   int nb_remote_accesses;
};

void memory_localize_init() {
   touched_mmaped_zones =  rbtree_create();
   nb_samples = 0;
}
void memory_localize_parse(struct s* s) {
   if(!s->ibs_dc_phys || s->ibs_dc_phys > get_memory_size())
      return;

   /*struct symbol *ss = get_symbol(s);
   if(strstr(ss->function, "@plt"))
      return;*/

   struct dyn_lib * l = sample_to_mmap(s);
   if(!l)
      return;

   char *name = l->name;
   char *var = sample_to_variable(s);
   if(var && !strncmp(var, "thread-stack", sizeof("thread-stack") - 1))
      name = "[stack]";

   struct mmaped_zone_stat* stat = rbtree_lookup(touched_mmaped_zones, name, (compare_func)strcmp);
   if(!stat) {
      stat = calloc(1, sizeof(*stat));
      stat->name = strdup(name);
      rbtree_insert(touched_mmaped_zones, name, stat, (compare_func)strcmp);
   }

   nb_samples++;
   stat->nb_accesses++;
   //printf("%d %d %p %s\n", get_addr_node(s),  cpu_to_node(s->cpu), (void*)s->ibs_dc_phys, l->name);
   if(get_addr_node(s) != cpu_to_node(s->cpu)) {
      stat->nb_remote_accesses++;
   } else {
      stat->nb_local_accesses++;
   }
}

static int localize_sort(const void *_a, const void *_b) {
   struct mmaped_zone_stat *a = (*(const rbtree_node*)_a)->value;
   struct mmaped_zone_stat *b = (*(const rbtree_node*)_b)->value;
   return b->nb_accesses - a->nb_accesses;
}

void memory_localize_show() {
   int i;
   rbtree_key_val_arr_t *sorted = rbtree_sort(touched_mmaped_zones, localize_sort);
   for(i = 0; i < sorted->nb_elements; i++) { 
      struct mmaped_zone_stat* val = sorted->vals[i]->value;
      printf("%30s: %7d (%5.2f%%) [%.2f%% local]\n", short_name(val->name), val->nb_accesses, 100.*((float)val->nb_accesses)/((float)nb_samples), 100.*((float)val->nb_local_accesses)/((float)val->nb_local_accesses+val->nb_remote_accesses));
   }
}

