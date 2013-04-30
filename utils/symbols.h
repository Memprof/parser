#ifndef SYMB_LIB_H
#define SYMB_LIB_H 1

#define KERNEL_NAME "[kernel]"

struct mmapped_dyn_lib;
struct dyn_lib {
   char *name;
   uint64_t real_size;
   rbtree symbols;
   struct mmapped_dyn_lib *enclosing_lib;
};

/* See various functions in process.h to know how to get a struct symbol from an IBS sample */
struct symbol {
   int uid;
   void *ip; /** WARN: this is the translated IP inside the mmaped file. Hard to link with an actual RIP. */
   struct dyn_lib *file;
   char *function;
   uint64_t func_offset;   /* SASHA */

   int tid;
   int cpu;
   size_t size;
   
   uint64_t callchain_size;
   uint64_t callchain[];
};


void init_symbols();
struct dyn_lib* find_lib(char *name);
struct dyn_lib* get_invalid_lib();
void add_lib(char *file);
char *short_name(char *libname);

struct symbol* ip_to_symbol(struct dyn_lib *d, void *ip);

#endif
