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

/*-------------------------------------------------------------------------*/
/* list of all controllers using this driver
 * */

static LIST_HEAD (hci_hcd_list);


/* URB states (urb_state) */
/* isoc, interrupt single state */

/* bulk transfer main state and 0-length packet */
#define US_BULK     0
#define US_BULK0    1

/* three setup states */
#define US_CTRL_SETUP   2
#define US_CTRL_DATA    1
#define US_CTRL_ACK     0

#define TT_CONTROL      0
#define TT_ISOCHRONOUS  1
#define TT_BULK         2
#define TT_INTERRUPT    3


/*-------------------------------------------------------------------------*/
/* HC private part of a device descriptor
 * */
#define MAX_NUM_PORT 4 /* srikanth 4 */
#define NUM_EDS 32

typedef struct epd {
    struct urb * pipe_head;
    struct list_head urb_queue;
    /* int urb_state; */
    struct timer_list timeout;
    int last_iso;           /* timestamp of last queued ISOC transfer */

} epd_t;

struct hci_device {
    epd_t ed [NUM_EDS];
};

/*-------------------------------------------------------------------------*/
/* Virtual Root HUB
 * */

#define usb_to_hci(usb) ((struct hci_device *)(usb)->hcpriv)



#if 1
/* USB HUB CONSTANTS (not OHCI-specific; see hub.h and USB spec) */

/* destination of request */
#define RH_INTERFACE               0x01
#define RH_ENDPOINT                0x02
#define RH_OTHER                   0x03

#define RH_CLASS                   0x20
#define RH_VENDOR                  0x40

/* Requests: bRequest << 8 | bmRequestType */
#define RH_GET_STATUS           0x0080
#define RH_CLEAR_FEATURE        0x0100
#define RH_SET_FEATURE          0x0300
#define RH_SET_ADDRESS          0x0500
#define RH_GET_DESCRIPTOR       0x0680
#define RH_SET_DESCRIPTOR       0x0700
#define RH_GET_CONFIGURATION    0x0880
#define RH_SET_CONFIGURATION    0x0900
#define RH_GET_STATE            0x0280
#define RH_GET_INTERFACE        0x0A80
#define RH_SET_INTERFACE        0x0B00
#define RH_SYNC_FRAME           0x0C80
/* Our Vendor Specific Request */
#define RH_SET_EP               0x2000


/* Hub port features */
#define RH_PORT_CONNECTION         0x00
#define RH_PORT_ENABLE             0x01
#define RH_PORT_SUSPEND            0x02
#define RH_PORT_OVER_CURRENT       0x03
#define RH_PORT_RESET              0x04
#define RH_PORT_POWER              0x08
#define RH_PORT_LOW_SPEED          0x09

#define RH_C_PORT_CONNECTION       0x10
#define RH_C_PORT_ENABLE           0x11
#define RH_C_PORT_SUSPEND          0x12
#define RH_C_PORT_OVER_CURRENT     0x13
#define RH_C_PORT_RESET            0x14

/* Hub features */
#define RH_C_HUB_LOCAL_POWER       0x00
#define RH_C_HUB_OVER_CURRENT      0x01

#define RH_DEVICE_REMOTE_WAKEUP    0x00
#define RH_ENDPOINT_STALL          0x01

#endif


/*-------------------------------------------------------------------------*/
/* struct for each HC
 * */

#define MAX_TRANS 32

typedef struct hci {
    wait_queue_head_t waitq;    /* deletion of URBs and devices needs a waitqueue */
    int active;         /* HC is operating */
    struct list_head hci_hcd_list;  /* list of all hci_hcd */
    int valid_host_port[MAX_NUM_PORT];  /* contains array of host port number, not occupy = -1 */
    sie_t sie[2];                   /* two SIE's in the chip */
    hcipriv_t port[MAX_NUM_PORT];    /* individual part of hc type */
} hci_t;


/*-------------------------------------------------------------------------*/
/* condition (error) codes and mapping OHCI like
 * */

