/*
 * linux/drivers/usbd/usbd-bi.c - USB Bus Interface Driver
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
#include <linux/module.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>

#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>

#include <linux/netdevice.h>
#include <linux/proc_fs.h>

#include <asm/uaccess.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif


#include "../usbd.h"
#include "../usbd-debug.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"
#include "../../cy7c67200_300_common.h"
#include "../../cy7c67200_300_otg.h"
#include "cy7c67200_300_pcd.h"



/* Module Parameters ************************************************************************* */

//static char *dbg = NULL;
static char *dbg = "init=0:intr=0:tick=0:usbe=0:rx=0:tx=0:dma=0:setup=0:ep0=0:udc=0:stall=0:pm=0:pur=0";

MODULE_PARM(dbg, "s");

/* Debug switches (module parameter "dbg=...") *********************************************** */

int      dbgflg_usbdbi_init;
int      dbgflg_usbdbi_intr;
int      dbgflg_usbdbi_tick;
int      dbgflg_usbdbi_usbe;
int      dbgflg_usbdbi_rx;
int      dbgflg_usbdbi_tx;
int      dbgflg_usbdbi_dma_flg;
int      dbgflg_usbdbi_setup;
int      dbgflg_usbdbi_ep0;
int      dbgflg_usbdbi_udc;
int      dbgflg_usbdbi_stall;
int      dbgflg_usbdbi_pm;
int      dbgflg_usbdbi_pur;


static debug_option dbg_table[] = {
    {&dbgflg_usbdbi_init,NULL,"init","initialization and termination"},
    {&dbgflg_usbdbi_intr,NULL,"intr","interrupt handling"},
    {&dbgflg_usbdbi_tick,NULL,"tick","interrupt status monitoring on clock tick"},
    {&dbgflg_usbdbi_usbe,NULL,"usbe","USB events"},
    {&dbgflg_usbdbi_rx,NULL,"rx","USB RX (host->device) handling"},
    {&dbgflg_usbdbi_tx,NULL,"tx","USB TX (device->host) handling"},
    {&dbgflg_usbdbi_dma_flg,NULL,"dma","DMA handling"},
    {&dbgflg_usbdbi_setup,NULL,"setup","Setup packet handling"},
    {&dbgflg_usbdbi_ep0,NULL,"ep0","End Point 0 packet handling"},
    {&dbgflg_usbdbi_udc,NULL,"udc","USB Device"},
    {&dbgflg_usbdbi_stall,NULL,"stall","Testing"},
    {&dbgflg_usbdbi_pm,NULL,"pm","Power Management"},
    {&dbgflg_usbdbi_pur,NULL,"pur","USB cable Pullup Resistor"},
    {NULL,NULL,NULL,NULL}
};


/* globals */
int have_cable_irq;

/* ticker */
int ticker_terminating;
int ticker_timer_set;

/**
 * kickoff_thread - start management thread
 */
void ticker_kickoff(void);

/**
 * killoff_thread - stop management thread
 */
void ticker_killoff(void);

#ifdef CONFIG_PM
struct pm_dev      *pm_dev;                 // power management
#endif


/* Bus Interface Callback Functions ********************************************************** */

/**
 * bi_cancel_urb - cancel sending an urb
 * @urb: data urb to cancel
 *
 * Used by the USB Device Core to cancel an urb.
 */
int bi_cancel_urb(struct usbd_urb *urb)
{
    return 0;
}


/**
 * bi_endpoint_halted - check if endpoint halted
 * @device: device
 * @endpoint: endpoint to check
 *
 * Used by the USB Device Core to check endpoint halt status.
 */
int bi_endpoint_halted(struct usb_device_instance *device, int endpoint)
{
    return 0;
}


/**
 * bi_device_feature - handle set/clear feature requests
 * @device: device
 * @endpoint: endpoint to check
 * @flag: set or clear
 *
 * Used by the USB Device Core to check endpoint halt status.
 */
