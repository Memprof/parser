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
