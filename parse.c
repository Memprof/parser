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
#include "compatibility.h"

/* Samples analysis */
#include "builtin-top-functions.h"      // -M --top-fun
#include "builtin-top-mmaps.h"          // --top-mmap
#include "builtin-top-obj.h"            // -X --top-obj
#include "builtin-object.h"             // --obj <uid>
#include "builtin-memory-overlap.h"     // -O -P -T -N
#include "builtin-stats.h"              // -S
#include "builtin-zones.h"              // -Z
#include "builtin-pages.h"              // --pages

/* Statistics on samples (repartition, etc.) */
#include "builtin-memory-hierarchy.h"   // -C
#include "builtin-memory-repartition.h" // -m
#include "builtin-get-sched-stats.h"    // --get-sched-stats <tid>
#include "builtin-get-npages.h"         // --get-npages <tid>


/* Options stat might modify the machine state (thread placement, etc.) */
#include "builtin-sched.h"              // --sched <app>
#include "builtin-migrate.h"            // --migrate

/* Utility options */
#include "builtin-dump.h"               // -dX [X between 1 and 9]
#include "builtin-stack-check.h"        // --stack
#include "builtin-sql.h"                // dump a trace in sql format

#include <getopt.h>
static int pipe_to_less();
static void join_less();


buffer_parser_t parse;
results_shower_t show_results;
init_function_t init;

int verbose = 0;
char *mmaped_file_name;
FILE *mmaped_file;
char *app_filter;
char *fun_regex;
int pid_filter = -1;
int tid_filter = -1;
int cpu_filter = -1;
enum ring_filter_t { filter_userland = 1, filter_kernel = 2, filter_irq = 4, exclude_filter_irq = 8 };
int ring_filter = 0;
int node_filter = -1;
int filter_local = 0;
int filter_l1l2 = 0;
int filter_dram = 0;
int filter_dist_cache = 0;
int filter_stack = 0;
int node_exclude_filter = -1;
int force_stdout = 0;
char *function_filter;
int filter_store = 0;
int filter_load = 0;
uint64_t time_beg = -1, time_end = -1;
uint64_t phys_beg = -1, phys_end = -1;
uint64_t virt_beg = -1, virt_end = -1;
char *default_perf_file = "./perf.raw";
struct list *perf_files = NULL;
struct list *data_files = NULL;
char *path_to_binaries = NULL;
char *opt_machine;
int latency_filter = 0;
int create_sorted_file = 0;
uint64_t first_rdt = -1;
uint64_t last_rdt = 0;
char *alloc_location = NULL;

