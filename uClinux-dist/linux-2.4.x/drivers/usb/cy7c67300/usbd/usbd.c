/*
 * linux/drivers/usbd/usbd.c.c - USB Device Core Layer
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


/*
 * Notes...
 *
 * 1. The order of loading must be:
 *
 *        usb-device
 *        usb-function(s)
 *        usb-bus(s)
 *
 * 2. Loading usb-function modules allows them to register with the usb-device
 * layer. This makes their usb configuration descriptors available. Currently
 * function modules cannot be loaded after a bus module has been loaded.
 *
 * 3. Loading usb-bus modules causes them to register with the usb-device
 * layer and then enable their upstream port. This in turn will cause the
 * upstream host to enumerate this device. Note that bus modules can create
 * multiple devices, one per physical interface.
 *
 * 4. The usb-device layer provides a default configuration for endpoint 0 for
 * each device. 
 *
 * 5. Additional configurations are added to support the configuration
 * function driver when entering the configured state.
 *
 * 6. The usb-device maintains a list of configurations from the loaded
 * function drivers. It will provide this to the USB HOST as part of the
 * enumeration process and will add someof them as the active configuration as
 * per the USB HOST instructions.  
 *
 * 6. Two types of bus interface modules can be implemented: simple and compound.
 *
 * 7. Simple bus interface modules can only support a single function module. The
 * first function module is used.
 *
 * 8. Compound bus interface modules can support multiple functions. When implemented
 * these will use all available function modules.
 *
 */

#include <linux/config.h>
#include <linux/module.h>

#include "usbd-export.h"
#include "usbd-build.h"
/* #include "usbd-module.h" */

MODULE_AUTHOR("sl@lineo.com, tbr@lineo.com");
MODULE_DESCRIPTION("USB Device Core Support");

/* USBD_MODULE_INFO("usbdcore 0.1"); */

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

#include "hotplug.h"

#define MAX_INTERFACES 2

/* Module Parameters ************************************************************************* */

MODULE_PARM(maxstrings, "i");
MODULE_PARM(dbg, "s");

MODULE_PARM_DESC(maxstrings, "Maximum number of strings to allow (0..255)");
MODULE_PARM_DESC(dbg, "debug parameters");

int maxstrings = 20;
static char *dbg = "init=0:ep0=0:rx=0:tx=0:usbe=0:mem=0";

/* Debug switches (module parameter "dbg=...") *********************************************** */

int dbgflg_usbdcore_init;
int dbgflg_usbdcore_ep0;
int dbgflg_usbdcore_rx;
int dbgflg_usbdcore_tx;
int dbgflg_usbdcore_usbe;
int dbgflg_usbdcore_urbmem;

static debug_option dbg_table[] = {
    {&dbgflg_usbdcore_init,NULL,"init","initialization and termination"},
    {&dbgflg_usbdcore_ep0,NULL,"ep0","End Point 0 (setup) packet handling"},
    {&dbgflg_usbdcore_rx,NULL,"rx","USB RX (host->device) handling"},
    {&dbgflg_usbdcore_tx,NULL,"tx","USB TX (device->host) handling"},
    {&dbgflg_usbdcore_usbe,NULL,"usbe","USB events"},
    {&dbgflg_usbdcore_urbmem,NULL,"mem","URB memory allocation"},
#if 0
    {NULL,NULL,"hot","hotplug actions"},
    {NULL,NULL,"mon","USB conection monitor"},
#endif
    {NULL,NULL,NULL,NULL}
};

#define dbg_init(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_init,lvl,fmt,##args)
#define dbg_ep0(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_ep0,lvl,fmt,##args)
#define dbg_rx(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_rx,lvl,fmt,##args)
#define dbg_tx(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_tx,lvl,fmt,##args)
#define dbg_usbe(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_usbe,lvl,fmt,##args)
#define dbg_urbmem(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_urbmem,lvl,fmt,##args)

/* Global variables ************************************************************************** */

struct usb_string_descriptor **usb_strings;

int usb_devices;

extern struct usb_function_driver ep0_driver;

int registered_functions;
int registered_devices;

char *usbd_device_events[] = {
    "DEVICE_UNKNOWN",
    "DEVICE_INIT",
    "DEVICE_CREATE",
    "DEVICE_HUB_CONFIGURED",
    "DEVICE_RESET",
    "DEVICE_ADDRESS_ASSIGNED",
    "DEVICE_CONFIGURED",
    "DEVICE_SET_INTERFACE",
    "DEVICE_SET_FEATURE",
    "DEVICE_CLEAR_FEATURE",
    "DEVICE_DE_CONFIGURED",
    "DEVICE_BUS_INACTIVE",
    "DEVICE_BUS_ACTIVITY",
    "DEVICE_POWER_INTERRUPTION",
    "DEVICE_HUB_RESET",
    "DEVICE_DESTROY",
    "DEVICE_FUNCTION_PRIVATE",
};

char *usbd_device_states[] = {
    "STATE_INIT", 
    "STATE_CREATED", 
    "STATE_ATTACHED", 
    "STATE_POWERED", 
    "STATE_DEFAULT", 
    "STATE_ADDRESSED", 
    "STATE_CONFIGURED", 
    "STATE_UNKNOWN", 
};


/* List support functions ******************************************************************** */

