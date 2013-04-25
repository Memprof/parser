#include "parse.h"
#include "builtin-get-npages.h"

static char *name_filter = NULL;
static int nb_pages = 0;
static rbtree pages;
void get_npages_set(char *name) {
   name_filter = name;
}

void get_npages_init() {
   assert(name_filter);
   pages = rbtree_create();
   printf("Looking for %s\n", name_filter);
}

void get_npages_parse(struct s* s) {
   struct dyn_lib* l = sample_to_mmap(s);
   if(strcmp(l->name, name_filter))
      return;

   uint64_t page = s->ibs_dc_phys % PAGE_SIZE;
   if(rbtree_lookup(pages, (void*)page, pointer_cmp))
      return;

   nb_pages++;
   rbtree_insert(pages, (void*)page, (void*)1, pointer_cmp);
   printf("Found page on node %d\n", get_addr_node(s));
}

void get_npages_show() {
   printf("NB PAGES  for %s : %d\n", name_filter, nb_pages);
}




