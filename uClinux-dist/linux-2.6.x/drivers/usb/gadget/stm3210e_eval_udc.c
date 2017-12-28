/*
 * linux/drivers/usb/gadget/stm32f10x_udc.c
 * Sharp str91x on-chip full speed USB device controllers
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

#include <linux/platform_device.h>

#include "stm3210e_eval_udc.h"

//#define DEBUG printk
//#define DEBUG_EP0 printk
//#define DEBUG_SETUP printk

#ifndef DEBUG_EP0
# define DEBUG_EP0(fmt,args...)
#endif
#ifndef DEBUG_SETUP
# define DEBUG_SETUP(fmt,args...)
#endif
#ifndef DEBUG
# define NO_STATES
# define DEBUG(fmt,args...)
#endif

#define	DRIVER_DESC			"STM32F10x USB Device Controller"
#define	DRIVER_VERSION		__DATE__

#ifndef _BIT			/* FIXME - what happended to _BIT in 2.6.7bk18? */
#define _BIT(x) (1<<(x))
#endif

struct stm32f10x_udc *the_controller = NULL;

static const char driver_name[] = "stm32f10x_udc";
static const char driver_desc[] = DRIVER_DESC;
static const char ep0name[] = "ep0-control";

/*
  Local definintions.
*/

#ifndef NO_STATES
static char *state_names[] = {
	"WAIT_SETUP",
	"DATA_STATE_XMIT",
	"DATA_STATE_NEED_ZLP",
	"WAIT_FOR_OUT_STATUS",
	"DATA_STATE_RECV"
};
#endif


/****************************************************************************************
*	Private USB Conf Functions							*
****************************************************************************************/
/*******************************************************************************
* Function Name  : Set_System
* Description    : Configures system clocks & power
* Input          : None.
* Return         : None.
*******************************************************************************/
void Set_System(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  /* Enable USB_DISCONNECT GPIO clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIO_DISCONNECT, ENABLE);

  /* Configure USB pull-up pin */
  GPIO_InitStructure.GPIO_Pin = USB_DISCONNECT_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
  GPIO_Init(USB_DISCONNECT, &GPIO_InitStructure);
}

/*******************************************************************************
* Function Name  : Set_USBClock
* Description    : Configures USB Clock input (48MHz)
* Input          : None.
* Return         : None.
*******************************************************************************/
void Set_USBClock(void)
{
  /* USBCLK = PLLCLK / 1.5 */
  RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);
  /* Enable USB clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

/*******************************************************************************
* Function Name  : USB_Interrupts_Config
* Description    : Configures the USB interrupts
* Input          : None.
* Return         : None.
*******************************************************************************/
void USB_Interrupts_Config(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

  NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

/*******************************************************************************
* Function Name  : USB_Cable_Config
* Description    : Software Connection/Disconnection of USB Cable
* Input          : None.
* Return         : Status
*******************************************************************************/
void USB_Cable_Config (FunctionalState NewState)
{
  if (NewState != DISABLE)
  {
    GPIO_ResetBits(USB_DISCONNECT, USB_DISCONNECT_PIN);
  }
  else
  {
    GPIO_SetBits(USB_DISCONNECT, USB_DISCONNECT_PIN);
  }
}

/**************************** END OF Private USB Conf Functions *****************/


/*
  Local declarations.
*/
static int stm32f10x_ep_enable(struct usb_ep *ep,
			     const struct usb_endpoint_descriptor *);
static int stm32f10x_ep_disable(struct usb_ep *ep);
static struct usb_request *stm32f10x_alloc_request(struct usb_ep *ep, gfp_t);
static void stm32f10x_free_request(struct usb_ep *ep, struct usb_request *);

static int stm32f10x_queue(struct usb_ep *ep, struct usb_request *, gfp_t);
static int stm32f10x_dequeue(struct usb_ep *ep, struct usb_request *);
static int stm32f10x_set_halt(struct usb_ep *ep, int);
static int stm32f10x_fifo_status(struct usb_ep *ep);
static int stm32f10x_fifo_status(struct usb_ep *ep);
static void stm32f10x_fifo_flush(struct usb_ep *ep);
static void stm32f10x_ep0_kick(struct stm32f10x_udc *dev, struct stm32f10x_ep *ep);
static void stm32f10x_handle_ep0(struct stm32f10x_udc *dev, u32 intr, u32 csr);

static void done(struct stm32f10x_ep *ep, struct stm32f10x_request *req,
		 int status);
static void pio_irq_enable(int bEndpointAddress);
static void pio_irq_disable(int bEndpointAddress);
static void stop_activity(struct stm32f10x_udc *dev,
			  struct usb_gadget_driver *driver);
static void flush(struct stm32f10x_ep *ep);
static void udc_enable(struct stm32f10x_udc *dev);
static void udc_set_address(struct stm32f10x_udc *dev, unsigned char address);

/*-------------------------------------------------------------------------*/

static struct usb_ep_ops stm32f10x_ep_ops = {
	.enable = stm32f10x_ep_enable,
	.disable = stm32f10x_ep_disable,

	.alloc_request = stm32f10x_alloc_request,
	.free_request = stm32f10x_free_request,

	.queue = stm32f10x_queue,
	.dequeue = stm32f10x_dequeue,

	.set_halt = stm32f10x_set_halt,
	.fifo_status = stm32f10x_fifo_status,
	.fifo_flush = stm32f10x_fifo_flush,
};

/* Inline code */

static __inline__ int write_packet(struct stm32f10x_ep *ep,
				   struct stm32f10x_request *req, int max)
{
	int length, count;
	int i = 0;
	u8 *fifo = ( u8 *)ep->fifo, *buf = NULL;
	unsigned long *d = NULL,*s = NULL;

	buf = req->req.buf + req->req.actual;

	length = req->req.length - req->req.actual;
	length = min(length, max);
	req->req.actual += length;

	DEBUG("Write %d (max %d), fifo %p\n", length, max, fifo);

	count = length;
	i = 0;

	d = (unsigned long *)fifo;
	s = (unsigned long *)buf;
	while(count >= 0)
	{
		*d++ = *s++;
		count -= 4;
	}
	
	return length;
}

static __inline__ u32 usb_read(u32 port)
{
	return *(volatile u32 *)(port);
}

static __inline__ void usb_write(u32 val, u32 port)
{
	*(volatile u32 *)(port) = val;
}

static __inline__ void usb_set(u32 val, u32 port)
{
	volatile u32 *ioport = (volatile u32 *)(port);
	u32 after = (*ioport) | val;
	*ioport = after;
}

static __inline__ void usb_clear(u32 val, u32 port)
{
	volatile u32 *ioport = (volatile u32 *)(port);
	u32 after = (*ioport) & ~val;
	*ioport = after;
}

#ifdef CONFIG_USB_GADGET_DEBUG_FILES

static const char proc_node_name[] = "driver/udc";

static int
udc_proc_read(char *page, char **start, off_t off, int count,
	      int *eof, void *_dev)
{
	char *buf = page;
	struct stm32f10x_udc *dev = _dev;
	char *next = buf;
	unsigned size = count;
	unsigned long flags;
	int t;

	if (off != 0)
		return 0;

	local_irq_save(flags);

	/* basic device status */
	t = scnprintf(next, size,
		      DRIVER_DESC "\n"
		      "%s version: %s\n"
		      "Gadget driver: %s\n"
		      "Host: %s\n\n",
		      driver_name, DRIVER_VERSION,
		      dev->driver ? dev->driver->driver.name : "(none)",
		       "full speed");
	size -= t;
	next += t;

	local_irq_restore(flags);
	*eof = 1;
	return count - size;
}

#define create_proc_files() 	create_proc_read_entry(proc_node_name, 0, NULL, udc_proc_read, dev)
#define remove_proc_files() 	remove_proc_entry(proc_node_name, NULL)

#else	/* !CONFIG_USB_GADGET_DEBUG_FILES */

#define create_proc_files() do {} while (0)
#define remove_proc_files() do {} while (0)

#endif	/* CONFIG_USB_GADGET_DEBUG_FILES */

/*
 * 	udc_disable - disable USB device controller
 */
static void udc_disable(struct stm32f10x_udc *dev)
{
	DEBUG("%s, %p\n", __FUNCTION__, dev);

	udc_set_address(dev, 0);

     /* disable all ints and force USB reset */
     _SetCNTR(USB_CNTR_FRES);
     /* clear interrupt status register */
     _SetISTR(0);
     /* Disable the Pull-Up*/
     USB_Cable_Config(DISABLE);
    /* switch-off device */
    _SetCNTR(USB_CNTR_FRES+USB_CNTR_PDWN);
    /* sw variables reset */
	/* ... */


	/* if hardware supports it, disconnect from usb */
	/* make_usb_disappear(); */

	dev->ep0state = WAIT_FOR_SETUP;
	dev->gadget.speed = USB_SPEED_UNKNOWN;
	dev->usb_address = 0;
}

/*
 * 	udc_reinit - initialize software state
 */
static void udc_reinit(struct stm32f10x_udc *dev)
{
	u32 i;

	DEBUG("%s, %p\n", __FUNCTION__, dev);

	SetBTABLE(BTABLE_ADDRESS);
	/* Set this device to response on default address */
	udc_set_address(dev, 0);

	/* device/ep0 records init */
	INIT_LIST_HEAD(&dev->gadget.ep_list);
	INIT_LIST_HEAD(&dev->gadget.ep0->ep_list);
	dev->ep0state = WAIT_FOR_SETUP;

	/* basic endpoint records init */
	for (i = 0; i < UDC_MAX_ENDPOINTS; i++) {
		struct stm32f10x_ep *ep = &dev->ep[i];

		if (i != 0)
			list_add_tail(&ep->ep.ep_list, &dev->gadget.ep_list);

		ep->desc = 0;
		ep->stopped = 0;
		INIT_LIST_HEAD(&ep->queue);
		ep->pio_irqs = 0;
	}

	/* the rest was statically initialized, and is read-only */
}

#define BYTES2MAXP(x)	(x / 8)
#define MAXP2BYTES(x)	(x * 8)

/* until it's enabled, this UDC should be completely invisible
 * to any USB host.
 */
static void udc_enable(struct stm32f10x_udc *dev)
{
	int ep;

	DEBUG("%s, %p\n", __FUNCTION__, dev);

	dev->gadget.speed = USB_SPEED_UNKNOWN;


/*** cable plugged-in ? ***/
        USB_Cable_Config(ENABLE);
		
	/*** USB_CNTR_PWDN = 0 ***/

	/*** USB_CNTR_FRES = 0 ***/
    	_SetCNTR(0);
	/*** Clear pending interrupts ***/

	

	SetBTABLE(BTABLE_ADDRESS);
	/* Set MAXP values for each */
	
	/* Set this device to response on default address */
	_SetDADDR(0x00|DADDR_EF); /* set device address and enable function */
	
	/*** USB_CNTR_FRES = 0 ***/

	/*** Clear pending interrupts ***/
    	usb_write(0,USB_ISTR);
	/*** Set interrupt mask ***/
	_SetCNTR(IMR_MSK);
}

/*
  Register entry point for the peripheral controller driver.
*/
int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	struct stm32f10x_udc *dev = the_controller;
	int retval;

	DEBUG("%s: %s\n", __FUNCTION__, driver->driver.name);

	if (!driver
	    || driver->speed != USB_SPEED_FULL
	    || !driver->bind
	    || !driver->unbind || !driver->disconnect || !driver->setup)
	{
		DEBUG("early error 1\n");
		return -EINVAL;
	}
	if (!dev)
	{
		DEBUG("early error 2\n");
		return -ENODEV;
	}
	if (dev->driver)
	{
		DEBUG("early error 3\n");
		return -EBUSY;
	}

	/* first hook up the driver ... */
	dev->driver = driver;
	dev->gadget.dev.driver = &driver->driver;
	
	device_add(&dev->gadget.dev);
	DEBUG("device_add(&dev->gadget.dev) OK\n");
	retval = driver->bind(&dev->gadget);
	DEBUG("driver->bind(&dev->gadget)\n");
	if (retval) {
		printk("%s: bind to driver %s --> error %d\n", dev->gadget.name,
		       driver->driver.name, retval);
		device_del(&dev->gadget.dev);

		dev->driver = 0;
		dev->gadget.dev.driver = 0;
		return retval;
	}

	/* ... then enable host detection and ep0; and we're ready
	 * for set_configuration as well as eventual disconnect.
	 * NOTE:  this shouldn't power up until later.
	 */
	printk("%s: registered gadget driver '%s'\n", dev->gadget.name,
	       driver->driver.name);

	udc_enable(dev);

	return 0;
}

