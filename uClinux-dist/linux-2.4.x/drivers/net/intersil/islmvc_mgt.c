/*  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* Begin of include files */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <linux/netlink.h>

#include <asm/arch/blobcalls.h>
#include <linux_mvc_oids.h>
#include "islmvc_mgt.h"
#include "islmvc.h"
#include "isl_wds.h"
#include "isl_mgt.h"

/* For setting/clearing LEDs on the isl3893 platform. */
#ifdef BLOB_V2
#include <asm-armnommu/arch-isl3893/gpio.h>
#endif

/* End of include files */

struct wds_link_msg {
    unsigned char mac[6];
    char name[32];
} __attribute__ ((packed));

/* pointer to the netlink socket */
struct sock *nl_sock = NULL;

/* Static variable for the trap sequence counter */
__u32 trap_seq = 0;

extern unsigned int src_seq;

struct frame_attachment *frame_attach[256][MAX_ASSOC_IDS];


/****************************************************************
 *
 *      		Handlers
 *
 ***************************************************************/

static int islmvc_send_mvc_msg_hndl( unsigned int dev_type, unsigned int dev_seq,
                                    unsigned int src_id,   unsigned int src_seq,
                                    unsigned int prismop,  unsigned int oid, void *data, unsigned long len);
static int islmvc_wdslink_add_hndl( unsigned int, unsigned int, unsigned int, unsigned,
                             unsigned int, unsigned int, void *, unsigned long );
static int islmvc_wdslink_del_hndl( unsigned int, unsigned int, unsigned int, unsigned,
                             unsigned int, unsigned int, void *, unsigned long );
static int islmvc_dot11_oid_attachment_hndl( unsigned int src_id,   unsigned int src_seq,
                             unsigned int dev_id,   unsigned int dev_seq,
                             unsigned int op,       unsigned int oid, void *data, unsigned long len);
//static int islmvc_dot11_oid_authenticate_hndl( unsigned int src_id,   unsigned int src_seq,
//                             unsigned int dev_id,   unsigned int dev_seq,
//                             unsigned int op,       unsigned int oid, void *data, unsigned long len);
//static int islmvc_dot11_oid_associate_hndl( unsigned int src_id,   unsigned int src_seq,
//                             unsigned int dev_id,   unsigned int dev_seq,
//                             unsigned int op,       unsigned int oid, void *data, unsigned long len);

#ifdef HANDLE_IAPP_IN_DRIVER
static int islmvc_gen_iappframe_hndl( unsigned int src_id, unsigned int src_seq,
                                     unsigned int dev_id,   unsigned int dev_seq,
                                     unsigned int op,       unsigned int oid, void *data, unsigned long len);
#endif /* HANDLE_IAPP_IN_DRIVER */

#ifdef HANDLE_ASSOC_IN_DRIVER
static int islmvc_dot11_oid_authenticate_hndl( unsigned int src_id, unsigned int src_seq,
                                     unsigned int dev_id,   unsigned int dev_seq,
                                     unsigned int op,       unsigned int oid, void *data, unsigned long len);
static int islmvc_dot11_oid_associate_hndl( unsigned int src_id, unsigned int src_seq,
                                     unsigned int dev_id,   unsigned int dev_seq,
                                     unsigned int op,       unsigned int oid, void *data, unsigned long len);
#endif /* HANDLE_ASSOC_IN_DRIVER */

#ifdef BLOB_V2
static int islmvc_gen_oid_led_set_hndl( unsigned int dev_id,   unsigned int dev_seq,
                             unsigned int src_id,   unsigned int src_seq,
                             unsigned int op,       unsigned int oid, void *data, unsigned long len);
static int islmvc_gen_oid_led_clear_hndl( unsigned int dev_id,   unsigned int dev_seq,
                             unsigned int src_id,   unsigned int src_seq,
                             unsigned int op,       unsigned int oid, void *data, unsigned long len);
#endif /* BLOB_V2 */


static int mgmt_set_oid_attachment(char *data, struct net_device *dev, unsigned long length);

unsigned char * pda_get_mac_address(void);




/*
 * Initializes all handlers for the specific OIDs and a generic handler for the rest.
 *
 */
void islmvc_mgt_init(struct net_device *dev)
{
    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "islmvc_mgt_init for dev %s\n", dev->name);

    mgt_initialize();
    mgt_indication_handler ( DEV_NETWORK, dev->name, 0, islmvc_send_mvc_msg_hndl );

    if (GET_DEV_TYPE(dev->base_addr) == BIS_DT_802DOT11) /* Only needed for 802.11 devices */
    {
        mgt_indication_handler ( DEV_NETWORK, dev->name, DOT11_OID_WDSLINKADD,   islmvc_wdslink_add_hndl );
        mgt_indication_handler ( DEV_NETWORK, dev->name, DOT11_OID_WDSLINKREMOVE,islmvc_wdslink_del_hndl );
        mgt_indication_handler ( DEV_NETWORK, dev->name, DOT11_OID_ATTACHMENT,   islmvc_dot11_oid_attachment_hndl );

#ifdef HANDLE_IAPP_IN_DRIVER
        mgt_response_handler ( DEV_NETWORK, dev->name, DOT11_OID_ASSOCIATE,     islmvc_gen_iappframe_hndl );
        mgt_response_handler ( DEV_NETWORK, dev->name, DOT11_OID_ASSOCIATEEX,   islmvc_gen_iappframe_hndl );
        mgt_response_handler ( DEV_NETWORK, dev->name, DOT11_OID_REASSOCIATE,   islmvc_gen_iappframe_hndl );
        mgt_response_handler ( DEV_NETWORK, dev->name, DOT11_OID_REASSOCIATEEX, islmvc_gen_iappframe_hndl );
#endif

#ifdef HANDLE_ASSOC_IN_DRIVER
        mgt_response_handler ( DEV_NETWORK, dev->name, DOT11_OID_AUTHENTICATE, islmvc_dot11_oid_authenticate_hndl );
        mgt_response_handler ( DEV_NETWORK, dev->name, DOT11_OID_ASSOCIATE,    islmvc_dot11_oid_associate_hndl );
#endif
    }

    /* Mgmt handlers for setting/clearing the LEDs. */
#ifdef BLOB_V2
    mgt_indication_handler ( DEV_NETWORK, dev->name, GEN_OID_LED_SET,   islmvc_gen_oid_led_set_hndl );
    mgt_indication_handler ( DEV_NETWORK, dev->name, GEN_OID_LED_CLEAR, islmvc_gen_oid_led_clear_hndl );
#endif BLOB_V2

}


/****************************************************************
 *								*
 *    		Begin of management handling functions		*
 *								*
 ***************************************************************/

/* All OIDs */

#define PRISMOP2DEVOP(op) (op == PIMFOR_OP_GET ) ? OPGET : ( (op == PIMFOR_OP_SET ) ? OPSET : -1 )

