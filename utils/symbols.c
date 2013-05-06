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
#include "rbtree.h"
#include "symbols.h"
#include <glib.h>
#include <libelf.h>
#include <gelf.h>
#include <elf.h>
#include <fcntl.h>
#define PACKAGE  //hack for Gentoo
#include <bfd.h>
#include <limits.h>

char* kallsyms[] = {
  "./kallsyms.raw",
  "./kallsyms.raw.lzo",
  "/proc/kallsyms"
};

static GHashTable* parsed_files;
struct dyn_lib* anon_lib;
static struct dyn_lib* stack_lib;
static struct dyn_lib* data_lib;
static struct dyn_lib* heap_lib;
static struct dyn_lib* vdso_lib;
static struct dyn_lib* invalid_lib;
static struct dyn_lib* invalid_addr;

static struct dyn_lib* dyn_lib_new(char *name) {
   struct dyn_lib *d = calloc(1, sizeof(*d));
   d->name = name;
   d->symbols = rbtree_create();
   d->real_size = 0;
   return d;
}

static struct symbol* symb_new() {
   struct symbol* s = calloc(1, sizeof(*s));
   return s;
}

static int symbol_cmp(void *left, void *right) {
   if(left > right) {
      return 1;
   } else if(left < right) {
      return -1;
   } else if(left == right) {
      return 0;
   }
   return 0; //Pleases GCC
}


static void symb_insert(struct dyn_lib *d, struct symbol* s) {
   assert(d && d->symbols && s);
   s->file = d;
   rbtree_insert(d->symbols, s->ip, s, symbol_cmp);
}

static void init_kernel() {
   struct dyn_lib *kernel = dyn_lib_new(KERNEL_NAME);
   g_hash_table_insert(parsed_files, KERNEL_NAME, kernel);

   char *line = NULL;
   FILE * sym_file = NULL;
   int curr_kallsym = 0;
   while(!sym_file && (curr_kallsym < sizeof(kallsyms)/sizeof(*kallsyms))) {
      sym_file = open_file(kallsyms[curr_kallsym]);
      curr_kallsym++;
   }
   assert(sym_file);
   if(curr_kallsym == sizeof(kallsyms)/sizeof(*kallsyms)) {
      printf("Warn: using file " RED "%s" RESET " (might be wrong)\n", kallsyms[curr_kallsym-1]);
   } else {
      //printf("Using file %s\n", kallsyms[curr_kallsym-1]);
   }

   char useless_char;
   char module_name[512];
   while (!feof(sym_file)) {
      line = NULL;
      size_t n, line_len = getline(&line, &n, sym_file);
      if (line_len <= 5)
         break;
      line[--line_len] = '\0';

      struct symbol *k = symb_new();
      k->function_name = malloc(400);
      int nb_lex = sscanf(line, "%p %c %s\t[%s", &k->ip, &useless_char, k->function_name, module_name);
      free(line);
      if (nb_lex < 3)
         goto fail;
      if(!k->ip)
         goto fail;
      if (nb_lex == 4) { 
         strcat(k->function_name, " [");
         strcat(k->function_name, module_name);
      }   
      symb_insert(kernel, k);
      continue;
fail:
      //printf("Line %s failed\n", line);
      free(k->function_name);
      free(k);
   }   
}


void init_symbols() {
   struct symbol *s;
   parsed_files = g_hash_table_new(g_str_hash, g_str_equal);
   init_kernel();

   s = symb_new();
   data_lib = dyn_lib_new("[data]");
   s->ip = 0;
   s->function_name = "[data]";
   symb_insert(data_lib,s);

   s = symb_new();
   stack_lib = dyn_lib_new("[stack]");
   s->ip = 0;
   s->function_name = "[stack]";
   symb_insert(stack_lib,s);

   s = symb_new();
   anon_lib = dyn_lib_new("//anon");
   s->ip = 0;
   s->function_name = "//anon";
   symb_insert(anon_lib,s);

   s = symb_new();
   heap_lib = dyn_lib_new("[heap]");
   s->ip = 0;
   s->function_name = "[heap]";
   symb_insert(heap_lib,s);

   s = symb_new();
   vdso_lib = dyn_lib_new("[vdso]");
   s->ip = 0;
   s->function_name = "[vdso]";
   symb_insert(vdso_lib,s);

   s = symb_new();
   invalid_lib = dyn_lib_new("[invalid data]");
   s->ip = 0;
   s->function_name = "[invalid data]";
   symb_insert(invalid_lib,s);

   s = symb_new();
   invalid_addr = dyn_lib_new("[invalid addr]");
   s->ip = 0;
   s->function_name = "[invalid addr]";
   symb_insert(invalid_addr,s);

   if (elf_version(EV_CURRENT) == EV_NONE)
      die("Cannot initiate libelf");
}

