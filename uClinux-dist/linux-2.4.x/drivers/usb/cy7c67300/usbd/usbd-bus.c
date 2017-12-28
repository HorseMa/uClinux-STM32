/*
 * linux/drivers/usbd/usbd-bus.c - USB Device Prototype
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

#include <linux/config.h>
#define USBD_CONFIG_NOWAIT_DEREGISTER_DEVICE 1
#include <linux/module.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>

#include "usbd.h"
#include "usbd-debug.h"
#include "usbd-func.h"
#include "usbd-bus.h"
#include "usbd-inline.h"

extern int usb_devices;
extern struct usb_function_driver ep0_driver;
extern int registered_functions;

extern struct list_head function_drivers;
extern struct list_head devices;        

static __inline__ struct usb_function_driver *
list_entry_func(const struct list_head *le)
{
    return list_entry(le, struct usb_function_driver, drivers);
}

static __inline__ struct usb_device_instance *
list_entry_device(const struct list_head *le)
{
    return list_entry(le, struct usb_device_instance, devices);
}

/*
 * Debug stuff - dbgflg_usbdbi_init must be defined in the main part of the bus 
 * driver (e.g. *_bi/init.c) 
 */
extern int      dbgflg_usbdbi_init;
#define dbg_init(lvl,fmt,args...) dbgPRINT(dbgflg_usbdbi_init,lvl,fmt,##args)

extern int registered_functions;
extern int registered_devices;


/*
 * usbd_fill_rcv - fill the rx recyle queue with empty urbs
 * @endpoint:
 *
 * Fill the recycle queue so that receive has some empty urbs to use.
 */
void usbd_fill_rcv(struct usb_device_instance *device, struct usb_endpoint_instance *endpoint, int num)
{
    int i;
    unsigned buffersize;

    dbg_init(1, "endpoint: %d", endpoint->endpoint_address);

    if (endpoint->rcv_packetSize) {

        /* 
         * we need to allocate URBs with enough room for the endpoints 
         * transfersize plus one rounded 
         * up to the next packetsize
         */
        
        buffersize = ((endpoint->rcv_transferSize + endpoint->rcv_packetSize) / endpoint->rcv_packetSize) *
            endpoint->rcv_packetSize;

        dbg_init(1, "endpoint: %d packetSize: %d transferSize: %d buffersize: %d", 
                endpoint->endpoint_address, endpoint->rcv_packetSize, endpoint->rcv_transferSize, buffersize);

        for (i = 0; i < num; i++) {
            usbd_recycle_urb(usbd_alloc_urb(device, device->function_instance_array, endpoint->endpoint_address, buffersize));
        }
    }
    else {
        dbg_init(0, "endpoint: %d packetSize is Zero!", endpoint->endpoint_address);
    }

}


/**
 * usbd_flush_rcv - flush rcv
 * @endpoint:
 *
 * Iterate across the rx list and dispose of any urbs.
 */
void usbd_flush_rcv(struct usb_endpoint_instance *endpoint)
{
    struct usbd_urb *rcv_urb = NULL;
    unsigned long flags; 
    
    if (endpoint) {
        local_irq_save(flags);

        if ((rcv_urb = endpoint->rcv_urb)) {
            endpoint->rcv_urb = NULL;
            usbd_dealloc_urb(rcv_urb);
        }

        while ((rcv_urb = first_urb_detached(&endpoint->rdy))) {
            usbd_dealloc_urb(rcv_urb);
        }

        local_irq_restore(flags);
    }
}


/**
 * usbd_flush_tx - flush tx urbs from endpoint
 * @endpoint:
 *
 * Iterate across the tx list and cancel any outstanding urbs.
 */