static int islmvc_send_mvc_msg_hndl( unsigned int dev_type, unsigned int dev_seq,
                                    unsigned int src_id,   unsigned int src_seq,
                                    unsigned int prismop,  unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;
    unsigned int devop = PRISMOP2DEVOP(prismop);

    _DEBUG(DBG_FUNC_CALL | DBG_MGMT,
          "islmvc_send_mvc_msg_hndl, dev_type %d, dev_seq %d, src_id %d, src_seq %d, prismop %d, oid %x, data %p, len %lu \n",
          dev_type, dev_seq, src_id, src_seq, prismop, oid, data, len);

    if ( devop < 0 )
        return -1;

    if ( ! (dev = dev_get_by_index(dev_seq)) ) {
        if ( (mgt_response( src_id, src_seq, dev_type, dev_seq, PIMFOR_OP_ERROR, oid, data, len)) < 0 ) {
            return -1;
        }
        return 0;
    }
    dev_put( dev );

    /* src_id and src_seq sunk */

    if ( islmvc_send_mvc_msg( dev, devop, oid, data, len ) < 0 ) {
        if ( (mgt_response( src_id, src_seq, dev_type, dev_seq, PIMFOR_OP_ERROR, oid, data, len)) < 0 ) {
            return -1;
        }
        return 0;
    }

    if ( (mgt_response( src_id, src_seq, dev_type, dev_seq, PIMFOR_OP_RESPONSE, oid, data, len)) < 0 ) {
        return -1;
    }

    return 0;
}


/**********************************************************************
 *  islmvc_send_mvc_msg
 *
 *  DESCRIPTION: Send a message to perform on the MVC.
 *
 *  PARAMETERS:	 dev       - device to operate on.
 *               operation - operation to perform.
 *               oid       - OID to perform operation on.
 *               data      - pointer to buffer that (will) hold the data.
 *               data_len  - length of data.
 *
 *  RETURN:      0 on success
 *               -EOVERFLOW when length of data is too small.
 *               -EINVAL when operation did not succeed.
 *
 **********************************************************************/
int islmvc_send_mvc_msg(struct net_device *dev, int operation, int oid, long* data, unsigned long data_len)
{
    int error = 0;
    struct msg_conf msg;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "islmvc_send_mvc_msg, dev %s, oper %d, oid %x, data %p, len %lu. \n",
          dev->name, operation, oid, data, data_len);

    msg.operation = operation;
    msg.oid       = oid;
    msg.data      = data;
#ifdef BLOB_V2
    msg.size      = data_len; /* MVC v2 only */
#endif


#ifdef TEST_ON_POLDHU
    /*
       OIDs for the blob device need some special handling. Don't know if MVC v2 still needs a
       construction like this?
    */

    if ( (oid & 0xFF000000) == 0x80000000) /* This is an OID for the blob device */
    {
        DEBUG(DBG_MGMT, "Msg for the BLOB device...\n");
        /* This is an OID for the BLOB device. We only allow access to the blob device via eth0  */
        if (GET_DEV_TYPE(dev->base_addr) == BIS_DT_802DOT3)
        {
            /* Check if we're manipulating the WLAN LED.
             * If so, we should halt the WLAN device.
             */
            error = BER_NONE;
            switch (oid)
            {
                case BLOB_OID_LEDSET:
                case BLOB_OID_LEDCLEAR:
                if ( (*data & 2) &&
                         (dev_state(1 /* DEVICE_WLAN*/ ) == STRUNNING) )
                {
                    error = dev_halt(1/* DEVICE_WLAN*/);
                }
                break;
                case BLOB_OID_LED_RESET:
                if ( (*data & 2) &&
                         (dev_state(1 /* DEVICE_WLAN*/) == STREADY) )
                {
                    error = dev_run(1 /* DEVICE_WLAN*/);
                }
                return (error < BER_NONE) ? -EINVAL : 0;
                default:
                /* Not one of these */
                break;
            }
            if (error < BER_NONE)
                return -EINVAL;

            if ((error = blob_conf(&msg)) < BER_NONE)
                return -EINVAL;
        }
        else
        {
            return -EOPNOTSUPP;
        }
    }
    else
#endif /* TEST_ON_POLDHU */


    /* Send message to MVC */
    error = dev_conf (GET_DEV_NR(dev->base_addr), &msg);

    if (error == BER_OVERFLOW)
    {
#ifdef BLOB_V2
        printk("Size passed to MVC (%ld) not correct. MVC needs %ld\n", data_len, msg.size);
#else
        printk("Size passed to MVC (%ld) not correct.\n", data_len);
#endif
        return -EOVERFLOW;
    }

    /* Copy the message data for retrieval by the calling function. */
    if (msg.data != NULL)
        memcpy(data, msg.data, data_len);

    if (error < 0)
        return -EINVAL;
    else
        return error;
}


/**********************************************************************
 *  islmvc_send_blob_msg
 *
 *  DESCRIPTION: Send a message to perform on the MVC blob device.
 *
 *  PARAMETERS:  operation - operation to perform.
 *               oid       - OID to perform operation on.
 *               data      - pointer to buffer that (will) hold the data.
 *               data_len  - length of data.
 *
 *  RETURN:	 0 on success.
 *               -EINVAL on error.
 *
 **********************************************************************/
int islmvc_send_blob_msg(int operation, int oid, long* data, unsigned long data_len)
{
    int             error;
    struct msg_conf msg;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "islmvc_send_blob_msg, oper %d, oid %x, data %p, len %lu. \n",
          operation, oid, data, data_len);

    msg.operation = operation;
    msg.oid       = oid;
    msg.data      = data;
#ifdef BLOB_V2
    msg.size      = data_len;
#endif

    if ((error = blob_conf(&msg)) < BER_NONE)
        return -EINVAL;

    if (msg.data != NULL)
        memcpy(data, msg.data, data_len);

    return 0;

}


/****************************************************************
 *								*
 *    		End of management handling functions		*
 *								*
 ***************************************************************/


/****************************************************************
 *								*
 *    	Functions to retrieve information from the PDA      *
 *      (Production Data Area)                  	        *
 *								*
 ***************************************************************/
#define PDR_END                                 0x0000
#define PDR_MAC_ADDRESS                         0x0101
#define PDR_INTERFACE_LIST                      0x1001
#define PDR_RSSI_LINEAR_APPROXIMATION_DB        0x1905
#define PDR_ZIF_TX_IQ_CALIBR_DATA		0x1906

#define BR_IF_ID_ISL39300A                      0x0010

#define BR_IF_VAR_ISL39000_SYNTH_MASK           0x0007
#define BR_IF_VAR_ISL39000_SYNTH_1              0x0001
#define BR_IF_VAR_ISL39000_SYNTH_2              0x0002
#define BR_IF_VAR_ISL39000_SYNTH_3              0x0003

#define BR_IF_VAR_ISL39000_IQ_CAL_MASK          0x0018
#define BR_IF_VAR_ISL39000_IQ_CAL_PA_DETECTOR   0x0000
#define BR_IF_VAR_ISL39000_IQ_CAL_DISABLED      0x0008
#define BR_IF_VAR_ISL39000_IQ_CAL_ZIF           0x0010

#define BR_IF_VAR_ISL39000_ASM_MASK		0x0400
#define BR_IF_VAR_ISL39000_ASM_XSWON		0x0400

#define PTR_PDA_AREA_START 0x00000208 /* Contents of address defines the start of the PDA area. */
#ifdef CONFIG_MTD_PHYSMAP_START
static unsigned long *Pda_area_start = (unsigned long*) (CONFIG_MTD_PHYSMAP_START + PTR_PDA_AREA_START);
#else
// configuration not set, assume it is 0.
static unsigned long *Pda_area_start = (unsigned long*) PTR_PDA_AREA_START;
#endif

struct Pdr_Struct {
   unsigned short   PdrLength;
   unsigned short   PdrCode;
   unsigned char    PdrData[2];
};

