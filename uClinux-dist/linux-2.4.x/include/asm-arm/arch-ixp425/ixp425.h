/*
 * include/asm-arm/arch-ixp425/ixp425.h 
 *
 * Register definitions for IXP425 
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H__
#error "Do not include this directly, please #include <asm/hardware.h>"
#endif

#ifndef _ASM_ARM_IXP425_H_
#define _ASM_ARM_IXP425_H_

#ifndef BIT    
#define BIT(bit)	(1 << (bit))
#endif


/*
 * 
 * IXP425 Memory map:
 *
 * Phy		Phy Size	Map Size	Virt		Description
 * =========================================================================
 *
 * 0x00000000	0x10000000					SDRAM 1 
 *
 * 0x10000000	0x10000000					SDRAM 2
 *
 * 0x20000000	0x10000000					SDRAM 3
 *
 * 0x30000000	0x10000000					SDRAM 4 
 *
 * The above four are aliases to the same memory location  (0x00000000)
 *
 * 0x48000000	 0x4000000					PCI Memory
 *
 * 0x50000000	0x10000000			Not Mapped	EXP BUS
 *
 * 0x6000000	0x00004000	    0x4000	0xFFFEB000	QMgr
 *
 * 0xC0000000	     0x100   	    0x1000	0xFFFDD000	PCI CFG 
 *
 * 0xC4000000	     0x100          0x1000	0xFFFDE000	EXP CFG 
 *
 * 0xC8000000	    0xC000          0xC000	0xFFFDF000	PERIPHERAL
 *
 * 0xCC000000	     0x100   	    0x1000	Not Mapped	SDRAM CFG 
 */


/*
 * PCI Configuration space
 */
#define IXP425_PCI_CFG_BASE_PHYS	(0xC0000000)
#define IXP425_PCI_CFG_BASE_VIRT	(0xFF013000)
#define IXP425_PCI_CFG_REGION_SIZE	(0x00001000)

/*
 * Expansion BUS Configuration registers
 */
#define IXP425_EXP_CFG_BASE_PHYS	(0xC4000000)
#define IXP425_EXP_CFG_BASE_VIRT	(0xFF014000)
#define IXP425_EXP_CFG_REGION_SIZE	(0x00001000)

/*
 * Peripheral space
 */
#define IXP425_PERIPHERAL_BASE_PHYS	(0xC8000000)
#define IXP425_PERIPHERAL_BASE_VIRT	(0xFF000000)
#define IXP425_PERIPHERAL_REGION_SIZE	(0x00013000)

/* 
 * Q Manager space .. not static mapped
 */
#define IXP425_QMGR_BASE_PHYS		(0x60000000)
#define IXP425_QMGR_BASE_VIRT		(0xFF015000)
#define IXP425_QMGR_REGION_SIZE		(0x00004000)

/*
 * Expansion BUS
 *
 * Expansion Bus 'lives' at either base1 or base 2 depending on the value of
 * Exp Bus config registers:
 *
 * Setting bit 31 of IXP425_EXP_CFG0 puts SDRAM at zero,
 * and The expansion bus to IXP425_EXP_BUS_BASE2
 */
#define IXP425_EXP_BUS_BASE1_PHYS	(0x00000000)
#define IXP425_EXP_BUS_BASE2_PHYS	(0x50000000)
#define IXP425_EXP_BUS_BASE2_VIRT	(0xF0000000)

#define IXP425_EXP_BUS_BASE_PHYS	IXP425_EXP_BUS_BASE2_PHYS
#define IXP425_EXP_BUS_BASE_VIRT	IXP425_EXP_BUS_BASE2_VIRT

#define IXP425_EXP_BUS_REGION_SIZE	(0x08000000)
#define IXP425_EXP_BUS_CSX_REGION_SIZE	(0x01000000)

#define IXP425_EXP_BUS_CS0_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x00000000)
#define IXP425_EXP_BUS_CS1_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x01000000)
#define IXP425_EXP_BUS_CS2_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x02000000)
#define IXP425_EXP_BUS_CS3_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x03000000)
#define IXP425_EXP_BUS_CS4_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x04000000)
#define IXP425_EXP_BUS_CS5_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x05000000)
#define IXP425_EXP_BUS_CS6_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x06000000)
#define IXP425_EXP_BUS_CS7_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x07000000)

