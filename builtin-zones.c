#include "parse.h"
#include "builtin-zones.h"

static int mode = 1;
static int cluster_size = 10000;

static struct addresses {
   uint64_t addr;
} **addresses;
static int nb_addresses_max, nb_addresses;

struct mem_zone {
   uint64_t nb_elem;
   size_t pos;
   uint64_t low_boundary;
   uint64_t high_boundary;
};
static pqueue_t* zones;
static int cmp_pri(uint64_t next, uint64_t curr) { return (next < curr); }
static uint64_t get_pri(void *a) {  return ((struct mem_zone *) a)->nb_elem; }
static void set_pri(void *a, uint64_t pri) {((struct mem_zone *) a)->nb_elem = pri; }
static size_t get_pos(void *a) { return ((struct mem_zone *) a)->pos; }
static void set_pos(void *a, size_t pos) { ((struct mem_zone *) a)->pos = pos; }

void zone_set_cluster_size(int size) {
   cluster_size = size;
}

void zone_modifier(int m) {
   if(m > 2 || m < 1)
      die("Mode %d is not supported by zones\n", m);
   mode = m;
}

void zone_init() {
   nb_addresses_max = 500;
   nb_addresses = 0;
   addresses = malloc(nb_addresses_max*sizeof(*addresses));
   zones = pqueue_init(10, cmp_pri, get_pri, set_pri, get_pos, set_pos);
}

void zone_parse(struct s* s) {
   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   ibs_op_data3_t *data3 = (void*)&udata3;

   if(s->ibs_dc_phys > get_memory_size())
        return;

   if(!data3->ibsdcphyaddrvalid)
         return;

   if(nb_addresses >= nb_addresses_max) {
      nb_addresses_max*=2;
      addresses = realloc(addresses, nb_addresses_max*sizeof(*addresses));
   }
   addresses[nb_addresses] = malloc(sizeof(*addresses[nb_addresses]));
   if(mode == 1) {
      addresses[nb_addresses]->addr = s->ibs_dc_phys;
   } else {
      addresses[nb_addresses]->addr = s->ibs_dc_linear;
   }
   nb_addresses++;
}

static int cmp_addresses(const void *_a, const void *_b) {
   struct addresses *a = *(struct addresses **)_a;
   struct addresses *b = *(struct addresses **)_b;
   if(a->addr > b->addr)
      return 1;
   else if(a->addr < b->addr)
      return -1;
   else
      return 0;
   //return a->addr - b->addr; //Nope because overflow
}

void insert_zone(int nb_elem, uint64_t low_boundary,  uint64_t high_boundary) {
   struct mem_zone *m = malloc(sizeof(*m));
   m->nb_elem = nb_elem;
   m->low_boundary = low_boundary;
   m->high_boundary = high_boundary;
   pqueue_insert(zones, m);
} 

void zone_show() {
   int i, nb_elem = 0;
   uint64_t low_boundary = 0, high_boundary = 0;
   qsort (addresses, nb_addresses, sizeof (*addresses), cmp_addresses);


   for(i = 0; i < nb_addresses; i++) {
      if(addresses[i]->addr - high_boundary < cluster_size) {
         nb_elem++;
      } else {
         if(nb_elem > 0) 
            insert_zone(nb_elem, low_boundary, high_boundary);
         low_boundary = addresses[i]->addr;
         nb_elem = 1;
      }
      high_boundary = addresses[i]->addr;
   }
   if(nb_elem > 0) 
      insert_zone(nb_elem, low_boundary, high_boundary);

   if(mode == 1)
      printf("#Clustered by physical addresses (cluster size %d)\n", cluster_size);
   else
      printf("#Clustered by virtual addresses (cluster size %d)\n", cluster_size);

   uint64_t sum = 0;
   struct mem_zone *m;
   while((m = pqueue_pop(zones))) {
      sum += m->nb_elem;
      printf("[%lu-%lu] %.2f%%\n", m->low_boundary, m->high_boundary, 100.*((float)m->nb_elem/(float)nb_addresses));
   }
   printf("#Sum = %.2f%%\n", 100.*((float)sum/(float)nb_addresses));
}