/**********************************************************************
 *  pda_find_pdr
 *
 *  DESCRIPTION: Finds a Production Data Record in the PDA. (Production Data Area)
 *
 *  PARAMETERS:  pdrCode - code of production data record to find.
 *
 *  RETURN:	 Pointer to found PDR data, or NULL when not found.
 *
 **********************************************************************/
static struct Pdr_Struct *pda_find_pdr (unsigned short pdrCode)
{
   unsigned short   pdaOffset = 0;
   unsigned short    *pdaPtr;    /* Used as index through the PDA pointing to start of records. */
   struct Pdr_Struct *pdrPtr;    /* Used as pointer to the contents of the record. */

   pdaPtr = (unsigned short *) *Pda_area_start;
   pdrPtr = (struct Pdr_Struct *) pdaPtr;

   DEBUG(DBG_FUNC_CALL | DBG_MGMT, "pda_find_pdr, pdrCode = %x\n", pdrCode);

  /* Walk through the cached PDA and check each PDR for it's PDRcode to match the one we are looking for. */
   while (pdrPtr->PdrCode != PDR_END) {
      if (pdrPtr->PdrCode == pdrCode) {
         return pdrPtr;
      } else {
         pdaOffset = pdrPtr->PdrLength + 1;
         /* If we get past 8k or we have an invalid PdrLength, return NULL
            since this indicates an invalid PDA. */
         if ((pdaOffset > 8192) || (pdrPtr->PdrLength == 65535) || (pdrPtr->PdrLength == 0)) {
             return (struct Pdr_Struct *)0;
         }
        /* point to next record */
         pdaPtr = (unsigned short *)pdaPtr + pdaOffset;
         pdrPtr = (struct Pdr_Struct *)pdaPtr;
      }
   };

   if (pdrCode == PDR_END) {
      /* If we were looking for PDR_END, we found it */
       return pdrPtr;
   } else {
      /* We were looking for another PDR and did not find it */
     return (struct Pdr_Struct *)0;
   }
}


/**********************************************************************
 *  pda_get_mac_address
 *
 *  DESCRIPTION: Gets the MAC address from the PDA. (Production Data Area)
 *
 *  RETURN:	 Pointer to MAC address, or NULL when not found.
 *
 **********************************************************************/
unsigned char * pda_get_mac_address(void)
{
   struct Pdr_Struct *pdrStructPtr;

   DEBUG(DBG_FUNC_CALL | DBG_MGMT, "pda_get_mac_address\n");

   pdrStructPtr = pda_find_pdr (PDR_MAC_ADDRESS);
   if (pdrStructPtr) {
       return (unsigned char *)(pdrStructPtr->PdrData);
   } else {
       return (unsigned char *)0;
   }
}

#ifdef BLOB_V2

/**********************************************************************
 *  pda_get_rssi_linear_approximation_db
 *
 *  DESCRIPTION: Gets the RSSI vector from the PDA. (Production Data Area)
 *
 *  RETURN:	 Pointer to data structure, or NULL when not found.
 *
 **********************************************************************/
short *pda_get_rssi_linear_approximation_db(void)
{
    struct Pdr_Struct *pdrStructPtr;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "pda_get_rssi_linear_approximation_db\n");

    pdrStructPtr = pda_find_pdr (PDR_RSSI_LINEAR_APPROXIMATION_DB);
    if (pdrStructPtr) {
        return (short*)(pdrStructPtr->PdrData);
    } else {
        return NULL;
    }
}


/**********************************************************************
 *  pda_get_zif_tx_iq_calibr_data
 *
 *  DESCRIPTION: Gets the zif TX IQ calibration data  from the PDA.
 *
 *  RETURN:	 Pointer to data structure, or NULL when not found.
 *
 **********************************************************************/
struct obj_iqtable *pda_get_zif_tx_iq_calibr_data(int *len)
{
    struct Pdr_Struct *pdrStructPtr;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "pda_get_zif_tx_iq_calibr_data\n");

    pdrStructPtr = pda_find_pdr (PDR_ZIF_TX_IQ_CALIBR_DATA);

    *len = 2 * pdrStructPtr->PdrLength - 1;

    if (pdrStructPtr) {
        return (struct obj_iqtable*)(pdrStructPtr->PdrData);
    } else {
        return NULL;
    }
}


/**********************************************************************
 *  pda_get_interface_list
 *
 *  DESCRIPTION: Gets the interface list from the PDA.
 *
 *  RETURN:	 Pointer to data structure, or NULL when not found.
 *
 **********************************************************************/
struct Pdr_Struct *pda_get_interface_list(void)
{
    struct Pdr_Struct *pdrStructPtr;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "pda_get_interface_list\n");

    pdrStructPtr = pda_find_pdr (PDR_INTERFACE_LIST);
    if (pdrStructPtr) {
        return pdrStructPtr;
    } else {
        return NULL;
    }
}


/**********************************************************************
 *  set_rssi_linear_approximation_db
 *
 *  DESCRIPTION: Gets the RSSI linear approximation data from the PDA,
 *               formats it into MVC parseable format, and sends it to
 *               the MVC.
 *
 *  PARAMETERS:  dev - device to do setting on.
 *
 *  RETURN:	 0 on success. -EINVAL on error.
 *
 **********************************************************************/
int set_rssi_linear_approximation_db(struct net_device *dev)
{
    short *rssi_pda_data;
    struct obj_rssivector array[2];

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "set_rssi_linear_approximation_db\n");

    if ((rssi_pda_data = pda_get_rssi_linear_approximation_db()) != NULL) {

        /* Convert the data from PDA format to MVC format. */
        array[0].a = (long)(*(rssi_pda_data));
        rssi_pda_data++;
        array[0].b = (long)(*(rssi_pda_data));
        rssi_pda_data++;
        array[1].a = (long)(*(rssi_pda_data));
        rssi_pda_data++;
        array[1].b = (long)(*(rssi_pda_data));

        if (islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_RSSIVECTOR, (long*)array, sizeof(array)) < BER_NONE)
            return -EINVAL;
    } else {
        printk(KERN_INFO "Could't get the RSSI Vector from the PDA.\n");
        return -EINVAL;
    }

    return 0;
}


/**********************************************************************
 *  set_zif_tx_iq_calibr_data
 *
 *  DESCRIPTION: Get the zif tx iq calibration data, and send it to the MVC.
 *
 *  PARAMETERS:  dev - device to do setting on.
 *
 *  RETURN:	 0 on success. -EINVAL on error.
 *
 **********************************************************************/
int set_zif_tx_iq_calibr_data(struct net_device *dev)
{
    struct obj_iqtable *iqtable;
    int length;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "set_zif_tx_iq_calibr_data\n");

    if ((iqtable = pda_get_zif_tx_iq_calibr_data(&length)) != NULL)
    {
        if (islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_IQCALIBRATIONTABLE, (long*)iqtable,
                                length) < BER_NONE)
        {
            printk(KERN_INFO "Couldn't set IQ Calibration Table in MVC!\n");
            return -EINVAL;
        }
    }
    else
    {
        printk(KERN_INFO "Couldn't get the calibration table from the PDA.\n");
        return -EINVAL;
    }

    return 0;
}


/**********************************************************************
 *  set_hw_interface_variant
 *
 *  DESCRIPTION: Get the interface list from the PDA, process it's contents
 *               and send it to the MVC.
 *
 *  PARAMETERS:  dev - device to do setting on.
 *
 *  RETURN:	 0 on success. -EINVAL on error.
 *
 **********************************************************************/