#define TD_CC_NOERROR      0x00
#define TD_CC_CRC          0x01
#define TD_CC_BITSTUFFING  0x02
#define TD_CC_DATATOGGLEM  0x03
#define TD_CC_STALL        0x04
#define TD_DEVNOTRESP      0x05
#define TD_PIDCHECKFAIL    0x06
#define TD_UNEXPECTEDPID   0x07
#define TD_DATAOVERRUN     0x08
#define TD_DATAUNDERRUN    0x09
#define TD_BUFFEROVERRUN   0x0C
#define TD_BUFFERUNDERRUN  0x0D
#define TD_NOTACCESSED     0x0F

#if 0 
static int cc_to_error[16] = {
  /* mapping of the OHCI CC status to error codes */
  /* No  Error  */               USB_ST_NOERROR,
  /* CRC Error  */               USB_ST_CRC,
  /* Bit Stuff  */               USB_ST_BITSTUFF,
  /* Data Togg  */               USB_ST_CRC,
  /* Stall      */               USB_ST_STALL,
  /* DevNotResp */               USB_ST_NORESPONSE,
  /* PIDCheck   */               USB_ST_BITSTUFF,
  /* UnExpPID   */               USB_ST_BITSTUFF,
  /* DataOver   */               USB_ST_DATAOVERRUN,
  /* DataUnder  */               USB_ST_DATAUNDERRUN,
  /* reservd    */               USB_ST_NORESPONSE,
  /* reservd    */               USB_ST_NORESPONSE,
  /* BufferOver */               USB_ST_BUFFEROVERRUN,
  /* BuffUnder  */               USB_ST_BUFFERUNDERRUN,
  /* Not Access */               USB_ST_NORESPONSE,
  /* Not Access */               USB_ST_NORESPONSE
};
#endif

/*-------------------------------------------------------------------------*/
/* misc
 * */
#ifndef min
    #define min(a,b) (((a)<(b))?(a):(b))
#endif
/* #define GET_FRAME_NUMBER(hci) (hci)->frame_number */

/*-------------------------------------------------------------------------*/
/* data types
 * */

extern struct usb_operations hci_device_operations;

/*-------------------------------------------------------------------------*/
/* functions
 * */

/* urb interface functions */
int hci_get_current_frame_number (struct usb_device *usb_dev);

/* root hub */
int rh_submit_urb (struct urb *urb, int port_num);
int rh_unlink_urb (struct urb *urb, int port_num);

/* schedule functions */
void sh_td_list_check_stat (cy_priv_t * cy_priv, int sie_num);
int sh_schedule_trans (cy_priv_t * cy_priv, int sie_num);

/* hc specific functions */
void hc_flush_data_cache (hci_t * hci, void * data, int len);

/* debug functions */
void HWTrace(unsigned short data);

/* debug| print the main components of an URB
 * small: 0) header + data packets 1) just header */

static inline void urb_print (struct urb * urb, char * str, int small)
{
    unsigned int pipe= urb->pipe;
    int i, len;

    if (!urb->dev || !urb->dev->bus) {
        dbg("%s URB: no dev", str);
        return;
    }

    printk("%s URB:[%4x] dev:%2d,ep:%2d-%c,type:%s,flags:%4x,len:%d/%d,stat:%d(%x)\n",
            str,
            hci_get_current_frame_number (urb->dev),
            usb_pipedevice (pipe),
            usb_pipeendpoint (pipe),
            usb_pipeout (pipe)? 'O': 'I',
            usb_pipetype (pipe) < 2? (usb_pipeint (pipe)? "INTR": "ISOC"):
                (usb_pipecontrol (pipe)? "CTRL": "BULK"),
            urb->transfer_flags,
            urb->actual_length,
            urb->transfer_buffer_length,
            urb->status, urb->status);
    if (!small) {
        if (usb_pipecontrol (pipe)) {
            printk ( __FILE__ ": cmd(8):");
            for (i = 0; i < 8 ; i++)
                printk (" %02x", ((__u8 *) urb->setup_packet) [i]);
            printk ("\n");
        }
        if (urb->transfer_buffer_length > 0 && urb->transfer_buffer) {
            printk ( __FILE__ ": data(%d/%d):",
                urb->actual_length,
                urb->transfer_buffer_length);
            len = usb_pipeout (pipe)?
                        urb->transfer_buffer_length: urb->actual_length;
            for (i = 0; i < 2096 && i < len; i++)
                printk (" %02x", ((__u8 *) urb->transfer_buffer) [i]);
            printk ("%s stat:%d\n", i < len? "...": "", urb->status);
        }
    }
}

