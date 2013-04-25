#include "parse.h"
#include "builtin-get-pid.h"

static int tid_to_find = -1;
void get_pid_set(int tid) {
   assert(tid_to_find == -1);
   tid_to_find = tid;
}

void get_pid_init() {
   if(tid_to_find == -1) {
      fprintf(stderr, "Please specify a tid\n");
      exit(-1);
   }
}
int changes = 0;
int last_cpu = 0;
void get_pid_parse(struct s* s) {
   if(get_tid(s) == tid_to_find) {
      //printf("%d (%s)\n", get_pid(s), get_app(s));
      //exit(0);
      if(cpu_to_node(s->cpu) != last_cpu) {
         last_cpu = cpu_to_node(s->cpu);
         changes++;
      }
   }
}
void get_pid_show() {
   printf("Changes %d\n", changes);
   printf("Tid %d not found\n", tid_to_find);
}