EXPORT_SYMBOL(usb_gadget_register_driver);

/*
  Unregister entry point for the peripheral controller driver.
*/
int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct stm32f10x_udc *dev = the_controller;
	unsigned long flags;

	if (!dev)
		return -ENODEV;
	if (!driver || driver != dev->driver)
		return -EINVAL;

	spin_lock_irqsave(&dev->lock, flags);
	dev->driver = 0;
	stop_activity(dev, driver);
	spin_unlock_irqrestore(&dev->lock, flags);

	driver->unbind(&dev->gadget);
	device_del(&dev->gadget.dev);

	udc_disable(dev);

	DEBUG("unregistered gadget driver '%s'\n", driver->driver.name);
	return 0;
}

EXPORT_SYMBOL(usb_gadget_unregister_driver);

/*-------------------------------------------------------------------------*/

/** Write request to FIFO (max write == maxp size)
 *  Return:  0 = still running, 1 = completed, negative = errno
 *  NOTE: INDEX register must be set for EP
 */
static int write_fifo(struct stm32f10x_ep *ep, struct stm32f10x_request *req)
{
	u32 max;
	unsigned count;
	int is_last, is_short;

	max = le16_to_cpu(ep->desc->wMaxPacketSize);

		count = write_packet(ep, req, max);
	    SetEPTxCount(ep_index(ep),count);
	    SetEPTxStatus(ep_index(ep), EP_TX_VALID);
		mdelay(2);
		/* last packet is usually short (or a zlp) */
		if (unlikely(count != max))
			is_last = is_short = 1;
		else {
			if (likely(req->req.length != req->req.actual)
			    || req->req.zero)
				is_last = 0;
			else
				is_last = 1;
			/* interrupt/iso maxpacket may not fill the fifo */
			is_short = unlikely(max < ep_maxpacket(ep));
		}

		DEBUG("%s: wrote %s %d bytes%s%s %d left %p\n", __FUNCTION__,
		      ep->ep.name, count,
		      is_last ? "/L" : "", is_short ? "/S" : "",
		      req->req.length - req->req.actual, req);

		/* requests complete when all IN data is in the FIFO */
		if (is_last) {
			done(ep, req, 0);
			if (list_empty(&ep->queue)) {
				pio_irq_disable(ep_index(ep));
			}
			return 1;
		}
	return 0;
} 


/** Read to request from FIFO (max read == bytes in fifo)
 *  Return:  0 = still running, 1 = completed, negative = errno
 *  NOTE: INDEX register must be set for EP
 */
static int read_fifo(struct stm32f10x_ep *ep, struct stm32f10x_request *req)
{
	u32 csr;
	u8 *buf;
	unsigned bufferspace, count, is_short;
	volatile u32 *fifo = (volatile u32 *)ep->fifo;

	/* make sure there's a packet in the FIFO. */

	buf = req->req.buf + req->req.actual;
	prefetchw(buf);
	bufferspace = req->req.length - req->req.actual;

	/* read all bytes from this packet */
	count = GetEPRxCount(ENDP2);
	req->req.actual += min(count, bufferspace);

	is_short = (count < ep->ep.maxpacket);
	DEBUG("read %s %02x, %d bytes%s req %p %d/%d\n",
	      ep->ep.name, csr, count,
	      is_short ? "/S" : "", req, req->req.actual, req->req.length);

	while (likely(count-- != 0)) {
		u8 byte = (u8) (*fifo & 0xff);

		if (unlikely(bufferspace == 0)) {
			/* this happens when the driver's buffer
			 * is smaller than what the host sent.
			 * discard the extra data.
			 */
			if (req->req.status != -EOVERFLOW)
				printk("%s overflow %d\n", ep->ep.name, count);
			req->req.status = -EOVERFLOW;
		} else {
			*buf++ = byte;
			bufferspace--;
		}
	}

	/* completion */
	if (is_short || req->req.actual == req->req.length) {
		done(ep, req, 0);
		
		if (list_empty(&ep->queue))
			pio_irq_disable(ep_index(ep));
		return 1;
	}

	/* finished that packet.  the next one may be waiting... */
	return 0;
}