#define IXP425_EXP_BUS_CS0_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x00000000)
#define IXP425_EXP_BUS_CS1_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x01000000)
#define IXP425_EXP_BUS_CS2_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x02000000)
#define IXP425_EXP_BUS_CS3_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x03000000)
#define IXP425_EXP_BUS_CS4_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x04000000)
#define IXP425_EXP_BUS_CS5_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x05000000)
#define IXP425_EXP_BUS_CS6_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x06000000)
#define IXP425_EXP_BUS_CS7_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x07000000)

#define IXP425_FLASH_WRITABLE	(0x2)
#define IXP425_FLASH_DEFAULT	(0xbcd23c40)
#define IXP425_FLASH_WRITE	(0xbcd23c42)


#define IXP425_EXP_CS0_OFFSET	0x00
#define IXP425_EXP_CS1_OFFSET   0x04
#define IXP425_EXP_CS2_OFFSET   0x08
#define IXP425_EXP_CS3_OFFSET   0x0C
#define IXP425_EXP_CS4_OFFSET   0x10
#define IXP425_EXP_CS5_OFFSET   0x14
#define IXP425_EXP_CS6_OFFSET   0x18
#define IXP425_EXP_CS7_OFFSET   0x1C
#define IXP425_EXP_CFG0_OFFSET	0x20
#define IXP425_EXP_CFG1_OFFSET	0x24
#define IXP425_EXP_CFG2_OFFSET	0x28
#define IXP425_EXP_CFG3_OFFSET	0x2C

/*
 * Expansion Bus Controller registers.
 */
#define IXP425_EXP_REG(x) ((volatile u32 *)(IXP425_EXP_CFG_BASE_VIRT+(x)))

#define IXP425_EXP_CS0      IXP425_EXP_REG(IXP425_EXP_CS0_OFFSET)
#define IXP425_EXP_CS1      IXP425_EXP_REG(IXP425_EXP_CS1_OFFSET)
#define IXP425_EXP_CS2      IXP425_EXP_REG(IXP425_EXP_CS2_OFFSET) 
#define IXP425_EXP_CS3      IXP425_EXP_REG(IXP425_EXP_CS3_OFFSET)
#define IXP425_EXP_CS4      IXP425_EXP_REG(IXP425_EXP_CS4_OFFSET)
#define IXP425_EXP_CS5      IXP425_EXP_REG(IXP425_EXP_CS5_OFFSET)
#define IXP425_EXP_CS6      IXP425_EXP_REG(IXP425_EXP_CS6_OFFSET)     
#define IXP425_EXP_CS7      IXP425_EXP_REG(IXP425_EXP_CS7_OFFSET)

#define IXP425_EXP_CFG0     IXP425_EXP_REG(IXP425_EXP_CFG0_OFFSET) 
#define IXP425_EXP_CFG1     IXP425_EXP_REG(IXP425_EXP_CFG1_OFFSET) 
#define IXP425_EXP_CFG2     IXP425_EXP_REG(IXP425_EXP_CFG2_OFFSET) 
#define IXP425_EXP_CFG3     IXP425_EXP_REG(IXP425_EXP_CFG3_OFFSET)


/*
 * Peripheral Space Registers 
 */
#define IXP425_UART1_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x0000)
#define IXP425_UART2_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x1000)
#define IXP425_PMU_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x2000)
#define IXP425_INTC_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x3000)
#define IXP425_GPIO_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x4000)
#define IXP425_TIMER_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x5000)
#define IXP425_NPEA_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x6000)
#define IXP425_NPEB_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x7000)
#define IXP425_NPEC_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x8000)
#define IXP425_EthA_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x9000)
#define IXP425_EthB_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0xA000)
#define IXP425_USB_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0xB000)

#define IXP425_UART1_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x0000)
#define IXP425_UART2_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x1000)
#define IXP425_PMU_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x2000)
#define IXP425_INTC_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x3000)
#define IXP425_GPIO_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x4000)
#define IXP425_TIMER_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x5000)
#define IXP425_NPEA_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x6000)
#define IXP425_NPEB_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x7000)
#define IXP425_NPEC_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x8000)
#define IXP425_EthA_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x9000)
#define IXP425_EthB_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0xA000)
#define IXP425_USB_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0xB000)


/* 
 * UART Register Definitions , Offsets only as there are 2 UARTS.
 *   IXP425_UART1_BASE , IXP425_UART2_BASE.
 */

#undef  UART_NO_RX_INTERRUPT

#if defined(CONFIG_IVPN_20MHZ)
#define IXP425_UART_XTAL        8936727
#elif defined(CONFIG_MACH_IVPN)
#define IXP425_UART_XTAL        13271040
#else
#define IXP425_UART_XTAL        14745600
#endif