void usage() {
   fprintf(stderr, "Usage : ./parse [--perf perf.raw] [--data data.processed.raw] [option] ibs.raw\n");
   fprintf(stderr, "\nTo obtain .raw files:\n");
   fprintf(stderr, "\tUse ../scripts/profile_app.sh or type the following commands:\n");
   fprintf(stderr, "\trm /tmp/data.raw.*; sudo insmod ../module/memprof.ko; echo b > /proc/memprof_cntl;\n");
   fprintf(stderr, "\tLD_PRELOAD=../library/ldlib.so <app>;\n");
   fprintf(stderr, "\techo e > /proc/memprof_cntl;\n");
   fprintf(stderr, "\tcat /proc/memprof_ibs > ibs.raw\n");
   fprintf(stderr, "\tcat /proc/memprof_perf > perf.raw\n");
   fprintf(stderr, "\t../library/merge /tmp/data.raw #will create data.processed.raw in current dir\n");
   fprintf(stderr, "\nMandatory options (select one):\n");
   fprintf(stderr, "\t-M, --top-fun           \tShow top functions\n");
   fprintf(stderr, "\t-X, --top-obj [-2]      \tShow most accessed objects\n");
   fprintf(stderr, "\t                        \tAdd a -2 to cluster objects by the location (function) where they were allocated\n");
   fprintf(stderr, "\t--obj <uid>             \tShow access patterns to object <uid> (uid can be found with option -X)\n");
   fprintf(stderr, "\t--pages <uid>           \tShow accesses to pages of object <uid> (uid can be found with option -X; when uid=0 show access to all pages)\n");
   fprintf(stderr, "\t--top-mmap              \tShow most accesses mmmaps (less precise than objects)\n");
   fprintf(stderr, "\t--get-npages <mmap>     \tShow number of accessed pages of mmap <mmap> (e.g. libc.so)\n");
   fprintf(stderr, "\t-Z <size> [-2]          \tReturns ranges of virtual addresses where most of the accesses are performed (less precise than mmaps)\n");
   fprintf(stderr, "\t                        \tIf two addresses are separated by less than <size>, they are grouped in the same cluster\n");
   fprintf(stderr, "\t                        \tAdd a '-2' after to size to cluster by physical addresses (meaningless most of the time)\n");
   fprintf(stderr, "\t-O, --overlap-app <app> \tShow objects that are accessed by multiple apps\n");
   fprintf(stderr, "\t-P, --overlap-pid <pid> \tShow objects that are accessed by multiple pids\n");
   fprintf(stderr, "\t-T, --overlap-tid <tid> \tShow objects that are accessed by multiple tids\n");
   fprintf(stderr, "\t                        \te.g.: ./parse -O httpd -O php-cgi -T 4852 log.raw\n");
   fprintf(stderr, "\t                        \t      Show objects manipulated by at least two tids from {httpd's tids, php's tids, <4852>}\n");
   fprintf(stderr, "\t-N, --non-shared <tid>  \tShow physical pages which are accessed only by <tid>\n");
   fprintf(stderr, "\t                        \te.g.: ./parse -O httpd -N 4852 log.raw\n");
   fprintf(stderr, "\t                        \t      Show objects manipulated by <4852> but NOT by any other httpd tid\n");
   fprintf(stderr, "\t-m                      \tShow stats: memory Memory repartition (Applications/Nodes/Cpu)\n");
   fprintf(stderr, "\t-C                      \tShow stats: memory access repartition (DRAM/Stack/... User/Kernel/...)\n");
   fprintf(stderr, "\t-S, --stats             \tShow stats: number of accessed pages per tid, per core to each node, with nice colors\n");
   fprintf(stderr, "\t--get-sched-stats <tid> \tReturns the number of node changes of <tid> (and also outputs the pid of <tid>)\n");
   fprintf(stderr, "\t-dX                     \tDump raw file in a readable format (X in {1..9}, 1 is default)\n");
   fprintf(stderr, "\t                        \t* 1: dump IBS samples without any analysis\n");
   fprintf(stderr, "\t                        \t* 2: dump IBS samples with function and accessed variables\n");
   fprintf(stderr, "\t                        \t* 3-4: legacy options, not useful anymore\n");
   fprintf(stderr, "\t                        \t* 5: Breakdown of latency of memory accesses\n");
   fprintf(stderr, "\t                        \t* 6: For each studied threads, the number of cpu switch\n");
   fprintf(stderr, "\t                        \t* 7: Linear addresses touched by all threads plot'able with ../scripts/threads.cmd\n");
   fprintf(stderr, "\t                        \t* 8: Physical addresses touched by all threads plot'able with ../scripts/threads.cmd\n");
   fprintf(stderr, "\t                        \t* 9: Locality of memory accesses plot'able with ../scripts/locality.cmd\n");
   fprintf(stderr, "\t--sched <app>           \tTaskset <app> according to its memory accesses pattern\n");
   fprintf(stderr, "\t                        \tMultiple apps may be specified with multiple --sched parameters\n");
   fprintf(stderr, "\t                        \tSee ../scripts/move_processes_close_to_their_pages.pl to learn how to use this option\n");
   fprintf(stderr, "\t--migrate               \tMigrate memory pages close to the thread using them most\n");
   fprintf(stderr, "\t                        \tSee ../scripts/move_unshared_page_close_to_their_owner.pl to learn how to use this option\n");
   fprintf(stderr, "\t--sql                   \tConvert a profiling file to an SQL dump\n");
   fprintf(stderr, "\nOther options (filters):\n");
   fprintf(stderr, "\t-a, --app <app>         \tShow only samples of application <app>\n");
   fprintf(stderr, "\t-p, --pid <pid>         \tShow only samples of pid <pid>\n");
   fprintf(stderr, "\t-t, --tid <tid>         \tShow only samples of tid <tid>\n");
   fprintf(stderr, "\t-c, --cpu <cpu>         \tShow only samples of cpu <cpu>\n");
   fprintf(stderr, "\t--node <node>           \tShow only samples touching node <node>\n");
   fprintf(stderr, "\t--exclude-node <node>   \tShow only samples not touching node <node>\n");
   fprintf(stderr, "\t-l, --local             \tShow only samples touching local memory\n");
   fprintf(stderr, "\t-L, --non-local         \tShow only samples touching distant memory\n");
   fprintf(stderr, "\t--non-local-cache       \tShow only samples not accessing local L1 or L2\n");
   fprintf(stderr, "\t-u, --user              \tShow ONLY samples coming from userland\n");
   fprintf(stderr, "\t-k, --kernel            \tShow ONLY samples coming from kernelland\n");
   fprintf(stderr, "\t-f, --function <f>      \tShow ONLY samples coming from function <f>\n");
   fprintf(stderr, "\t-F, --Function <f>      \tShow only samples coming from functions m/f/\n");
   fprintf(stderr, "\t--allocated <location>  \tShow only samples touching objects allocated from <location>\n");
   fprintf(stderr, "\t--st                    \tShow ONLY store operations\n");
   fprintf(stderr, "\t--ld                    \tShow ONLY load operations\n");
   fprintf(stderr, "\t--DRAM                  \tShow ONLY operations touching DRAM\n");
   fprintf(stderr, "\t--non-stack             \tShow ONLY operations not touching the stack\n");
   fprintf(stderr, "\t--perf <file>           \tGet MMAP and pid information from <file>\n");
   fprintf(stderr, "\t--phys <a>-<b>          \tShow ONLY samples that access memory between <a> and <b> physical addresses\n");
   fprintf(stderr, "\t                        \t<a> and <b> can be either hexa or decimal, e.g., --phys 0xa-0xb or --phys 10-11\n");
   fprintf(stderr, "\t--virt <a>-<b>          \tShow ONLY samples that access memory between <a> and <b> virtual addresses\n");
   fprintf(stderr, "\t--time <a>-<b>          \tShow ONLY samples happening between <a> and <b> (rdtscll values)\n");
   fprintf(stderr, "\t--latency <l>           \tShow ONLY samples whose latency is more than l (according to IBS)\n");
   fprintf(stderr, "\t--bin, -B <path>        \tPath to binaries and libraries. If specified, samples will be resolved with libraries stored in that path\n");
   fprintf(stderr, "\t--stdout                \tPrint output to stdout directly instead of piping to a less\n");
   fprintf(stderr, "\t-v                      \tVerbose\n");
   fprintf(stderr, "Legacy options:\n");
   fprintf(stderr, "\t--stack                 \tShow percentage of hits in the stack\n");
   fprintf(stderr, "\t--machine <m>           \tSpecify the machine name. E.g. --machine sci100. Currently ignored.\n");
   exit(-1);
}

