/* $Header$
 *
 * arch/arm/mm/mm-isl3893.c
 *
 * Extra MPU routines for the ISL3893 architecture
 *
 * Copyright (C) 2002 Intersil Americas Inc.
 */


#include <linux/config.h>
#include <linux/mman.h>
#include <asm/mmu.h>
#include <asm/uaccess.h>

#ifdef CONFIG_ARM9_MPU
void print_mpu_areas( void )
{
	int i;
	unsigned long mpu_regs[8];
	unsigned long mpu_dperm;
	unsigned long mpu_iperm;

	/* Read MPU area definitions */
	__asm__ __volatile__ ( "mrc p15, 0, %0, c6, c0, 0 " : "=r" (mpu_regs[0]) :);
	__asm__ __volatile__ ( "mrc p15, 0, %0, c6, c1, 0 " : "=r" (mpu_regs[1]) :);
	__asm__ __volatile__ ( "mrc p15, 0, %0, c6, c2, 0 " : "=r" (mpu_regs[2]) :);
	__asm__ __volatile__ ( "mrc p15, 0, %0, c6, c3, 0 " : "=r" (mpu_regs[3]) :);
	__asm__ __volatile__ ( "mrc p15, 0, %0, c6, c4, 0 " : "=r" (mpu_regs[4]) :);
	__asm__ __volatile__ ( "mrc p15, 0, %0, c6, c5, 0 " : "=r" (mpu_regs[5]) :);
	__asm__ __volatile__ ( "mrc p15, 0, %0, c6, c6, 0 " : "=r" (mpu_regs[6]) :);
	__asm__ __volatile__ ( "mrc p15, 0, %0, c6, c7, 0 " : "=r" (mpu_regs[7]) :);

	/* Read MPU Access Permissions */
	__asm__ __volatile__ ( "mrc p15, 0, %0, c5, c0, 2 " : "=r" (mpu_dperm) :); // Data
	__asm__ __volatile__ ( "mrc p15, 0, %0, c5, c0, 3 " : "=r" (mpu_iperm) :); // Instr

	printk("MPU register contents:\n");
	for (i = 0; i < 8; i++) {
		printk("%d: Area (0x%.8lx-0x%.8lx) En=%d, DataPerm=%lx, InstrPerm=%lx\n", i,
			ARM_MPU_AREA_START(mpu_regs[i]),
			ARM_MPU_AREA_END(mpu_regs[i]),
			ARM_MPU_REGION_ENABLED(mpu_regs[i]),
			(mpu_dperm >> (i * 4)) & 0xf,
			(mpu_iperm >> (i * 4)) & 0xf);
	}
}
#endif /* CONFIG_ARM9_MPU */