int bi_device_feature(struct usb_device_instance *device, int endpoint, int flag)
{
    if (flag) {
        dbg_ep0(1,"stalling endpoint");
        // XXX logical to physical?
        udc_stall_ep(endpoint, device);
        return 0;
    }

    dbg_ep0(1,"reseting endpoint %d", endpoint);

    udc_reset_ep(endpoint, device);
    return 0;
}

/*
 * bi_disable_endpoints - disable udc and all endpoints
 */
static void bi_disable_endpoints(struct usb_device_instance *device)
{
    int i;
    if (device && device->bus && device->bus->endpoint_array) {
        for (i = 0; i < udc_max_endpoints(); i++) {
            struct usb_endpoint_instance *endpoint;
            if ((endpoint = device->bus->endpoint_array + i)) {
                usbd_flush_ep(endpoint);
            }
        }
    }
}

/* bi_config - commission bus interface driver
 */
static int bi_config(struct usb_device_instance *device)
{
    int i;

    struct usb_interface_descriptor *interface;
    struct usb_endpoint_descriptor *endpoint_descriptor;

    int found_tx = 0;
    int found_rx = 0;

    dbg_init(1,"checking config: config: %d interface: %d alternate: %d",
            device->configuration, device->interface, device->alternate);

    bi_disable_endpoints(device);

    // audit configuration for compatibility
    if (!(interface = usbd_device_interface_descriptor(device,
                    0, device->configuration, device->interface, device->alternate)))
    {
        dbg_init(0,"cannot fetch interface descriptor c:%d i:%d a:%d",
                 device->configuration, device->interface, device->alternate);
        return -EINVAL;
    }


    dbg_init(2, "endpoints: %d", interface->bNumEndpoints);

    // iterate across all endpoints for this configuration and verify they are valid
    for (i = 0; i < interface->bNumEndpoints; i++) {
        int transfersize;

        int physical_endpoint;
        int logical_endpoint;

        //dbg_init(0, "fetching endpoint %d", i);
        if (!(endpoint_descriptor = usbd_device_endpoint_descriptor_index(device, 0,
                device->configuration, device->interface, device->alternate, i)))
        {
            dbg_init(0, "cannot fetch endpoint descriptor: %d", i);
            continue;
        }

        // XXX check this
        transfersize = usbd_device_endpoint_transfersize(device, 0,
                device->configuration, device->interface, device->alternate, i);



        logical_endpoint = endpoint_descriptor->bEndpointAddress;

        if (!(physical_endpoint = udc_check_ep(logical_endpoint,
                                               endpoint_descriptor->wMaxPacketSize,
                                               device))) {
            dbg_init(2, "endpoint[%d]: l: %02x p: %02x transferSize: %d packetSize: %02x INVALID",
                    i, logical_endpoint, physical_endpoint,
                    transfersize, endpoint_descriptor->wMaxPacketSize);

            dbg_init(2, "invalid endpoint: %d %d", logical_endpoint, physical_endpoint);
            return -EINVAL;
        }
        else {
            struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + physical_endpoint;

            dbg_init(2, "endpoint[%d]: l: %02x p: %02x transferSize: %d packetSize: %02x FOUND",
                     i, logical_endpoint, physical_endpoint,
                     transfersize, endpoint_descriptor->wMaxPacketSize);

            dbg_init(2,"epd->bEndpointAddress=%d",
                     endpoint_descriptor->bEndpointAddress);
            endpoint->endpoint_address = endpoint_descriptor->bEndpointAddress;

            if (endpoint_descriptor->wMaxPacketSize > 64) {
                dbg_init(0,"incompatible with endpoint size: %x",
                        endpoint_descriptor->wMaxPacketSize);
                return -EINVAL;
            }

            if (endpoint_descriptor->bEndpointAddress & IN) {
                found_tx++;
                endpoint->tx_attributes = endpoint_descriptor->bmAttributes;
                endpoint->tx_transferSize = transfersize&0xfff;
                endpoint->tx_packetSize = endpoint_descriptor->wMaxPacketSize;
                endpoint->last = 0;
                endpoint->tx_urb = NULL;
            }
            else {
                found_rx++;
                endpoint->rcv_attributes = endpoint_descriptor->bmAttributes;
                endpoint->rcv_transferSize = transfersize&0xfff;
                endpoint->rcv_packetSize = endpoint_descriptor->wMaxPacketSize;
                endpoint->rcv_urb = NULL;
            }
        }
    }

    if (device->status == USBD_OK) {
        for (i = 1; i < device->bus->driver->max_endpoints; i++) {

            struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + i;

            // udc_setup_ep(device, i, endpoint->endpoint_address?  endpoint : NULL);
            udc_setup_ep(device, i, endpoint);
        }
    }

    return 0;
}


