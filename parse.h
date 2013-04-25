#ifndef PARSE_H
#define PARSE_H 1

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>   
#include <sys/wait.h>
#include <stdlib.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sched.h>

#define MAX_NODE 5 /* 4 real + 1 fake */
#define MAX_REAL_NODE 4 
#define MAX_CPU 32
#define PAGE_SIZE (4*1024)
#define PAGE_MASK (~(PAGE_SIZE-1))
#define ONE_GB (1LL*1024*1024*1024LL)

#include "machine.h"
#include "memprof-structs.h"
#include "rbtree.h"
#include "list.h"
#include "pqueue.h"
#include "symbols.h"
#include "process.h"
#include "perf.h"
#include "data.h"
#include "mmap.h"


typedef void (*buffer_parser_t) (struct s*);
typedef void (*results_shower_t) (void);
typedef void (*init_function_t) (void);
typedef void (*modifier_function_t) (int);

typedef struct {
   uint64_t   ibsldop:1;
   uint64_t   ibsstop:1;
   uint64_t   ibsdcl1tlbmiss:1;
   uint64_t   ibsdcl2tlbmiss:1;
   uint64_t   ibsdcl1tlbhit2m:1;
   uint64_t   ibsdcl1tlbhit1g:1;
   uint64_t   ibsdcl2tlbhit2m:1;
   uint64_t   ibsdcmiss:1;
   uint64_t   ibsdcmissacc:1;
   uint64_t   ibsdcldbnkcon:1;
   uint64_t   ibsdcstbnkcon:1;
   uint64_t   ibsdcsttoldfwd:1;
   uint64_t   ibsdcsttoldcan:1;
   uint64_t   ibsdcucmemacc:1;
   uint64_t   ibsdcwcmemacc:1;
   uint64_t   ibsdclockedop:1;
   uint64_t   ibsdcmabhit:1;
   uint64_t   ibsdclinaddrvalid:1;
   uint64_t   ibsdcphyaddrvalid:1;
   uint64_t   reserved1:13;
   uint64_t   ibsdcmisslat:16;
   uint64_t   reserved2:16;
} ibs_op_data3_t;

typedef struct {
   uint64_t ibscomptoretctr:16;
   uint64_t ibstagtoretctr:16;
   uint64_t ibsopbrnresync:1;
   uint64_t ibsopmispreturn:1;
   uint64_t ibsopreturn:1;
   uint64_t ibsopbrntaken:1;
   uint64_t ibsopbrnmisp:1;
   uint64_t ibsopbrnret:1;
   uint64_t reserved:26;
} ibs_op_data_t;

extern struct i i;
char *get_app(struct s *s);
int is_user(struct s *s);
int is_kernel(struct s *s);
int is_softirq(struct s *s);
int get_pid(struct s *s);
int get_tid(struct s *s);
int get_addr_node(struct s *s);
int get_latency(struct s *s);
extern char *open_file_error;
FILE* open_file(char *f);
struct symbol *get_symbol(struct s *s);
char *get_function_name(struct s *s);
struct dyn_lib* get_mmap(struct s *s);
int is_store(struct s *s);
int is_load(struct s *s);
int hit_dram(struct s *s);
int hit_locall1l2(struct s *s);
int hit_dist_cache(struct s *s);
int hit_stack(struct s *s);
int is_distant(struct s *s);
extern int verbose;
extern char* path_to_binaries;
extern char* opt_machine;

#define __unused __attribute__((unused))

#define max(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })
#define min(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a < _b ? _a : _b; })

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define BLUE    "\033[34m"      /* Blue */

#define die(msg, args...) \
   do {                         \
      fprintf(stderr,"(%s,%d) " msg "\n", __FUNCTION__ , __LINE__, ##args); \
      exit(-1);                 \
   } while(0)

#endif
