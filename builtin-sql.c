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
#include "builtin-sql.h"

static int nb_obj = 0;
static rbtree tid, oid;
struct ttid {
   int tid, pid;
   char *app;
};
struct ooid {
   int oid, size, alloc_thr;
   char *code, *type;
};


void sql_init() {
   tid = rbtree_create();
   oid = rbtree_create();

   printf("BEGIN TRANSACTION;\n");
   printf("CREATE TABLE access (target_node NUMERIC, aid NUMERIC, callchain BLOB, cause1 NUMERIC, cause2 NUMERIC, cause3 TEXT, cause4 NUMERIC, distant NUMERIC, function TEXT, latency NUMERIC, mode TEXT, node NUMERIC, oid NUMERIC, tid NUMERIC);\n");
   printf("CREATE TABLE obj (alloc_thread NUMERIC, code_location TEXT, oid NUMERIC, size NUMERIC, type TEXT);\n");
   printf("CREATE TABLE thread (app TEXT, pid NUMERIC, tid NUMERIC);\n");
}
static int aid = 0;
void sql_parse(struct s* s) {
   struct symbol *ob = sample_to_static_object(s); //unsafe, may return NULL ; static object
   struct symbol *ob2 = sample_to_variable2(s); //idem ; malloc object
   struct dyn_lib* ob3 = sample_to_mmap(s);
   char *obj = NULL;
   if(ob2)
      obj = ob2->function;
   if(!obj && ob)
      obj = ob->function;
   if(!obj && ob3)
      obj = ob3->name;

   struct ooid *o = rbtree_lookup(oid, obj, (compare_func)pointer_cmp);
   if(!o) {
      o = calloc(1, sizeof(*o));
      o->oid = nb_obj++;
      o->alloc_thr = ob2?ob2->tid:get_pid(s);
      o->size = 0;
      o->code = obj;
      o->type = "nimps";
      rbtree_insert(oid, obj, o, (compare_func)pointer_cmp);
      printf("INSERT INTO obj VALUES(%d,'%s',%d,%d,'%s');\n",
         o->alloc_thr, o->code, o->oid, o->size, o->type
      );
   }

   struct ttid *t = rbtree_lookup(tid, (void*)(long)get_tid(s), (compare_func)pointer_cmp);
   if(!t) {
      t= calloc(1, sizeof(*t));
      t->tid = get_tid(s);
      t->pid = get_pid(s);
      t->app = s->comm;
      rbtree_insert(tid, (void*)(long)get_tid(s), t, (compare_func)pointer_cmp);
      printf("INSERT INTO thread VALUES('%s',%d,%d);\n",
            t->app, t->pid, t->tid);
   }

   printf("INSERT INTO access VALUES(%d,%d,'%s',%d,%d,%d,%d,%d,'%s',%d,'%s',%d,%d,%d);\n",
      get_addr_node(s),
      aid++,
      "rien",
      0,0,0,0,
      is_distant(s),
      get_symbol(s)->function,
      get_latency(s),
      is_kernel(s)?"kernel":"user",
      cpu_to_node(s->cpu),
      o->oid,
      t->tid
   );
}

void sql_show() {
   printf("COMMIT;\n");
}

