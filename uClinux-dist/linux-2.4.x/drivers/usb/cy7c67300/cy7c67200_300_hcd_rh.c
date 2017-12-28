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

/*
 * based on usb-ohci.c by R. Weissgaerber et al.
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

/* External */
extern int get_port_num_from_urb(cy_priv_t * cy_priv, struct urb * urb);
extern __u32 get_port_stat_change(hci_t *hci, int port_num);
extern void set_port_stat(hci_t *hci, __u16 bitPos, int port_num);
extern void set_port_change(hci_t *hci, __u16 bitPos, int port_num);
extern void clr_port_stat(hci_t *hci, __u16 bitPos, int port_num);
extern void clr_port_change(hci_t *hci, __u16 bitPos, int port_num);
extern int cc_to_error (int cc);

/* Local */
static int rh_init_int_timer (struct urb * urb, int port_num);

/*-------------------------------------------------------------------------*
 * Virtual Root Hub
 *-------------------------------------------------------------------------*/

/* Device descriptor */
static __u8 root_hub_dev_des[4][18] =
{
    {
    0x12,       /*  __u8  bLength; */
    0x01,       /*  __u8  bDescriptorType; Device */
    0x10,       /*  __u16 bcdUSB; v1.1 */
    0x01,
    0x09,       /*  __u8  bDeviceClass; HUB_CLASSCODE */
    0x00,       /*  __u8  bDeviceSubClass; */
    0x00,       /*  __u8  bDeviceProtocol; */
    0x08,       /*  __u8  bMaxPacketSize0; 8 Bytes */
    0x00,       /*  __u16 idVendor; */
    0x00,
    0x00,       /*  __u16 idProduct; */
    0x00,
    0x00,       /*  __u16 bcdDevice; */
    0x00,
    0x00,       /*  __u8  iManufacturer; */
    0x02,       /*  __u8  iProduct; */
    0x01,       /*  __u8  iSerialNumber; */
    0x01        /*  __u8  bNumConfigurations; */
    },
    {
    0x12,       /*  __u8  bLength; */
    0x01,       /*  __u8  bDescriptorType; Device */
    0x10,       /*  __u16 bcdUSB; v1.1 */
    0x01,
    0x09,       /*  __u8  bDeviceClass; HUB_CLASSCODE */
    0x00,       /*  __u8  bDeviceSubClass; */
    0x00,       /*  __u8  bDeviceProtocol; */
    0x08,       /*  __u8  bMaxPacketSize0; 8 Bytes */
    0x00,       /*  __u16 idVendor; */
    0x00,
    0x00,       /*  __u16 idProduct; */
    0x00,
    0x00,       /*  __u16 bcdDevice; */
    0x00,
    0x00,       /*  __u8  iManufacturer; */
    0x02,       /*  __u8  iProduct; */
    0x01,       /*  __u8  iSerialNumber; */
    0x01        /*  __u8  bNumConfigurations; */
    },
    {
    0x12,       /*  __u8  bLength; */
    0x01,       /*  __u8  bDescriptorType; Device */
    0x10,       /*  __u16 bcdUSB; v1.1 */
    0x01,
    0x09,       /*  __u8  bDeviceClass; HUB_CLASSCODE */
    0x00,       /*  __u8  bDeviceSubClass; */
    0x00,       /*  __u8  bDeviceProtocol; */
    0x08,       /*  __u8  bMaxPacketSize0; 8 Bytes */
    0x00,       /*  __u16 idVendor; */
    0x00,
    0x00,       /*  __u16 idProduct; */
    0x00,
    0x00,       /*  __u16 bcdDevice; */
    0x00,
    0x00,       /*  __u8  iManufacturer; */
    0x02,       /*  __u8  iProduct; */
    0x01,       /*  __u8  iSerialNumber; */
    0x01        /*  __u8  bNumConfigurations; */
    },
    {
    0x12,       /*  __u8  bLength; */
    0x01,       /*  __u8  bDescriptorType; Device */
    0x10,       /*  __u16 bcdUSB; v1.1 */
    0x01,
    0x09,       /*  __u8  bDeviceClass; HUB_CLASSCODE */
    0x00,       /*  __u8  bDeviceSubClass; */
    0x00,       /*  __u8  bDeviceProtocol; */
    0x08,       /*  __u8  bMaxPacketSize0; 8 Bytes */
    0x00,       /*  __u16 idVendor; */
    0x00,
    0x00,       /*  __u16 idProduct; */
    0x00,
    0x00,       /*  __u16 bcdDevice; */
    0x00,
    0x00,       /*  __u8  iManufacturer; */
    0x02,       /*  __u8  iProduct; */
    0x01,       /*  __u8  iSerialNumber; */
    0x01        /*  __u8  bNumConfigurations; */
    }

};

