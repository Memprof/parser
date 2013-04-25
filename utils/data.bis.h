#ifndef DATA_H
#define DATA_H 1

enum data_ev_type { MALLOC, FREE, REALLOC };
struct data_ev {
   uint64_t rdt;
   size_t pos;
   enum data_ev_type type;
   int cpu, tid;
   union {
      struct {
         uint64_t begin;
         uint64_t end;
         char *info;
      } malloc;
      struct {
         uint64_t begin;
      } free;
   };
};

void read_data_events(char *mmaped_file);
void process_data_samples(uint64_t time);
char* sample_to_variable(struct s* s);
struct data_ev* sample_to_variable2(struct s* s);
void show_data_stats();

#endif