struct dyn_lib * find_lib(char *name) {
   if(!parsed_files)
      init_symbols();
   if(name && (name[0] != '[' || !strcmp(name, KERNEL_NAME))      /* [kernel] is ok but [stack] [vdso] etc is not */
            && !(name[0] == '/' && name[1] == '/')) { /* //anon */
      return g_hash_table_lookup(parsed_files, name);
   } else {
      if(!strncmp(name, "//ano", sizeof("//ano") -1)) { 
         /* There was a pb in the first version of perf_mmap which dumped //ano instead of //anon ... */
         return anon_lib;
      }
      if(!strcmp(name, "[stack]"))
         return stack_lib;
      if(!strcmp(name, "[heap]"))
         return heap_lib;
      if(!strcmp(name, "[vdso]"))
         return vdso_lib;
      return data_lib;
   }
}
struct dyn_lib* get_invalid_lib() {
   if(!parsed_files)
      init_symbols();
   return invalid_addr;
}

struct symbol* _ip_to_symbol(rbtree_node n, void *ip) {
   while (n != NULL) {
      if(n->left)
         assert(n->key > n->left->key);
      if(n->right)
         assert(n->key < n->right->key);

      if(ip < n->key) {
         n = n->left;
      } else {
         if(n->right && ip >= n->right->key) {
            n = n->right;
         } else {
            /* Look in the right subtree for a better candidate */
            struct symbol *best_son = _ip_to_symbol(n->right, ip);
            return best_son?best_son:n->value;
         }
      }
   }
   return NULL;
}

struct symbol* ip_to_symbol(struct dyn_lib *d, void *ip) {
   rbtree t = d->symbols;
   assert(d->symbols);

   rbtree_node n = t->root;
   return _ip_to_symbol(n, ip);
}



/********* Taken from parser-sampling **********/
static Elf_Scn *elf_section_by_name(Elf *elf, GElf_Ehdr *ep, GElf_Shdr *shp, const char *name, size_t *idx) {
   Elf_Scn *sec = NULL;
   size_t cnt = 1;
   while ((sec = elf_nextscn(elf, sec)) != NULL) {
      char *str;

      gelf_getshdr(sec, shp);
      str = elf_strptr(elf, ep->e_shstrndx, shp->sh_name);
      if (!strcmp(name, str)) {
         if (idx)
            *idx = cnt;
         break;
      }
      ++cnt;
   }
   return sec;
}
static inline uint8_t elf_sym__type(const GElf_Sym *sym) {
   return GELF_ST_TYPE(sym->st_info);
}
/** We consider static global objects as functions. **/
static inline int elf_sym__is_function(const GElf_Sym *sym) {
   return ((elf_sym__type(sym) == STT_FUNC)) && sym->st_name != 0 && sym->st_shndx
            != SHN_UNDEF;
}
static inline int elf_sym__is_object(const GElf_Sym *sym) {
   return ((elf_sym__type(sym) == STT_OBJECT)) && sym->st_name != 0 && sym->st_shndx
            != SHN_UNDEF;
}
static inline int elf_sym__is_label(const GElf_Sym *sym) {
   return elf_sym__type(sym) == STT_NOTYPE && sym->st_name != 0
            && sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS;
}
static inline const char *elf_sym__name(const GElf_Sym *sym, const Elf_Data *symstrs) {
   return symstrs->d_buf + sym->st_name;
}
static inline const char *elf_sec__name(const GElf_Shdr *shdr, const Elf_Data *secstrs) {
   return secstrs->d_buf + shdr->sh_name;
}
static inline int elf_sec__is_text(const GElf_Shdr *shdr, const Elf_Data *secstrs) {
   return strstr(elf_sec__name(shdr, secstrs), "text") != NULL;
}
#define elf_section__for_each_rel(reldata, pos, pos_mem, idx, nr_entries) \
        for (idx = 0, pos = gelf_getrel(reldata, 0, &pos_mem); \
             idx < nr_entries; \
             ++idx, pos = gelf_getrel(reldata, idx, &pos_mem))

#define elf_section__for_each_rela(reldata, pos, pos_mem, idx, nr_entries) \
        for (idx = 0, pos = gelf_getrela(reldata, 0, &pos_mem); \
             idx < nr_entries; \
             ++idx, pos = gelf_getrela(reldata, idx, &pos_mem))
