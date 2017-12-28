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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <linux/smp_lock.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/usb.h>
#include "cy7c67200_300.h"
#include "cy7c67200_300_common.h"
#include "cy7c67200_300_lcd.h"
#include "lcp_cmd.h"
#include "lcp_data.h"
#include "cy7c67200_300_otg.h"
#include "cy7c67200_300_hcd.h"
#include "cy7c67200_300_hcd_simple.h"
#include "cy7c67200_300_debug.h"

/* maximum bandwidth in the one millisecond frame in terms of bits */
#define MAX_FRAME_BW 4096
#define MAX_PERIODIC_BW 630

#define SETUP_STAGE     0
#define DATA_STAGE      1
#define STATUS_STAGE    2


/* main lock for urb access */
static spinlock_t usb_urb_lock = SPIN_LOCK_UNLOCKED;

static int iso_urb_cnt=0;
static const int usb_bandwidth_option = 1;

/* forward declarations */
static inline int qu_pipeindex (__u32 pipe);
static int qu_queue_urb (cy_priv_t * cy_priv, struct urb * urb, int port_num);
int sie_check_bandwidth (hci_t *hci, int sie_num, struct usb_device *dev, struct
urb *urb);
void sie_claim_bandwidth (hci_t * hci, int sie_num, struct usb_device *dev,
struct urb *urb, int bustime, int isoc);
void sie_release_bandwidth(hci_t *hci, int sie_num, struct usb_device *dev,
struct urb *urb, int isoc);
int sh_schedule_trans (cy_priv_t * cy_priv, int sie_num);


/* External */
extern int fillTd(hci_t * hci, td_t * td, __u16 * td_addr, __u16 * buf_addr,
__u8 devaddr,
                  __u8 epaddr, int pid, int len, int toggle, int slow,
                  int tt, int port_num);

extern int urb_debug;

/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
/* URB HCD API function layer
 * * * */

int get_port_num_from_urb(cy_priv_t * cy_priv, struct urb * urb)
{
    hci_t * hci = cy_priv->hci;
    int i;

    if ((hci == NULL) || (urb == NULL))
        return ERROR;

    if (urb->dev == NULL)
        return ERROR;

    if (urb->dev->bus == NULL)
        return ERROR;

    for (i = 0; i < MAX_NUM_PORT; i++)
    {
        /* Find the right bus port device number */
        if (hci->valid_host_port[i] == 1)
        {
            if (urb->dev->bus->busnum == hci->port[i].bus->busnum)
            {
                return (i);
            }
        }
    }

    cy_err("get_port_num_from_urb: can't find valid port num\n");
    return ERROR;
}

static int get_port_num_from_dev(cy_priv_t * cy_priv, struct usb_device *
usb_dev)
{
    hci_t * hci = cy_priv->hci;
    int i;

    if ((hci == NULL) || (usb_dev == NULL))
        return ERROR;

    if (usb_dev->bus == NULL)
        return ERROR;

    for (i = 0; i < MAX_NUM_PORT; i++)
    {
        /* Find the right bus port device number */
        if (hci->valid_host_port[i] == 1)
        {
            if (usb_dev->bus->busnum == hci->port[i].bus->busnum)
            {
                return (i);
            }
        }
    }
    return ERROR;
}

/***************************************************************************
 * Function Name : hcs_urb_queue
 *
 * This function initializes the urb status and length before queueing the
 * urb.
 *
 * Input:  hci = data structure for the host controller
 *         urb = USB request block data structure
 *
 * Return: 0
 **************************************************************************/

static inline int hcs_urb_queue (cy_priv_t * cy_priv, struct urb * urb, int
port)
{
    int i;
    hci_t * hci = cy_priv->hci;
    int bustime = 0;

    cy_dbg("enter hcs_urb_queue \n");
    if (usb_pipeisoc (urb->pipe))
    {
        cy_dbg("hcs_urb_queue: isoc pipe \n");
        for (i = 0; i < urb->number_of_packets; i++)
        {
            urb->iso_frame_desc[i].actual_length = 0;
            urb->iso_frame_desc[i].status = -EXDEV;
        }

        /* urb->next hack : 1 .. resub, 0 .. single shot */
    urb->interval = urb->next ? 1 : 0;
    }

    /* allocate and claim bandwidth if needed; ISO
     * needs start frame index if it was't provided.
     */
    switch (usb_pipetype (urb->pipe))
    {
        case PIPE_ISOCHRONOUS:
        case PIPE_INTERRUPT:
            if (urb->bandwidth == 0) {
                    bustime = usb_check_bandwidth (urb->dev, urb);
            }
            if (bustime < 0) {
                usb_dec_dev_use (urb->dev);
                return bustime;
            }
            usb_claim_bandwidth (urb->dev, urb, bustime, usb_pipeisoc
            (urb->pipe));
     }

    urb->status = USB_ST_URB_PENDING;
    urb->actual_length = 0;
    urb->error_count = 0;

    if (usb_pipecontrol (urb->pipe)) {
        urb->interval = 0;
        hc_flush_data_cache (hci, urb->setup_packet, 8);
    }

    if (usb_pipeout (urb->pipe)) {
        hc_flush_data_cache (hci, urb->transfer_buffer,
                             urb->transfer_buffer_length);
    }

    qu_queue_urb (cy_priv, urb, port);

    return 0;
}

/***************************************************************************
 * Function Name : hcs_return_urb
 *
 * This function the return path of URB back to the USB core. It calls the
 * the urb complete function if exist, and also handles the resubmition of
 * interrupt URBs.
 *
 * Input:  hci = data structure for the host controller
 *         urb = USB request block data structure
 *         resub_ok = resubmit flag: 1 = submit urb again, 0 = not submit
 *
 * Return: 0
 **************************************************************************/

static int hcs_return_urb (cy_priv_t * cy_priv, struct urb * urb, int port_num,
int resub_ok)
{
    struct usb_device * dev = urb->dev;
    int resubmit = 0;

    /* cy_dbg("hcs_return_urb \n"); */

    if (urb == NULL)
    {
        cy_err("hcs_return_urb: urb == NULL\n");
    }
    cy_dbg("enter hcs_return_urb, urb pointer = 0x%x, "
             "transferbuffer point = 0x%x, next pointer = 0x%x,"
             " setup packet pointer = 0x%x, context pointer = 0x%x \n",
             (__u32 *) urb, (__u32 *) urb->transfer_buffer,
             (__u32 *) urb->next, (__u32 *) urb->setup_packet,
             (__u32 *) urb->context);
    if (urb_debug)
        urb_print (urb, "RET", usb_pipeout (urb->pipe));

    resubmit = urb->interval && resub_ok;

    switch (usb_pipetype (urb->pipe)) {
        case PIPE_INTERRUPT:
            /* Release int bandwidth */
            if (urb->bandwidth) {
                usb_release_bandwidth (urb->dev, urb, 0);
            }

            urb->dev = urb->hcpriv = NULL;

            if (urb->complete)
            {
                urb->complete (urb);
            }

            if (resubmit)
            {
                /* requeue the URB */
                urb->dev = dev;
                hcs_urb_queue (cy_priv, urb, port_num);
            }
            break;

        case PIPE_ISOCHRONOUS:
        case PIPE_BULK:
        case PIPE_CONTROL: /* unlink URB, call complete */
            /* Release iso bandwidth */
            if ((urb->bandwidth) && (usb_pipetype (urb->pipe) == PIPE_ISOCHRONOUS))
            {
                usb_release_bandwidth (urb->dev, urb, 1);
            }

            if (urb->complete)
            {
                urb->complete (urb); /* call complete */
                if (usb_pipeisoc(urb->pipe))
                {
                    iso_urb_cnt--;
                }
            }
            break;
        }

    return 0;
}

/***************************************************************************
 * Function Name : hci_submit_urb
 *
 * This function is called by the USB core API when an URB is available to
 * process.  This function does the following
 *
 * 1) Check the validity of the URB
 * 2) Parse the device number from the URB
 * 3) Pass the URB to the root hub routine if its intended for the hub, else
 *    queue the urb for the attached device.
 *
 * Input: urb = USB request block data structure
 *
 * Return: 0 if success or error code
 **************************************************************************/