/*
 *	done - retire a request; caller blocked irqs
 *  INDEX register is preserved to keep same
 */
static void done(struct stm32f10x_ep *ep, struct stm32f10x_request *req, int status)
{
	unsigned int stopped = ep->stopped;
	u32 index;

	DEBUG("%s, %p\n", __FUNCTION__, ep);
	list_del_init(&req->queue);

	if (likely(req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	if (status && status != -ESHUTDOWN)
		DEBUG("complete %s req %p stat %d len %u/%u\n",
		      ep->ep.name, &req->req, status,
		      req->req.actual, req->req.length);

	/* don't modify queue heads during completion callback */
	ep->stopped = 1;
	
	spin_unlock(&ep->dev->lock);
	req->req.complete(&ep->ep, &req->req);
	spin_lock(&ep->dev->lock);

	/* Restore index */
	ep->stopped = stopped;
}

/** Enable EP interrupt */
static void pio_irq_enable(int ep)
{
	DEBUG("%s: %d\n", __FUNCTION__, ep);
}

/** Disable EP interrupt */
static void pio_irq_disable(int ep)
{

	DEBUG("%s: %d\n", __FUNCTION__, ep);
}

/*
 * 	nuke - dequeue ALL requests
 */
void nuke(struct stm32f10x_ep *ep, int status)
{

	struct stm32f10x_request *req;

	DEBUG("%s, %p\n", __FUNCTION__, ep);
	/* Flush FIFO */
	flush(ep);

	/* called with irqs blocked */
	while (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct stm32f10x_request, queue);
		done(ep, req, status);
	}

	/* Disable IRQ if EP is enabled (has descriptor) */
	if (ep->desc)
		pio_irq_disable(ep_index(ep));

}

/** Flush EP
 * NOTE: INDEX register must be set before this call
 */
static void flush(struct stm32f10x_ep *ep)
{

}

/**
 * stm32f10x_in_epn - handle IN interrupt
 */
 unsigned char USB_ENUM_DONE = 0;
static void stm32f10x_in_epn(struct stm32f10x_udc *dev, u32 ep_idx)
{
	u32 csr;
	struct stm32f10x_ep *ep = &dev->ep[ep_idx];
	struct stm32f10x_request *req;
	USB_ENUM_DONE = 1;
	csr = _GetENDPOINT(ep_idx);
	DEBUG("%s: %d, csr %x\n", __FUNCTION__, ep_idx, csr);
	
	    if((csr & EP_CTR_TX) != 0) // EP1/3 Correct Tx to HOST IN
	    {
	        /* clear int flag */
		_ClearEP_CTR_TX(ep_idx);
			/* call IN service function */
		if (!ep->desc) {
			DEBUG("%s: NO EP DESC\n", __FUNCTION__);
			return;
		}

		if (list_empty(&ep->queue))
			req = 0;
		else
			req = list_entry(ep->queue.next, struct stm32f10x_request, queue);

		DEBUG("req: %p\n", req);

		SetEPTxCount(ep_idx,0);
		SetEPTxStatus(ep_idx, EP_TX_VALID);
		if (!req)
		{
		return;
		}

		write_fifo(ep, req);
		
		
	    } /* if((wEPVal & EP_CTR_TX) != 0) */
}

/* ********************************************************************************************* */
/* Bulk OUT (recv)
 */

static void stm32f10x_out_epn(struct stm32f10x_udc *dev, u32 ep_idx, u32 intr)
{
	struct stm32f10x_ep *ep = &dev->ep[ep_idx];
	struct stm32f10x_request *req;

	DEBUG("%s: %d\n", __FUNCTION__, ep_idx);

		_ClearEP_CTR_RX(ep_idx);

	if (ep->desc) {
		u32 csr = _GetENDPOINT(ep_idx);;


		if (GetEPRxStatus(ep_idx) & EP_RX_STALL)
		{
				flush(ep);
				SetEPRxStatus(ep_idx, EP_RX_VALID);
		
		} else{
				if (list_empty(&ep->queue))
					req = 0;
				else
					req =
					    list_entry(ep->queue.next,
						       struct stm32f10x_request,
						       queue);

				if (!req) {
					printk("%s: NULL REQ %d\n",
					       __FUNCTION__, ep_idx);
					flush(ep);
					//break;
				} else {
					read_fifo(ep, req);
					SetEPRxStatus(ep_idx, EP_RX_VALID);
				}
			SetEPRxStatus(ep_idx, EP_RX_VALID);
			}


	} else {
		/* Throw packet away.. */
		printk("%s: No descriptor?!?\n", __FUNCTION__);
		flush(ep);
		SetEPRxStatus(ep_idx, EP_RX_VALID);
		
	}
}

static void stop_activity(struct stm32f10x_udc *dev,
			  struct usb_gadget_driver *driver)
{
	int i;

	/* don't disconnect drivers more than once */
	if (dev->gadget.speed == USB_SPEED_UNKNOWN)
		driver = 0;
	dev->gadget.speed = USB_SPEED_UNKNOWN;

	/* prevent new request submissions, kill any outstanding requests  */
	for (i = 0; i < UDC_MAX_ENDPOINTS; i++) {
		struct stm32f10x_ep *ep = &dev->ep[i];
		ep->stopped = 1;

		nuke(ep, -ESHUTDOWN);
	}

	/* report disconnect; the driver is already quiesced */
	if (driver) {
		spin_unlock(&dev->lock);
		driver->disconnect(&dev->gadget);
		spin_lock(&dev->lock);
	}

	/* re-init driver-visible data structures */
	udc_reinit(dev);
}

/** Handle USB RESET interrupt
 */
static void stm32f10x_reset_intr(struct stm32f10x_udc *dev)
{
	int ep;
	/* Does not work always... */

	DEBUG("%s: %d\n", __FUNCTION__, dev->usb_address);

	for (ep = 0; ep < UDC_MAX_ENDPOINTS; ep++) {
		struct stm32f10x_ep *ep_reg = &dev->ep[ep];
		u32 csr;

		switch (ep_reg->ep_type) {
		case ep_bulk_in:
				/* Initialize Endpoint 1 */
				SetEPType(ENDP1, EP_BULK);
				SetEPTxStatus(ENDP1, EP_TX_NAK);
				SetEPTxAddr(ENDP1, ENDP1_TXADDR);
		        	SetEPRxStatus(ENDP1, EP_RX_DIS);
				_SetEPAddress((u8)ep, (u8)ep);
					
				break;
			
		case ep_interrupt:
				/* Initialize Endpoint 3 */
				SetEPType(ENDP3, EP_INTERRUPT);
				SetEPTxAddr(ENDP3, ENDP3_TXADDR);
				SetEPTxStatus(ENDP3, EP_TX_NAK);
		        	SetEPRxStatus(ENDP3, EP_RX_DIS);
				_SetEPAddress((u8)ep, (u8)ep);
				break;			
			
		case ep_control:
				/* Initialize Endpoint 0 */
				SetEPType(ENDP0, EP_CONTROL);
				SetEPTxStatus(ENDP0, EP_TX_NAK);
				SetEPRxAddr(ENDP0, ENDP0_RXADDR);
				SetEPRxCount(ENDP0, 0x40);
				SetEPTxAddr(ENDP0, ENDP0_TXADDR);
				SetEPTxCount(ENDP0, 0x08);
				Clear_Status_Out(ENDP0);
				_SetEPAddress((u8)ep, (u8)ep);
				SetEPRxValid(ENDP0);
				printk("setup control EP done\n");
				break;
		case ep_bulk_out:
			        /* Initialize Endpoint 2 */
			        SetEPType(ENDP2, EP_BULK);
			        SetEPRxAddr(ENDP2, ENDP2_RXADDR);
			        SetEPRxCount(ENDP2, 0x40);
			        SetEPRxStatus(ENDP2, EP_RX_VALID);
			        SetEPTxStatus(ENDP2, EP_TX_DIS);
				_SetEPAddress((u8)ep, (u8)ep);
				break;
		}

		flush(ep_reg);
		nuke(ep_reg, 0);
	}

	dev->ep0state = WAIT_FOR_SETUP;
	/* Set Device ID to 0 */
	udc_set_address(dev, 0);


	/* Re-enable UDC */
	udc_enable(dev);
	
	dev->gadget.speed = USB_SPEED_FULL;
}




/*
 *	stm32f10x usb client interrupt handler.
 */
u16 SaveRState;
u16 SaveTState;
#define vSetEPRxStatus(st)	(SaveRState = st)
#define vSetEPTxStatus(st)	(SaveTState = st)

static irqreturn_t stm32f10x_udc_irq(int irq, void *_dev)
{
	struct stm32f10x_udc *dev = _dev;

	unsigned char EPindex;
	volatile unsigned short  wIstr;//, wUSB_CNTR;  /* USB_ISTR register last read value */
	u32 csr;


	spin_lock(&dev->lock);
	for(;;)
	{
		wIstr = _GetISTR();
		if(!(wIstr & (USB_ISTR_RESET | USB_ISTR_SUSP | USB_ISTR_WKUP | USB_ISTR_ESOF | USB_ISTR_CTR) ))
		{
			break;
		}

	    	usb_write(0,USB_ISTR);

		if (wIstr & USB_ISTR_RESET )
		{
			_SetISTR((u16)CLR_RESET);
		
			stm32f10x_reset_intr(dev);
		}
		if(wIstr & USB_ISTR_SUSP)
		{
			_SetISTR((u16)CLR_SUSP);
			
		}
		if(wIstr & USB_ISTR_WKUP)
		{
	 		_SetISTR((u16)CLR_WKUP);
					if (dev->gadget.speed != USB_SPEED_UNKNOWN
					    && dev->driver
					    && dev->driver->resume) {
						dev->driver->resume(&dev->gadget);
					}
			
		}
	if(wIstr & USB_ISTR_ESOF)
		{
		   _SetISTR((u16)CLR_ESOF);
		}
		
	if(((wIstr) & USB_ISTR_CTR)!= 0)  // USB_ISTR_CTR 0x8000 Correct Transfer
	{
		_SetCNTR(USB_CNTR_RESETM);
		EPindex = (u8)(wIstr & USB_ISTR_EP_ID);
		if ((wIstr & USB_ISTR_DIR) == 0) {	/*IN*/
			_SetISTR((u16)CLR_CTR); /* clear CTR flag */

			if (EPindex == 1) {
				DEBUG("USB_IN_INT_EP1\n");	
				stm32f10x_in_epn(dev, 1);
			}
			if (EPindex == 0) {
				csr = _GetENDPOINT(ENDP0);
				DEBUG("USB_IN_INT_EP0 (control)\n");
				stm32f10x_handle_ep0(dev,wIstr,csr);
				SetEPTxStatus(ENDP0, EP_TX_NAK);
				SetEPRxStatus(ENDP0,EP_RX_VALID);
			}			
			if(EPindex == 3) {
				DEBUG("USB_IN_INT_EP3\n");
				stm32f10x_in_epn(dev, 3);
			}		
				
		}

		else if ((wIstr & USB_ISTR_DIR) != 0)  {	/*OUT*/
			_SetISTR((u16)CLR_CTR); /* clear CTR flag */

			if (EPindex == 2) {
				DEBUG("USB_OUT_INT_EP2\n");
//				printk("USB_OUT_INT_EP2\n");
				stm32f10x_out_epn(dev, 2, wIstr);
			}
			if (EPindex == 0) {
				csr = _GetENDPOINT(ENDP0);
				DEBUG("USB_OUT_INT_EP0 (control)\n");
				stm32f10x_handle_ep0(dev,wIstr,csr);
			}
			
		}

	}

	}
	spin_unlock(&dev->lock);

	return IRQ_HANDLED;
}

static int stm32f10x_ep_enable(struct usb_ep *_ep,
			     const struct usb_endpoint_descriptor *desc)
{
	struct stm32f10x_ep *ep;
	struct stm32f10x_udc *dev;
	unsigned long flags;

	DEBUG("%s, %p\n", __FUNCTION__, _ep);

	ep = container_of(_ep, struct stm32f10x_ep, ep);
	if (!_ep || !desc || ep->desc || _ep->name == ep0name
	    || desc->bDescriptorType != USB_DT_ENDPOINT
	    || ep->bEndpointAddress != desc->bEndpointAddress
	    || ep_maxpacket(ep) < le16_to_cpu(desc->wMaxPacketSize)) {
		DEBUG("%s, bad ep or descriptor\n", __FUNCTION__);
		return -EINVAL;
	}

	/* xfer types must match, except that interrupt ~= bulk */
	if (ep->bmAttributes != desc->bmAttributes
	    && ep->bmAttributes != USB_ENDPOINT_XFER_BULK
	    && desc->bmAttributes != USB_ENDPOINT_XFER_INT) {
		DEBUG("%s, %s type mismatch\n", __FUNCTION__, _ep->name);
		return -EINVAL;
	}

	/* hardware _could_ do smaller, but driver doesn't */
	if ((desc->bmAttributes == USB_ENDPOINT_XFER_BULK
	     && le16_to_cpu(desc->wMaxPacketSize) != ep_maxpacket(ep))
	    || !desc->wMaxPacketSize) {
		DEBUG("%s, bad %s maxpacket\n", __FUNCTION__, _ep->name);
		return -ERANGE;
	}

	dev = ep->dev;
	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN) {
		DEBUG("%s, bogus device state\n", __FUNCTION__);
		return -ESHUTDOWN;
	}

	spin_lock_irqsave(&ep->dev->lock, flags);

	ep->stopped = 0;
	ep->desc = desc;
	ep->pio_irqs = 0;
	ep->ep.maxpacket = le16_to_cpu(desc->wMaxPacketSize);

	spin_unlock_irqrestore(&ep->dev->lock, flags);

	/* Reset halt state (does flush) */
	stm32f10x_set_halt(_ep, 0);

	DEBUG("%s: enabled %s\n", __FUNCTION__, _ep->name);
	return 0;
}

