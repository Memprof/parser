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
#include "parse.h"
#include "builtin-get-sched-stats.h"

static int tid_to_find = -1;
void get_sched_stats_set(int tid) {
   assert(tid_to_find == -1);
   tid_to_find = tid;
}

void get_sched_stats_init() {
   if(tid_to_find == -1) {
      fprintf(stderr, "Please specify a tid\n");
      exit(-1);
   }
}

static int changes = 0;
static int last_node = -1;
static int pid_found = -1;
void get_sched_stats_parse(struct s* s) {
   if(get_tid(s) == tid_to_find) {
      pid_found = get_pid(s);
      if(cpu_to_node(s->cpu) != last_node) {
         last_node = cpu_to_node(s->cpu);
         changes++;
      }
   }
}

void get_sched_stats_show() {
   if(changes == 0) {
      printf("Tid %d not found\n", tid_to_find);
   } else {
      printf("Tid %d (pid %d) has move %d times between nodes\n", tid_to_find, pid_found, changes);
   }
}