int set_hw_interface_variant(struct net_device *dev)
{
    int i;
    unsigned short temp;
    struct Pdr_Struct *pdrPtr;
    u_char* p;
    long mvc_data;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "set_hw_interface_variant\n");

    if ((pdrPtr = pda_get_interface_list()) != NULL) {

        p = &(pdrPtr->PdrData[0]);

        for(i=0; i < (pdrPtr->PdrLength/5); i++) {
            p = p + 2; /* skip role */

            temp = (*p);
            p++;
            temp |= (*p << 8);
            if(temp == BR_IF_ID_ISL39300A) {
                /* found the HW interface */
                p++; /* goto variant */

                temp = (*p);
                p++;
                temp |= (*p << 8);

                /* Synthesizer */
                if((temp & BR_IF_VAR_ISL39000_SYNTH_MASK) == BR_IF_VAR_ISL39000_SYNTH_1) {
                    mvc_data = 15;	// CMD_RADIO_CONFIG1
                }
                else if((temp & BR_IF_VAR_ISL39000_SYNTH_MASK) == BR_IF_VAR_ISL39000_SYNTH_2) {
                    mvc_data = 18;	// CMD_RADIO_CONFIG4
                }
                else if((temp & BR_IF_VAR_ISL39000_SYNTH_MASK) == BR_IF_VAR_ISL39000_SYNTH_3) {
                    mvc_data = 20;	// CMD_RADIO_CONFIG6
                }
                else {
                    printk(KERN_INFO "Unknown synthesizer type in PDA.\n");
                    mvc_data = -1;
                }

                if (mvc_data != -1) {
                    if (islmvc_send_blob_msg(OPSET, MT_OID_MTCMD, (long*)&mvc_data, sizeof(mvc_data)) < BER_NONE)
                        return -EINVAL;
                }

                /* IQ Calibration */
                /* NOT IMPLEMENTED in MVC (YET).
                if((temp & BR_IF_VAR_ISL39000_IQ_CAL_MASK) == BR_IF_VAR_ISL39000_IQ_CAL_PA_DETECTOR) {
                    mvc_data = FIXME;
                }
                else if((temp & BR_IF_VAR_ISL39000_IQ_CAL_MASK) == BR_IF_VAR_ISL39000_IQ_CAL_DISABLED) {
                    mvc_data = FIXME;
                }
                else if((temp & BR_IF_VAR_ISL39000_IQ_CAL_MASK) == BR_IF_VAR_ISL39000_IQ_CAL_ZIF) {
                    mvc_data = FIXME;
                }
                else {
                    printk(KERN_INFO "Unknown type for IQ Calibration in PDA.\n");
                    mvc_data = -1;
                }

                if (mvc_data != -1) {
                    if (islmvc_send_mvc_msg(dev, OPSET, MT_OID_MTCMD, (long*)&mvc_data, sizeof(mvc_data)) < BER_NONE)
                        return -EINVAL;
                }
                */

		if ((temp & BR_IF_VAR_ISL39000_ASM_MASK) == BR_IF_VAR_ISL39000_ASM_XSWON)
		    mvc_data = 29;	// CMD_ANTSEL_MODE1
		else
		    mvc_data = 28;	// CMD_ANTSEL_MODE0

		if (islmvc_send_mvc_msg(dev, OPSET, MT_OID_MTCMD, (long*)&mvc_data, sizeof(mvc_data)) < BER_NONE)
		{
		    return -EINVAL;
		}
                return 0;
            }
            else {
                p = p + 7; /* goto next interface */
            }
        }

        printk(KERN_INFO "No HW interface for the ISL39300A found in PDR INTERFACE_LIST !\n");
    }
    else {
        printk(KERN_INFO "Couldn't get the interface list from the PDA.\n");
    }

    return -EINVAL;
}

#endif /* BLOB_V2 */

/****************************************************************
 *								*
 *      	End of PDA retrieve functions                   *
 *								*
 ***************************************************************/


/****************************************************************
 *
 *    			WDS Link handlers
 *
 ***************************************************************/

static int islmvc_wdslink_add_hndl( unsigned int dev_id,   unsigned int dev_seq,
                             unsigned int src_id,   unsigned int src_seq,
                             unsigned int op,       unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;
    struct wds_link_msg *wlmp;
    struct wds_priv *wdsp;

    _DEBUG(DBG_FUNC_CALL | DBG_MGMT, "islmvc_wdslink_add_hndl, dev_id %d, dev_seq %d, src_id %d, src_seq %d, op %d, oid %x, data %p, len %lu \n",
          dev_id, dev_seq, src_id, src_seq, op, oid, data, len);

    if ( len != sizeof(struct wds_link_msg))
        return -1;

    wlmp = (struct wds_link_msg *)data;

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    wdsp = ((struct net_local *)dev->priv)->wdsp;

    if ( add_wds_link(dev, wdsp, wlmp->mac, wlmp->name) < 0 )
        return -1;

    if ( islmvc_send_mvc_msg( dev, op, oid, (long *)wlmp->mac, 6 ) < 0 )
        return -1;

    /* open the dot1X data port for the AP link */
    if ( islmvc_send_mvc_msg( dev, op, DOT11_OID_EAPAUTHSTA, (long *)wlmp->mac, 6 ) < 0 )
        return -1;

    /*
    printk ( KERN_ERR "islmvc_wdslink_add_hndl(%x.%x.%x.%x.%x.%x, %s)\n",
             wlmp->mac[0], wlmp->mac[1], wlmp->mac[2], wlmp->mac[3], wlmp->mac[4], wlmp->mac[5], wlmp->name );
    */
    if ( (mgt_response( src_id, src_seq, dev_id, dev_seq, PIMFOR_OP_RESPONSE, oid, data, len)) < 0 )
        return -1;

    return 0;
}

static int islmvc_wdslink_del_hndl( unsigned int dev_id,   unsigned int dev_seq,
                             unsigned int src_id,   unsigned int src_seq,
                             unsigned int op,       unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;
    struct wds_link_msg *wlmp;
    struct wds_priv *wdsp;

    _DEBUG(DBG_FUNC_CALL | DBG_MGMT,
          "islmvc_wdslink_del_hndl, dev_id %d, dev_seq %d, src_id %d, src_seq %d, op %d, oid %x, data %p, len %lu \n",
          dev_id, dev_seq, src_id, src_seq, op, oid, data, len);

    if ( len != sizeof(struct wds_link_msg))
        return -1;

    wlmp = (struct wds_link_msg *)data;

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
   	dev_put( dev );

    wdsp = ((struct net_local *)dev->priv)->wdsp;

    /* close the dot1X data port for the AP link */

    if ( islmvc_send_mvc_msg( dev, op, DOT11_OID_EAPUNAUTHSTA, (long *)wlmp->mac, 6 ) < 0 )
        return -1;

    if ( islmvc_send_mvc_msg( dev, op, oid, (long *)wlmp->mac, 6 ) < 0 )
        return -1;

    if ( del_wds_link(dev, wdsp, wlmp->mac, wlmp->name ) < 0 )
        return -1;

    /*
    printk ( KERN_ERR "islmvc_wdslink_del_hndl(%x.%x.%x.%x.%x.%x, %s)\n",
             wlmp->mac[0], wlmp->mac[1], wlmp->mac[2], wlmp->mac[3], wlmp->mac[4], wlmp->mac[5], wlmp->name );
    */
    if ( (mgt_response( src_id, src_seq, dev_id, dev_seq, PIMFOR_OP_RESPONSE, oid, data, len)) < 0 )
        return -1;

    return 0;
}

