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
#include "de1_drv.h"
#include "../cy7c67200_300_otg.h"

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
#define DRIVER_VERSION "v0.3"
#define DRIVER_AUTHOR "usbapps@cypress.com"
#define DRIVER_DESC "CY7C67200/300 OTG Design Example Driver"
#define DE1_DEVICE_NAME "DE1"

/* Module paramaters */
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");


/* Define these values to match your device */
#define USB_OTG_DE1_VENDOR_ID	0x04b4
#define USB_OTG_DE1_PRODUCT_ID	0xDE01
#define DEVICE_CONNECTED		1
#define DEVICE_NOTCONNECTED		0
#define DE1_BUFFER_SIZE			64
#define DE1_DEVICE_MAJOR		253
#define A_DEV			0
#define B_DEV 			1

/* External prototypes */

/* table of devices that work with this driver */
static struct usb_device_id otg_de1_table [] = {
	{ USB_DEVICE(USB_OTG_DE1_VENDOR_ID, USB_OTG_DE1_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, otg_de1_table);

/* Get a minor range for your devices from the usb maintainer */
#define USB_OTG_DE1_MINOR_BASE	200	

/* we can have up to this number of device plugged in at once */
#define MAX_DEVICES		  	1

#define INTERRUPT_REQ_SIZE		4	/* data request size for interrupt endpoint */
#define INTERRUPT_EP_INTERVAL 	100 /* interval for interrupt endpoint in millisecond */
#define DE_REPORT_INVALID	   	0
#define DE_REPORT_VALID			1


/* Structure to hold all of our device specific stuff */
struct USB_OTG_DE1 {
	struct usb_device *	udev;			/* save off the usb device pointer */
	struct usb_interface *	interface;	/* the interface for this device */
	unsigned char		major;			/* the starting minor number for this device */
	unsigned char		connect_state;  /* connect state of the device */

	unsigned char * 	int_in_buffer;	/* the buffer to receive interrupt data */
	int					int_in_size;	/* the size of the receive interrupt buffer */
	struct urb *		int_in_urb;  	/* the urb used to receive interrupt data */
	__u8				int_in_endpointAddr;	/* the address of the interrupt in endpoint */
	__u8				de_rpt_valid;
	unsigned char *		de_rpt_buffer;

	unsigned char * 	int_out_buffer;	/* the buffer to receive interrupt data */
	int					int_out_size;	/* the size of the receive interrupt buffer */
	__u8				int_out_endpointAddr;	/* the address of the interrupt in endpoint */

	unsigned char * 	bulk_in_buffer;	/* the buffer to receive data */
	int					bulk_in_size;	/* the size of the receive buffer */
	__u8				bulk_in_endpointAddr;	/* the address of the bulk in endpoint */

	unsigned char * 	bulk_out_buffer;	/* the buffer to send data */
	int					bulk_out_size;		/* the size of the send buffer */
	struct urb *		bulk_out_urb;  		/* the urb used to send bulk out data */
	__u8				bulk_out_endpointAddr;	/* the address of the bulk in endpoint */

