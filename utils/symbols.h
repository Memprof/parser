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

/* A struct symbol represents either an object or a function
 * because fundamentally a function and an object are the same things (data in memory).
 * To clarify the code, some fields are anonymous unions.
 */
struct symbol {
   union {
      int uid;
      int object_uid;
      int function_uid;
   };
   union {
      void *ip; /** WARN: this is the translated IP inside the mmaped file. Hard to link with an actual RIP. */
                /** It is the virtual address of the function, not of the access. To have the IP of the access, use struct s -> rip. */
      void *function_virt_addr;
      void *object_virt_addr;
   };
   union {
      struct dyn_lib *file;
      struct dyn_lib *mmap_containing_function;
      struct dyn_lib *mmap_containing_object;
   };
   union {
      char *function_name;
      char *object_name; // the object "name" if the name of the function that allocated it +offset (e.g. myfun()+0x60)
   };

   /** WARN: all fields bellow are only initialized for objects (vs functions) */
   int allocator_tid;   // tid that allocated the object
   int allocator_cpu;   // cpu from which the object was allocated
   size_t size;         // size of the object 
   
   uint64_t callchain_size;   // array of the array bellow
   uint64_t callchain[];      // callchain of functions that led to the object allocation
};


void init_symbols();
struct dyn_lib* find_lib(char *name);
struct dyn_lib* get_invalid_lib();
void add_lib(char *file);
char *short_name(char *libname);

struct symbol* ip_to_symbol(struct dyn_lib *d, void *ip);

#endif