/* Configuration descriptor */
static __u8 root_hub_config_des[4][25] =
{
    {
    0x09,       /*  __u8  bLength; */
    0x02,       /*  __u8  bDescriptorType; Configuration */
    0x19,       /*  __u16 wTotalLength; */
    0x00,
    0x01,       /*  __u8  bNumInterfaces; */
    0x01,       /*  __u8  bConfigurationValue; */
    0x00,       /*  __u8  iConfiguration; */
    0x40,       /*  __u8  bmAttributes;
                    Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup,
                    4..0: resvd */
    0x00,       /*  __u8  MaxPower; */

    /* interface */
    0x09,       /*  __u8  if_bLength; */
    0x04,       /*  __u8  if_bDescriptorType; Interface */
    0x00,       /*  __u8  if_bInterfaceNumber; */
    0x00,       /*  __u8  if_bAlternateSetting; */
    0x01,       /*  __u8  if_bNumEndpoints; */
    0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
    0x00,       /*  __u8  if_bInterfaceSubClass; */
    0x00,       /*  __u8  if_bInterfaceProtocol; */
    0x00,       /*  __u8  if_iInterface; */

    /* endpoint */
    0x07,       /*  __u8  ep_bLength; */
    0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
    0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
    0x03,       /*  __u8  ep_bmAttributes; Interrupt */
    0x02,       /*  __u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
    0x00,
    0xff        /*  __u8  ep_bInterval; 255 ms */
    },
    {
    0x09,       /*  __u8  bLength; */
    0x02,       /*  __u8  bDescriptorType; Configuration */
    0x19,       /*  __u16 wTotalLength; */
    0x00,
    0x01,       /*  __u8  bNumInterfaces; */
    0x01,       /*  __u8  bConfigurationValue; */
    0x00,       /*  __u8  iConfiguration; */
    0x40,       /*  __u8  bmAttributes;
                    Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup,
                    4..0: resvd */
    0x00,       /*  __u8  MaxPower; */

    /* interface */
    0x09,       /*  __u8  if_bLength; */
    0x04,       /*  __u8  if_bDescriptorType; Interface */
    0x00,       /*  __u8  if_bInterfaceNumber; */
    0x00,       /*  __u8  if_bAlternateSetting; */
    0x01,       /*  __u8  if_bNumEndpoints; */
    0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
    0x00,       /*  __u8  if_bInterfaceSubClass; */
    0x00,       /*  __u8  if_bInterfaceProtocol; */
    0x00,       /*  __u8  if_iInterface; */

    /* endpoint */
    0x07,       /*  __u8  ep_bLength; */
    0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
    0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
    0x03,       /*  __u8  ep_bmAttributes; Interrupt */
    0x02,       /*  __u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
    0x00,
    0xff        /*  __u8  ep_bInterval; 255 ms */
    },
    {
    0x09,       /*  __u8  bLength; */
    0x02,       /*  __u8  bDescriptorType; Configuration */
    0x19,       /*  __u16 wTotalLength; */
    0x00,
    0x01,       /*  __u8  bNumInterfaces; */
    0x01,       /*  __u8  bConfigurationValue; */
    0x00,       /*  __u8  iConfiguration; */
    0x40,       /*  __u8  bmAttributes;
                    Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup,
                    4..0: resvd */
    0x00,       /*  __u8  MaxPower; */

    /* interface */
    0x09,       /*  __u8  if_bLength; */
    0x04,       /*  __u8  if_bDescriptorType; Interface */
    0x00,       /*  __u8  if_bInterfaceNumber; */
    0x00,       /*  __u8  if_bAlternateSetting; */
    0x01,       /*  __u8  if_bNumEndpoints; */
    0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
    0x00,       /*  __u8  if_bInterfaceSubClass; */
    0x00,       /*  __u8  if_bInterfaceProtocol; */
    0x00,       /*  __u8  if_iInterface; */

    /* endpoint */
    0x07,       /*  __u8  ep_bLength; */
    0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
    0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
    0x03,       /*  __u8  ep_bmAttributes; Interrupt */
    0x02,       /*  __u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
    0x00,
    0xff        /*  __u8  ep_bInterval; 255 ms */
    },
    {
    0x09,       /*  __u8  bLength; */
    0x02,       /*  __u8  bDescriptorType; Configuration */
    0x19,       /*  __u16 wTotalLength; */
    0x00,
    0x01,       /*  __u8  bNumInterfaces; */
    0x01,       /*  __u8  bConfigurationValue; */
    0x00,       /*  __u8  iConfiguration; */
    0x40,       /*  __u8  bmAttributes;
                    Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup,
                    4..0: resvd */
    0x00,       /*  __u8  MaxPower; */

    /* interface */
    0x09,       /*  __u8  if_bLength; */
    0x04,       /*  __u8  if_bDescriptorType; Interface */
    0x00,       /*  __u8  if_bInterfaceNumber; */
    0x00,       /*  __u8  if_bAlternateSetting; */
    0x01,       /*  __u8  if_bNumEndpoints; */
    0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
    0x00,       /*  __u8  if_bInterfaceSubClass; */
    0x00,       /*  __u8  if_bInterfaceProtocol; */
    0x00,       /*  __u8  if_iInterface; */

    /* endpoint */
    0x07,       /*  __u8  ep_bLength; */
    0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
    0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
    0x03,       /*  __u8  ep_bmAttributes; Interrupt */
    0x02,       /*  __u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
    0x00,
    0xff        /*  __u8  ep_bInterval; 255 ms */
    }

};

