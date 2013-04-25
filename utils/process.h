#ifndef PROCESS_LIB_H
#define PROCESS_LIB_H 1


struct process {
   char *name;
   rbtree mmapped_dyn_lib;
   rbtree allocated_obj;
};

void process_init();
void process_reset(int pid);
struct process* find_process(int pid);
struct process* find_process_safe(int pid); //Creates process if it does not exist

#endif
