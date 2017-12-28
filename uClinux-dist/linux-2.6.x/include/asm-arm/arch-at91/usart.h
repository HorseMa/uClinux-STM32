/*
 * include/asm-arm/arch-at91/usart.h
 *
 * for Atmel AT91 series
 * 2001 Erwin Authried
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __ASM_ARCH_AT91X40_H__
#define __ASM_ARCH_AT91X40_H__

#define	AT91_USART_CNT		2
#define	AT91_USART0_BASE	(0xfffd0000)
#define	AT91_USART1_BASE	(0xfffcc000)
#define	AT91_TC_BASE		(0xfffe0000)
#define	AIC_BASE		(0xfffff000)	
#define	AT91_PIOA_BASE		(0xffff0000)
#define	AT91_SF_CIDR		(0xfff00000)

#define	HW_AT91_TIMER_INIT(timer)	/* no PMC */

/*
 * Use TC0 as hardware timer to create high resolution timestamps for
 * debugging. Timer 0 must be set up as a free running counter,
 * e.g. in the bootloader.
 */
#define	HW_COUNTER		\
	(((struct at91_timers *)AT91_TC_BASE)->chans[0].ch.cv)

/*
 * Enable US0, US1
 */
#define	HW_AT91_USART_INIT	\
	((volatile struct pio_regs *)AT91_PIOA_BASE)->pdr = \
		PIOA_RXD0|PIOA_TXD0|PIOA_RXD1|PIOA_TXD1; 

/*
 * PIOA bit allocation.
 */
#define PIOA_TCLK0	(1<<0)					
#define PIOA_TI0A0	(1<<1)					
#define PIOA_TI0B0	(1<<2)					
#define PIOA_TCLK1	(1<<3)					
#define PIOA_TIOA1	(1<<4)				
#define PIOA_TIOB1	(1<<5)				
#define PIOA_TCLK2	(1<<6)					
#define PIOA_TIOA2	(1<<7)				
#define PIOA_TIOB2	(1<<8)				
#define PIOA_IRQ0	(1<<9)				
#define PIOA_IRQ1	(1<<10)				
#define PIOA_IRQ2	(1<<11)				
#define PIOA_FIQ	(1<<12)					
#define PIOA_SCK0	(1<<13)					
#define PIOA_TXD0	(1<<14)					
#define PIOA_RXD0	(1<<15)

#define PIOA_SCK1	(1<<20)					
#define PIOA_TXD1	(1<<21)					
#define PIOA_RXD1	(1<<22)

#define PIOA_MCK0	(1<<25)	
#define PIOA_NCS2	(1<<26)
#define PIOA_NCS3	(1<<27)	

#define PIOA_A20_CS7	(1<<28)
#define PIOA_A21_CS6	(1<<29)	
#define PIOA_A22_CS5	(1<<30)
#define PIOA_A23_CS4	(1<<31)

#endif /* __ASM_ARCH_AT91X40_H__ */



#ifndef __ASM_ARCH_HARDWARE_H__
#define __ASM_ARCH_HARDWARE_H__

/*
 * 0=TC0, 1=TC1, 2=TC2
 */
#define KERNEL_TIMER 1	

#define AIC_SMR(i)	(AIC_BASE + i*4)
#define AIC_IVR		(AIC_BASE + 0x100)
#define AIC_FVR		(AIC_BASE + 0x104)
#define AIC_ISR		(AIC_BASE + 0x108)
#define AIC_IPR		(AIC_BASE + 0x10C)
#define AIC_IMR		(AIC_BASE + 0x110)
#define AIC_CISR	(AIC_BASE + 0x114)
#define AIC_IECR	(AIC_BASE + 0x120)
#define AIC_IDCR	(AIC_BASE + 0x124)
#define AIC_ICCR	(AIC_BASE + 0x128)
#define AIC_ISCR	(AIC_BASE + 0x12C)
#define AIC_EOICR	(AIC_BASE + 0x130)

#ifndef __ASSEMBLER__
struct at91_timer_channel
{
	u32	ccr;		/* channel control register	(WO) */
	u32	cmr;		/* channel mode register	(RW) */
	u32	reserved[2];
	u32	cv;		/* counter value		(RW) */
	u32	ra;		/* register A			(RW) */
	u32	rb;		/* register B			(RW) */
	u32	rc;		/* register C			(RW) */
	u32	sr;		/* status register		(RO) */
	u32	ier;		/* interrupt enable register	(WO) */
	u32	idr;		/* interrupt disable register	(WO) */
	u32	imr;		/* interrupt mask register	(RO) */
};