/****************************************************************
 *
 *    			DOT11 OID Attachment handler
 *
 ***************************************************************/
static int islmvc_dot11_oid_attachment_hndl( unsigned int dev_id,   unsigned int dev_seq,
                             unsigned int src_id,   unsigned int src_seq,
                             unsigned int op,       unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;

    _DEBUG(DBG_FUNC_CALL | DBG_MGMT,
          "islmvc_dot11_oid_attachment_hndl, dev_id %d, dev_seq %d, src_id %d, src_seq %d, op %d, oid %x, data %p, len %lu \n",
          dev_id, dev_seq, src_id, src_seq, op, oid, data, len);

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    if ( mgmt_set_oid_attachment(data, dev, len) < 0 )
        return -1;

    if ( (mgt_response( src_id, src_seq, dev_id, dev_seq, PIMFOR_OP_RESPONSE, oid, data, len)) < 0 )
        return -1;

    return 0;
}


/****************************************************************
 *
 *   		DOT11 OID Authentication/Association handlers
 *
 ***************************************************************/

#ifdef HANDLE_ASSOC_IN_DRIVER
/**********************************************************************
 *
 *  mgmt_authenticate
 *
 **********************************************************************/
static void mgmt_authenticate (struct net_device *dev, char *data)
{
   struct obj_mlme *obj = (struct obj_mlme*) data;

   DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_authenticate, dev %s, data %p, (macaddress %s) \n",
         dev->name, data, hwaddr2string(obj->address));

   /* When there's no authentication application, we succeed always via this way... */
   if (obj->state == DOT11_STATE_AUTHING)
   {
       obj->code  = 0; // = DOT11_SC_SUCCESFUL;
       islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_AUTHENTICATE, (long *) obj, sizeof(struct obj_mlme));
   }

}


/* DOT11_OID_AUTHENTICATE */
static int islmvc_dot11_oid_authenticate_hndl( unsigned int src_id, unsigned int src_seq,
                                     unsigned int dev_id,   unsigned int dev_seq,
                                     unsigned int op,       unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT,
          "islmvc_dot11_oid_authenticate_hndl, src_id %d, src_seq %d, dev_id %d, dev_seq %d, op %d, oid %x, data %p, len %lu \n",
          src_id, src_seq, dev_id, dev_seq, op, oid, data, len);

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    mgmt_authenticate (dev, (char*) data);

    return 0;
}


/**********************************************************************
 *
 *  mgmt_associate
 *
 **********************************************************************/
static void mgmt_associate (struct net_device *dev, char *data)
{
   struct obj_mlme *obj = (struct obj_mlme*) data;

   DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_associate, dev %s, macaddress = %s\n",
         dev->name, hwaddr2string(obj->address));

   if (obj->state == DOT11_STATE_ASSOCING)
   {
       /* When there's no authentication application, we succeed always via this way... */
       obj->code  = 0; // = DOT11_SC_SUCCESFUL;
       islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_ASSOCIATE, (long *) obj, sizeof(struct obj_mlme));
   }
}


/* DOT11_OID_ASSOCIATE */
static int islmvc_dot11_oid_associate_hndl( unsigned int src_id, unsigned int src_seq,
                                     unsigned int dev_id,   unsigned int dev_seq,
                                     unsigned int op,       unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT,
          "islmvc_dot11_oid_associate_hndl, src_id %d, src_seq %d, dev_id %d, dev_seq %d, op %d, oid %x, data %p, len %lu \n",
          src_id, src_seq, dev_id, dev_seq, op, oid, data, len);

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    mgmt_associate (dev, (char*) data);

    return 0;
}

#endif /* HANDLE_ASSOC_IN_DRIVER */



#ifdef HANDLE_IAPP_IN_DRIVER
/**********************************************************************
 *
 *              IAPP update frame generation
 *
 **********************************************************************/

static int islmvc_gen_iappframe_hndl( unsigned int src_id, unsigned int src_seq,
                                     unsigned int dev_id, unsigned int dev_seq,
                                     unsigned int op, unsigned int oid, void *data, unsigned long len)
{
    struct obj_mlme *obj = (struct obj_mlme*) data;
    struct net_device *dev;
    struct sk_buff *skb;
    unsigned char buf[20];

    DEBUG(DBG_FUNC_CALL | DBG_MGMT,
          "islmvc_gen_iappframe_hndl, src_id %d, src_seq %d, dev_id %d, dev_seq %d, op %d, oid %x, data %p, len %lu \n",
          src_id, src_seq, dev_id, dev_seq, op, oid, data, len);

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    if (obj->state == DOT11_STATE_ASSOC)
    {
        // the client is successfully associated, now generate the IAPP update frame
        DEBUG( DBG_MGMT, "islmvc_gen_iappframe_hndl, client %02x:%02x:%02x:%02x:%02x:%02x associated successfully \n",
            obj->address[0], obj->address[1], obj->address[2], obj->address[3], obj->address[4], obj->address[5] );

        memset(buf, 0xff, 6);		        // Broadcast Destination address
        memcpy(buf + 6, obj->address, 6);	// Source address of associated client

        buf[12] = 0;			        // Length of payload = 6 octets
        buf[13] = 6;
        buf[14] = 0;			        // DSAP
        buf[15] = 0;			        // SSAP
        buf[16] = 175;			        // Control field

        buf[17] = 129;			        // XID = 129.1.0
        buf[18] = 1;
        buf[19] = 0;

        skb = dev_alloc_skb(20+2);

        if (skb)
        {
	    memcpy(skb_put(skb, 20), buf, 20);
            skb->dev = dev;
            skb->protocol = eth_type_trans(skb, dev);
            skb->ip_summed = CHECKSUM_UNNECESSARY;
            netif_rx(skb);
        }
        else
        {
            printk("Unable to allocate skb for IAPP update frame\n");
        }
    }

    // pass on the frame to higher layer applications
    return mgt_confirm( src_id, src_seq, dev_id, dev_seq, op, oid, data, len);
}

#endif /* HANDLE_IAPP_IN_DRIVER */


/****************************************************************
 *
 *   		GENERIC OID SET/CLEAR LED handlers
 *
 ***************************************************************/

#ifdef BLOB_V2
static int islmvc_gen_oid_led_set_hndl( unsigned int dev_id,   unsigned int dev_seq,
                             unsigned int src_id,   unsigned int src_seq,
                             unsigned int op,       unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;

    _DEBUG(DBG_FUNC_CALL | DBG_MGMT,
          "islmvc_gen_oid_les_set_hndl, src_id %d, src_seq %d, dev_id %d, dev_seq %d, op %d, oid %x, data %p, len %lu \n",
          src_id, src_seq, dev_id, dev_seq, op, oid, data, len);

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    if ( mgmt_set_led(dev, LED_SET) < 0 )
        return -1;

    if ( (mgt_response( src_id, src_seq, dev_id, dev_seq, PIMFOR_OP_RESPONSE, oid, data, len)) < 0 )
        return -1;

    return 0;
}