/**
 * bi_device_event - handle generic bus event
 * @device: device pointer
 * @event: interrupt event
 *
 * Called by usb core layer to inform bus of an event.
 */
int bi_device_event(struct usb_device_instance *device, usb_device_event_t event, int data)
{
    sie_info * sie_data;
    cy_priv_t * cy_priv;
    otg_t * otg;

    sie_data = (sie_info *) device->bus->privdata;
    cy_priv = (cy_priv_t *) sie_data->cy_priv;
    otg = (otg_t *) cy_priv->otg;

    //printk(KERN_DEBUG "bi_device_event: event: %d\n", event);

    if (!device) {
        return 0;
    }
    switch (event) {
    case DEVICE_UNKNOWN:
        break;
    case DEVICE_INIT:
        break;
    case DEVICE_CREATE:         // XXX should this stuff be in DEVICE_INIT?

        bi_config(device);

        // enable udc, enable interrupts, enable connect
        udc_enable(device);
        udc_all_interrupts(device);
        udc_connect();
        break;

    case DEVICE_HUB_CONFIGURED:
        break;

    case DEVICE_RESET:
        device->address = 0;
        udc_set_address(device->address, device);
        udc_reset_ep(0, device);
        udc_all_interrupts(device); // XXX
        break;


    case DEVICE_ADDRESS_ASSIGNED:
        udc_set_address(device->address, device);
        device->status = USBD_OK;
        break;

    case DEVICE_CONFIGURED:
        bi_config(device);
        break;

    case DEVICE_DE_CONFIGURED:
        udc_reset_ep(1, device);
        udc_reset_ep(2, device);
        udc_reset_ep(3, device);
        break;

    case DEVICE_SET_INTERFACE:
        bi_config(device);
        break;

    case DEVICE_SET_FEATURE:
        break;

    case DEVICE_CLEAR_FEATURE:
        break;

    case DEVICE_BUS_INACTIVE:
        // disable suspend interrupt
        udc_suspended_interrupts(device);

        // XXX check on linkup and sl11
        // if we are no longer connected then force a reset
        if (!udc_connected()) {
            usbd_device_event_irq(device, DEVICE_RESET, 0);
        }
        break;

    case DEVICE_BUS_ACTIVITY:
        // enable suspend interrupt
        udc_all_interrupts(device);
        break;

    case DEVICE_POWER_INTERRUPTION:
        break;

    case DEVICE_HUB_RESET:
        break;

    case DEVICE_DESTROY:
        udc_disconnect();
        bi_disable_endpoints(device);
        udc_disable_interrupts(device);
        udc_disable();
        break;

    case DEVICE_BUS_REQUEST:
        otg->b_bus_req = TRUE;
        update_otg_state(otg);
        break;

    case DEVICE_BUS_RELEASE:
        otg->b_bus_req = FALSE;
        update_otg_state(otg);
        break;

    case DEVICE_RCV_URB_RECYCLED:
        udc_rcv_urb_recycled();
        break;

    case DEVICE_ACCEPT_HNP:
        otg->b_hnp_en = TRUE;
        update_otg_state(otg);
    break;

    case DEVICE_REQUEST_SRP:
        //otg->srp_start_flag = TRUE;
        otg->b_bus_req = TRUE;
        otg->b_se0_srp = TRUE;
        update_otg_state(otg);
    break;

    case DEVICE_FUNCTION_PRIVATE:
        break;
    }

    return 0;
}