void parse_options(int argc, char** argv) {
   int c;
   if(argc < 2) {
      usage();
   }

   modifier_function_t modifier_function = NULL;
   while (1) {
      static struct option long_options[] = {
         {"memory-repartition",     no_argument,       0, 'm'},
         {"top-mmap",     no_argument,       0, 'W'},
         {"dump",  no_argument,       0, 'd'},
         {"stack",  no_argument, 0, '!'},
         {"cache-hit",  no_argument, 0, 'C'},
         {"stats",  no_argument, 0, 'S'},
         {"user",  no_argument, 0, 'u'},
         {"kernel",  no_argument, 0, 'k'},
         {"stdout",  no_argument, 0, '*'},
         {"top-fun",  no_argument, 0, 'M'},
         {"sql",  no_argument, 0, 's'},
         {"top-obj",  no_argument, 0, 'X'},
         {"get-npages",  no_argument, 0, 'Y'},
         {"st",  no_argument, 0, ';'},
         {"ld",  no_argument, 0, '.'},
         {"DRAM",  no_argument, 0, '|'},
         {"dist-cache",  no_argument, 0, '}'},
         {"non-stack",  no_argument, 0, ']'},
         {"app",  required_argument, 0, 'a'},
         {"pid",  required_argument, 0, 'p'},
         {"tid",  required_argument, 0, 't'},
         {"cpu",  required_argument, 0, 'c'},
         {"sched",  required_argument, 0, '%'},
         {"get-sched-stats",  required_argument, 0, '$'},
         {"obj",  required_argument, 0, '@'},
         {"pages",  required_argument, 0, '\''},
         {"migrate",  no_argument, 0, '<'},
         {"zones",  optional_argument, 0, 'Z'},
         {"node",  required_argument, 0, '_'},
         {"exclude-node",  required_argument, 0, '('},
         {"local",  no_argument, 0, 'l'},
         {"non-local",  no_argument, 0, 'L'},
         {"non-local-cache",  no_argument, 0, '`'},
         {"overlap-app",  required_argument, 0, 'O'},
         {"overlap-tid",  required_argument, 0, 'T'},
         {"overlap-tid",  required_argument, 0, 'P'},
         {"non-shared",  required_argument, 0, 'N'},
         {"function",  required_argument, 0, 'f'},
         {"Function",  required_argument, 0, 'F'},
         {"perf",  required_argument, 0, '+'},
         {"data",  required_argument, 0, '='},
         {"time",  required_argument, 0, '>'},
         {"phys",  required_argument, 0, '/'},
         {"virt",  required_argument, 0, '{'},
         {"allocated",  required_argument, 0, '&'},
         {"latency",  required_argument, 0, '['},
         {"bin",  required_argument, 0, 'B'},
         {"machine",  required_argument, 0, 'Q'},
         {"help",  optional_argument, 0, 'h'},
         {"verbose",  optional_argument, 0, 'v'},
         {"one",  no_argument, 0, '1'},
         {"two",  no_argument, 0, '2'},
         {"three",  no_argument, 0, '3'},
         {"four",  no_argument, 0, '4'},
         {"five",  no_argument, 0, '5'},
         {"six",  no_argument, 0, '6'},
         {"seven",  no_argument, 0, '7'},
         {"eight",  no_argument, 0, '8'},
         {"nine",  no_argument, 0, '8'},
         {"V",  no_argument, 0, 'V'},
         {0, 0, 0, 0}
      };
      int option_index = 0;

      c = getopt_long (argc, argv, "hmd!a:p:Cc:ukO:t:T:SN:(:_:%:Mf:;.iIv$:P:+:=:W>:/:B:123456789Q:lLF:Z:é:|{:[:V<]}XY:s@:`':'", long_options, &option_index);
      if (c == -1)
         break;

      switch (c) {
         case 0:
            if (long_options[option_index].flag != 0)
               break;
            printf ("option %s", long_options[option_index].name);
            if (optarg)
               printf (" with arg %s", optarg);
            printf ("\n");
            break;

         case '1':
         case '2':
         case '3':
         case '4':
         case '5':
         case '6':
         case '7':
         case '8':
         case '9':
            if(!modifier_function) {
               fprintf(stderr, "Extra argument %c cannot be set with current flags\n", c);
               usage();
            }
            modifier_function(c-'0');
            break;

         case 'V':
            create_sorted_file = 1;
            force_stdout = 1;
            break;

         case 'm':
            init = memory_repartition_init;
            parse = memory_repartition_parse;
            show_results = memory_repartition_show;
            break;
      
         case 's': //SQL
            init = sql_init;
            parse = sql_parse;
            show_results = sql_show;
            break;

         case '@': //--obj
            init = obj_init;
            parse = obj_parse;
            show_results = obj_show;
            obj_set(atoi(optarg));
            break;

         case '\'': //--pages
            init = pages_init;
            parse = pages_parse;
            show_results = pages_show;
            if(optarg)
               pages_object_set(atoi(optarg));
            break;

         case 'W':
            init = top_mmap_init;
            parse = top_mmap_parse;
            show_results = top_mmap_show;
            break;

         case 'X':
            init = top_obj_init;
            parse = top_obj_parse;
            show_results = top_obj_show;
            modifier_function = top_obj_modifier;
            break;

         case '!':
            init = stack_check_init;
            parse = stack_check_parse;
            show_results = stack_check_show;
            break;

         case 'd':
            init = dump_init;
            parse = dump_parse;
            show_results = dump_show;
            modifier_function = dump_modifier;
            break;

         case 'C':
            init = memory_repartition_init;
            parse = memory_repartition_parse;
            show_results = memory_repartition_show;
            break;

         case 'O':
            init = overlap_init;
            parse = overlap_parse;
            show_results = overlap_show;
            overlap_add_application(optarg);
            break;

         case 'T':
            init = overlap_init;
            parse = overlap_parse;
            show_results = overlap_show;
            overlap_add_tid(atoi(optarg));
            break;

         case 'P':
            init = overlap_init;
            parse = overlap_parse;
            show_results = overlap_show;
            overlap_add_pid(atoi(optarg));
            break;

         case 'N':
            init = overlap_init;
            parse = overlap_parse;
            show_results = overlap_show;
            overlap_add_tid(atoi(optarg));
            overlap_add_tid_unshared(atoi(optarg));
            break;

         case 'Y':
            init = get_npages_init;
            parse = get_npages_parse;
            show_results = get_npages_show;
            get_npages_set(strdup(optarg));
            break;

         case 'S':
            init = stats_init;
            parse = stats_parse;
            show_results = stats_show;
            break;

         case '<':
            init = migrate_init;
            parse = migrate_parse;
            show_results = migrate_show;
            break;

         case '%':
            init = sched_init;
            parse = sched_parse;
            show_results = sched_show;
            sched_add_application(optarg);
            break;

         case 'M':
            init = top_fun_init;
            parse = top_fun_parse;
            show_results = top_fun_show;
            modifier_function = top_fun_modifier;
            break;

         case '$':
            force_stdout = 1;
            init = get_sched_stats_init;
            parse = get_sched_stats_parse;
            show_results = get_sched_stats_show;
            get_sched_stats_set(atoi(optarg));
            break;

         case 'Z':
            init = zone_init;
            parse = zone_parse;
            show_results = zone_show;
            modifier_function = zone_modifier;
            if(optarg)
               zone_set_cluster_size(atoi(optarg));
            break;

         /* The checks bellow are performed on the samples BEFORE sending them to xxx_parse */
         case 'a':
            app_filter = optarg;
            break;

         case 'F':
            fun_regex = optarg;
            break;

         case 'p':
            pid_filter = atoi(optarg);
            break;

         case 't':
            tid_filter = atoi(optarg);
            break;

         case 'c':
            cpu_filter = atoi(optarg);
            break;

         case 'u':
            ring_filter |= filter_userland;
            break;

         case 'k':
            ring_filter |= filter_kernel;
            break;

         case 'i':
            ring_filter |= filter_irq;
            break;

         case 'I':
            ring_filter |= exclude_filter_irq;
            break;

         case '(':
            node_exclude_filter = atoi(optarg);
            break;

         case '_':
            node_filter = atoi(optarg);
            break;

         case '[':
            latency_filter = atoi(optarg);
            break;

         case 'f':
            function_filter = strdup(optarg);
            break;

         case '*':
            force_stdout = 1;
            break;

         case ';':
            filter_store = 1;
            break;

         case '.':
            filter_load = 1;
            break;
      
         case '|':
            filter_dram = 1;
            break;

         case '`':
            filter_l1l2 = 1;
            break;

         case '}':
            filter_dist_cache = 1;
            break;

         case ']':
            filter_stack = 1;
            break;

         case 'l':
            filter_local = 1;
            break;

         case 'L':
            filter_local = 2;
            break;

         case '+':
            perf_files = list_add(perf_files, strdup(optarg));
            break;

         case '=':
            data_files = list_add(data_files, strdup(optarg));
            break;

         case '>':
            if(sscanf(optarg, "%lu-%lu", &time_beg, &time_end) != 2) {
               fprintf(stderr, "Invalid value for --time\n");
               exit(-1);
            }
            break;

         case '/':
            if(sscanf(optarg, "0x%lx-0x%lx", &phys_beg, &phys_end) != 2) {
               if(sscanf(optarg, "%lu-%lu", &phys_beg, &phys_end) != 2) {
                  fprintf(stderr, "Invalid value for --phys\n");
                  exit(-1);
               }
            }
            break;

         case '{':
            if(sscanf(optarg, "0x%lx-0x%lx", &virt_beg, &virt_end) != 2) {
               if(sscanf(optarg, "%lu-%lu", &virt_beg, &virt_end) != 2) {
                  fprintf(stderr, "Invalid value for --virt\n");
                  exit(-1);
               }
            }
            printf("%lx - %lx \n", virt_beg, virt_end);
            break;

         case 'B':
            path_to_binaries = strdup(optarg);
            break;

         case 'Q':
            opt_machine = strdup(optarg);
            break;

         case '&':
            alloc_location = strdup(optarg);
            break;

         case 'h':
            usage();
            break;


         case 'v':
            verbose = 1;
            break;

         case '?':
            /* getopt_long already printed an error message. */
            exit(-1);

         default:
            fprintf(stderr, "Unknown option\n");
            exit(-1);
      }
   }

   if(!init && !create_sorted_file) {
      fprintf(stderr, "Please specify at least one mandatory option\n\n");
      usage();
   }

   if(init == overlap_init && app_filter != NULL) {
      fprintf(stderr, "Options -O/-T and -a are incompatible.\n");
      exit(-1);
   }
   if(init == overlap_init && tid_filter != -1) {
      fprintf(stderr, "Options -O/-T and -t are incompatible.\n");
      exit(-1);
   }


   if (optind < argc) {
      mmaped_file_name = argv[optind];
      mmaped_file = open_file(argv[optind]);
      if(!mmaped_file) {
         fprintf(stderr, "Fatal: %s\n", open_file_error);
         exit(-1);
      }
   } else {
      fprintf(stderr, "Please specify a file to parse.\n");
      usage();
   }

   if(!perf_files)
      perf_files = list_add(perf_files, default_perf_file);
}

