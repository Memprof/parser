#include "parse.h"
#include "builtin-memory-hierarchy.h"

/*
 * Shows were memory accesses found their data (L1, L2, L3, DRAM, ...).
 * Called using the -C option.
 */

static int restrict_to_data_misses = 0;

static char* source_name[] = {
   "invalid",
   "Local L3",
   "Other core L1/L2",
   "Local DRAM",
   "?",
   "?",
   "?",
   "APIC/PCI",

   "invalid",
   "?",
   "Remote L1/L2/L3",
   "Remote DRAM",
   "?",
   "?",
   "?",
   "APIC/PCI",
};
static int global_data_source[16];
static int stack_data_source[16];
static int kernel_data_source[16];
static int softirq_data_source[16];
static int user_data_source[16];
static int num_local_per_source[16];
static int num_distant_per_source[16];
static int num_local_per_source_user[16];
static int num_distant_per_source_user[16];
static int num_local_per_source_kernel[16];
static int num_distant_per_source_kernel[16];
static int total_samples;
static int data_miss;
static int load, store;
static int lin_valid, phy_valid;
static int nb_non_null_phys_kern, nb_non_null_phys_user;

void mem_hierarchy_init() {
   memset(global_data_source, 0, sizeof(global_data_source));
   memset(stack_data_source, 0, sizeof(stack_data_source));
   memset(kernel_data_source, 0, sizeof(kernel_data_source));
   memset(softirq_data_source, 0, sizeof(softirq_data_source));
   memset(user_data_source, 0, sizeof(user_data_source));
   total_samples = data_miss = load = store = 0;
   lin_valid = phy_valid = 0;
   nb_non_null_phys_user = nb_non_null_phys_kern = 0;
}

void mem_hierarchy_parse(struct s* s) {
   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   ibs_op_data3_t *data3 = (void*)&udata3;
   if(!s->ibs_dc_phys)
      return;

   if(!restrict_to_data_misses || (restrict_to_data_misses && data3->ibsdcmiss)) {
      int status = (s->ibs_op_data2_low & 7) + 1*((s->ibs_op_data2_low & 16)>>1);
      total_samples++;

      global_data_source[status]++;
      if(is_kernel(s))
         kernel_data_source[status]++;
      else
         user_data_source[status]++;
      if(is_softirq(s))
         softirq_data_source[status]++;

      if(s->stack && s->ibs_dc_linear) {
         if(s->ibs_dc_linear >= s->usersp && s->ibs_dc_linear <= (uint64_t)s->stack) {
            stack_data_source[status]++;
         }
      }
      if(data3->ibsdcmiss)
         data_miss++;
      if(data3->ibsstop)
         store++;
      if(data3->ibsldop)
         load++;
      if(data3->ibsdclinaddrvalid)
         lin_valid++;
      if(data3->ibsdcphyaddrvalid)
         phy_valid++;
      if(s->ibs_dc_phys) {
         if(is_kernel(s)) {
            nb_non_null_phys_kern++;
            if(cpu_to_node(s->cpu) != get_addr_node(s))
               num_distant_per_source_kernel[status]++;
            else
               num_local_per_source_kernel[status]++;
         } else {
            nb_non_null_phys_user++;
            if(cpu_to_node(s->cpu) != get_addr_node(s))
               num_distant_per_source_user[status]++;
            else
               num_local_per_source_user[status]++;
         }
      }
   }
}

