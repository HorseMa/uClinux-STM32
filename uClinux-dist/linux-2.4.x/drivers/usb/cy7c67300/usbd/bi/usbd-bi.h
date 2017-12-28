/*
 * linux/drivers/usbd/usbd-bi.h
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *      Bruce Balden <balden@lineo.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define MAX_DEVICES 1

/* struct udc_bi_data
 *
 * private data structure for this bus interface driver
 */
struct bi_data {
    int num;
    struct tq_struct  cradle_bh;
};
 

extern unsigned int udc_interrupts;

extern int dbgflg_usbdbi_init;
extern int dbgflg_usbdbi_intr;
extern int dbgflg_usbdbi_tick;
extern int dbgflg_usbdbi_usbe;
extern int dbgflg_usbdbi_rx;
extern int dbgflg_usbdbi_tx;
extern int dbgflg_usbdbi_dma_flg;
extern int dbgflg_usbdbi_setup;
extern int dbgflg_usbdbi_ep0;
extern int dbgflg_usbdbi_udc;
extern int dbgflg_usbdbi_stall;
extern int dbgflg_usbdbi_pm;
extern int dbgflg_usbdbi_pur;

#define dbg_init(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_init,lvl,fmt,##args)
#define dbg_intr(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_intr,lvl,fmt,##args)
#define dbg_tick(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_tick,lvl,fmt,##args)
#define dbg_usbe(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_usbe,lvl,fmt,##args)
#define dbg_rx(lvl,fmt,args...)    dbgPRINT(dbgflg_usbdbi_rx,lvl,fmt,##args)
#define dbg_tx(lvl,fmt,args...)    dbgPRINT(dbgflg_usbdbi_tx,lvl,fmt,##args)
#define dbg_dmaflg(lvl,fmt,args...) dbgPRINT(dbgflg_usbdbi_dma_flg,lvl,fmt,##args)
#define dbg_setup(lvl,fmt,args...) dbgPRINT(dbgflg_usbdbi_setup,lvl,fmt,##args)
#define dbg_ep0(lvl,fmt,args...)   dbgPRINT(dbgflg_usbdbi_ep0,lvl,fmt,##args)
#define dbg_udc(lvl,fmt,args...)   dbgPRINT(dbgflg_usbdbi_udc,lvl,fmt,##args)
#define dbg_stall(lvl,fmt,args...) dbgPRINT(dbgflg_usbdbi_stall,lvl,fmt,##args)
#define dbg_pm(lvl,fmt,args...)    dbgPRINT(dbgflg_usbdbi_pm,lvl,fmt,##args)
#define dbg_pur(lvl,fmt,args...)   dbgPRINT(dbgflg_usbdbi_pur,lvl,fmt,##args)


struct usb_bus_driver * bi_retrieve_bus_driver(void);

int bi_udc_init(void);

/**
 * udc_start_in_irq - start transmit
 * @endpoint:
 *
 * Called with interrupts disabled.
 */
void udc_start_in_irq(struct usb_endpoint_instance *endpoint, 
                      struct usb_device_instance * device);

/**
 * udc_stall_ep - stall endpoint
 * @ep: physical endpoint
 *
 * Stall the endpoint.
 */
void udc_stall_ep(unsigned int ep, struct usb_device_instance * device);

/**
 * udc_reset_ep - reset endpoint
 * @ep: physical endpoint
 * reset the endpoint.
 *
 * returns : 0 if ok, -1 otherwise
 */
void udc_reset_ep(unsigned int ep, struct usb_device_instance * device);

/**
 * udc_endpoint_halted - is endpoint halted
 * @ep:
 *
 * Return non-zero if endpoint is halted
 */
int udc_endpoint_halted(unsigned int ep, struct usb_device_instance * device);

/**
 * udc_set_address - set the USB address for this device
 * @address:
 *
 * Called from control endpoint function after it decodes a set address setup packet.
 */
void udc_set_address(unsigned char address, 
                     struct usb_device_instance * device);

/**
 * udc_serial_init - set a serial number if available
 */
int __init udc_serial_init(struct usb_bus_instance *);

/**
 * udc_max_endpoints - max physical endpoints 
 *
 * Return number of physical endpoints.
 */
int udc_max_endpoints(void);

/**
 * udc_check_ep - check logical endpoint 
 * @lep:
 * @packetsize:
 *
 * Return physical endpoint number to use for this logical endpoint or zero if not valid.
 */
int udc_check_ep(int logical_endpoint, int packetsize, 
                 struct usb_device_instance * device);

/**
 * udc_set_ep - setup endpoint 
 * @ep:
 * @endpoint:
 *
 * Associate a physical endpoint with endpoint_instance
 */
void udc_setup_ep(struct usb_device_instance * device, 
                  unsigned int ep, 
                  struct usb_endpoint_instance *endpoint);


/**
 * udc_disable_ep - disable endpoint
 * @ep:
 *
 * Disable specified endpoint 
 */
void udc_disable_ep(unsigned int ep);


/**
 * udc_connected - is the USB cable connected
 *
 * Return non-zeron if cable is connected.
 */
int udc_connected(void);


/**
 * udc_connect - enable pullup resistor
 *
 * Turn on the USB connection by enabling the pullup resistor.
 */
void udc_connect(void);


/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect(void);

/**
 * udc_all_interrupts - enable interrupts
 *
 * Switch on UDC interrupts.
 *
 */
void udc_all_interrupts(struct usb_device_instance *);


/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts(struct usb_device_instance *);

/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts(struct usb_device_instance *);

/**
 * udc_ep0_packetsize - return ep0 packetsize
 */
int udc_ep0_packetsize(void);

/**
 * udc_enable - enable the UDC
 *
 * Switch on the UDC
 */
void udc_enable(struct usb_device_instance *device);

/**
 * udc_disable - disable the UDC
 *
 * Switch off the UDC
 */
void udc_disable(void);

/**
 * udc_startup - allow udc code to do any additional startup
 */
void udc_startup(struct usb_device_instance *device);

/**
 * udc_startup - allow udc code to do any additional startup
 */
void udc_startup_events(struct usb_device_instance *);

/**
 * udc_init - initialize USB Controller
 * 
 * Get ready to use the USB Controller.
 *
 * Register an interrupt handler and IO region. Return non-zero for error.
 */
int udc_init(void);

/**
 * udc_exit - Stop using the USB Controller
 *
 * Stop using the USB Controller.
 *
 * Shutdown and free dma channels, de-register the interrupt handler.
 */
void udc_exit(void);

/**
 * udc_regs - dump registers
 *
 * Dump registers with printk
 */
void udc_regs(void);

/**
 * udc_name - return name of USB Device Controller
 */
char * udc_name(void);

/**
 * udc_request_udc_irq - request UDC interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_udc_irq(void);

/**
 * udc_request_cable_irq - request Cable interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_cable_irq(void);

/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io(void);

/**
 * udc_release_udc_irq - release UDC irq
 */
void udc_release_udc_irq(void);

/**
 * udc_release_cable_irq - release Cable irq
 */
void udc_release_cable_irq(void);

/**
 * udc_release_io - release UDC io region
 */
void udc_release_io(void);

/**
 * udc_connect_event - called from cradle interrupt handler
 * @connected - true if we are in cradle
 */
void udc_cable_event(void);

void udc_ticker_poke(void);
