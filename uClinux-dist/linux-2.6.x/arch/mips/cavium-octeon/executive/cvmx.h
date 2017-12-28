/***********************license start***************
 * Author: Cavium Networks 
 * 
 * Contact: support@caviumnetworks.com 
 * This file is part of the OCTEON SDK
 * 
 * Copyright (c) 2003-2008 Cavium Networks 
 * 
 * This file is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License, Version 2, as published by 
 * the Free Software Foundation. 
 * 
 * This file is distributed in the hope that it will be useful, 
 * but AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or NONINFRINGEMENT. 
 * See the GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this file; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/. 
 * 
 * This file may also be available under a different license from Cavium. 
 * Contact Cavium Networks for more information
 ***********************license end**************************************/

/**
 * @file
 *
 * Main Octeon executive header file (This should be the second header
 * file included by an application).
 *
 */
#ifndef __CVMX_H__
#define __CVMX_H__

/* Control whether simple executive applications use 1-1 TLB mappings
** to access physical memory addresses.  This must be disabled to
** allow large programs that use more than the 0x10000000 - 0x20000000
** virtual address range.
*/
#ifndef CVMX_USE_1_TO_1_TLB_MAPPINGS
#define CVMX_USE_1_TO_1_TLB_MAPPINGS 1
#endif

#include "cvmx-platform.h"
#include "cvmx-asm.h"
#include "cvmx-packet.h"
#include "cvmx-warn.h"
#include "cvmx-sysinfo.h"

/* To have a global variable be shared among all cores,
 * declare with the CVMX_SHARED attribute.  Ex:
 * CVMX_SHARED int myglobal;
 * This will cause the variable to be placed in a special
 * section that the loader will map as shared for all cores
 * This is for data structures use by software ONLY,
 * as it is not 1-1 VA-PA mapped.
 */
#define CVMX_SHARED      __attribute__ ((cvmx_shared))

#define EXTERN_ASM extern

#ifndef CVMX_ENABLE_PARAMETER_CHECKING
#define CVMX_ENABLE_PARAMETER_CHECKING 1
#endif

#ifndef CVMX_ENABLE_DEBUG_PRINTS
#define CVMX_ENABLE_DEBUG_PRINTS 1
#endif

#if CVMX_ENABLE_DEBUG_PRINTS
#define cvmx_dprintf        printk
#else
#define cvmx_dprintf(...)   {}
#endif

#define CVMX_MAX_CORES          (16)
#define CVMX_CACHE_LINE_SIZE    (128)	/* In bytes */
#define CVMX_CACHE_LINE_MASK    (CVMX_CACHE_LINE_SIZE - 1)	/* In bytes */
#define CVMX_CACHE_LINE_ALIGNED __attribute__ ((aligned(CVMX_CACHE_LINE_SIZE)))
#define CAST64(v) ((long long)(long)(v))
#define CASTPTR(type, v) ((type *)(long)(v))

/* simprintf uses simulator tricks to speed up printouts.  The format
** and args are passed to the simulator and processed natively on the host.
** Simprintf is limited to 7 arguments, and they all must use %ll (long long)
** format specifiers to be displayed correctly.
*/
EXTERN_ASM void simprintf(const char *format, ...);

/**
 * This function performs some default initialization of the Octeon
 * executive.  It initializes the cvmx_bootmem memory allocator with
 * the list of physical memory provided by the bootloader, and creates
 * 1-1 TLB mappings for this memory.  This function should be called
 * on all cores that will use either the bootmem allocator or the 1-1
 * TLB mappings.  Applications which require a different configuration
 * can replace this function with a suitable application specific one.
 *
 * @return 0 on success
 *         -1 on failure
 */
int cvmx_user_app_init(void);

/* Returns processor ID, different Linux and simple exec versions
** provided in the cvmx-app-init*.c files */
static inline uint32_t cvmx_get_proc_id(void) __attribute__ ((pure));
static inline uint32_t cvmx_get_proc_id(void)
{
	uint32_t id;
	asm("mfc0 %0, $15,0" : "=r"(id));
	return id;
}

