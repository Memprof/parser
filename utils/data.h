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
#ifndef DATA_H
#define DATA_H 1

enum data_ev_type { MALLOC, FREE, REALLOC };

void read_data_events(char *mmaped_file);
void process_data_samples(uint64_t time);
char* sample_to_variable(struct s* s);
struct symbol* sample_to_variable2(struct s* s);
void show_data_stats();

#endif
