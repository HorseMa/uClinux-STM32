#ifndef _H8300_BITOPS_H
#define _H8300_BITOPS_H

/*
 * Copyright 1992, Linus Torvalds.
 * Copyright 2002, Yoshinori Sato
 */

#include <linux/config.h>
#include <asm/byteorder.h>	/* swab32 */
#include <asm/system.h>

#ifdef __KERNEL__
/*
 * Function prototypes to keep gcc -Wall happy
 */

/*
 * ffz = Find First Zero in word. Undefined if no zero exists,
 * so code should check against ~0UL first..
 */
static __inline__ unsigned long ffz(unsigned long word)
{
        unsigned long result;

	result = -1;
	__asm__("1:\n\t"
		"shlr.l %2\n\t"
		"adds #1,%0\n\t"
		"bcs 1b"
		: "=r" (result)
		: "0"  (result),"r" (word));
	return result;
}

#define H8300_GEN_BITOP(FNNAME,OP)					   \
static __inline__ void FNNAME(int nr, volatile void * addr)		   \
{									   \
	volatile unsigned char *b_addr;					   \
	b_addr = (volatile unsigned char *)addr + ((nr >> 3) ^ 3);         \
	if (__builtin_constant_p (nr & 7))				   \
	{								   \
		switch(nr & 7)						   \
		{							   \
		case 0:							   \
			__asm__(OP " #0,@%0" : :"r"(b_addr) : "memory");   \
			break;						   \
		case 1:							   \
			__asm__(OP " #1,@%0" : :"r"(b_addr) : "memory");   \
			break;						   \
		case 2:							   \
			__asm__(OP " #2,@%0" : :"r"(b_addr) : "memory");   \
			break;						   \
		case 3:							   \
			__asm__(OP " #3,@%0" : :"r"(b_addr) : "memory");   \
			break;						   \
		case 4:							   \
			__asm__(OP " #4,@%0" : :"r"(b_addr) : "memory");   \
			break;						   \
		case 5:							   \
			__asm__(OP " #5,@%0" : :"r"(b_addr) : "memory");   \
			break;						   \
		case 6:							   \
			__asm__(OP " #6,@%0" : :"r"(b_addr) : "memory");   \
			break;						   \
		case 7:							   \
			__asm__(OP " #7,@%0" : :"r"(b_addr) : "memory");   \
			break;						   \
		}							   \
	}								   \
	else								   \
	{								   \
		__asm__(OP " %w0,@%1" : :"r"(nr),"r"(b_addr) : "memory");  \
	}								   \
}

/*
 * clear_bit() doesn't provide any barrier for the compiler.
 */
#define smp_mb__before_clear_bit()	barrier()
#define smp_mb__after_clear_bit()	barrier()

H8300_GEN_BITOP(set_bit, "bset");

static __inline__ void _set_bit(int nr, const volatile void * addr)
{
	*(((volatile unsigned int *)addr)+(nr >> 5)) |=  (1 << (nr & 31));
}

H8300_GEN_BITOP(clear_bit, "bclr");
  
static __inline__ void _clear_bit(int nr, const volatile void * addr)
{
	*(((volatile unsigned int *)addr)+(nr >> 5)) &= ~(1 << (nr & 31));
}

H8300_GEN_BITOP(change_bit, "bnot");
  
static __inline__ void __change_bit(int nr, const volatile void * addr)
{
	*(((volatile unsigned int *)addr)+(nr >> 5)) ^=  (1 << (nr & 31));
}

/*
 * This routine doesn't need to be atomic.
 */

static __inline__ int test_bit(int nr, const volatile void * addr)
{
	return (*((volatile unsigned char *)addr + ((nr >> 3) ^ 3)) & (1UL << (nr & 7))) != 0;
}

#define __test_bit(nr, addr) test_bit((nr), (addr))

#define H8300_GEN_TEST_BITOP_IMM_INT(OP,BIT)		 	           \
	__asm__("stc ccr,%w1\n\t"                                           \
		"orc #0x80,ccr\n\t"                                        \
		"bld #" BIT ",@%3\n\t"                                     \
		OP " #" BIT ",@%3\n\t"                                     \
		"rotxl.l %0\n\t"                                           \
		"ldc %w1,ccr"                                               \
		: "=r"(retval),"=&r"(ccrsave)                              \
		: "0" (retval),"r" (a)                                     \
		: "memory");