static int hci_submit_urb (struct urb * urb)
{
    hci_t * hci;
    cy_priv_t * cy_priv;
    unsigned int pipe = urb->pipe;
    unsigned long int_flags;
    int ret = -1;
    int i;

    /* cy_dbg("enter hci_submit_urb, pipe = 0x%x, dev = 0x%x, bus = 0x%x\n", 
                 urb->pipe, urb->dev, urb->dev->bus); */

  if (!urb->dev || !urb->dev->bus || urb->hcpriv)
    {
        cy_err("hci_submit_urb: urb->dev = 0x%x, urb->dev->bus = 0x%x, urb->hcpriv = 0x%x\n",
               (int) urb->dev, (int) urb->dev->bus, (int) urb->hcpriv);
        return -EINVAL;
    }

    if (usb_endpoint_halted (urb->dev, usb_pipeendpoint (pipe),
        usb_pipeout (pipe)))
    {
        cy_err("hci_submit_urb: endpoint_halted\n");
        return -EPIPE;
    }
    cy_priv = (cy_priv_t *) urb->dev->bus->hcpriv;
    hci = cy_priv->hci;

    if (cy_priv == NULL || hci == NULL || urb == NULL)
    {
        cy_err("hci_submit_urb: Error, cy_priv = 0x%x, hci = 0x%x, urb = 0x%x! \n",
                (int) cy_priv, (int) hci, (int) urb);
        return -EINVAL;
    }

    /* a request to the virtual root hub */
    for (i = 0; i < MAX_NUM_PORT; i++)
    {
        /* Find the right bus port device number */

        if (hci->valid_host_port[i] == 1) {

            if (urb->dev->bus->busnum != hci->port[i].bus->busnum)
            {
                /* cy_dbg("sumbit urb: busnum = 0x%x,  port %d s/b 0x%x\n",  
  			urb->dev->bus->busnum,i, hci->port[i].bus->busnum);*/
                continue;
            }

            /* Is the bus being disable? */
            if (usb_pipedevice (pipe) == hci->port[i].rh.devnum)
            {
                if (urb_debug > 1)
                    urb_print (urb, "SUB-RH", usb_pipein (pipe));
                return rh_submit_urb (urb, i);
            }

            /* queue the URB to its endpoint-queue */

            if (usb_pipeisoc(urb->pipe))
            {
                iso_urb_cnt++;
            }

            spin_lock_irqsave (&usb_urb_lock, int_flags);
            ret = hcs_urb_queue (cy_priv, urb, i);
            if (ret != 0)
            {
                /* error on return */
                cy_err("hci_submit_urb:return err,ret=0x%x,urb->status=0x%x\n", 
			ret, urb->status);
            }

            spin_unlock_irqrestore (&usb_urb_lock, int_flags);
        }
    }

    return ret;
}

/***************************************************************************
 * Function Name : hci_unlink_urb
 *
 * This function mark the URB to unlink
 *
 * Input: urb = USB request block data structure
 *
 * Return: 0 if success or error code
 **************************************************************************/

static int hci_unlink_urb (struct urb * urb)
{
    unsigned long int_flags;
    hci_t * hci;
    struct hci_device * hci_dev;
    DECLARE_WAITQUEUE (wait, current);
    void * comp = NULL;
    int i = 0;
    int port_num = -1;
    cy_priv_t * cy_priv = NULL;
    epd_t * ed = NULL;

    cy_dbg("enter hci_unlink_urb \n");

    if (!urb) /* just to be sure */
        return -EINVAL;

    if (!urb->dev || !urb->dev->bus)
        return -ENODEV;

    cy_priv = (cy_priv_t * ) urb->dev->bus->hcpriv;

    hci = cy_priv->hci;
    hci_dev = usb_to_hci (urb->dev);
    if (hci_dev != NULL)
        ed = &hci_dev->ed [qu_pipeindex (urb->pipe)];

    if ((hci == NULL) || (cy_priv == NULL) || hci_dev == NULL || ed == NULL)
    {
        cy_err("hci_unlink_urb: error hci = 0x%x, cy_priv = 0x%x, hci_dev = 0x%x, ed = 0x%x\n",
               (int) hci, (int) cy_priv, (int) hci_dev, (int) ed);
        return ERROR;
    }

    /* a request to the virtual root hub */
    for (i = 0; i < MAX_NUM_PORT; i++)
    {
        if (hci->valid_host_port[i] == 1) {

            if (usb_pipedevice (urb->pipe) == hci->port[i].rh.devnum)
            {
                /* The urb belong to the root hub, so send to root hub */
                return rh_unlink_urb (urb, i);
            }
            if (urb->dev->bus->busnum == hci->port[i].bus->busnum)
            {
                /* need port_num for non root hub urbs */
                port_num = i;
            }
        }
    }

    if (port_num == -1)
    {
        cy_err("hci_unlink_urb: unknown port number\n");
    }

    if (urb_debug)
        urb_print (urb, "UNLINK", 1);

    spin_lock_irqsave (&usb_urb_lock, int_flags);

    /* Release int/iso bandwidth */
    if (urb->bandwidth) {
        switch (usb_pipetype(urb->pipe)) {
        case PIPE_INTERRUPT:
            usb_release_bandwidth (urb->dev, urb, 0);
            break;
        case PIPE_ISOCHRONOUS:
            usb_release_bandwidth (urb->dev, urb, 1);
            break;
        default:
            break;
        }
    }

    if (!list_empty (&urb->urb_list) && urb->status == USB_ST_URB_PENDING)
    {
        /* URB active? */

        if (urb->transfer_flags & (USB_ASYNC_UNLINK | USB_TIMEOUT_KILLED))
        {
            /* asynchron with callback */
            list_del (&urb->urb_list); /* relink the urb to the del list */
            list_add (&urb->urb_list, &hci->port[port_num].del_list);

            if (ed->pipe_head == urb)
                ed->pipe_head = NULL;

            spin_unlock_irqrestore (&usb_urb_lock, int_flags);

        }
        else
        {
            /* synchron without callback */
            add_wait_queue (&hci->waitq, &wait);

            set_current_state (TASK_UNINTERRUPTIBLE);
            comp = urb->complete;
            urb->complete = NULL;

            list_del (&urb->urb_list); /* relink the urb to the del list */
            list_add (&urb->urb_list, &hci->port[port_num].del_list);

            spin_unlock_irqrestore (&usb_urb_lock, int_flags);

            schedule_timeout(HZ/50);

            if (!list_empty (&urb->urb_list))
                list_del (&urb->urb_list);

            if (ed->pipe_head == urb)
                ed->pipe_head = NULL;

            urb->complete = comp;
            urb->hcpriv = NULL;
            remove_wait_queue (&hci->waitq, &wait);
        }
    }
    else
    {
        /* hcd does not own URB but we keep the driver happy anyway */
        spin_unlock_irqrestore (&usb_urb_lock, int_flags);

        if (urb->complete && (urb->transfer_flags & USB_ASYNC_UNLINK))
        {
            urb->status = -ENOENT;
            urb->actual_length = 0;

            urb->complete (urb);  /* DO WE NEED TO DO THIS?? */
            urb->status = 0;
        }
        else
        {
            urb->status = -ENOENT;
        }
    }

    return 0;
}

/***************************************************************************
 * Function Name : hci_alloc_dev
 *
 * This function allocates private data space for the usb device and
 * initialize the endpoint descriptor heads.
 *
 * Input: usb_dev = pointer to the usb device
 *
 * Return: 0 if success or error code
 **************************************************************************/

static int hci_alloc_dev (struct usb_device * usb_dev)
{
    struct hci_device * dev;
    int i;

    /* cy_dbg("enter hci_alloc_dev \n"); */

    dev = kmalloc (sizeof (*dev), GFP_KERNEL);
    if (!dev)
    return -ENOMEM;

    memset (dev, 0, sizeof (*dev));

    for (i = 0; i < NUM_EDS; i++)
    {
        INIT_LIST_HEAD (&(dev->ed [i].urb_queue));
        dev->ed [i].pipe_head = NULL;
        dev->ed [i].last_iso = 0;
    }
    usb_dev->hcpriv = dev;

    /* cy_dbg("USB HC dev alloc %d bytes \n" , sizeof (*dev)); */

    return 0;

}

/***************************************************************************
 * Function Name : hci_free_dev
 *
 * This function de-allocates private data space for the usb devic
 *
 * Input: usb_dev = pointer to the usb device
 *
 * Return: 0
 **************************************************************************/

static int hci_free_dev (struct usb_device * usb_dev)
{
    cy_dbg("enter hci_free_dev \n");

    if (usb_dev->hcpriv)
        kfree (usb_dev->hcpriv);

    usb_dev->hcpriv = NULL;
    return 0;
}

/***************************************************************************
 * Function Name : hci_get_current_frame_number
 *
 * This function get the current USB frame number
 *
 * Input: usb_dev = pointer to the usb device
 *
 * Return: frame number
 **************************************************************************/

int hci_get_current_frame_number (struct usb_device *usb_dev)
{
    cy_priv_t * cy_priv = usb_dev->bus->hcpriv;
    hci_t * hci = cy_priv->hci;
    int port_num = -1;

    /* cy_dbg("enter hci_get_current_frame_number \n"); */

    if ((hci == NULL) || (cy_priv == NULL))
    {
        cy_err("hci_get_current_frame_number: error hci = 0x%x, cy_priv = 0x%x\n",
               (int) hci, (int) cy_priv);
        return ERROR;
    }

    port_num = get_port_num_from_dev(cy_priv, usb_dev);

    if (port_num == ERROR)
    {
        cy_err("hci_get_current_frame_number: can't find port num\n");
        return ERROR;
    }

    /* Get the right port */
    return (cy67x00_get_current_frame_number (cy_priv, port_num));
}


/***************************************************************************
 * List of all io-functions
 **************************************************************************/

struct usb_operations hci_device_operations = {
    hci_alloc_dev,
    hci_free_dev,
    hci_get_current_frame_number,
    hci_submit_urb,
    hci_unlink_urb
};

