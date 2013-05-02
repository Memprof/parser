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
#ifndef MMAP_H
#define MMAP_H 1

struct mmapped_dyn_lib {
   int uid;
   struct dyn_lib *lib;
   uint64_t begin, end, off;
};

void add_mmap_event(struct mmap_event *m);

struct symbol* sample_to_function(struct s* s); /* IP */
struct symbol* sample_to_static_object(struct s*s); /* VA */
struct symbol* sample_to_callchain(struct s*s, struct symbol *ss, int index); /* IP */
struct dyn_lib* sample_to_mmap(struct s* s); /* VA */
struct mmapped_dyn_lib* sample_to_mmap2(struct s* s); /* VA */

void show_mmap_stats();

#endif
