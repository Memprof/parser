#ifndef COMPATIBILITY_H
#define COMPATIBILITY_H 1

typedef size_t (*sample_reader_f)(void * ptr, size_t size, size_t nitems, FILE * stream);
extern sample_reader_f read_sample;
typedef void (*read_extra_header_f)(FILE * stream, struct i *i);
extern read_extra_header_f read_header;
typedef void (*read_perf_f)(char* stream);
extern read_perf_f read_perf_events;
void set_version(struct h *h);

#endif
