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
#include "parse.h"
#include "builtin-stack-check.h"

/*
 * Simple check: are we correctly attributing samples that touch the stack (@virt between usersp and stack)
 * to the stack mapping?
 */

static int nb_stack_hit, nb_stack_miss;            /* Calculated with usersp */
static int nb_stack_hit_addr, nb_stack_miss_addr;  /* Calculated with mmap resolution */
void stack_check_init() {
   nb_stack_miss = nb_stack_hit = 0;
   nb_stack_hit_addr = nb_stack_miss_addr = 0;
}

void stack_check_parse(struct s* s) {
   if(s->stack && s->ibs_dc_linear) {
      if(s->ibs_dc_linear >= s->usersp && s->ibs_dc_linear <= (uint64_t)s->stack) {
         nb_stack_hit++;
      } else {
         nb_stack_miss++;
      }

      if(!strcmp(get_mmap(s)->name, "[stack]")) {
         nb_stack_hit_addr++;
      } else {
         nb_stack_miss_addr++;
      }
   }
}

void stack_check_show() {
   printf("[ Calculated ] Nb stack hit: %d (%.2f%%); nb outside stack: %d (%.2f%%)\n", nb_stack_hit, 100.*((float)nb_stack_hit/(float)(nb_stack_hit+nb_stack_miss)), nb_stack_miss, 100.*((float)nb_stack_miss/(float)(nb_stack_hit+nb_stack_miss)));
   printf("[From Symbols] Nb stack hit: %d (%.2f%%); nb outside stack: %d (%.2f%%)\n", nb_stack_hit_addr, 100.*((float)nb_stack_hit_addr/(float)(nb_stack_hit_addr+nb_stack_miss_addr)), nb_stack_miss_addr, 100.*((float)nb_stack_miss_addr/(float)(nb_stack_hit_addr+nb_stack_miss_addr)));
}