void usbd_flush_tx(struct usb_endpoint_instance *endpoint)
{
    struct usbd_urb *send_urb = NULL;
    unsigned long flags; 
    
    if (endpoint) {
        local_irq_save(flags);

        if ((send_urb = endpoint->tx_urb)) {
            endpoint->tx_urb = NULL;
            usbd_urb_sent_irq(send_urb, SEND_FINISHED_ERROR);
        }

        while ((send_urb = first_urb_detached(&endpoint->tx))) {
            usbd_urb_sent_irq(send_urb, SEND_FINISHED_ERROR);
        }

        local_irq_restore(flags);
    }
}


/**
 * usbd_flush_ep - flush urbs from endpoint
 * @endpoint:
 *
 * Iterate across the approrpiate tx or rcv list and cancel any outstanding urbs.
 */
void usbd_flush_ep(struct usb_endpoint_instance *endpoint)
{
    if (endpoint) {
        if (endpoint->endpoint_address & 0x7f) {
            usbd_flush_tx(endpoint);
        }
        else {
            usbd_flush_rcv(endpoint);
        }
    }
}


/**
 * usbd_device_bh - 
 * @data:
 *
 * Bottom half handler to process sent or received urbs.
 */      
void usbd_device_bh(void *data)
{
    struct usb_device_instance *device = data;
    int i;

    for (i = 0; i < device->bus->driver->max_endpoints; i++) {
        struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + i;

        /* process received urbs */
        if (endpoint->endpoint_address && (endpoint->endpoint_address & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) {
            struct usbd_urb *urb;
            while ((urb = first_urb_detached(&endpoint->rcv))) {
                if ( !urb->function_instance || !urb->function_instance->function_driver->ops->recv_urb) {
                    printk("usbd_device_bh: no recv_urb function\n");
                    usbd_recycle_urb(urb);
                }
                else if (urb->function_instance->function_driver->ops->recv_urb(urb)) {
                    /* printk(KERN_ERR"usbd_device_bh: recv_urb failed\n"); */
                    usbd_recycle_urb(urb);
                }
            }
        }

        /* process sent urbs */
        if (endpoint->endpoint_address && (endpoint->endpoint_address & USB_REQ_DIRECTION_MASK) == USB_REQ_DEVICE2HOST) {
            struct usbd_urb *urb;
            while ((urb = first_urb_detached(&endpoint->done))) {
                if ( !urb->function_instance || !urb->function_instance->function_driver->ops->urb_sent ||
                        urb->function_instance->function_driver->ops->urb_sent(urb, urb->status)) 
                {
                    printk(KERN_ERR"usbd_device_bh: no urb_sent function\n");
                    usbd_dealloc_urb(urb);
                }
            }
        }

    }

#if defined(CONFIG_SA1100_COLLIE) && defined(CONFIG_PM)
    {
        /* 
         * Please clear autoPowerCancel flag during the transmission 
         * and  the reception.
         */
        /* XXX XXX extern */
        int    autoPowerCancel;
        autoPowerCancel = 0;                /* Auto Power Off Cancel */
    }
#endif

#if defined(CONFIG_SA1110_CALYPSO) && defined(CONFIG_PM)
    /* Shouldn't need to make this atomic, all we need is a change indicator */
    device->usbd_rxtx_timestamp = jiffies;
#endif

    /* MOD_DEC_USE_COUNT; */
    /* printk(KERN_DEBUG"usbd_device_bh: USE: %d\n", 
		GET_USE_COUNT(THIS_MODULE)); */
    return;
}

/**
 * usbd_function_bh - 
 * @data:
 *
 * Bottom half handler to process events for functions.
 */      
void usbd_function_bh(void *data)
{
    struct usb_device_instance *device;

    if ((device = data) && (device->status != USBD_CLOSING)) {
        int i;

        /* XXX this should only be done for endpoint zero... */
        for (i = 0; i < device->bus->driver->max_endpoints; i++) {
            struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + i;

            /* process event urbs */
            if (!endpoint->endpoint_address) {
                struct usbd_urb *urb;
                while ((urb = first_urb_detached(&endpoint->events))) {
                    if (device->function_instance_array && (device->function_instance_array+0)->function_driver->ops->event) {
                        (device->function_instance_array+0)->function_driver->ops->event(device, urb->event, urb->data);
                    }
                    usbd_dealloc_urb(urb);
                }
            }
        }
    }
#if 0
    /* Not an error if closing in progress */
    else if (NULL == device) {
        printk(KERN_ERR"usbd_function_bh: device NULL\n");
    }
#endif
}



/* usb-device USB BUS INTERFACE generic functions ******************************************** */

/**
 * usbd_register_bus - called by a USB BUS INTERFACE driver to register a bus driver
 * @driver: pointer to bus driver structure
 *
 * Used by a USB Bus interface driver to register itself with the usb device
 * layer.
 */
struct __init usb_bus_instance *usbd_register_bus(struct usb_bus_driver *driver)
{
    struct usb_bus_instance *bus;
    int i;

    dbg_init(2, "-");

    if ((bus = ckmalloc(sizeof(struct usb_bus_instance), GFP_ATOMIC))==NULL) {
        return NULL;
    }
    bus->driver = driver;
    if (!(bus->endpoint_array = ckmalloc(sizeof(struct usb_endpoint_instance) * bus->driver->max_endpoints, GFP_ATOMIC))) {
        kfree(bus);
        return NULL;
    }

    for (i = 0; i < bus->driver->max_endpoints; i++) {

        struct usb_endpoint_instance *endpoint = bus->endpoint_array + i;

        urb_link_init(&endpoint->events);
        urb_link_init(&endpoint->rcv);
        urb_link_init(&endpoint->rdy);
        urb_link_init(&endpoint->tx);
        urb_link_init(&endpoint->done);
    }

    return bus;
}


/**
 * usbd_deregister_bus - called by a USB BUS INTERFACE driver to deregister a bus driver
 * @bus: pointer to bus driver instance
 *
 * Used by a USB Bus interface driver to de-register itself with the usb device
 * layer.
 */
void __exit usbd_deregister_bus(struct usb_bus_instance *bus)
{
    dbg_init(3, "%s", bus->driver->name);
    kfree(bus->endpoint_array);
    kfree(bus);
}


/* usb-device USB Device generic functions *************************************************** */

/**
 * usbd_register_device - called to create a virtual device
 * @name: name
 * @bus: pointer to struct usb_device_instance
 * @maxpacketsize: ep0 maximum packetsize
 *
 * Used by a USB Bus interface driver to create a virtual device.
 */
struct usb_device_instance *
__init usbd_register_device(char *name, struct usb_bus_instance *bus, int maxpacketsize)
{
    struct usb_device_instance *device;
    struct usb_function_instance *function_instance_array;
    struct list_head *lhd;
    int num = usb_devices++;
    char buf[32];
    int function;

    dbg_init(3, "-      -       -       -       -       -       -");

    /* allocate a usb_device_instance structure */
    if (!(device = ckmalloc(sizeof(struct usb_device_instance), GFP_ATOMIC))) {
        dbg_init(0, "ckmalloc device failed");
        return NULL;
    }

    /* create a name */
    if (!name || !strlen(name)) {
        sprintf(buf, "usb%d", num);
        name = buf;
    }
    if ((device->name = strdup(name))==NULL) {
        kfree(device);
        dbg_init(0, "strdup name failed");
        return NULL;
    }

    device->device_state = STATE_CREATED;
    device->status = USBD_OPENING;

    /* allocate a usb_function_instance for ep0 default ctrl function driver */
    if ((device->ep0 = ckmalloc(sizeof(struct usb_function_instance),GFP_ATOMIC))==NULL) {
        kfree(device->name);
        kfree(device);
        dbg_init(0, "ckmalloc device failed");
        return NULL;
    }
    device->ep0->function_driver = &ep0_driver;

    /* allocate an array of usb configuration instances */
    if ((function_instance_array = 
                ckmalloc(sizeof(struct usb_function_instance)*registered_functions, GFP_ATOMIC))==NULL)
    {
        kfree(device->ep0);
        kfree(device->name);
        kfree(device);
        dbg_init(0, "ckmalloc function_instance_array failed");
        return NULL;
    }

    device->functions = registered_functions;
    device->function_instance_array = function_instance_array;

    dbg_init(2, "device: %p function_instance[]: %p registered_functions: %d",
             device, device->function_instance_array, registered_functions);

    /* connect us to bus */
    device->bus = bus;

    /* 
     * iterate across all of the function drivers to construct a 
     * complete list of configuration descriptors
     */
    /*  XXX there is currently only one XXX */
    function = 0;
    dbg_init(1, "function init");
    list_for_each(lhd, &function_drivers) {
        struct usb_function_driver *function_driver;
        function_driver = list_entry_func(lhd);
        /* build descriptors */
        dbg_init(1, "calling function_init");
        usbd_function_init(bus, device, function_driver);
        /* save */ 
        function_instance_array[function].function_driver = function_driver;
    }

    /* device bottom half */
    device->device_bh.routine = usbd_device_bh;
    device->device_bh.data = device;
    /* XXX device->device_bh.sync = 0; */

    /* function bottom half */
    device->function_bh.routine = usbd_function_bh;
    device->function_bh.data = device;
    /* XXX device->function_bh.sync = 0; */

    dbg_init(3,"%p %p", device, device->device_bh.data);

    /* add to devices queue */
    list_add_tail(&device->devices, &devices);
    registered_devices++;

    dbg_init(3, "%s finished", bus->driver->name);
    return device;
}

/**
 * usbd_deregister_device - called by a USB BUS INTERFACE driver to deregister a physical interface
 * @device: pointer to struct usb_device_instance
 *
 * Used by a USB Bus interface driver to destroy a virtual device.
 */
void __exit usbd_deregister_device(struct usb_device_instance *device)
{
    struct usb_function_instance *function_instance;

    dbg_init(3, "%s start", device->bus->driver->name);

    /* prevent any more bottom half scheduling */
    device->status = USBD_CLOSING;

#ifdef USBD_CONFIG_NOWAIT_DEREGISTER_DEVICE
    if (device->device_bh.sync) {
	/* The only way deal with this other than waiting is to do the task. */
	run_task_queue(&tq_immediate);
        if (device->device_bh.sync) {
            printk(KERN_ERR "%s: run_task_queue(&tq_immediate) fails to clear device_bh!\n",__FUNCTION__);
        }
    }
    if (device->function_bh.sync) {
	/* The only way deal with this other than waiting is to do the task. */
        flush_scheduled_tasks();
        if (device->function_bh.sync) {
            printk(KERN_ERR "%s: flush_scheduled_tasks() fails to clear function_bh!\n",__FUNCTION__);
        }
    }
    /* Leave the wait in as paranoia - in case one of the task operations
       above fails to clear the queued task.  It should never happen, but ... */
#endif
    /* wait for pending device and function bottom halfs to finish */
    while (device->device_bh.sync || device->function_bh.sync) {
        dbg_init(1, "wainting for usbd_device_bh");
        schedule_timeout(10*HZ);
    }

    /* tell the function driver to close */
    usbd_function_close(device);

    /* disconnect from bus */
    device->bus = NULL;
    
    /* remove from devices queue */
    list_del(&device->devices);

    /* free function_instances */

    if ((function_instance = device->function_instance_array)) {
        device->function_instance_array = NULL;
        dbg_init(3, "freeing function instances: %p", function_instance);
        kfree(function_instance);
    }

    /* de-configured ep0 */
    if ((function_instance = device->ep0)) {
        device->ep0 = NULL;
        dbg_init(3, "freeing ep0 instance: %p", function_instance);
        kfree(function_instance);
    }

    kfree(device->name);
    kfree(device);

    registered_devices--;
    dbg_init(3, "finished");
}