LIST_HEAD(function_drivers);    /* list of all registered function modules */
LIST_HEAD(devices);             /* list of all registered devices */


static inline struct usb_function_driver *
list_entry_func(const struct list_head *le)
{
    return list_entry(le, struct usb_function_driver, drivers);
}

static inline struct usb_device_instance *
list_entry_device(const struct list_head *le)
{
    return list_entry(le, struct usb_device_instance, devices);
}


/* Support Functions ************************************************************************* */

/**
 * usbd_hotplug - call a hotplug script
 * @action: hotplug action
 */
int usbd_hotplug(char *interface, char *action)
{
#ifdef CONFIG_HOTPLUG
    dbg_init(1, "agent: usbd interface: %s action: %s", interface, action);
    return hotplug("usbd", interface, action);
#else
    return(0);
#endif
}

void usbd_request_bus(struct usb_device_instance *device, int request)
{

#ifdef JON_CHECK_THIS_OUT
    if(device->bus->driver->ops->bi_event)
    {
        if(request)
        {
            device->bus->driver->ops->bi_event(device, DEVICE_BUS_REQUEST, NULL);
        }
        else
        {
            device->bus->driver->ops->bi_event(device, DEVICE_BUS_RELEASE, NULL);
        }
    }
#endif
}


/* Descriptor support functions ************************************************************** */


/**
 * usbd_get_string - find and return a string descriptor
 * @index: string index to return
 *
 * Find an indexed string and return a pointer to a it.
 */
struct usb_string_descriptor * usbd_get_string(__u8 index)
{
    if (index >= maxstrings) {
        return NULL;
    }
    return usb_strings[index];
}


/**
 * usbd_cancel_urb - cancel an urb being sent
 * @urb: pointer to an urb structure
 *
 * Used by a USB Function driver to cancel an urb that is being
 * sent.
 */
int usbd_cancel_urb(struct usb_device_instance *device, struct usbd_urb *urb)
{
    dbg_tx(3,"%p", urb);
    if (!device->bus->driver->ops->cancel_urb) {
        /* XXX should we do usbd_dealloc_urb(urb); */
        return 0;
    }
    /* urb->device->urbs_queued++; */
    return device->bus->driver->ops->cancel_urb(urb);
}


/* Access to device descriptor functions ***************************************************** */


/* *
 * usbd_device_function_instance - find a function instance for this device
 * @device: 
 * @configuration: index to configuration, 0 - N-1 
 *
 * Get specifed device configuration. Index should be bConfigurationValue-1. 
 */
struct usb_function_instance *
usbd_device_function_instance(struct usb_device_instance *device, unsigned int port)
{
    /* dbg_init(2,"device: %p port: %d functions: %d", device, port,
		 device->functions); */
    if (port >= device->functions) {
        dbg_init(0, "configuration out of range: %d %d", port, device->functions);
        return NULL;
    }
    return device->function_instance_array+port;
}


/* *
 * usbd_device_configuration_instance - find a configuration instance for this device
 * @device: 
 * @configuration: index to configuration, 0 - N-1 
 *
 * Get specifed device configuration. Index should be bConfigurationValue-1. 
 */
static struct usb_configuration_instance *
usbd_device_configuration_instance(struct usb_device_instance *device, unsigned int port, unsigned int configuration)
{
    struct usb_function_instance *function_instance;
    struct usb_function_driver *function_driver;

    if (!(function_instance = usbd_device_function_instance(device, port))) {
        dbg_init(0, "function_instance NULL");
        return NULL;
    }
    if (!(function_driver = function_instance->function_driver)) {
        dbg_init(0, "function_driver NULL");
        return NULL;
    }

    if (configuration >= function_driver->configurations) {
        dbg_init(0, "configuration out of range: %d %d %d", port, configuration, function_driver->configurations);
        return NULL;
    }
    return function_driver->configuration_instance_array+configuration;
}


/* *
 * usbd_device_interface_instance
 * @device: 
 * @configuration: index to configuration, 0 - N-1 
 * @interface: index to interface
 *
 * Return the specified interface descriptor for the specified device.
 */
struct usb_interface_instance *
usbd_device_interface_instance(struct usb_device_instance *device, int port, int configuration, int interface)
{
    struct usb_configuration_instance *configuration_instance;

    if ((configuration_instance = usbd_device_configuration_instance(device, port, configuration)) == NULL) {
        return NULL;
    }
    if (interface >= configuration_instance->interfaces) {
        return NULL;
    }
    return configuration_instance->interface_instance_array+interface;
}

/* *
 * usbd_device_alternate_descriptor_list
 * @device: 
 * @configuration: index to configuration, 0 - N-1 
 * @interface: index to interface
 * @alternate: alternate setting
 *
 * Return the specified alternate descriptor for the specified device.
 */
struct usb_alternate_instance *
usbd_device_alternate_instance(struct usb_device_instance *device, int port, int configuration, int interface, int alternate)
{
    struct usb_interface_instance *interface_instance;

    if ((interface_instance = usbd_device_interface_instance(device, port, configuration, interface)) == NULL) {
        return NULL;
    }

    if (alternate >= interface_instance->alternates) {
        return NULL;
    }

    return interface_instance->alternates_instance_array+alternate;
}


