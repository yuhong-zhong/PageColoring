# Linux Page Coloring

This repository contains a page coloring implementation on Linux. When Optane Persistent Memory (PMem) is in Memory Mode, DRAM becomes a direct-mapped cache for PMem, which means the OS does not have direct control over the amount of DRAM a process can use. [Page coloring](https://en.wikipedia.org/wiki/Cache_coloring) is a technique that can control the DRAM share of a process by limiting which physical pages the process can use. The page coloring implementation in this repository provides a modified Linux kernel (based on 5.10) and some user-space tools to configure the DRAM limit of a process. 

## Assumptions

[Lu et al.](https://arxiv.org/pdf/2009.14469.pdf) and [Hildebrand et al.](https://ieeexplore.ieee.org/document/9408179) report that the mapping between DRAM and PMem in Optane Memory Mode is a modulo function with the DRAM size as the modulus. If there is only one NUMA node and the total DRAM capacity is D, then two physical memory addresses PA1 and PA2 are mapped to the same place in DRAM iff `PA1 % D == PA2 % D`. We verified this finding on both Optane PMem 100 series and Optane PMem 200 series.

This implementation only applies page coloring to the anonymous pages of user-space processes. Page cache and kernel pages are out of the scope of this work.

Compared to the unmodified kernel, the modified kernel has a slower page allocator due to the page coloring overhead. This is not a problem for the applications that allocate all the pages at the beginning, but in other cases a better implementation may be required.

## Usage

### Compiling the Kernel

The modified kernel hardcodes some hardware configurations in `linux/include/linux/colormask.h`. You need to modify them to make it work in your setting:

* `DRAM_SIZE_PER_NODE`: Total DRAM capacity per NUMA node (in bytes)
* `NR_COLORS`: Total number of colors
* `NR_PMEM_CHUNK`: Ratio between PMem capacity and DRAM capacity
* `COLOR_THP`: Enable page coloring for Transparent Huge Page (THP)

On our test server, there are 16 GB * 6 = 96 GB DRAM (`DRAM_SIZE_PER_NODE = 96ul << 30`) and 128 GB * 6 = 768 GB PMem on each NUMA node, which implies that the PMem to DRAM ratio is 8:1 (`NR_PMEM_CHUNK = 8`). We set `NR_COLORS` to `768`, so each color represents 96 GB / 128 = 128 MB DRAM and 768 GB / 768 = 1GB PMem.

By default, the macro `COLOR_THP` is not set, which means page coloring does not apply to THP pages. If `COLOR_THP` is set, page coloring is only used for THP pages. We enable `COLOR_THP` in most of our experiments to reduce the effect of TLB misses.

After modifying these constants, you need to compile and boot into the modified kernel.

### Using the User-Space Tools

The user-space tools are located in the `tools` folder.

To use page coloring, you need to first reserve some pages for page coloring:

```
./reserve_color <number of pages per color>
```

This command will allocate some pages to fill the page coloring pool. When page coloring is in effect, pages are allocated from the page coloring pool instead of the buddy system. In our setting with `COLOR_THP` and `NR_COLORS = 768`, we usually run `./reserve_color 384` to reserve 384 * 2 MB * 768 = 576 GB memory (75% of the total system memory) for page coloring. Reserving too much memory may cause the kernel or other processes to OOM, so the modified kernel does not allow any number that translates to more than 75% of the total memory. This command usually takes a while to run, but you only need to run it once unless you reboot the server.

You can set the DRAM limit of a process using `colorctl`:

```
./colorctl <list of available colors (e.g., 0-7,8-15,20)> <pid> [command (optional)]
```

For example, to execute a program `./foo` with 3 GB DRAM (i.e. 3 GB / 128 MB = 3 * 8 = 24 colors), you can run `numactl --cpunodebind 0 --membind 0 ./colorctl 0-$((3 * 8  - 1)) 0 ./foo`. This command specifies that the current process (pid 0) can only allocate pages from color 0 to 23. In the current implementation, if there is no page available in the page coloring pool, the allocator will fall back to the normal page allocation routine and generate an error message to `/sys/kernel/debug/tracing/tace`. This could happen if `./foo` needs more than (384 * 24) 2MB pages.

On a host with 2 NUMA nodes, color 0 on NUMA 0 and color 0 on NUMA 1 are two different colors. So a process run with `./colorctl 0,1 0` can use up to 2 * 128 MB * 2 = 512 MB DRAM in total.

You can also check the list of the colors that can be used by a process:

```
./get_color <pid>
```

Since `NR_COLORS` is also hardcoded in `colorctl.cpp` and `get_color.cpp`, you need to change them if you update `NR_COLORS` in the kernel.

### Page Coloring Stats

You can find some stats related to page coloring:

```
cat /proc/colorinfo | more
```

It prints the number of free pages (`free`) of each color on each NUMA node. It also indicate how many pages have been allocated from each color (`allocated`). Note that `allocated` is cumulative, which means it won't be decremented when a page is returned to the page coloring pool.

Conflicting pages with the same color are separated into different lists. For example, if the PMem to DRAM ratio is 8:1, the 8 pages that are mapped to the same place in DRAM will locate in 8 different lists under the same color. `/proc/colorinfo` shows the stats at a per-list granularity.

## Implementation

The page coloring system maintains a global page coloring pool (`struct free_color_area color_area_arr`) which stores the pages used for page coloring. The page coloring pool is protected by per-color spin locks for concurrency control. When the `reserve_color` syscall is called, it will call `rebalance_colormem` to allocate pages from the buddy system to fill the page coloring pool.

`colorctl` calls the `set_color` syscall to configure the set of the colors that can be used by a process. A bitmap of all the colors available to a process (`colors_allowed`) is stored in `struct task_struct`.

When a page fault happens, the buddy allocator (`__alloc_pages_nodemask`) will check the allocation request. If the request is from a user-space process, the buddy allocator will let the page coloring allocator (`alloc_color_page`) to handle this request. The page coloring allocator then checks the set of available colors of that process and fetches a matching page from the pool. If a process has more than one available colors, the color used for allocation will be chosen in a round-robin way to minimize conflicts. The pages allocated by the page coloring allocator are tagged as `PageColored`.

After a process terminates, all of its pages will be freed. When a page tagged as `PageColored` is being freed, it will be returned to the page coloring pool instead of being returned to the buddy system.