int is_valid(struct s *s) {
   if(pid_filter >= 0 && get_pid(s) != pid_filter) 
      return 0;
   if(tid_filter >= 0 && get_tid(s) != tid_filter) 
      return 0;
   if(cpu_filter >= 0 && s->cpu != cpu_filter) 
      return 0;
   if(latency_filter > 0 && get_latency(s) < latency_filter) 
      return 0;
   if((ring_filter & filter_kernel) && !is_kernel(s)) 
      return 0;
   if((ring_filter & filter_userland) && !is_user(s))
      return 0;
   if((ring_filter & filter_irq) && !is_softirq(s))
      return 0;
   if((ring_filter & exclude_filter_irq) && is_softirq(s))
      return 0;
   if(app_filter && strcmp(get_app(s), app_filter))
      return 0;
   if(node_filter >= 0 && (!s->ibs_dc_phys || get_addr_node(s) != node_filter)) 
      return 0;
   if(node_exclude_filter >= 0 && (!s->ibs_dc_phys || get_addr_node(s) == node_exclude_filter)) /*Exclude from node */
      return 0;
   if(filter_local == 1 && (!s->ibs_dc_phys || get_addr_node(s) != cpu_to_node(s->cpu)))
      return 0;
   if(filter_local == 2 && (!s->ibs_dc_phys || get_addr_node(s) == cpu_to_node(s->cpu)))
      return 0;
   if(function_filter && strcmp(get_function_name(s), function_filter))
      return 0;
   if(fun_regex && !strstr(get_function_name(s), fun_regex))
      return 0;
   if(alloc_location && strcmp(sample_to_variable(s), alloc_location))
      return 0;
   if(filter_store && !is_store(s))
      return 0;
   if(filter_load && !is_load(s))
      return 0;
   if(time_beg != -1 && (s->rdt < time_beg || s->rdt > time_end))
      return 0;
   if(phys_beg != -1 && (s->ibs_dc_phys < phys_beg || s->ibs_dc_phys > phys_end))
      return 0;
   if(virt_beg != -1 && (s->ibs_dc_linear < virt_beg || s->ibs_dc_linear > virt_end))
      return 0;
   if(filter_dram && (!hit_dram(s)))
      return 0;
   if(filter_l1l2 && (hit_locall1l2(s)))
      return 0;
   if(filter_dist_cache && (!hit_dist_cache(s)))
      return 0;
   if(filter_stack && hit_stack(s))
      return 0;


   return 1;
}