/* *
 * usbd_device_device_descriptor
 * @device: which device
 * @configuration: index to configuration, 0 - N-1 
 * @port: which port
 *
 * Return the specified configuration descriptor for the specified device.
 */
struct usb_device_descriptor *
usbd_device_device_descriptor(struct usb_device_instance *device, int port)
{
    struct usb_function_instance *function_instance;
    if (!(function_instance = usbd_device_function_instance(device, port))) {
        return NULL;
    }
    if (!function_instance->function_driver || !function_instance->function_driver->device_descriptor) {
        return NULL;
    }
    return (function_instance->function_driver->device_descriptor);
}


/**
 * usbd_device_configuration_descriptor
 * @device: which device
 * @port: which port
 * @configuration: index to configuration, 0 - N-1 
 *
 * Return the specified configuration descriptor for the specified device.
 */
struct usb_configuration_descriptor *
usbd_device_configuration_descriptor(struct usb_device_instance *device, int port, int configuration)
{
    struct usb_configuration_instance *configuration_instance;
    if (!(configuration_instance = usbd_device_configuration_instance(device, port, configuration))) {
        return NULL;
    }
    return (configuration_instance->configuration_descriptor);
}


/**
 * usbd_device_interface_descriptor
 * @device: which device
 * @port: which port
 * @configuration: index to configuration, 0 - N-1 
 * @interface: index to interface
 * @alternate: alternate setting
 *
 * Return the specified interface descriptor for the specified device.
 */
struct usb_interface_descriptor *
usbd_device_interface_descriptor(struct usb_device_instance *device, int port, int configuration, int interface, int alternate)
{
    struct usb_interface_instance *interface_instance;
    if (!(interface_instance = usbd_device_interface_instance(device, port, configuration, interface))) {
        return NULL;
    }
    if ((alternate < 0) || (alternate >= interface_instance->alternates)) {
        return NULL;
    }
    return (interface_instance->alternates_instance_array[alternate].interface_descriptor);
}


/**
 * usbd_device_class_descriptor_index
 * @device: which device
 * @port: which port
 * @configuration: index to configuration, 0 - N-1 
 * @interface: index to interface
 * @index: which index
 *
 * Return the specified class descriptor for the specified device.
 */
struct usb_class_descriptor *
usbd_device_class_descriptor_index(
        struct usb_device_instance *device, 
        int port, 
        int configuration, 
        int interface, 
        int alternate, 
        int index)
{
    struct usb_alternate_instance *alternate_instance;

    if (!(alternate_instance = usbd_device_alternate_instance(device, port, configuration, interface, alternate))) {
        return NULL;
    }
    if (index >= alternate_instance->classes) {
        return NULL;
    }
    return *(alternate_instance->classes_descriptor_array+index);
}


/**
 * usbd_device_endpoint_descriptor_index
 * @device: which device
 * @port: which port
 * @configuration: index to configuration, 0 - N-1 
 * @interface: index to interface
 * @alternate: index setting
 * @index: which index
 *
 * Return the specified endpoint descriptor for the specified device.
 */
struct usb_endpoint_descriptor *
usbd_device_endpoint_descriptor_index(
        struct usb_device_instance *device, 
        int port, 
        int configuration, 
        int interface, 
        int alternate, 
        int index)
{
    struct usb_alternate_instance *alternate_instance;

    if (!(alternate_instance = usbd_device_alternate_instance(device, port, configuration, interface, alternate))) {
        return NULL;
    }
    if (index >= alternate_instance->endpoints) {
        return NULL;
    }
    return *(alternate_instance->endpoints_descriptor_array+index);
}


/**
 * usbd_device_endpoint_transfersize
 * @device: which device
 * @port: which port
 * @configuration: index to configuration, 0 - N-1 
 * @interface: index to interface
 * @index: which index
 *
 * Return the specified endpoint transfer size;
 */
int 
usbd_device_endpoint_transfersize(
        struct usb_device_instance *device, 
        int port, 
        int configuration, 
        int interface, 
        int alternate, 
        int index)
{
    struct usb_alternate_instance *alternate_instance;

    if (!(alternate_instance = usbd_device_alternate_instance(device, port, configuration, interface, alternate))) {
        return 0;
    }
    if (index >= alternate_instance->endpoints) {
        return 0;
    }
    return *(alternate_instance->endpoint_transfersize_array+index);
}


/**
 * usbd_device_endpoint_descriptor
 * @device: which device
 * @port: which port
 * @configuration: index to configuration, 0 - N-1 
 * @interface: index to interface
 * @alternate: alternate setting
 * @endpoint: which endpoint
 *
 * Return the specified endpoint descriptor for the specified device.
 */
struct usb_endpoint_descriptor *
usbd_device_endpoint_descriptor(
        struct usb_device_instance *device, 
        int port, 
        int configuration, 
        int interface, 
        int alternate, 
        int endpoint)
{
    struct usb_endpoint_descriptor *endpoint_descriptor;
    int i;

    for (i = 0;
            !(endpoint_descriptor = usbd_device_endpoint_descriptor_index(device, port, configuration, interface, alternate, i)); 
            i++) 
    {
        if (endpoint_descriptor->bEndpointAddress == endpoint) {
            return endpoint_descriptor;
        }
    }
    return NULL;
}