/***************************************************************************
 * URB queueing:
 *
 * For each type of transfer (INTR, BULK, ISO, CTRL) there is a list of
 * active URBs.
 * (hci->intr_list, hci->bulk_list, hci->iso_list, hci->ctrl_list)
 * For every endpoint the head URB of the queued URBs is linked to one of
 * those lists.
 *
 * The rest of the queued URBs of an endpoint are linked into a
 * private URB list for each endpoint. (hci_dev->ed [endpoint_io].urb_queue)
 * hci_dev->ed [endpoint_io].pipe_head .. points to the head URB which is
 * in one of the active URB lists.
 *
 * The index of an endpoint consists of its number and its direction.
 *
 * The state of an intr and iso URB is 0.
 * For ctrl URBs the states are US_CTRL_SETUP, US_CTRL_DATA, US_CTRL_ACK
 * Bulk URBs states are US_BULK and US_BULK0 (with 0-len packet)
 *
 **************************************************************************/

/***************************************************************************
 * Function Name : qu_urb_timeout
 *
 * This function is called when the URB timeout. The function unlinks the
 * URB.
 *
 * Input: lurb: URB
 *
 * Return: none
 **************************************************************************/

#ifdef HC_URB_TIMEOUT
static void qu_urb_timeout (unsigned long lurb)
{
    struct urb * urb = (struct urb *) lurb;

    cy_dbg("enter qu_urb_timeout \n");
    urb->transfer_flags |= USB_TIMEOUT_KILLED;
    hci_unlink_urb (urb);
}
#endif

/***************************************************************************
 * Function Name : qu_pipeindex
 *
 * This function gets the index of the pipe.
 *
 * Input: pipe: the urb pipe
 *
 * Return: index
 **************************************************************************/

static inline int qu_pipeindex (__u32 pipe)
{
    /* cy_dbg("enter qu_pipeindex \n"); */
    return (usb_pipeendpoint (pipe) << 1) | (usb_pipecontrol (pipe) ?
                0 : usb_pipeout (pipe));
}

/***************************************************************************
 * Function Name : qu_seturbstate
 *
 * This function set the state of the URB.
 *
 * control pipe: 3 states -- Setup, data, status
 * interrupt and bulk pipe: 1 state -- data
 *
 * Input: urb = USB request block data structure
 *        state = the urb state
 *
 * Return: none
 **************************************************************************/

static inline void qu_seturbstate (struct urb * urb, int state)
{
    /* cy_dbg("enter qu_seturbstate \n"); */
    urb->pipe &= ~0x1f;
    urb->pipe |= state & 0x1f;
}

/***************************************************************************
 * Function Name : qu_urbstate
 *
 * This function get the current state of the URB.
 *
 * Input: urb = USB request block data structure
 *
 * Return: none
 **************************************************************************/

static inline int qu_urbstate (struct urb * urb)
{

    /* cy_dbg("enter qu_urbstate\n"); */

    return urb->pipe & 0x1f;
}

/***************************************************************************
 * Function Name : qu_queue_active_urb
 *
 * This function adds the urb to the appropriate active urb list and set
 * the urb state.
 *
 * There are four active lists: isochoronous list, interrupt list,
 * control list, and bulk list.
 *
 * Input: cy_priv  = private data structure
 *        urb      = USB request block data structure
 *        ed       = endpoint descriptor
 *        port_num = port number
 *
 * Return: none
 **************************************************************************/

static inline void qu_queue_active_urb (cy_priv_t * cy_priv, struct urb * urb,
epd_t * ed, int port_num)
{
    int urb_state = 0;
    hci_t * hci = cy_priv->hci;

    /* cy_dbg("enter qu_queue_active_urb \n"); */

    switch (usb_pipetype (urb->pipe))
    {
    case PIPE_CONTROL:
        list_add (&urb->urb_list, &hci->port[port_num].ctrl_list);
        break;

    case PIPE_BULK:
        list_add (&urb->urb_list, &hci->port[port_num].bulk_list);
        break;

    case PIPE_INTERRUPT:
        urb->start_frame = cy67x00_get_current_frame_number (cy_priv, port_num);
        list_add (&urb->urb_list, &hci->port[port_num].intr_list);
        break;

    case PIPE_ISOCHRONOUS:
        cy_dbg("qu_queue_active_urb: add urb to iso_list, start_frame = "
                "0x%x, # Pkts = 0x%x, err Cnt = 0x%x\n ", urb->start_frame,
                     urb->number_of_packets, urb->error_count);

        list_add (&urb->urb_list, &hci->port[port_num].iso_list);
        break;
    }

#ifdef HC_URB_TIMEOUT
    if (urb->timeout)
    {
        ed->timeout.data = (unsigned long) urb;
        ed->timeout.expires = urb->timeout + jiffies;
        ed->timeout.function = qu_urb_timeout;
        add_timer (&ed->timeout);
    }
#endif

    qu_seturbstate (urb, urb_state);
}

/***************************************************************************
 * Function Name : qu_queue_urb
 *
 * This function adds the urb to the endpoint descriptor list
 *
 * Input: hci = data structure for the host controller
 *        urb = USB request block data structure
 *
 * Return: none
 **************************************************************************/

static int qu_queue_urb (cy_priv_t * cy_priv, struct urb * urb, int port_num)
{
    struct hci_device * hci_dev = usb_to_hci (urb->dev);
    epd_t * ed = &hci_dev->ed [qu_pipeindex (urb->pipe)];

    /* cy_dbg("Enter qu_queue_urb \n"); */

    /* for ISOC transfers calculate start frame index */

    if (usb_pipeisoc (urb->pipe) && urb->transfer_flags & USB_ISO_ASAP)
    {
        if (ed->pipe_head)
        {
            urb->start_frame = ed->last_iso;
            ed->last_iso = (ed->last_iso + urb->number_of_packets) % 2048;
        }
        else
        {
            urb->start_frame = cy67x00_get_next_frame_number(cy_priv, port_num);
            ed->last_iso = (urb->start_frame + urb->number_of_packets) % 2048;
        }
    }

    if (ed->pipe_head)
    {
        list_add_tail(&urb->urb_list, &ed->urb_queue);
    }
    else
    {
        ed->pipe_head = urb;
        qu_queue_active_urb (cy_priv, urb, ed, port_num);
    }

    return 0;
}

/***************************************************************************
 * Function Name : qu_next_urb
 *
 * This function removes the URB from the queue and add the next URB to
 * active list.
 *
 * Input: hci = data structure for the host controller
 *        urb = USB request block data structure
 *        resub_ok = resubmit flag
 *
 * Return: pointer to the next urb
 **************************************************************************/

static struct urb * qu_next_urb (cy_priv_t * cy_priv, struct urb * urb, int
port_num, int resub_ok)
{
    struct hci_device * hci_dev = usb_to_hci (urb->dev);
    epd_t * ed = &hci_dev->ed [qu_pipeindex (urb->pipe)];
    struct urb * tmp_urb = NULL;
    struct list_head * list_lh;
    struct list_head * lh;

    /* cy_dbg("enter qu_next_urb \n"); */

    list_del (&urb->urb_list);

    if (ed->pipe_head == urb)
    {

#ifdef HC_URB_TIMEOUT
        if (urb->timeout)
            del_timer (&ed->timeout);
#endif
        if (!list_empty (&ed->urb_queue))
        {
            list_lh = &ed->urb_queue;
            lh = list_lh->next;
            tmp_urb = list_entry (lh, struct urb, urb_list);
            list_del (&tmp_urb->urb_list);
            ed->pipe_head = tmp_urb;
            qu_queue_active_urb (cy_priv, tmp_urb, ed, port_num);
            cy_dbg("qu_next_urb: queue next active urb, iso = %c\n",
            usb_pipeisoc(tmp_urb->pipe)?'Y':'N' );
        }
        else
        {
            ed->pipe_head = NULL;
            tmp_urb = NULL;
            cy_dbg("qu_next_urb: queue is empty!\n");
        }
    }

    return tmp_urb;
}

/***************************************************************************
 * Function Name : qu_return_urb
 *
 * This function is part of the return path.
 *
 * Input: hci = data structure for the host controller
 *        urb = USB request block data structure
 *        resub_ok = resubmit flag
 *
 * Return: pointer to the next urb
 **************************************************************************/

static struct urb * qu_return_urb (cy_priv_t * cy_priv, struct urb * urb, int
port_num, int resub_ok)
{

    struct urb * next_urb;

    cy_dbg("enter qu_return_rub \n");
    next_urb = qu_next_urb (cy_priv, urb, port_num, resub_ok);
    hcs_return_urb (cy_priv, urb, port_num, resub_ok);
    return next_urb;
}

int sh_calc_urb_bitTime(struct urb * urb)
{
    int bitTime = 0;

    bitTime = 8 * (urb->transfer_buffer_length - urb->actual_length);

    if (bitTime > MAX_FRAME_BW)
    {
       bitTime = MAX_FRAME_BW;
    }
    return (bitTime);

}

