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
#ifndef BUILTIN_STACK_REP
#define BUILTIN_STACK_REP
#include "memprof-structs.h"

void stack_check_init();
void stack_check_parse(struct s* s);
void stack_check_show();

#endif