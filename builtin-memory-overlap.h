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
#ifndef BUILTIN_MEMORY_OVERLAP
#define BUILTIN_MEMORY_OVERLAP 1

#define NB_STORED_FIELDS ((nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids)*3 + 2)
#define LOAD_INDEX(app) ((nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids) + app)
#define STORE_INDEX(app) ((nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids)*2 + app)
#define IN_KERNEL ((nb_overlapping_apps+nb_overlapping_tids+nb_overlapping_pids)*3)
#define USERLAND (IN_KERNEL + 1)

void overlap_add_application(char *app);
void overlap_add_tid(int tid);
void overlap_add_pid(int pid);
void overlap_add_tid_unshared(int tid);
void overlap_init();
void overlap_parse(struct s* s);
void overlap_show();

#endif