void mem_hierarchy_show() {
   int i, n, sum_glob = 0, sum_stack = 0, sum_user = 0, sum_kernel = 0, sum_irq, sum_local = 0, sum_distant = 0, sum_local_user = 0, sum_distant_user = 0, sum_local_kernel = 0, sum_distant_kernel = 0;

   printf("\t\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      printf("%d (%s)%n", i, source_name[i], &n);
      printf("%*.*s", 24-n, 24-n, "");
      sum_glob += global_data_source[i];
      sum_stack += stack_data_source[i];
      sum_kernel += kernel_data_source[i];
      sum_user += user_data_source[i];
      sum_irq += softirq_data_source[i];

      num_local_per_source[i] = num_local_per_source_user[i] + num_local_per_source_kernel[i];
      sum_local += num_local_per_source[i];
      sum_local_user += num_local_per_source_user[i];
      sum_local_kernel += num_local_per_source_kernel[i];

      num_distant_per_source[i] = num_distant_per_source_user[i] + num_distant_per_source_kernel[i];
      sum_distant += num_distant_per_source[i];
      sum_distant_user += num_distant_per_source_user[i];
      sum_distant_kernel += num_distant_per_source_kernel[i];
   }
   printf("\n");
   if(!sum_glob) sum_glob = 1;
   if(!sum_stack) sum_stack = 1;
   if(!sum_kernel) sum_kernel = 1;
   if(!sum_user) sum_user = 1;
   if(!sum_irq) sum_irq = 1;
   if(!sum_distant) sum_distant = 1;
   if(!sum_distant_user) sum_distant_user = 1;
   if(!sum_distant_kernel) sum_distant_kernel = 1;

   printf("Total:\t\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      if(global_data_source[i] > 10000000)
         printf("%dK\t(%5.2f%%)\t", global_data_source[i]/1000, 100.*((float)global_data_source[i])/((float)sum_glob));
      else
         printf("%d\t(%5.2f%%)\t", global_data_source[i], 100.*((float)global_data_source[i])/((float)sum_glob));
   }
   printf("\n");

   printf("Stack:\t\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      printf("%d\t(%5.2f%%)\t", stack_data_source[i], 100.*((float)stack_data_source[i])/((float)sum_stack));
   }
   printf("\n");

   printf("User:\t\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      printf("%d\t(%5.2f%%)\t", user_data_source[i], 100.*((float)user_data_source[i])/((float)sum_user));
   }
   printf("\n");

   printf("Kernel:\t\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      printf("%d\t(%5.2f%%)\t", kernel_data_source[i], 100.*((float)kernel_data_source[i])/((float)sum_kernel));
   }
   printf("\n");

   printf("SoftIRQ: (no longer supported)\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      printf("%d\t(%5.2f%%)\t", softirq_data_source[i], 100.*((float)softirq_data_source[i])/((float)sum_irq));
   }
   printf("\n");

   printf("Locality:\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      printf("\t %5.2f%% \t", 100.*((float)num_local_per_source[i])/((float)(num_local_per_source[i]+num_distant_per_source[i])));
   }
   printf("\n");

   printf("Loc (user):\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      printf("\t %5.2f%% \t", 100.*((float)num_local_per_source_user[i])/((float)(num_local_per_source_user[i]+num_distant_per_source_user[i])));
   }
   printf("\n");

   printf("Loc (kern):\t");
   for(i = 0; i < 12; i++) {
      if(i==4 || i==5 || i==6 || i==7 || i==8 || i==9) continue;
      printf("\t %5.2f%% \t", 100.*((float)num_local_per_source_kernel[i])/((float)(num_local_per_source_kernel[i]+num_distant_per_source_kernel[i])));
   }
   printf("\n");

   printf("\n");

   printf("%d data miss (%.2f%%)\n", data_miss, 100.*((float)data_miss)/((float)total_samples));
   printf("%d load\n", load);
   printf("%d store (%.2f%%)\n", store, 100.*((float)store)/((float)(load+store)));
   printf("%d lin_valid\n", lin_valid);
   printf("%d phy_valid\n", phy_valid);

   printf("Non null kernel: %d\n", nb_non_null_phys_kern);
   printf("Non null user: %d\n", nb_non_null_phys_user);
   printf("%.2f%% of local accesses\n", 100.*((float)sum_local)/((float)sum_local+sum_distant));
   printf("%.2f%% of local accesses (user)\n", 100.*((float)sum_local_user)/((float)sum_local_user+sum_distant_user));
   printf("%.2f%% of local accesses (kern)\n", 100.*((float)sum_local_kernel)/((float)sum_local_kernel+sum_distant_kernel));

   for(i = 4; i < 8; i++) {
      if(global_data_source[i]) {
         fprintf(stdout, "#Warning: data in global_data_source[%d] (%s)\n", i, source_name[i]);
      }
   }
   for(i = 8; i < 16; i++) {
      if(i==10 || i==11) continue;
      if(global_data_source[i]) {
         fprintf(stdout, "#Warning: data in global_data_source[%d] (%s)\n", i, source_name[i]);
      }
   }
}