struct usb_otg_descriptor *
usbd_device_otg_descriptor(
        struct usb_device_instance *device,
        int port,
        int configuration,
        int interface,
        int alternate)
{
    struct usb_alternate_instance *alt_instance;

    if (!(alt_instance = usbd_device_alternate_instance(device, port, configuration, interface, alternate))) {
        return 0;
    }

    return (alt_instance->otg_descriptor);
}



/**
 * usbd_alloc_urb_data - allocate urb data buffer
 * @urb: pointer to urb
 * @len: size of buffer to allocate
 *
 * Allocate a data buffer for the specified urb structure.
 */
int usbd_alloc_urb_data(struct usbd_urb *urb, int len)
{
    len += 2;
    urb->buffer_length = len;
    urb->actual_length = 0;
    if (len==0) {
        dbg_urbmem(0, "len == zero");
        return 0;
    }

    if (urb->endpoint &&
	urb->endpoint->endpoint_address && urb->function_instance && 
            urb->function_instance->function_driver->ops->alloc_urb_data) 
    {
        dbg_urbmem(0,"urb->function->ops used");
        return urb->function_instance->function_driver->ops->alloc_urb_data(urb, len);
    }
    return !(urb->buffer = ckmalloc(len, GFP_ATOMIC));
}


/**
 * usbd_alloc_urb - allocate an URB appropriate for specified endpoint
 * @device: device instance
 * @function_instance: device instance
 * @endpoint: endpoint 
 * @length: size of transfer buffer to allocate
 *
 * Allocate an urb structure. The usb device urb structure is used to
 * contain all data associated with a transfer, including a setup packet for
 * control transfers.
 *
 */
struct usbd_urb * usbd_alloc_urb(
        struct usb_device_instance * device, 
        struct usb_function_instance *function_instance,
        __u8 endpoint_address, 
        int length)
{
    int i;

    for (i=0; i < device->bus->driver->max_endpoints; i++) {
        struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + i;

        dbg_urbmem(2,"i=%d epa=%d want %d",i,
                   (endpoint->endpoint_address&0x7f),endpoint_address);
        if ((endpoint->endpoint_address&0x7f) == endpoint_address) {

            struct usbd_urb *urb;

            dbg_urbmem(4,"[%d]: endpoint: %p %02x", i, endpoint, endpoint->endpoint_address);

            if (!(urb = ckmalloc(sizeof(struct usbd_urb), GFP_ATOMIC))) {
                dbg_urbmem(0,"ckmalloc(%u,GFP_ATOMIC) failed",
                           sizeof(struct usbd_urb));
                return NULL;
            }

            urb->endpoint = endpoint;
            urb->device = device;
            urb->function_instance = function_instance;
            urb_link_init(&urb->link);

            if (length) {
                if (usbd_alloc_urb_data(urb, length)) {
                    dbg_urbmem(0,"usbd_alloc_urb_data(%u,GFP_ATOMIC) failed",
                               length);
                    kfree(urb);
                    return NULL;
                }
            }
            else {
                urb->buffer_length = urb->actual_length = 0;
            }
            return urb;
        }
    }   
    printk ("can't find endpoint #%02x!\n",endpoint_address);
    dbg_urbmem(0,"can't find endpoint #%02x!",endpoint_address);
    for (i=0; i < device->bus->driver->max_endpoints; i++) {
        struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + i;
        dbg_urbmem(0,"i=%d epa=%d want %d",i,
                   (endpoint->endpoint_address&0x7f),endpoint_address);
        if ((endpoint->endpoint_address&0x7f) == endpoint_address) {
            dbg_urbmem(0,"found it, but too late");
            break;
        }
    }
    return NULL;
}


/**
 * usbd_device_event - called to respond to various usb events
 * @device: pointer to struct device
 * @event: event to respond to
 *
 * Used by a Bus driver to indicate an event.
 */