/**
 * bi_send_urb - start transmit
 * @urb:
 */
int bi_send_urb(struct usbd_urb *urb)
{
    unsigned long flags;
    dbg_tx(4, "urb: %p", urb);
    local_irq_save(flags);
    if (urb && urb->endpoint && !urb->endpoint->tx_urb) {
        dbg_tx(2, "urb: %p endpoint: %x", urb, urb->endpoint->endpoint_address);
        usbd_tx_complete_irq(urb->endpoint, 0);
        udc_start_in_irq(urb->endpoint, urb->device);
    }
    local_irq_restore(flags);

    // Shouldn't need to make this atomic, all we need is a change indicator
    urb->device->usbd_rxtx_timestamp = jiffies;

    return 0;
}


#ifdef CONFIG_PM
static int bi_pm_event(struct pm_dev *pm_dev, pm_request_t request, void *unused);
#endif



struct usb_bus_operations bi_ops = {
    send_urb: bi_send_urb,
    cancel_urb: bi_cancel_urb,
    endpoint_halted: bi_endpoint_halted,
    device_feature: bi_device_feature,
    device_event: bi_device_event,
};

struct usb_bus_driver bi_driver = {
    name: "",
    max_endpoints: 0,
    ops: &bi_ops,
    this_module: THIS_MODULE,
};


struct usb_bus_driver * bi_retrieve_bus_driver()
{
    return(&bi_driver);
}


/* Bus Interface Received Data *************************************************************** */

struct usb_device_instance *device_array[MAX_DEVICES];


/* Bus Interface Support Functions *********************************************************** */

/**
 * udc_cable_event - called from cradle interrupt handler
 */
void udc_cable_event(void)
{
    struct usb_bus_instance *bus;
    struct usb_device_instance *device;
    struct bi_data *data;

    dbgENTER(dbgflg_usbdbi_init,1);

    // sanity check
    if (!(device = device_array[0]) || !(bus = device->bus) || !(data = bus->privdata)) {
        return;
    }

    {
        unsigned long flags;
        local_irq_save(flags);
        if (udc_connected()) {
            dbg_init(1, "state: %d connected: %d", device->device_state, 1);;
            if (device->device_state == STATE_ATTACHED) {
                dbg_init(1, "LOADING");
                usbd_device_event_irq(device, DEVICE_HUB_CONFIGURED, 0);
                usbd_device_event_irq(device, DEVICE_RESET, 0);
            }
        }
        else {
            dbg_init(1, "state: %d connected: %d", device->device_state, 0);;
            if (device->device_state != STATE_ATTACHED) {
                dbg_init(1, "UNLOADING");
                usbd_device_event_irq(device, DEVICE_RESET, 0);
                usbd_device_event_irq(device, DEVICE_POWER_INTERRUPTION, 0);
                usbd_device_event_irq(device, DEVICE_HUB_RESET, 0);
            }
        }
        local_irq_restore(flags);
    }
    dbgLEAVE(dbgflg_usbdbi_init,1);
}


/* Module Init - the device init and exit routines ******************************************* */

/*
  bi_udc_init - initialize USB Device Controller

  This function is called for each physical bus interface that is found.

  Register an interrupt handler and IO region. Return non-zero for error.
 */
