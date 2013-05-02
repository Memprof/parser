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
#include "../parse.h"
#include "process.h"

static rbtree pid_tree;
static int process_initiated;
void process_init() {
   if(process_initiated)
      return;

   process_initiated = 1;
   pid_tree = rbtree_create();
}

void process_reset(int pid) {
   process_init();
   /* Not done. No need to because new mmaps will replace the mmaps of the old process ? */
}

struct process* find_process(int pid) {
   process_init();

   struct process *p = rbtree_lookup(pid_tree, (void*)(long)pid, int_cmp);
   return p;
}

struct process* find_process_safe(int pid) {
   struct process *p = find_process(pid);
   if(!p) {
      p = calloc(1, sizeof(*p));
      p->name = "";
      p->mmapped_dyn_lib = rbtree_create();
      p->allocated_obj = rbtree_create();
      rbtree_insert(pid_tree, (void*)(long)pid, p, int_cmp);
   }
   return p;
}