/*
 * Constants to make it easy to access  Interrupt Controller registers
 */
#define IXP425_ICPR_OFFSET	0x00 /* Interrupt Status */
#define IXP425_ICMR_OFFSET	0x04 /* Interrupt Enable */
#define IXP425_ICLR_OFFSET	0x08 /* Interrupt IRQ/FIQ Select */
#define IXP425_ICIP_OFFSET      0x0C /* IRQ Status */
#define IXP425_ICFP_OFFSET	0x10 /* FIQ Status */
#define IXP425_ICHR_OFFSET	0x14 /* Interrupt Priority */
#define IXP425_ICIH_OFFSET	0x18 /* IRQ Highest Pri Int */
#define IXP425_ICFH_OFFSET	0x1C /* FIQ Highest Pri Int */

#define IXP425_ICPR2_OFFSET	0x20 /* Interrupt Status */
#define IXP425_ICMR2_OFFSET	0x24 /* Interrupt Enable */
#define IXP425_ICLR2_OFFSET	0x28 /* Interrupt IRQ/FIQ Select */
#define IXP425_ICIP2_OFFSET      0x2C /* IRQ Status */
#define IXP425_ICFP2_OFFSET	0x30 /* FIQ Status */
#define IXP425_ICHR2_OFFSET	0x34 /* Interrupt Priority */
#define IXP425_ICIH2_OFFSET	0x38 /* IRQ Highest Pri Int */
#define IXP425_ICFH2_OFFSET	0x3C /* FIQ Highest Pri Int */

/*
 * Interrupt Controller Register Definitions.
 */
#define IXP425_INTC_REG(x) ((volatile u32 *)(IXP425_INTC_BASE_VIRT+(x)))

#define IXP425_ICPR     IXP425_INTC_REG(IXP425_ICPR_OFFSET)
#define IXP425_ICMR     IXP425_INTC_REG(IXP425_ICMR_OFFSET)
#define IXP425_ICLR     IXP425_INTC_REG(IXP425_ICLR_OFFSET)
#define IXP425_ICIP     IXP425_INTC_REG(IXP425_ICIP_OFFSET)
#define IXP425_ICFP     IXP425_INTC_REG(IXP425_ICFP_OFFSET)
#define IXP425_ICHR     IXP425_INTC_REG(IXP425_ICHR_OFFSET)
#define IXP425_ICIH     IXP425_INTC_REG(IXP425_ICIH_OFFSET) 
#define IXP425_ICFH     IXP425_INTC_REG(IXP425_ICFH_OFFSET)
                                                                                
#define IXP425_ICPR2    IXP425_INTC_REG(IXP425_ICPR2_OFFSET)
#define IXP425_ICMR2    IXP425_INTC_REG(IXP425_ICMR2_OFFSET)
#define IXP425_ICLR2    IXP425_INTC_REG(IXP425_ICLR2_OFFSET)
#define IXP425_ICIP2    IXP425_INTC_REG(IXP425_ICIP2_OFFSET)
#define IXP425_ICFP2    IXP425_INTC_REG(IXP425_ICFP2_OFFSET)
#define IXP425_ICHR2    IXP425_INTC_REG(IXP425_ICHR2_OFFSET)
#define IXP425_ICIH2    IXP425_INTC_REG(IXP425_ICIH2_OFFSET) 
#define IXP425_ICFH2    IXP425_INTC_REG(IXP425_ICFH2_OFFSET)
                                                                                
/*
 * Constants to make it easy to access GPIO registers
 */
#define IXP425_GPIO_GPOUTR_OFFSET       0x00
#define IXP425_GPIO_GPOER_OFFSET        0x04
#define IXP425_GPIO_GPINR_OFFSET        0x08
#define IXP425_GPIO_GPISR_OFFSET        0x0C
#define IXP425_GPIO_GPIT1R_OFFSET	0x10
#define IXP425_GPIO_GPIT2R_OFFSET	0x14
#define IXP425_GPIO_GPCLKR_OFFSET	0x18
#define IXP425_GPIO_GPDBSELR_OFFSET	0x1C

/* 
 * GPIO Register Definitions.
 * [Only perform 32bit reads/writes]
 */
#define IXP425_GPIO_REG(x) ((volatile u32 *)(IXP425_GPIO_BASE_VIRT+(x)))