void usbd_device_event_irq(struct usb_device_instance *device, usb_device_event_t event, int data)
{
    struct usbd_urb *urb;

    usb_device_state_t  state;

    if (!device || !device->bus) {
        dbg_usbe(1,"(%p,%d) NULL device or device->bus",device,event);
        return;
    }

    state = device->device_state;

    dbg_usbe(3,"-------------------------------------------------------------------------------------");

    switch(event) {
	    case DEVICE_UNKNOWN:
	        dbg_usbe(1, "%s", usbd_device_events[DEVICE_UNKNOWN]);
	        break;
	    case DEVICE_INIT:
	        dbg_usbe(3, "%s", usbd_device_events[DEVICE_INIT]);
	        device->device_state = STATE_INIT;
	        break;

	    case DEVICE_CREATE:
	        dbg_usbe(3, "%s", usbd_device_events[DEVICE_CREATE]);
	        device->device_state = STATE_ATTACHED;
	        break;

	    case DEVICE_HUB_CONFIGURED:
	        dbg_usbe(3, "%s", usbd_device_events[DEVICE_HUB_CONFIGURED]);
	        device->device_state = STATE_POWERED;
	        break;

	    case DEVICE_RESET:
	        dbg_usbe(1, "%s", usbd_device_events[DEVICE_RESET]);
	        device->device_state = STATE_DEFAULT;
	        device->address = 0;
	        break;

	    case DEVICE_ADDRESS_ASSIGNED:
	        dbg_usbe(1, "%s", usbd_device_events[DEVICE_ADDRESS_ASSIGNED]);
	        device->device_state = STATE_ADDRESSED;
	        break;

	    case DEVICE_CONFIGURED:
	        dbg_usbe(1, "%s",  usbd_device_events[DEVICE_CONFIGURED]);
	        device->device_state = STATE_CONFIGURED;
	        break;

	    case DEVICE_DE_CONFIGURED:
	        dbg_usbe(1, "%s",  usbd_device_events[DEVICE_DE_CONFIGURED]);
	        device->device_state = STATE_ADDRESSED;
	        break;

	    case DEVICE_BUS_INACTIVE:
	        dbg_usbe(1,  "%s", usbd_device_events[DEVICE_BUS_INACTIVE]);
	        if (device->status != USBD_CLOSING) {
	            device->status = USBD_SUSPENDED;
	        }
	        break;
	    case DEVICE_BUS_ACTIVITY:
	        dbg_usbe(1, "%s", usbd_device_events[DEVICE_BUS_ACTIVITY]);
	        if (device->status != USBD_CLOSING) {
	            device->status = USBD_OK;
	        }
	        break;

	    case DEVICE_SET_INTERFACE:
	        dbg_usbe(1,  "%s", usbd_device_events[DEVICE_SET_INTERFACE]);
	        break;
	    case DEVICE_SET_FEATURE:
	        dbg_usbe(1,  "%s", usbd_device_events[DEVICE_SET_FEATURE]);
	        break;
	    case DEVICE_CLEAR_FEATURE:
	        dbg_usbe(1,  "%s", usbd_device_events[DEVICE_CLEAR_FEATURE]);
	        break;

	    case DEVICE_POWER_INTERRUPTION:
	        dbg_usbe(1,  "%s", usbd_device_events[DEVICE_POWER_INTERRUPTION]);
	        device->device_state = STATE_POWERED;
	        break;
	    case DEVICE_HUB_RESET:
	        dbg_usbe(1,  "%s", usbd_device_events[DEVICE_HUB_RESET]);
	        device->device_state = STATE_ATTACHED;
	        break;
	    case DEVICE_DESTROY:
	        dbg_usbe(1,  "%s", usbd_device_events[DEVICE_DESTROY]);
	        device->device_state = STATE_UNKNOWN;
	        break;
            
        case DEVICE_BUS_REQUEST:
        
            break;
            
        case DEVICE_BUS_RELEASE:
        
            break;
            
        case DEVICE_ACCEPT_HNP:
        
            break;
            
        case DEVICE_REQUEST_SRP:
        
            break;
	    case DEVICE_FUNCTION_PRIVATE:
	        dbg_usbe(1,  "%s", usbd_device_events[DEVICE_FUNCTION_PRIVATE]);
	        break;
    }

    dbg_usbe(3,"%s event: %d oldstate: %d newstate: %d status: %d address: %d", 
            device->name, event, state, device->device_state, device->status, device->address);;

    /* tell the bus interface driver */
    if (device->bus && device->bus->driver->ops->device_event) {
        dbg_usbe(3, "calling bus->event");
        device->bus->driver->ops->device_event(device, event, data);
    }

#if 0
    if (device->function_instance_array && (device->function_instance_array+port)->function_driver->ops->event) {
        dbg_usbe(3, "calling function->event");
        (device->function_instance_array+port)->function_driver->ops->event(device, event);
    }
#endif

    if (device->ep0 && device->ep0->function_driver->ops->event) {
        dbg_usbe(3, "calling ep0->event");
        device->ep0->function_driver->ops->event(device, event, data);
    }

#if 0
    /* tell the bus interface driver */
    if (device->bus && device->bus->driver->ops->device_event) {
        dbg_usbe(3, "calling bus->event");
        device->bus->driver->ops->device_event(device, event);
    }
#endif

    /* dbg_usbe(0, "FINISHED - NO FUNCTION BH"); */
    /* return; */

    /* XXX queue an urb to endpoint zero */
    if ((urb = usbd_alloc_urb(device, device->function_instance_array, 0, 0))) {
        urb->event = event;
        urb->data = data;
        urb_append_irq(&(urb->endpoint->events), urb);
        usbd_schedule_function_bh(device);
    }
    dbg_usbe(5, "FINISHED");
    return;
}

void usbd_device_event(struct usb_device_instance *device, usb_device_event_t event, int data)
{
    unsigned long flags;
    local_irq_save(flags);
    usbd_device_event_irq(device, event, data);
    local_irq_restore(flags);
}

/**
 * usbd_endpoint_halted
 * @device: point to struct usb_device_instance
 * @endpoint: endpoint to check
 *
 * Return non-zero if endpoint is halted.
 */
int usbd_endpoint_halted(struct usb_device_instance *device, int endpoint)
{
    if (!device->bus->driver->ops->endpoint_halted) {
        return 1;
    }
    return device->bus->driver->ops->endpoint_halted(device, endpoint);
}


/**
 * usbd_device_feature - set usb device feature
 * @device: point to struct usb_device_instance
 * @endpoint: which endpoint
 * @feature: feature value
 *
 * Return non-zero if endpoint is halted.
 */