int bi_udc_init()
{
    dbg_init(1,"Loading %s", udc_name());

    bi_driver.name = udc_name();
    bi_driver.max_endpoints = udc_max_endpoints();
    bi_driver.maxpacketsize = udc_ep0_packetsize();

    dbg_init(1,"name: %s endpoints: %d ep0: %d", bi_driver.name, bi_driver.max_endpoints, bi_driver.maxpacketsize);

    // request device IRQ.
    if (udc_request_udc_irq()) {
        dbg_init(0,"name: %s request udc irq failed", udc_name());
        return -EINVAL;
    }

    // request device IO
    if (udc_request_io()) {
        udc_release_udc_irq();
        dbg_init(0,"name: %s request udc io failed", udc_name());
        return -EINVAL;
    }

    if (udc_init()) {
        udc_release_udc_irq();
        udc_release_io();
        dbg_init(1,"name: %s probe failed", udc_name());
        return -EINVAL;
    }

    return 0;
}


/**
 * bi_udc_exit - Stop using the USB Device Controller
 *
 * Stop using the USB Device Controller.
 *
 * Shutdown and free dma channels, de-register the interrupt handler.
 */
void bi_udc_exit(void)
{
    int i;

    dbg_init(1, "Unloading %s", udc_name());

    for (i = 0; i < udc_max_endpoints(); i++) {
        udc_disable_ep(i);
    }

    // free io and irq
    udc_disconnect();
    udc_disable();
    udc_release_io();
    udc_release_udc_irq();

    if (have_cable_irq) {
        udc_release_cable_irq();
    }
}



/* bi_exit - decommission bus interface driver
 */
static void bi_exit(void)
{
}

#ifdef CONFIG_PM
/* Module Init - Power management ************************************************************ */

/* The power management scheme is simple. Simply do the following:
 *
 *      Event           Call            Equivalent
 *      ------------------------------------------
 *      PM_SUSPEND      bi_exit();      rmmod
 *      PM_RESUME       bi_init();      insmod
 *
 */

static int pm_suspended;

/*
 * usbd_pm_callback
 * @dev:
 * @rqst:
 * @unused:
 *
 * Used to signal power management events.
 */
static int bi_pm_event(struct pm_dev *pm_dev, pm_request_t request, void *unused)
{
    struct usb_device_instance *device;

    dbg_pm(0, "request: %d pm_dev: %p data: %p", request, pm_dev, pm_dev->data);

    if (!(device = pm_dev->data)) {
        dbg_pm(0, "DATA NULL, NO DEVICE");
        return 0;
    }

    switch (request) {
#if defined(CONFIG_IRIS)
    case PM_STANDBY:
    case PM_BLANK:
#endif
    case PM_SUSPEND:
        dbg_pm(0, "PM_SUSPEND");
        if (!pm_suspended) {
            pm_suspended = 1;
            dbg_init(1,"MOD_INC_USE_COUNT %d", GET_USE_COUNT(THIS_MODULE));
            udc_disconnect();                       // disable USB pullup if we can
            udc_disable_interrupts(device);         // disable interupts
            udc_disable();                          // disable UDC
            dbg_pm(0, "PM_SUSPEND: finished");
        }
        break;
#if defined(CONFIG_IRIS)
    case PM_UNBLANK:
#endif
    case PM_RESUME:
        dbg_pm(0, "PM_RESUME");
        if (pm_suspended) {
            // probe for device
            if (udc_init()) {
                dbg_init(0,"udc_init failed");
                //return -EINVAL;
            }
            udc_enable(device);                     // enable UDC
            udc_all_interrupts(device);             // enable interrupts
            udc_connect(device);                          // enable USB pullup if we can

            pm_suspended = 0;
            dbg_init(1,"MOD_INC_USE_COUNT %d", GET_USE_COUNT(THIS_MODULE));
            dbg_pm(0, "PM_RESUME: finished");
        }
        break;
    }
    return 0;
}
#endif


/* bi_modinit - commission bus interface driver
 */
