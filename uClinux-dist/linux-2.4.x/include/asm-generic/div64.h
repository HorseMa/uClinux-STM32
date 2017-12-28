#ifndef _ASM_GENERIC_DIV64_H
#define _ASM_GENERIC_DIV64_H

#include <linux/types.h>

/*
 * do_div() performs a 64bit/32bit unsigned division and modulo.
 * The 64bit result is stored back in the divisor, the 32bit
 * remainder is returned.
 *
 * A semantically correct implementation would do this:
 *
 *	static inline uint32_t do_div(uint64_t &n, uint32_t base)
 *	{
 *		uint32_t rem;
 *		rem = n % base;
 *		n = n / base;
 *		return rem;
 *	}
 *
 * We can't rely on GCC's "long long" math since it would turn
 * everything into a full 64bit division implemented through _udivdi3(),
 * which is much slower.
 */

#if BITS_PER_LONG == 64

# define do_div(n,base) ({					\
	uint32_t __res;						\
	__res = ((uint64_t)(n)) % (uint32_t)(base);		\
	(n) = ((uint64_t)(n)) / (uint32_t)(base);		\
	__res;							\
 })

#elif BITS_PER_LONG == 32

#define __div64_32(n, base) ({					\
								\
        uint64_t __rem = (n);					\
        uint64_t __b = base;					\
        uint64_t __res, __d = 1;				\
        uint32_t __high = __rem >> 32;				\
								\
        /* Reduce the thing a bit first */			\
        __res = 0;						\
        if (__high >= base) {					\
                __high /= base;					\
                __res = (uint64_t) __high << 32;		\
                __rem -= (uint64_t) (__high*base) << 32;	\
        }							\
								\
        while ((int64_t)__b > 0 && __b < __rem) {		\
                __b = __b+__b;					\
                __d = __d+__d;					\
        }							\
								\
        do {							\
                if (__rem >= __b) {				\
                        __rem -= __b;				\
                        __res += __d;				\
                }						\
                __b >>= 1;					\
                __d >>= 1;					\
        } while (__d);						\
								\
        (n) = __res;						\
        __rem;							\
})

# define do_div(n,base)	({					\
								\
	uint32_t __low, __high, __rem;				\
	__low  = (n) & 0xffffffff;				\
	__high = (n) >> 32;					\
	if (__high) {						\
		__rem = __div64_32(n, base);			\
	} else {						\
		__rem = __low % (uint32_t)(base);		\
		(n) = (__low / (uint32_t)(base));		\
	}							\
	__rem;							\
 })

#else /* BITS_PER_LONG == ?? */
# error do_div() does not yet support the C64
#endif /* BITS_PER_LONG */

#endif /* _ASM_GENERIC_DIV64_H */