/***************************************************************************
 * Function Name : sh_add_urb_to_frame
 *
 * This function adds the urb to the current USB frame.  There can be
 * multiple USB transactions associated with the URB.  Providing that
 * the transfer_buffer_length - actual_length can be big.  Therefore we
 * have to include all USB transaction associated with the URB in this frame
 * Therefore, the URB  will add to the frame list if the
 * max packet size + current frame BW < EOT
 *
 * Input: hci = data structure for the host controller
 *        urb = USB request block data structure
 *
 * Return: 1 = successful; bandwidth still available to add more urb
 *         0 = unsuccessful; no more bandwidth
 **************************************************************************/
int sh_add_urb_to_frame(hci_t * hci, struct urb * urb, int port_num)
{
    int sie_num = port_num / 2;
    int bitTime;

    if (urb == NULL)
    {
        cy_dbg("sh_add_urb_to_frame \n");
        return 0;
    }

    bitTime = 8 * usb_maxpacket (urb->dev, urb->pipe, usb_pipeout (urb->pipe));

    /* CHECK total BANDWIDTH */

    if ((bitTime + hci->sie[sie_num].bandwidth_allocated) > MAX_FRAME_BW)
    {
        /* no bandwidth left in the frame, don't schedule it */
        return -1;
    }

    switch (usb_pipetype (urb->pipe))
    {
        case PIPE_ISOCHRONOUS:
        case PIPE_INTERRUPT:

        /* Check for periodic bandwidth */

        if ((hci->sie[sie_num].periodic_bw_allocated + bitTime) >
        MAX_PERIODIC_BW)
        {
            return -1;
        }
        else
        {
            hci->sie[sie_num].periodic_bw_allocated += sh_calc_urb_bitTime(urb);
        }
    }

    hci->sie[sie_num].bandwidth_allocated += sh_calc_urb_bitTime(urb);

    /* Break the urb from the urb chain */
    list_del(&urb->urb_list);

    /* Add the urb to the frame list */

    list_add_tail(&urb->urb_list, &hci->port[port_num].frame_urb_list);
    return bitTime;
}

boolean  sh_check_frame_bw(hci_t * hci, int sie_num, int len)
{
    int bitTime = len * 8;
    int alloc_bw =  hci->sie[sie_num].bandwidth_allocated;

    return((alloc_bw + bitTime) < MAX_FRAME_BW);
}

void sh_claim_frame_bw(hci_t * hci, int sie_num, int len)
{
    int bitTime = len * 8;
    hci->sie[sie_num].bandwidth_allocated += bitTime;
}

void sh_release_frame_bw (hci_t * hci, int sie_num, int len)
{
    int bitTime = len * 8;
    hci->sie[sie_num].bandwidth_allocated -= bitTime;
}

void sh_init_frame_bw (hci_t * hci, int sie_num)
{
    hci->sie[sie_num].bandwidth_allocated = 0;
    hci->sie[sie_num].periodic_bw_allocated = 0;
}

/* This function will convert the URB into the list of TD's and
 * copies the TD and its data into the CY7C67200/300 memory
 */

