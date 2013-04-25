#include "parse.h"
#include "builtin-stack-repartition.h"

static int nb_stack_hit, nb_stack_miss;            /* Calculated with usersp */
static int nb_stack_hit_addr, nb_stack_miss_addr;  /* Calculated with mmap resolution */
void stack_repartition_init() {
   nb_stack_miss = nb_stack_hit = 0;
   nb_stack_hit_addr = nb_stack_miss_addr = 0;
}

void stack_repartition_parse(struct s* s) {
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

void stack_repartition_show() {
   printf("[ Calculated ] Nb stack hit: %d (%.2f%%); nb outside stack: %d (%.2f%%)\n", nb_stack_hit, 100.*((float)nb_stack_hit/(float)(nb_stack_hit+nb_stack_miss)), nb_stack_miss, 100.*((float)nb_stack_miss/(float)(nb_stack_hit+nb_stack_miss)));
   printf("[From Symbols] Nb stack hit: %d (%.2f%%); nb outside stack: %d (%.2f%%)\n", nb_stack_hit_addr, 100.*((float)nb_stack_hit_addr/(float)(nb_stack_hit_addr+nb_stack_miss_addr)), nb_stack_miss_addr, 100.*((float)nb_stack_miss_addr/(float)(nb_stack_hit_addr+nb_stack_miss_addr)));
}

