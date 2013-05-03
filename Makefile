CFLAGS = -I ../merge_data/ -I./utils/ -g -O0 -Wall -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I /usr/lib/x86_64-linux-gnu/glib-2.0/include/ -I../module -I../library
LDFLAGS = -lglib-2.0 -lelf -lbfd -lnuma -lm
OBJECTS = ./utils/symbols.o \
			 ./utils/process.o \
			 ./utils/machine.o \
			 ./utils/rbtree.o \
			 ./utils/pqueue.o \
			 ./utils/compatibility.o \
			 ./utils/compatibility-machine.o \
			 ./utils/perf.o \
			 ./utils/data.o \
			 ./utils/mmap.o \
			 builtin-top-functions.o \
			 builtin-top-obj.o \
			 builtin-top-mmaps.o \
			 builtin-stack-check.o \
			 builtin-dump.o \
			 builtin-memory-hierarchy.o \
			 builtin-memory-overlap.o \
			 builtin-stats.o \
			 builtin-sched.o \
			 builtin-get-sched-stats.o \
			 builtin-memory-repartition.o \
			 builtin-zones.o \
			 builtin-migrate.o \
			 builtin-get-npages.o \
			 builtin-sql.o \
			 builtin-object.o \
			 builtin-pages.o \
			 parse.o 

.PHONY: all clean 

VERBOSE ?= n
is_error := n
has_bfd := $(shell sh -c "(echo '\#include <bfd.h>'; echo 'int main(void) { bfd_demangle(0, 0, 0); return 0; }') | $(CC) -x c - -lbfd 2>&1 && rm a.out && echo y")
ifneq ($(has_bfd),y)
   is_error := y
   ifeq ($(VERBOSE), n)
      has_bfd := 
   endif
   $(warning $(has_bfd) No bfd.h/libbfd found, install binutils-dev.)
endif

has_elf := $(shell sh -c "(echo '\#include <libelf.h>'; echo 'int main(void) { return 0; }') | $(CC) -x c - 2>&1 && rm a.out && echo y")
ifneq ($(has_elf),y)
   is_error := y
   ifeq ($(VERBOSE), n)
      has_elf := 
   endif
   $(warning $(has_elf) No libelf.h found, install libelf-dev.)
endif

has_glib := $(shell sh -c "(echo 'int main(void) { return 0; }') | $(CC) -x c - -lglib-2.0 2>&1 && rm a.out && echo y")
ifneq ($(has_glib),y)
   is_error := y
   ifeq ($(VERBOSE), n)
      has_glib := 
   endif
   $(warning $(has_glib) No glib2.0 found. Install libglib2.0-dev.)
endif

has_numa := $(shell sh -c "(echo 'int main(void) { return 0; }') | $(CC) -x c - -lnuma 2>&1 && rm a.out && echo y")
ifneq ($(has_numa),y)
   is_error := y
   ifeq ($(VERBOSE), n)
      has_numa := 
   endif
   $(warning $(has_numa) No libnuma found. Install libnuma-dev.)
endif

ifeq ($(is_error),y)
   $(error Exiting due to missing dependencies; use "VERBOSE=yes make" to show detailled error messages.)
endif


all : makefile.dep parse

makefile.dep : *.[Cch] utils/*.[Cch]
	for i in *.[Cc] utils/*.[Cch]; do echo -n `dirname "$${i}"`/; gcc -MM "$${i}" ${CFLAGS}; done > $@

-include makefile.dep

parse: ${OBJECTS}
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f parse *.o utils/*.o makefile.dep