/* turn the variable name into a string */
#define CVMX_TMP_STR(x) CVMX_TMP_STR2(x)
#define CVMX_TMP_STR2(x) #x
/*
 * The macros cvmx_likely and cvmx_unlikely use the
 * __builtin_expect GCC operation to control branch
 * probabilities for a conditional. For example, an "if"
 * statement in the code that will almost always be
 * executed should be written as "if (cvmx_likely(...))".
 * If the "else" section of an if statement is more
 * probable, use "if (cvmx_unlikey(...))".
 */
#define cvmx_likely(x)      __builtin_expect(!!(x), 1)
#define cvmx_unlikely(x)    __builtin_expect(!!(x), 0)
/**
 * Builds a bit mask given the required size in bits.
 *
 * @param bits   Number of bits in the mask
 * @return The mask
 */ static inline uint64_t cvmx_build_mask(uint64_t bits)
{
	return ~((~0x0ull) << bits);
}

/**
 * Builds a memory address for I/O based on the Major and Sub DID.
 *
 * @param major_did 5 bit major did
 * @param sub_did   3 bit sub did
 * @return I/O base address
 */
static inline uint64_t cvmx_build_io_address(uint64_t major_did,
					     uint64_t sub_did)
{
	return (0x1ull << 48) | (major_did << 43) | (sub_did << 40);
}

/**
 * Perform mask and shift to place the supplied value into
 * the supplied bit rage.
 *
 * Example: cvmx_build_bits(39,24,value)
 * <pre>
 * 6       5       4       3       3       2       1
 * 3       5       7       9       1       3       5       7      0
 * +-------+-------+-------+-------+-------+-------+-------+------+
 * 000000000000000000000000___________value000000000000000000000000
 * </pre>
 *
 * @param high_bit Highest bit value can occupy (inclusive) 0-63
 * @param low_bit  Lowest bit value can occupy inclusive 0-high_bit
 * @param value    Value to use
 * @return Value masked and shifted
 */
static inline uint64_t cvmx_build_bits(uint64_t high_bit,
				       uint64_t low_bit, uint64_t value)
{
	return (value & cvmx_build_mask(high_bit - low_bit + 1)) << low_bit;
}

#ifndef TRUE
#define FALSE   0
#define TRUE    (!(FALSE))
#endif

typedef enum {
	CVMX_MIPS_SPACE_XKSEG = 3LL,
	CVMX_MIPS_SPACE_XKPHYS = 2LL,
	CVMX_MIPS_SPACE_XSSEG = 1LL,
	CVMX_MIPS_SPACE_XUSEG = 0LL
} cvmx_mips_space_t;

typedef enum {
	CVMX_MIPS_XKSEG_SPACE_KSEG0 = 0LL,
	CVMX_MIPS_XKSEG_SPACE_KSEG1 = 1LL,
	CVMX_MIPS_XKSEG_SPACE_SSEG = 2LL,
	CVMX_MIPS_XKSEG_SPACE_KSEG3 = 3LL
} cvmx_mips_xkseg_space_t;

/* decodes <14:13> of a kseg3 window address */
typedef enum {
	CVMX_ADD_WIN_SCR = 0L,
	/* see cvmx_add_win_dma_dec_t for further decode */
	CVMX_ADD_WIN_DMA = 1L,
	CVMX_ADD_WIN_UNUSED = 2L,
	CVMX_ADD_WIN_UNUSED2 = 3L
} cvmx_add_win_dec_t;

/* decode within DMA space */
typedef enum {
	/* add store data to the write buffer entry, allocating it if
	 * necessary */
	CVMX_ADD_WIN_DMA_ADD = 0L,
	/* send out the write buffer entry to DRAM */
	CVMX_ADD_WIN_DMA_SENDMEM = 1L,
	/* store data must be normal DRAM memory space address in this
	 * case send out the write buffer entry as an IOBDMA
	 * command */
	CVMX_ADD_WIN_DMA_SENDDMA = 2L,
	/* see CVMX_ADD_WIN_DMA_SEND_DEC for data contents */
	/* send out the write buffer entry as an IO write */
	CVMX_ADD_WIN_DMA_SENDIO = 3L,
	/* store data must be normal IO space address in this case */
	/* send out a single-tick command on the NCB bus */
	CVMX_ADD_WIN_DMA_SENDSINGLE = 4L,
	/* no write buffer data needed/used */
} cvmx_add_win_dma_dec_t;