int usbd_device_feature(struct usb_device_instance *device, int endpoint, int feature)
{
    if (!device->bus->driver->ops->device_feature) {
        return 1;
    }
    return device->bus->driver->ops->device_feature(device, endpoint, feature);
}

#ifdef CONFIG_USBD_PROCFS
/* Proc Filesystem *************************************************************************** */
/* *
 * dohex
 *
 */
static void dohexdigit(char *cp, unsigned char val) 
{
    if (val < 0xa) {
        *cp = val + '0';
    }
    else if ((val >= 0x0a) && (val <= 0x0f)) {
        *cp = val - 0x0a + 'a';
    }
}

/* *
 * dohex
 *
 */
static void dohexval(char *cp, unsigned char val) 
{
    dohexdigit(cp++, val>>4);
    dohexdigit(cp++, val&0xf);
}

/* *
 * dump_descriptor
 */
static int dump_descriptor(char *buf, char *sp, int num)
{
    int len = 0;
    while (sp && num--) {
        dohexval(buf, *sp++);
        buf += 2;
        *buf++ = ' ';
        len += 3;
    }
    len++;
    *buf = '\n';
    return len;
}


/* *
 * usbd_device_proc_read - implement proc file system read.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Standard proc file system read function.
 *
 * We let upper layers iterate for us, *pos will indicate which device to return
 * statistics for.
 */
