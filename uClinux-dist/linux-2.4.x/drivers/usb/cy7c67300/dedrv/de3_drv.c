/*******************************************************************************
 *
 * Copyright (c) 2003 Cypress Semiconductor
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
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/smp_lock.h>
//#include <linux/devfs_fs_kernel.h>
#include <linux/usb.h>
#include "de3_drv.h"

#ifdef CONFIG_USB_DEBUG
	static int debug = 1;
#else
	static int debug;
#endif

/* Use our own dbg macro */
#undef dbg
#define dbg(format, arg...) \
   do { if (debug) printk(KERN_DEBUG __FILE__ ":"  "%d: " format "\n" ,__LINE__, ## arg); } while (0)

/* Version Information */
#define DRIVER_VERSION "v0.1"
#define DRIVER_AUTHOR "usbapps@cypress.com"
#define DRIVER_DESC "CY7C67200/300 Design Example 3 Keyboard Driver"
#define DE3_DEVICE_NAME "DE3"

/* Module paramaters */
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");


/* Define these values to match your device */
#define DEVICE_CONNECTED		1
#define DEVICE_NOTCONNECTED		0
#define DE3_BUFFER_SIZE			8
#define DE3_DEVICE_MAJOR		252

/* table of devices that work with this driver */

static struct usb_device_id de3_kbd_id_table [] = {
	{ USB_INTERFACE_INFO(3, 1, 1) },
	{ }						/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, de3_kbd_id_table);

/* Structure to hold all of our device specific stuff */
struct USB_DE3_KBD {
	struct usb_device *	udev;			/* save off the usb device pointer */
	struct usb_interface *	interface;	/* the interface for this device */
	unsigned char		major;			/* the starting minor number for this device */
	unsigned char		connect_state;  /* connect state of the device */

	unsigned char * 	int_in_buffer;	/* the buffer to receive interrupt data */
	int					int_in_size;	/* the size of the receive interrupt buffer */
	int					int_in_pipe;	/* the address of the interrupt in endpoint */
	struct urb			int_in_urb;		/* interrupt in urb */
	int					pending_request;/* pending interrupt in request */

	hid_keyboard_report_t hid_rpt;		/* hid keyboard report buffer */

	struct tq_struct	tqueue;			/* task queue for line discipline waking up */
	int					open_count;	 	/* number of times this port has been opened */
	struct semaphore	sem;			/* locks this structure */
};


/* local function prototypes */
static int 		de3_kbd_ioctl  	(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int 		de3_kbd_open   	(struct inode *inode, struct file *file);
static int 		de3_kbd_release	(struct inode *inode, struct file *file);
	
static void * 	de3_kbd_probe	(struct usb_device *dev, unsigned int ifnum, const struct usb_device_id *id);
static void 	de3_kbd_disconnect	(struct usb_device *dev, void *ptr);

static void 	de3_kbd_int_callback (struct urb *urb);

/* DE1 structure */
static struct USB_DE3_KBD 		*dev = NULL;

/*
 * File operations needed when we register this driver.
 */
static struct file_operations de3_kbd_fops = {
	owner:		THIS_MODULE,
	ioctl:		de3_kbd_ioctl,
	open:		de3_kbd_open,
	release:	de3_kbd_release,
};      


/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver de3_kbd_driver = {
	name:		"de3_kbd",
	probe:		de3_kbd_probe,
	disconnect:	de3_kbd_disconnect,
	id_table:	de3_kbd_id_table,
};


/**
 *	USB_DE3_KBD_debug_data
 */
static inline void de3_kbd_debug_data (const char *function, int size, const unsigned char *data)
{
	int i;

	if (!debug)
		return;
	
	printk (KERN_DEBUG __FILE__": %s - length = %d, data = ", 
		function, size);
	for (i = 0; i < size; ++i) {
		printk ("%.2x ", data[i]);
	}
	printk ("\n");
}


/**
 *	de3_kbd_delete
 */
static inline void de3_kbd_delete (struct USB_DE3_KBD *dev)
{
	/* Release buffers and URB(s) */

	if (dev != NULL) {
		/* Free interrupt buffer and urb */
		if (dev->int_in_buffer != NULL)
			kfree (dev->int_in_buffer);

		/* Free device structure */
		kfree (dev);
	}	
}


/**
 *	de3_kbd_open
 */
static int de3_kbd_open (struct inode *inode, struct file *file)
{
	int retval = 0;
	
	//info ("DE3 keyboard device open");

	if (dev == NULL) {
		dbg("object is NULL");
		return -ENODEV;
	}

	if (dev->open_count > 0) {
		err("Device already open!");
		return -ENODEV;
	}

	/* Increment our usage count for the module.
	 * This is redundant here, because "struct file_operations"
	 * has an "owner" field. This line is included here soley as
	 * a reference for drivers using lesser structures... ;-)
	 */
	MOD_INC_USE_COUNT;

	/* lock this device */
	down (&dev->sem);

	/* increment our usage count for the driver */
	++dev->open_count;

	/* save our object in the file's private structure */
	file->private_data = dev;

	/* unlock this device */
	up (&dev->sem);

	return retval;
}


/**
 *	de3_kbd_release
 */
static int de3_kbd_release (struct inode *inode, struct file *file)
{
	int retval = 0;

	if (dev == NULL) {
		dbg ("object is NULL");
		return -ENODEV;
	}

	/* lock our device */
	down (&dev->sem);

	if (dev->open_count <= 0) {
		dbg ("device not opened");
		retval = -ENODEV;
		goto exit_not_opened;
	}

	/* decrement our usage count for the device */
	--dev->open_count;

	/* decrement our usage count for the module */
	MOD_DEC_USE_COUNT;

exit_not_opened:
	up (&dev->sem);

	return retval;
}


/**
 *	de3_kbd_int_callback
 */
static void de3_kbd_int_callback (struct urb *urb)
{
	if (urb->status) return;

	if (dev->hid_rpt.valid != HID_REPORT_VALID) {
		/* Indicate a valid report */
		dev->hid_rpt.valid = HID_REPORT_VALID;

		/* copy data */
		memcpy(&dev->hid_rpt.report, dev->int_in_buffer, HID_REPORT_SIZE);
	}
}


/**
 *	de3_kbd_ioctl
 */
static int de3_kbd_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	hid_keyboard_report_t * hidreport;

	if (dev == NULL) {
		dbg ("object is NULL");
		return -ENODEV;
	}

	/* lock this object */
	down (&dev->sem);

    switch (cmd) {

   	case IOCTL_GET_HID_RPT:
	    /* Get the keyboard report from the interrupt pipe */
		if (dev->connect_state == DEVICE_CONNECTED) {
			hidreport = (hid_keyboard_report_t *) arg;

			/* Initialize report valid status */
			hidreport->valid = HID_REPORT_INVALID;

			if (dev->hid_rpt.valid == HID_REPORT_VALID) {
				if (copy_to_user ((void *) hidreport->report,
				                  dev->hid_rpt.report, HID_REPORT_SIZE)) {
					ret = -EFAULT;
				} else {
					/* indicate valid report */
					hidreport->valid = HID_REPORT_VALID;
				}

				/* Clear report valid status */
				dev->hid_rpt.valid = HID_REPORT_INVALID;
			}

			/* Check for pending interrupt in request */
			if (dev->pending_request == 0) {
				/* Submit the interrupt URB */
				dev->pending_request = 1;
				dev->int_in_urb.dev = dev->udev;
				if (usb_submit_urb(&dev->int_in_urb))
					ret = -EIO;
			}
		} else {
			ret = -ENOTCONN;
		}
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}
   	
	/* unlock the device */
	up (&dev->sem);
	
	return ret;
}

/**
 *	de3_kbd_probe
 *
 *	Called by the usb core when a new device is connected that it thinks
 *	this driver might be interested in.
 */
static void * de3_kbd_probe(struct usb_device *udev, unsigned int ifnum, const struct usb_device_id *id)
{
	struct usb_interface *interface;
	struct usb_interface_descriptor *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int pipe, maxp;

	if (dev == NULL) {
		dbg ("object is NULL");
		return NULL;
	}

	interface = &udev->actconfig->interface[ifnum];
	iface_desc = &interface->altsetting[interface->act_altsetting];

	if (iface_desc->bNumEndpoints != 1) return NULL;

	endpoint = iface_desc->endpoint + 0;
	if (!(endpoint->bEndpointAddress & 0x80)) return NULL;
	if ((endpoint->bmAttributes & 3) != 3) return NULL;

	pipe = usb_rcvintpipe(udev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(udev, pipe, usb_pipeout(pipe));

	usb_set_protocol(udev, iface_desc->bInterfaceNumber, 0);
	usb_set_idle(udev, iface_desc->bInterfaceNumber, 0, 0);

	/* See if we are already connected to a de1 device */
	if (dev->connect_state == DEVICE_CONNECTED) {
		info ("Too many devices plugged in, can not handle this device.");
		return NULL;
	}

	/* Save info */
	dev->udev = udev;
	dev->interface = interface;
	dev->int_in_pipe = pipe;

	FILL_INT_URB(&dev->int_in_urb, dev->udev, pipe, dev->int_in_buffer,
	             maxp > DE3_BUFFER_SIZE ? DE3_BUFFER_SIZE : maxp,
				 de3_kbd_int_callback, dev, endpoint->bInterval);

	/* Indicate that a device is connected */
	dev->connect_state = DEVICE_CONNECTED;

	/* let the user know what node this device is now attached to */
	info ("DE3 keyboard device now attached (major %d)", dev->major);

	return dev;
}


/**
 *	de3_kbd_disconnect
 *
 *	Called by the usb core when the device is removed from the system.
 */
static void de3_kbd_disconnect(struct usb_device *udev, void *ptr)
{
	if (dev == NULL) {
		dbg ("object is NULL");
		return;
	}

	/* lock the device */
	down (&dev->sem);

	/* unlink the Int in URB */
	usb_unlink_urb(&dev->int_in_urb);

	/* Indicate that a device is not connected */
	dev->connect_state = DEVICE_NOTCONNECTED;

	/* Indicate no pending interrupt in request */
	dev->pending_request = 0;

	/* unlock the device */
	up (&dev->sem);

	info("DE3 keyboard device now disconnected");
}



/**
 *	USB_DE3_KBD_init
 */
static int __init de3_kbd_init(void)
{
	int result;

	/* allocate memory for our device state and intialize it */
	dev = kmalloc (sizeof(struct USB_DE3_KBD), GFP_KERNEL);
	if (dev == NULL) {
		err ("Out of memory");
		return -1;
	}
	memset ((void *) dev, 0x00, sizeof (struct USB_DE3_KBD));

	/* allocate memory for the int_in buffer */
	dev->int_in_buffer = kmalloc (DE3_BUFFER_SIZE, GFP_KERNEL);
	if (!dev->int_in_buffer) {
		err("Couldn't allocate int_in_buffer");
		return -1;
	}
	dev->int_in_size = DE3_BUFFER_SIZE;

	/* Indicate invalid hid report */
	dev->hid_rpt.valid = HID_REPORT_INVALID;

	/* register this driver with the USB subsystem */
	result = usb_register(&de3_kbd_driver);
	if (result < 0) {
		err("usb_register failed for the "__FILE__" driver. Error number %d",
		    result);
		return -1;
	}

	/* register this driver */
#if 0
    dev->major = register_chrdev( 0, DE3_DEVICE_NAME, &de3_kbd_fops );
    if( dev->major == 0 )
    {
        err("register_chrdev() failed.");
		return -1;
    }
#else
    dev->major = DE3_DEVICE_MAJOR; 
    result = register_chrdev( dev->major, DE3_DEVICE_NAME, &de3_kbd_fops );
    if( result != 0 )
    {
        err("register_chrdev() failed.");
		return -1;
    }
#endif

	/* Indicate that a device is not connected */
	dev->connect_state = DEVICE_NOTCONNECTED;

	/* Indicate no pending interrupt in request */
	dev->pending_request = 0;

	/* Initialize the driver semaphore */
	init_MUTEX (&dev->sem);

	info(DRIVER_DESC " " DRIVER_VERSION " (Major = %d)", dev->major);

	return 0;
}


/**
 *	USB_DE3_KBD_exit
 */
static void __exit de3_kbd_exit(void)
{

	if (dev == NULL) {
		dbg ("object is NULL");
		return;
	}

	/* deregister this driver with the USB subsystem */
	usb_deregister(&de3_kbd_driver);

	/* Unregister device */
    unregister_chrdev( dev->major, DE3_DEVICE_NAME );

	/* Delete buffers */
	de3_kbd_delete (dev);
}


module_init (de3_kbd_init);
module_exit (de3_kbd_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