/**
 *   Physical Address Decode
 *
 * Octeon-I HW never interprets this X (<39:36> reserved
 * for future expansion), software should set to 0.
 *
 *  - 0x0 XXX0 0000 0000 to      DRAM         Cached
 *  - 0x0 XXX0 0FFF FFFF
 *
 *  - 0x0 XXX0 1000 0000 to      Boot Bus     Uncached  (Converted to 0x1 00X0 1000 0000
 *  - 0x0 XXX0 1FFF FFFF         + EJTAG                           to 0x1 00X0 1FFF FFFF)
 *
 *  - 0x0 XXX0 2000 0000 to      DRAM         Cached
 *  - 0x0 XXXF FFFF FFFF
 *
 *  - 0x1 00X0 0000 0000 to      Boot Bus     Uncached
 *  - 0x1 00XF FFFF FFFF
 *
 *  - 0x1 01X0 0000 0000 to      Other NCB    Uncached
 *  - 0x1 FFXF FFFF FFFF         devices
 *
 * Decode of all Octeon addresses
 */
typedef union {

	uint64_t u64;

	struct {
		cvmx_mips_space_t R:2;
		uint64_t offset:62;
	} sva;	/* mapped or unmapped virtual address */

	struct {
		uint64_t zeroes:33;
		uint64_t offset:31;
	} suseg;	/* mapped USEG virtual addresses (typically) */

	struct {
		uint64_t ones:33;
		cvmx_mips_xkseg_space_t sp:2;
		uint64_t offset:29;
	} sxkseg;	/* mapped or unmapped virtual address */

	struct {
		cvmx_mips_space_t R:2;	/* CVMX_MIPS_SPACE_XKPHYS in this case */
		uint64_t cca:3;	/* ignored by octeon */
		uint64_t mbz:10;
		uint64_t pa:49;	/* physical address */
	} sxkphys;	/* physical address accessed through xkphys unmapped virtual address */

	struct {
		uint64_t mbz:15;
		uint64_t is_io:1;	/* if set, the address is uncached and resides on MCB bus */
		uint64_t did:8;	/* the hardware ignores this field when is_io==0, else device ID */
		uint64_t unaddr:4;	/* the hardware ignores <39:36> in Octeon I */
		uint64_t offset:36;
	} sphys;		/* physical address */

	struct {
		uint64_t zeroes:24;	/* techically, <47:40> are dont-cares */
		uint64_t unaddr:4;	/* the hardware ignores <39:36> in Octeon I */
		uint64_t offset:36;
	} smem;			/* physical mem address */

	struct {
		uint64_t mem_region:2;
		uint64_t mbz:13;
		uint64_t is_io:1;	/* 1 in this case */
		uint64_t did:8;	/* the hardware ignores this field when is_io==0, else device ID */
		uint64_t unaddr:4;	/* the hardware ignores <39:36> in Octeon I */
		uint64_t offset:36;
	} sio;			/* physical IO address */

	struct {
		uint64_t ones:49;
		cvmx_add_win_dec_t csrdec:2;	/* CVMX_ADD_WIN_SCR (0) in this case */
		uint64_t addr:13;
	} sscr;			/* scratchpad virtual address - accessed through a window at the end of kseg3 */

	/* there should only be stores to IOBDMA space, no loads */
	struct {
		uint64_t ones:49;
		cvmx_add_win_dec_t csrdec:2;	/* CVMX_ADD_WIN_DMA (1) in this case */
		uint64_t unused2:3;
		cvmx_add_win_dma_dec_t type:3;
		uint64_t addr:7;
	} sdma;			/* IOBDMA virtual address - accessed through a window at the end of kseg3 */

	struct {
		uint64_t didspace:24;
		uint64_t unused:40;
	} sfilldidspace;

} cvmx_addr_t;

/* These macros for used by 32 bit applications */

#define CVMX_MIPS32_SPACE_KSEG0 1l
#define CVMX_ADD_SEG32(segment, add)          (((int32_t)segment << 31) | (int32_t)(add))

