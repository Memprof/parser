#include "parse.h"
#include "builtin-sql.h"
/**
BEGIN TRANSACTION;
CREATE TABLE access (target_node NUMERIC, aid NUMERIC, callchain BLOB, cause1 NUMERIC, cause2 NUMERIC, cause3 TEXT, cause4 NUMERIC, distant NUMERIC, function TEXT, latency NUMERIC, mode TEXT, node NUMERIC, oid NUMERIC, tid NUMERIC);
INSERT INTO access VALUES(0,1,'slip
  slop
    slup',1,2,0,0,1,'titi',100,'user',1,1,1);
INSERT INTO access VALUES(0,2,'slip',0,1,0,0,1,1,120,'kern',2,1,1);
INSERT INTO access VALUES(0,3,'slop
',1,0,0,0,1,2,1000,'user',3,2,1);
INSERT INTO access VALUES(0,4,'slup',0,1,0,0,1,'fhdhg',150,'kern',1,1,2);


CREATE TABLE obj (alloc_thread NUMERIC, code_location TEXT, oid NUMERIC, size NUMERIC, type TEXT);
INSERT INTO obj VALUES(1,'malloc(slip)',1,15,'dyn_obj');
INSERT INTO obj VALUES(1,'malloc(slop)',2,1044,'dyn_obj');


CREATE TABLE thread (app TEXT, pid NUMERIC, tid NUMERIC);                                                                                                                                            
INSERT INTO thread VALUES('stream',1,1);
INSERT INTO thread VALUES('stream',1,2);
CREATE INDEX oid ON obj(oid ASC);
CREATE INDEX tid ON thread(tid ASC);
COMMIT;
**/

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

