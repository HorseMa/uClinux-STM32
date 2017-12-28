#ifndef __ASM_CACHE_H
#define __ASM_CACHE_H

#include <linux/config.h>

/* bytes per L1 cache line */
#ifdef CONFIG_FR55x
#define L1_CACHE_SHIFT		6
#else
#define L1_CACHE_SHIFT		5
#endif
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

#define __cacheline_aligned	__attribute__((aligned(L1_CACHE_BYTES)))
#define ____cacheline_aligned	__attribute__((aligned(L1_CACHE_BYTES)))

#endif