/* Currently all IOs are performed using XKPHYS addressing. Linux uses the
    CvmMemCtl register to enable XKPHYS addressing to IO space from user mode.
    Future OSes may need to change the upper bits of IO addresses. The
    following define controls the upper two bits for all IO addresses generated
    by the simple executive library */
#define CVMX_IO_SEG CVMX_MIPS_SPACE_XKPHYS

/* These macros simplify the process of creating common IO addresses */
#define CVMX_ADD_SEG(segment, add)          ((((uint64_t)segment) << 62) | (add))
#ifndef CVMX_ADD_IO_SEG
#define CVMX_ADD_IO_SEG(add)                CVMX_ADD_SEG(CVMX_IO_SEG, (add))
#endif
#define CVMX_ADDR_DIDSPACE(did)             (((CVMX_IO_SEG) << 22) | ((1ULL) << 8) | (did))
#define CVMX_ADDR_DID(did)                  (CVMX_ADDR_DIDSPACE(did) << 40)
#define CVMX_FULL_DID(did, subdid)           (((did) << 3) | (subdid))

/* from include/ncb_rsl_id.v */
#define CVMX_OCT_DID_MIS 0ULL	/* misc stuff */
#define CVMX_OCT_DID_GMX0 1ULL
#define CVMX_OCT_DID_GMX1 2ULL
#define CVMX_OCT_DID_PCI 3ULL
#define CVMX_OCT_DID_KEY 4ULL
#define CVMX_OCT_DID_FPA 5ULL
#define CVMX_OCT_DID_DFA 6ULL
#define CVMX_OCT_DID_ZIP 7ULL
#define CVMX_OCT_DID_RNG 8ULL
#define CVMX_OCT_DID_IPD 9ULL
#define CVMX_OCT_DID_PKT 10ULL
#define CVMX_OCT_DID_TIM 11ULL
#define CVMX_OCT_DID_TAG 12ULL
/* the rest are not on the IO bus */
#define CVMX_OCT_DID_L2C 16ULL
#define CVMX_OCT_DID_LMC 17ULL
#define CVMX_OCT_DID_SPX0 18ULL
#define CVMX_OCT_DID_SPX1 19ULL
#define CVMX_OCT_DID_PIP 20ULL
#define CVMX_OCT_DID_ASX0 22ULL
#define CVMX_OCT_DID_ASX1 23ULL
#define CVMX_OCT_DID_IOB 30ULL

#define CVMX_OCT_DID_PKT_SEND       CVMX_FULL_DID(CVMX_OCT_DID_PKT, 2ULL)
#define CVMX_OCT_DID_TAG_SWTAG      CVMX_FULL_DID(CVMX_OCT_DID_TAG, 0ULL)
#define CVMX_OCT_DID_TAG_TAG1       CVMX_FULL_DID(CVMX_OCT_DID_TAG, 1ULL)
#define CVMX_OCT_DID_TAG_TAG2       CVMX_FULL_DID(CVMX_OCT_DID_TAG, 2ULL)
#define CVMX_OCT_DID_TAG_TAG3       CVMX_FULL_DID(CVMX_OCT_DID_TAG, 3ULL)
#define CVMX_OCT_DID_TAG_NULL_RD    CVMX_FULL_DID(CVMX_OCT_DID_TAG, 4ULL)
#define CVMX_OCT_DID_TAG_CSR        CVMX_FULL_DID(CVMX_OCT_DID_TAG, 7ULL)
#define CVMX_OCT_DID_FAU_FAI        CVMX_FULL_DID(CVMX_OCT_DID_IOB, 0ULL)
#define CVMX_OCT_DID_TIM_CSR        CVMX_FULL_DID(CVMX_OCT_DID_TIM, 0ULL)
#define CVMX_OCT_DID_KEY_RW         CVMX_FULL_DID(CVMX_OCT_DID_KEY, 0ULL)
#define CVMX_OCT_DID_PCI_6          CVMX_FULL_DID(CVMX_OCT_DID_PCI, 6ULL)
#define CVMX_OCT_DID_MIS_BOO        CVMX_FULL_DID(CVMX_OCT_DID_MIS, 0ULL)
#define CVMX_OCT_DID_PCI_RML        CVMX_FULL_DID(CVMX_OCT_DID_PCI, 0ULL)
#define CVMX_OCT_DID_IPD_CSR        CVMX_FULL_DID(CVMX_OCT_DID_IPD, 7ULL)
#define CVMX_OCT_DID_DFA_CSR        CVMX_FULL_DID(CVMX_OCT_DID_DFA, 7ULL)
#define CVMX_OCT_DID_MIS_CSR        CVMX_FULL_DID(CVMX_OCT_DID_MIS, 7ULL)
#define CVMX_OCT_DID_ZIP_CSR        CVMX_FULL_DID(CVMX_OCT_DID_ZIP, 0ULL)

