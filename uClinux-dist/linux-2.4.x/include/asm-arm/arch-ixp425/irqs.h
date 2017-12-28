/*
 * include/asm-arm/arch-ixp425/irqs.h 
 *
 * IRQ definitions for IXP425 based systems
 *
 * Copyright (C) 2002 Intel Corporation.
 * Copyright (C) 2003 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _ARCH_IXP425_IRQS_H_
#define _ARCH_IXP425_IRQS_H_

#ifdef CONFIG_CPU_IXP46X
#define NR_IRQS				64
#else
#define NR_IRQS				32
#endif

#define IRQ_IXP425_NPEA		0
#define IRQ_IXP425_NPEB		1
#define IRQ_IXP425_NPEC		2
#define IRQ_IXP425_QM1		3
#define IRQ_IXP425_QM2		4
#define IRQ_IXP425_TIMER1	5
#define IRQ_IXP425_GPIO0	6
#define IRQ_IXP425_GPIO1	7
#define IRQ_IXP425_PCI_INT	8
#define IRQ_IXP425_PCI_DMA1	9
#define IRQ_IXP425_PCI_DMA2	10
#define IRQ_IXP425_TIMER2	11
#define IRQ_IXP425_USB		12
#define IRQ_IXP425_UART2	13
#define IRQ_IXP425_TIMESTAMP	14
#define IRQ_IXP425_UART1	15
#define IRQ_IXP425_WDOG		16
#define IRQ_IXP425_AHB_PMU	17
#define IRQ_IXP425_XSCALE_PMU	18
#define IRQ_IXP425_GPIO2	19
#define IRQ_IXP425_GPIO3	20
#define IRQ_IXP425_GPIO4	21
#define IRQ_IXP425_GPIO5	22
#define IRQ_IXP425_GPIO6	23
#define IRQ_IXP425_GPIO7	24
#define IRQ_IXP425_GPIO8	25
#define IRQ_IXP425_GPIO9	26
#define IRQ_IXP425_GPIO10	27
#define IRQ_IXP425_GPIO11	28
#define IRQ_IXP425_GPIO12	29
#define IRQ_IXP425_SW_INT1	30
#define IRQ_IXP425_SW_INT2	31

#ifdef CONFIG_CPU_IXP46X
#define IRQ_IXP46X_USB_HOST         (32)
#define IRQ_IXP46X_I2C              (33)
#define IRQ_IXP46X_SSP              (34)
#define IRQ_IXP46X_TSYNC            (35)
#define IRQ_IXP46X_EAU_DONE         (36)
#define IRQ_IXP46X_SHA_HASHING_DONE (37)
#define IRQ_IXP46X_RSVD_38          (38)
#define IRQ_IXP46X_RSVD_39          (39)
#define IRQ_IXP46X_RSVD_40          (40)
#define IRQ_IXP46X_RSVD_41          (41)
#define IRQ_IXP46X_RSVD_42          (42)
#define IRQ_IXP46X_RSVD_43          (43)
#define IRQ_IXP46X_RSVD_44          (44)
#define IRQ_IXP46X_RSVD_45          (45)
#define IRQ_IXP46X_RSVD_46          (46)
#define IRQ_IXP46X_RSVD_47          (47)
#define IRQ_IXP46X_RSVD_48          (48)
#define IRQ_IXP46X_RSVD_49          (49)
#define IRQ_IXP46X_RSVD_50          (50)
#define IRQ_IXP46X_RSVD_51          (51)
#define IRQ_IXP46X_RSVD_52          (52)
#define IRQ_IXP46X_RSVD_53          (53)
#define IRQ_IXP46X_RSVD_54          (54)
#define IRQ_IXP46X_RSVD_55          (55)
#define IRQ_IXP46X_RSVD_56          (56)
#define IRQ_IXP46X_RSVD_57          (57)
#define IRQ_IXP46X_SWCP             (58)
#define IRQ_IXP46X_RSVD_59          (59)
#define IRQ_IXP46X_AQM              (60)
#define IRQ_IXP46X_MCU              (61)
#define IRQ_IXP46X_EBC              (62)
#define IRQ_IXP46X_RSVD_63          (63)
#endif

#define IRQ_USB			(IRQ_IXP425_USB)
#define	XSCALE_PMU_IRQ		(IRQ_IXP425_XSCALE_PMU)

/*
 * IXDP425 Board IRQs
 */
#define	IRQ_IXDP425_PCI_INTA	IRQ_IXP425_GPIO11
#define	IRQ_IXDP425_PCI_INTB	IRQ_IXP425_GPIO10
#define	IRQ_IXDP425_PCI_INTC	IRQ_IXP425_GPIO9
#define	IRQ_IXDP425_PCI_INTD	IRQ_IXP425_GPIO8

/*
 * PrPMC1100 Board IRQs
 */
#define	IRQ_PRPMC1100_PCI_INTA	IRQ_IXP425_GPIO11
#define	IRQ_PRPMC1100_PCI_INTB	IRQ_IXP425_GPIO10
#define	IRQ_PRPMC1100_PCI_INTC	IRQ_IXP425_GPIO9
#define	IRQ_PRPMC1100_PCI_INTD	IRQ_IXP425_GPIO8

/*
 * ADI Coyote Board IRQs
 */
#define IRQ_COYOTE_PCI_INTA	IRQ_IXP425_GPIO11
#define IRQ_COYOTE_PCI_INTB	IRQ_IXP425_GPIO6

/*
 * SnapGear ESS710 IRQs
 */
#define IRQ_ESS710_PCI_INTA	IRQ_IXP425_GPIO6
#define IRQ_ESS710_PCI_INTB	IRQ_IXP425_GPIO7
#define IRQ_ESS710_PCI_INTC	IRQ_IXP425_GPIO8

/*
 * Secure Computing SG590 Board IRQs
 */
#define IRQ_SG590_PCI_INTA	IRQ_IXP425_GPIO8

/*
 * Secure Computing SG720 Board IRQs
 */

#define IRQ_SG720_PCI_INTA	IRQ_IXP425_GPIO8
#define IRQ_SG720_PCI_INTB	IRQ_IXP425_GPIO9

/*
 * SnapGear SG565 Board IRQs
 */
#define IRQ_SG565_PCI_INTA	IRQ_IXP425_GPIO8

#define IRQ_SG8100_PCI_INTA	IRQ_IXP425_GPIO8

/*
 * Intel IXP425 Montejade demonstration platform
 */
#define IRQ_MONTEJADE_PCI_INTA	IRQ_IXP425_GPIO6
#define IRQ_MONTEJADE_PCI_INTB	IRQ_IXP425_GPIO7

#endif