static ssize_t
usbd_device_proc_read_functions(struct file *file, char *buf, size_t count, loff_t * pos)
{
    unsigned long   page;
    int len = 0;
    int index;

    struct list_head *lhd;
    struct usb_function_driver *function_driver;
    struct usb_device_instance *device;

    /* get a page, max 4095 bytes of data... */
    if (!(page = get_free_page(GFP_KERNEL))) {
        return -ENOMEM;
    }

    len = 0;
    index = (*pos)++;

    if (index == 0) {
        len += sprintf((char *)page+len, "usb-device list\n");
    }

    /* read_lock(&usb_device_rwlock); */
    if (index == 1) {
        list_for_each(lhd, &function_drivers) {
            function_driver = list_entry_func(lhd);
            if (function_driver->configuration_instance_array ) {
#if 0
                {
                    int configuration;
                    /* max 4095 bytes of data... */
                    len += sprintf((char *)page+len, "Function: %s", function_driver->name);
                    len += sprintf((char *)page+len, "\tDescriptors: %d", function_driver->configurations);
                    len += sprintf((char *)page+len, "\n");

                    for (configuration = 0; configuration < function_driver->configurations;configuration++) {
                        int interface;
                        struct usb_configuration_description *configuration = &function_driver->configuration_description[configuration];
                        len += sprintf((char *)page+len, "\tConfig: %d %s\n", configuration, configuration->iConfiguration);

                        for (interface = 0; interface < configuration->interfaces; interface++) {

                            int alternate;
                            struct usb_interface_instance *interface = &configuration->interface_instance_array[interface];

                            int endpoint;
                            struct usb_interface_description *interface = &configuration->interface_list[interface];
                            len += sprintf((char *)page+len, "\t\tInterface: %d\n", interface);


                            for (endpoint = 0; endpoint < interface->endpoints; endpoint++) {
                                /* XXX */
                                struct usb_endpoint_description *endpoint = &interface->endpoint_list[endpoint];
                                len += sprintf((char *)page+len, "\t\t\tEndpoint: %d %d\n", endpoint, endpoint->bEndpointAddress);
                            }

                        }
                    }

                    break;
                }
#endif
                len += sprintf((char *)page+len, "\nDevice descriptor                  ");
                len += dump_descriptor((char *)page+len, (char *)function_driver->device_descriptor,
                        sizeof(struct usb_device_descriptor));
                { 
                    int configuration;
                    struct usb_configuration_instance *configuration_instance_array =
                        function_driver->configuration_instance_array;

                    for (configuration=0; configuration < function_driver->configurations;configuration++) {

                        int interface;
                        struct usb_configuration_descriptor *configuration_descriptor = 
                            configuration_instance_array[configuration].configuration_descriptor;


                        len += sprintf((char *)page+len, "\nConfiguration descriptor [%d      ] ", configuration);
                        len += dump_descriptor((char *)page+len, (char *)configuration_descriptor, 
                                sizeof(struct usb_configuration_descriptor));

                        for (interface=0;interface< configuration_instance_array[configuration].interfaces;interface++) {

                            int alternate;
                            /* int class; */
                            struct usb_interface_instance *interface_instance =
                                configuration_instance_array[configuration].interface_instance_array + interface;

                            dbg_init(1, "interface: %d:%d alternates: %d", configuration, interface, interface_instance->alternates);

                            for (alternate = 0; alternate < interface_instance->alternates; alternate++) {

                                int endpoint;
                                int class;
                                struct usb_alternate_instance *alternate_instance =
                                    interface_instance->alternates_instance_array + alternate;

                                struct usb_interface_descriptor *interface_descriptor =
                                    alternate_instance->interface_descriptor;

                                dbg_init(1, "alternate: %d:%d:%d classes: %d", configuration, interface, alternate,
                                        alternate_instance->classes);

                                len += sprintf((char *)page+len, "\nInterface descriptor     [%d:%d:%d  ] ", 
                                        configuration, interface+1, alternate);

                                len += dump_descriptor((char *)page+len, (char *)interface_descriptor, 
                                        sizeof(struct usb_interface_descriptor));

                                for (class = 0; class < alternate_instance->classes; class++) {
                                    struct usb_class_descriptor *class_descriptor = 
                                        alternate_instance->classes_descriptor_array[class];
                                    len += sprintf((char *)page+len, "Class descriptor         [%d:%d:%d  ] ", 
                                            configuration, interface+1, class);

                                    len += dump_descriptor((char *)page+len, (char *)class_descriptor, 
                                            *(char *)class_descriptor);
                                }


                                dbg_init(1, "alternate: %d:%d:%d endpoints: %d", configuration, interface, alternate,
                                        alternate_instance->endpoints);

                                for (endpoint=0; endpoint < alternate_instance->endpoints; endpoint++) {

                                    struct usb_endpoint_descriptor *endpoint_descriptor =
                                        alternate_instance->endpoints_descriptor_array[endpoint];

                                    dbg_init(1, "endpoint: %d:%d:%d:%d", configuration, interface, alternate, endpoint);

                                    len += sprintf((char *)page+len, "Endpoint descriptor      [%d:%d:%d:%d] ", 
                                            configuration, interface+1, alternate, endpoint);

                                    len += dump_descriptor((char *)page+len, (char *)endpoint_descriptor, 
                                            sizeof(struct usb_endpoint_descriptor));
                                }
                            }
                        }
                    }
                    break;
                }
            }

        }
    }
    else if (index == 2) {
        int i;
        int k;
        struct usb_string_descriptor *string_descriptor;

        len += sprintf((char *)page+len, "\n\n");

        if ((string_descriptor = usbd_get_string(0))!=NULL) {
            len += sprintf((char *)page+len, "String                   [%2d]      ", 0);

            for (k = 0; k < (string_descriptor->bLength/2)-1;k++) {
                len += sprintf((char *)page+len, "%02x %02x ",  
                        string_descriptor->wData[k] >> 8,
                        string_descriptor->wData[k] & 0xff
                        );
                len++;
            }
            len += sprintf((char *)page+len, "\n");
        }

        for (i=1;i<maxstrings;i++) {

            if ((string_descriptor = usbd_get_string(i))!=NULL) {


                    len += sprintf((char *)page+len, "String                   [%2d]      ", i);

                    /* bLength = sizeof(struct usb_string_descriptor) 
				+ 2*strlen(str)-2; */

                    for (k = 0; k < (string_descriptor->bLength/2)-1;k++) {
                        *(char *)(page+len) = (char)string_descriptor->wData[k];
                        len++;
                    }
                    len += sprintf((char *)page+len, "\n");
                }
        }
    }

    else if (index==3) {
        list_for_each(lhd, &devices) {
            device = list_entry_device(lhd);

            len += sprintf((char *)page+len, "\n\nBus: %s", device->bus->driver->name);
            if (device->ep0) {
                len += sprintf((char *)page+len, "  ep0:  %s", device->ep0->function_driver->name);
            }
            if (device->function_instance_array) {
                len += sprintf((char *)page+len, "  func: %s", device->function_instance_array->function_driver->name);
            }
            len += sprintf((char *)page+len, "   state: %s", usbd_device_states[device->device_state]);
            len += sprintf((char *)page+len, "   addr: %d", device->address);
            len += sprintf((char *)page+len, "   conf: %d", device->configuration);
            len += sprintf((char *)page+len, "   int:  %d", device->interface);
            len += sprintf((char *)page+len, "   alt:  %d", device->alternate);

            len += sprintf((char *)page+len, "\n");
            break;
        }
        len += sprintf((char *)page+len, "\n");
    }

    /* read_unlock(&usb_device_rwlock); */

    if (len > count) {
        len = -EINVAL;
    }
    else if (len > 0 && copy_to_user(buf, (char *) page, len)) {
        len = -EFAULT;
    }
    free_page(page);
    return len;
}

static struct file_operations usbd_device_proc_operations_functions = {
    read: usbd_device_proc_read_functions,
};


/* *
 * usbd_device_proc_read - implement proc file system read.
 * @file: xx 
 * @buf:  xx
 * @count:  xx
 * @pos:  xx
 *
 * Standard proc file system read function.
 *
 * We let upper layers iterate for us, *pos will indicate which device to return
 * statistics for.
 */