#define IXP425_GPIO_GPOUTR	IXP425_GPIO_REG(IXP425_GPIO_GPOUTR_OFFSET)
#define IXP425_GPIO_GPOER       IXP425_GPIO_REG(IXP425_GPIO_GPOER_OFFSET)
#define IXP425_GPIO_GPINR       IXP425_GPIO_REG(IXP425_GPIO_GPINR_OFFSET)
#define IXP425_GPIO_GPISR       IXP425_GPIO_REG(IXP425_GPIO_GPISR_OFFSET)
#define IXP425_GPIO_GPIT1R      IXP425_GPIO_REG(IXP425_GPIO_GPIT1R_OFFSET)
#define IXP425_GPIO_GPIT2R      IXP425_GPIO_REG(IXP425_GPIO_GPIT2R_OFFSET)
#define IXP425_GPIO_GPCLKR      IXP425_GPIO_REG(IXP425_GPIO_GPCLKR_OFFSET)
#define IXP425_GPIO_GPDBSELR    IXP425_GPIO_REG(IXP425_GPIO_GPDBSELR_OFFSET)

/*
 * Constants to make it easy to access Timer Control/Status registers
 */
#define IXP425_OSTS_OFFSET	0x00  /* Continious TimeStamp */
#define IXP425_OST1_OFFSET	0x04  /* Timer 1 Timestamp */
#define IXP425_OSRT1_OFFSET	0x08  /* Timer 1 Reload */
#define IXP425_OST2_OFFSET	0x0C  /* Timer 2 Timestamp */
#define IXP425_OSRT2_OFFSET	0x10  /* Timer 2 Reload */
#define IXP425_OSWT_OFFSET	0x14  /* Watchdog Timer */
#define IXP425_OSWE_OFFSET	0x18  /* Watchdog Enable */
#define IXP425_OSWK_OFFSET	0x1C  /* Watchdog Key */
#define IXP425_OSST_OFFSET	0x20  /* Timer Status */

/*
 * Operating System Timer Register Definitions.
 */

#define IXP425_TIMER_REG(x) ((volatile u32 *)(IXP425_TIMER_BASE_VIRT+(x)))

#define IXP425_OSTS	IXP425_TIMER_REG(IXP425_OSTS_OFFSET)
#define IXP425_OST1	IXP425_TIMER_REG(IXP425_OST1_OFFSET)
#define IXP425_OSRT1	IXP425_TIMER_REG(IXP425_OSRT1_OFFSET)
#define IXP425_OST2	IXP425_TIMER_REG(IXP425_OST2_OFFSET)
#define IXP425_OSRT2	IXP425_TIMER_REG(IXP425_OSRT2_OFFSET)
#define IXP425_OSWT	IXP425_TIMER_REG(IXP425_OSWT_OFFSET)
#define IXP425_OSWE	IXP425_TIMER_REG(IXP425_OSWE_OFFSET)
#define IXP425_OSWK	IXP425_TIMER_REG(IXP425_OSWK_OFFSET)
#define IXP425_OSST	IXP425_TIMER_REG(IXP425_OSST_OFFSET)

/*
 * Timer register values and bit definitions 
 */
#define IXP425_OST_ENABLE              BIT(0)
#define IXP425_OST_ONE_SHOT            BIT(1)
/* Low order bits of reload value ignored */
#define IXP425_OST_RELOAD_MASK         (0x3)    
#define IXP425_OST_DISABLED            (0x0)
#define IXP425_OSST_TIMER_1_PEND       BIT(0)
#define IXP425_OSST_TIMER_2_PEND       BIT(1)
#define IXP425_OSST_TIMER_TS_PEND      BIT(2)
#define IXP425_OSST_TIMER_WDOG_PEND    BIT(3)
#define IXP425_OSST_TIMER_WARM_RESET   BIT(4)

/*
 * Constants to make it easy to access PCI Control/Status registers
 */
#define PCI_NP_AD_OFFSET            0x00
#define PCI_NP_CBE_OFFSET           0x04
#define PCI_NP_WDATA_OFFSET         0x08
#define PCI_NP_RDATA_OFFSET         0x0c
#define PCI_CRP_AD_CBE_OFFSET       0x10
#define PCI_CRP_WDATA_OFFSET        0x14
#define PCI_CRP_RDATA_OFFSET        0x18
#define PCI_CSR_OFFSET              0x1c
#define PCI_ISR_OFFSET              0x20
#define PCI_INTEN_OFFSET            0x24
#define PCI_DMACTRL_OFFSET          0x28
#define PCI_AHBMEMBASE_OFFSET       0x2c
#define PCI_AHBIOBASE_OFFSET        0x30
#define PCI_PCIMEMBASE_OFFSET       0x34
#define PCI_AHBDOORBELL_OFFSET      0x38
#define PCI_PCIDOORBELL_OFFSET      0x3C
#define PCI_ATPDMA0_AHBADDR_OFFSET  0x40
#define PCI_ATPDMA0_PCIADDR_OFFSET  0x44
#define PCI_ATPDMA0_LENADDR_OFFSET  0x48
#define PCI_ATPDMA1_AHBADDR_OFFSET  0x4C
#define PCI_ATPDMA1_PCIADDR_OFFSET  0x50
#define PCI_ATPDMA1_LENADDR_OFFSET	0x54

