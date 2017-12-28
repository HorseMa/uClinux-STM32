/*
 * linux/drivers/usb/gadget/lh7a40x_udc.h
 * Sharp LH7A40x on-chip full speed USB device controllers
 *
 * Copyright (C) 2004 Mikko Lahteenmaki, Nordic ID
 * Copyright (C) 2004 Bo Henriksen, Nordic ID
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
 *
 */

#ifndef __STM3210E_EVAL_UDC_H_
#define __STM3210E_EVAL_UDC_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include <asm/byteorder.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "stm32_usblib/inc/usb_lib.h"
#include <asm/arch/stm32f10x_conf.h>

/*-------------------------------------------------------------*/
/* EP_NUM */
/* defines how many endpoints are used by the device */
/*-------------------------------------------------------------*/

#define EP_NUM                          (4)

/*-------------------------------------------------------------*/
/* --------------   Buffer Description Table  -----------------*/
/*-------------------------------------------------------------*/
/* buffer table base address */
/* buffer table base address */
#define BTABLE_ADDRESS      (0x00)

/* EP0  */
/* rx/tx buffer base address */
#define ENDP0_RXADDR        (0x118)
#define ENDP0_TXADDR        (0x158)

/* EP1  */
/* tx buffer base address */
#define ENDP1_TXADDR        (0x200)

/* EP2  */
/* Rx buffer base address */
#define ENDP2_RXADDR        (0x240)

#define ENDP3_TXADDR	    (0x280)


/*-------------------------------------------------------------*/
/* -------------------   ISTR events  -------------------------*/
/*-------------------------------------------------------------*/
/* IMR_MSK */
/* mask defining which events has to be handled */
/* by the device application software */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM  | CNTR_ERRM | CNTR_RESETM )


#define EPxFIFO_ADDR(EPx)		((EPx)+PMAAddr)

#define EP0_PACKETSIZE  	0x08
#define EP_MAXPACKETSIZE 	0x40

#define UDC_MAX_ENDPOINTS EP_NUM

/* ********************************************************************************************* */
/* IO
 */

typedef enum ep_type {
	ep_control, ep_bulk_in, ep_bulk_out, ep_interrupt
} ep_type_t;

struct stm32f10x_ep {
	struct usb_ep ep;
	struct stm32f10x_udc *dev;

	const struct usb_endpoint_descriptor *desc;
	struct list_head queue;
	unsigned long pio_irqs;

	u8 stopped;
	u8 bEndpointAddress;
	u8 bmAttributes;

	ep_type_t ep_type;
	u32 fifo;
	u32 csr;
};

struct stm32f10x_request {
	struct usb_request req;
	struct list_head queue;
};

struct stm32f10x_udc {
	struct usb_gadget gadget;
	struct usb_gadget_driver *driver;
	struct device *dev;
	spinlock_t lock;

	int ep0state;
	struct stm32f10x_ep ep[UDC_MAX_ENDPOINTS];

	unsigned char usb_address;

	unsigned req_pending:1, req_std:1, req_config:1;
};

extern struct stm32f10x_udc *the_controller;

#define ep_is_in(EP) 		(((EP)->bEndpointAddress&USB_DIR_IN)==USB_DIR_IN)
#define ep_index(EP) 		((EP)->bEndpointAddress&0xF)
#define ep_maxpacket(EP) 	((EP)->ep.maxpacket)


#define USB_DISCONNECT            GPIOB  
#define USB_DISCONNECT_PIN        GPIO_Pin_14
#define RCC_APB2Periph_GPIO_DISCONNECT      RCC_APB2Periph_GPIOB
  
#define USB_CNTR		((u32)CNTR)		//USB Main Control Register
#define USB_ISTR		((u32)ISTR)		//USB  Interrupt status register

#define WAIT_FOR_SETUP          0
#define DATA_STATE_XMIT         1
#define DATA_STATE_NEED_ZLP     2
#define WAIT_FOR_OUT_STATUS     3
#define DATA_STATE_RECV         4

#define USB_FA			DADDR
#define USB_FA_ADDR_UPDATE	(1<<7)
#define USB_FA_FUNCTION_ADDR	(DADDR_EF - 1)

#endif
