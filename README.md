Memprof Parser
==============
The Memprof Parser. Used to analyse data collected by the module and by the library.

WARNING: In order to compile the parser, you MUST have a copy of the Memprof Kernel module in ../module and a copy of the Memprof Library in ../library.


Usage:

```bash
make 
sudo insmod ../module/memprof.ko 
echo b > /proc/memprof_cntl 
LD_PRELOAD=../library/ldlib.so <app> 
echo e > /proc/memprof_cntl 
cat /proc/memprof_ibs > ibs.raw 
cat /proc/memprof_perf > perf.raw 
../library/merge /tmp/data.* 
./parse ibs.raw --data data.processed.raw --perf perf.raw [options, e.g. -M] 
```

Options
=======
The most important options are:

```bash
--top-fun: outputs a list of function, sorted by the number of memory accesses they have performed 
--top-obj: ouputs the list of objects accessed in memory 
--obj <id>: outputs the access pattern to the object <id>. <id> can be found using the --top-obj option (it is the first number on each line of the output)
--top-mmap: outputs the list of most accessed mmaps/files (--top-object is more precise because it can splits mmaps into objects)
```

Note: the parser API has changed since the publication of "MemProf: a Memory Profiler for NUMA Multicore Systems". We now use a much simpler API based on 2 functions: get_object(sample) and get_function(sample) that return respectively the accessed object and the function performing the access. You can also use sample_to_mmap(s) to get the mmap of an object.
