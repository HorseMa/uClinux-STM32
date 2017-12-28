/*
 * linux/include/asm-arm/arch-oki/hardware.h
 *
 * (c) 2004 Simtec Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
*/

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#ifndef __ASSEMBLY__
extern int oki_ml67500x;
extern int oki_ml67400x;

extern int oki_hclk;
extern int oki_cclk;
#endif

#ifdef CONFIG_CPU_OKIM67500X
/*
 ******************* OKI ML67500X ********************
 */

#define ARM_CLK	60000000

#define M67X_UART_BASE		(0xB7B00000)

#define HARD_RESET_NOW()

#define HW_M67X_TIMER_INIT(timer)	/* no PMC */


#elif CONFIG_CPU_OKIM67400X
/*
 ******************* OKI ML67400X ********************
 */

#define ARM_CLK	25000000

#define M67X_UART_BASE		(0xB7B00000)

#define HARD_RESET_NOW()

#define HW_M67X_TIMER_INIT(timer)	/* no PMC */

#else
  #error "Configuration error: No CPU defined"
#endif


/*
 ******************* COMMON PART ********************
 */

/* 7.2.1 */

#define OKI_BCKCTL        (0xB7000004)
#define OKI_CLKSTP        (0xB8000004)
#define OKI_CGBCNT0       (0xB8000008)
#define OKI_CKWT          (0xB800000C)

#define OKI_CLKGR_H(x)    (1 << ((x) & 7))
#define OKI_CLKGR_C(x)    (1 << (((x) >> 4) & 7))

/* 11.2.1 */
#define OKI_BWC           (0x78100000)

#define OKI_IRQREG(x)     (0x78000000 + (x))
#define OKI_IRQREG2(x)    (0x7BF00000 + (x))

#define OKI_IRQ             OKI_IRQREG(0x00)    /* 0x78000000 */

/* 8.4.2 */
#define OKI_IRQS            OKI_IRQREG(0x04)    /* 0x78000004 */

/* 8.4.3 */
#define OKI_FIQ             OKI_IRQREG(0x08)    /* 0x78000008 */
#define OKI_FIQ_FIQ                   (0x01)

/* 8.4.4 */
#define OKI_FIQRAW          OKI_IRQREG(0x0C)    /* 0x7800000C */
#define OKI_FIQRAW_FIQRAW             (0x01)

/* 8.4.5 */
#define OKI_FIQEN           OKI_IRQREG(0x10)    /* 0x78000010 */
#define OKI_FIQEN_FIQEN               (0x01)

/* 8.4.6 */
#define OKI_IRN             OKI_IRQREG(0x14)    /* 0x78000014 */
#define OKI_IRN_MASK                  (0x3f)

/* 8.4.7 */
#define OKI_CIL             OKI_IRQREG(0x18)    /* 0x78000018 */
#define OKI_CIL_GET(x)      (((x) >> 1) & 0x3f)

/* 8.4.8 */
#define OKI_ILC0            OKI_IRQREG(0x20)    /* 0x78000020 */

#define OKI_ILC0_MASK                 (0x07)
#define OKI_ILC0_ILR0_SHIFT           (0)
#define OKI_ILC0_ILR1_SHIFT           (4)
#define OKI_ILC0_ILR4_SHIFT           (16)
#define OKI_ILC0_ILR6_SHIFT           (24)

/* 8.4.9 */
#define OKI_ILC1            OKI_IRQREG(0x24)    /* 0x78000024 */

#define OKI_ILC1_MASK                 (0x07)
#define OKI_ILC1_ILR8_SHIFT           (0)
#define OKI_ILC1_ILR9_SHIFT           (4)
#define OKI_ILC1_ILR10_SHIFT          (8)
#define OKI_ILC1_ILR11_SHIFT          (12)
#define OKI_ILC1_ILR12_SHIFT          (16)
#define OKI_ILC1_ILR13_SHIFT          (20)
#define OKI_ILC1_ILR14_SHIFT          (24)
#define OKI_ILC1_ILR15_SHIFT          (28)

/* 8.4.10 */
#define OKI_CILCL           OKI_IRQREG(0x28)    /* 0x78000028 */
#define OKI_CILE            OKI_IRQREG(0x2C)    /* 0x7800002C */

/* these next bits seem to be fore IRQs 31..16 */

#define OKI_IRCL            OKI_IRQREG2(0x04)   /* 0x7BF00004 */
#define OKI_IRQA            OKI_IRQREG2(0x10)   /* 0x7BF00010 */
#define OKI_IDM             OKI_IRQREG2(0x14)   /* 0x7BF00014 */
#define OKI_ILC             OKI_IRQREG2(0x18)   /* 0x7BF00018 */

#endif  /* _ASM_ARCH_HARDWARE_H */