static int symb_synthesize_plt_symbols(struct dyn_lib *arr, Elf *elf) {
   uint32_t nr_rel_entries, idx;
   GElf_Sym sym;
   uint64_t plt_offset;
   GElf_Shdr shdr_plt;
   GElf_Shdr shdr_rel_plt, shdr_dynsym;
   Elf_Data *reldata, *syms, *symstrs;
   Elf_Scn *scn_plt_rel, *scn_symstrs, *scn_dynsym;
   size_t dynsym_idx;
   GElf_Ehdr ehdr;
   char sympltname[1024];
   int nr = 0, symidx, err = 0;

   if (gelf_getehdr(elf, &ehdr) == NULL)
      goto out_elf_end;

   scn_dynsym
            = elf_section_by_name(elf, &ehdr, &shdr_dynsym, ".dynsym", &dynsym_idx);
   if (scn_dynsym == NULL)
      goto out_elf_end;

   scn_plt_rel
            = elf_section_by_name(elf, &ehdr, &shdr_rel_plt, ".rela.plt", NULL);
   if (scn_plt_rel == NULL) {
      scn_plt_rel
               = elf_section_by_name(elf, &ehdr, &shdr_rel_plt, ".rel.plt", NULL);
      if (scn_plt_rel == NULL)
         goto out_elf_end;
   }

   err = -1;

   if (shdr_rel_plt.sh_link != dynsym_idx)
      goto out_elf_end;

   if (elf_section_by_name(elf, &ehdr, &shdr_plt, ".plt", NULL) == NULL)
      goto out_elf_end;

   /*
    * Fetch the relocation section to find the idxes to the GOT
    * and the symbols in the .dynsym they refer to.
    */
   reldata = elf_getdata(scn_plt_rel, NULL);
   if (reldata == NULL)
      goto out_elf_end;

   syms = elf_getdata(scn_dynsym, NULL);
   if (syms == NULL)
      goto out_elf_end;

   scn_symstrs = elf_getscn(elf, shdr_dynsym.sh_link);
   if (scn_symstrs == NULL)
      goto out_elf_end;

   symstrs = elf_getdata(scn_symstrs, NULL);
   if (symstrs == NULL)
      goto out_elf_end;

   nr_rel_entries = shdr_rel_plt.sh_size / shdr_rel_plt.sh_entsize;
   plt_offset = shdr_plt.sh_offset;

   if (shdr_rel_plt.sh_type == SHT_RELA) {
      GElf_Rela pos_mem, *pos;

      elf_section__for_each_rela(reldata, pos, pos_mem, idx,
               nr_rel_entries) {
         symidx = GELF_R_SYM(pos->r_info);
         plt_offset += shdr_plt.sh_entsize;
         gelf_getsym(syms, symidx, &sym);
         snprintf(sympltname, sizeof(sympltname), "%s@plt", elf_sym__name(&sym, symstrs));
         struct symbol *s = symb_new();
         s->ip = (void*)plt_offset;
         s->function_name = strdup(sympltname);
         symb_insert(arr, s);
         ++nr;
      }
   }
   else if (shdr_rel_plt.sh_type == SHT_REL) {
      GElf_Rel pos_mem, *pos;
      elf_section__for_each_rel(reldata, pos, pos_mem, idx,
               nr_rel_entries) {
         symidx = GELF_R_SYM(pos->r_info);
         plt_offset += shdr_plt.sh_entsize;
         gelf_getsym(syms, symidx, &sym);
         snprintf(sympltname, sizeof(sympltname), "%s@plt", elf_sym__name(&sym, symstrs));
         struct symbol *s = symb_new();
         s->ip = (void*)plt_offset;
         s->function_name = strdup(sympltname);
         symb_insert(arr, s);
         ++nr;
      }
   }

   err = 0;
   out_elf_end: if (err == 0)
      return nr;
   fprintf(stderr, "%s: problems reading PLT info.\n", __func__);
   return 0;
}

char *absolute_path_to_filename(char *abs) {
   char *end = abs + strlen(abs);
   while(end > abs && *end != '/')
      end--;
   char *res = NULL;
   asprintf(&res, "%s%s", path_to_binaries?path_to_binaries:"./bin", end);
   return res; 
}

#define elf_symtab__for_each_symbol(syms, nr_syms, idx, sym) \
        for (idx = 0, gelf_getsym(syms, idx, &sym);\
             idx < nr_syms; \
             idx++, gelf_getsym(syms, idx, &sym))
