#ifndef MMAP_H
#define MMAP_H 1

struct mmapped_dyn_lib {
   int uid;
   struct dyn_lib *lib;
   uint64_t begin, end, off;
};

void add_mmap_event(struct mmap_event *m);

struct symbol* sample_to_function(struct s* s); /* IP */
struct symbol* sample_to_static_object(struct s*s); /* VA */
struct symbol* sample_to_callchain(struct s*s, struct symbol *ss, int index); /* IP */
struct dyn_lib* sample_to_mmap(struct s* s); /* VA */
struct mmapped_dyn_lib* sample_to_mmap2(struct s* s); /* VA */

void show_mmap_stats();

#endif
