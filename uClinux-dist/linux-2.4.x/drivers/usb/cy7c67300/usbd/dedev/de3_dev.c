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

/*******************************************************************
 *
 *    DESCRIPTION: Peripheral Funciton Driver for EZ-Host Design 
 *                 Example Four of the development kit.
 *
 *******************************************************************/


/** include files **/
//#define  MODULE
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/compatmac.h>
#include <linux/slab.h>
#include <asm/semaphore.h>
#include <asm/ioctls.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-debug.h"
#include "../usbd-inline.h"
#include "../usbd-arch.h"
#include "../usbd-export.h"
#include "../usbd-build.h"
#include "../../cy7c67200_300_common.h"
#include "../../cy7c67200_300_debug.h"
#include "de3_dev.h"


static int dev_dbg_on = 0;

#define dev_dbg(format, arg...) \
    if( dev_dbg_on != 0 ) \
        printk(KERN_DEBUG __FILE__ ":"  "%d: " format "\n" ,__LINE__, ## arg)

/** local definitions **/

struct de3_priv 
{
    int                             major_number;
    struct usb_device_instance *    device;
    
    struct urb_link                 int_in_tx;
    
    int                             open_count;
    struct semaphore                sem;
};


static struct de3_priv * de3_priv = NULL;

extern int dbgflg_usbdfd_usbe;
#define dbg_usbe(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_usbe,lvl,fmt,##args)


/* function prototypes */

static void de3_event(struct usb_device_instance *, usb_device_event_t, int);
static int  de3_urb_sent(struct usbd_urb *, int);
static int  de3_recv_urb(struct usbd_urb *);
static void de3_function_init(struct usb_bus_instance *, 
                              struct usb_device_instance *, 
                              struct usb_function_driver *);
static void de3_function_exit(struct usb_device_instance *);

static ssize_t de3_read(struct file *, char *, size_t, loff_t *);
static ssize_t de3_write(struct file *, const char *, size_t, loff_t *);
static ssize_t ep_write(struct de3_priv *, const char *, size_t, int);
static int de3_open(struct inode *, struct file *);
static int de3_release(struct inode *, struct file *);
static int de3_ioctl(struct inode *, struct file *, unsigned int, 
                     unsigned long);

static void de3_recycle_urb(struct usbd_urb *urb, struct urb_link *link);


/******************************************************************************
*   FUNCTION CONFIGURATION DESCRIPTORS 
*/

/* Design Example 3 Endpoints
 *  +---------------------+---------------+--------------+
 *  |    Interrupt in     |  Endpoint 1   |  report      |
 *  +---------------------+ --------------+--------------|
 */
   
   
struct usb_endpoint_description de3_endpoint_description[] = 
{
    {
        bEndpointAddress :  INT_IN_EP,
        bmAttributes :      INTERRUPT,
        wMaxPacketSize :    8,
        bInterval :         10,
        direction :         IN,
        transferSize :      8,
    },
};


struct usb_class_description de3_class_description[] = 
{
    {
        bDescriptorSubtype: USB_ST_HID,
        elements:           0,
        description: {
                     hid: {
                          bcdHID:             0x100,
                          bCountryCode:       0x21,
                          bDescriptorType1:   USB_ST_REP,
                          // size needs to be consistant with de3_bios.asm
                          // HID report descriptor size
                          wDescriptorLength1: 0x3f,
                          },
                     },
    },
};

                
struct usb_alternate_description de3_alternate_description = 
{
    iInterface :            0,
    bAlternateSetting :     0,
    classes :               sizeof(de3_class_description) /
                            sizeof(struct usb_class_description),
    class_list :            de3_class_description,
    endpoints :             sizeof(de3_endpoint_description) /
                            sizeof(struct usb_endpoint_description),
    endpoint_list :         de3_endpoint_description,
    otg_description :       0,
};

struct usb_interface_description de3_interface_description[] = 
{
    {
        bInterfaceClass :       HID_INTERFACE_CLASS,
        bInterfaceSubClass :    HID_INTERFACE_SUBCLASS_BOOT,
        bInterfaceProtocol :    HID_INTERFACE_PROTOCOL_KEYBOARD,
        iInterface :            0,
        alternates :            sizeof(de3_alternate_description) /
                                sizeof(struct usb_alternate_description),
        alternate_list :        &de3_alternate_description
    },
};

struct usb_configuration_description de3_configuration_description = 
{
    iConfiguration :        0,
    bmAttributes :          BMATTRIBUTE_RESERVED | BMATTRIBUTE_SELF_POWERED,
    bMaxPower :             0,
    interfaces :            sizeof(de3_interface_description) /
                            sizeof(struct usb_interface_description),
    interface_list :        de3_interface_description
};

struct usb_device_description de3_device_description = 
{
    bDeviceClass :      DE3_DEVICE_CLASS,
    bDeviceSubClass :   DE3_DEVICE_SUBCLASS,
    bDeviceProtocol :   DE3_DEVICE_PROTOCOL,

    idVendor :          DE3_VENDOR_ID,
    idProduct :         DE3_PRODUCT_ID,

    iManufacturer :     DE3_MANUFACTURER_STR,
    iProduct :          DE3_PRODUCT_STR,
    iSerialNumber :     DE3_SERIAL_NUMBER_STR,
};

/*****************************************************************************/

struct usb_function_operations de3_function_ops = 
{
    event : de3_event,
    urb_sent : de3_urb_sent,
    recv_urb : de3_recv_urb,
    recv_setup : 0,
    function_init : de3_function_init,
    function_exit : de3_function_exit,
};

struct usb_function_driver de3_function_driver = 
{
    name : "DE3_function_driver",
    ops : &de3_function_ops,
    device_description : &de3_device_description,
    configurations : sizeof(de3_configuration_description) /
                     sizeof(struct usb_configuration_description),
    configuration_description : &de3_configuration_description,
    this_module : THIS_MODULE,
};

struct file_operations de3_file_ops = 
{
    owner : THIS_MODULE,
    read : de3_read,
    write : de3_write,
    open : de3_open,
    release : de3_release,
    ioctl : de3_ioctl,
};


/******************************************************************************
 *  PARAMETERS:  bus -> pointer to usb_bus_instance
 *               device -> pointer to usb_device_instance
 *               func_driver -> pointer to usb_function_driver
 *
 *  DESCRIPTION: This routine will initialize the function driver's private 
 *               data.  It is called from the peripheral controller 
 *
 *  RETURNS:
 *
 */

void de3_function_init(struct usb_bus_instance * bus, 
                       struct usb_device_instance * device, 
                       struct usb_function_driver * func_driver)
{
    dev_dbg("de3_function_init enter");
    
    de3_priv = kmalloc( sizeof(struct de3_priv), GFP_KERNEL);

    if(de3_priv == NULL) 
    {
        cy_err("de3_priv memory allocation failed.");
        return;
    }

    /* Register device driver */
    de3_priv->major_number = 
        register_chrdev( 0, DE3_DEVICE_NAME, &de3_file_ops );

    if( de3_priv->major_number == 0 )
    {
        cy_err("register_chrdev() failed.");
        kfree(de3_priv); 
        de3_priv = NULL;
        return;
    }
                                  
    /* save off the usb_device_instance pointer */
    de3_priv->device = device;
    
    de3_priv->open_count = 0;

    /* initialize the semaphore, (kernel) */
    sema_init(&de3_priv->sem, 1);
    
    /* save our private data into the usb_function_instance private pointer */
    device->function_instance_array->privdata = de3_priv;
}



/******************************************************************************
 *
 *  DESCRIPTION: Not used for DE3
 *
 *  RETURNS:
 *
 */

void de3_function_exit(struct usb_device_instance * device)
{
    int i;

    dev_dbg("de3_function_exit enter");

    /* deallocate URB's for each endpoint */
    for(i = 0; i < TOTAL_ENDPOINTS; i++) 
    {
        /* free up URB buffers */
    }
      
    unregister_chrdev( de3_priv->major_number, DE3_DEVICE_NAME );

    /* deallocate the function private structure */
    kfree(de3_priv);
    de3_priv = NULL;
    
    /* set the usb_function_instance privdata pointer to null */
    device->function_instance_array->privdata = NULL;
}




/******************************************************************************
 *
 *  DESCRIPTION: Event handler for events from usbd core
 *
 *  RETURNS:
 *
 */

void de3_event(struct usb_device_instance * device, usb_device_event_t event, 
               int data)
{
    extern char *usbd_device_events[];

    dev_dbg("de3_event enter: event: %s, data = %d", 
           usbd_device_events[event], data);

    switch(event)
    {
        case DEVICE_INIT:
            {
                struct usbd_urb * urb;
                size_t i;
                
                /* initialize URB linked lists */
                urb_link_init(&de3_priv->int_in_tx);

                /* allocate int send urbs */
                for(i=0; i<5; i++)
                {
                    urb = usbd_alloc_urb(device, 
                                         device->function_instance_array, 
                                         INT_IN_EP, 
                                         64);

                    de3_recycle_urb(urb, &de3_priv->int_in_tx);
                }
            }
            break;
        
        case DEVICE_UNKNOWN:
        case DEVICE_CREATE:
        case DEVICE_HUB_CONFIGURED:
        case DEVICE_RESET:
        case DEVICE_ADDRESS_ASSIGNED:
        case DEVICE_CONFIGURED:
        case DEVICE_SET_INTERFACE:
        case DEVICE_SET_FEATURE:
        case DEVICE_CLEAR_FEATURE:
        case DEVICE_DE_CONFIGURED:
        case DEVICE_BUS_INACTIVE:
        case DEVICE_BUS_ACTIVITY:
        case DEVICE_POWER_INTERRUPTION:
        case DEVICE_HUB_RESET:
        case DEVICE_DESTROY:
        case DEVICE_FUNCTION_PRIVATE:
            break;
    }
}


/******************************************************************************
 *
 *  DESCRIPTION: Callback to notify function that a URB has been transmitted
 *  on the bus successfully.
 *
 *  RETURNS: Zero for SUCCESS, negative for ERROR.
 *
 */

int de3_urb_sent(struct usbd_urb * sent_urb, int data)
{
    int return_value = ERROR;

    dev_dbg("de3_urb_sent enter");

    /* check the status of the completed URB */
    switch(sent_urb->status)
    {
    case SEND_FINISHED_OK:
    case SEND_FINISHED_ERROR:

        switch( sent_urb->endpoint->endpoint_address & 0xf)
        {
        case INT_IN_EP:
            de3_recycle_urb(sent_urb, &de3_priv->int_in_tx);
            return_value = SUCCESS;
            break;

        default:
            cy_warn("ERROR didn't recycle urb");
            break;
        }

        break;
    
    default:
        cy_err("ERROR de3_urb_sent: unhandled sent status.");
        break;
    }

    return(return_value);
}


/******************************************************************************
 *
 *  DESCRIPTION:  Called to receive a URB, called in the interrupt context.  
 *  This function determines to which endpoint the received URB belongs, and 
 *  saves off the pointer to this URB in the function's private data.  This 
 *  will then be processed in the read() function when called by the 
 *  application.  The read() function will then recycle the URB.  
 *
 *  In the future, this may just store the data in a buffer and return.  Then 
 *  the read function would read directly from the buffer.  This implementation
 *  would return the URB's immediately to the peripheral controller driver.  
 *  Currently, instead of allocating a separate buffer, this function saves 
 *  off a pointer to the URB and the read() function handles it directly, 
 *  essentially using the URB buffer to store the data, instead of allocating 
 *  separate buffer space.
 *
 *  RETURNS:  A negative value for an error condition, and zero for success.  
 *  If an error is returned, then the calling function will check this return 
 *  value, and recycle the URB, essentially discarding the data.
 *
 */

int de3_recv_urb(struct usbd_urb * recv_urb)
{
    int ret_value = -EINVAL;   
    
    dev_dbg("de3_recv_urb enter");

    if((recv_urb != NULL) && (recv_urb->status != RECV_ERROR)) 
    {
     
        // determine which endpoint received the URB.    
        switch(recv_urb->endpoint->endpoint_address & 0xf) 
        {
            default:
                usbd_recycle_urb(recv_urb);
                break;
        }

    }
    else 
    {
        usbd_recycle_urb(recv_urb);
    }
    
    return ret_value;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux read function
 *
 *  RETURNS:
 *
 */

ssize_t de3_read(struct file *file, char *buffer, size_t count, 
                        loff_t *ppos)
{
    cy_err("Bulk reads not supported in DE3");
    return -EINVAL;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux write function
 *
 *  RETURNS:
 *
 */

ssize_t de3_write(struct file *file, const char *buffer, size_t count, 
                  loff_t *ppos)
{
    cy_err("Bulk writes not supported in DE3");
    return -EINVAL;
}

/******************************************************************************
 *
 *  DESCRIPTION: Shared write function for Linux writes and ioctls
 *
 *  RETURNS:
 *
 */

ssize_t ep_write(struct de3_priv *priv, const char *buffer, size_t count, 
                 int endpoint)
{
    struct usbd_urb * urb;
    ssize_t return_value = ERROR;
    ssize_t bytes_written = 0;
    struct urb_link *urb_link;
    
    dev_dbg("ep_write enter - count:%d, ep:%d", count, endpoint);

    /* verify that the device wasn't unplugged */
    if( priv->device != NULL && 
        !( priv->device->status == USBD_OK && 
           priv->device->device_state == DEVICE_CONFIGURED ) )
    {
        dev_dbg("device not connected");
        return -ENOTCONN;
    }    

    /* is the count set */
    if(count <= 0 || buffer == NULL) 
    {
        cy_err("invalid count/buffer");
        return -EINVAL;
    }

    if( endpoint == INT_IN_EP )
    {
        urb_link = &priv->int_in_tx;
    }
    else
    {
        cy_err("Writing on unknown endpoint");
        return -EINVAL;
    }
    
    /* we have some data to send, get a URB and send it */
    urb = first_urb_detached(urb_link);

    if( urb == 0 ) 
    {
        dev_dbg("no urb pointer");
        return -EAGAIN;
    }

    /* we can only write as much as 1 urb will hold */
    bytes_written = (count > urb->buffer_length) ? urb->buffer_length : count;

    /* copy the data from userspace into our urb */
    if (copy_from_user(urb->buffer, buffer, bytes_written)) 
    {
        de3_recycle_urb(urb, urb_link);
        cy_err("copy from user error");
        return -EFAULT;
    }

    urb->actual_length = bytes_written;

    /* submit the URB */
    if( usbd_send_urb(urb) == 0 )
    {
        return_value = bytes_written;
    }
    else
    {
        cy_err("Failed to send urb");
        de3_recycle_urb(urb, urb_link);
        return_value = -EAGAIN;
    }

    dev_dbg("write exit");
    
    return return_value;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux device open
 *
 *  RETURNS:
 *
 */

int de3_open(struct inode *inode, struct file *file)
{
    int subminor;
    int retval = 0;
    
    dev_dbg("de3_open enter");

    subminor = MINOR(inode->i_rdev) - USB_DE3_MINOR_BASE;

    if ((subminor < 0) ||
        (subminor >= MAX_DEVICES)) 
    {
        return -ENODEV;
    }
    
    MOD_INC_USE_COUNT;

    /* lock this device */
    down(&de3_priv->sem);

    /* increment our usage count for the driver */
    ++de3_priv->open_count;

    /* save our object in the file's private structure */
    file->private_data = de3_priv;

    /* unlock this device */
    up(&de3_priv->sem);

    dev_dbg("de3_open exit");

    return retval;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux device close
 *
 *  RETURNS:
 *
 */

int de3_release(struct inode *inode, struct file *filp)
{

    dev_dbg("de3_release enter");

    if (de3_priv == NULL) 
    {
        cy_err("%s - object is NULL", __FUNCTION__);
        return -EFAULT;
    }

    /* lock our device */
    down(&de3_priv->sem);
    
    /* decrement our usage count for the module */
    MOD_DEC_USE_COUNT;
    
    up(&de3_priv->sem);

    dev_dbg("de3_release exit");

    return 0;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux device ioctl
 *
 *  RETURNS:
 *
 */

int de3_ioctl(struct inode *inode, struct file *file, unsigned int cmd, 
              unsigned long arg)
{
    struct de3_priv * priv;
    int return_value = ERROR;

    hid_keyboard_report_t *hidrpt = (hid_keyboard_report_t *) arg;

    //dev_dbg("de3_ioctl enter, %d", cmd);

    if((priv = (struct de3_priv *) file->private_data) != NULL) {

        /* lock this object */
        down(&priv->sem);
        
        /* verify that the device wasn't unplugged */
        if(priv->device != NULL) {
            /* we have a valid device */
            
            switch(cmd) 
            {
            case IOCTL_SEND_HID_RPT:
                if (hidrpt != NULL) 
                {
                    return_value = ep_write( priv, (char*)hidrpt->report, 
                                             HID_REPORT_SIZE, INT_IN_EP );

                    if( return_value >=0 )
                    {
                        return_value = SUCCESS;
                    }
                }
                break;
            
            case TCGETS:
                break;

            default:
                cy_err("unrecognized ioctl command: 0x%x", cmd);
                break;
            }
        }

        /* unlock the device */
        up(&priv->sem);
    }

    //dev_dbg("de3_ioctl exit");
    
    return return_value;

}


/******************************************************************************
 *
 *  DESCRIPTION: recycle transmit urbs
 *
 *  RETURNS:
 *
 */

void de3_recycle_urb(struct usbd_urb *urb, struct urb_link *link)
{
    if( urb != 0 && link != 0)
    {
        unsigned long int_flags;

        urb_link_init(&urb->link);

        local_irq_save(int_flags);
        urb_append_irq(link, urb);
        local_irq_restore(int_flags);
    }
}


/******************************************************************************
 *
 *  DESCRIPTION: Module initialization
 *
 *  RETURNS:
 *
 */

int __init de3_modinit(void)
{
    int err;

    dev_dbg("de3_modinit enter");

    /* optional name override */
    de3_function_driver.name = DRIVER_NAME;    
    
    /* register this function driver with the USBD core */
    err = usbd_register_function(&de3_function_driver);

    if( err != 0 ) 
    {
        cy_err("usbd_register_function() failed.");
    }

    return err;
}


/******************************************************************************
 *
 *  DESCRIPTION: Module exit - not used in DE3
 *
 *  RETURNS:
 *
 */

void __exit de3_modexit(void)
{
    dev_dbg("de3_modexit enter");

    usbd_deregister_function(&de3_function_driver);
}

/*****************************************************************************/

#ifdef MODULE
module_init(de3_modinit);
module_exit(de3_modexit);
#endif
