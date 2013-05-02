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
#include "data.h"

static int processed_data_samples = 0;
static int total_data_samples = 0;

static pqueue_t* data_events;
static int cmp_pri(uint64_t next, uint64_t curr) { return (next > curr); }
static uint64_t get_pri(void *a) {  return ((struct data_ev *) a)->rdt; }
static void set_pri(void *a, uint64_t pri) {((struct data_ev *) a)->rdt = pri; }
static size_t get_pos(void *a) { return ((struct data_ev *) a)->pos; }
static void set_pos(void *a, size_t pos) { ((struct data_ev *) a)->pos = pos; }

static rbtree active_data;

/**
 * Expects a file like:
 **/
void read_data_events(char *mmaped_file) {
   /** Header **/
   FILE *data = open_file(mmaped_file);
   if(!data) {
      printf("#Warning: data file %s not found\n", mmaped_file);
      return;
   }

   if(!data_events)
      data_events = pqueue_init(10, cmp_pri, get_pri, set_pri, get_pos, set_pos);

   rbtree metadata = rbtree_create();

   char line[512];
   struct data_ev *event;
   int nb_lines = 0;
   uint64_t type;


   while(fgets(line, sizeof(line), data)) {
      nb_lines++;
      event = malloc(sizeof(*event));

      if(sscanf(line, "%lu %lu %lu %lu %d %u", &event->rdt, &event->malloc.begin, &event->malloc.end, &type, &event->cpu, &event->tid) != 6) {
         goto test_info;
      }
      if(type == 0) {               // free
         event->type = FREE;
      } else if(type == 2) {        // munmap
         event->type = FREE;        //munmap is not handled correctly yet => fake free
      } else {                      // malloc / mmap
         event->type = MALLOC;
         event->malloc.end = event->malloc.begin + event->malloc.end;
         if(type == 1) {
            char * val = rbtree_lookup(metadata, (void*)event->rdt, pointer_cmp);
            if(val)
               event->malloc.info = val;
            else
               asprintf(&event->malloc.info, "datasize%lu-%d", event->malloc.end - event->malloc.begin, nb_lines);
         } else {
            /*#define MAP_SHARED    0x01 
            #define MAP_PRIVATE     0x02*/
            if(event->malloc.end - event->malloc.begin == 8392704) { /* All stacks seem to be of that size */
               asprintf(&event->malloc.info, "thread-stack-%d", nb_lines);
            } else if(type & 0x01) {
               asprintf(&event->malloc.info, "mmap-shared%lu-%d", event->malloc.end - event->malloc.begin, nb_lines);
            } else if(type & 0x02) {
               asprintf(&event->malloc.info, "mmap-priv%lu-%d", event->malloc.end - event->malloc.begin, nb_lines);
            } else {
               asprintf(&event->malloc.info, "mmap-??%lu-%d", event->malloc.end - event->malloc.begin, nb_lines);
            }
         }
      }


      pqueue_insert(data_events, event);
      total_data_samples++;
      continue;

test_info:;
      uint64_t time, loc;
      int read;
      if(sscanf(line, "#%lu 0x%lx %n\n", &time, &loc, &read) != 2) {
         //printf("fail %s %d\n", line, read);
         goto fail;
      }
      char *met_value = strdup(line + read);
      int met_len = strlen(met_value)-1;
      if(met_len < 5) // malloc probably not correctly resolved
         asprintf(&met_value, "%lu", time);
      else
         met_value[met_len] = '\0';
      rbtree_insert(metadata, (void*)time, met_value, pointer_cmp);

fail:
      //printf("#Unrecognized line: %s", line);
      free(event);
      continue;
   }

   if(!active_data)
      active_data = rbtree_create();

   if(verbose)
      printf("#All data events added successfully ; now processing samples\n");
}

static struct data_ev* _sample_to_variable(rbtree_node n, void *addr) {
   while (n != NULL) {
      if(n->left)
         assert(n->key > n->left->key);
      if(n->right)
         assert(n->key < n->right->key);

      if(addr < n->key) {
         n = n->left;
      } else {
         if(n->right && addr >= n->right->key) {
            n = n->right;
         } else {
            /* Look in the right subtree for a better candidate */
            struct data_ev* best_son = _sample_to_variable(n->right, addr);
            return best_son?best_son:n->value;
         }
      }
   }
   return NULL;
}

static __unused int print_mmap(void *key, void *value) {
   printf("%p-%p: %s\n", key,
         (void*)((struct data_ev *)value)->malloc.end,
         ((struct data_ev *)value)->malloc.info);
   return 0;
}

char* sample_to_variable(struct s* s) {
   if(!s->ibs_dc_linear || !active_data)
      return NULL;
   /*rbtree_print(active_data, print_mmap);
   exit(0);*/
   rbtree_node n = active_data->root;
   struct data_ev * ret =  _sample_to_variable(n, (void*)s->ibs_dc_linear);
   if(!ret || ret->malloc.end < s->ibs_dc_linear)
      return NULL;
   return ret->malloc.info;
}

struct data_ev* sample_to_variable2(struct s* s) {
   if(!s->ibs_dc_linear || !active_data)
      return NULL;
   rbtree_node n = active_data->root;
   struct data_ev * ret =  _sample_to_variable(n, (void*)s->ibs_dc_linear);
   if(!ret || ret->malloc.end < s->ibs_dc_linear)
      return NULL;
   return ret;
}


static int data_success, data_fail;
void show_data_stats() {
   if(data_success+data_fail)
      printf("#Processes data events %d successes (%.2f%%)\n", data_success, 100.*((float)data_success)/((float)data_success+data_fail));
}

void process_data_samples(uint64_t time) {
   if(!data_events)
      return;

   struct data_ev *event = pqueue_peek(data_events);
   while(event && event->rdt <= time) {
      /*printf("%s:%lu:%p:%s\n", event->type==MALLOC?"malloc":"free", event->rdt, (void*)event->free.begin,
            event->type==MALLOC?event->malloc.info:"");*/
      if(event->type==MALLOC) {
         void * data = rbtree_lookup(active_data, (void*)event->free.begin, pointer_cmp_reverse);
         if(data) {
            //printf("#Variable inserted twice ?!\n");
            ((struct data_ev *)data)->malloc.end = event->malloc.end;
            data_fail++;
         } else {
            rbtree_insert(active_data, (void*)event->malloc.begin, event, pointer_cmp_reverse);
            data_success++;
         }
      } else if(event->type==FREE) {
         void * data = rbtree_lookup(active_data, (void*)event->free.begin, pointer_cmp_reverse);
         if(!data) {
            //printf("#Free of unknown pointer!\n");
            data_fail++;
         } else {
            rbtree_delete(active_data, (void*)event->free.begin, pointer_cmp_reverse);
            data_success++;
         }
      }

      processed_data_samples++;
      pqueue_pop(data_events);
      event = pqueue_peek(data_events);
   }
}