#define H8300_GEN_TEST_BITOP_IMM(OP,BIT)			           \
	__asm__("bld #" BIT ",@%2\n\t"                                     \
		OP " #" BIT ",@%2\n\t"                                     \
		"rotxl.l %0\n\t"                                           \
		: "=r"(retval)                                             \
		: "0" (retval),"r" (a)                                     \
		: "memory");

#define H8300_GEN_TEST_BITOP_INT(FNNAME,OP)                                \
static __inline__ int FNNAME(int nr, volatile void * addr)                 \
{                                                                          \
	int retval = 0;                                                    \
        char ccrsave;                                                      \
	volatile unsigned char *a;                                         \
                                                                           \
	a = (volatile unsigned char *)addr + ((nr >> 3) ^ 3);              \
	if(__builtin_constant_p(nr) == 0) {                                \
		__asm__("stc ccr,%w1\n\t"                                  \
			"orc #0x80,ccr\n\t"                                \
			"btst %w4,@%3\n\t"                                 \
			OP " %w4,@%3\n\t"                                  \
			"beq 1f\n\t"                                       \
			"inc.l #1,%0\n"                                    \
			"1:\n\t"                                           \
			"ldc %w1,ccr"                                      \
			: "=r"(retval),"=&r"(ccrsave)                      \
			: "0" (retval),"r" (a),"r"(nr)                     \
			: "memory");                                       \
		return retval;                                             \
	} else {                                                           \
		switch(nr & 7) {                                           \
		case 0:                                                    \
			H8300_GEN_TEST_BITOP_IMM_INT(OP,"0");              \
			break;                                             \
		case 1:                                                    \
			H8300_GEN_TEST_BITOP_IMM_INT(OP,"1");              \
			break;                                             \
		case 2:                                                    \
			H8300_GEN_TEST_BITOP_IMM_INT(OP,"2");              \
			break;                                             \
		case 3:                                                    \
			H8300_GEN_TEST_BITOP_IMM_INT(OP,"3");              \
			break;                                             \
		case 4:                                                    \
			H8300_GEN_TEST_BITOP_IMM_INT(OP,"4");              \
			break;                                             \
		case 5:                                                    \
			H8300_GEN_TEST_BITOP_IMM_INT(OP,"5");              \
			break;                                             \
		case 6:                                                    \
			H8300_GEN_TEST_BITOP_IMM_INT(OP,"6");              \
			break;                                             \
		case 7:                                                    \
			H8300_GEN_TEST_BITOP_IMM_INT(OP,"7");              \
			break;                                             \
		}                                                          \
		return retval;                                             \
	}                                                                  \
}

#define H8300_GEN_TEST_BITOP(FNNAME,OP)                                    \
static __inline__  int FNNAME(int nr, volatile void * addr)                \
{                                                                          \
	int retval = 0;                                                    \
	volatile unsigned char *a;                                         \
                                                                           \
	a = (volatile unsigned char *)addr + ((nr >> 3) ^ 3);              \
	if(__builtin_constant_p(nr) == 0) {                                \
		__asm__("btst %w3,@%2\n\t"                                 \
			OP " %w3,@%2\n\t"                                  \
			"beq 1f\n\t"                                       \
			"inc.l #1,%0\n"                                    \
			"1:\n\t"                                           \
			: "=r"(retval)                                     \
			: "0" (retval),"r" (a),"r"(nr)                     \
			: "memory");                                       \
		return retval;                                             \
	} else {                                                           \
		switch(nr & 7) {                                           \
		case 0:                                                    \
			H8300_GEN_TEST_BITOP_IMM(OP,"0");                  \
			break;                                             \
		case 1:                                                    \
			H8300_GEN_TEST_BITOP_IMM(OP,"1");                  \
			break;                                             \
		case 2:                                                    \
			H8300_GEN_TEST_BITOP_IMM(OP,"2");                  \
			break;                                             \
		case 3:                                                    \
			H8300_GEN_TEST_BITOP_IMM(OP,"3");                  \
			break;                                             \
		case 4:                                                    \
			H8300_GEN_TEST_BITOP_IMM(OP,"4");                  \
			break;                                             \
		case 5:                                                    \
			H8300_GEN_TEST_BITOP_IMM(OP,"5");                  \
			break;                                             \
		case 6:                                                    \
			H8300_GEN_TEST_BITOP_IMM(OP,"6");                  \
			break;                                             \
		case 7:                                                    \
			H8300_GEN_TEST_BITOP_IMM(OP,"7");                  \
			break;                                             \
		}                                                          \
		return retval;                                             \
	}                                                                  \
}

