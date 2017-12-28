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
 *    DESCRIPTION: Peripheral Funciton Driver for EZ-HOST Design 
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
#include "de4_dev.h"


static int dev_dbg_on = 0;

#define dev_dbg(format, arg...) \
    if( dev_dbg_on != 0 ) \
        printk(KERN_DEBUG __FILE__ ":"  "%d: " format "\n" ,__LINE__, ## arg)


/** local definitions **/

struct de4_priv 
{
    int                             major_number;
    struct usb_device_instance *    device;
    
    struct urb_link                 bulk_out_rcv;
    struct urb_link                 bulk_in_tx;
    
    hid_keyboard_report_t           hidrpt;

    int                             open_count;
    struct semaphore                sem;
};


static struct de4_priv * de4_priv = NULL;

extern int dbgflg_usbdfd_usbe;
#define dbg_usbe(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_usbe,lvl,fmt,##args)


/* function prototypes */

static void de4_event(struct usb_device_instance *, usb_device_event_t, int);
static int  de4_urb_sent(struct usbd_urb *, int);
static int  de4_recv_urb(struct usbd_urb *);
static void de4_function_init(struct usb_bus_instance *, 
                              struct usb_device_instance *, 
                              struct usb_function_driver *);
static void de4_function_exit(struct usb_device_instance *);

static ssize_t de4_read(struct file *, char *, size_t, loff_t *);
static ssize_t ep_read(struct de4_priv *, char *, size_t, int);
static ssize_t de4_write(struct file *, const char *, size_t, loff_t *);
static ssize_t ep_write(struct de4_priv *, const char *, size_t, int);
static int de4_open(struct inode *, struct file *);
static int de4_release(struct inode *, struct file *);
static int de4_ioctl(struct inode *, struct file *, unsigned int, 
                     unsigned long);

static void de4_recycle_urb(struct usbd_urb *urb, struct urb_link *link);


/******************************************************************************
*   FUNCTION CONFIGURATION DESCRIPTORS 
*/

/* Design Example 4 Endpoints
 *  +---------------------+---------------+--------------+
 *  |      Bulk in        |  Endpoint 1   |  loopback    |
 *  +---------------------+ --------------+--------------|
 *  |      Bulk out       |  Endpoint 2   |  loopback    |
 *  +---------------------+---------------+--------------+
 *  |      Bulk out       |  Endpoint 3   |  report      |
 *  +---------------------+---------------+--------------+
 */
   
   
struct usb_endpoint_description de4_endpoint_description[] = 
{
    {
        bEndpointAddress :  BULK_IN_EP,
        bmAttributes :      BULK,
        wMaxPacketSize :    64,
        bInterval :         0,
        direction :         IN,
        transferSize :      1024,
    },
     
    {
        bEndpointAddress :  BULK_OUT_EP,
        bmAttributes :      BULK,
        wMaxPacketSize :    64,
        bInterval :         0,
        direction :         OUT,
        transferSize :      1024,
    },

    {
        bEndpointAddress :  BULK_OUT2_EP,
        bmAttributes :      BULK,
        wMaxPacketSize :    64,
        bInterval :         0,
        direction :         OUT,
        transferSize :      64,
    },

};

struct usb_alternate_description de4_alternate_description = 
{
    iInterface :            0,
    bAlternateSetting :     0,
    classes :               0,
    class_list :           NULL,
    endpoints :             sizeof(de4_endpoint_description) /
                            sizeof(struct usb_endpoint_description),
    endpoint_list :        de4_endpoint_description,
    otg_description :      0
};

struct usb_interface_description de4_interface_description = 
{
    bInterfaceClass :       0,
    bInterfaceSubClass :    0,
    bInterfaceProtocol :    0,
    iInterface :            0,
    alternates :            sizeof(de4_alternate_description) /
                            sizeof(struct usb_alternate_description),
    alternate_list :        &de4_alternate_description
};

struct usb_configuration_description de4_configuration_description = 
{
    iConfiguration :        0,
    bmAttributes :          BMATTRIBUTE_RESERVED | BMATTRIBUTE_SELF_POWERED,
    bMaxPower :             0,
    interfaces :            sizeof(de4_interface_description) /
                            sizeof(struct usb_interface_description),
    interface_list :        &de4_interface_description
};

struct usb_device_description de4_device_description = 
{
    bDeviceClass :      DE4_DEVICE_CLASS,
    bDeviceSubClass :   DE4_DEVICE_SUBCLASS,
    bDeviceProtocol :   DE4_DEVICE_PROTOCOL,

    idVendor :          DE4_VENDOR_ID,
    idProduct :         DE4_PRODUCT_ID,

    iManufacturer :     DE4_MANUFACTURER_STR,
    iProduct :          DE4_PRODUCT_STR,
    iSerialNumber :     DE4_SERIAL_NUMBER_STR
};

/*****************************************************************************/

struct usb_function_operations de4_function_ops = 
{
    event : de4_event,
    urb_sent : de4_urb_sent,
    recv_urb : de4_recv_urb,
    recv_setup : 0,
    function_init : de4_function_init,
    function_exit : de4_function_exit,
};

struct usb_function_driver de4_function_driver = 
{
    name : "DE4_function_driver",
    ops : &de4_function_ops,
    device_description : &de4_device_description,
    configurations : sizeof(de4_configuration_description) /
                     sizeof(struct usb_configuration_description),
    configuration_description : &de4_configuration_description,
    this_module : THIS_MODULE,
};

struct file_operations de4_file_ops = 
{
    owner : THIS_MODULE,
    read : de4_read,
    write : de4_write,
    open : de4_open,
    release : de4_release,
    ioctl : de4_ioctl,
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

void de4_function_init(struct usb_bus_instance * bus, 
                       struct usb_device_instance * device, 
                       struct usb_function_driver * func_driver)
{
    dev_dbg("de4_function_init enter");
    
    de4_priv = kmalloc( sizeof(struct de4_priv), GFP_KERNEL);

    if(de4_priv == NULL) 
    {
        cy_err("de4_priv memory allocation failed.");
        return;
    }

    de4_priv->hidrpt.valid = 0;

    /* Register device driver */
    de4_priv->major_number = 
        register_chrdev( 0, DE4_DEVICE_NAME, &de4_file_ops );

    if( de4_priv->major_number == 0 )
    {
        cy_err("register_chrdev() failed.");
        kfree(de4_priv); 
        de4_priv = NULL;
        return;
    }
                                  
    /* save off the usb_device_instance pointer */
    de4_priv->device = device;
    
    de4_priv->open_count = 0;

    /* initialize the semaphore, (kernel) */
    sema_init(&de4_priv->sem, 1);
    
    /* save our private data into the usb_function_instance private pointer */
    device->function_instance_array->privdata = de4_priv;
}



/******************************************************************************
 *
 *  DESCRIPTION: not used in DE4
 *
 *  RETURNS:
 *
 */

void de4_function_exit(struct usb_device_instance * device)
{
    int i;

    dev_dbg("de4_function_exit enter");

    /* deallocate URB's for each endpoint */
    for(i = 0; i < TOTAL_ENDPOINTS; i++) 
    {
        /* free up URB buffers */
    }
      
    unregister_chrdev( de4_priv->major_number, DE4_DEVICE_NAME );

    /* deallocate the function private structure */
    kfree(de4_priv);
    de4_priv = NULL;
    
    /* set the usb_function_instance privdata pointer to null */
    device->function_instance_array->privdata = NULL;
}




/******************************************************************************
 *
 *  DESCRIPTION: Event handler that processes events from the usbd core.
 *
 *  RETURNS:
 *
 */

void de4_event(struct usb_device_instance * device, usb_device_event_t event, 
               int data)
{
    extern char *usbd_device_events[];

    dev_dbg("de4_event enter: event: %s, data = %d", 
           usbd_device_events[event], data);

    switch(event)
    {
        case DEVICE_INIT:
            {
                struct usbd_urb * urb;
                size_t i;
                
                /* initialize URB linked lists */
                urb_link_init(&de4_priv->bulk_out_rcv);
                urb_link_init(&de4_priv->bulk_in_tx);

                /* allocate bulk send urbs */
                for(i=0; i<5; i++)
                {
                    urb = usbd_alloc_urb(device, 
                                         device->function_instance_array, 
                                         BULK_IN_EP, 
                                         BULK_IN_BUFF_SZ);

                    de4_recycle_urb(urb, &de4_priv->bulk_in_tx);
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

int de4_urb_sent(struct usbd_urb * sent_urb, int data)
{
    int return_value = ERROR;

    dev_dbg("de4_urb_sent enter");

    /* check the status of the completed URB */
    switch(sent_urb->status)
    {
    case SEND_FINISHED_OK:
    case SEND_FINISHED_ERROR:

        switch( sent_urb->endpoint->endpoint_address & 0xf)
        {
        case BULK_IN_EP:
            de4_recycle_urb(sent_urb, &de4_priv->bulk_in_tx);
            return_value = SUCCESS;
            break;

        default:
            cy_warn("ERROR didn't recycle urb");
            break;
        }

        break;
    
    default:
        cy_err("ERROR de4_urb_sent: unhandled sent status.");
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

int de4_recv_urb(struct usbd_urb * recv_urb)
{
    int port = 0;
    struct usb_device_instance *device = recv_urb->device;
    struct de4_priv * priv = (device->function_instance_array+port)->privdata;
    int ret_value = -EINVAL;   
    
    dev_dbg("de4_recv_urb enter");

    if((recv_urb != NULL) && (recv_urb->status != RECV_ERROR)) 
    {
     
        // determine which endpoint received the URB.    
        switch(recv_urb->endpoint->endpoint_address & 0xf) 
        {

                /* save off the URB pointer for a subsequent read by the 
                 * application, this will add the URB to a linked list, it 
                 * allows us to receive data faster than the application 
                 * may request it */
            case BULK_OUT_EP:               
                dev_dbg("de4_recv_urb BULK_OUT");
                urb_append_irq(&priv->bulk_out_rcv, recv_urb);
            
                /* the read() function will recycle the URB after the 
                 * application retrieves the data, so we are done */
                ret_value = SUCCESS;
                break;            

            case BULK_OUT2_EP:
                dev_dbg("de4_recv_urb BULK_OUT2");
                if (de4_priv->hidrpt.valid == 0) 
                {
                    if (recv_urb->actual_length == HID_REPORT_SIZE )
                    {
                        if( memcpy(de4_priv->hidrpt.report, recv_urb->buffer, 
                                   HID_REPORT_SIZE) != NULL) 
                        {
                            dev_dbg("Valid HID report");
                            de4_priv->hidrpt.valid = 1;
                        }
                    }
                }
                
                usbd_recycle_urb(recv_urb);
                ret_value = SUCCESS;            
                break;

            default:
                usbd_recycle_urb(recv_urb);
                ret_value = -EINVAL;
                break;
        }

    }
    else 
    {
        ret_value = -EINVAL;
        usbd_recycle_urb(recv_urb);
    }
    
    return ret_value;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux device read
 *
 *  RETURNS:
 *
 */

ssize_t de4_read(struct file *file, char *buffer, size_t count, 
                        loff_t *ppos)
{
    ssize_t ret;

    struct de4_priv * priv = (struct de4_priv *) file->private_data;

    down(&priv->sem);

    ret = ep_read( priv, buffer, count, BULK_OUT_EP );

    up(&priv->sem);

    return ret;
}


/******************************************************************************
 *
 *  DESCRIPTION: Read implementation for read and ioctl
 *
 *  RETURNS:
 *
 */

ssize_t ep_read(struct de4_priv *priv, char *buffer, size_t count, int endpoint)
{

    struct usbd_urb * urb;
    ssize_t return_value = ERROR;
    int length;
    struct urb_link *urb_link;
    
    //dev_dbg("ep_read enter, count:%d, ep:%d", count, endpoint);

    if( endpoint == BULK_OUT_EP )
    {
        urb_link = &priv->bulk_out_rcv;
    }
    else
    {
        cy_err("Reading on unknown endpoint");
        return -EINVAL;
    }

    /* verify that the device wasn't unplugged */
    if( priv->device != NULL && 
        !( priv->device->status == USBD_OK && 
           priv->device->device_state == DEVICE_CONFIGURED ) )
    {
        dev_dbg("device not connected");
        return -ENOTCONN;
    }    
    
    if( (urb = first_urb_detached(urb_link)) == NULL )
    {
        return -EAGAIN;
    }

    length = urb->actual_length;
        
    if(count >= length) 
    {
        if( copy_to_user(buffer, urb->buffer, length) ) 
        {
            /* put back on queue */
            urb_link_init(&urb->link);
            urb_append_irq(urb_link, urb);

            cy_err("copy_to_user failure");
            return_value = -EFAULT;
        }
        else 
        {
            usbd_recycle_urb(urb);
            return_value = length;
        }            

        dev_dbg("1:de4_read, return:%d", return_value);
    }
    else
    {
        /* there is more data in this URB than is currently requested,
           return the requested amount, and then place the remaining data
           back in the receive queue, at the head. */
        int difference;
        
        difference = length - count; /* size of remaining data of URB buffer */
        
        if( copy_to_user(buffer, urb->buffer, count) ) 
        {
            /* put back on queue */
            urb_link_init(&urb->link);
            urb_append_irq(urb_link, urb);

            cy_err("copy_to_user failure");
            return_value = -EFAULT;
        }
        else {
            memcpy(urb->buffer, urb->buffer + count, difference); 
            urb->actual_length = difference; 

            /* put back on queue */
            urb_link_init(&urb->link);
            urb_append_irq(urb_link, urb);

            return_value = count;
        }

        dev_dbg("2:de4_read, return:%d", return_value);
    }

    return return_value;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux device write
 *
 *  RETURNS:
 *
 */

ssize_t de4_write(struct file *file, const char *buffer, size_t count, 
                  loff_t *ppos)
{
    ssize_t ret;

    struct de4_priv *priv = (struct de4_priv *) file->private_data;

    down(&priv->sem);

    ret = ep_write(priv, buffer, count, BULK_IN_EP);

    up(&priv->sem);

    return ret;
}

/******************************************************************************
 *
 *  DESCRIPTION: Write implementation for write and ioctl
 *
 *  RETURNS:
 *
 */

ssize_t ep_write(struct de4_priv *priv, const char *buffer, size_t count, 
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

    if( endpoint == BULK_IN_EP )
    {
        urb_link = &priv->bulk_in_tx;
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
        de4_recycle_urb(urb, urb_link);
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
        de4_recycle_urb(urb, urb_link);
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

int de4_open(struct inode *inode, struct file *file)
{
    int subminor;
    int retval = 0;
    
    dev_dbg("de4_open enter");

    subminor = MINOR(inode->i_rdev) - USB_DE4_MINOR_BASE;

    if ((subminor < 0) ||
        (subminor >= MAX_DEVICES)) 
    {
        return -ENODEV;
    }
    
    MOD_INC_USE_COUNT;

    /* lock this device */
    down(&de4_priv->sem);

    /* increment our usage count for the driver */
    ++de4_priv->open_count;

    /* save our object in the file's private structure */
    file->private_data = de4_priv;

    /* unlock this device */
    up(&de4_priv->sem);

    dev_dbg("de4_open exit");

    return retval;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux device close
 *
 *  RETURNS:
 *
 */

int de4_release(struct inode *inode, struct file *filp)
{

    dev_dbg("de4_release enter");

    if (de4_priv == NULL) 
    {
        cy_err("%s - object is NULL", __FUNCTION__);
        return -EFAULT;
    }

    /* lock our device */
    down(&de4_priv->sem);
    
    /* decrement our usage count for the module */
    MOD_DEC_USE_COUNT;
    
    up(&de4_priv->sem);

    dev_dbg("de4_release exit");

    return 0;
}


/******************************************************************************
 *
 *  DESCRIPTION: Linux device ioctl
 *
 *  RETURNS:
 *
 */


int de4_ioctl(struct inode *inode, struct file *file, unsigned int cmd, 
              unsigned long arg)
{
    struct de4_priv * priv;
    int return_value = ERROR;

    hid_keyboard_report_t *hidrpt = (hid_keyboard_report_t *) arg;

    //dev_dbg("de4_ioctl enter, %d", cmd);

    if((priv = (struct de4_priv *) file->private_data) != NULL) 
    {
        /* lock this object */
        down(&priv->sem);
        
        /* verify that the device wasn't unplugged */
        if(priv->device != NULL) 
        {
            /* we have a valid device */
            switch(cmd) 
            {
            case IOCTL_GET_HID_RPT:
                if (hidrpt != NULL) 
                {
                    /* Check for valid report to send */
                    if( de4_priv->hidrpt.valid != 0 ) 
                    {
                        /*
                        int i;
                        printk("ker");
                        for(i=0; i<HID_REPORT_SIZE; i++)
                            printk(":0x%02x", de4_priv->hidrpt.report[i]);
                        printk("\n");
                        */

                        /* Copy report data to user buffer */
                        if( copy_to_user(hidrpt, &de4_priv->hidrpt,
                                         sizeof(hid_keyboard_report_t)) == 0 ) 
                        {
                            return_value = SUCCESS;
                        } 

                        de4_priv->hidrpt.valid = 0;
                    } 
                    else 
                    {
                        int valid = 0;

                        /* No report to send */
                        copy_to_user(&hidrpt->valid,
                                     &valid,
                                     sizeof(int));

                        return_value = SUCCESS;
                    }
                } 
                break;
            
            case IOCTL_IS_CONNECTED:
                if( priv->device != NULL && 
                    priv->device->status == USBD_OK && 
                    priv->device->device_state == DEVICE_CONFIGURED )
                {
                    return_value = SUCCESS;
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

    //dev_dbg("de4_ioctl exit");
    
    return return_value;

}


/******************************************************************************
 *
 *  DESCRIPTION: recycle transmit urbs
 *
 *  RETURNS:
 *
 */

void de4_recycle_urb(struct usbd_urb *urb, struct urb_link *link)
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
 *  DESCRIPTION: Driver module initialization
 *
 *  RETURNS:
 *
 */

int __init de4_modinit(void)
{
    int err;

    dev_dbg("de4_modinit enter");

    /* optional name override */
    de4_function_driver.name = DRIVER_NAME;    
    
    /* register this function driver with the USBD core */
    err = usbd_register_function(&de4_function_driver);

    if( err != 0 ) 
    {
        cy_err("usbd_register_function() failed.");
    }

    return err;
}


/******************************************************************************
 *
 *  DESCRIPTION: Device module exit - not used in DE4
 *
 *  RETURNS:
 *
 */

void __exit de4_modexit(void)
{
    dev_dbg("de4_modexit enter");

    usbd_deregister_function(&de4_function_driver);
}

/*****************************************************************************/

#ifdef MODULE
module_init(de4_modinit);
module_exit(de4_modexit);
#endif
