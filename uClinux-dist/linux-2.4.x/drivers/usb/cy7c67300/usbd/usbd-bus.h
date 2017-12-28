/*
 * linux/drivers/usbd/usb-bus.c - basic bus interface support
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
 * USB Bus Interface Driver structures
 *
 * Driver description:
 * 
 *  struct usb_bus_operations
 *  struct usb_bus_driver
 *
 */


struct usb_device_description;

/* Operations that the slave layer or function driver can use to interact
 * with the bus interface driver. 
 *
 * The send_urb() function is used by the usb-device endpoint 0 driver and
 * function drivers to submit data to be sent.
 *
 * The cancel_urb() function is used by the usb-device endpoint 0 driver and
 * function drivers to remove previously queued data to be sent.
 *
 * The endpoint_halted() function is used by the ep0 control function to
 * check if an endpoint is halted.
 *
 * The device_feature() function is used by the ep0 control function to
 * set/reset device features on an endpoint.
 *
 * The device_event() function is used by the usb device core to tell the
 * bus interface driver about various events.
 */
struct usb_bus_operations {
    int (* send_urb) (struct usbd_urb *);
    int (* cancel_urb) (struct usbd_urb *);
    int (* endpoint_halted)(struct usb_device_instance *, int);
    int (* device_feature)(struct usb_device_instance *, int, int);
    int (* device_event)(struct usb_device_instance *, usb_device_event_t, int );
};

/* Bus Interface data structure
 *
 * Keep track of specific bus interface. 
 *
 * This is passed to the usb-device layer when registering. It contains all
 * required information about each real bus interface found such that the
 * usb-device layer can create and maintain a usb-device structure.
 *
 * Note that bus interface registration is incumbent on finding specific
 * actual real bus interfaces. There will be a registration for each such
 * device found.
 *
 * The max_tx_endpoints and max_rx_endpoints are the maximum number of
 * possible endpoints that this bus interface can support. The default
 * endpoint 0 is not included in these counts.
 *
 */
struct usb_bus_driver {
    char               *name;
    unsigned char       max_endpoints;     /* maximimum number of rx enpoints */
    unsigned char       maxpacketsize;
    struct usb_device_description *device_description;
    struct usb_bus_operations *ops;
    struct module	*this_module;	   /* manage inc use counts to prevent 
					    * unload races */
};

