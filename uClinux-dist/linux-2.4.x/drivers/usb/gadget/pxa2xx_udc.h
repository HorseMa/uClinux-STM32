/*
 * linux/drivers/usb/gadget/pxa2xx_udc.h
 * Intel PXA2xx on-chip full speed USB device controller
 * 
 * Copyright (C) 2003 Robert Schwebel <r.schwebel@pengutronix.de>, Pengutronix
 * Copyright (C) 2003 David Brownell
 * Copyright (C) 2003 Joshua Wise
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_USB_GADGET_PXA2XX_H
#define __LINUX_USB_GADGET_PXA2XX_H

#include <linux/types.h>

/*-------------------------------------------------------------------------*/

/* pxa2xx has this (move to include/asm-arm/arch-pxa/pxa-regs.h) */
#define UFNRH_SIR	(1 << 7)	/* SOF interrupt request */
#define UFNRH_SIM	(1 << 6)	/* SOF interrupt mask */
#define UFNRH_IPE14	(1 << 5)	/* ISO packet error, ep14 */
#define UFNRH_IPE9	(1 << 4)	/* ISO packet error, ep9 */
#define UFNRH_IPE4	(1 << 3)	/* ISO packet error, ep4 */

/* pxa255 has this (move to include/asm-arm/arch-pxa/pxa-regs.h) */
#define	UDCCFR		UDC_RES2	/* UDC Control Function Register */
#define UDCCFR_AREN	(1 << 7)	/* ACK response enable (now) */
#define UDCCFR_ACM	(1 << 2)	/* ACK control mode (wait for AREN) */

/* for address space reservation */
#define	REGISTER_FIRST	((unsigned long)(&UDCCR))
#define	REGISTER_LAST	((unsigned long)(&UDDR14))	/* not UDDR15! */
#define REGISTER_LENGTH	((REGISTER_LAST - REGISTER_FIRST) + 4)

/*-------------------------------------------------------------------------*/

struct pxa2xx_udc;

struct pxa2xx_ep {
	struct usb_ep				ep;
	struct pxa2xx_udc			*dev;

	const struct usb_endpoint_descriptor	*desc;
	struct list_head			queue;
	unsigned long				pio_irqs;
	unsigned long				dma_irqs;
	short					dma; 

	unsigned short				fifo_size;
	u8					bEndpointAddress;
	u8					bmAttributes;

	unsigned				stopped : 1;
	unsigned				dma_fixup : 1;
							 
	/* UDCCS = UDC Control/Status for this EP
	 * UBCR = UDC Byte Count Remaining (contents of OUT fifo)
	 * UDDR = UDC Endpoint Data Register (the fifo)
	 * DRCM = DMA Request Channel Map
	 */
	volatile u32				*reg_udccs;
	volatile u32				*reg_ubcr;
	volatile u32				*reg_uddr;
#ifdef USE_DMA
	volatile u32				*reg_drcmr;
#define	drcmr(n)  .reg_drcmr = & DRCMR ## n ,
#else
#define	drcmr(n)  
#endif
};

struct pxa2xx_request {
	struct usb_request			req;
	struct list_head			queue;
};

enum ep0_state { 
	EP0_IDLE,
	EP0_IN_DATA_PHASE,
	EP0_OUT_DATA_PHASE,
	EP0_END_XFER,
	EP0_STALL,
};

#define EP0_FIFO_SIZE	((unsigned)16)
#define BULK_FIFO_SIZE	((unsigned)64)
#define ISO_FIFO_SIZE	((unsigned)256)
#define INT_FIFO_SIZE	((unsigned)8)

struct udc_stats {
	struct ep0stats {
		unsigned long		ops;
		unsigned long		bytes;
	} read, write;
	unsigned long			irqs;
};

#ifdef CONFIG_USB_PXA2XX_SMALL
/* when memory's tight, SMALL config saves code+data.  */
#undef	USE_DMA
#define	PXA_UDC_NUM_ENDPOINTS	3
#endif

#ifndef	PXA_UDC_NUM_ENDPOINTS
#define	PXA_UDC_NUM_ENDPOINTS	16
#endif

struct pxa2xx_udc {
	struct usb_gadget			gadget;
	struct usb_gadget_driver		*driver;

	enum ep0_state				ep0state;
	struct udc_stats			stats;
	unsigned				got_irq : 1,
						got_disc : 1,
						has_cfr : 1,
						req_pending : 1,
						req_std : 1,
						req_config : 1;

#define start_watchdog(dev) mod_timer(&dev->timer, jiffies + (HZ/200))
	struct timer_list			timer;

	struct pxa2xx_ep			ep [PXA_UDC_NUM_ENDPOINTS];
};

/* 2.5 changes ... */

#ifndef container_of
#define container_of    list_entry
#endif

