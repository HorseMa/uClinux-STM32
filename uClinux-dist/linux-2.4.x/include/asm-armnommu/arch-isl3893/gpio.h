/*
 * include/asm-armnommu/arch-isl3893/gpio.h
 */
/*  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __ASM_ARCH_GPIO_H
#define __ASM_ARCH_GPIO_H

/* Register values */

/* - DR */
#define	GPIODR				(0x0)
#define	GPIODRMask			(0xffff)
#define	GPIODRAccessType		(INITIAL_TEST)
#define	GPIODRInitialValue		(0x0)
#define	GPIODRTestMask			(0xffff)

/* - ST */
#define	GPIOST				(0x4)
#define	GPIOSTMask			(0xffff)
#define	GPIOSTAccessType		(NO_TEST)
#define	GPIOSTInitialValue		(0x0)
#define	GPIOSTTestMask			(0xffff)

/* - DDR */
#define	GPIODDR				(0x8)
#define	GPIODDRMask			(0xffff)
#define	GPIODDRAccessType		(INITIAL_TEST)
#define	GPIODDRInitialValue		(0x0)
#define	GPIODDRTestMask			(0xffff)

/* - FIQ */
#define	GPIOFIQ				(0xc)
#define	GPIOFIQMask			(0xffff)
#define	GPIOFIQAccessType		(INITIAL_TEST)
#define	GPIOFIQInitialValue		(0x0)
#define	GPIOFIQTestMask			(0xffff)

/* - IRQ */
#define	GPIOIRQ				(0x10)
#define	GPIOIRQMask			(0xffff)
#define	GPIOIRQAccessType		(INITIAL_TEST)
#define	GPIOIRQInitialValue		(0x0)
#define	GPIOIRQTestMask			(0xffff)

/* - INT */
#define	GPIOINT				(0x14)
#define	GPIOINTMask			(0xffff)
#define	GPIOINTAccessType		(NO_TEST)
#define	GPIOINTInitialValue		(0x0)
#define	GPIOINTTestMask			(0xffff)

/* - INTCL */
#define	GPIOINTCL			(0x18)
#define	GPIOINTCLMask			(0xffff)
#define	GPIOINTCLAccessType		(WO)
#define	GPIOINTCLInitialValue		(0x0)
#define	GPIOINTCLTestMask		(0xffff)

/* - SNR */
#define	GPIOSNR				(0x1c)
#define	GPIOSNRMask			(0xffff)
#define	GPIOSNRAccessType		(INITIAL_TEST)
#define	GPIOSNRInitialValue		(0x0)
#define	GPIOSNRTestMask			(0xffff)

/* - EENR */
#define	GPIOEENR			(0x20)
#define	GPIOEENRMask			(0xffff)
#define	GPIOEENRAccessType		(INITIAL_TEST)
#define	GPIOEENRInitialValue		(0x0)
#define	GPIOEENRTestMask		(0xffff)

/* - FSEL */
#define	GPIOFSEL			(0x24)
#define	GPIOFSELTxUDC			(0x80)
#define	GPIOFSELRxUDCp			(0x40)
#define	GPIOFSELRxUDCm			(0x20)
#define	GPIOFSELBypass			(0x10)
#define	GPIOFSELTxUDCp			(0x8)
#define	GPIOFSELTxUDCm			(0x4)
#define	GPIOFSELUDCEnable		(0x2)
#define	GPIOFSELUDCSuspend		(0x1)
#define	GPIOFSELMask			(0xff)
#define	GPIOFSELAccessType		(INITIAL_TEST)
#define	GPIOFSELInitialValue		(0x0)
#define	GPIOFSELTestMask		(0xff)

/* Instance values */
#define aGPIO1DR			 0x380
#define aGPIO1ST			 0x384
#define aGPIO1DDR			 0x388
#define aGPIO1FIQ			 0x38c
#define aGPIO1IRQ			 0x390
#define aGPIO1INT			 0x394
#define aGPIO1INTCL			 0x398
#define aGPIO1SNR			 0x39c
#define aGPIO1EENR			 0x3a0
#define aGPIO1FSEL			 0x3a4
#define aGPIO2DR			 0x400
#define aGPIO2ST			 0x404
#define aGPIO2DDR			 0x408
#define aGPIO2FIQ			 0x40c
#define aGPIO2IRQ			 0x410
#define aGPIO2INT			 0x414
#define aGPIO2INTCL			 0x418
#define aGPIO2SNR			 0x41c
#define aGPIO2EENR			 0x420
#define aGPIO2FSEL			 0x424
#define aGPIO3DR			 0x480
#define aGPIO3ST			 0x484
#define aGPIO3DDR			 0x488
#define aGPIO3FIQ			 0x48c
#define aGPIO3IRQ			 0x490
#define aGPIO3INT			 0x494
#define aGPIO3INTCL			 0x498
#define aGPIO3SNR			 0x49c
#define aGPIO3EENR			 0x4a0
#define aGPIO3FSEL			 0x4a4

