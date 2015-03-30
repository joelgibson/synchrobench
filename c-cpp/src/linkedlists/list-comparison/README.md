# Comparison of concurrent linked lists

Each list should be compiled using only the two C files `<algo>.c` and
`test.c`. For example, the Harris linked list can be compiled essentially
by doing

    gcc -std=c11 -DHARRIS harris.c test.c -o harris

Each algorithm can define a `struct intset` and a `struct node` in `<algo>.h`,
and implement the set operations in `<algo>.c`. There are some common set
operations in the `mixin.c` files, which can be `#incude`d into the `<algo>.c`
file, or an algorithm can write its own.

It's sometimes worth running the algorithms using a different memory allocator,
to quickly see if that's a bottleneck. Two common ones are tcmalloc and jemalloc.

    LD_PRELOAD=/usr/lib/libtcmalloc.so.4
    LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so

$ sudo bash
for i in /sys/devices/system/cpu/cpu[0-7]
do
    echo performance > $i/cpufreq/scaling_governor
done
#