#ifndef WARN_ON
#define WARN_ON BUG_ON
#endif

/*-------------------------------------------------------------------------*/

/* please keep machine-specific defines in alphabetical order. */

// CONFIG_ARCH_ADI_COYOTE behaves

#ifdef CONFIG_ARCH_E7XX
#  include <asm/arch/e7xx-gpio.h>
#endif

#ifdef CONFIG_ARCH_H1900
#  include <asm/arch/h1900-gpio.h>
#endif

#ifdef CONFIG_ARCH_H3900
#  include <asm/arch/h3900-gpio.h>
#endif

#ifdef CONFIG_ARCH_H5400
#  include <asm/arch/h5400-gpio.h>
#endif

#ifdef CONFIG_ARCH_INNOKOM
#include <asm/arch/innokom.h>
#endif

#ifdef CONFIG_ARCH_LUBBOCK
#include <asm/arch/lubbock.h>
/* lubbock can also report usb connect/disconnect irqs */

#ifdef DEBUG
#define HEX_DISPLAY(n)	if (machine_is_lubbock()) { LUB_HEXLED = (n); }

#define LED_CONNECTED_ON	if (machine_is_lubbock()) { \
	DISCRETE_LED_ON(D26); }
#define LED_CONNECTED_OFF	if(machine_is_lubbock()) { \
	DISCRETE_LED_OFF(D26); LUB_HEXLED = 0; }
#define LED_EP0_ON	if (machine_is_lubbock()) { DISCRETE_LED_ON(D25); }
#define LED_EP0_OFF	if (machine_is_lubbock()) { DISCRETE_LED_OFF(D25); }
#endif /* DEBUG */

#endif

#ifdef CONFIG_ARCH_PXA_CORGI
/* Sharp Zaurus C-700, C-750, C-760, C-860 */
#define	CORGI_CONNECT_GPIO	45
/* use the ARM-Linux registered symbol, not a Lineo-private one */
#define CONFIG_MACH_CORGI
#endif

#ifdef CONFIG_ARCH_PXA_POODLE
/* Sharp B-500, SL-5600 */
#define	POODLE_CONNECT_GPIO	20
/* use the ARM-Linux registered symbol, not a Lineo-private one */
#define CONFIG_MACH_POODLE
#endif

/*-------------------------------------------------------------------------*/

/* LEDs are only for debug */
#ifndef HEX_DISPLAY
#define HEX_DISPLAY(n)		do {} while(0)
#endif

#ifndef LED_CONNECTED_ON
#define LED_CONNECTED_ON	do {} while(0)
#define LED_CONNECTED_OFF	do {} while(0)
#endif
#ifndef LED_EP0_ON
#define LED_EP0_ON		do {} while (0)
#define LED_EP0_OFF		do {} while (0)
#endif

/*-------------------------------------------------------------------------*/

static struct pxa2xx_udc *the_controller;

/* one GPIO should be used to detect host disconnect */
static int is_usb_connected(void)
{
	static int first = 0;
	
	// CONFIG_ARCH_ADI_COYOTE cannot detect or force disconnect
#ifdef CONFIG_ARCH_E7XX
	if (machine_is_e7xx())
		return (GPLR(GPIO_E7XX_USB_DISC)
				& GPIO_bit(GPIO_E7XX_USB_DISC));
#endif
#if 0
#ifdef CONFIG_ARCH_H1900
	if (machine_is_h1900())
		return (!(GPLR(GPIO_NR_H1900_USB_DETECT_N)
				& GPIO_bit(GPIO_NR_H1900_USB_DETECT_N)));
#endif
#ifdef CONFIG_ARCH_H3900
	if (machine_is_h3900())
		return 1;
#endif
#ifdef CONFIG_ARCH_H5400
	// h5400 ... ?
#endif
#endif
#ifdef CONFIG_ARCH_INNOKOM
	if (machine_is_innokom())
		return (GPLR(GPIO_INNOKOM_USB_DISC)
				& GPIO_bit(GPIO_INNOKOM_USB_DISC));
#endif
#ifdef CONFIG_ARCH_LUBBOCK
	if (machine_is_lubbock())
		return ((LUB_MISC_RD & (1 << 9)) == 0);
#endif
	// Sharp's sources didn't show a corgi or poodle hook

	if (!first) {
		pr_info("%s: can't check host connect\n", driver_name);
		first++;
	}
	return 1;
}

static int disc_first = 0;

