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