void show_options() {
   if(cpu_filter != -1)
      printf("#CPU filter: %d\n", cpu_filter);
   if(app_filter)
      printf("#APP filter: %s\n", app_filter);
}

struct read_symbol {
   uint64_t rdt;
   size_t pos;
   struct s s;
};

static int cmp_pri(uint64_t next, uint64_t curr) { return (next > curr); }
static uint64_t get_pri(void *a) {  return ((struct read_symbol *) a)->rdt; }
static void set_pri(void *a, uint64_t pri) {((struct read_symbol *) a)->rdt = pri; }
static size_t get_pos(void *a) { return ((struct read_symbol *) a)->pos; }
static void set_pos(void *a, size_t pos) { ((struct read_symbol *) a)->pos = pos; }


struct i i;
int main(int argc, char** argv) {
   struct h h;
   char *perf_file;
   int n;

   parse_options(argc, argv);

   pipe_to_less();
   if(verbose)
      show_options();

   /* Read version and header */
   fread(&h, sizeof(h), 1, mmaped_file);
   set_version(&h);
   read_header(mmaped_file, &i);

   /* Add perf events (no processing yet) */
   list_foreach(perf_files, perf_file) 
      read_perf_events(perf_file);
   list_foreach(data_files, perf_file) 
      read_data_events(perf_file);

   /* If the file is not sorted by rdt (V1 -- V3), we need to sort the IBS samples. */
   /* This is required for perf samples and data samples to be correctly interleaved between IBS samples */
   /* init / parse / show_results are defined according to the mandatory option */
   if(!i.sorted_by_rdt || create_sorted_file) {
      pqueue_t* symbols = pqueue_init(10, cmp_pri, get_pri, set_pri, get_pos, set_pos);
      struct read_symbol *s = malloc(sizeof(*s));
      while ((n = read_sample(&s->s, sizeof(s->s), 1, mmaped_file))) {
         s->rdt = s->s.rdt;
         pqueue_insert(symbols, s);
         s = malloc(sizeof(*s));
      }

      if(!create_sorted_file) {
         init();
         while((s = pqueue_pop(symbols))) {
            process_perf_samples(s->s.rdt);
            process_data_samples(s->s.rdt);
            if(is_valid(&s->s))
               parse(&s->s);
            free(s);
         }
         free(symbols);
         show_results();
      } else {
         char *new_file_name = NULL, *back_file_name = NULL;
         assert(asprintf(&new_file_name, "%s.new", mmaped_file_name));
         assert(asprintf(&back_file_name, "%s.bak", mmaped_file_name));

         FILE * sorted_file = fopen(new_file_name, "wb");
         if(!sorted_file)
            die("Cannot open %s for writing\n", new_file_name);

         h.version = S_VERSION;
         assert(fwrite (&h, 1, sizeof(h) , sorted_file));
         i.sorted_by_rdt = 1;
         assert(fwrite (&i, 1, sizeof(i) , sorted_file));

         while((s = pqueue_pop(symbols))) {
            assert(fwrite (&s->s, 1, sizeof(s->s) , sorted_file));
         }
         fclose (sorted_file);
         
         if(rename(mmaped_file_name, back_file_name)) 
            die("Failed to rename %s to %s\n", mmaped_file_name, back_file_name);
         if(rename(new_file_name, mmaped_file_name)) 
            die("Failed to rename %s to %s\n", new_file_name, mmaped_file_name);

         printf("#Conversion done.\n");
      }
   } else {
      struct s s;
      init();
      while ((n = read_sample(&s, sizeof(s), 1, mmaped_file))) {
         process_perf_samples(s.rdt);
         process_data_samples(s.rdt);
         if(is_valid(&s)) 
            parse(&s);

         if(s.rdt < first_rdt)
            first_rdt = s.rdt;
         if(s.rdt > last_rdt)
            last_rdt = s.rdt;
      }
      show_results();
      printf("#Duration %lu (%lu - %lu)\n", last_rdt - first_rdt, first_rdt, last_rdt);
   }

   /* Show stats gathered in process.c ; mainly the number of symbol resolution failures... */
   show_mmap_stats();
   show_data_stats();

   join_less();
	return 0;
}