/**
 * Convert a memory pointer (void*) into a hardware compatable
 * memory address (uint64_t). Octeon hardware widgets don't
 * understand logical addresses.
 *
 * @param ptr    C style memory pointer
 * @return Hardware physical address
 */
static inline uint64_t cvmx_ptr_to_phys(void *ptr)
{
	if (CVMX_ENABLE_PARAMETER_CHECKING)
		cvmx_warn_if(ptr == NULL,
			     "cvmx_ptr_to_phys() passed a NULL pointer\n");
	if (sizeof(void *) == 8) {
		/* We're running in 64 bit mode. Normally this means that we can use
		   40 bits of address space (the hardware limit). Unfortunately there
		   is one case were we need to limit this to 30 bits, sign extended
		   32 bit. Although these are 64 bits wide, only 30 bits can be used */
		if ((CAST64(ptr) >> 62) == 3)
			return CAST64(ptr) & cvmx_build_mask(30);
		else
			return CAST64(ptr) & cvmx_build_mask(40);
	} else {
		return (long)(ptr) & 0x1fffffff;
	}
}

/**
 * Convert a hardware physical address (uint64_t) into a
 * memory pointer (void *).
 *
 * @param physical_address
 *               Hardware physical address to memory
 * @return Pointer to memory
 */
static inline void *cvmx_phys_to_ptr(uint64_t physical_address)
{
	if (CVMX_ENABLE_PARAMETER_CHECKING)
		cvmx_warn_if(physical_address == 0,
			     "cvmx_phys_to_ptr() passed a zero address\n");
	if (sizeof(void *) == 8) {
		/* Just set the top bit, avoiding any TLB uglyness */
		return CASTPTR(void,
			       CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS,
					    physical_address));
	} else {
		return CASTPTR(void,
			       CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0,
					      physical_address));
	}
}

#include "cvmx-csr.h"

/* The following #if controls the definition of the macro
    CVMX_BUILD_WRITE64. This macro is used to build a store operation to
    a full 64bit address. With a 64bit ABI, this can be done with a simple
    pointer access. 32bit ABIs require more complicated assembly */
#if defined(CVMX_ABI_N64) || defined(CVMX_ABI_EABI)

/* We have a full 64bit ABI. Writing to a 64bit address can be done with
    a simple volatile pointer */