/* one GPIO should force the host to see this device (or not) */
static void make_usb_disappear(void)
{
	// CONFIG_ARCH_ADI_COYOTE cannot detect or force disconnect
#ifdef CONFIG_ARCH_E7XX
	if (machine_is_e7xx()) {
		GPSR(GPIO_E7XX_USB_PULLUP) = GPIO_bit(GPIO_E7XX_USB_PULLUP);
		return;
	}
#endif
	// h1900 ... ?
#ifdef CONFIG_ARCH_H3900
	if (machine_is_h3900()) {
		GPDR0 &= ~GPIO_H3900_USBP_PULLUP;
		return;
	}
#endif
#ifdef CONFIG_ARCH_H5400
	if (machine_is_h5400()) {
		GPDR(GPIO_NR_H5400_USB_PULLUP) &=
					~GPIO_bit(GPIO_NR_H5400_USB_PULLUP);
		return;
	}
#endif
#ifdef CONFIG_ARCH_INNOKOM
	if (machine_is_innokom()) {
		GPSR(GPIO_INNOKOM_USB_ONOFF) = GPIO_bit(GPIO_INNOKOM_USB_ONOFF);
		printk("innokom: disappear\n");
		udelay(5);
		return;
	}
#endif
#ifdef CONFIG_ARCH_CSB226
	if (machine_is_csb226()) {
		GPCR0 |= 0x00000080;
		printk("csb226: disappear\n");
		udelay(5);
		return;
	}
#endif
	// lubbock has no D+ pullup
#ifdef CONFIG_MACH_CORGI
	if (machine_is_corgi()) {
		GPDR(CORGI_CONNECT_GPIO) |= GPIO_bit(CORGI_CONNECT_GPIO);
		GPCR(CORGI_CONNECT_GPIO) = GPIO_bit(CORGI_CONNECT_GPIO);
	}
#endif
#ifdef CONFIG_MACH_POODLE
	if (machine_is_poodle()) {
		GPDR(POODLE_CONNECT_GPIO) |= GPIO_bit(POODLE_CONNECT_GPIO);
		GPCR(POODLE_CONNECT_GPIO) = GPIO_bit(POODLE_CONNECT_GPIO);
	}
#endif

	if (!disc_first) {
		pr_info("%s: can't force usb disconnect\n", driver_name);
		disc_first++;
	}
}

static void let_usb_appear(void)
{
	// CONFIG_ARCH_ADI_COYOTE cannot detect or force disconnect
#ifdef CONFIG_ARCH_E7XX
	if (machine_is_e7xx()) {
		GPCR(GPIO_E7XX_USB_PULLUP) = GPIO_bit(GPIO_E7XX_USB_PULLUP);
		return;
	}
#endif
	// h1900 ... ?
#ifdef CONFIG_ARCH_H3900
	if (machine_is_h3900()) {
		GPDR0 |= GPIO_H3900_USBP_PULLUP;
		GPSR0 |= GPIO_H3900_USBP_PULLUP;
		return;
	}
#endif
#ifdef CONFIG_ARCH_H5400
	if (machine_is_h5400()) {
		GPDR(GPIO_NR_H5400_USB_PULLUP) |=
					GPIO_bit(GPIO_NR_H5400_USB_PULLUP);
		return;
	}
#endif
#ifdef CONFIG_ARCH_INNOKOM
	if (machine_is_innokom()) {
		GPCR(GPIO_INNOKOM_USB_ONOFF) = GPIO_bit(GPIO_INNOKOM_USB_ONOFF);
		printk("innokom: appear\n");
		udelay(5);
		return;
	}
#endif
#ifdef CONFIG_ARCH_CSB226
	if (machine_is_csb226()) {
		GPDR0 |= 0x00000080;
		GPSR0 |= 0x00000080;
		printk("csb226: appear\n");
		udelay(5);
		return;
	}
#endif
	// lubbock has no D+ pullup
#ifdef CONFIG_MACH_CORGI
	if (machine_is_corgi()) {
		GPDR(CORGI_CONNECT_GPIO) |= GPIO_bit(CORGI_CONNECT_GPIO);
		GPSR(CORGI_CONNECT_GPIO) = GPIO_bit(CORGI_CONNECT_GPIO);
	}
#endif
#ifdef CONFIG_MACH_POODLE
	if (machine_is_poodle()) {
		GPDR(POODLE_CONNECT_GPIO) |= GPIO_bit(POODLE_CONNECT_GPIO);
		GPSR(POODLE_CONNECT_GPIO) = GPIO_bit(POODLE_CONNECT_GPIO);
	}
#endif

	if (!disc_first) {
		pr_info("%s: can't force usb disconnect\n", driver_name);
		disc_first++;
	}
}

/*-------------------------------------------------------------------------*/

/* LEDs are only for debug */
#ifndef LED_CONNECTED_ON
#define LED_CONNECTED_ON	do {} while(0)
#define LED_CONNECTED_OFF	do {} while(0)
#endif
#ifndef LED_EP0_ON
#define LED_EP0_ON		do {} while (0)
#define LED_EP0_OFF		do {} while (0)
#endif