/** Disable EP
 *  NOTE: Sets INDEX register
 */
static int stm32f10x_ep_disable(struct usb_ep *_ep)
{
	struct stm32f10x_ep *ep;
	unsigned long flags;

	DEBUG("%s, %p\n", __FUNCTION__, _ep);

	ep = container_of(_ep, struct stm32f10x_ep, ep);
	if (!_ep || !ep->desc) {
		DEBUG("%s, %s not enabled\n", __FUNCTION__,
		      _ep ? ep->ep.name : NULL);
		return -EINVAL;
	}

	spin_lock_irqsave(&ep->dev->lock, flags);

//	usb_set_index(ep_index(ep));

	/* Nuke all pending requests (does flush) */
	nuke(ep, -ESHUTDOWN);

	/* Disable ep IRQ */
	pio_irq_disable(ep_index(ep));

	ep->desc = 0;
	ep->stopped = 1;

	spin_unlock_irqrestore(&ep->dev->lock, flags);

	DEBUG("%s: disabled %s\n", __FUNCTION__, _ep->name);
	return 0;
}

static struct usb_request *stm32f10x_alloc_request(struct usb_ep *ep,
						 gfp_t gfp_flags)
{
	struct stm32f10x_request *req;

	DEBUG("%s, %p\n", __FUNCTION__, ep);

	req = kmalloc(sizeof *req, gfp_flags);
	if (!req)
		return 0;

	memset(req, 0, sizeof *req);
	INIT_LIST_HEAD(&req->queue);

	return &req->req;
}

static void stm32f10x_free_request(struct usb_ep *ep, struct usb_request *_req)
{
	struct stm32f10x_request *req;

	DEBUG("%s, %p\n", __FUNCTION__, ep);

	req = container_of(_req, struct stm32f10x_request, req);
	WARN_ON(!list_empty(&req->queue));
	kfree(req);
}

/** Queue one request
 *  Kickstart transfer if needed
 *  NOTE: Sets INDEX register
 */