int __init bi_modinit(void)
{
    struct usb_bus_instance * bus;
    struct usb_device_instance * device;
    //extern const char __usbd_module_info[];

    //printk(KERN_INFO "%s (dbg=\"%s\")\n", __usbd_module_info, dbg?dbg:"");

    if (0 != scan_debug_options("usb-bus",dbg_table,dbg)) {
        printk("Failed to scan dbg in bi_modinit\n");
    }

    if (bi_udc_init()) {
        return(-EINVAL);
    }

    // register this bus interface driver and create the device driver instance
    if ((bus = usbd_register_bus(&bi_driver))==NULL) {
        bi_udc_exit();
        return -EINVAL;
    }

    // see if we can scrounge up something to set a sort of unique device
    // address
    if (!udc_serial_init(bus)) {
        dbg_init(1,"serial: %s %04x", bus->serial_number_str,
                 bus->serial_number);
    }

    if ((device = usbd_register_device(NULL, bus, 8))==NULL) {
        usbd_deregister_bus(bus);
        bi_udc_exit();
        return -EINVAL;
    }

#ifdef CONFIG_PM
    // register with power management
    pm_dev = pm_register(PM_USB_DEV, PM_SYS_UNKNOWN, bi_pm_event);
    pm_dev->data = device;
#endif

    bus->device = device;
    device_array[0] = device;

    // initialize the device
    {
        struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + 0;

        // setup endpoint zero

        endpoint->endpoint_address = 0;

        endpoint->tx_attributes = 0;
        endpoint->tx_transferSize = 255;
        endpoint->tx_packetSize = udc_ep0_packetsize();

        endpoint->rcv_attributes = 0;
        endpoint->rcv_transferSize = 0x8;
        endpoint->rcv_packetSize = udc_ep0_packetsize();

        udc_setup_ep(device, 0, endpoint);
    }

    // hopefully device enumeration will finish this process
    udc_startup_events(device);

#ifdef CONFIG_PM
    dbg_pm(0,"pm_dev->callback#%p",pm_dev->callback);
    if (!udc_connected()) {
       /* Fake a call from the PM system to suspend the UDC until it
           is needed (cable connect, etc) */
        (void) bi_pm_event(pm_dev,PM_SUSPEND,NULL);
    /* There appears to be no constant for this, but inspection
           of arch/arm/mach-l7200/apm.c:send_event() shows that the
           suspended state is 3 (i.e. pm_send_all(PM_SUSPEND, (void *)3))
           corresponding to ACPI_D3. */
    pm_dev->state = 3;
    }
#endif
    if (dbgflg_usbdbi_tick > 0) {
        // start ticker
        ticker_kickoff();
    }

    dbgLEAVE(dbgflg_usbdbi_init,1);
    return 0;
}

/* bi_modexit - decommission bus interface driver
 */
void __exit bi_modexit(void)
{
    struct usb_bus_instance *bus;
    struct usb_device_instance *device;
    struct bi_data *data;

    dbgENTER(dbgflg_usbdbi_init,1);

    if ((device = device_array[0])) {

        // XXX moved to usbd_deregister_device()
        //device->status = USBD_CLOSING;

        udc_disconnect();
        udc_disable();

        // XXX XXX
        if (dbgflg_usbdbi_tick > 0) {
            ticker_killoff();
        }

        bus = device->bus;
        data = bus->privdata;

        // XXX
        usbd_device_event(device, DEVICE_RESET, 0);
        usbd_device_event(device, DEVICE_POWER_INTERRUPTION, 0);
        usbd_device_event(device, DEVICE_HUB_RESET, 0);

        dbg_init(1,"DEVICE_DESTROY");
        usbd_device_event(device, DEVICE_DESTROY, 0);

        dbg_init(1,"DISABLE ENDPOINTS");
        bi_disable_endpoints(device);

        dbg_init(1,"UDC_DISABLE");
        //udc_disable();

        dbg_init(1,"BI_UDC_EXIT");
        bi_udc_exit();

        device_array[0] = NULL;
        bus->privdata = NULL;


#ifdef CONFIG_PM
        dbg_init(1,"PM_UNREGISTER(pm_dev#%p)",pm_dev);
        if (pm_dev) {
            pm_unregister(pm_dev);
        }
#endif
        dbg_init(1,"DEREGISTER DEVICE");
        usbd_deregister_device(device);
        bus->device = NULL;

        dbg_init(1,"kfree(data#%p)",data);
        if (data) {
            kfree(data);
        }

        if (bus->serial_number_str) {
            kfree(bus->serial_number_str);
        }

        dbg_init(1,"DEREGISTER BUS");
        usbd_deregister_bus(bus);

    }
    else {
        dbg_init(0,"device is NULL");
    }
    dbg_init(1,"BI_EXIT");
    bi_exit();
    dbgLEAVE(dbgflg_usbdbi_init,1);
}