/*-------------------------------------------------------------------------*/

/*
 * Debugging support vanishes in non-debug builds.  DBG_NORMAL should be
 * mostly silent during normal use/testing, with no timing side-effects.
 */
#define DBG_NORMAL	1	/* error paths, device state transitions */
#define DBG_VERBOSE	2	/* add some success path trace info */
#define DBG_NOISY	3	/* ... even more: request level */
#define DBG_VERY_NOISY	4	/* ... even more: packet level */

#ifdef DEBUG

static const char *state_name[] = {
	"EP0_IDLE",
	"EP0_IN_DATA_PHASE", "EP0_OUT_DATA_PHASE",
	"EP0_END_XFER", "EP0_STALL"
};

#define DMSG(stuff...) printk(KERN_DEBUG "udc: " stuff)

#ifdef VERBOSE
#    define UDC_DEBUG DBG_VERBOSE
#else
#    define UDC_DEBUG DBG_NORMAL
#endif

static void __attribute__ ((__unused__))
dump_udccr(const char *label)
{
	u32	udccr = UDCCR;
	DMSG("%s %02X =%s%s%s%s%s%s%s%s\n",
		label, udccr,
		(udccr & UDCCR_REM) ? " rem" : "",
		(udccr & UDCCR_RSTIR) ? " rstir" : "",
		(udccr & UDCCR_SRM) ? " srm" : "",
		(udccr & UDCCR_SUSIR) ? " susir" : "",
		(udccr & UDCCR_RESIR) ? " resir" : "",
		(udccr & UDCCR_RSM) ? " rsm" : "",
		(udccr & UDCCR_UDA) ? " uda" : "",
		(udccr & UDCCR_UDE) ? " ude" : "");
}

static void __attribute__ ((__unused__))
dump_udccs0(const char *label)
{
	u32		udccs0 = UDCCS0;

	DMSG("%s %s %02X =%s%s%s%s%s%s%s%s\n",
		label, state_name[the_controller->ep0state], udccs0,
		(udccs0 & UDCCS0_SA) ? " sa" : "",
		(udccs0 & UDCCS0_RNE) ? " rne" : "",
		(udccs0 & UDCCS0_FST) ? " fst" : "",
		(udccs0 & UDCCS0_SST) ? " sst" : "",
		(udccs0 & UDCCS0_DRWF) ? " dwrf" : "",
		(udccs0 & UDCCS0_FTF) ? " ftf" : "",
		(udccs0 & UDCCS0_IPR) ? " ipr" : "",
		(udccs0 & UDCCS0_OPR) ? " opr" : "");
}

static void __attribute__ ((__unused__))
dump_state(struct pxa2xx_udc *dev)
{
	u32		tmp;
	unsigned	i;

	DMSG("%s %s, uicr %02X.%02X, usir %02X.%02x, ufnr %02X.%02X\n",
		is_usb_connected() ? "host " : "disconnected",
		state_name[dev->ep0state],
		UICR1, UICR0, USIR1, USIR0, UFNRH, UFNRL);
	dump_udccr("udccr");
	if (dev->has_cfr) {
		tmp = UDCCFR;
		DMSG("udccfr %02X =%s%s\n", tmp,
			(tmp & UDCCFR_AREN) ? " aren" : "",
			(tmp & UDCCFR_ACM) ? " acm" : "");
	}

	if (!dev->driver) {
		DMSG("no gadget driver bound\n");
		return;
	} else
		DMSG("ep0 driver '%s'\n", dev->driver->driver.name);
	
	if (!is_usb_connected())
		return;

	dump_udccs0 ("udccs0");
	DMSG("ep0 IN %lu/%lu, OUT %lu/%lu\n",
		dev->stats.write.bytes, dev->stats.write.ops,
		dev->stats.read.bytes, dev->stats.read.ops);

	for (i = 1; i < PXA_UDC_NUM_ENDPOINTS; i++) {
		if (dev->ep [i].desc == 0)
			continue;
		DMSG ("udccs%d = %02x\n", i, *dev->ep->reg_udccs);
	}
}

#else

#define DMSG(stuff...)		do{}while(0)

#define	dump_udccr(x)	do{}while(0)
#define	dump_udccs0(x)	do{}while(0)
#define	dump_state(x)	do{}while(0)

#define UDC_DEBUG ((unsigned)0)

#endif

#define DBG(lvl, stuff...) do{if ((lvl) <= UDC_DEBUG) DMSG(stuff);}while(0)

#define WARN(stuff...) printk(KERN_WARNING "udc: " stuff)
#define INFO(stuff...) printk(KERN_INFO "udc: " stuff)


/* 2.4 backport support */
#define irqreturn_t	void
#define	IRQ_HANDLED


#endif /* __LINUX_USB_GADGET_PXA2XX_H */
