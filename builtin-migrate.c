#include "parse.h"
#include "numa.h"
#include <linux/mempolicy.h>
#include "builtin-migrate.h"

static int pid;
struct page {
   int *accesses;
   void *addr;
};
struct pages_array {
   void **pages;
   int *nodes;
   int *status;
   int count, max_count;
} pages_array;

static rbtree migrate_tree;
void migrate_init() {
   if(geteuid() != 0)
      die("Please use --migrate as root\n");

   migrate_tree = rbtree_create();
}

#define CLUSTER 512
void migrate_parse(struct s* s) {
   if(!s->ibs_dc_phys)
      return;

   void *addr = (void*)((s->ibs_dc_linear / PAGE_SIZE / CLUSTER ) * (PAGE_SIZE * CLUSTER));
   struct page* v = rbtree_lookup(migrate_tree, addr, pointer_cmp);
   if(!v) {
      v = calloc(1, sizeof(*v));
      v->addr = addr;
      v->accesses = calloc(1, sizeof(*v->accesses)*max_node);
      rbtree_insert(migrate_tree, addr, v, pointer_cmp);
   }
   v->accesses[cpu_to_node(s->cpu)]++;
   pid = get_pid(s);
}

static int page_to_most_accessing_die(struct page *p) {
   int max = p->accesses[0], die = 0, i;
   for(i = 1; i < max_node; i++) {
      if(p->accesses[i] > max) {
         max = p->accesses[i];
         die = i;
      }
   }
   return die;
}

static void insert_page(struct page* value, int node) {
   if(!pages_array.max_count) {
      pages_array.count = 0;
      pages_array.max_count = 512;
      pages_array.pages = malloc(sizeof(*pages_array.pages)*pages_array.max_count);
      pages_array.nodes = malloc(sizeof(*pages_array.nodes)*pages_array.max_count);
   }
   if(pages_array.count == pages_array.max_count) {
      pages_array.max_count *= 2;
      pages_array.pages = realloc(pages_array.pages, sizeof(*pages_array.pages)*pages_array.max_count);
      pages_array.nodes = realloc(pages_array.nodes, sizeof(*pages_array.nodes)*pages_array.max_count);
   }
   pages_array.pages[pages_array.count] = value->addr;
   pages_array.nodes[pages_array.count] = node;
   pages_array.count++;
}

static int fill_page_to_migrate(void *key, void *value) {
   int i, node = page_to_most_accessing_die(value);
   struct page *v = value;
   v->addr += PAGE_SIZE*CLUSTER/2;
   for(i = 0; i < CLUSTER; i++) {
      insert_page(v, node);
      v->addr -= PAGE_SIZE;
   }
   return 0;
}

static char *move_pages_to_error_code(int code) {
   switch(code) {
      case E2BIG: 
         return "E2BIG";
      case EACCES: 
         return "EACCESS";
      case EFAULT:
         return "EFAULT";
      case ENODEV:
         return "ENODEV";
      case ENOENT:
         return "ENOENT";
      case EPERM:
         return "EPERM";
      case ESRCH:
         return "ESRCH";
      default:
         return "-";
   }
}

static char *status_to_error(int st) {
   switch(st) {
      case -EACCES:
         return "EACCES pages used by multiples processes";
      case -EBUSY:
         return "EBUSY page (IO or reference inside kernel)";
      case -EFAULT:
         return "EFAULT";
      case -EIO:
         return "EIO";
      case -EINVAL:
         return "EINVAL?";
      case -ENOENT:
         return "ENOENT";
      case -ENOMEM:
         return "ENOMEM NO MEMORY";
      default:
         return "-";
   }
}

void migrate_show() {
   rbtree_print(migrate_tree, fill_page_to_migrate);

   pages_array.status = malloc(sizeof(*pages_array.status)*pages_array.count);
   int ret = numa_move_pages(pid, pages_array.count, pages_array.pages, pages_array.nodes, pages_array.status, MPOL_MF_MOVE_ALL);
   printf("#%d pages migrated (result = %d %s)\n", pages_array.count, ret, move_pages_to_error_code(ret));

   int i; 
   for(i = 0; i < pages_array.count; i++) {
      printf("#%lx %d %d %s\n", (long unsigned)pages_array.pages[i], pages_array.nodes[i], pages_array.status[i], status_to_error(pages_array.status[i]));
   }
}  