/* Hub class-specific descriptor is constructed dynamically */


/***************************************************************************
 * Function Name : rh_send_irq
 *
 * This function examine the port change in the virtual root hub.
 *
 * Note: This function assumes only one port exist in the root hub.
 *
 * Input:  hci = data structure for the host controller
 *         rh_data = The pointer to port change data
 *         rh_len = length of the data in bytes
 *
 * Return: length of data
 **************************************************************************/

static int rh_send_irq (hci_t * hci, void * rh_data, int rh_len, int port_num)
{
    int num_ports;
    int i;
    int ret;
    int len;
    int temp;
    __u8 data[8];

    /* Assuming the root hub has one port.  This value need to change if
     * there are more than one port for the root hub
     */

    num_ports = 1;

    /* The root hub status is not implemented, it basically has two fields:
     *     -- Local Power Status
     *     -- Over Current Indicator
     *     -- Local Power Change
     *     -- Over Current Indicator
     *
     * Right now, It is assume the power is good and no changes
     */

    *(__u8 *) data = 0;

    ret = *(__u8 *) data;

    /* Has the port status change within the root hub: It checks for
     *      -- Port Connect Status change
     *      -- Port Enable Change
     */

    for ( i = 0; i < num_ports; i++)
    {
        *(__u8 *) (data + (i + 1) / 8) |= (((get_port_stat_change (hci, port_num) >> 16)
            & (PORT_CONNECT_STAT | PORT_ENABLE_STAT)) ? 1: 0) <<
                    ((i + 1) % 8);
        ret += *(__u8 *) (data + (i + 1) / 8);

        /* After the port change is read, it should be reset so the next time
         * is it doesn't trigger a change again */
    }
    len = i/8 + 1;

    if (ret > 0)
    {
        temp = min (rh_len, (int) sizeof(data));
        memcpy (rh_data, (void *) data, min (len, temp));
        return len;
    }

    return 0;
}

/***************************************************************************
 * Function Name : rh_int_timer_do
 *
 * This function is called when the timer expires.  It gets the the port
 * change data and pass along to the upper protocol.
 *
 * Note:  The virtual root hub interrupt pipe are polled by the timer
 *        every "interval" ms
 *
 * Input:  ptr = ptr to the urb
 *
 * Return: none
 **************************************************************************/

