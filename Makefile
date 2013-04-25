CFLAGS = -I ../merge_data/ -I./utils/ -g -O0 -Wall -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I /usr/lib/x86_64-linux-gnu/glib-2.0/include/ -I../standalone_kernel_module/ 
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
			 builtin-memory-repartition.o \
			 builtin-stack-repartition.o \
			 builtin-dump.o \
			 builtin-cache.o \
			 builtin-memory-overlap.o \
			 builtin-stats.o \
			 builtin-sched.o \
			 builtin-top.o \
			 builtin-get-pid.o \
			 builtin-memory-localize.o \
			 builtin-zones.o \
			 builtin-migrate.o \
			 builtin-static-obj.o \
			 builtin-get-npages.o \
			 builtin-sql.o \
			 builtin-object.o \
			 builtin-pages.o \
			 parse.o 

.PHONY: all clean 


has_bfd := $(shell sh -c "(echo '\#include <bfd.h>'; echo 'int main(void) { bfd_demangle(0, 0, 0); return 0; }') | $(CC) -x c - -lbfd && rm a.out && echo y")
ifeq ($(has_bfd),y)
else
   $(error $(has_bfd) No bfd.h/libbfd found, install binutils-dev[el]/zlib-static to gain symbol demangling)
endif

has_glib := $(shell sh -c "(echo 'int main(void) { return 0; }') | $(CC) -x c - -lglib-2.0 && rm a.out && echo y")
ifeq ($(has_glib),y)
else
   $(error $(has_glib) No glib2.0 found. Install libglib2.0-dev.)
endif

has_numa := $(shell sh -c "(echo 'int main(void) { return 0; }') | $(CC) -x c - -lnuma && rm a.out && echo y")
ifeq ($(has_numa),y)
else
   $(error $(has_numa) No libnuma found. Install libnuma-dev.)
endif


all : makefile.dep parse

makefile.dep : *.[Cch] utils/*.[Cch]
	for i in *.[Cc] utils/*.[Cch]; do gcc -MM "$${i}" ${CFLAGS}; done > $@

-include makefile.dep

parse: ${OBJECTS}
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f parse *.o utils/*.o