static int stm32f10x_queue(struct usb_ep *_ep, struct usb_request *_req,
			 gfp_t gfp_flags)
{
	struct stm32f10x_request *req;
	struct stm32f10x_ep *ep;
	struct stm32f10x_udc *dev;
	unsigned long flags;

	DEBUG("\n\n\n%s, %p\n", __FUNCTION__, _ep);

	req = container_of(_req, struct stm32f10x_request, req);
	if (unlikely
	    (!_req || !_req->complete || !_req->buf
	     || !list_empty(&req->queue))) {
		DEBUG("%s, bad params\n", __FUNCTION__);
		return -EINVAL;
	}

	ep = container_of(_ep, struct stm32f10x_ep, ep);
	if (unlikely(!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		DEBUG("%s, bad ep\n", __FUNCTION__);
		return -EINVAL;
	}

	dev = ep->dev;
	if (unlikely(!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN)) {
		DEBUG("%s, bogus device state %p\n", __FUNCTION__, dev->driver);
		return -ESHUTDOWN;
	}

	DEBUG("%s queue req %p, len %d buf %p\n", _ep->name, _req, _req->length,
	      _req->buf);

	spin_lock_irqsave(&dev->lock, flags);

	_req->status = -EINPROGRESS;
	_req->actual = 0;

	/* kickstart this i/o queue? */
	DEBUG("Add to %d Q %d %d\n", ep_index(ep), list_empty(&ep->queue),
	      ep->stopped);
	if (list_empty(&ep->queue) && likely(!ep->stopped)) {
		u32 csr;

		if (unlikely(ep_index(ep) == 0)) {
			/* EP0 */
			list_add_tail(&req->queue, &ep->queue);
			stm32f10x_ep0_kick(dev, ep);
			req = 0;
		} else if (ep_is_in(ep)) {
			/* EP1 & EP3 */
			pio_irq_enable(ep_index(ep));

				if (write_fifo(ep, req) == 1)
					req = 0;
			SetEPTxCount(ep_index(ep),0);
			SetEPTxStatus(ep_index(ep), EP_TX_VALID);
				

		} else {
			/* EP2 */

			pio_irq_enable(ep_index(ep));
			if (GetEPRxCount(ep_index(ep))) {
				if (read_fifo(ep, req) == 1)
					req = 0;
			SetEPRxCount(ep_index(ep),0);
			SetEPRxStatus(ep_index(ep), EP_RX_VALID);
			}
		}
	}

	/* pio or dma irq handler advances the queue. */
	if (likely(req != 0))
		list_add_tail(&req->queue, &ep->queue);

	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

/* dequeue JUST ONE request */
static int stm32f10x_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct stm32f10x_ep *ep;
	struct stm32f10x_request *req;
	unsigned long flags;

	DEBUG("%s, %p\n", __FUNCTION__, _ep);

	ep = container_of(_ep, struct stm32f10x_ep, ep);
	if (!_ep || ep->ep.name == ep0name)
		return -EINVAL;

	spin_lock_irqsave(&ep->dev->lock, flags);

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req)
			break;
	}
	if (&req->req != _req) {
		spin_unlock_irqrestore(&ep->dev->lock, flags);
		return -EINVAL;
	}

	done(ep, req, -ECONNRESET);

	spin_unlock_irqrestore(&ep->dev->lock, flags);
	return 0;
}

/** Halt specific EP
 *  Return 0 if success
 *  NOTE: Sets INDEX register to EP !
 */