static void rh_int_timer_do (unsigned long ptr)
{
    int len;
    struct urb * urb = (struct urb *) ptr;
    int port_num = -1;
    cy_priv_t * cy_priv;
    hci_t * hci;

    if (urb == NULL)
    {
        cy_err("rh_int_timer_do: error, urb = NULL");
        return;
    }

    cy_priv = urb->dev->bus->hcpriv;
    if (cy_priv == NULL)
    {
        cy_err("rh_int_timer_do: error, cy_priv = NULL");
        return;
    }

    hci = cy_priv->hci;
    if (hci == NULL)
    {
        cy_err("rh_int_timer_do: error, hci = NULL");
        return;
    }

    port_num = get_port_num_from_urb(cy_priv, urb);

    if (port_num == ERROR)
    {
        cy_err("rh_int_timer_do: can't find port num\n");
        return;
    }

    if(hci->port[port_num].rh.send)
    {
        len = rh_send_irq (hci, urb->transfer_buffer,
                           urb->transfer_buffer_length, port_num);
        if (len > 0)
        {
            urb->actual_length = len;
            if (urb_debug == 2)
                urb_print (urb, "RET-t(rh)", usb_pipeout (urb->pipe));

            if (urb->complete)
            {
                urb->complete (urb);
            }
        }
    }

    /* re-activate the timer */
    rh_init_int_timer (urb, port_num);
}

/***************************************************************************
 * Function Name : rh_init_int_timer
 *
 * This function creates a timer that act as interrupt pipe in the
 * virtual hub.
 *
 * Note:  The virtual root hub's interrupt pipe are polled by the timer
 *        every "interval" ms
 *
 * Input: urb = USB request block
 *
 * Return: 0
 **************************************************************************/

static int rh_init_int_timer (struct urb * urb, int port_num)
{
    cy_priv_t * cy_priv =  urb->dev->bus->hcpriv;
    hci_t * hci = cy_priv->hci;
    hci->port[port_num].rh.interval = urb->interval;

    init_timer (&hci->port[port_num].rh.rh_int_timer);
    hci->port[port_num].rh.rh_int_timer.function = rh_int_timer_do;
    hci->port[port_num].rh.rh_int_timer.data = (unsigned long) urb;
    hci->port[port_num].rh.rh_int_timer.expires = jiffies +
               (HZ * (urb->interval < 30? 30: urb->interval)) / 1000;
    add_timer (&hci->port[port_num].rh.rh_int_timer);

    return 0;
}

/*-------------------------------------------------------------------------*/

/* helper macro */
#define OK(x)           len = (x); break

/***************************************************************************
 * Function Name : rh_submit_urb
 *
 * This function handles all USB request to the the virtual root hub
 *
 * Input: urb = USB request block
 *
 * Return: 0
 **************************************************************************/

