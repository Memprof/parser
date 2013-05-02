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
#include "perf.h"

int processed_perf_samples = 0;
int total_perf_samples = 0;

pqueue_t* perf_events;
enum perf_ev_type { MMAP, COMM, TASK };
struct perf_ev {
   uint64_t rdt;
   size_t pos;
   enum perf_ev_type type;
   union {
      struct mmap_event mmap_event;
      struct comm_event comm_event;
      struct task_event task_event;
   };
};
static int cmp_pri(uint64_t next, uint64_t curr) { return (next > curr); }
static uint64_t get_pri(void *a) {  return ((struct perf_ev *) a)->rdt; }
static void set_pri(void *a, uint64_t pri) {((struct perf_ev *) a)->rdt = pri; }
static size_t get_pos(void *a) { return ((struct perf_ev *) a)->pos; }
static void set_pos(void *a, size_t pos) { ((struct perf_ev *) a)->pos = pos; }

void read_perf(char *mmaped_file) {
   int i;

   /** Header **/
   struct h h;
   struct m m;
   FILE *perf = open_file(mmaped_file);
   if(!perf) {
      printf("#Warning: perf file %s not found\n", mmaped_file);
      return;
   }

   int n = fread(&h, sizeof(h), 1, perf);
   if(!n) {
      fprintf(stderr, "No version found in file %s\n", mmaped_file);
      exit(-1);
   } else if(h.version != M_VERSION) {
      fprintf(stderr, "VERSION MISMATCH in %s expected %d seen %d\n", mmaped_file, M_VERSION, h.version);
      exit(-1);
   }

   /** Nb Events **/
   assert(fread(&m, sizeof(m), 1, perf) == 1);
   if(verbose)
      printf("#nb_mmap_events %d nb_comm_events %d nb_task_events %d\n",
            m.nb_mmap_events,
            m.nb_comm_events,
            m.nb_task_events);
   total_perf_samples +=  m.nb_mmap_events +  m.nb_comm_events + m.nb_task_events;

   if(!perf_events)
      perf_events = pqueue_init(10, cmp_pri, get_pri, set_pri, get_pos, set_pos);
   struct perf_ev *event;
   for(i = 0; i < m.nb_mmap_events; i++) {
      event = malloc(sizeof(*event));
      event->type = MMAP;
      assert(fread(&event->mmap_event, sizeof(event->mmap_event), 1, perf) == 1);
      event->rdt = event->mmap_event.rdt;
      pqueue_insert(perf_events, event);
      //fprintf(stdout, "#%d mmap %s start=%lx end=%lx\n", event->mmap_event.pid, event->mmap_event.file_name, event->mmap_event.start, event->mmap_event.start+event->mmap_event.len);
   }

   for(i = 0; i < m.nb_comm_events; i++) {
      event = malloc(sizeof(*event));
      event->type = COMM;
      assert(fread(&event->comm_event, sizeof(event->comm_event), 1, perf) == 1);
      event->rdt = event->comm_event.rdt;
      pqueue_insert(perf_events, event);
   }

   for(i = 0; i < m.nb_task_events; i++) {
      event = malloc(sizeof(*event));
      event->type = TASK;
      assert(fread(&event->task_event, sizeof(event->task_event), 1, perf) == 1);
      event->rdt = event->task_event.rdt;
      pqueue_insert(perf_events, event);
   }

   if(verbose)
      printf("#All perf events added successfully ; now processing samples\n");
}

void process_perf_samples(uint64_t time) {
   struct perf_ev *event = pqueue_peek(perf_events);
   while(event && event->rdt <= time) {
      if(event->type == MMAP) {
         //fprintf(stdout, "mmap %s start=%lx end=%lx size=%lx offset=%lx\n", event->mmap_event.file_name, event->mmap_event.start, event->mmap_event.start+event->mmap_event.len, event->mmap_event.len, event->mmap_event.pgoff);
         add_lib(event->mmap_event.file_name);
         add_mmap_event(&event->mmap_event);
      } else if(event->type == COMM) {
         //fprintf(stderr, "comm pid %d tid %d comm %s\n", event->comm_event.pid, event->comm_event.tid, event->comm_event.comm);
      } else if(event->type == TASK) {
         /*fprintf(stderr, "task pid %d tid %d ppid %d ptid %d\n", 
               event->task_event.pid, event->task_event.tid,
               event->task_event.ppid, event->task_event.ptid);*/
      }
      processed_perf_samples++;
      pqueue_pop(perf_events);
      //free(event);
      event = pqueue_peek(perf_events);
   }
}