static int stm32f10x_set_halt(struct usb_ep *_ep, int value)
{
	struct stm32f10x_ep *ep;
	unsigned long flags;

	ep = container_of(_ep, struct stm32f10x_ep, ep);
	if (unlikely(!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		DEBUG("%s, bad ep\n", __FUNCTION__);
		return -EINVAL;
	}


	DEBUG("%s, ep %d, val %d\n", __FUNCTION__, ep_index(ep), value);

	spin_lock_irqsave(&ep->dev->lock, flags);

	if (ep_index(ep) == 0) {
		/* EP0 */
	SetEPRxStatus(ENDP0,EP_RX_DIS);
	SetEPTxStatus(ENDP0,EP_TX_DIS);	
	} else if (ep_is_in(ep)) {
		u32 csr = GetEPTxCount(ep_index(ep));
		if (value && ((csr)
			      || !list_empty(&ep->queue))) {
			/*
			 * Attempts to halt IN endpoints will fail (returning -EAGAIN)
			 * if any transfer requests are still queued, or if the controller
			 * FIFO still holds bytes that the host hasnt collected.
			 */
			spin_unlock_irqrestore(&ep->dev->lock, flags);
			DEBUG
			    ("Attempt to halt IN endpoint failed (returning -EAGAIN) %d %d\n",
			     (csr),
			     !list_empty(&ep->queue));
			return -EAGAIN;
		}
		flush(ep);
		if (value)
			//usb_set(USB_IN_CSR1_SEND_STALL, ep->csr1);
			SetEPTxStatus(ep_index(ep),EP_TX_DIS);	
		else {
			SetEPTxStatus(ep_index(ep),EP_TX_STALL);	

		}

	} else {

		flush(ep);
		if (value)
			SetEPRxStatus(ep_index(ep),EP_RX_DIS);	

		else 
			SetEPRxStatus(ep_index(ep),EP_RX_STALL);	

	}

	if (value) {
		ep->stopped = 1;
	} else {
		ep->stopped = 0;
	}

	spin_unlock_irqrestore(&ep->dev->lock, flags);

	DEBUG("%s %s halted\n", _ep->name, value == 0 ? "NOT" : "IS");

	return 0;
}

/** Return bytes in EP FIFO
 *  NOTE: Sets INDEX register to EP
 */
static int stm32f10x_fifo_status(struct usb_ep *_ep)
{
	u32 csr;
	int count = 0;
	struct stm32f10x_ep *ep;

	ep = container_of(_ep, struct stm32f10x_ep, ep);
	if (!_ep) {
		DEBUG("%s, bad ep\n", __FUNCTION__);
		return -ENODEV;
	}

	DEBUG("%s, %d\n", __FUNCTION__, ep_index(ep));

	/* LPD can't report unclaimed bytes from IN fifos */
	if (ep_is_in(ep))
		return -EOPNOTSUPP;

//	usb_set_index(ep_index(ep));

	csr = GetEPRxStatus(ep_index(ep));
	if (ep->dev->gadget.speed != USB_SPEED_UNKNOWN ||
	    csr & EP_RX_NAK) {
		count = GetEPRxCount(ep_index(ep));
	}

	return count;
}

/** Flush EP FIFO
 *  NOTE: Sets INDEX register to EP
 */
static void stm32f10x_fifo_flush(struct usb_ep *_ep)
{
	struct stm32f10x_ep *ep;

	ep = container_of(_ep, struct stm32f10x_ep, ep);
	if (unlikely(!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		DEBUG("%s, bad ep\n", __FUNCTION__);
		return;
	}

//	usb_set_index(ep_index(ep));
	flush(ep);
}

/****************************************************************/
/* End Point 0 related functions                                */
/****************************************************************/

/* return:  0 = still running, 1 = completed, negative = errno */
static int write_fifo_ep0(struct stm32f10x_ep *ep, struct stm32f10x_request *req)
{
	u32 max;
	unsigned count;
	int is_last;

	max = ep_maxpacket(ep);

	DEBUG_EP0("%s\n", __FUNCTION__);

	count = write_packet(ep, req, max);
	SetEPTxCount(ENDP0,count);
	SetEPTxStatus(ENDP0, EP_TX_VALID);
	mdelay(2);

	/* last packet is usually short (or a zlp) */
	if (unlikely(count != max))
		is_last = 1;
	else {
		if (likely(req->req.length != req->req.actual) || req->req.zero)
			is_last = 0;
		else
			is_last = 1;
	}

	DEBUG_EP0("%s: wrote %s %d bytes%s %d left %p\n", __FUNCTION__,
		  ep->ep.name, count,
		  is_last ? "/L" : "", req->req.length - req->req.actual, req);

	/* requests complete when all IN data is in the FIFO */
	if (is_last) {
		done(ep, req, 0);
		return 1;
	}

	return 0;
}

static __inline__ void stm32f10x_fifo_write(struct stm32f10x_ep *ep,
					  unsigned char *cp, int count)
{
	volatile u32 *fifo = (volatile u32 *)ep->fifo;
	DEBUG_EP0("fifo_write: %d %d\n", ep_index(ep), count);
	while (count--)
		*fifo = *cp++;
}

static int read_fifo_ep0(struct stm32f10x_ep *ep, struct stm32f10x_request *req)
{
	u32 csr;
	u8 *buf;
	unsigned bufferspace, count, is_short;
	volatile u32 *fifo =  (volatile u32 *)(GetEPRxAddr(ENDP0)+PMAAddr);

	DEBUG_EP0("%s\n", __FUNCTION__);

	buf = req->req.buf + req->req.actual;
	prefetchw(buf);
	bufferspace = req->req.length - req->req.actual;

	/* read all bytes from this packet */
		count = GetEPRxCount(ENDP0);
		req->req.actual += min(count, bufferspace);


	is_short = (count < ep->ep.maxpacket);
	DEBUG_EP0("read %s %02x, %d bytes%s req %p %d/%d\n",
		  ep->ep.name, csr, count,
		  is_short ? "/S" : "", req, req->req.actual, req->req.length);

	while (likely(count-- != 0)) {
		u8 byte = (u8) (*fifo & 0xff);

		if (unlikely(bufferspace == 0)) {
			/* this happens when the driver's buffer
			 * is smaller than what the host sent.
			 * discard the extra data.
			 */
			if (req->req.status != -EOVERFLOW)
				DEBUG_EP0("%s overflow %d\n", ep->ep.name,
					  count);
			req->req.status = -EOVERFLOW;
		} else {
			*buf++ = byte;
			bufferspace--;
		}
	}

	/* completion */
	if (is_short || req->req.actual == req->req.length) {
		done(ep, req, 0);
		return 1;
	}

	/* finished that packet.  the next one may be waiting... */
	return 0;
}

/**
 * udc_set_address - set the USB address for this device
 * @address:
 *
 * Called from control endpoint function after it decodes a set address setup packet.
 */
static void udc_set_address(struct stm32f10x_udc *dev, unsigned char address)
{
	DEBUG_EP0("%s: %d\n", __FUNCTION__, address);
	dev->usb_address = address;
	usb_set((address & USB_FA_FUNCTION_ADDR), USB_FA);
	usb_set(USB_FA_ADDR_UPDATE | (address & USB_FA_FUNCTION_ADDR), USB_FA);
	/* usb_read(USB_FA); */
}

/*
 * DATA_STATE_RECV (OUT_PKT_RDY)
 *      - if error
 *              set EP0_CLR_OUT | EP0_DATA_END | EP0_SEND_STALL bits
 *      - else
 *              set EP0_CLR_OUT bit
 				if last set EP0_DATA_END bit
 */
static void stm32f10x_ep0_out(struct stm32f10x_udc *dev, u32 csr)
{
	struct stm32f10x_request *req;
	struct stm32f10x_ep *ep = &dev->ep[0];
	int ret;

	DEBUG_EP0("%s: %x\n", __FUNCTION__, csr);

	if (list_empty(&ep->queue))
		req = 0;
	else
		req = list_entry(ep->queue.next, struct stm32f10x_request, queue);

	if (req) {

		if (req->req.length == 0) {
			DEBUG_EP0("ZERO LENGTH OUT!\n");
			SetEPRxStatus(ENDP0,EP_RX_VALID);
			dev->ep0state = WAIT_FOR_SETUP;
			return;
		}
		ret = read_fifo_ep0(ep, req);
		if (ret) {
			/* Done! */
			DEBUG_EP0("%s: finished, waiting for status\n",
				  __FUNCTION__);

			dev->ep0state = WAIT_FOR_SETUP;
		} else {
			/* Not done yet.. */
			DEBUG_EP0("%s: not finished\n", __FUNCTION__);
			SetEPRxStatus(ENDP0, EP_RX_VALID);

		}
	} else {
		DEBUG_EP0("NO REQ??!\n");
	}
}

/*
 * DATA_STATE_XMIT
 */
static int stm32f10x_ep0_in(struct stm32f10x_udc *dev, u32 csr)
{
	struct stm32f10x_request *req;
	struct stm32f10x_ep *ep = &dev->ep[0];
	int ret, need_zlp = 0;

	DEBUG_EP0("%s: %x\n", __FUNCTION__, csr);

	if (list_empty(&ep->queue))
		req = 0;
	else
		req = list_entry(ep->queue.next, struct stm32f10x_request, queue);

	if (!req) {
		DEBUG_EP0("%s: NULL REQ\n", __FUNCTION__);
		return 0;
	}

	if (req->req.length == 0) {

		vSetEPTxStatus(EP_TX_STALL);
		dev->ep0state = WAIT_FOR_SETUP;
		return 1;
	}

	if (req->req.length - req->req.actual == EP0_PACKETSIZE) {
		/* Next write will end with the packet size, */
		/* so we need Zero-length-packet */
		need_zlp = 1;
	}

	ret = write_fifo_ep0(ep, req);

	if (ret == 1 && !need_zlp) {
		/* Last packet */
		DEBUG_EP0("%s: finished, waiting for status\n", __FUNCTION__);

		dev->ep0state = WAIT_FOR_SETUP;
	} else {
		DEBUG_EP0("%s: not finished\n", __FUNCTION__);

	}

	if (need_zlp) {
		DEBUG_EP0("%s: Need ZLP!\n", __FUNCTION__);
		dev->ep0state = DATA_STATE_NEED_ZLP;
	}

	return 1;
}

static int stm32f10x_handle_get_status(struct stm32f10x_udc *dev,
				     struct usb_ctrlrequest *ctrl)
{
	struct stm32f10x_ep *ep0 = &dev->ep[0];
	struct stm32f10x_ep *qep;
	int reqtype = (ctrl->bRequestType & USB_RECIP_MASK);
	u16 val = 0;

	if (reqtype == USB_RECIP_INTERFACE) {
		/* This is not supported.
		 * And according to the USB spec, this one does nothing..
		 * Just return 0
		 */
		DEBUG_SETUP("GET_STATUS: USB_RECIP_INTERFACE\n");
	} else if (reqtype == USB_RECIP_DEVICE) {
		DEBUG_SETUP("GET_STATUS: USB_RECIP_DEVICE\n");
		val |= (1 << 0);	/* Self powered */
	} else if (reqtype == USB_RECIP_ENDPOINT) {
		int ep_num = (ctrl->wIndex & ~USB_DIR_IN);

		DEBUG_SETUP
		    ("GET_STATUS: USB_RECIP_ENDPOINT (%d), ctrl->wLength = %d\n",
		     ep_num, ctrl->wLength);

		if (ctrl->wLength > 2 || ep_num > 3)
			return -EOPNOTSUPP;

		qep = &dev->ep[ep_num];
		if (ep_is_in(qep) != ((ctrl->wIndex & USB_DIR_IN) ? 1 : 0)
		    && ep_index(qep) != 0) {
			return -EOPNOTSUPP;
		}

		val = _GetEPRxStatus(ep_num);


		DEBUG_SETUP("GET_STATUS, ep: %d (%x), val = %d\n", ep_num,
			    ctrl->wIndex, val);
	} else {
		DEBUG_SETUP("Unknown REQ TYPE: %d\n", reqtype);
		return -EOPNOTSUPP;
	}

	/* Clear "out packet ready" */
	/* Put status to FIFO */
	stm32f10x_fifo_write(ep0, (u8 *) & val, sizeof(val));
	/* Issue "In packet ready" */
	SetEPTxCount(ENDP0,sizeof(val));
	SetEPTxStatus(ENDP0, EP_TX_VALID);


	return 0;
}

/*
 * WAIT_FOR_SETUP (OUT_PKT_RDY)
 *      - read data packet from EP0 FIFO
 *      - decode command
 *      - if error
 *              set EP0_CLR_OUT | EP0_DATA_END | EP0_SEND_STALL bits
 *      - else
 *              set EP0_CLR_OUT | EP0_DATA_END bits
 */
static void stm32f10x_ep0_setup(struct stm32f10x_udc *dev, u32 csr)
{
	struct stm32f10x_ep *ep = &dev->ep[0];
	struct usb_ctrlrequest ctrl;
	int i, bytes, is_in;
	unsigned short *pBuf;

	DEBUG_SETUP("%s: %x\n", __FUNCTION__, csr);

	/* Nuke all previous transfers */
	nuke(ep, -EPROTO);

	/* read control req from fifo (8 bytes) */
	bytes = GetEPRxCount(ENDP0);
	if(bytes == 0)
	{
		SetEPTxCount(ENDP0,0);
		SetEPTxStatus(ENDP0, EP_TX_NAK);
		SetEPRxStatus(ENDP0,EP_RX_VALID);
		return;
	}
	 pBuf= (u16 *)(GetEPRxAddr(ENDP0)+PMAAddr);
	 ctrl.bRequestType = (*pBuf)&0xFF; /* bmRequestType */
    	ctrl.bRequest = ((*pBuf)&0xFF00)>>8; /* bRequest */
    ctrl.wValue = (*(pBuf+1)); /* wValue */
    ctrl.wIndex = (*(pBuf+2)); /* wIndex */
    ctrl.wLength = *(pBuf+3);  /* wLength */
 

	

	DEBUG_SETUP("Read CTRL REQ %d bytes\n", bytes);
	DEBUG_SETUP("CTRL.bRequestType = %d (is_in %d)\n", ctrl.bRequestType,
		    ctrl.bRequestType == USB_DIR_IN);
	printk("CTRL.bRequestType = %d (is_in %d)\n", ctrl.bRequestType,
		    ctrl.bRequestType == USB_DIR_IN);
	DEBUG_SETUP("CTRL.bRequest = %d\n", ctrl.bRequest);
	DEBUG_SETUP("CTRL.wLength = %d\n", ctrl.wLength);
	DEBUG_SETUP("CTRL.wValue = %d (%d)\n", ctrl.wValue, ctrl.wValue >> 8);
	DEBUG_SETUP("CTRL.wIndex = %d\n", ctrl.wIndex);

	/* Set direction of EP0 */
	if (likely(ctrl.bRequestType & USB_DIR_IN)) {
		ep->bEndpointAddress |= USB_DIR_IN;
		is_in = 1;
	} else {
		ep->bEndpointAddress &= ~USB_DIR_IN;
		is_in = 0;
	}

	dev->req_pending = 1;

	/* Handle some SETUP packets ourselves */
	switch (ctrl.bRequest) {
	case USB_REQ_SET_ADDRESS:
		if (ctrl.bRequestType != (USB_TYPE_STANDARD | USB_RECIP_DEVICE))
			break;

		DEBUG_SETUP("USB_REQ_SET_ADDRESS (%d)\n", ctrl.wValue);

		dev->usb_address = ctrl.wValue;
		
		
		
		SetEPTxCount(ENDP0,0);
		SetEPTxStatus(ENDP0, EP_TX_VALID);
		mdelay(2);
		udc_set_address(dev, ctrl.wValue);
		return;

	case USB_REQ_GET_STATUS:{
			if (stm32f10x_handle_get_status(dev, &ctrl) == 0)
				return;

	case USB_REQ_CLEAR_FEATURE:
	case USB_REQ_SET_FEATURE:
			if (ctrl.bRequestType == USB_RECIP_ENDPOINT) {
				struct stm32f10x_ep *qep;
				int ep_num = (ctrl.wIndex & 0x0f);

				/* Support only HALT feature */
				if (ctrl.wValue != 0 || ctrl.wLength != 0
				    || ep_num > 3 || ep_num < 1)
					break;

				qep = &dev->ep[ep_num];
				spin_unlock(&dev->lock);
				if (ctrl.bRequest == USB_REQ_SET_FEATURE) {
					DEBUG_SETUP("SET_FEATURE (%d)\n",
						    ep_num);
					stm32f10x_set_halt(&qep->ep, 1);
				} else {
					DEBUG_SETUP("CLR_FEATURE (%d)\n",
						    ep_num);
					stm32f10x_set_halt(&qep->ep, 0);
				}
				spin_lock(&dev->lock);
				return;
			}
			break;
		}

	default:
		break;
	}

	if (likely(dev->driver)) {
		/* device-2-host (IN) or no data setup command, process immediately */
		spin_unlock(&dev->lock);
		i = dev->driver->setup(&dev->gadget, &ctrl);
		spin_lock(&dev->lock);

		if (i < 0) {
			/* setup processing failed, force stall */
			DEBUG_SETUP
			    ("  --> ERROR: gadget setup FAILED (stalling), setup returned %d\n",
			     i);
			SetEPTxStatus(ep_index(ep),EP_TX_STALL);	

			/* ep->stopped = 1; */
			dev->ep0state = WAIT_FOR_SETUP;
		}
		SetEPTxCount(ENDP0,0);
		SetEPTxStatus(ENDP0, EP_TX_VALID);

		SetEPTxCount(ENDP1,0);
		SetEPTxStatus(ENDP1, EP_TX_VALID);

		SetEPTxCount(ENDP3,0);
		SetEPTxStatus(ENDP3, EP_TX_VALID);

		SetEPRxStatus(ENDP2, EP_RX_VALID);
		
	}
}

/*
 * DATA_STATE_NEED_ZLP
 */
static void stm32f10x_ep0_in_zlp(struct stm32f10x_udc *dev, u32 csr)
{
	DEBUG_EP0("%s: %x\n", __FUNCTION__, csr);

	vSetEPTxStatus(EP_TX_VALID);
	vSetEPRxStatus(EP_RX_VALID);

	dev->ep0state = WAIT_FOR_SETUP;
}

/*
 * handle ep0 interrupt
 */
static void stm32f10x_handle_ep0(struct stm32f10x_udc *dev, u32 intr, u32 csr)
{
	struct stm32f10x_ep *ep = &dev->ep[0];

	DEBUG_EP0("%s: CSR = %x\n", __FUNCTION__, csr);
	
	/*
	 * if SENT_STALL is set
	 *      - clear the SENT_STALL bit
	 */
	 if(GetTxStallStatus(ENDP0) || GetRxStallStatus(ENDP0))
	 {
		DEBUG_EP0("%s: EP0_SENT_STALL is set: %x\n", __FUNCTION__, csr);
		nuke(ep, -ECONNABORTED);
		dev->ep0state = WAIT_FOR_SETUP;
		return;
	}
	/*
	 * if a transfer is in progress && IN_PKT_RDY and OUT_PKT_RDY are clear
	 *      - fill EP0 FIFO
	 *      - if last packet
	 *      -       set IN_PKT_RDY | DATA_END
	 *      - else
	 *              set IN_PKT_RDY
	 */
	if (!(csr & (EP_CTR_TX | EP_CTR_RX))) {
			
		
		DEBUG_EP0("%s: IN_PKT_RDY and OUT_PKT_RDY are clear\n",
			  __FUNCTION__);

		switch (dev->ep0state) {
		case DATA_STATE_XMIT:
			DEBUG_EP0("continue with DATA_STATE_XMIT\n");
			stm32f10x_ep0_in(dev, csr);
			return;
		case DATA_STATE_NEED_ZLP:
			DEBUG_EP0("continue with DATA_STATE_NEED_ZLP\n");
			stm32f10x_ep0_in_zlp(dev, csr);
			return;
		default:
			/* Stall? */
			DEBUG_EP0("Odd state!! state = %s\n",
				  state_names[dev->ep0state]);
			dev->ep0state = WAIT_FOR_SETUP;
			break;
		}
	}
	else if (csr & EP_CTR_TX)	{
	
		_ClearEP_CTR_TX(ENDP0);
		if(!(csr & EP_SETUP))
		{
			if(dev->ep0state == DATA_STATE_XMIT)
			{
				DEBUG_EP0("continue with DATA_STATE_XMIT\n");
				stm32f10x_ep0_in(dev, csr);
				if(dev->ep0state == WAIT_FOR_SETUP)
				{
				SetEPTxStatus(ENDP0,EP_TX_NAK);
				SetEPTxCount(ENDP0, 0);
				SetEPRxStatus(ENDP0,EP_RX_VALID);
				}
				return;
			}
			else if(dev->ep0state == WAIT_FOR_SETUP)
			{
				DEBUG_EP0("WAIT_SETUP\n");
				SetEPRxStatus(ENDP0,EP_RX_VALID);
				SetEPTxStatus(ENDP0,EP_TX_NAK);
				return;

			}
		}
		
		
	}

	/*
	 * if SETUP_END is set
	 *      - abort the last transfer
	 *      - set SERVICED_SETUP_END_BIT
	 */
	if (csr & EP_SETUP) {
		DEBUG_EP0("%s: EP0_SETUP_END is set: %x\n", __FUNCTION__, csr);

			_ClearEP_CTR_RX(ENDP0);


		nuke(ep, 0);
		dev->ep0state = WAIT_FOR_SETUP;
	}

	/*
	 * if EP0_OUT_PKT_RDY is set
	 *      - read data packet from EP0 FIFO
	 *      - decode command
	 *      - if error
	 *              set SERVICED_OUT_PKT_RDY | DATA_END bits | SEND_STALL
	 *      - else
	 *              set SERVICED_OUT_PKT_RDY | DATA_END bits
	 */
	if (csr & EP_CTR_RX) {

		DEBUG_EP0("%s: EP0_OUT_PKT_RDY is set: %x\n", __FUNCTION__,
			  csr);
			_ClearEP_CTR_RX(ENDP0);

		switch (dev->ep0state) {
		case WAIT_FOR_SETUP:
			DEBUG_EP0("WAIT_SETUP\n");
			if(csr & EP_SETUP)
				stm32f10x_ep0_setup(dev, csr);
			else
				{
				SetEPTxStatus(ENDP0,EP_TX_VALID);
				SetEPTxCount(ENDP0, 0);
				SetEPRxStatus(ENDP0,EP_RX_VALID);
				return;
				}
			
			break;

		case DATA_STATE_RECV:
			DEBUG_EP0("DATA_STATE_RECV\n");
			stm32f10x_ep0_out(dev, csr);
			break;

		default:
			/* send stall? */
			DEBUG_EP0("strange state!! 2. send stall? state = %d\n",
				  dev->ep0state);
			break;
		}
	}

}

static void stm32f10x_ep0_kick(struct stm32f10x_udc *dev, struct stm32f10x_ep *ep)
{
	u32 csr;

	
#if 1
	csr = usb_read(USB_CNTR);

	DEBUG_EP0("%s: %x\n", __FUNCTION__, csr);

	/* Clear "out packet ready" */

	if (ep_is_in(ep)) {
		dev->ep0state = DATA_STATE_XMIT;
		stm32f10x_ep0_in(dev, csr);
	} else {
		dev->ep0state = DATA_STATE_RECV;
		stm32f10x_ep0_out(dev, csr);
	}
#endif	
}

/* ---------------------------------------------------------------------------
 * 	device-scoped parts of the api to the usb controller hardware
 * ---------------------------------------------------------------------------
 */

static int stm32f10x_udc_get_frame(struct usb_gadget *_gadget)
{
	return GetFNR() &0x7FFF;

}

static int stm32f10x_udc_wakeup(struct usb_gadget *_gadget)
{
	return -ENOTSUPP;
}

static const struct usb_gadget_ops stm32f10x_udc_ops = {
	.get_frame = stm32f10x_udc_get_frame,
	.wakeup = stm32f10x_udc_wakeup,
	/* current versions must always be self-powered */
};

static void nop_release(struct device *dev)
{
	DEBUG("%s %s\n", __FUNCTION__, dev->bus_id);
}

static struct stm32f10x_udc memory = {
	.usb_address = 0,

	.gadget = {
		   .ops = &stm32f10x_udc_ops,
		   .ep0 = &memory.ep[0].ep,
		   .name = driver_name,
		   .dev = {
			   .bus_id = "gadget",
			   .release = nop_release,
			   },
		   },

	/* control endpoint */
	.ep[0] = {
		  .ep = {
			 .name = ep0name,
			 .ops = &stm32f10x_ep_ops,
			 .maxpacket = EP0_PACKETSIZE, //8,
			 },
		  .dev = &memory,
		  .bEndpointAddress = 0,
		  .bmAttributes = 0,

		  .ep_type = ep_control,
		  .fifo = EPxFIFO_ADDR(ENDP0_TXADDR),	/*EPO TX ADDR*/
		  .csr = USB_CNTR,
		  },

	/* first group of endpoints */
	.ep[1] = {
		  .ep = {
			 .name = "ep1in-bulk",
			 .ops = &stm32f10x_ep_ops,
			 .maxpacket = EP_MAXPACKETSIZE,
			 },
		  .dev = &memory,
		  .bEndpointAddress = USB_DIR_IN | 1,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,

		  .ep_type = ep_bulk_in,
  		  .fifo = EPxFIFO_ADDR(ENDP1_TXADDR),	/*EP1 TX ADDR*/
		  .csr = USB_CNTR,
		  },

	.ep[2] = {
		  .ep = {
			 .name = "ep2out-bulk",
			 .ops = &stm32f10x_ep_ops,
			 .maxpacket = EP_MAXPACKETSIZE,
			 },
		  .dev = &memory,
		  .bEndpointAddress = 2,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,

		  .ep_type = ep_bulk_out,
		  .fifo = EPxFIFO_ADDR(ENDP2_RXADDR),	/*EP2RX ADDR*/

		  .csr = USB_CNTR,
		  },
#if 1
	.ep[3] = {
		  .ep = {
			 .name = "ep3in-int",
			 .ops = &stm32f10x_ep_ops,
			 .maxpacket = EP_MAXPACKETSIZE,
			 },
		  .dev = &memory,
		  .bEndpointAddress = USB_DIR_IN | 3,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .ep_type = ep_interrupt,
		  .fifo = EPxFIFO_ADDR(ENDP3_TXADDR),	/*EP3TX ADDR*/
		  .csr = USB_CNTR,
		  },
#endif		 
};

/*
 * 	probe - binds to the platform device
 */

static int stm32f10x_udc_probe(struct platform_device *pdev)
{
	struct stm32f10x_udc *dev = &memory;
	int retval;

	DEBUG("%s: %p\n", __FUNCTION__, pdev);

	spin_lock_init(&dev->lock);
	dev->dev = &pdev->dev;

	device_initialize(&dev->gadget.dev);
	dev->gadget.dev.parent = &pdev->dev;

	the_controller = dev;
	platform_set_drvdata(pdev, dev);
	
	Set_System();
	Set_USBClock();
	
	udc_disable(dev);
	udc_reinit(dev);

    	USB_Cable_Config(ENABLE);



	/* irq setup after old hardware state is cleaned up */
	retval =
	    request_irq(USB_LP_CAN1_RX0_IRQn, stm32f10x_udc_irq,IRQF_DISABLED, driver_name,
			dev);
	if (retval != 0) {
		DEBUG(KERN_ERR "%s: can't get irq %i, err %d\n", driver_name,
		      USB_LP_CAN1_RX0_IRQn, retval);
		return -EBUSY;
	}
	
	/* TODO:Configure NVIC */
	 USB_Interrupts_Config();
	 
	create_proc_files();

	return retval;
}

static int stm32f10x_udc_remove(struct platform_device *pdev)
{
	struct stm32f10x_udc *dev = platform_get_drvdata(pdev);

	DEBUG("%s: %p\n", __FUNCTION__, pdev);

	udc_disable(dev);
	remove_proc_files();
	usb_gadget_unregister_driver(dev->driver);

	free_irq(USB_LP_CAN1_RX0_IRQn, dev);

	platform_set_drvdata(pdev, 0);

	the_controller = 0;

	return 0;
}

/*-------------------------------------------------------------------------*/

static struct platform_driver udc_driver = {
	.probe = stm32f10x_udc_probe,
	.remove = stm32f10x_udc_remove,
	    /* FIXME power management support */
	    /* .suspend = ... disable UDC */
	    /* .resume = ... re-enable UDC */
	.driver	= {
		.name = (char *)driver_name,
		.owner = THIS_MODULE,
	},
};

static int __init udc_init(void)
{
	DEBUG("%s: %s version %s\n", __FUNCTION__, driver_name, DRIVER_VERSION);
	return platform_driver_register(&udc_driver);	

}

static void __exit udc_exit(void)
{
	platform_driver_unregister(&udc_driver);
}

module_init(udc_init);
module_exit(udc_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("MCD Application Team");
MODULE_LICENSE("GPL");
