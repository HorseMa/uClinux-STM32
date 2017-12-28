/*
 *  linux/arch/armnommu/mach-isl3893/mpu.S
 *
 */

#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/memory.h>

/*
 * ARM9 MPU area definitions
 */

#define Size_4KB	(0x0b << 1)
#define Size_8KB	(0x0c << 1)
#define Size_16KB	(0x0d << 1)
#define Size_32KB	(0x0e << 1)
#define Size_64KB	(0x0f << 1)
#define Size_128KB	(0x10 << 1)
#define Size_256KB	(0x11 << 1)
#define Size_512KB	(0x12 << 1)
#define Size_1MB	(0x13 << 1)
#define Size_2MB	(0x14 << 1)
#define Size_4MB	(0x15 << 1)
#define Size_8MB	(0x16 << 1)
#define Size_16MB	(0x17 << 1)
#define Size_32MB	(0x18 << 1)
#define Size_64MB	(0x19 << 1)
#define Size_128MB	(0x1a << 1)
#define Size_256MB	(0x1b << 1)
#define Size_512MB	(0x1c << 1)
#define Size_1GB	(0x1d << 1)
#define Size_2GB	(0x1e << 1)
#define Size_4GB	(0x1f << 1)

#define Area_disable	0
#define Area_enable	1

/* ARM946 (Extended) MPU Access Permissions
 * Coding: MPU_PERM_<priviliged>_<user>
 *	NA: No Access
 *	RO: Read Only
 *	RW: Read/Write
 */
#define	MPU_PERM_NA_NA	0x0
#define	MPU_PERM_RW_NA	0x1
#define	MPU_PERM_RW_RO	0x2
#define	MPU_PERM_RW_RW	0x3
#define	MPU_PERM_RO_NA	0x5
#define	MPU_PERM_RO_RO	0x6

.section ".text.init",#alloc,#execinstr
		.type	init_mpu_sections, #function 

/*
 * Configure memory protection unit and the caches
 *
 * Area 4, 5, 6 and 7 are used for memory protection.
 * If memory protection is disabled, we use area 4 to
 * give full access to the entire RAM area.
 *
 * Trashes r0 and r1
 */
ENTRY(init_mpu_sections)

@ First disable all areas. We can then set the access permissions freely.
	ldr		r0, =0x00000000 + Size_4GB + Area_disable
	mcr     	p15, 0, r0, c6, c4, 0	@ area 4
	mcr     	p15, 0, r0, c6, c5, 0	@ area 5
	mcr     	p15, 0, r0, c6, c6, 0	@ area 6
	mcr     	p15, 0, r0, c6, c7, 0	@ area 7


	mrc		p15, 0, r0, c5, c0, 2	@ read data extended access permissions
#ifdef CONFIG_ARM9_MPU
						@ Area 4 (read only) is used for text mapping
	ldr		r1, =(MPU_PERM_RW_RO << 16) + (MPU_PERM_RW_RW << 20) + (MPU_PERM_RW_RW << 24) + (MPU_PERM_RW_RW << 28)
#else /* CONFIG_ARM9_MPU */
	ldr		r1, =(MPU_PERM_RW_RW << 16)	@ Full access for area 4
#endif
	orr		r0, r0, r1
#ifdef ISL3893_NO_MVC
	/* This is only when the MPU areas are initialized by the bootrom.
	 * We need to make the "All Memory" area (area 0) as Not Accessible for User Space
	 */
	and		r0, r0, #0xfffffff0
	orr		r0, r0, #MPU_PERM_RW_NA
#endif
	mcr		p15, 0, r0, c5, c0, 2	@ set data extended access permissions


	mrc		p15, 0, r0, c5, c0, 3	@ read instruction extended access permissions
#ifdef CONFIG_ARM9_MPU
						@ Full access for areas 4, 5, 6, 7
	ldr		r1, =(MPU_PERM_RW_RW << 16) + (MPU_PERM_RW_RW << 20) + (MPU_PERM_RW_RW << 24) + (MPU_PERM_RW_RW << 28)
#else
	ldr		r1, =(MPU_PERM_RW_RW << 16)	@ Full access for area 4
#endif
	orr		r0, r0, r1
#ifdef ISL3893_NO_MVC
	/* This is only when the MPU areas are initialized by the bootrom.
	 * We need to make the "All Memory" area (area 0) as Not Accessible for User Space
	 */
	and		r0, r0, #0xfffffff0
	orr		r0, r0, #MPU_PERM_RW_NA
#endif
	mcr		p15, 0, r0, c5, c0, 3	@ set instruction extended access permissions

.ltorg


/* If the MPU is enabled, we leave all areas disabled. They are set when they are needed
 * for memory protection. If the MPU is disabled, we use area 4 for the entire RAM and leave
 * the rest disabled.
 */

#ifndef CONFIG_ARM9_MPU
	mov		r0, #PHYS_OFFSET
	add		r0, r0, #Size_16MB + Area_enable

	mcr     	p15, 0, r0, c6, c4, 0	@ area 4
#endif

/*
 * define cacheable data areas
 */
	mrc		p15, 0, r0, c2, c0, 0	                @ read cacheable data settings
	orr		r0, r0, #(1 << 4) + (1 << 5) + (1 << 6) + (1 << 7)	@ enable for area 4, 5, 6, 7
	mcr		p15, 0, r0, c2, c0, 0	                @ cacheable data


/*
 * define bufferable data areas
 */
	mrc		p15, 0, r0, c3, c0, 0                   @ read bufferable data settings
	and		r0, r0, #0xffffff0f			@ disable for area 5, 6, 7
	mcr		p15, 0, r0, c3, c0, 0                   @ bufferable data
 

/*
 * define cacheable code areas
 */
	mrc		p15, 0, r0, c2, c0, 1	                @ read cacheable code settings
	orr		r0, r0, #(1 << 4) + (1 << 5) + (1 << 6) + (1 << 7)	@ enable for area 4, 5, 6, 7
	mcr		p15, 0, r0, c2, c0, 1	                @ cacheable code 

	mov		pc, lr