static int islmvc_gen_oid_led_clear_hndl( unsigned int dev_id,   unsigned int dev_seq,
                             unsigned int src_id,   unsigned int src_seq,
                             unsigned int op,       unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;

    _DEBUG(DBG_FUNC_CALL | DBG_MGMT,
          "islmvc_gen_oid_led_clear_hndl, src_id %d, src_seq %d, dev_id %d, dev_seq %d, op %d, oid %x, data %p, len %lu \n",
          src_id, src_seq, dev_id, dev_seq, op, oid, data, len);

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    if ( mgmt_set_led(dev, LED_CLEAR) < 0 )
        return -1;

    if ( (mgt_response( src_id, src_seq, dev_id, dev_seq, PIMFOR_OP_RESPONSE, oid, data, len)) < 0 )
        return -1;

    return 0;
}
#endif /* BLOB_V2 */


/****************************************************************
 *
 *    			Client table functions
 *
 ***************************************************************/

#ifdef ENABLE_CLIENT_TABLE_CODE

/**********************************************************************
 *  mac_hash
 *
 *  DESCRIPTION: Computes hash value for a given mac-address.
 *               Taken from bridging code by Lennert Buytenhek.
 *
 *  PARAMETERS:	 Pointer to mac address.
 *
 *  RETURN:      Hash value.
 *
 *  NOTE:        Inline function.
 *
 **********************************************************************/
static inline int mac_hash(unsigned char *mac)
{
    unsigned long x;

    //FIXME, use faster hash func.

    x = mac[0];
    x = (x << 2) ^ mac[1];
    x = (x << 2) ^ mac[2];
    x = (x << 2) ^ mac[3];
    x = (x << 2) ^ mac[4];
    x = (x << 2) ^ mac[5];

    x ^= x >> 8;

    return x & (MAC_HASH_SIZE - 1);
}


/*
 * Inline function that links client into the association table.
 */
static inline void client_link(struct net_local *lp, struct client_information *cip, int hash)
{
	cip->next_client = lp->assoc_table[hash];

        if(cip->next_client != NULL)
            cip->next_client->pprev_client = &cip->next_client; //FIXME, check neccesity...
        lp->assoc_table[hash] = cip;
        cip->pprev_client = &lp->assoc_table[hash];
}


/*
 * Inline function that removes client from the association table.
 */
static inline void client_unlink(struct client_information *cip)
{
	*(cip->pprev_client) = cip->next_client;
	if (cip->next_client != NULL)
		cip->next_client->pprev_client = cip->pprev_client;
	cip->next_client = NULL;
	cip->pprev_client= NULL;
}


/**********************************************************************
 *  mgmt_find_client
 *
 *  DESCRIPTION: Searches the association table of the device for the
 *               given client mac-address.
 *
 *  PARAMETERS:	 - pointer to device that holds the association table.
 *               - pointer to mac address we're searching for.
 *
 *  RETURN:      Pointer to client_information struct or
 *               NULL when client is not found.
 *
 *  NOTE:        Inline function.
 *
 **********************************************************************/
static inline struct client_information *mgmt_find_client(struct net_device *dev, char *address)
{
    struct net_local *lp = dev->priv;
    int hash = mac_hash(address);
    struct client_information *cip = lp->assoc_table[hash];

    while(cip != NULL)
    {
        if (!memcmp(cip->mac_addr, address, ETH_ALEN))
            return (cip);

        cip = cip->next_client;
    }

    _DEBUG(DBG_MGMT, "Address %s not found in assoc table... \n", hwaddr2string(address));

    return NULL;
}


static int mgmt_add_client(struct net_device *dev, char *address)
{
    struct client_information *cip;
    struct net_local *lp = dev->priv;
    int hash;

    hash = mac_hash(address);

    cip = lp->assoc_table[hash];

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_add_client, dev = %s, hash = %i, addr = %s \n",
                    dev->name, hash, hwaddr2string(address));

    while(cip != NULL)
    {
        if (!memcmp(cip->mac_addr, address, ETH_ALEN))
        {
            DEBUG(DBG_MGMT, "address %s already in assoc_table...\n", hwaddr2string(address));
            return -EINVAL;
        }

        cip = cip->next_client;
    }

    cip = kmalloc(sizeof(*cip), GFP_ATOMIC);
    if (cip == NULL)
    {
        printk("Unable to allocate memory for client \n");
        return -ENOMEM;
    }

    DEBUG(DBG_MGMT, "Address %s not yet in table, going to add it...", hwaddr2string(address));

    /* Fill in the client information fields... */
    memcpy(cip->mac_addr, address, ETH_ALEN);

    /* Link the client into the assoc table... */
    client_link(lp, cip, hash);

    return 0;

}


/*
 * mgmt_del_client
 *
 * Removes a client from the assoc table of the device.
 *
 * RETURN: 0 when client is removed from assocation table.
 *        -1 when client is not removed from assocation table.
 */
static int mgmt_del_client(struct net_device *dev, char *address)
{
    struct client_information *cip;
    struct net_local *lp = dev->priv;
    int hash;

    hash = mac_hash(address);

    cip = lp->assoc_table[hash];

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_del_client, dev = %s, hash = %i, addr = %s \n",
                    dev->name, hash, hwaddr2string(address));

    while(cip != NULL)
    {
        if (!memcmp(cip->mac_addr, address, ETH_ALEN))
        {
            DEBUG(DBG_MGMT, "found address %2.2x.%2.2x.%2.2x.%2.2x.%2.2x.%2.2x to remove from assoc_table...\n",
                   address[0], address[1], address[2], address[3], address[4], address[5]);

            client_unlink(cip);
            kfree(cip);
            return 0;
        }

        cip = cip->next_client;
    }

    DEBUG(DBG_MGMT, "Address %2.2x.%2.2x.%2.2x.%2.2x.%2.2x.%2.2x not found in assoc table... \n",
                    address[0], address[1], address[2], address[3], address[4], address[5]);

    return -1;
}

#endif /* ENABLE_CLIENT_TABLE_CODE */


/**********************************************************************
 *  mgmt_make_obj_attachment
 *
 *  DESCRIPTION: Calculates the size of all cached information elements.
 *               Mallocs a new obj attachment with this size.
 *               Copies all information elements into the data of this
 *               obj attachment.
 *
 *  PARAMETERS:	 Pointer to frame_attachment struct.
 *               Pointer to old object attachment for oa type and id.
 *
 *  RETURN:	 Pointer to new object attachment when succesfully malloced
 *               and data succesfully copied.
 *               NULL when information element == NULL or kmalloc failed.
 *
 **********************************************************************/
static struct obj_attachment *mgmt_make_obj_attachment(struct frame_attachment *fa, struct obj_attachment *old_oa)
{
    int i;

    int totalsize = 0;
    int offset = 0;
    struct information_element *ie = fa->ie_root;
    struct obj_attachment *oa = NULL;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_make_obj_attachment, fa %p, old_oa %p \n", fa, old_oa);

    if (ie == NULL)
    {
        printk(KERN_WARNING "No information elements for this attachment found. \n");
        return NULL;
    }

    /* We need to determine the total size of the information elements first. */
    for (i=0; (i <= MAX_IE_ELEMENTS && ie != NULL); i++, ie=ie->next)
        totalsize += (short)ie->size;

    /* Set the pointer back to it's roots... */
    ie = fa->ie_root;

    /* Malloc a buffer for the obj_attachment + the data. */
    if ( (oa = kmalloc( (sizeof(struct obj_attachment) + totalsize), GFP_KERNEL)) == NULL)
        return NULL;

