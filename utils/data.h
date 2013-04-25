#ifndef DATA_H
#define DATA_H 1

enum data_ev_type { MALLOC, FREE, REALLOC };

void read_data_events(char *mmaped_file);
void process_data_samples(uint64_t time);
char* sample_to_variable(struct s* s);
struct symbol* sample_to_variable2(struct s* s);
void show_data_stats();

#endif