/*
 * PCI Control/Status Registers
 */
#define IXP425_PCI_CSR(x) ((volatile u32 *)(IXP425_PCI_CFG_BASE_VIRT+(x)))

#define PCI_NP_AD               IXP425_PCI_CSR(PCI_NP_AD_OFFSET)
#define PCI_NP_CBE              IXP425_PCI_CSR(PCI_NP_CBE_OFFSET)
#define PCI_NP_WDATA            IXP425_PCI_CSR(PCI_NP_WDATA_OFFSET)
#define PCI_NP_RDATA            IXP425_PCI_CSR(PCI_NP_RDATA_OFFSET)
#define PCI_CRP_AD_CBE          IXP425_PCI_CSR(PCI_CRP_AD_CBE_OFFSET)
#define PCI_CRP_WDATA           IXP425_PCI_CSR(PCI_CRP_WDATA_OFFSET)
#define PCI_CRP_RDATA           IXP425_PCI_CSR(PCI_CRP_RDATA_OFFSET)
#define PCI_CSR                 IXP425_PCI_CSR(PCI_CSR_OFFSET) 
#define PCI_ISR                 IXP425_PCI_CSR(PCI_ISR_OFFSET)
#define PCI_INTEN               IXP425_PCI_CSR(PCI_INTEN_OFFSET)
#define PCI_DMACTRL             IXP425_PCI_CSR(PCI_DMACTRL_OFFSET)
#define PCI_AHBMEMBASE          IXP425_PCI_CSR(PCI_AHBMEMBASE_OFFSET)
#define PCI_AHBIOBASE           IXP425_PCI_CSR(PCI_AHBIOBASE_OFFSET)
#define PCI_PCIMEMBASE          IXP425_PCI_CSR(PCI_PCIMEMBASE_OFFSET)
#define PCI_AHBDOORBELL         IXP425_PCI_CSR(PCI_AHBDOORBELL_OFFSET)
#define PCI_PCIDOORBELL         IXP425_PCI_CSR(PCI_PCIDOORBELL_OFFSET)
#define PCI_ATPDMA0_AHBADDR     IXP425_PCI_CSR(PCI_ATPDMA0_AHBADDR_OFFSET)
#define PCI_ATPDMA0_PCIADDR     IXP425_PCI_CSR(PCI_ATPDMA0_PCIADDR_OFFSET)
#define PCI_ATPDMA0_LENADDR     IXP425_PCI_CSR(PCI_ATPDMA0_LENADDR_OFFSET)
#define PCI_ATPDMA1_AHBADDR     IXP425_PCI_CSR(PCI_ATPDMA1_AHBADDR_OFFSET)
#define PCI_ATPDMA1_PCIADDR     IXP425_PCI_CSR(PCI_ATPDMA1_PCIADDR_OFFSET)
#define PCI_ATPDMA1_LENADDR     IXP425_PCI_CSR(PCI_ATPDMA1_LENADDR_OFFSET)

/*
 * PCI register values and bit definitions 
 */

/* CSR bit definitions */
#define PCI_CSR_HOST    	BIT(0)
#define PCI_CSR_ARBEN   	BIT(1)
#define PCI_CSR_ADS     	BIT(2)
#define PCI_CSR_PDS     	BIT(3)
#define PCI_CSR_ABE     	BIT(4)
#define PCI_CSR_DBT     	BIT(5)
#define PCI_CSR_ASE     	BIT(8)
#define PCI_CSR_IC      	BIT(15)

/* ISR (Interrupt status) Register bit definitions */
#define PCI_ISR_PSE     	BIT(0)
#define PCI_ISR_PFE     	BIT(1)
#define PCI_ISR_PPE     	BIT(2)
#define PCI_ISR_AHBE    	BIT(3)
#define PCI_ISR_APDC    	BIT(4)
#define PCI_ISR_PADC    	BIT(5)
#define PCI_ISR_ADB     	BIT(6)
#define PCI_ISR_PDB     	BIT(7)