    oa->type = old_oa->type;
    oa->id   = old_oa->id;
    oa->size = totalsize;

    /* Copy the information elements into the buffer. */
    for (i=0; (i <= MAX_IE_ELEMENTS && ie != NULL); i++, ie=ie->next)
     {
        if (ie->data == NULL || ie->size == 0)
            continue;
        memcpy((oa->data+offset), ie->data, ie->size);
        offset += ie->size;
    }

    return(oa);
}

/**********************************************************************
 *  mgmt_create_information_element
 *
 *  DESCRIPTION: Checks if we have an information element for this element id.
 *               If so, it replaces the current element data for the new one,
 *               or removes the element when size == 0.
 *               If no previous information element for this id was found,
 *               a new information element is malloced, and put into the list.
 *
 *  PARAMETERS:  Pointer to frame_attachment for this frame type.
 *               Pointer to the information element data. Contains element id.
 *               ie_size, which is the size of ie_data.
 *
 *  RETURN:	 0 when succesfully added or replaced.
 *               1 when succesfully removed.
 *               -ENOENT when the supplied element was not found in the cache
 *               -ENOSPC when MAX_IE_ELEMENTS was reached.
 *               -ENOMEM when we couldn't allocate memory for the information
 *               element or elements data.
 *
 **********************************************************************/
static int mgmt_create_information_element(struct frame_attachment *fa, char *ie_data, short ie_size)
{
    int i;
    unsigned char elem_id;

    struct information_element *ie   = fa->ie_root;
    struct information_element *prev = ie;

    /* The element id should be set as first parameter in the information element data. */
    elem_id  = *ie_data;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_create_information_element, fa %p, ie_data %p, ie_size %d\n", fa, ie_data, ie_size);

    /* Check if we have an id for this element */
    for (i=0; (ie != NULL && i <= MAX_IE_ELEMENTS); i++)
    {
        if (ie->elem_id == elem_id)
        {
            if (ie->data == NULL)
                DEBUG(DBG_MGMT, "Found information element, but it contains no data. \n");

            DEBUG(DBG_MGMT, "Found information element %d with size %d\n", ie->elem_id, ie->size);

            /* Valid information elements must have a size greater than 1. */
            if (ie_size <= 1)
            {
                DEBUG(DBG_MGMT, "Going to remove information element %d \n", elem_id);

                if (ie == fa->ie_root)
                    fa->ie_root = ie->next;
                else
                    prev->next = ie->next;
                kfree(ie);
                return 1;
            }
            else
            {
                /* If size is the same or less as the previous data size and we have a buffer, copy the data */
                if (ie->size <= ie_size && ie->data != NULL)
                {
                    ie->size = ie_size;
                    memcpy(ie->data, ie_data, ie->size);
                    DEBUG(DBG_MGMT, "IE size (%d) is same or less as the previous data size (%d). \n", ie_size, ie->size);
                }
                else
                {
                    /* We need to malloc a new area for the data. */
                    kfree(ie->data); /* Free the old area first! */

                    if ( (ie->data = kmalloc(ie_size, GFP_KERNEL)) == NULL)
                    {
                        printk(KERN_ERR "kmalloc of data for information element %d failed. \n", ie->elem_id);
                        ie->size = 0;
                        return -ENOMEM;
                    }
                    ie->size = ie_size;
                    memcpy(ie->data, ie_data, ie->size);
                    DEBUG(DBG_MGMT, "IE size (%d) is greater then the previous data size (%d). \n", ie_size, ie->size);
                }

                return 0;
            }
        }
        else
        {
            prev = ie;
            ie   = ie->next;
        }
    }

    if (i == MAX_IE_ELEMENTS)
    {
        printk(KERN_WARNING "Element not found. Maximum information elements (%d) reached. \n", MAX_IE_ELEMENTS);
        return -ENOSPC;
    }

    /* No element has been found in the cache mathing the supplied element ID */
    DEBUG(DBG_MGMT, "We didn't find an element... add it.\n");

    if (ie_size == 0) {
        /* If ie_size == 0, we're asked to remove this element.
           But since we didn't find an element in our list we can't do that. */
        DEBUG(DBG_MGMT, "Asked to remove information element (%d), but no previous element found. \n", elem_id);
        return -ENOENT;
    }

    /* Malloc a new information element. */
    if ( (ie = kmalloc(sizeof(struct information_element), GFP_KERNEL)) == NULL) {
        printk("Couldn't malloc memory for information element. \n");
        return -ENOMEM;
    }

    ie->elem_id = elem_id;
    ie->size    = 0;
    ie->data    = NULL;
    ie->next    = NULL;

    if ( (ie->data = kmalloc(ie_size, GFP_KERNEL)) == NULL)
    {
        printk(KERN_ERR "kmalloc of data for information element failed. \n");
        kfree(ie);
        ie->size = 0;
        return -ENOMEM;
    }
    ie->size = ie_size;
    memcpy(ie->data, ie_data, ie_size);

    /* Add the information element to the list. */
    DEBUG(DBG_MGMT, "Add the information element to the list.\n");
    ie->next    = fa->ie_root;
    fa->ie_root = ie;
    return 0;
}


/**********************************************************************
 *  find_frame_attach
 *
 *  DESCRIPTION: Tries to find a frame attachment for frame type and
 *               association id.
 *
 *  PARAMETERS:	 type - type of frame attachment.
 *               id   - association id.
 *
 *  RETURN:	 - pointer to frame attachment when found.
 *               - new frame attachment for this type when no frame attachment was
 *                 found and malloc of new frame attachment succeeded.
 *               - NULL when malloc failed.
 *
 **********************************************************************/
static struct frame_attachment *find_frame_attach(short type, short id)
{
    struct frame_attachment *fa   = frame_attach[type][id];

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "find_frame_attach, type %x, id(+1) %x, (fa = %p).\n", type, id, fa);

    if (fa == NULL) /* Not found */
    {
        /* No previous frame_attachment for this type and id. Malloc one. */
        if ( (fa = kmalloc(sizeof(struct frame_attachment), GFP_KERNEL)) == NULL)
        {
            printk(KERN_ERR "Couldn't malloc space for frame_attachment. \n");
            return NULL;
        }
        fa->next        = NULL;
        fa->frame_type  = type;
        fa->ie_root     = NULL;
        fa->all_ie_data = NULL;

        frame_attach[type][id] = fa;
        DEBUG(DBG_MGMT, "find_frame_attach, no frame attachment found. Added it to the list. fa = %p\n", fa);
        return(fa);
    }
    else {/* Found it */
        DEBUG(DBG_MGMT, "find_frame_attach, frame attachment found. fa = %p\n", fa);
        return(fa);

    }
}

/**********************************************************************
 *  mgmt_cache_frame_attachment
 *
 *  DESCRIPTION: Information elements in frame attachments must be cached
 *               in the driver, as the MVC won't do that for us.
 *
 *  PARAMETERS:	 data       - pointer to the struct obj_attachment.
 *               data_len   - length of the
 *               dev        - pointer to device we need to send information
 *                            elements to.
 *
 *  RETURN:	 0  when caching and sending the information elements to
 *                  the MVC succeeded.
 *               -1 when caching and sending information elements to the
 *                  MVC failed.
 *
 **********************************************************************/