int sh_add_urb_to_TD(cy_priv_t * cy_priv,  struct urb *urb, int port_num)
{
    hci_t * hci = cy_priv->hci;
    td_t * td;
    int sie_num = port_num / 2;
    int len = 0;
    int current_len = 0;
    int toggle = 0;
    int maxps = usb_maxpacket (urb->dev, urb->pipe, usb_pipeout (urb->pipe));
    int endpoint = usb_pipeendpoint (urb->pipe);
    int address = usb_pipedevice (urb->pipe);
    int slow = usb_pipeslow (urb->pipe);
    int out = usb_pipeout (urb->pipe);
    int pid = 0;
    int iso = 0;
    int tt = 0;  /* transfer type */
    int i;
    int frame_num;

    if (maxps == 0)
        maxps = 8;

    /* cy_dbg("sh_add_urb_to_TD \n"); */
    /* calculate len, toggle bit and add the transaction */
    switch (usb_pipetype (urb->pipe))
    {
    case PIPE_ISOCHRONOUS:
        pid = out ? HCD_PID_OUT : HCD_PID_IN;
        iso = 1;

        frame_num = cy67x00_get_next_frame_number (cy_priv, port_num);

        if (frame_num >= urb->start_frame)
            i = frame_num - urb->start_frame;
        else
            i = (frame_num + 2048) - urb->start_frame;

        len = urb->iso_frame_desc[i].length > maxps ? maxps :
              urb->iso_frame_desc[i].length;
        tt = TT_ISOCHRONOUS;

        /* Check for bandwidth */
        if( sh_check_frame_bw(hci, sie_num, len) &&
           (hci->sie[sie_num].last_td_addr + CY_TD_SIZE <
            hci->sie[sie_num].tdBaseAddr + SIE_TD_SIZE) &&
           (hci->sie[sie_num].last_buf_addr + len <
            hci->sie[sie_num].bufBaseAddr + SIE_TD_BUF_SIZE) )
        {
            /* claim SIE bandwidth */
            sh_claim_frame_bw(hci, sie_num, len);

            /* allocate the TD */
            /* Srikanth 8th Feb */
            /* td = (td_t *) kmalloc(sizeof (td_t), GFP_KERNEL); */ 
            td = (td_t *) kmalloc(sizeof (td_t), GFP_ATOMIC);

            cy_dbg("sh_add_urb_to_TD: %x \n", td);
            if (td == NULL)
            {
                cy_err("sh_add_urb_to_TD: unable to allocate TD\n");
                return 0;
            }

            td->last_td_flag = 0;

            /* set the external processor data pointer */
            td->sa_data_ptr = urb->transfer_buffer + urb->iso_frame_desc[i].offset;

            /* Fill the TD */
            fillTd(hci, td, &hci->sie[sie_num].last_td_addr,
                   &hci->sie[sie_num].last_buf_addr, address, endpoint, pid,
                   len, toggle, slow, tt, port_num);
            td->urb = urb;
            cy_dbg("sh_add_urb_to_TD: td->urb is 0x%x\n", urb);

            /* add the TD to the TD list */
            list_add_tail(&td->td_list, &hci->sie[sie_num].td_list_head);

            /* update the last_td_addr and last_buf_addr */
            hci->sie[sie_num].last_td_addr += CY_TD_SIZE;
            hci->sie[sie_num].last_buf_addr += len;
        }
        break;

    case PIPE_CONTROL:
        /*
         * Three different types of TDs need to be created each phase of control
         * pipe: SETUP, DATA, STATUS
         */

        tt = TT_CONTROL;

        /* Check for Setup Stage */
        if ((urb->interval == 0) || (urb->interval == SETUP_STAGE)) {
            /* Check for bandwidth */
            if( sh_check_frame_bw(hci, sie_num, 8) &&
               (hci->sie[sie_num].last_td_addr + CY_TD_SIZE <
                hci->sie[sie_num].tdBaseAddr + SIE_TD_SIZE) &&
               (hci->sie[sie_num].last_buf_addr + len <
                hci->sie[sie_num].bufBaseAddr + SIE_TD_BUF_SIZE) )
            {
                sh_claim_frame_bw(hci, sie_num, 8);

                /* allocate the TD */
                /* Srikanth 8th Feb */
                /* td = (td_t *) kmalloc(sizeof (td_t), GFP_KERNEL); */
                td = (td_t *) kmalloc(sizeof (td_t), GFP_ATOMIC);
                cy_dbg("SETUP_STAGE - sh_add_urb_to_TD: %x \n", td);
                if (td == NULL)
                {
                    cy_err("sh_add_urb_to_TD: unable to allocate TD\n");
                    return 0;
                }

                td->last_td_flag = 0;

                /* Setup packets are 8 bytes in length */
                len = 8;
                pid = HCD_PID_SETUP;
                td->sa_data_ptr = (char *) urb->setup_packet;
                toggle = 0;

                /* Fill the TD */
                fillTd(hci, td, &hci->sie[sie_num].last_td_addr,
                       &hci->sie[sie_num].last_buf_addr, address, endpoint, pid,
                       len, toggle, slow, tt, port_num);
                td->urb = urb;

                td->last_td_flag = SETUP_STAGE;
                urb->interval = SETUP_STAGE;

                /* add the TD to the TD list */
                list_add_tail(&td->td_list, &hci->sie[sie_num].td_list_head);

                /* update the last_td_addr */
                hci->sie[sie_num].last_td_addr += CY_TD_SIZE;
                hci->sie[sie_num].last_buf_addr += len;

                cy_dbg("sh_add_urb_to_TD: td->urb 0x%x, td 0x%x \n", td->urb, td);
            }
        }
        else if (urb->interval == DATA_STAGE) {
            if (urb->transfer_buffer_length != urb->actual_length)
            {
                cy_dbg("DATA_STAGE urb->transfer_buffer_length %x urb->actual_length %x \n", urb->transfer_buffer_length,urb->actual_length);
                current_len = urb->actual_length;
                toggle = (current_len & maxps) ? 0 : 1;
                while (urb->transfer_buffer_length - current_len)
                {
                    len = (urb->transfer_buffer_length - current_len) > maxps
                          ? maxps : urb->transfer_buffer_length - current_len;

                    /* Check for bandwidth */
                    if( sh_check_frame_bw(hci, sie_num, len) &&
                       (hci->sie[sie_num].last_td_addr + CY_TD_SIZE <
                        hci->sie[sie_num].tdBaseAddr + SIE_TD_SIZE) &&
                       (hci->sie[sie_num].last_buf_addr + len <
                        hci->sie[sie_num].bufBaseAddr + SIE_TD_BUF_SIZE) )
                    {
                        sh_claim_frame_bw(hci, sie_num, len);

                        /* allocate the TD */
                        /* Srikanth 8th Feb */
                        /* td = (td_t *) kmalloc(sizeof (td_t), GFP_KERNEL); */
                        td = (td_t *) kmalloc(sizeof (td_t), GFP_ATOMIC);
                        cy_dbg("sh_add_urb_to_TD: %x \n", td);
                        if (td == NULL)
                        {
                            cy_err("sh_add_urb_to_TD: unable to allocate TD\n");
                            return 0;
                        }

                        td->last_td_flag = 0;

                        pid = out ? HCD_PID_OUT : HCD_PID_IN;

                        /* set the external processor data pointer */
                        td->sa_data_ptr = urb->transfer_buffer + current_len;

                        /* Fill the TD */
                        fillTd(hci, td, &hci->sie[sie_num].last_td_addr,
                           &hci->sie[sie_num].last_buf_addr, address, endpoint,
                           pid, len, toggle, slow, tt, port_num);
                        td->urb = urb;
                        cy_dbg("sh_add_urb_to_TD: td->urb is 0x%x\n", urb);

                        td->last_td_flag = DATA_STAGE;

                        /* add the TD to the TD list */
                        list_add_tail(&td->td_list, &hci->sie[sie_num].td_list_head);

                        /* Clear td pointer */
                        td = NULL;

                        /* update the last_td_addr */
                        hci->sie[sie_num].last_td_addr += CY_TD_SIZE;
                        hci->sie[sie_num].last_buf_addr += len;
                        current_len += len;
                        toggle = !toggle;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        else if (urb->interval == STATUS_STAGE) {
            /* Check for bandwidth */
            if( sh_check_frame_bw(hci, sie_num, 1) &&
               (hci->sie[sie_num].last_td_addr + CY_TD_SIZE <
                hci->sie[sie_num].tdBaseAddr + SIE_TD_SIZE) &&
               (hci->sie[sie_num].last_buf_addr + len <
                hci->sie[sie_num].bufBaseAddr + SIE_TD_BUF_SIZE) )\
            {
                sh_claim_frame_bw(hci, sie_num, 1);

                /* allocate the TD */
                /* Srikanth 8th Feb */
                /* td = (td_t *) kmalloc(sizeof (td_t), GFP_KERNEL); */
                td = (td_t *) kmalloc(sizeof (td_t), GFP_ATOMIC);
                if (td == NULL)
                {
                    cy_err("sh_add_urb_to_TD: unable to allocate TD\n");
                    return 0;
                }

                td->last_td_flag = 0;

                len = 0;
                pid = !out ? HCD_PID_OUT : HCD_PID_IN;
                toggle = 1;
                usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe),
                usb_pipeout (urb->pipe), toggle);

                /* set the external processor data pointer: arbitrary pick it */
                td->sa_data_ptr = urb->setup_packet;

                /* Fill the TD */
                fillTd(hci, td, &hci->sie[sie_num].last_td_addr,
                       &hci->sie[sie_num].last_buf_addr, address, endpoint, pid,
                       len, toggle, slow, tt, port_num);
                td->urb = urb;
                cy_dbg("sh_add_urb_to_TD: td->urb is 0x%x\n", urb);

                td->last_td_flag = STATUS_STAGE;

                /* add the TD to the TD list */
                list_add_tail(&td->td_list, &hci->sie[sie_num].td_list_head);

                /* update the last_td_addr */
                hci->sie[sie_num].last_td_addr += CY_TD_SIZE;
                hci->sie[sie_num].last_buf_addr += len;
            }
        }

        break;

    case PIPE_BULK: /* BULK and BULK0 */
        /* Intentional fall through */
    case PIPE_INTERRUPT:
        current_len = urb->actual_length;
        toggle = usb_gettoggle (urb->dev, endpoint, out);
        pid = out ? HCD_PID_OUT : HCD_PID_IN;
        if (usb_pipetype(urb->pipe) == PIPE_BULK)
        {
            tt = TT_BULK;
        }
        else
        {
            tt = TT_INTERRUPT;
        }
        while ((urb->transfer_buffer_length - current_len) > 0)
        {
            len = ((urb->transfer_buffer_length - current_len) > maxps) ?
                maxps : (urb->transfer_buffer_length - current_len);

            if( sh_check_frame_bw(hci, sie_num, len) &&
               (hci->sie[sie_num].last_td_addr + CY_TD_SIZE <
                hci->sie[sie_num].tdBaseAddr + SIE_TD_SIZE) &&
               (hci->sie[sie_num].last_buf_addr + len <
                hci->sie[sie_num].bufBaseAddr + SIE_TD_BUF_SIZE) )
            {
                /* claim SIE bandwidth */
                sh_claim_frame_bw(hci, sie_num, len);

                /* allocate the TD */
                /* Srikanth 8th Feb */
                /* td = (td_t *) kmalloc(sizeof (td_t), GFP_KERNEL); */

                td = (td_t *) kmalloc(sizeof (td_t), GFP_ATOMIC);
                cy_dbg("sh_add_urb_to_TD: %x \n", td);
                if (td == NULL)
                {
                    cy_err("sh_add_urb_to_TD: unable to allocate TD\n");
                    return 0;
                }

                td->last_td_flag = 0;

                /* set the external processor data pointer */
                td->sa_data_ptr = urb->transfer_buffer + current_len;

                /* Fill the TD */
                fillTd(hci, td, &hci->sie[sie_num].last_td_addr,
                   &hci->sie[sie_num].last_buf_addr, address, endpoint, pid,
                   len, toggle, slow, tt, port_num);
                td->urb = urb;
                cy_dbg("sh_add_urb_to_TD: td->urb is 0x%x\n", urb);

                /* add the TD to the TD list */
                list_add_tail(&td->td_list, &hci->sie[sie_num].td_list_head);

                /* Clear td pointer */
                td = NULL;

                /* update the last_td_addr */
                hci->sie[sie_num].last_td_addr += CY_TD_SIZE;
                hci->sie[sie_num].last_buf_addr += len;
                current_len+= len;
                toggle = !toggle;
            }
            else
            {
                break;
            }
        }
    }

    return 1;
}

/***************************************************************************
 * Function Name : sh_execute_td
 *
 * This function copies the TD and its associated data into cy7c67200/300.
 *
 * Input: cy_priv = data structure for the host controller
 *        td  = pointer to a TD
 *
 * Return: 1 = successful; bandwidth still available to add more urb
 *         0 = unsuccessful; no more bandwidth
 **************************************************************************/
int sh_execute_td(cy_priv_t * cy_priv, td_t * td, int td_addr, int sie_num)
{
    char * pdata = td->sa_data_ptr;
    int len = td->port_length & TD_PORTLENMASK_DL;
    unsigned short *data = td->sa_data_ptr;
    int i;
    spinlock_t lock;


    /* copy the data to cy7c67200/300 */
    /* Note:  Do not write data for a read */
    if ((td->pid_ep & TD_PIDEPMASK_PID) != TD_PID_IN)
    {
       lcd_write_memory(td->ly_base_addr, len, pdata, cy_priv);
    }


    /* cy_dbg("sh_execute_td \n"); */
    /* copy the TD to cy7c67200/300 */

    lcd_write_memory_big(td_addr, CY_TD_SIZE, (char *) td, cy_priv);



    return 0;
}

int sh_process_frame(cy_priv_t * cy_priv, int sie_num)
{
    hci_t * hci = cy_priv->hci;
    td_t * td;
    int td_addr = 0;
    struct list_head * lh = &hci->sie[sie_num].td_list_head;
    unsigned short tempdata;


    lh = lh->next;
    if (list_empty(lh))
    {
        hci->sie[sie_num].td_active = FALSE;

        /* Empty List: nothing to do, exit */

        return 0;
    }

    td_addr = hci->sie[sie_num].tdBaseAddr;

    /* Check for wrap around condition of the doubly linked list */
    while (lh != &hci->sie[sie_num].td_list_head)
    {
        /* cy_dbg("sh_process_frame lh is 0X%x , lh->next is 0X%x\n", 
                        lh,lh->next); */
        td = list_entry(lh, td_t, td_list);
        /* cy_dbg("sh_process_frame td is 0X%x lh is 0X%x, lh->next is  0x%x\n",
		 td, lh, lh->next); */

        if (td== NULL)
        {
            cy_dbg("sh_process_frame NULL TD \n");
            return 0;
        }

        if (lh->next == &hci->sie[sie_num].td_list_head)
        {
            /* This is the last TD in the TD list */
            /* so put the td.next_td_addr to be null to indicate the last td */

            td->next_td_addr = 0;
            /* cy_dbg("sh_process_frame next_td_addr is NULL \n"); */

        }

        /* copy data and TD to ASIC */
        sh_execute_td(cy_priv, td, td_addr, sie_num);


        /* get the next td address */
        td_addr = td->next_td_addr;
        cy_dbg("sh_process_frame next_td_addr is 0X%x \n", td->next_td_addr);

        /* get the next TD */
        lh = lh->next;
    }


    hci->sie[sie_num].td_active = TRUE;

    /* set the TD pointer in the ASIC */
    if (sie_num == SIE1)
    {

        lcd_write_reg(HUSB_SIE1_pCurrentTDPtr, hci->sie[sie_num].tdBaseAddr, cy_priv);

    }
    else
    {
        lcd_write_reg(HUSB_SIE2_pCurrentTDPtr, hci->sie[sie_num].tdBaseAddr,  cy_priv);
    }

    return (1);
}


/***************************************************************************
 * Function Name : sh_scan_urb_list
 *
 * This function goes through the urb list and schedule the
 * the transaction.
 *
 * Input: hci = data structure for the host controller
 *        list_lh = pointer to the isochronous list
 *
 * Return: 0 = unsuccessful; 1 = successful
 **************************************************************************/

static int sh_scan_urb_list (cy_priv_t * cy_priv, struct list_head * list_lh,
int port_num)
{
    struct list_head * lh = list_lh->next;
    struct urb * urb;
    int frame_num = 0;
    int index = 0;

    if (list_lh == NULL)
    {
        cy_err("sh_scan_urb_list: error, list_lh == NULL\n");
        return 1;
    }

    while (lh != list_lh)
    {

        if (lh == NULL)
        {
            cy_err("sh_scan_urb_list: error, lh == NULL\n");
            return 1;
        }

        urb = list_entry (lh, struct urb, urb_list);

        if (lh == lh->next)
        {
            cy_err("sh_scan_urb_list: error, loop found!\n");
            return 1;
        }

        lh = lh->next;
        if (urb == NULL)
        {
            return 1;
        }

        frame_num = cy67x00_get_next_frame_number(cy_priv, port_num);

        if (frame_num >= urb->start_frame)
        {
            index = frame_num - urb->start_frame;
        }
        else
        {
            index = (frame_num + 2048) - urb->start_frame;
        }

        switch (usb_pipetype (urb->pipe))
        {

        case PIPE_ISOCHRONOUS:
            if (index < urb->number_of_packets)
            {
                /* add the urb to to the frame list */
                if (!sh_add_urb_to_TD (cy_priv, urb, port_num))
                {
                    return 0;
                }
            }
            break;

        case PIPE_INTERRUPT:
            if (index >= urb->interval)
            {
                /* add the urb to to the frame list */
                if (!sh_add_urb_to_TD (cy_priv, urb, port_num))
                {
                    return 0;
                }
                else
                {
                    /* Successful added to the TD,
                     * now update the urb->start_frame */
                    urb->start_frame = cy67x00_get_current_frame_number(cy_priv,
                    port_num);
                }
            }
            break;
        case PIPE_CONTROL:
        case PIPE_BULK:

            /* add the urb to to the frame list */
            if (!sh_add_urb_to_TD (cy_priv, urb, port_num))
            {
                return 0;
            }
            break;
        default:
            cy_err("sh_scan_urb_list: unknown pipe %d\n",
            usb_pipetype(urb->pipe));
            return 0;
        }
    }

    return 1;
}

/* intialize the active TD list */

static void sh_init_active_td(cy_priv_t * cy_priv, int sie_num)
{
    hci_t * hci = cy_priv->hci;
    struct list_head * td_lh = &hci->sie[sie_num].td_list_head;
    td_t * td;
    struct list_head *td_lh_temp;

    /* cy_dbg("sh_init_active_td"); */

    /* Initialize the TD bandwitdh */

    sh_init_frame_bw (hci, sie_num);

    /* initialize the link list */

    if (!list_empty(&hci->sie[sie_num].td_list_head))
    {
        /* Remove any remaining td's */
        td_lh = td_lh->next;
        while ((td_lh != &hci->sie[sie_num].td_list_head) && (td_lh != NULL))
        {
            cy_dbg("td_lh %x, td_lh->next is %x, td_lh->prev is %x \n ", td_lh, td_lh->next,td_lh->prev);
            td = list_entry(td_lh, td_t, td_list);
            cy_dbg(" Deleting td %x \n", td);

            td_lh_temp = td_lh->next;
            list_del(&td->td_list);
            td_lh = td_lh_temp;
            cy_dbg("td_lh %x, td_lh->next is %x \n ", td_lh, td_lh->next);
            /* cy_dbg("td %x \n" td); */
            /* cy_dbg("td_list_head %x \n", &hci->sie[sie_num].td_list_head); */
            kfree(td);

        }
        /* cy_dbg("sh_init_active_td %x \n", td_lh); */
    }


    if (sie_num == 0)
    {
        hci->sie[sie_num].tdBaseAddr = cy_priv->cy_buf_addr + SIE1_TD_OFFSET;
        hci->sie[sie_num].bufBaseAddr = cy_priv->cy_buf_addr + SIE1_BUF_OFFSET;
    }
    else
    {
        hci->sie[sie_num].tdBaseAddr = cy_priv->cy_buf_addr + SIE2_TD_OFFSET;
        hci->sie[sie_num].bufBaseAddr = cy_priv->cy_buf_addr + SIE2_BUF_OFFSET;
    }

    /* initalize the addr beginning of the active TD list */
    hci->sie[sie_num].last_td_addr = hci->sie[sie_num].tdBaseAddr;
    hci->sie[sie_num].last_buf_addr = hci->sie[sie_num].bufBaseAddr;

}

/***************************************************************************
 * Function Name : sh_shedule_trans
 *
 * This function schedule the USB transaction.
 * This function will process the endpoint in the following order:
 * interrupt, control, and bulk.
 *
 * Input: hci = data structure for the host controller
 *        isSOF = flag indicate if Start Of Frame has occurred
 *
 * Return: 0
 **************************************************************************/

int sh_schedule_trans (cy_priv_t * cy_priv, int sie_num)
{
    hci_t * hci = cy_priv->hci;
    int units_left = 1;
    struct list_head * lh = NULL;
    int port_a = 2 * sie_num;
    int port_b = (2 * sie_num) + 1;
    sie_t * sie = &hci->sie[sie_num];

    /* cy_dbg("sh_schedule_trans"); */
    if (hci == NULL)
    {
        cy_err("sh_schedule_trans: hci == NULL\n");
        return 0;
    }

    /* Check for active td list */
    if (sie->td_active == TRUE)
    {
        /* TDs in-progress */
        return 0;
    }

    /* initalize the active TD */
    /* srikanth 25th Jan */
    sh_init_active_td(cy_priv, sie_num);

    /* the first port of the SIE */

    /* schedule the next available isochronous transfer or the next
     * stage of the interrupt transfer */

    if (hci->valid_host_port[port_a] == 1) {
        if (!list_empty(&hci->port[port_a].iso_list) &&
            (hci->port[port_a].RHportStatus->portStatus & PORT_CONNECT_STAT))
        {
            units_left = sh_scan_urb_list(cy_priv, &hci->port[port_a].iso_list,
            port_a);
        }
    }
    if (hci->valid_host_port[port_b] == 1) {
        if (!list_empty(&hci->port[port_b].iso_list) && (units_left > 0) &&
            (hci->port[port_b].RHportStatus->portStatus & PORT_CONNECT_STAT))
        {
            units_left = sh_scan_urb_list(cy_priv, &hci->port[port_b].iso_list,
            port_b);
        }
    }

    /* schedule the next available interrupt transfer or the next
     * stage of the interrupt transfer */

    if (hci->valid_host_port[port_a] == 1) {
        if (!list_empty(&hci->port[port_a].intr_list) && (units_left > 0) &&
            (hci->port[port_a].RHportStatus->portStatus & PORT_CONNECT_STAT))
        {
            units_left = sh_scan_urb_list (cy_priv,
            &hci->port[port_a].intr_list, port_a);
        }
    }

    if (hci->valid_host_port[port_b] == 1) {
        if (!list_empty(&hci->port[port_b].intr_list) && (units_left > 0) &&
            (hci->port[port_b].RHportStatus->portStatus & PORT_CONNECT_STAT))
        {
            units_left = sh_scan_urb_list (cy_priv,
            &hci->port[port_b].intr_list, port_b);
        }
    }

    /* schedule the next available control transfer or the next
     * stage of the control transfer */

    if (hci->valid_host_port[port_a] == 1) {
        if (!list_empty(&hci->port[port_a].ctrl_list) && (units_left > 0) &&
            (hci->port[port_a].RHportStatus->portStatus & PORT_CONNECT_STAT))
        {
            units_left = sh_scan_urb_list (cy_priv,
            &hci->port[port_a].ctrl_list, port_a);
        }
    }

    if (hci->valid_host_port[port_b] == 1) {
        if (!list_empty(&hci->port[port_b].ctrl_list) && (units_left > 0) &&
            (hci->port[port_b].RHportStatus->portStatus & PORT_CONNECT_STAT))
        {
            units_left = sh_scan_urb_list (cy_priv,
            &hci->port[port_b].ctrl_list, port_b);
        }
    }

    /* schedule the next available bulk transfer or the next
     * stage of the bulk transfer */
    if (hci->valid_host_port[port_a] == 1) {
        if (!list_empty(&hci->port[port_a].bulk_list) && (units_left > 0) &&
            (hci->port[port_a].RHportStatus->portStatus & PORT_CONNECT_STAT))
        {
            units_left = sh_scan_urb_list (cy_priv,
            &hci->port[port_a].bulk_list, port_a);

            /* be fair to each BULK URB (move list head around)
             * only when the new SOF happens */

            lh = hci->port[port_a].bulk_list.next;
            list_del (&hci->port[port_a].bulk_list);
            list_add (&hci->port[port_a].bulk_list, lh);
        }
    }

    if (hci->valid_host_port[port_b] == 1) {
        if (!list_empty(&hci->port[port_b].bulk_list) && (units_left > 0) &&
            (hci->port[port_b].RHportStatus->portStatus & PORT_CONNECT_STAT))
        {
            units_left = sh_scan_urb_list (cy_priv,
            &hci->port[port_b].bulk_list, port_b);

            /* be fair to each BULK URB (move list head around)
             * only when the new SOF happens */

            lh = hci->port[port_b].bulk_list.next;
            list_del (&hci->port[port_b].bulk_list);
            list_add (&hci->port[port_b].bulk_list, lh);
        }
    }

    /* process the frame */
    sh_process_frame(cy_priv, sie_num);

    return 0;
}


/***************************************************************************
 * Function Name : cc_to_error
 *
 * This function maps the CY hardware error code to the linux USB error
 * code.
 *
 * Input: cc = hardware error code
 *
 * Return: USB error code
 **************************************************************************/

int cc_to_error (int cc)
{
    int errCode = 0;

    if (cc & ERROR_FLG)
    {
        errCode |= USB_ST_CRC;
    }
    else if (cc & LENGTH_EXCEPT_FLG)
    {
        errCode |= USB_ST_DATAOVERRUN;
    }
    else if (cc & STALL_FLG)
    {
        errCode |= USB_ST_STALL;
    }
    else if (cc & TIMEOUT_FLG)
    {
        errCode |= USB_ST_TIMEOUT;
    }

    return errCode;
}


/***************************************************************************
 * Function Name : td_clear_urb
 *
 * This function clears the urb field on all the TDs that match the urb.
 *
 * Return: NA
 **************************************************************************/

void td_clear_urb(cy_priv_t * cy_priv, struct urb * urb, int sie_num)
{
    hci_t * hci = cy_priv->hci;
    td_t * td;
    struct list_head * td_lh = &hci->sie[sie_num].td_list_head;

    cy_dbg("td_clear_urb \n");
    if (!list_empty(&hci->sie[sie_num].td_list_head))
    {
        /* Clear the td's */
        td_lh = td_lh->next;
        while (td_lh != &hci->sie[sie_num].td_list_head)
        {
            td = list_entry(td_lh, td_t, td_list);
            if (td->urb == urb)
            {
                /* Clear the urb field */
                td->urb = NULL;
            }

            td_lh = td_lh->next;
        }
    }
}


/************************************************************************
 * Function Name : sh_parse_td
 *
 * This function checks the status of the transmitted or received packet
 * and copy the data from the CY7C67200_300 register into a buffer.
 *
 * 1) Check the status of the packet
 *
 * Input:  cy_priv = private data structure for the host controller
 *         td = pointer to the current td
 *         td_addr = address of the td on the CY7C67200_300
 *         actbytes = pointer to actual number of bytes
 *         cc = packet status
 *         activeFlag = the state of the current td
 *         toggle = data toggle
 *         pipetype = type of pipe
 *
 * Return: 0
 ***********************************************************************/
int sh_parse_td (cy_priv_t * cy_priv, td_t * td, int td_addr, int * actbytes,
                               int *cc, int * activeFlag, int * toggle, int
                               pipetype)
{
    int transfer_status;
    char residue;
    int pid = td->pid_ep & TD_PIDEPMASK_PID;
    int len = td->port_length & TD_PORTLENMASK_DL;
    int retryCnt = 0;

    /* read the TD from cy7c67200/300 */
    /* cy_dbg("sh_parse_td \n"); */
    lcd_read_memory(td_addr, CY_TD_SIZE, (char *) td, cy_priv);

    retryCnt = td->retry_cnt & TD_RETRYCNTMASK_RTY_CNT;
    residue = td->residue;
    *activeFlag = td->retry_cnt & TD_RETRYCNTMASK_ACT_FLG;
    *cc = td->status;

    /* cy_dbg("The residue is 0x%x, active flag is  0x%x, status is 0x%x \n", 
		td->residue, *activeFlag, td->status); */

    /* cy_dbg("The base_addr is 0x%x, port_length 0x%x, pid_ep 0x%x,  \
		dev_addr 0x%x\n", td->ly_base_addr, td->port_length, 
		td->pid_ep, td->dev_addr); */
    /* cy_dbg("The td ctrl_reg is 0x%x,td status is 0x%x,next_td_addr is 0x%x\n",
   		td->ctrl_reg,td->status,td->next_td_addr); */


    /* Check the Active Flag */
    if ((*activeFlag == 0) ||
        (pipetype == PIPE_ISOCHRONOUS) ||
        (pipetype == PIPE_INTERRUPT))
    {
        /* check the status */
        transfer_status = td->status;
        /* cy_dbg("The td status is 0x%x \n", td->status); */

        if ((transfer_status == 0) ||
           ((transfer_status & TD_STATUSMASK_OVF) &&
           (pid == TD_PID_IN) && ((residue & 0x80) == 0)) ||
           ((transfer_status & TD_STATUSMASK_ACK) &&
           ((pipetype == PIPE_ISOCHRONOUS)||(pipetype == PIPE_INTERRUPT)))
           )
        {
            /* transfer successful */
            *toggle = !*toggle;
            if (pid == TD_PID_OUT)
            {
                /* USB OUT */
                *actbytes = len;
            }
            else
            {
                /* USB IN */
                /* NOTE: Residue = expected byte count - actual byte count,
                 * therefore: actual byte count = expected byte count - Residue
                 */
                *actbytes = len - residue;

                cy_dbg("The td actbytes = 0x%x, len = 0X%x \n", *actbytes, len);
            }
        }
        else
        {
            /* cy_dbg("The td status is 0X%x ", td->status); */

            /* transfer failed */
            return td->status;
        }
    }
    else   /* Active flag is set */
    {
        *actbytes = 0;
       /* don't do anything, it will take care of it in the next frame */
    }

    return 0;
}


/***************************************************************************
 * Function Name : sh_td_list_check_stat
 *
 * This function does the following
 *
 * 1) Gets a TD list from cy7c67200/300
 * 2) handles hardware error
 * 3) If no errors occur on the TD, then move the TD to the done list
 *    to process later.
 *
 * Input: hci = data structure for the host controller
 *        sie_num = sie number
 *
 * Return:  urb_state or -1 if the transaction has complete
 **************************************************************************/

void sh_td_list_check_stat (cy_priv_t * cy_priv, int sie_num)
{
    hci_t * hci = cy_priv->hci;
    int actbytes = 0;
    int active_flag = 0;
    int maxps = 0;
    int toggle;
    struct urb * urb;
    int iso_index = 0;
    int out;
    td_t * td;
    int transfer_status;
    int port_num = -1;
    struct list_head * td_lh = &hci->sie[sie_num].td_list_head;
    struct list_head *td_lh_temp;
    unsigned short td_addr;
    int residue = 0;
    int pipetype;
    int frame_num;

    /* cy_dbg("enter sh_td_list_check_stat\n"); */

    if (cy_priv == NULL || hci == NULL)
    {
        cy_err("sh_td_list_check_stat error!: cy_priv = 0x%x, hci = 0x%x\n",
                (int) cy_priv, (int) hci);
        return;
    }


    td_lh = &hci->sie[sie_num].td_list_head;
    td_lh = td_lh->next;
    td_addr = hci->sie[sie_num].tdBaseAddr;
    while ((td_lh != &hci->sie[sie_num].td_list_head) )
    {
        if (td_lh == NULL)
        {
            return;
        }

        /* Get the TD from the list */
        td = list_entry(td_lh, td_t, td_list);
        /* cy_dbg("enter sh_td_list_check_stat td 0X%x, td_lh is 0X%x  \
		td_lh->next is 0X%x \n", td, td_lh, td_lh->next); */
        if (td == NULL)
        {
            cy_dbg("enter sh_td_list_check_stat td is NULL \n", td);
            return;
        }

        /* Get the urb associated with the TD */
        urb = td->urb;
        if (urb == NULL)
        {
            /* Remove the td from the list */
            td_lh_temp = td_lh->next;
            list_del(&td->td_list);

            td_addr += CY_TD_SIZE;
            td_lh = td_lh_temp;

            cy_dbg("enter sh_td_list_check_stat URB is NULL \n");
            kfree(td);
            continue;
        }

        port_num = get_port_num_from_urb(cy_priv, urb);
        if (port_num == ERROR)
        {
            cy_err("sh_td_list_check_stat: can't find port num\n");

            /* Remove the td from the list */
            td_lh_temp = td_lh->next;
            list_del(&td->td_list);

            td_addr += CY_TD_SIZE;
            td_lh = td_lh_temp;

            kfree(td);
            continue;
        }

        out = usb_pipeout(urb->pipe);
        pipetype = usb_pipetype(urb->pipe);
        maxps = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));
        if (maxps == 0)
        {
            maxps = 8;
        }

        if (usb_pipeisoc (urb->pipe))
        {
            frame_num = cy67x00_get_current_frame_number (cy_priv, port_num);

            if (frame_num >= urb->start_frame)
                iso_index = frame_num - urb->start_frame;
            else
                iso_index = (frame_num + 2048) - urb->start_frame;

            toggle = 0;
        }
        else
        {
            toggle = usb_gettoggle (urb->dev, usb_pipeendpoint (urb->pipe),
                                    usb_pipeout (urb->pipe));
        }

        /* parse the TD */
        sh_parse_td (cy_priv, td, td_addr, &actbytes, &transfer_status,
                     &active_flag, &toggle, pipetype);

        residue = td->residue;

        /* Check the Active Flag */

        if ((active_flag == 0) ||
            (pipetype == PIPE_ISOCHRONOUS) ||
            (pipetype == PIPE_INTERRUPT))
        {
            if ((transfer_status == 0) ||
               ((transfer_status & TD_STATUSMASK_OVF) &&
               (usb_pipein(urb->pipe)) && ((residue & 0x80) == 0)) ||
               ((transfer_status & TD_STATUSMASK_ACK) &&
               ((pipetype == PIPE_ISOCHRONOUS) ||
                (pipetype == PIPE_INTERRUPT)))
               )
            {
                /* transfer successful */

                switch (pipetype)
                {
                case PIPE_ISOCHRONOUS:
                    /* NEED to check for ISOC in or out */

                    if (usb_pipein(urb->pipe))
                    {
                        /* USB IN URB */
                        /* The data copy phase will take care of copying the IN
                         * data to the transfer buffer.
                         * The data copy phase uses the Done list, therefore we
                         * add the TD to the done list.
                         * Also, after the data copy phase is done, it will
                         * send it to the upper protocol */

                        /* The done list does not work, because the schedule is
                           done immediately after this check routine.  The urb is
                           still present and gets sent out again, even though it may
                           be satified on this response.
                         */

                        if (iso_index < urb->number_of_packets)
                            {
                            urb->iso_frame_desc [iso_index].actual_length =  actbytes;
                            urb->iso_frame_desc [iso_index].status = cc_to_error(transfer_status);

                            if (actbytes > 0) {
                                cy_dbg("enter sh_td_list_check_stat \n");

/* Srikanth 17th Jan */
                                lcd_read_memory_little(td->ly_base_addr,
                                                        actbytes,
                                                urb->transfer_buffer + urb->
                                                iso_frame_desc[iso_index].offset,
                                                cy_priv);

                                }
                            }

                        if ((iso_index + 1) >= urb->number_of_packets)
                            {
                                /* All ISOC packets for this URB have been
                                received */
                                urb->status = cc_to_error(transfer_status);

                                /* send the urb back to the upper protocol */
                                qu_return_urb(cy_priv, urb, port_num, 0);
                            }
                    }
                    else
                    {
                        if (iso_index < urb->number_of_packets)
                            {
                            urb->iso_frame_desc [iso_index].actual_length = actbytes;
                            urb->iso_frame_desc [iso_index].status = cc_to_error (transfer_status);
                            }

                        if ((iso_index + 1) >= urb->number_of_packets)
                            {
                                /* All ISOC packets for this URB have been
                                transmitted */
                                urb->status = cc_to_error(transfer_status);

                                /* send the urb back to the upper protocol */
                                qu_return_urb(cy_priv, urb, port_num, 0);
                            }
                    }
                break;

                case PIPE_CONTROL:
                    if (td->last_td_flag == SETUP_STAGE)
                    {

                        cy_dbg("enter sh_td_list_check_stat SETUP_STAGE  \n");
                        /* Setup OK move to next stage */
                        if (urb->transfer_buffer_length == urb->actual_length)
                        {
                            urb->interval = STATUS_STAGE;
                        }
                        else
                        {
                            urb->interval = DATA_STAGE;
                        }
                    }

                    if (td->last_td_flag == DATA_STAGE)
                    {
                        if (usb_pipein(urb->pipe))
                        {
                            /* copy data from the ASIC */
                            if (actbytes > 0) {
                                lcd_read_memory_little(td->ly_base_addr, actbytes,
                                urb->transfer_buffer + urb->actual_length, cy_priv);
                            }
                        }

                        cy_dbg(" DATA STAGE actbytes %x, transfer_buffer_length %x, actual length %x \n", actbytes,urb->transfer_buffer_length,urb->actual_length);

                        /* Check for short packet */
                        if ((actbytes == 0) ||
                           (actbytes < maxps) ||
                           ((urb->actual_length + actbytes) >= urb->transfer_buffer_length)) {
                            cy_dbg("DATA_STAGE - Short Packet \n");
                            urb->interval = STATUS_STAGE;
                            td_clear_urb(cy_priv, urb, sie_num);

                        }

                        usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe),
                                       usb_pipeout (urb->pipe),toggle);
                        urb->actual_length += actbytes;
                    }

                    if (td->last_td_flag == STATUS_STAGE)
                    {
                        cy_dbg("enter sh_td_list_check_stat STATUS_STAGE  \n");
                        urb->interval = 0;
                        urb->status = cc_to_error(transfer_status);
                        qu_return_urb(cy_priv, urb, port_num, 0);
                    }
                break;

                case PIPE_INTERRUPT:
                case PIPE_BULK:
                    if (usb_pipein(urb->pipe))
                    {
                        usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe),
                               usb_pipeout (urb->pipe),toggle);

                        /* USB IN URB */
                        if (actbytes > 0) {
                            cy_dbg("enter sh_td_list_check_stat \n");
                            lcd_read_memory_little(td->ly_base_addr, actbytes,
                                            urb->transfer_buffer + urb->actual_length,
                                            cy_priv);

                            urb->actual_length += actbytes;
                        }


                        /* Check for short packet */
                        if ((actbytes == 0) ||
                           (actbytes < maxps) ||
                           (urb->actual_length >= urb->transfer_buffer_length))
                        {
                            urb->status = cc_to_error(transfer_status & ~ TD_STATUSMASK_OVF);
                            qu_return_urb(cy_priv, urb, port_num, 1);
                            td_clear_urb(cy_priv, urb, sie_num);
                        }
                    }
                    else
                    {
                        urb->actual_length += actbytes;
                        usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe),
                                       usb_pipeout (urb->pipe),toggle);
                        if ((urb->transfer_buffer_length == urb->actual_length) ||
                            !actbytes || (actbytes & (maxps-1)))
                        {
                            urb->status = cc_to_error(transfer_status);
                            qu_return_urb(cy_priv, urb, port_num, 1);
                        }
                    }
                }
            }
            else
            {
                if (transfer_status & TD_STATUSMASK_STALL) {
                    urb->status = cc_to_error(transfer_status);
                    qu_return_urb(cy_priv, urb, port_num, 0);
                    td_clear_urb(cy_priv, urb, sie_num);
                }
                else
                {
                    if ((pipetype != PIPE_ISOCHRONOUS) &&
                        (pipetype != PIPE_INTERRUPT)) {
                        /* serious error occurred  during transfer */
                        if (++urb->error_count > 3)
                        {
                            td->last_td_flag = 1;
                            urb->status = cc_to_error(transfer_status);
                            qu_return_urb(cy_priv, urb, port_num, 0);
                            td_clear_urb(cy_priv, urb, sie_num);
                        }
                    }
                }
            }
        }
        else
        {
            switch (usb_pipetype(urb->pipe))
            {
                /* If Isoc/Int transfer, need to handle the error */
                case PIPE_ISOCHRONOUS:
                case PIPE_INTERRUPT:
                /* if ctrl/bulk transfer, need to retry ?? */
                case PIPE_CONTROL:
                case PIPE_BULK:
                default:
                break;
            }
        }

        /* Remove the td from the list */
        /* cy_dbg("enter sh_td_list_check_stat td is 0X%x, td_lh->next 0X%x, \
		td->list 0X%x\n", td, td_lh->next, td->td_list); */

        td_lh_temp = td_lh->next;
        list_del(&td->td_list);
        td_addr += CY_TD_SIZE;
        td_lh = td_lh_temp;
        /* cy_dbg("enter sh_td_list_check_stat td_lh 0X%x  \
		td_lh->next is 0X%x\n", td_lh,td_lh->next); */
        /* cy_dbg("enter sh_td_list_check_stat list head 0X%x \n",
		 &hci->sie[sie_num].td_list_head); */


       kfree(td);
    }
}