/* INTEN (Interrupt Enable) Register bit definitions */
#define PCI_INTEN_PSE   	BIT(0)
#define PCI_INTEN_PFE   	BIT(1)
#define PCI_INTEN_PPE   	BIT(2)
#define PCI_INTEN_AHBE  	BIT(3)
#define PCI_INTEN_APDC  	BIT(4)
#define PCI_INTEN_PADC  	BIT(5)
#define PCI_INTEN_ADB   	BIT(6)
#define PCI_INTEN_PDB   	BIT(7)

/*
 * Shift value for byte enable on NP cmd/byte enable register
 */
#define IXP425_PCI_NP_CBE_BESL		4

/*
 * PCI commands supported by NP access unit
 */
#define NP_CMD_IOREAD			0x2
#define NP_CMD_IOWRITE			0x3
#define NP_CMD_CONFIGREAD		0xa
#define NP_CMD_CONFIGWRITE		0xb
#define NP_CMD_MEMREAD			0x6
#define	NP_CMD_MEMWRITE			0x7

#ifndef __ASSEMBLY__
extern int (*ixp425_pci_read)(u32 addr, u32 cmd, u32* data);
extern int ixp425_pci_write(u32 addr, u32 cmd, u32 data);
extern void ixp425_pci_init(void *);
#endif

/*
 * Constants for CRP access into local config space
 */
#define CRP_AD_CBE_BESL         20
#define CRP_AD_CBE_WRITE        BIT(16)

/*
 * Clock Speed Definitions. The CyberGuard iVPN has a 30MHz base clock
 * (to reduce power consumption).
 */
#if CONFIG_MACH_IVPN
#define IXP425_PERIPHERAL_BUS_CLOCK (60) /* 60Mhzi APB BUS   */ 
#else
#define IXP425_PERIPHERAL_BUS_CLOCK (66) /* 66Mhzi APB BUS   */ 
#endif

#ifndef NO_LINUX_IXP425_USB
/*
 * USB Device Controller
 *
 * These are used by the USB gadget driver, so they don't follow the
 * IXP425_ naming convetions.
 *
 */
# define IXP425_USB_REG(x)       (*((volatile u32 *)(x)))

/* UDC Undocumented - Reserved1 */
#define UDC_RES1	IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0004)  
/* UDC Undocumented - Reserved2 */
#define UDC_RES2	IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0008)  
/* UDC Undocumented - Reserved3 */
#define UDC_RES3	IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x000C)  
/* UDC Control Register */
#define UDCCR		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0000)  
/* UDC Endpoint 0 Control/Status Register */
#define UDCCS0		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0010)  
/* UDC Endpoint 1 (IN) Control/Status Register */
#define UDCCS1		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0014)  
/* UDC Endpoint 2 (OUT) Control/Status Register */
#define UDCCS2		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0018)  
/* UDC Endpoint 3 (IN) Control/Status Register */
#define UDCCS3		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x001C)  
/* UDC Endpoint 4 (OUT) Control/Status Register */
#define UDCCS4		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0020)  
/* UDC Endpoint 5 (Interrupt) Control/Status Register */
#define UDCCS5		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0024)  
/* UDC Endpoint 6 (IN) Control/Status Register */
#define UDCCS6		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0028)  
/* UDC Endpoint 7 (OUT) Control/Status Register */
#define UDCCS7		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x002C)  
/* UDC Endpoint 8 (IN) Control/Status Register */
#define UDCCS8		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0030)  
/* UDC Endpoint 9 (OUT) Control/Status Register */
#define UDCCS9		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0034)  
/* UDC Endpoint 10 (Interrupt) Control/Status Register */
#define UDCCS10		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0038)  
/* UDC Endpoint 11 (IN) Control/Status Register */
#define UDCCS11		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x003C)  
/* UDC Endpoint 12 (OUT) Control/Status Register */
#define UDCCS12		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0040)  
/* UDC Endpoint 13 (IN) Control/Status Register */
#define UDCCS13		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0044)  
/* UDC Endpoint 14 (OUT) Control/Status Register */
#define UDCCS14		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0048)  
/* UDC Endpoint 15 (Interrupt) Control/Status Register */
#define UDCCS15		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x004C)  
/* UDC Frame Number Register High */
#define UFNRH		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0060)  
/* UDC Frame Number Register Low */
#define UFNRL		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0064)  
/* UDC Byte Count Reg 2 */
#define UBCR2		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0068)  
/* UDC Byte Count Reg 4 */
#define UBCR4		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x006c)  
/* UDC Byte Count Reg 7 */
#define UBCR7		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0070)  
/* UDC Byte Count Reg 9 */
#define UBCR9		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0074)  
/* UDC Byte Count Reg 12 */
#define UBCR12		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0078)  
/* UDC Byte Count Reg 14 */
#define UBCR14		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x007c)  
/* UDC Endpoint 0 Data Register */
#define UDDR0		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0080)  
/* UDC Endpoint 1 Data Register */
#define UDDR1		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0100)  
/* UDC Endpoint 2 Data Register */
#define UDDR2		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0180)  
/* UDC Endpoint 3 Data Register */
#define UDDR3		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0200)  
/* UDC Endpoint 4 Data Register */
#define UDDR4		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0400)  
/* UDC Endpoint 5 Data Register */
#define UDDR5		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x00A0)  
/* UDC Endpoint 6 Data Register */
#define UDDR6		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0600)  
/* UDC Endpoint 7 Data Register */
#define UDDR7		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0680)  
/* UDC Endpoint 8 Data Register */
#define UDDR8		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0700)  
/* UDC Endpoint 9 Data Register */
#define UDDR9		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0900)  
/* UDC Endpoint 10 Data Register */
#define UDDR10		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x00C0)  
/* UDC Endpoint 11 Data Register */
#define UDDR11		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0B00)  
/* UDC Endpoint 12 Data Register */
#define UDDR12		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0B80)  
/* UDC Endpoint 13 Data Register */
#define UDDR13		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0C00)  
/* UDC Endpoint 14 Data Register */
#define UDDR14		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0E00)  
/* UDC Endpoint 15 Data Register */
#define UDDR15		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x00E0)  
/* UDC Interrupt Control Register 0 */
#define UICR0		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0050)  
/* UDC Interrupt Control Register 1 */
#define UICR1		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0054)  
/* UDC Status Interrupt Register 0 */
#define USIR0		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x0058)  
/* UDC Status Interrupt Register 1 */
#define USIR1		IXP425_USB_REG(IXP425_USB_BASE_VIRT+0x005C)  