struct at91_timers
{
	struct {
		struct at91_timer_channel ch;
		unsigned char padding[0x40-sizeof(struct at91_timer_channel)];
	} chans[3];
	u32	bcr;		/* block control register	(WO) */
	u32	bmr;		/* block mode	 register	(RW) */
};
#endif

/* TC control register */
#define TC_SYNC		(1)

/* TC mode register */
#define TC2XC2S(x)	(x & 0x3)
#define TC1XC1S(x)	(x<<2 & 0xc)
#define TC0XC0S(x)	(x<<4 & 0x30)
#define TCNXCNS(timer,v) ((v) << (timer<<1))

/* TC channel control */
#define TC_CLKEN	(1)			
#define TC_CLKDIS	(1<<1)			
#define TC_SWTRG	(1<<2)			

/* TC interrupts enable/disable/mask and status registers */
#define TC_MTIOB	(1<<18)
#define TC_MTIOA	(1<<17)
#define TC_CLKSTA	(1<<16)

#define TC_ETRGS	(1<<7)
#define TC_LDRBS	(1<<6)
#define TC_LDRAS	(1<<5)
#define TC_CPCS		(1<<4)
#define TC_CPBS		(1<<3)
#define TC_CPAS		(1<<2)
#define TC_LOVRS	(1<<1)
#define TC_COVFS	(1)

#define PIO(i)		(1<<i)

#ifndef __ASSEMBLER__
struct pio_regs {
	u32	per;
	u32	pdr;
	u32	psr;
	u32	res1;
	u32	oer;
	u32	odr;
	u32	osr;
	u32	res2;
	u32	ifer;
	u32	ifdr;
	u32	ifsr;
	u32	res3;
	u32	sodr;
	u32	codr;
	u32	odsr;
	u32	pdsr;
	u32	ier;
	u32	idr;
	u32	imr;
	u32	isr;
};

struct pmc_regs {
	u32	scer;
	u32	scdr;
	u32	scsr;
	u32	reserved;
	u32	pcer;
	u32	pcdr;
	u32	pcsr;
};
#endif /* __ASSEMBLER__ */

#endif /* __ASM_ARCH_HARDWARE_H__ */



#ifndef __ASM_ARCH_USART_H__
#define __ASM_ARCH_USART_H__

/* US control register */
#define US_SENDA	(1<<12)
#define US_STTO		(1<<11)
#define US_STPBRK	(1<<10)
#define US_STTBRK	(1<<9)
#define US_RSTSTA	(1<<8)
#define US_TXDIS	(1<<7)
#define US_TXEN		(1<<6)
#define US_RXDIS	(1<<5)
#define US_RXEN		(1<<4)
#define US_RSTTX	(1<<3)
#define US_RSTRX	(1<<2)

/* US mode register */
#define US_CLK0		(1<<18)
#define US_MODE9	(1<<17)
#define US_CHMODE(x)	(x<<14 & 0xc000)
#define US_NBSTOP(x)	(x<<12 & 0x3000)
#define US_PAR(x)	(x<<9 & 0xe00)
#define US_SYNC		(1<<8)
#define US_CHRL(x)	(x<<6 & 0xc0)
#define US_USCLKS(x)	(x<<4 & 0x30)

/* US interrupts enable/disable/mask and status register */
#define US_DMSI		(1<<10)
#define US_TXEMPTY	(1<<9)
#define US_TIMEOUT	(1<<8)
#define US_PARE		(1<<7)
#define US_FRAME	(1<<6)
#define US_OVRE		(1<<5)
#define US_ENDTX	(1<<4)
#define US_ENDRX	(1<<3)
#define US_RXBRK	(1<<2)
#define US_TXRDY	(1<<1)
#define US_RXRDY	(1)

#define US_ALL_INTS	(US_DMSI | US_TXEMPTY | US_TIMEOUT | US_PARE | \
			 US_FRAME | US_OVRE | US_ENDTX | US_ENDRX | \
			 US_RXBRK | US_TXRDY | US_RXRDY)

struct atmel_usart_regs {
	u32	cr;		/* control */
	u32	mr;		/* mode */
	u32	ier;		/* interrupt enable */
	u32	idr;		/* interrupt disable */
	u32	imr;		/* interrupt mask */
	u32	csr;		/* channel status */
	u32	rhr;		/* receive holding  */
	u32	thr;		/* tramsmit holding */
	u32	brgr;		/* baud rate generator */
	u32	rtor;		/* rx time-out */
	u32	ttgr;		/* tx time-guard */
	u32	res1;
	u32	rpr;		/* rx pointer */
	u32	rcr;		/* rx counter */
	u32	tpr;		/* tx pointer */
	u32	tcr;		/* tx counter */
};

#endif  /* __ASM_ARCH_USART_H__ */