char *get_app(struct s *s) {
   if(is_softirq(s))
      return "[softirq]";
   else
      return s->comm;
}

int is_user(struct s *s) {
   return (s->kern == 0); 
}

int is_kernel(struct s *s) {
   return (s->kern == 1);
}

int is_softirq(struct s *s) {
   return (0); // Used to work but requires a kernel modification...
}

int get_pid(struct s *s) {
   return s->pid; 
}

int get_tid(struct s *s) {
   return (s->tid); 
}

int get_addr_node(struct s *s) {
   return phys_to_node(s->ibs_dc_phys);
}

struct symbol *get_function(struct s *s) {
   return sample_to_function(s);
}

struct symbol *get_object(struct s *s) {
   struct symbol *ob = sample_to_variable2(s);
   if(ob)
      return ob;
   ob = sample_to_static_object(s);
   if(ob)
      return ob;

   return NULL;
}

char *get_function_name(struct s *s) {
   struct symbol * sym = get_function(s);
   return sym?sym->function_name:"";
}

struct dyn_lib* get_mmap(struct s *s) {
   return sample_to_mmap(s);
}

int is_store(struct s *s) {
   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   ibs_op_data3_t *data3 = (void*)&udata3;
   return data3->ibsstop;
}

int is_load(struct s *s) {
   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   ibs_op_data3_t *data3 = (void*)&udata3;
   return data3->ibsldop;
}