#ifndef __ASSEMBLER__

enum
{
    GPIO1_PIN_FACTORY_RESET	= 15,
    GPIO1_PIN_ANT_SEL		= 14,
    GPIO1_PIN_PE1		= 13,
    GPIO1_PIN_TR_SW_BAR		= 12,
    GPIO1_PIN_ANT_SEL_BAR	= 11,
    GPIO1_PIN_TR_SW		= 10,
    GPIO1_PIN_PLL_LD		= 9,
    GPIO1_PIN_PE2		= 8,
    GPIO1_PIN_HB_LB		= 7,
    GPIO1_PIN_SYNTH_DATA	= 6,
    GPIO1_PIN_SYNTH_CLK		= 5,
    GPIO1_PIN_CB_CLKRUN		= 2,
    GPIO1_PIN_SYNTH_LE		= 1,

    GPIO3_PIN_PERIPHERAL_RESET	= 7,
    GPIO3_PIN_UART_TX		= 6,
    GPIO3_PIN_UART_RX		= 5,
    GPIO3_PIN_LED4		= 4,
    GPIO3_PIN_LED3		= 3,
    GPIO3_PIN_LED2		= 2,
    GPIO3_PIN_LED1		= 1,
    GPIO3_PIN_LED0		= 0
};

enum
{
    GPIO1_MASK_FACTORY_RESET	= (1 << GPIO1_PIN_FACTORY_RESET),
    GPIO1_MASK_ANT_SEL		= (1 << GPIO1_PIN_ANT_SEL),
    GPIO1_MASK_PE1		= (1 << GPIO1_PIN_PE1),
    GPIO1_MASK_TR_SW_BAR	= (1 << GPIO1_PIN_TR_SW_BAR),
    GPIO1_MASK_ANT_SEL_BAR	= (1 << GPIO1_PIN_ANT_SEL_BAR),
    GPIO1_MASK_TR_SW		= (1 << GPIO1_PIN_TR_SW),
    GPIO1_MASK_PLL_LD		= (1 << GPIO1_PIN_PLL_LD),
    GPIO1_MASK_PE2		= (1 << GPIO1_PIN_PE2),
    GPIO1_MASK_HB_LB		= (1 << GPIO1_PIN_HB_LB),
    GPIO1_MASK_SYNTH_DATA	= (1 << GPIO1_PIN_SYNTH_DATA),
    GPIO1_MASK_SYNTH_CLK	= (1 << GPIO1_PIN_SYNTH_CLK),
    GPIO1_MASK_CB_CLKRUN	= (1 << GPIO1_PIN_CB_CLKRUN),
    GPIO1_MASK_SYNTH_LE		= (1 << GPIO1_PIN_SYNTH_LE),

    GPIO3_MASK_PERIPHERAL_RESET	= (1 << GPIO3_PIN_PERIPHERAL_RESET),
    GPIO3_MASK_UART_TX		= (1 << GPIO3_PIN_UART_TX),
    GPIO3_MASK_UART_RX		= (1 << GPIO3_PIN_UART_RX),
    GPIO3_MASK_LED4		= (1 << GPIO3_PIN_LED4),
    GPIO3_MASK_LED3		= (1 << GPIO3_PIN_LED3),
    GPIO3_MASK_LED2		= (1 << GPIO3_PIN_LED2),
    GPIO3_MASK_LED1		= (1 << GPIO3_PIN_LED1),
    GPIO3_MASK_LED0		= (1 << GPIO3_PIN_LED0)    
};

#define gpio_set( val )         ( (unsigned int) (val) << 16 | (val) )
#define gpio_clr( val )         ( (unsigned int) (val) << 16         )

/* C struct view */
typedef struct s_GPIO {
 unsigned DR; /* @0 */
 unsigned ST; /* @4 */
 unsigned DDR; /* @8 */
 unsigned FIQ; /* @12 */
 unsigned IRQ; /* @16 */
 unsigned INT; /* @20 */
 unsigned INTCL; /* @24 */
 unsigned SNR; /* @28 */
 unsigned EENR; /* @32 */
 unsigned FSEL; /* @36 */
} s_GPIO;

#endif /* !__ASSEMBLER__ */

#define uGPIO1 ((volatile struct s_GPIO *) 0xc0000380)
#define uGPIO2 ((volatile struct s_GPIO *) 0xc0000400)
#define uGPIO3 ((volatile struct s_GPIO *) 0xc0000480)

#endif /* __ASM_ARCH_GPIO_H */