H8300_GEN_TEST_BITOP_INT(test_and_set_bit,"bset")
H8300_GEN_TEST_BITOP_INT(test_and_clear_bit,"bclr")
H8300_GEN_TEST_BITOP_INT(test_and_change_bit,"bnot")
H8300_GEN_TEST_BITOP    (__test_and_set_bit,"bset")
H8300_GEN_TEST_BITOP    (__test_and_clear_bit,"bclr")
H8300_GEN_TEST_BITOP    (__test_and_change_bit,"bnot")

#define find_first_zero_bit(addr, size) \
        find_next_zero_bit((addr), (size), 0)

static __inline__ int find_next_zero_bit (void * addr, int size, int offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 5);
	unsigned long result = offset & ~31UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (32-offset);
		if (size < 32)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while (size & ~31UL) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += 32;
		size -= 32;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp |= ~0UL >> size;
found_middle:
	return result + ffz(tmp);
}

#define ffs(x) generic_ffs(x)

/*
 * hweightN: returns the hamming weight (i.e. the number
 * of bits set) of a N-bit word
 */

#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x) generic_hweight8(x)

static __inline__ int ext2_set_bit(int nr, volatile void * addr)
{
	int		mask, retval;
	unsigned long	flags;
	volatile unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	save_and_cli(flags);
	retval = (mask & *ADDR) != 0;
	*ADDR |= mask;
	restore_flags(flags);
	return retval;
}

static __inline__ int ext2_clear_bit(int nr, volatile void * addr)
{
	int		mask, retval;
	unsigned long	flags;
	volatile unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	save_and_cli(flags);
	retval = (mask & *ADDR) != 0;
	*ADDR &= ~mask;
	restore_flags(flags);
	return retval;
}

static __inline__ int ext2_test_bit(int nr, const volatile void * addr)
{
	int			mask;
	const volatile unsigned char	*ADDR = (const unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	return ((mask & *ADDR) != 0);
}

#define ext2_find_first_zero_bit(addr, size) \
        ext2_find_next_zero_bit((addr), (size), 0)

static __inline__ unsigned long ext2_find_next_zero_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 5);
	unsigned long result = offset & ~31UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if(offset) {
		/* We hold the little endian value in tmp, but then the
		 * shift is illegal. So we could keep a big endian value
		 * in tmp, like this:
		 *
		 * tmp = __swab32(*(p++));
		 * tmp |= ~0UL >> (32-offset);
		 *
		 * but this would decrease preformance, so we change the
		 * shift:
		 */
		tmp = *(p++);
		tmp |= __swab32(~0UL >> (32-offset));
		if(size < 32)
			goto found_first;
		if(~tmp)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while(size & ~31UL) {
		if(~(tmp = *(p++)))
			goto found_middle;
		result += 32;
		size -= 32;
	}
	if(!size)
		return result;
	tmp = *p;

found_first:
	/* tmp is little endian, so we would have to swab the shift,
	 * see above. But then we have to swab tmp below for ffz, so
	 * we might as well do this here.
	 */
	return result + ffz(__swab32(tmp) | (~0UL << size));
found_middle:
	return result + ffz(__swab32(tmp));
}

/* Bitmap functions for the minix filesystem.  */
#define minix_test_and_set_bit(nr,addr) test_and_set_bit(nr,addr)
#define minix_set_bit(nr,addr) set_bit(nr,addr)
#define minix_test_and_clear_bit(nr,addr) test_and_clear_bit(nr,addr)
#define minix_test_bit(nr,addr) test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size) find_first_zero_bit(addr,size)

#endif /* __KERNEL__ */

#endif /* _H8300_BITOPS_H */