int get_latency(struct s *s) {
   /*uint64_t udata = (((uint64_t)s->ibs_op_data1_high)<<32) + (uint64_t)s->ibs_op_data1_low;
   ibs_op_data_t *data = (void*)&udata;
   return data->ibstagtoretctr;*/

   uint64_t udata3 = (((uint64_t)s->ibs_op_data3_high)<<32) + (uint64_t)s->ibs_op_data3_low;
   ibs_op_data3_t *data3 = (void*)&udata3;
   return data3->ibsdcmisslat;
}

int hit_dram(struct s *s) {
   int status = s->ibs_op_data2_low & 7;
   return (status == 3);
}

int hit_locall1l2(struct s *s) {
   int status = (s->ibs_op_data2_low & 7);
   return (status == 0);
}

int hit_dist_cache(struct s *s) {
   int status = (s->ibs_op_data2_low & 7) + 1*((s->ibs_op_data2_low & 16)>>1);
   return (status == 10);
}

int hit_stack(struct s *s) {
   return (s->stack && s->ibs_dc_linear && s->ibs_dc_linear >= s->usersp && s->ibs_dc_linear <= (uint64_t)s->stack);
}

int is_distant(struct s *s) {
   return (!s->ibs_dc_phys || get_addr_node(s) != cpu_to_node(s->cpu));
}

