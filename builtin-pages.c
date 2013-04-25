#include "parse.h"
#include "builtin-pages.h"


static int uid;
static uint64_t last_page, max_hole, nb_pages, min_page = -1, max_page, size;
static rbtree pages;
void pages_init() {
   pages = rbtree_create();
}
void pages_modifier(int m) {
}
void pages_object_set(int _uid) {
   uid = _uid;
}
void pages_parse(struct s* s) {
   if(uid) {
      struct symbol *ob2 = sample_to_variable2(s);
      if(!ob2 || ob2->uid != uid)
         return;
      size = ob2->size;
   }
   void *p = (void*)(s->ibs_dc_linear & PAGE_MASK);
   int *value = rbtree_lookup(pages, p, pointer_cmp);
   if(!value) {
      value = calloc(1, sizeof(*value));
      rbtree_insert(pages, p, value, pointer_cmp);
   }
   value[0] = value[0] + 1;

   max_page = max((uint64_t)p, max_page);
   min_page = min(min_page, (uint64_t)p);
}

static int pages_print(void *k, void *v) {
   nb_pages++;
   if(last_page)
      max_hole = max(max_hole, last_page - (uint64_t)k);
   printf("%15lx [%15lx]: %d\n", (long unsigned)k, last_page - (uint64_t)k, *(int*)v);

   last_page = (uint64_t)k;
   return 0;
}
void pages_show() {
   rbtree_print(pages, pages_print);


   printf("#Nb pages: %5d ; Max Hole: %5x (%d pages)\n", (int)nb_pages, (int)max_hole, (int)max_hole/PAGE_SIZE);
   printf("#Studied object size: %lx (%d pages)\n", size, (int)size/PAGE_SIZE);
   printf("#--virt %lx-%lx\n", min_page, max_page);
}