int rh_submit_urb (struct urb * urb, int port_num)
{
    struct usb_device * usb_dev = urb->dev;
    cy_priv_t * cy_priv = usb_dev->bus->hcpriv;
    hci_t * hci = cy_priv->hci;
    unsigned int pipe = urb->pipe;
    struct usb_ctrlrequest * cmd = (struct usb_ctrlrequest *) urb->setup_packet;
    void * data = urb->transfer_buffer;
    int leni = urb->transfer_buffer_length;
    int len = 0;
    int status = TD_CC_NOERROR;
    int temp;
    __u32 datab[4];
    __u8  * data_buf = (__u8 *) datab;

    __u16 bmRType_bReq;
    __u16 wValue;
    __u16 wIndex;
    __u16 wLength;

    if (usb_pipeint(pipe))
    {
        hci->port[port_num].rh.urb =  urb;
        hci->port[port_num].rh.send = 1;
        hci->port[port_num].rh.interval = urb->interval;
        rh_init_int_timer(urb, port_num);
        urb->status = cc_to_error (TD_CC_NOERROR);

    return 0;
    }

    bmRType_bReq  = cmd->bRequestType | (cmd->bRequest << 8);
    wValue        = le16_to_cpu (cmd->wValue);
    wIndex        = le16_to_cpu (cmd->wIndex);
    wLength       = le16_to_cpu (cmd->wLength);

    switch (bmRType_bReq)
    {
    /* Request Destination:
       without flags: Device,
       RH_INTERFACE: interface,
       RH_ENDPOINT: endpoint,
       RH_CLASS means HUB here,
       RH_OTHER | RH_CLASS  almost ever means HUB_PORT here
    */

    case RH_GET_STATUS:
        *(__u16 *) data_buf = cpu_to_le16 (1); OK (2);

    case RH_GET_STATUS | RH_INTERFACE:
        *(__u16 *) data_buf = cpu_to_le16 (0); OK (2);


    case RH_GET_STATUS | RH_ENDPOINT:
        *(__u16 *) data_buf = cpu_to_le16 (0); OK (2);


    case RH_GET_STATUS | RH_CLASS:
        *(__u32 *) data_buf = cpu_to_le32 (0); OK (4);


    case RH_GET_STATUS | RH_OTHER | RH_CLASS:
        *(__u32 *) data_buf = cpu_to_le32 (get_port_stat_change(hci, port_num));
            OK (4);


    case RH_CLEAR_FEATURE | RH_ENDPOINT:
        switch (wValue)
        {
        case (RH_ENDPOINT_STALL):
        OK (0);
        }
        break;

    case RH_CLEAR_FEATURE | RH_CLASS:
        switch (wValue)
        {
        case RH_C_HUB_LOCAL_POWER:
            OK(0);

                case (RH_C_HUB_OVER_CURRENT):
            /* Over Current Not Implemented */
            OK (0);
        }
        break;

    case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
        switch (wValue)
        {
        case (RH_PORT_ENABLE):
            clr_port_stat (hci, PORT_ENABLE_STAT, port_num);
            OK (0);

        case (RH_PORT_SUSPEND):
            clr_port_stat (hci, PORT_SUSPEND_STAT, port_num);
            OK (0);

        case (RH_PORT_POWER):
            clr_port_stat (hci, PORT_POWER_STAT, port_num);
            OK (0);

        case (RH_C_PORT_CONNECTION):
            clr_port_change (hci, PORT_CONNECT_STAT, port_num);
            OK (0);

        case (RH_C_PORT_ENABLE):
            clr_port_change (hci, PORT_ENABLE_STAT, port_num);
            OK (0);

        case (RH_C_PORT_SUSPEND):
            clr_port_change (hci, PORT_SUSPEND_STAT, port_num);
            OK (0);

        case (RH_C_PORT_OVER_CURRENT):
            clr_port_change (hci, PORT_OVER_CURRENT_STAT, port_num);
            OK (0);

        case (RH_C_PORT_RESET):
            clr_port_change (hci, PORT_RESET_STAT, port_num);
            OK (0);
        }
        break;

    case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
        switch (wValue)
        {
        case (RH_PORT_SUSPEND):
            set_port_stat(hci, PORT_SUSPEND_STAT, port_num);
            OK (0);

        case (RH_PORT_RESET):
            set_port_stat(hci, PORT_RESET_STAT, port_num);
            clr_port_change (hci, PORT_CONNECT_CHANGE
                             | PORT_ENABLE_CHANGE | PORT_SUSPEND_CHANGE
                             | PORT_OVER_CURRENT_CHANGE, port_num);
            set_port_change(hci, PORT_RESET_CHANGE, port_num);
            clr_port_stat(hci, PORT_RESET_STAT, port_num);
            set_port_stat(hci, PORT_ENABLE_STAT, port_num);
            OK (0);

        case (RH_PORT_POWER):
            set_port_stat(hci, PORT_POWER_STAT, port_num);
            OK (0);

        case (RH_PORT_ENABLE):
            set_port_stat(hci, PORT_ENABLE_STAT, port_num);
            OK (0);
        }
        break;

    case RH_SET_ADDRESS:
            hci->port[port_num].rh.devnum = wValue;
            OK(0);

    case RH_GET_DESCRIPTOR:
        switch ((wValue & 0xff00) >> 8)
        {
        case (0x01): /* device descriptor */
            temp = min ((__u16) sizeof (root_hub_dev_des[port_num]), wLength);
            len = min (leni, temp);
            data_buf = root_hub_dev_des[port_num]; OK(len);

        case (0x02): /* configuration descriptor */
            temp = min ((__u16) sizeof (root_hub_config_des[port_num]), wLength);
            len = min (leni, temp);
            data_buf = root_hub_config_des[port_num]; OK(len);

        case (0x03): /* string descriptors */
            len = usb_root_hub_string (wValue & 0xff,(int)(long) port_num,
                                       "CY7C67200/300", data, wLength);
            if (len > 0)
            {
                data_buf = data;
                OK (min (leni, len));
            }

        default:
            status = STALL_FLG;
        }
        break;

    case RH_GET_DESCRIPTOR | RH_CLASS:
        data_buf [0] = 9;       /* min length; */
        data_buf [1] = 0x29;
        data_buf [2] = 1;       /* # of downstream port */
        data_buf [3] = 0;
        datab [1] = 0;
        data_buf [5] = 50;      /* 100 ms for port reset */
        data_buf [7] = 0xfc;    /* which port is attachable */
        if (data_buf [2] < 7)
        {
            data_buf [8] = 0xff;
        }
        else
        {
        }

        temp = min ((__u16) data_buf [0], wLength);
        len = min (leni, temp);
        OK (len);

    case RH_GET_CONFIGURATION:
        *(__u8 *) data_buf = 0x01;
        OK (1);

    case RH_SET_CONFIGURATION:
        OK (0);

    default:
        cy_err("unsupported root hub command");
        status = STALL_FLG;
    }

    len = min(len, leni);
    if (data != data_buf)
        memcpy (data, data_buf, len);

    urb->actual_length = len;
    urb->status = cc_to_error (status);

    urb->hcpriv = NULL;
    urb->dev = NULL;
    if (urb->complete)
    {
        urb->complete (urb);
    }

    return 0;
}