static ssize_t
usbd_device_proc_read_devices(struct file *file, char *buf, size_t count, loff_t * pos)
{
    unsigned long   page;
    int len = 0;
    int index;

    struct list_head *lhd;
    struct usb_device_instance *device;

    /* get a page, max 4095 bytes of data... */
    if (!(page = get_free_page(GFP_KERNEL))) {
        return -ENOMEM;
    }

    len = 0;
    index = (*pos)++;

    if (index == 0) {
        len += sprintf((char *)page+len, "usb-device list\n");
    }

    /* read_lock(&usb_device_rwlock); */

    if (index > 0) {

        list_for_each(lhd, &devices) {
            int configuration;

            device = list_entry_device(lhd);
            if (--index == 0) {
                /* max 4095 bytes of data... */
                len += sprintf((char *)page+len, "Device: %s", device->bus->driver->name);
                len += sprintf((char *)page+len, "\n");
                len += sprintf((char *)page+len, "%20s[        ] ", "Device:");
                len += dump_descriptor((char *)page+len, (char *)device->device_descriptor, 
                            sizeof(struct usb_device_descriptor));
                break;
            }

            /* iterate across configurations for this device */
            for (configuration = 0; index > 0 ; configuration++) {
                int interface;

                struct usb_configuration_descriptor *configuration_descriptor;

                if (!(configuration_descriptor = usbd_device_configuration_descriptor(device, 0, configuration))) {
                    break;
                }

                if (--index == 0) {
                    /* max 4095 bytes of data... */
                    len += sprintf((char *)page+len, "%20s[%2d      ] ", "Configuration:", configuration);
                    len += dump_descriptor((char *)page+len, (char *)configuration_descriptor, 
                            sizeof(struct usb_configuration_descriptor));
                    break;
                }

                // iterate across interfaces for this configurations
                for (interface = 0; index > 0 ; interface++) {
                    int alternate;
                    struct usb_interface_instance *interface_instance;

                    if (!(interface_instance = usbd_device_interface_instance(device, 0, configuration, interface))) {
                        break;
                    }

                    // itererate across alternates for this interafce
                    for (alternate = 0; index > 0; alternate++) {

                        int endpoint;
                        struct usb_interface_descriptor *interface_descriptor;

                        if (!(interface_descriptor = usbd_device_interface_descriptor(device, 0, 
                                        configuration, interface, alternate))) 
                        {
                            break;
                        }

                        if (--index == 0) {
                            // max 4095 bytes of data...
                            len += sprintf((char *)page+len, "%20s[%2d:%2d   ] ",  "Interface", configuration, interface);
                            len += dump_descriptor((char *)page+len, (char *)interface_descriptor, 
                                    sizeof(struct usb_interface_descriptor));
                            break;
                        }

                        // iterate across endpoints for this interface
                        for (endpoint = 0; index > 0 ;endpoint++) {
                            struct usb_endpoint_descriptor *endpoint_descriptor;

                            if (!(endpoint_descriptor = usbd_device_endpoint_descriptor_index(device, 0, 
                                            configuration, interface, alternate, endpoint))) {
                                break;
                            }
                            if (--index == 0) {
                                len += sprintf((char *)page+len, "%20s[%2d:%2d:%2d] ", "Endpoint", 
                                        configuration, interface, endpoint);

                                len += dump_descriptor((char *)page+len, (char *)endpoint_descriptor, 
                                        sizeof(struct usb_endpoint_descriptor));
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (index > 0) {
        int i;
        for (i=1;i<maxstrings;i++) {
            struct usb_string_descriptor *string_descriptor;

            if ((string_descriptor = usbd_get_string(i))!=NULL) {

                if (--index == 0) {
                    int k;

                    len += sprintf((char *)page+len, "%20s[%2d] ", "String", i);

                    // bLength = sizeof(struct usb_string_descriptor) + 2*strlen(str)-2;

                    for (k = 0; k < (string_descriptor->bLength/2)-1;k++) {
                        *(char *)(page+len) = (char)string_descriptor->wData[k];
                        len++;
                    }
                    len += sprintf((char *)page+len, "\n");
                }
            }
        }
    }

    //read_unlock(&usb_device_rwlock);

    if (len > count) {
        len = -EINVAL;
    }
    else if (len > 0 && copy_to_user(buf, (char *) page, len)) {
        len = -EFAULT;
    }
    free_page(page);
    return len;
}

static struct file_operations usbd_device_proc_operations_devices = {
    read:usbd_device_proc_read_devices,
};
#endif



/* Module init ******************************************************************************* */


unsigned intset(unsigned int a, unsigned b)
{
    return(a?a:b);
}

char * strset(char *s,char *d) {
    return((s&&strlen(s)?s:d));
}

int __init usbd_device_init(void)
{
#if 0
    debug_option *op;
#endif

#if 0
    if (NULL != (op = find_debug_option(dbg_table,"hot"))) {
        op->sub_table = usbd_hotplug_get_dbg_table();
    }
    if (NULL != (op = find_debug_option(dbg_table,"mon"))) {
        op->sub_table = usbd_monitor_get_dbg_table();
    }
#endif
    if (0 != scan_debug_options("usb-device",dbg_table,dbg)) {
        return(-EINVAL);
    }


#ifdef CONFIG_USBD_PROCFS
    {
        struct proc_dir_entry *p;

        // create proc filesystem entries
        if ((p = create_proc_entry("usb-functions", 0, 0)) == NULL)
            return -ENOMEM;
        p->proc_fops = &usbd_device_proc_operations_functions;

        if ((p = create_proc_entry("usb-devices", 0, 0)) == NULL)
            return -ENOMEM;
        p->proc_fops = &usbd_device_proc_operations_devices;
    }
#endif
    return 0;
}

#if 0
static void __exit usbd_device_exit(void) 
{
#ifdef CONFIG_USBD_PROCFS
    // remove proc filesystem entry
    remove_proc_entry("usb-functions", NULL);
    remove_proc_entry("usb-devices", NULL);
#endif
}

module_init(usbd_device_init);
module_exit(usbd_device_exit);
#endif

