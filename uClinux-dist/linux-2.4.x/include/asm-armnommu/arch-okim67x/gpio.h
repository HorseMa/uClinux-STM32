/* include/asm-armnommu/arch-okim67x/gpio.h
 *
 * (c) 2005 Simtec Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 * 
*/


#define OKI_GPCTL	(0xB7000000)

#define OKI_GPPOA	(0xB7A01000)
#define OKI_GPPIA	(0xB7A01004)
#define OKI_GPPMA	(0xB7A01008)
#define OKI_GPPIEA	(0xB7A0100C)
#define OKI_GPPIPA	(0xB7A01010)
#define OKI_GPPISA	(0xB7A01014)

#define OKI_GPPOB	(0xB7A01020)
#define OKI_GPPIB	(0xB7A01024)
#define OKI_GPPMB	(0xB7A01028)
#define OKI_GPPIEB	(0xB7A0102C)
#define OKI_GPPIPB	(0xB7A01030)
#define OKI_GPPISB	(0xB7A01034)

#define OKI_GPPOC	(0xB7A01040)
#define OKI_GPPIC	(0xB7A01044)
#define OKI_GPPMC	(0xB7A01048)
#define OKI_GPPIEC	(0xB7A0104C)
#define OKI_GPPIPC	(0xB7A01050)
#define OKI_GPPISC	(0xB7A01054)

#define OKI_GPPOD	(0xB7A01060)
#define OKI_GPPID	(0xB7A01064)
#define OKI_GPPMD	(0xB7A01068)
#define OKI_GPPIED	(0xB7A0106C)
#define OKI_GPPIPD	(0xB7A01070)
#define OKI_GPPISD	(0xB7A01074)

#define OKI_GPPOE	(0xB7A01080)
#define OKI_GPPIE	(0xB7A01084)
#define OKI_GPPME	(0xB7A01088)
#define OKI_GPPIEE	(0xB7A0108C)
#define OKI_GPPIPE	(0xB7A01090)
#define OKI_GPPISE	(0xB7A01094)

/* GPIO PIN mode control */

#define OKI_GPCTL_UART	(1<<0)
#define OKI_GPCTL_SIO	(1<<1)
#define OKI_GPCTL_XA	(1<<2)
#define OKI_GPCTL_DMA0	(1<<3)
#define OKI_GPCTL_DMA1	(1<<4)
#define OKI_GPCTL_PWM	(1<<5)
#define OKI_GPCTL_XWAIT	(1<<6)
#define OKI_GPCTL_XWR	(1<<7)
#define OKI_GPCTL_SSIO	(1<<8)
#define OKI_GPCTL_I2C	(1<<9)
#define OKI_GPCTL_EXTI0	(1<<10)
#define OKI_GPCTL_EXTI1	(1<<11)
#define OKI_GPCTL_EXTI2	(1<<12)
#define OKI_GPCTL_EXTI3	(1<<13)
#define OKI_GPCTL_EFIQ	(1<<14)

static __inline__ void gpctl_modify(unsigned int orr, unsigned int bic)
{
	unsigned long flags;
	unsigned long gpctl;

	local_irq_save(flags);

	gpctl = __raw_readw(OKI_GPCTL);
	gpctl |= orr;
	gpctl &= ~bic;
	__raw_writew(gpctl, OKI_GPCTL);

	local_irq_restore(flags);
}

static __inline__ void gpio_change(unsigned int reg,
				   unsigned int set, unsigned int clr)
{

	unsigned long flags;
	unsigned long gpctl;

	local_irq_save(flags);

	gpctl = __raw_readw(reg);
	gpctl |= set;
	gpctl &= ~clr;
	__raw_writew(gpctl, reg);

	local_irq_restore(flags);

}