char *open_file_error = NULL;
FILE* open_file(char *f) {
   FILE *r = NULL;
   struct stat buf;
   int n = lstat(f, &buf);
   if(n) {
      if(!strstr(f, ".lzo")) {
         char *tmp = NULL;
         assert(asprintf(&tmp, "%s.lzo", f));
         r = open_file(tmp);
         free(tmp);
      }

      if(!r) {
         asprintf(&open_file_error, "File %s does not exist", f);
         return NULL;
      } else {
         return r;
      }
   }

   /* Handle decompression of LZO files */
   if(strstr(f, ".lzo")) {
      char *cmd;
      asprintf(&cmd, "lzop -c -d %s", f);
      r = popen(cmd, "r");
      if(!r) {
         asprintf(&open_file_error, "Command %s failed (%s)", cmd, strerror(errno));
      }
      free(cmd);
   } else {
      r = fopen(f,"r");
      if(!r) {
         asprintf(&open_file_error, "Cannot open file %s (%s)", f, strerror(errno));
      }
   }
   return r;
}


static int filedes[2];
static int less_pid = 0;
static int pipe_to_less() {
   if(!force_stdout && isatty(1)) {
      pipe(filedes); 
      int pid = fork();
      if(pid) {
         less_pid = pid;
         atexit(join_less);
         close(filedes[0]);
         dup2(filedes[1], 1);
         return 0;
      } else {
         close(filedes[1]);
         dup2(filedes[0], 0);
         execl("/usr/bin/less", "less", "-R", "-S", NULL);
         exit(0);
      }
   } else {
      return -1;
   }
}

static void join_less() {
   if(less_pid > 0) {
      close(filedes[1]);
      fflush(stdout);
      close(1);
      int status;
      do {
         int w = waitpid(less_pid, &status, WUNTRACED | WCONTINUED);
         if (w == -1) { perror("waitpid"); exit(EXIT_FAILURE); }
      } while (!WIFEXITED(status) && !WIFSIGNALED(status));
      less_pid = 0;
   }
}
