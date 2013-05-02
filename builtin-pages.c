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
   printf("#Page virtual address [virtual address span to previous page] #accesses to the page\n");
   rbtree_print(pages, pages_print);

   printf("#Nb pages: %5d ; Max Hole: %5x (%d pages)\n", (int)nb_pages, (int)max_hole, (int)max_hole/PAGE_SIZE);
   printf("#Studied object size: %lx (%d pages)\n", size, (int)size/PAGE_SIZE);
   printf("#--virt %lx-%lx\n", min_page, max_page);
}