/* ticker */

/* Clock Tick Debug support ****************************************************************** */


#define RETRYTIME 10

int ticker_terminating;
int ticker_timer_set;

unsigned int udc_interrupts;
unsigned int udc_interrupts_last;

static DECLARE_MUTEX_LOCKED(ticker_sem_start);
static DECLARE_MUTEX_LOCKED(ticker_sem_work);

void ticker_tick(unsigned long data)
{
    ticker_timer_set = 0;
    up(&ticker_sem_work);
}

void udc_ticker_poke(void)
{
    up(&ticker_sem_work);
}

int ticker_thread(void *data)
{
    struct timer_list ticker;

    // detach

    lock_kernel();
    exit_mm(current);
    exit_files(current);
    exit_fs(current);

    // setsid equivalent, used at start of kernel thread, no error checks needed, or at least none made :).
    current->leader = 1;
    current->session = current->pgrp = current->pid;
    current->tty = NULL;
    current->tty_old_pgrp = 0;

    // Name this thread
    sprintf(current->comm, "usbd-bi");

    // setup signal handler
    current->exit_signal = SIGCHLD;
    spin_lock(&current->sigmask_lock);
    flush_signals(current);
    spin_unlock(&current->sigmask_lock);

    // XXX Run at a high priority, ahead of sync and friends
//    current->nice = -20;
    current->policy = SCHED_OTHER;

    unlock_kernel();

    // setup timer
    init_timer(&ticker);
    ticker.data = 0;
    ticker.function = ticker_tick;

    // let startup continue
    up(&ticker_sem_start);

    // process loop
    for (ticker_timer_set = ticker_terminating = 0; !ticker_terminating;) {

        char buf[100];
        char *cp;

        if (!ticker_timer_set) {
            mod_timer(&ticker, jiffies + HZ*RETRYTIME);
        }

        // wait for someone to tell us to do something
        down(&ticker_sem_work);

        if (udc_interrupts != udc_interrupts_last) {
            dbg_tick(1,"--------------");
        }

        // do some work
        memset(buf, 0, sizeof(buf));
        cp = buf;

        if (dbgflg_usbdbi_tick) {
            unsigned long flags;
            local_irq_save(flags);
            dbg_tick(1,"[%d]", udc_interrupts);
            udc_regs();
            local_irq_restore(flags);
        }
    }

    // remove timer
    del_timer(&ticker);

    // let the process stopping us know we are done and return
    up(&ticker_sem_start);
    return 0;
}

/**
 * kickoff_thread - start management thread
 */
void ticker_kickoff(void)
{
    ticker_terminating = 0;
    kernel_thread(&ticker_thread, NULL, 0);
    down(&ticker_sem_start);
}

/**
 * killoff_thread - stop management thread
 */
void ticker_killoff(void)
{
    if (!ticker_terminating) {
        ticker_terminating = 1;
        up(&ticker_sem_work);
        down(&ticker_sem_start);
    }
}


/* module */

/*
module_init(bi_modinit);
module_exit(bi_modexit);
*/