#define CVMX_BUILD_WRITE64(TYPE, ST)                                    \
static inline void cvmx_write64_##TYPE(uint64_t addr, TYPE##_t val)     \
{                                                                       \
    *CASTPTR(volatile TYPE##_t, addr) = val;                            \
}

#elif defined(CVMX_ABI_N32)

/* The N32 ABI passes all 64bit quantities in a single register, so it is
    possible to use the arguments directly. We have to use inline assembly
    for the actual store since a pointer would truncate the address */
#define CVMX_BUILD_WRITE64(TYPE, ST)                                    \
static inline void cvmx_write64_##TYPE(uint64_t addr, TYPE##_t val)     \
{                                                                       \
	asm volatile (ST " %[v], 0(%[c])" : : [v] "r" (val), [c] "r" (addr)); \
}

#elif defined(CVMX_ABI_O32)

#define CVMX_BUILD_WRITE64(TYPE, LT) extern void cvmx_write64_##TYPE(uint64_t csr_addr, TYPE##_t val);

#else

/* cvmx-abi.h didn't recognize the ABI. Force the compile to fail. */
#error Unsupported ABI

#endif

/* The following #if controls the definition of the macro
    CVMX_BUILD_READ64. This macro is used to build a load operation from
    a full 64bit address. With a 64bit ABI, this can be done with a simple
    pointer access. 32bit ABIs require more complicated assembly */
#if defined(CVMX_ABI_N64) || defined(CVMX_ABI_EABI)

/* We have a full 64bit ABI. Writing to a 64bit address can be done with
    a simple volatile pointer */
#define CVMX_BUILD_READ64(TYPE, LT)                                     \
static inline TYPE##_t cvmx_read64_##TYPE(uint64_t addr)                \
{                                                                       \
	return *CASTPTR(volatile TYPE##_t, addr);			\
}

#elif defined(CVMX_ABI_N32)

/* The N32 ABI passes all 64bit quantities in a single register, so it is
    possible to use the arguments directly. We have to use inline assembly
    for the actual store since a pointer would truncate the address */
#define CVMX_BUILD_READ64(TYPE, LT)                                     \
static inline TYPE##_t cvmx_read64_##TYPE(uint64_t addr)                \
{                                                                       \
	TYPE##_t val;							\
	asm volatile (LT " %[v], 0(%[c])" : [v] "=r" (val) : [c] "r" (addr)); \
	return val;							\
}

#elif defined(CVMX_ABI_O32)

#define CVMX_BUILD_READ64(TYPE, LT) extern TYPE##_t cvmx_read64_##TYPE(uint64_t csr_addr);

#else

/* cvmx-abi.h didn't recognize the ABI. Force the compile to fail. */
#error Unsupported ABI

#endif

/* The following defines 8 functions for writing to a 64bit address. Each
    takes two arguments, the address and the value to write.
    cvmx_write64_int64      cvmx_write64_uint64
    cvmx_write64_int32      cvmx_write64_uint32
    cvmx_write64_int16      cvmx_write64_uint16
    cvmx_write64_int8       cvmx_write64_uint8 */
CVMX_BUILD_WRITE64(int64, "sd");
CVMX_BUILD_WRITE64(int32, "sw");
CVMX_BUILD_WRITE64(int16, "sh");
CVMX_BUILD_WRITE64(int8, "sb");
CVMX_BUILD_WRITE64(uint64, "sd");
CVMX_BUILD_WRITE64(uint32, "sw");
CVMX_BUILD_WRITE64(uint16, "sh");
CVMX_BUILD_WRITE64(uint8, "sb");
#define cvmx_write64 cvmx_write64_uint64

/* The following defines 8 functions for reading from a 64bit address. Each
    takes the address as the only argument
    cvmx_read64_int64       cvmx_read64_uint64
    cvmx_read64_int32       cvmx_read64_uint32
    cvmx_read64_int16       cvmx_read64_uint16
    cvmx_read64_int8        cvmx_read64_uint8 */
CVMX_BUILD_READ64(int64, "ld");
CVMX_BUILD_READ64(int32, "lw");
CVMX_BUILD_READ64(int16, "lh");
CVMX_BUILD_READ64(int8, "lb");
CVMX_BUILD_READ64(uint64, "ld");
CVMX_BUILD_READ64(uint32, "lw");
CVMX_BUILD_READ64(uint16, "lhu");
CVMX_BUILD_READ64(uint8, "lbu");
#define cvmx_read64 cvmx_read64_uint64


static inline void cvmx_write_csr(uint64_t csr_addr, uint64_t val)
{
	cvmx_write64(csr_addr, val);

	/* Perform an immediate read after every write to an RSL register to force
	   the write to complete. It doesn't matter what RSL read we do, so we
	   choose CVMX_MIO_BOOT_BIST_STAT because it is fast and harmless */
	if ((csr_addr >> 40) == (0x800118))
		cvmx_read64(CVMX_MIO_BOOT_BIST_STAT);
}

static inline void cvmx_write_io(uint64_t io_addr, uint64_t val)
{
	cvmx_write64(io_addr, val);

}

static inline uint64_t cvmx_read_csr(uint64_t csr_addr)
{
	uint64_t val = cvmx_read64(csr_addr);
	return val;
}


static inline void cvmx_send_single(uint64_t data)
{
	const uint64_t CVMX_IOBDMA_SENDSINGLE = 0xffffffffffffa200ull;
	cvmx_write64(CVMX_IOBDMA_SENDSINGLE, data);
}

static inline void cvmx_read_csr_async(uint64_t scraddr, uint64_t csr_addr)
{
	union {
		uint64_t u64;
		struct {
			uint64_t scraddr:8;
			uint64_t len:8;
			uint64_t addr:48;
		} s;
	} addr;
	addr.u64 = csr_addr;
	addr.s.scraddr = scraddr >> 3;
	addr.s.len = 1;
	cvmx_send_single(addr.u64);
}

/* Return true if Octeon is CN38XX pass 1 */
static inline int cvmx_octeon_is_pass1(void)
{
#if OCTEON_IS_COMMON_BINARY()
	return 0;	/* Pass 1 isn't supported for common binaries */
#else
/* Now that we know we're built for a specific model, only check CN38XX */
#if OCTEON_IS_MODEL(OCTEON_CN38XX)
	return cvmx_get_proc_id() == OCTEON_CN38XX_PASS1;
#else
	return 0;	/* Built for non CN38XX chip, we're not CN38XX pass1 */
#endif
#endif
}

static inline unsigned int cvmx_get_core_num(void)
{
	unsigned int core_num;
	CVMX_RDHWRNV(core_num, 0);
	return core_num;
}

/**
 * Returns the number of bits set in the provided value.
 * Simple wrapper for POP instruction.
 *
 * @param val    32 bit value to count set bits in
 *
 * @return Number of bits set
 */
static inline uint32_t cvmx_pop(uint32_t val)
{
	uint32_t pop;
	CVMX_POP(pop, val);
	return pop;
}

/**
 * Returns the number of bits set in the provided value.
 * Simple wrapper for DPOP instruction.
 *
 * @param val    64 bit value to count set bits in
 *
 * @return Number of bits set
 */
static inline int cvmx_dpop(uint64_t val)
{
	int pop;
	CVMX_DPOP(pop, val);
	return pop;
}

/**
 * Provide current cycle counter as a return value
 *
 * @return current cycle counter
 */

#if defined(CVMX_ABI_O32)
static inline uint64_t cvmx_get_cycle(void)
{
	uint32_t tmp_low, tmp_hi;

	asm volatile ("   .set push                    \n"
		      "   .set mips64r2                \n"
		      "   .set noreorder               \n"
		      "   rdhwr %[tmpl], $31           \n"
		      "   dsrl  %[tmph], %[tmpl], 32   \n"
		      "   sll   %[tmpl], 0             \n"
		      "   sll   %[tmph], 0             \n"
		      "   .set pop                 \n"
		: [tmpl] "=&r"(tmp_low), [tmph] "=&r"(tmp_hi) :);

	return ((uint64_t) tmp_hi << 32) + tmp_low;
}
#else
static inline uint64_t cvmx_get_cycle(void)
{
	uint64_t cycle;
	CVMX_RDHWR(cycle, 31);
	return cycle;
}
#endif

/**
 * Reads a chip global cycle counter.  This counts CPU cycles since
 * chip reset.  The counter is 64 bit.
 * This register does not exist on CN38XX pass 1 silicion
 *
 * @return Global chip cycle count since chip reset.
 */
static inline uint64_t cvmx_get_cycle_global(void)
{
	if (cvmx_octeon_is_pass1())
		return 0;
	else
		return cvmx_read64(CVMX_IPD_CLK_COUNT);
}

/**
 * Wait for the specified number of cycle
 *
 * @param cycles
 */
static inline void cvmx_wait(uint64_t cycles)
{
	uint64_t done = cvmx_get_cycle() + cycles;

	while (cvmx_get_cycle() < done) {
		/* Spin */
	}
}

/**
 * Wait for the specified number of micro seconds
 *
 * @param usec   micro seconds to wait
 */
static inline void cvmx_wait_usec(uint64_t usec)
{
	uint64_t done =
	    cvmx_get_cycle() +
	    usec * cvmx_sysinfo_get()->cpu_clock_hz / 1000000;
	while (cvmx_get_cycle() < done) {
		/* Spin */
	}
}

/**
 * This macro spins on a field waiting for it to reach a value. It
 * is common in code to need to wait for a specific field in a CSR
 * to match a specific value. Conceptually this macro expands to:
 *
 * 1) read csr at "address" with a csr typedef of "type"
 * 2) Check if ("type".s."field" "op" "value")
 * 3) If #2 isn't true loop to #1 unless too much time has passed.
 */
#define CVMX_WAIT_FOR_FIELD64(address, type, field, op, value, timeout_usec)\
    (									\
{									\
	int result;							\
	do {								\
		uint64_t done = cvmx_get_cycle() + (uint64_t)timeout_usec * \
			cvmx_sysinfo_get()->cpu_clock_hz / 1000000;	\
		type c;							\
		while (1) {						\
			c.u64 = cvmx_read_csr(address);			\
			if ((c.s.field) op(value)) {			\
				result = 0;				\
				break;					\
			} else if (cvmx_get_cycle() > done) {		\
				result = -1;				\
				break;					\
			} else						\
				cvmx_wait(100);				\
		}							\
	} while (0);							\
	result;								\
})

/***************************************************************************/

/* Watchdog defines, to be moved.... */
typedef enum {
	CVMX_CIU_WDOG_MODE_OFF = 0,
	CVMX_CIU_WDOG_MODE_INT = 1,
	CVMX_CIU_WDOG_MODE_INT_NMI = 2,
	CVMX_CIU_WDOG_MODE_INT_NMI_SR = 3
} cvmx_ciu_wdog_mode_t;

static inline void cvmx_reset_octeon(void)
{
	cvmx_ciu_soft_rst_t ciu_soft_rst;
	ciu_soft_rst.u64 = 0;
	ciu_soft_rst.s.soft_rst = 1;
	cvmx_write_csr(CVMX_CIU_SOFT_RST, ciu_soft_rst.u64);
}

/* Return the number of cores available in the chip */
static inline uint32_t cvmx_octeon_num_cores(void)
{
	uint32_t ciu_fuse = (uint32_t) cvmx_read_csr(CVMX_CIU_FUSE) & 0xffff;
	return cvmx_pop(ciu_fuse);
}

/**
 * Read a byte of fuse data
 * @param byte_addr   address to read
 *
 * @return fuse value: 0 or 1
 */
static uint8_t cvmx_fuse_read_byte(int byte_addr)
{
	cvmx_mio_fus_rcmd_t read_cmd;

	read_cmd.u64 = 0;
	read_cmd.s.addr = byte_addr;
	read_cmd.s.pend = 1;
	cvmx_write_csr(CVMX_MIO_FUS_RCMD, read_cmd.u64);
	while ((read_cmd.u64 = cvmx_read_csr(CVMX_MIO_FUS_RCMD))
	       && read_cmd.s.pend)
		;
	return read_cmd.s.dat;
}

/**
 * Read a single fuse bit
 *
 * @param fuse   Fuse number (0-1024)
 *
 * @return fuse value: 0 or 1
 */
static inline int cvmx_fuse_read(int fuse)
{
	return (cvmx_fuse_read_byte(fuse >> 3) >> (fuse & 0x7)) & 1;
}

static inline int cvmx_octeon_model_CN36XX(void)
{
	return OCTEON_IS_MODEL(OCTEON_CN38XX)
		&& !cvmx_octeon_is_pass1()
		&& cvmx_fuse_read(264);
}

static inline int cvmx_octeon_zip_present(void)
{
	return octeon_has_feature(OCTEON_FEATURE_ZIP);
}

static inline int cvmx_octeon_dfa_present(void)
{
	if (!OCTEON_IS_MODEL(OCTEON_CN38XX)
	    && !OCTEON_IS_MODEL(OCTEON_CN31XX)
	    && !OCTEON_IS_MODEL(OCTEON_CN58XX))
		return 0;
	else if (OCTEON_IS_MODEL(OCTEON_CN3020))
		return 0;
	else if (cvmx_octeon_is_pass1())
		return 1;
	else
		return !cvmx_fuse_read(120);
}

static inline int cvmx_octeon_crypto_present(void)
{
	return octeon_has_feature(OCTEON_FEATURE_CRYPTO);
}

#endif /*  __CVMX_H__  */