/***************************************************************************
 * Function Name : rh_unlink_urb
 *
 * This function unlinks the URB
 *
 * Input: urb = USB request block
 *
 * Return: 0
 **************************************************************************/

int rh_unlink_urb (struct urb * urb, int port_num)
{
    cy_priv_t * cy_priv = urb->dev->bus->hcpriv;
    hci_t * hci = cy_priv->hci;

    cy_dbg("enter rh_unlink_urb\n");
    if (hci->port[port_num].rh.urb == urb)
    {
        hci->port[port_num].rh.send = 0;
        del_timer (&hci->port[port_num].rh.rh_int_timer);
        hci->port[port_num].rh.urb = NULL;

        urb->hcpriv = NULL;
        usb_dec_dev_use(urb->dev);
        urb->dev = NULL;
        if (urb->transfer_flags & USB_ASYNC_UNLINK)
        {
            urb->status = -ECONNRESET;
            if (urb->complete)
            {
                urb->complete (urb);
            }
        }
        else
            urb->status = -ENOENT;
    }
    return 0;
}

/***************************************************************************
 * Function Name : rh_connect_rh
 *
 * This function connect the virtual root hub to the USB stack
 *
 * Input: urb = USB request block
 *
 * Return: 0
 **************************************************************************/

int rh_connect_rh (cy_priv_t * cy_priv, int port_num)
{
    struct usb_device  * usb_dev = NULL;

    hci_t * hci = cy_priv->hci;
    hci->port[port_num].rh.devnum = 0;

    if (hci == NULL)
    {
        cy_err("rh_connect_rh: hci is NULL!\n");
    }

    if (hci->port[port_num].bus == NULL)
    {
        cy_err("rh_connect_rh: hci->port[port_num].bus is NULL!\n");
    }

    cy_dbg("rh_connect_rh: hci->port[%d].bus = 0x%x\n",
            port_num, hci->port[port_num].bus);

    usb_dev = usb_alloc_dev (NULL, hci->port[port_num].bus);
    if (!usb_dev)
    {
        cy_err("rh_connect_rh: usb_dev == NULL!\n");
        return -ENOMEM;
    }

    cy_dbg("rh_connect_rh: usb_dev->bus = 0x%x, busnu = 0x%x\n",
            usb_dev->bus, usb_dev->bus->busnum);

    if (usb_dev->bus == NULL)
    {
        cy_err("rh_connect_rh: usb_dev->bus is NULL");
    }

    hci->port[port_num].bus->root_hub = usb_dev;
    usb_connect (usb_dev);
    if (usb_new_device (usb_dev) != 0)
    {
        usb_free_dev (usb_dev);
        return -ENODEV;
    }

    return 0;
}