#ifndef DMGL_PARAMS
#define DMGL_PARAMS      (1 << 0)       /* Include function args */
#define DMGL_ANSI        (1 << 1)       /* Include const, volatile, etc */
#endif
void add_lib(char *file) {
   if(find_lib(file))
      return;

   struct symbol *s = symb_new();
   struct dyn_lib *lib = dyn_lib_new(file);
   g_hash_table_insert(parsed_files, file, lib);

   char *local_file = absolute_path_to_filename(file);
   int fd = open(local_file, O_RDONLY);
   free(local_file);
   if (fd < 0) {
      fd = open(file, O_RDONLY);
      if(fd < 0) {
         s->ip = 0;
         assert(asprintf(&s->function_name, "[Cannot open lib %s]", file));
         symb_insert(lib, s);
         goto out_close;
      }
   } 
   s->ip = 0;
   s->function_name = "[dummy]";
   symb_insert(lib,s);

    Elf_Data *symstrs, *secstrs;
   uint32_t nr_syms;
   uint32_t idx;
   GElf_Ehdr ehdr;
   GElf_Shdr shdr;
   Elf_Data *syms;
   GElf_Sym sym;
   Elf_Scn *sec, *sec_strndx;
   int adjust_symbols = 0;
   int nr = 0, kernel = !strcmp("[kernel]", file);
   Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
   if (elf == NULL) {
      die("%s: cannot read %s ELF file.\n", __func__, file);
   }

   if (gelf_getehdr(elf, &ehdr) == NULL) {
      if(verbose)
         fprintf(stderr,"%s: cannot get elf header of %s.\n", __func__, file);
      goto out_elf_end;
   }

   sec = elf_section_by_name(elf, &ehdr, &shdr, ".symtab", NULL);
   if (sec == NULL) {
      sec = elf_section_by_name(elf, &ehdr, &shdr, ".dynsym", NULL);
      if (sec == NULL) {
         fprintf(stderr,"%s(%s): cannot get .symtab or .dynsym", __func__, file);
         goto out_elf_end;
      }
   }

   syms = elf_getdata(sec, NULL);
   if (syms == NULL)
      goto out_elf_end;

   sec = elf_getscn(elf, shdr.sh_link);
   if (sec == NULL)
      goto out_elf_end;

   symstrs = elf_getdata(sec, NULL);
   if (symstrs == NULL)
      goto out_elf_end;

   sec_strndx = elf_getscn(elf, ehdr.e_shstrndx);
   if (sec_strndx == NULL)
      goto out_elf_end;

   secstrs = elf_getdata(sec_strndx, NULL);
   if (secstrs == NULL)
      goto out_elf_end;

   nr_syms = shdr.sh_size / shdr.sh_entsize;

   memset(&sym, 0, sizeof(sym));
   if (!kernel) {
      adjust_symbols
               = (ehdr.e_type == ET_EXEC
                        || elf_section_by_name(elf, &ehdr, &shdr, ".gnu.prelink_undo", NULL)
                                 != NULL);
   }
   else
      adjust_symbols = 0;

   uint64_t biggest_addr = 0;
   elf_symtab__for_each_symbol(syms, nr_syms, idx, sym) {
      struct symbol *s;
      const char *elf_name;
      char *demangled;
      int is_label = elf_sym__is_label(&sym);

      //printf("%s -> %p-%p (%p)\n", elf_sym__name(&sym, symstrs), sym.st_value, sym.st_value + sym.st_size, biggest_addr);
      if (!is_label && !elf_sym__is_function(&sym) && !elf_sym__is_object(&sym))
         continue;

      sec = elf_getscn(elf, sym.st_shndx);
      if (!sec) {
         //goto out_elf_end;
         continue;
      }

      gelf_getshdr(sec, &shdr);

      if (is_label && !elf_sec__is_text(&shdr, secstrs))
         continue;

      elf_sec__name(&shdr, secstrs);

      if (adjust_symbols /*&& !elf_sym__is_object(&sym)*/) {
         sym.st_value -= shdr.sh_addr - shdr.sh_offset;
      }
      /*printf("-%s -> %p-%p (%p) [%p-%p]\n", elf_sym__name(&sym, symstrs), sym.st_value, sym.st_value + sym.st_size, biggest_addr,
            shdr.sh_addr, shdr.sh_offset);*/

      /*
       * We need to figure out if the object was created from C++ sources
       * DWARF DW_compile_unit has this, but we don't always have access
       * to it...
       */
      elf_name = elf_sym__name(&sym, symstrs);
      demangled = bfd_demangle(NULL, elf_name, DMGL_PARAMS | DMGL_ANSI);
      if (demangled != NULL)
         elf_name = demangled;

      s = symb_new();
      s->ip = (void*)sym.st_value;
      s->size = sym.st_size;
      s->function_name = strdup(elf_name);
      if(sym.st_value + sym.st_size > biggest_addr)
         biggest_addr = sym.st_value + sym.st_size;
      symb_insert(lib, s);
      free(demangled);
      nr++;
   }
   lib->real_size = biggest_addr;
   symb_synthesize_plt_symbols(lib, elf);
   out_elf_end: elf_end(elf);
   close(fd);

   out_close:return;
}

char *short_name(char *libname) {
   size_t len = strlen(libname);
   if(len < 30)
      return libname;
   char *end = libname + len;
   while(end > libname && *end != '/')
      end--;
   if(*end == '/')
      end++;
   return end;
}