#define UDCCR_UDE	(1 << 0)	/* UDC enable */
#define UDCCR_UDA	(1 << 1)	/* UDC active */
#define UDCCR_RSM	(1 << 2)	/* Device resume */
#define UDCCR_RESIR	(1 << 3)	/* Resume interrupt request */
#define UDCCR_SUSIR	(1 << 4)	/* Suspend interrupt request */
#define UDCCR_SRM	(1 << 5)	/* Suspend/resume interrupt mask */
#define UDCCR_RSTIR	(1 << 6)	/* Reset interrupt request */
#define UDCCR_REM	(1 << 7)	/* Reset interrupt mask */

#define UDCCS0_OPR	(1 << 0)	/* OUT packet ready */
#define UDCCS0_IPR	(1 << 1)	/* IN packet ready */
#define UDCCS0_FTF	(1 << 2)	/* Flush Tx FIFO */
#define UDCCS0_DRWF	(1 << 3)	/* Device remote wakeup feature */
#define UDCCS0_SST	(1 << 4)	/* Sent stall */
#define UDCCS0_FST	(1 << 5)	/* Force stall */
#define UDCCS0_RNE	(1 << 6)	/* Receive FIFO no empty */
#define UDCCS0_SA	(1 << 7)	/* Setup active */

#define UDCCS_BI_TFS	(1 << 0)	/* Transmit FIFO service */
#define UDCCS_BI_TPC	(1 << 1)	/* Transmit packet complete */
#define UDCCS_BI_FTF	(1 << 2)	/* Flush Tx FIFO */
#define UDCCS_BI_TUR	(1 << 3)	/* Transmit FIFO underrun */
#define UDCCS_BI_SST	(1 << 4)	/* Sent stall */
#define UDCCS_BI_FST	(1 << 5)	/* Force stall */
#define UDCCS_BI_TSP	(1 << 7)	/* Transmit short packet */

#define UDCCS_BO_RFS	(1 << 0)	/* Receive FIFO service */
#define UDCCS_BO_RPC	(1 << 1)	/* Receive packet complete */
#define UDCCS_BO_DME	(1 << 3)	/* DMA enable */
#define UDCCS_BO_SST	(1 << 4)	/* Sent stall */
#define UDCCS_BO_FST	(1 << 5)	/* Force stall */
#define UDCCS_BO_RNE	(1 << 6)	/* Receive FIFO not empty */
#define UDCCS_BO_RSP	(1 << 7)	/* Receive short packet */

