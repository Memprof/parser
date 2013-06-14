#include "parse.h"
#include "builtin-get-npages.h"

/* Get the number of touched pages of a given library */
/* E.g., -- libc.so */

struct page {
   uint64_t phys;
   uint64_t nb_migr;
};

static rbtree pages;
static int nb_migrated_pages = 0;

void get_migr_stats_init() {
   pages = rbtree_create();
}

void get_migr_stats_parse(struct s* s) {
   uint64_t vpage = s->ibs_dc_linear % PAGE_SIZE;
   uint64_t ppage = s->ibs_dc_phys % PAGE_SIZE;

   struct page *p = rbtree_lookup(pages, (void*)vpage, pointer_cmp);
   if(!p) {
      p = calloc(1, sizeof(*p));
      p->phys = ppage;
      p->nb_migr = 0;
      rbtree_insert(pages, (void*)vpage, (void*)p, pointer_cmp);
   } else if(p->phys != ppage) {
      p->phys = ppage;
      p->nb_migr++;
      if(p->nb_migr == 1)
         nb_migrated_pages++;
   }
}

void get_migr_stats_show() {
   printf("NB MIGRATED PAGES %d\n", nb_migrated_pages);
}