	struct tq_struct	tqueue;			/* task queue for line discipline waking up */
	int					open_count;	 	/* number of times this port has been opened */
	struct semaphore	sem;			/* locks this structure */
};


/* local function prototypes */
static ssize_t 	otg_de1_read	(struct file *file, char *buffer, size_t count, loff_t *ppos);
static ssize_t 	otg_de1_write	(struct file *file, const char *buffer, size_t count, loff_t *ppos);
static int 		otg_de1_ioctl  	(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int 		otg_de1_open   	(struct inode *inode, struct file *file);
static int 		otg_de1_release	(struct inode *inode, struct file *file);
	
static void * 	otg_de1_probe	(struct usb_device *dev, unsigned int ifnum, const struct usb_device_id *id);
static void 	otg_de1_disconnect	(struct usb_device *dev, void *ptr);

static void 	otg_de1_write_bulk_callback	(struct urb *urb);
static void 	otg_de1_int_in_callback (struct urb *urb);

/* DE1 structure */
static struct USB_OTG_DE1 		*dev = NULL;

/*
 * File operations needed when we register this driver.
 */
static struct file_operations otg_de1_fops = {
	/*
	 * The owner field is part of the module-locking
	 * mechanism. The idea is that the kernel knows
	 * which module to increment the use-counter of
	 * BEFORE it calls the device's open() function.
	 * This also means that the kernel can decrement
	 * the use-counter again before calling release()
	 * or should the open() function fail.
	 *
	 * Not all device structures have an "owner" field
	 * yet. "struct file_operations" and "struct net_device"
	 * do, while "struct tty_driver" does not. If the struct
	 * has an "owner" field, then initialize it to the value
	 * THIS_MODULE and the kernel will handle all module
	 * locking for you automatically. Otherwise, you must
	 * increment the use-counter in the open() function
	 * and decrement it again in the release() function
	 * yourself.
	 */
	owner:		THIS_MODULE,

	read:		otg_de1_read,
	write:		otg_de1_write,
	ioctl:		otg_de1_ioctl,
	open:		otg_de1_open,
	release:	otg_de1_release,
};      


/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver otg_de1_driver = {
	name:		"otg_de1",
	probe:		otg_de1_probe,
	disconnect:	otg_de1_disconnect,
	fops:		&otg_de1_fops,
	minor:		USB_OTG_DE1_MINOR_BASE,
	id_table:	otg_de1_table,
};


/**
 *	USB_OTG_DE1_debug_data
 */
static inline void USB_OTG_DE1_debug_data (const char *function, int size, const unsigned char *data)
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
 *	otg_de1_delete
 */
static inline void otg_de1_delete (struct USB_OTG_DE1 *dev)
{
	/* Release buffers and URB(s) */

	if (dev != NULL) {
		/* Free bulk in buffer */
		if (dev->bulk_in_buffer != NULL)
			kfree (dev->bulk_in_buffer);

		/* Free bulk out buffer and urb */
		if (dev->bulk_out_buffer != NULL)
			kfree (dev->bulk_out_buffer);
		if (dev->bulk_out_urb != NULL)
			usb_free_urb(dev->bulk_out_urb);

		/* Free interrupt buffer and urb */
		if (dev->int_in_buffer != NULL)
			kfree (dev->int_in_buffer);
		if (dev->int_in_urb != NULL)
			usb_free_urb(dev->int_in_urb);

		/* Free interrupt out buffer */
		if (dev->int_out_buffer != NULL)
			kfree (dev->int_out_buffer);

		/* Free report buffer */
		if (dev->de_rpt_buffer != NULL)
			kfree (dev->de_rpt_buffer);

		/* Free device structure */
		kfree (dev);
	}	
}


/**
 *	otg_de1_open
 */
static int otg_de1_open (struct inode *inode, struct file *file)
{
	int retval = 0;
	
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
 *	otg_de1_release
 */
static int otg_de1_release (struct inode *inode, struct file *file)
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
	if (dev->open_count <= 0) {
		/* shutdown any bulk writes that might be going on */
		usb_unlink_urb (dev->bulk_out_urb);

		dev->open_count = 0;
	}

	/* decrement our usage count for the module */
	MOD_DEC_USE_COUNT;

exit_not_opened:
	up (&dev->sem);

	return retval;
}


/**
 *	otg_de1_read
 */
static ssize_t otg_de1_read (struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	int retval = 0;

	if (dev == NULL) {
		dbg ("object is NULL");
		return -ENODEV;
	}

	/* lock this object */
	down (&dev->sem);

	/* verify that the device wasn't unplugged */
	if ((dev->udev == NULL) || (dev->connect_state != DEVICE_CONNECTED)){
		up (&dev->sem);
		return -ENODEV;
	}
	
	if (count > 0) {
		/* Do an immediate bulk read to get data from the device */
		retval = usb_bulk_msg(dev->udev,
							usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
							dev->bulk_in_buffer, dev->bulk_in_size,
							&count, HZ*10);

		/* If the read was successful, copy the data to userspace */
		if (!retval) {
			if (copy_to_user (buffer, dev->bulk_in_buffer, count))
				retval = -EFAULT;
			else
				retval = count;
		}
	} else {
  	    dbg("read negative or zero bytes");
	}

	/* unlock the device */
	up (&dev->sem);
	return retval;
}


/**
 *	otg_de1_write
 */
static ssize_t otg_de1_write (struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	ssize_t bytes_written = 0;
	int retval = 0;

	if (dev == NULL) {
		dbg ("object is NULL");
		return -ENODEV;
	}

	/* lock this object */
	down (&dev->sem);

	/* verify that the device wasn't unplugged */
	if ((dev->udev == NULL) || (dev->connect_state != DEVICE_CONNECTED)) {
		retval = -ENODEV;
		goto exit;
	}

	/* verify that we actually have some data to write */
	if (count == 0) {
		dbg("write request of 0 bytes");
		goto exit;
	}

	/* see if we are already in the middle of a write */
	if (dev->bulk_out_urb->status == -EINPROGRESS) {
		dbg ("already writing");
		goto exit;
	}
   
	/* we can only write as much as 1 urb will hold */
	bytes_written = (ssize_t) min(count, (size_t) dev->bulk_out_size);

	/* copy the data from userspace into our urb */
	if (copy_from_user(dev->bulk_out_buffer, buffer, 
			   		   bytes_written)) {
		retval = -EFAULT;
		goto exit;
	}

	/* set up our urb */
	FILL_BULK_URB(dev->bulk_out_urb, dev->udev, 
		      usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
		      dev->bulk_out_buffer, bytes_written,
		      otg_de1_write_bulk_callback, dev);

	/* send the data out the bulk port */
	retval = usb_submit_urb(dev->bulk_out_urb);
	if (retval) {
		err("failed submitting bulk out urb, error %d",
		    retval);
	} else {
		retval = bytes_written;
	}

exit:
	/* unlock the device */
	up (&dev->sem);

	return retval;
}


/**
 *	otg_de1_ioctl
 */
static int otg_de1_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int count = 0;
	int ret = 0;
	int val;
	unsigned char size;

	if (dev == NULL) {
		dbg ("object is NULL");
		return -ENODEV;
	}

	/* lock this object */
	down (&dev->sem);

    switch (cmd) {

	case IOCTL_GET_DE_RPT:
		if (dev->connect_state == DEVICE_CONNECTED) {
			if (dev->de_rpt_valid == DE_REPORT_VALID) {
                copy_from_user((void *) &size, (void *) arg, 1);

				if (copy_to_user ((void *) arg,
				                  dev->de_rpt_buffer,
				                  size)) {
					ret = -EFAULT;
				}
				
				/* Clear report valid status */
				dev->de_rpt_valid = DE_REPORT_INVALID;
			} else {
				ret = -EAGAIN;
			}
		} else {
			ret = -ENOTCONN;
		}
		break;

	case IOCTL_SEND_DE_RPT:
		if (dev->connect_state == DEVICE_CONNECTED) {
			copy_from_user((void *) &size, (void *) arg, 1);

			if (copy_from_user (dev->int_out_buffer,
							  (void *) arg,
							  size)) {
  		           	printk(KERN_DEBUG"IOCTL_SEND_DE_RPT efault\n");
					ret = -EFAULT;
			} else {
				/* Do an immediate bulk write */
				ret = usb_bulk_msg(dev->udev,
				     			   usb_sndbulkpipe(dev->udev, dev->int_out_endpointAddr),
								   dev->int_out_buffer, size,
								   &count, HZ);
			}
		} else {
			ret = -ENOTCONN;
		}
		break;

    case IOCTL_END_SESSION:
	    otg_end_session();
		break;

	case IOCTL_ENABLE_HNP:
		if (dev->connect_state == DEVICE_CONNECTED) {

			/* Check for direct connect (no intervening hubs) */
			if ((dev->udev->parent == 0) || (dev->udev->parent->parent == 0)) {

			    /* set b_hnp_enable */			
			    ret = usb_control_msg(dev->udev,
							          usb_sndctrlpipe(dev->udev, 0), 
								      USB_REQ_SET_FEATURE,
								      USB_TYPE_STANDARD,
								      3, 0, NULL, 0, HZ);
                if (ret) {
                    otg_set_device_not_respond();
                }
                else
                {
    			    otg_offer_hnp();
                }
            }
		}
		break;

	case IOCTL_END_HNP:
        otg_end_hnp();
		break;

    case IOCTL_OTG_STATE:
	    ret = otg_state(&val);
        if( ret || copy_to_user( (void *)arg, &val, sizeof(int) ) )
        {
            ret = -EFAULT;
        }
        break;

    case IOCTL_REQUEST_SRP:
        otg_start_session();
        break;

    case IOCTL_OTG_DEBUG:
        otg_print_state();
        break;

	case IOCTL_UNSUPPORTED_DEVICE:
		val = otg_get_unsupport_device();

		if (copy_to_user ((void *) arg, &val, sizeof(int))) {
			ret = -EFAULT;
		}
		break;

	case IOCTL_DEVICE_NOT_RESPOND:
		val = otg_get_device_not_respond();

		if (copy_to_user ((void *) arg, &val, sizeof(int))) {
			ret = -EFAULT;
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
 *	otg_de1_write_bulk_callback
 */
static void otg_de1_write_bulk_callback (struct urb *urb)
{
	if (urb->status != 0) {  
		dbg("nonzero write bulk status received: %d",
		    urb->status);
		return;
	}

	return;
}

/**
 *	otg_de1_int_in_callback
 */
static void otg_de1_int_in_callback (struct urb *urb)
{
	if (urb->status) return;

	if (dev->de_rpt_valid != DE_REPORT_VALID) {
		/* Indicate a valid report */
		dev->de_rpt_valid = DE_REPORT_VALID;

		/* copy data */
		memcpy(dev->de_rpt_buffer,
			   dev->int_in_buffer, 
			   (urb->actual_length > DE1_BUFFER_SIZE ? DE1_BUFFER_SIZE : urb->actual_length));
	}
}



/**
 *	otg_de1_probe
 *
 *	Called by the usb core when a new device is connected that it thinks
 *	this driver might be interested in.
 */
static void * otg_de1_probe(struct usb_device *udev, unsigned int ifnum, const struct usb_device_id *id)
{
	struct usb_interface *interface;
	struct usb_interface_descriptor *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int pipe, maxp, size;
	int i;
	int ret;

	if (dev == NULL) {
		dbg ("object is NULL");
		return NULL;
	}

	/* See if the device offered us matches what we can accept */
	if ((udev->descriptor.idVendor != USB_OTG_DE1_VENDOR_ID) ||
	    (udev->descriptor.idProduct != USB_OTG_DE1_PRODUCT_ID)) {
		return NULL;
	}

	/* See if we are already connected to a de1 device */
	if (dev->connect_state == DEVICE_CONNECTED) {
		//info ("Too many devices plugged in, can not handle this device.");
		return NULL;
	}

	interface = &udev->actconfig->interface[ifnum];

	dev->udev = udev;
	dev->interface = interface;

	/* set up the endpoint information */
	/* check out the endpoints */
	iface_desc = &interface->altsetting[0];
	for (i = 0; i < iface_desc->bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i];

		if ((endpoint->bEndpointAddress & 0x80) &&
		    ((endpoint->bmAttributes & 3) == 0x02)) {
			/* we found a bulk in endpoint */
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
		}
		
		if (((endpoint->bEndpointAddress & 0x80) == 0x00) &&
		    ((endpoint->bmAttributes & 3) == 0x02)) {
			/* we found a bulk out endpoint */
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}
		
		if ((endpoint->bEndpointAddress & 0x80) &&
		    ((endpoint->bmAttributes & 3) == 0x03)) {
			/* we found an interrupt in endpoint */
			dev->int_in_endpointAddr = endpoint->bEndpointAddress;

			/* Submit an interrupt in urb */
			pipe = usb_rcvintpipe(dev->udev, endpoint->bEndpointAddress);
			maxp = usb_maxpacket(dev->udev, pipe, usb_pipeout(pipe));
			size = maxp > DE1_BUFFER_SIZE ? DE1_BUFFER_SIZE : maxp;

			FILL_INT_URB(dev->int_in_urb, dev->udev, pipe, dev->int_in_buffer,
			             size, otg_de1_int_in_callback, dev, endpoint->bInterval);

			if ((ret = usb_submit_urb(dev->int_in_urb)) != 0) {
				err("failed submitting int in urb, error %d", ret);
				return NULL;
			}
		}

		if (((endpoint->bEndpointAddress & 0x80) == 0x00) &&
		    ((endpoint->bmAttributes & 3) == 0x03)) {
			/* we found an interrupt out endpoint */
			dev->int_out_endpointAddr = endpoint->bEndpointAddress;
		}
	}

	/* Indicate that a device is connected */
	dev->connect_state = DEVICE_CONNECTED;

	/* let the user know what node this device is now attached to */
	//info ("Cypress OTG device now attached (major %d - minor %d)", dev->major, USB_OTG_DE1_MINOR_BASE);

	return dev;
}


/**
 *	otg_de1_disconnect
 *
 *	Called by the usb core when the device is removed from the system.
 */
static void otg_de1_disconnect(struct usb_device *udev, void *ptr)
{
	if (dev == NULL) {
		dbg ("object is NULL");
		return;
	}

	/* lock the device */
	down (&dev->sem);

	/* unlink the Int in URB */
	usb_unlink_urb(dev->int_in_urb);

	/* Indicate that a device is not connected */
	dev->connect_state = DEVICE_NOTCONNECTED;

	/* Indicate that any remaining report is not valid */
	dev->de_rpt_valid = DE_REPORT_INVALID;

	/* unlock the device */
	up (&dev->sem);

	//info("Cypress OTG device DE1 now disconnected");
}



/**
 *	USB_OTG_DE1_init
 */
static int __init USB_OTG_DE1_init(void)
{
	int result;

	/* allocate memory for our device state and intialize it */
	dev = kmalloc (sizeof(struct USB_OTG_DE1), GFP_KERNEL);
	if (dev == NULL) {
		err ("Out of memory");
		return -1;
	}
	memset ((void *) dev, 0x00, sizeof (struct USB_OTG_DE1));

	/* allocate memory for the bulk-in buffer */
	dev->bulk_in_buffer = kmalloc (DE1_BUFFER_SIZE, GFP_KERNEL);
	if (!dev->bulk_in_buffer) {
		err("Couldn't allocate bulk_in_buffer");
		return -1;
	}
	dev->bulk_in_size = DE1_BUFFER_SIZE;

	/* allocate memory for the bulk-out URB */
	dev->bulk_out_urb = usb_alloc_urb(0);
	if (!dev->bulk_out_urb) {
		err("No free urbs available");
		return -1;
	}

	/* allocate memory for the bulk-out buffer */
	dev->bulk_out_buffer = kmalloc (DE1_BUFFER_SIZE, GFP_KERNEL);			
	if (!dev->bulk_out_buffer) {
		err("Couldn't allocate bulk_out_buffer");
		return -1;
	}
	dev->bulk_out_size = DE1_BUFFER_SIZE;

	/* allocate memory for the int-in URB */
	dev->int_in_urb = usb_alloc_urb(0);
	if (!dev->int_in_urb) {
		err("No free urbs available");
		return -1;
	}

	/* allocate memory for the int_in buffer */
	dev->int_in_buffer = kmalloc (DE1_BUFFER_SIZE, GFP_KERNEL);			
	if (!dev->int_in_buffer) {
		err("Couldn't allocate int_in_buffer");
		return -1;
	}
	dev->int_in_size = DE1_BUFFER_SIZE;

	/* allocate memory for the int_out buffer */
	dev->int_out_buffer = kmalloc (DE1_BUFFER_SIZE, GFP_KERNEL);			
	if (!dev->int_out_buffer) {
		err("Couldn't allocate int_out_buffer");
		return -1;
	}
	dev->int_out_size = DE1_BUFFER_SIZE;

	/* allocate memory for the int_out buffer */
	dev->de_rpt_buffer = kmalloc (DE1_BUFFER_SIZE, GFP_KERNEL);			
	if (!dev->de_rpt_buffer) {
		err("Couldn't allocate de_rpt_buffer");
		return -1;
	}
	dev->de_rpt_valid = DE_REPORT_INVALID;


	/* register this driver with the USB subsystem */
	result = usb_register(&otg_de1_driver);
	if (result < 0) {
		err("usb_register failed for the "__FILE__" driver. Error number %d",
		    result);
		return -1;
	}

	dev->major = DE1_DEVICE_MAJOR;
    result = register_chrdev( dev->major, DE1_DEVICE_NAME, &otg_de1_fops );
    if( result != 0 )
    {
        err("register_chrdev() failed.");
		return -1;
    }

	/* Indicate that a device is not connected */
	dev->connect_state = DEVICE_NOTCONNECTED;

	/* Initialize the driver semaphore */
	init_MUTEX (&dev->sem);

	//info(DRIVER_DESC " " DRIVER_VERSION " (Major = %d)", dev->major);
	return 0;
}


/**
 *	USB_OTG_DE1_exit
 */
static void __exit USB_OTG_DE1_exit(void)
{

	if (dev == NULL) {
		dbg ("object is NULL");
		return;
	}

	/* deregister this driver with the USB subsystem */
	usb_deregister(&otg_de1_driver);

	/* Unregister device */
    unregister_chrdev( dev->major, DE1_DEVICE_NAME );

	/* Delete buffers */
	otg_de1_delete (dev);
}


module_init (USB_OTG_DE1_init);
module_exit (USB_OTG_DE1_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