#define UDCCS_II_TFS	(1 << 0)	/* Transmit FIFO service */
#define UDCCS_II_TPC	(1 << 1)	/* Transmit packet complete */
#define UDCCS_II_FTF	(1 << 2)	/* Flush Tx FIFO */
#define UDCCS_II_TUR	(1 << 3)	/* Transmit FIFO underrun */
#define UDCCS_II_TSP	(1 << 7)	/* Transmit short packet */

#define UDCCS_IO_RFS	(1 << 0)	/* Receive FIFO service */
#define UDCCS_IO_RPC	(1 << 1)	/* Receive packet complete */
#define UDCCS_IO_ROF	(1 << 3)	/* Receive overflow */
#define UDCCS_IO_DME	(1 << 3)	/* DMA enable */
#define UDCCS_IO_RNE	(1 << 6)	/* Receive FIFO not empty */
#define UDCCS_IO_RSP	(1 << 7)	/* Receive short packet */

#define UDCCS_INT_TFS	(1 << 0)	/* Transmit FIFO service */
#define UDCCS_INT_TPC	(1 << 1)	/* Transmit packet complete */
#define UDCCS_INT_FTF	(1 << 2)	/* Flush Tx FIFO */
#define UDCCS_INT_TUR	(1 << 3)	/* Transmit FIFO underrun */
#define UDCCS_INT_SST	(1 << 4)	/* Sent stall */
#define UDCCS_INT_FST	(1 << 5)	/* Force stall */
#define UDCCS_INT_TSP	(1 << 7)	/* Transmit short packet */

#define UICR0_IM0	(1 << 0)	/* Interrupt mask ep 0 */
#define UICR0_IM1	(1 << 1)	/* Interrupt mask ep 1 */
#define UICR0_IM2	(1 << 2)	/* Interrupt mask ep 2 */
#define UICR0_IM3	(1 << 3)	/* Interrupt mask ep 3 */
#define UICR0_IM4	(1 << 4)	/* Interrupt mask ep 4 */
#define UICR0_IM5	(1 << 5)	/* Interrupt mask ep 5 */
#define UICR0_IM6	(1 << 6)	/* Interrupt mask ep 6 */
#define UICR0_IM7	(1 << 7)	/* Interrupt mask ep 7 */

#define UICR1_IM8	(1 << 0)	/* Interrupt mask ep 8 */
#define UICR1_IM9	(1 << 1)	/* Interrupt mask ep 9 */
#define UICR1_IM10	(1 << 2)	/* Interrupt mask ep 10 */
#define UICR1_IM11	(1 << 3)	/* Interrupt mask ep 11 */
#define UICR1_IM12	(1 << 4)	/* Interrupt mask ep 12 */
#define UICR1_IM13	(1 << 5)	/* Interrupt mask ep 13 */
#define UICR1_IM14	(1 << 6)	/* Interrupt mask ep 14 */
#define UICR1_IM15	(1 << 7)	/* Interrupt mask ep 15 */

#define USIR0_IR0	(1 << 0)	/* Interrup request ep 0 */
#define USIR0_IR1	(1 << 1)	/* Interrup request ep 1 */
#define USIR0_IR2	(1 << 2)	/* Interrup request ep 2 */
#define USIR0_IR3	(1 << 3)	/* Interrup request ep 3 */
#define USIR0_IR4	(1 << 4)	/* Interrup request ep 4 */
#define USIR0_IR5	(1 << 5)	/* Interrup request ep 5 */
#define USIR0_IR6	(1 << 6)	/* Interrup request ep 6 */
#define USIR0_IR7	(1 << 7)	/* Interrup request ep 7 */

#define USIR1_IR8	(1 << 0)	/* Interrup request ep 8 */
#define USIR1_IR9	(1 << 1)	/* Interrup request ep 9 */
#define USIR1_IR10	(1 << 2)	/* Interrup request ep 10 */
#define USIR1_IR11	(1 << 3)	/* Interrup request ep 11 */
#define USIR1_IR12	(1 << 4)	/* Interrup request ep 12 */
#define USIR1_IR13	(1 << 5)	/* Interrup request ep 13 */
#define USIR1_IR14	(1 << 6)	/* Interrup request ep 14 */
#define USIR1_IR15	(1 << 7)	/* Interrup request ep 15 */

#define DCMD_LENGTH	0x01fff		/* length mask (max = 8K - 1) */

#endif	/* NO_LINUX_IXP425_USB */

#ifndef __ASSEMBLY__
static inline int cpu_is_ixp46x(void)
{
#ifdef CONFIG_CPU_IXP46X
	return 1;
#else
	return 0;
#endif
}
#endif

#endif