static int mgmt_cache_frame_attachment(struct net_device *dev, char *data)
{
    int error;
    struct obj_attachment   del_oa;

    struct frame_attachment *fa     = NULL;
    struct obj_attachment   *oa     = NULL;
    struct obj_attachment   *old_oa = (struct obj_attachment *)data;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_cache_frame_attachment, dev %s, data %p \n", dev->name, data);

    /* Find the corresponding frame_attachment for this type and id. If none exists, one is malloced for us.*/
    if ( (fa = find_frame_attach(old_oa->type, old_oa->id+1)) == NULL) { /* id+1, as id can be -1. We map -1 to 0 this way. */
        printk(KERN_ERR "Couldn't malloc space for frame attachment. \n");
        return -ENOMEM;
    }

    /* Create information element in this frame attachment. */
    if (error = mgmt_create_information_element(fa, old_oa->data, old_oa->size),
        (error == -ENOSPC) || (error == -ENOMEM)) {
        printk(KERN_WARNING "Error %d. Couldn't create information element in frame attachment (type %d, id %d). \n", error, old_oa->type, old_oa->id);
        return -1;
    }
    else if ( error == -ENOENT )
    {
        DEBUG(DBG_MGMT, "Couldn't find information element in frame attachment (data %d). \n", old_oa->data);
    }

    /* Okay, we have a valid list of information elements now. Place them in one obj attachment. */
    oa = mgmt_make_obj_attachment(fa, old_oa);

    if (oa == NULL) {
        printk(KERN_ERR "Couldn't make new object attachment for information elements. \n");
        return -ENOMEM;
    }

    /* Delete/invalidate old attachment data on the MVC. */
    del_oa.type = fa->frame_type;
    del_oa.id   = old_oa->id;
    del_oa.size = 0;

    if (islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_ATTACHMENT, (long*)&del_oa, sizeof(struct obj_attachment)) < BER_NONE)
        printk(KERN_WARNING "Couldn't delete old attachment data (type = %x) from MVC.\n", fa->frame_type);

    /* Now send the new attachment to the MVC. */
    error = islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_ATTACHMENT, (long*)oa, oa->size);

    kfree(oa); /* No need to save it, as all data is available in the information elements list. */

    if (error < 0) {
        DEBUG(DBG_MGMT, "Error %d setting DOT11_OID_ATTACHMENT. \n", error);
        return -1;
    }

    return 0;
}


/**********************************************************************
 *  mgmt_set_oid_attachment
 *
 *  DESCRIPTION: Information elements in frame attachments for management
 *               frame types must be cached in the driver,
 *               as the MVC won't do that for us.
 *
 *  PARAMETERS:	 data       - pointer to the struct obj_attachment.
 *               dev        - pointer to device we need to send information
 *                            elements to.
 *               length     - Length of obj_attachment.
 *
 *  RETURN:	 0  when caching and sending the information elements to
 *                  the MVC succeeded.
 *               -1 when caching and sending information elements to the
 *                  MVC failed.
 *
 **********************************************************************/
static int mgmt_set_oid_attachment(char *data, struct net_device *dev, unsigned long length)
{
    int err;

    struct obj_attachment *oa = (struct obj_attachment *)data;

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_set_oid_attachment, data %p, dev %s, length %lu, (type = %x) \n",
          data, dev->name, length, oa->type);

    /* Only cache information elements for management frame types.
       Send all other frame attachments through un-cached.*/
    if ((oa->type & 0x0C /* D11FT_RESERVED */) != 0) {
        DEBUG(DBG_MGMT, "mgmt_set_oid_attachment, No management frame type. Send through uncached. \n");
        return (islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_ATTACHMENT, (long*)data, length));
    }

    if ( (err = mgmt_cache_frame_attachment(dev, data)) != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *  mgmt_set_led
 *
 *  DESCRIPTION: Sets or clears the LED for device dev.
 *
 *  PARAMETERS:  dev - pointer to device we want to set the LED for.
 *               mode - set or clear the LED.
 *
 *  RETURN:	 0  when setting/clearing the LED succeeded
 *               -1 when setting/clearing the LED failed.
 *
 *  NOTE:       This code is ISL3893 specific.
 *
 **********************************************************************/
#ifdef BLOB_V2
int mgmt_set_led(struct net_device *dev, int mode)
{
    long data, mask, led;
    long bank = 3; /* GP bank 3 */

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "mgmt_set_led for %s, mode: %s led.\n",
          dev->name, (mode == LED_SET ? "set" : "clear"));

    /* Determine which LED to set/clear. */
    switch (GET_DEV_NR(dev->base_addr))
    {
        case ETH_LAN: {
            led = GPIO3_MASK_LED3;
            break;
        }
        case ETH_WLAN: {
            /* Beware, to set/clear wireless LED, ledconfig must be either 0, or 
               wireless interface must be stopped. Otherwise, the MVC is driving 
               the WLAN LED, and change is not reflected on the LED.
               It's the users responsibility to do this.
            */   
            led = GPIO3_MASK_LED1;            
            break;
        }
        case ETH_WAN: {
            led = GPIO3_MASK_LED2;
            break;
        }
        default: {
            // FIXME: For demo LMAC crashes flash the USR1 LED !
            led = GPIO3_MASK_LED0;
//            return -1;
        }
    }

    mask = gpio_set(led);

    if (mode == LED_SET) {
        data = gpio_set(led);
    } else if (mode == LED_CLEAR) {
        data = gpio_clr(led);
    } else {
        printk("Unknown mode (%d) for mgmt_set_led.\n", mode);
        return -1;
    }

    /* Select the GPIO bank for this LED */
    if (islmvc_send_blob_msg(OPSET, BLOB_OID_GPIOBANK, &bank, sizeof(long)) < 0)
        return -1;
    /* Set the GPIO Data Direction for this LED (ddr == mask) */
    if (islmvc_send_blob_msg(OPSET, BLOB_OID_GPIODDR, &mask, sizeof(long)) < 0)
        return -1;
    /* Set the GPIO write mask for this LED*/
    if (islmvc_send_blob_msg(OPSET, BLOB_OID_GPIOWM, &mask, sizeof(long)) < 0)
        return -1;
    /* Write the data to the GPIO bank */
    if (islmvc_send_blob_msg(OPSET, BLOB_OID_GPIODATA, &data, sizeof(long)) < 0)
        return -1;
    
    return 0;
}
#endif /* BLOB_V2 */


/****************************************************************
 *								*
 *    			End of management functions		*
 *								*
 ***************************************************************/


/***************************************************************
*								*
*    		Start of generic functions		        *
*								*
***************************************************************/


void do_hard_print(char *string)
{

#ifdef CONFIG_POLDEBUG_PRINTK

       struct msg_data debug_msg;

       debug_msg.data = string;
       debug_msg.length = strlen(string) + 1;
       debug_msg.max = 0;

       dev_write(DEVICE_DEBUG, &debug_msg);

#endif /* CONFIG_POLDEBUG_PRINTK */

}


/*
 * Returns a pointer to the human-readable hardware address of
 * the client. Buffer is static from function and overwritten
 * with each call
 */
char *hwaddr2string(char *a)
{
    static char buf[18];

    snprintf(buf, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
	    a[0], a[1], a[2], a[3], a[4], a[5]);

    return(buf);
}


/****************************************************************
 *								*
 *    		    End of generic functions                    *
 *								*
 ***************************************************************/

#ifdef BLOB_V2
EXPORT_SYMBOL(mgmt_set_led);
#endif
EXPORT_SYMBOL(pda_get_mac_address);
