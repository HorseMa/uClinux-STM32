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

#include <linux/config.h>
#include <linux/netdevice.h>
#ifdef MODULE
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#else
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT
#endif

#include <asm/io.h>
#ifdef CONFIG_ARCH_ISL3893
#include <asm/arch/blobcalls.h>
#endif
#include <linux/pci.h>

#include "isl_gen.h"
#include "isl_38xx.h"
#include "islpci_mgt.h"
#include "bloboid.h"      // additional types and defs for isl38xx fw

#ifdef CONFIG_ARCH_ISL3893
#include "islpci_hotplug.h"
#define INTERSIL_EVENTS
#define WDS_LINKS
#endif

#ifdef INTERSIL_EVENTS
#include "isl_mgt.h"
#endif

#ifdef WDS_LINKS
#include "isl_wds.h"
#endif

#ifdef WIRELESS_EVENTS
#if WIRELESS_EXT > 12
#include <net/iw_handler.h>
#endif
#endif

/******************************************************************************
        Global variable definition section
******************************************************************************/
int pc_debug = VERBOSE;
int init_mode = CARD_DEFAULT_MODE;
int init_channel = CARD_DEFAULT_CHANNEL;
int init_bsstype = CARD_DEFAULT_BSSTYPE;
int init_wep = CARD_DEFAULT_WEP;
int init_filter = CARD_DEFAULT_FILTER;
int init_wds = CARD_DEFAULT_WDS;
int init_authen = CARD_DEFAULT_AUTHEN;
int init_dot1x = CARD_DEFAULT_DOT1X;
int init_mlme = CARD_DEFAULT_MLME_MODE;
char *init_ssid = NULL;

#ifdef INTERSIL_EVENTS
extern unsigned int src_seq;
#endif
extern int firmware_uploaded;

#ifdef MODULE
MODULE_PARM( pc_debug, "i" );
MODULE_PARM( init_mode, "i" );
MODULE_PARM( init_channel, "i" );
MODULE_PARM( init_bsstype, "i" );
MODULE_PARM( init_ssid, "s" );
MODULE_PARM( init_wep, "i" );
MODULE_PARM( init_filter, "i" );
/* MODULE_PARM( init_wds, "i" ); */
MODULE_PARM( init_authen, "i" );
MODULE_PARM( init_dot1x, "i" );
MODULE_PARM( init_mlme, "i" );
#endif

DECLARE_WAIT_QUEUE_HEAD(mgmt_queue_wait);

/******************************************************************************
    Frame (re)format functions
******************************************************************************/
void pimfor_encode_header(int operation, unsigned long oid,
                                 int device_id, int flags, int length,
                                 pimfor_header * header)
{
    // byte oriented members
    header->version = PIMFOR_VERSION;
    header->operation = operation;
    header->device_id = device_id;
    header->flags = flags;

    // word oriented members with byte order depending on the flags
    if (flags & PIMFOR_FLAG_LITTLE_ENDIAN)
    {
        // use little endian coding
        header->oid[3] = oid >> 24;
        header->oid[2] = oid >> 16;
        header->oid[1] = oid >> 8;
        header->oid[0] = oid;
        header->length[3] = 0;
        header->length[2] = 0;
        header->length[1] = length >> 8;
        header->length[0] = length;
    }
    else
    {
        // use big endian coding
        header->oid[0] = oid >> 24;
        header->oid[1] = oid >> 16;
        header->oid[2] = oid >> 8;
        header->oid[3] = oid;
        header->length[0] = 0;
        header->length[1] = 0;
        header->length[2] = length >> 8;
        header->length[3] = length;
    }
}

void pimfor_decode_header(pimfor_header * header, int *operation,
                                 unsigned long *oid, int *device_id,
                                 int *flags, int *length)
{
    // byte oriented members
    *operation = header->operation;
    *device_id = header->device_id;
    *flags = header->flags;

    // word oriented members with byte order depending on the flags
    if (*flags & PIMFOR_FLAG_LITTLE_ENDIAN)
    {
        // use little endian coding
        *oid = ((int) header->oid[3] << 24) |
            ((int) header->oid[2] << 16) |
            ((int) header->oid[1] << 8) | ((int) header->oid[0]);
        *length = ((int) header->length[1] << 8) |
            ((int) header->length[0]);
    }
    else
    {
        // use big endian coding
        *oid = ((int) header->oid[0] << 24) |
            ((int) header->oid[1] << 16) |
            ((int) header->oid[2] << 8) | ((int) header->oid[3]);
        *length = ((int) header->length[2] << 8) |
            ((int) header->length[3]);
    }
}

/******************************************************************************
    Memory Management functions
******************************************************************************/
void islpci_init_queue(queue_root * queue)
{
    queue->host_address = (u32) NULL;
    queue->dev_address = (u32) NULL;
    queue->size = 0;
    queue->fragments = 0;
}

int islpci_put_queue(void *device, queue_root * queue, queue_entry * entry)
{
    queue_entry *last;

    // the size and next members are both accessed from kernel code and from
    // interrupt handler, disable the update interrupt as short as possible
    isl38xx_disable_update_interrupt(device);

    // check whether the queue is empty or not
    if (queue->size == 0)
    {
        // queue is empty, add the entry at the start of the queue
        queue->next = (void *) entry;
        queue->last = (void *) entry;
    }
    else
    {
        // the queue is not empty, add the entry at the back
        last = (queue_entry *) queue->last;
        last->next = (void *) entry;
        queue->last = (void *) entry;
    }

    // increment the queue size
    queue->size++;

    // enable the update interrupt again
    isl38xx_enable_update_interrupt(device);

    return 0;
}

int islpci_get_queue(void *device, queue_root * queue,
                     queue_entry ** entry)
{
    queue_entry *first;

    // the size and next members are both accessed from kernel code and from
    // interrupt handler, disable the update interrupt as short as possible
    isl38xx_disable_update_interrupt(device);

    // check whether the queue is empty or not
    if (queue->size == 0)
    {
        // queue is empty, nothing to get
        // enable the update interrupt again
        isl38xx_enable_update_interrupt(device);

        return -1;
    }
    else
    {
        // the queue is not empty, get the entry at the start
        first = (queue_entry *) queue->next;
        queue->next = first->next;
        first->next = NULL;
        *entry = first;
    }

    // decrement the queue size
    queue->size--;

    // enable the update interrupt again
    isl38xx_enable_update_interrupt(device);

    return 0;
}

int islpci_queue_size(void *device, queue_root * queue)
{
    int qsize;

    // disable the update interrupt of the device
    isl38xx_disable_update_interrupt(device);

    // read the size of the queue from the root entry
    qsize = (int) queue->size;

    // enable the update interrupt of the device
    isl38xx_enable_update_interrupt(device);

    return qsize;
}


/******************************************************************************
    Driver Configuration functions
******************************************************************************/
void islpci_mgt_initialize( islpci_private *private_config )
{
    char configmode[4] = { INL_CONFIG_MANUALRUN, 0, 0, 0 };
    char cardmode[4] = { 0, 0, 0, 0 };
    char bsstype[4] =  { 0, 0, 0, 0 };
    char channel[4] =  { 0, 0, 0, 0 };
    char authen[4] = { 0, 0, 0, 0 };
    char invoke[4] = { 0, 0, 0, 0 };
    char exunencrypt[4] = { 0, 0, 0, 0 };
    char dot1xen[4] = { 0, 0, 0, 0 };
	char mlmemode[4] = { 0, 0, 0, 0 };
    struct obj_key key = { DOT11_PRIV_WEP, 0, "" };
    struct obj_ssid essid_obj = { sizeof( CARD_DEFAULT_SSID ) - 1,
                                CARD_DEFAULT_SSID };
    struct obj_buffer psm_buffer = { cpu_to_le32(PSM_BUFFER_SIZE), 0 };

    // store all device initialization management frames in the
    // management transmit shadow queue
    cardmode[0] = (char) init_mode;
    channel[0] = (char) init_channel;
    channel[1] = (char) (init_channel >> 8);
    bsstype[0] = (char) init_bsstype;
    authen[0] = (char) init_authen;
    invoke[0] = (char) init_wep;
    exunencrypt[0] = (char) init_filter;
    dot1xen[0] = (char) init_dot1x;
    mlmemode[0] = (char) init_mlme;
    if( init_ssid != NULL )
    {
        // parameter specified when loading the module
        essid_obj.length = strlen( init_ssid );
        strcpy( essid_obj.octets, init_ssid );
    }
    psm_buffer.base = (void *) cpu_to_le32( private_config->device_psm_buffer );
    if( init_wds )
    {
        // enable wds mode in the host interface
        configmode[0] |= INL_CONFIG_WDS;
    }

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "cardmode: %i \n", init_mode );
    DEBUG(SHOW_TRACING, "channel: %i \n", init_channel );
    DEBUG(SHOW_TRACING, "bsstype: %i \n", init_bsstype );
    DEBUG(SHOW_TRACING, "wep: %i \n", init_wep );
    DEBUG(SHOW_TRACING, "authen: %i \n", init_authen );
    DEBUG(SHOW_TRACING, "filter: %i \n", init_filter );
    DEBUG(SHOW_TRACING, "wds: %i \n", init_wds );
    DEBUG(SHOW_TRACING, "ssid: %i %s \n", essid_obj.length, essid_obj.octets );
    DEBUG(SHOW_TRACING, "psm: %p %li\n", psm_buffer.base, psm_buffer.size );
#endif

    // store all device initialization management frames in the
    // management transmit shadow queue

    //  get firmware configuration data
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        GEN_OID_MACADDRESS, 0, NULL, 6, 0);

    // set the configuration mode to manual run & wds option
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        OID_INL_CONFIG, 0, &configmode[0], 4, 0);

    // set the mode for stopping the device
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        OID_INL_MODE, 0, &cardmode[0], 4, 0);

    // set the bsstype
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_BSSTYPE, 0, &bsstype[0], 4, 0);

    // set the channel
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_CHANNEL, 0, &channel[0], 4, 0);

    // set the SSID
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_SSID, 0, (void *) &essid_obj, sizeof(struct obj_ssid), 0);

	// use the essid object to override the SSID 
	essid_obj.length = sizeof( CARD_DEFAULT_SSIDOVERRIDE ) - 1;
	memcpy( essid_obj.octets, CARD_DEFAULT_SSIDOVERRIDE, essid_obj.length );
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_SSIDOVERRIDE,
		0, (void *) &essid_obj, sizeof(struct obj_ssid), 0);

    // set the PSM buffer object
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_PSMBUFFER, 0, (void *) cpu_to_le32( &psm_buffer ),
        sizeof(struct obj_buffer), 0);

    // set the authentication enable option
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_AUTHENABLE, 0, &authen[0], 4, 0);

    // set the privacy invoked mode
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_PRIVACYINVOKED, 0, &invoke[0], 4, 0);

    // set the exunencrypted object
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_EXUNENCRYPTED, 0, &exunencrypt[0], 4, 0);

    // set default key 1
    strcpy( key.key, CARD_DEFAULT_KEY1 );
    key.length = strlen(CARD_DEFAULT_KEY1) > sizeof(key.key) ?
        sizeof(key.key) : strlen(CARD_DEFAULT_KEY1);
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_DEFKEY1, 0,
        &key, sizeof(struct obj_key), 0);

    // set default key 2
    strcpy( key.key, CARD_DEFAULT_KEY2 );
    key.length = strlen(CARD_DEFAULT_KEY2) > sizeof(key.key) ?
        sizeof(key.key) : strlen(CARD_DEFAULT_KEY2);
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_DEFKEY2, 0,
        &key, sizeof(struct obj_key), 0);

    // set default key 3
    strcpy( key.key, CARD_DEFAULT_KEY3 );
    key.length = strlen(CARD_DEFAULT_KEY3) > sizeof(key.key) ?
        sizeof(key.key) : strlen(CARD_DEFAULT_KEY3);
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_DEFKEY3, 0,
        &key, sizeof(struct obj_key), 0);

    // set default key 4
    strcpy( key.key, CARD_DEFAULT_KEY4 );
    key.length = strlen(CARD_DEFAULT_KEY4) > sizeof(key.key) ?
        sizeof(key.key) : strlen(CARD_DEFAULT_KEY4);
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_DEFKEY4, 0,
        &key, sizeof(struct obj_key), 0);

    // set the dot1x enable object
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_DOT1XENABLE, 0, &dot1xen[0], 4, 0);

    // set the mlme mode object
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_MLMEAUTOLEVEL, 0, &mlmemode[0], 4, 0);

#ifndef CONFIG_ARCH_ISL3893
    // set the mode again for starting the device
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        OID_INL_MODE, 0, &cardmode[0], 4, 0);
#endif

    // call the transmit function for triggering the transmitting of the
    // first frame in the shadow queue
    islpci_mgt_transmit( private_config );
}

/******************************************************************************
    Device Interface functions
******************************************************************************/
int islpci_mgt_transmit(islpci_private * private_config)
{
    int fragments, counter;
    isl38xx_control_block *control_block = private_config->control_block;
    isl38xx_fragment *fragment;
    u32 index, in_queue, in_shadowq;
    queue_entry *entry;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_mgt_transmit \n");
#endif

    // check whether there is a pending response in the receive queue which
    // needs to be handled first before applying a new frame
    if( in_queue = isl38xx_in_queue(control_block, ISL38XX_CB_RX_MGMTQ),
        in_queue != 0 )
    {
        // receive queue not empty
#if VERBOSE > SHOW_ERROR_MESSAGES
        DEBUG(SHOW_TRACING, "receive queue not empty\n");
#endif
        return 0;
    }

    // read the number of entries in the transmit queue and check whether the
    // transmit queue is empty for receiving the next pimfor frame
    if( in_queue = isl38xx_in_queue(control_block, ISL38XX_CB_TX_MGMTQ),
        in_queue != 0 )
    {
        // transmit queue not empty
#if VERBOSE > SHOW_ERROR_MESSAGES
        DEBUG(SHOW_TRACING, "transmit queue not empty\n");
#endif
        // trigger the device
        pci_map_single( private_config->pci_device, private_config->control_block, 
                        sizeof(struct isl38xx_cb), PCI_DMA_BIDIRECTIONAL );
        isl38xx_trigger_device( &private_config->powerstate, private_config->remapped_device_base );

        return 0;
    }

    // first check whether a clean up should take place on the shadow queue
    while( private_config->index_mgmt_tx > in_queue )
    {
        // free the first entry in the shadow queue which contains the
        // PIMFOR frame transmitted to the device
        islpci_get_queue(private_config->remapped_device_base,
                &private_config->mgmt_tx_shadowq, &entry);
        islpci_put_queue(private_config->remapped_device_base,
                &private_config->mgmt_tx_freeq, entry);

        // decrement the real management index
        private_config->index_mgmt_tx--;
    }

    // check whether there is at least one frame in the shadowq
    if( in_shadowq = islpci_queue_size( private_config->remapped_device_base,
        &private_config->mgmt_tx_shadowq ), !in_shadowq )
    {
        // no entries in the shadowq, quit
#if VERBOSE > SHOW_ERROR_MESSAGES
        DEBUG(SHOW_TRACING, "shadowq empty, no more frames\n");
#endif
        return 0;
    }

    // there is a frame in the shadowq, get its fragments count
    entry = (queue_entry *) &private_config->mgmt_tx_shadowq;
    entry = (queue_entry *) entry->next;
    fragments = entry->fragments;

    // transmit the fragment(s) by placing them in the transmit queue
    for (counter = 0; counter < fragments; counter++)
    {
        u16 fragment_flags;

        // determine the fragment flags depending on the nr of fragments
        fragment_flags = fragments - counter > 1 ? FRAGMENT_FLAG_MF : 0;

        // get a pointer to the destination fragment using the frame counter
        // and link it with the free queue entry data block
        index = le32_to_cpu(control_block->driver_curr_frag[ISL38XX_CB_TX_MGMTQ]) %
                ISL38XX_CB_MGMT_QSIZE;
        fragment = &control_block->tx_data_mgmt[index];

#if VERBOSE > SHOW_ERROR_MESSAGES
        DEBUG(SHOW_TRACING, "Entry address %p\n", entry);
        DEBUG(SHOW_TRACING, "Index in transmit queue %i\n", index);
        DEBUG(SHOW_TRACING, "Fragment address %p\n", fragment);
#endif

        fragment->address = cpu_to_le32(entry->dev_address);
        fragment->size = cpu_to_le16(entry->size);
        fragment->flags = cpu_to_le16(fragment_flags);

        // increment the index pointer, wrapping is not done on queue but
        // on variable type range
        add_le32p(&control_block->driver_curr_frag[ISL38XX_CB_TX_MGMTQ], 1);

        // increment the queue entry pointer
        entry = (queue_entry *) entry->next;
    }

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Frame with [%i] fragments added to transmit queue \n",
      fragments);
#endif

    // increment the read management queue index
    private_config->index_mgmt_tx += fragments;

    // flush the dcache write buffer
    pci_map_single( private_config->pci_device, private_config->control_block, 
                    sizeof(struct isl38xx_cb), PCI_DMA_TODEVICE );

    // trigger the device
    isl38xx_trigger_device( &private_config->powerstate, private_config->remapped_device_base );

    return 0;
}

/*
       // ====================================================
        if( private_config->resetdevice == 1 )
        {
            DEBUG(SHOW_FUNCTION_CALLS,
                    "Reset Device flag set, quit IRQ handler\n" );

            DEBUG(SHOW_QUEUE_INDEXES,
                "CB drv Qs: [%i][%i][%i][%i][%i][%i]\n",
                le32_to_cpu(private_config->control_block->driver_curr_frag[0]),
                le32_to_cpu(private_config->control_block->driver_curr_frag[1]),
                le32_to_cpu(private_config->control_block->driver_curr_frag[2]),
                le32_to_cpu(private_config->control_block->driver_curr_frag[3]),
                le32_to_cpu(private_config->control_block->driver_curr_frag[4]),
                le32_to_cpu(private_config->control_block->driver_curr_frag[5])
                );

            DEBUG(SHOW_QUEUE_INDEXES,
                "CB dev Qs: [%i][%i][%i][%i][%i][%i]\n",
                le32_to_cpu(private_config->control_block->device_curr_frag[0]),
                le32_to_cpu(private_config->control_block->device_curr_frag[1]),
                le32_to_cpu(private_config->control_block->device_curr_frag[2]),
                le32_to_cpu(private_config->control_block->device_curr_frag[3]),
                le32_to_cpu(private_config->control_block->device_curr_frag[4]),
                le32_to_cpu(private_config->control_block->device_curr_frag[5])
                );


            return 0;
        }
        // ====================================================
*/



int islpci_mgt_queue(islpci_private * private_config,
                                  int operation, unsigned long oid,
                                  int flags, void *data, int length,
                                  int use_tunnel_oid)
{
    int fragments, counter;
    u32 source, destination;
    queue_entry *entry;
    char firmware[64];

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_mgt_queue \n");
#endif

#ifdef CONFIG_ARCH_ISL3893
    /* check whether the firmware has been uploaded in the device */
    if( !firmware_uploaded )
    {
        DEBUG(SHOW_ERROR_MESSAGES, "islpci_mgt_queue failed: Firmware not uploaded\n" );
        return -1;    
    }
#endif

    // determine the amount of fragments needed for the frame in case of a SET
    if (use_tunnel_oid)
    {
        fragments = (length + 2 * PIMFOR_HEADER_SIZE) / MGMT_FRAME_SIZE;
        if ((length + 2 * PIMFOR_HEADER_SIZE) % MGMT_FRAME_SIZE != 0)
            fragments++;
    }
    else
    {
        fragments = (length + PIMFOR_HEADER_SIZE) / MGMT_FRAME_SIZE;
        if ((length + PIMFOR_HEADER_SIZE) % MGMT_FRAME_SIZE != 0)
        fragments++;
    }

    // check whether data is actually written to the device
    if (operation == PIMFOR_OP_GET)
    {
        // operation is get, no data is actually written, reset the fragments
        // counter to send a single fragment
        fragments = 1;
    }

    // check whether the number of fragments doesn't exceed the maximal possible
    // amount of frames in the transmit queue
    if( fragments > ISL38XX_CB_MGMT_QSIZE )
    {
        // Oops, the fragment count exceeds the maximal value
        DEBUG(SHOW_ERROR_MESSAGES, "Error: Frame with [%i] fragments is too "
            "large \n", fragments );
        return -1;
    }

    // check whether there is enough room for the fragment(s) in the freeq
    if( islpci_queue_size( private_config->remapped_device_base,
        &private_config->mgmt_tx_freeq ) < fragments )
    {
        // Oops, the management freeq does not have enough entries
        DEBUG(SHOW_ERROR_MESSAGES, "Error: Management FreeQ not enough entries "
            "for [%i] fragments \n", fragments );
        return -1;
    }

    // queue the frame(s) in the transmit shadow queue
    for (counter = 0, source = (u32) data; counter < fragments; counter++)
    {
        u16 fragment_size = 0;

        // get an entry from the free queue for hosting the frame
        islpci_get_queue(private_config->remapped_device_base,
            &private_config->mgmt_tx_freeq, &entry);

        // set the destination address to the entry data block for building
        // the PIMFOR frame there
        destination = entry->host_address;

#if VERBOSE > SHOW_ERROR_MESSAGES
        DEBUG(SHOW_TRACING, "Entry address %p\n", entry);
        DEBUG(SHOW_TRACING, "Destination address %x\n", destination);
        DEBUG(SHOW_TRACING, "Destination for device %x\n", entry->dev_address);
#endif

        // place the PIMFOR header in the first fragment
        if (counter == 0)
        {
            // check whether the frame should have a tunnel oid
            if (use_tunnel_oid)
            {
                // start with a tunnel oid header
                pimfor_encode_header(PIMFOR_OP_SET, OID_INL_TUNNEL,
                     PIMFOR_DEV_ID_MHLI_MIB, flags,
                     length + PIMFOR_HEADER_SIZE,
                     (pimfor_header *) destination);

                // set the fragment size and increment the destin pointer
                fragment_size += PIMFOR_HEADER_SIZE;
                destination += PIMFOR_HEADER_SIZE;
            }

            // create the header directly in the fragment data area
            pimfor_encode_header(operation, oid, PIMFOR_DEV_ID_MHLI_MIB,
                 flags, length, (pimfor_header *) destination);

#if VERBOSE > SHOW_ERROR_MESSAGES
            // display the buffer contents for debugging
            display_buffer((char *) destination, 12);
#endif

            // set the fragment size and increment the destin pointer
            fragment_size += PIMFOR_HEADER_SIZE;
            destination += PIMFOR_HEADER_SIZE;
        }

        // add the data to the fragment 
        if( length != 0 && source != 0 )
        {
            int size = (length > (MGMT_FRAME_SIZE - fragment_size)) ?
                MGMT_FRAME_SIZE - fragment_size : length;
            memcpy((void *) destination, (void *) source, size);

            // set the fragment size and flag members
            fragment_size += size;

#if VERBOSE > SHOW_ERROR_MESSAGES
            // display the buffer contents for debugging
            display_buffer((char *) destination, 12);
#endif

            // update the source pointer and the data length
            source += size;
            length -= size;
        }

        // save the number of fragments still to go in the frame together with
        // the fragment size
        entry->fragments = fragments - counter;
        entry->size = fragment_size;

        // store the entry in the tx shadow queue
        islpci_put_queue(private_config->remapped_device_base,
            &private_config->mgmt_tx_shadowq, entry);
    }

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Frame with [%i] fragments added to shadow tx queue \n",
        fragments);
#endif

    return 0;
}

int islpci_mgt_receive(islpci_private * private_config)
{
    struct net_device *nw_device = private_config->my_module;
    isl38xx_control_block *control_block = private_config->control_block;
    pimfor_header *received_header;
    char *received_data;
    int device_id, operation, flags, length;
    unsigned long oid;
    queue_entry *entry, *new_entry;

#ifdef WIRELESS_EVENTS
    union iwreq_data wrq;
#endif

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_mgt_receive \n");
#endif

    // get the received response from the shadow queue
    islpci_get_queue(private_config->remapped_device_base,
                &private_config->mgmt_rx_shadowq, &entry);

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Entry address 0x%p\n", entry);
    DEBUG(SHOW_TRACING, "Entry device address 0x%x\n", entry->dev_address);
#endif

    // determine how the process the fragment depending on the flags in the
    // PIMFOR header, check for tunnel oid PIMFOR frames
    received_header = (pimfor_header *) entry->host_address;
    if (pimfor_decode_header(received_header, &operation, &oid, &device_id,
        &flags, &length), oid == OID_INL_TUNNEL)
    {
#if VERBOSE > SHOW_ERROR_MESSAGES
        DEBUG(SHOW_PIMFOR_FRAMES, "PIMFOR: Tunnel OID frame received \n");
#endif

        // PMFOR frame in fragment has the tunnel oid, redo the header decode
        received_header = (pimfor_header *) (entry->host_address +
            PIMFOR_HEADER_SIZE);
        pimfor_decode_header(received_header, &operation, &oid, &device_id,
            &flags, &length);
    }

    /* The device ID from the pimfor packet received from the MVC is always 0
     * This is not the device_id we want to give to the applications above
     * so lets get the device_id using the private net_local structure
     */
    device_id = private_config->my_module->ifindex;
    
    received_data = (char *) received_header + PIMFOR_HEADER_SIZE;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_PIMFOR_FRAMES,
        "PIMFOR: op %i, oid 0x%lx, device %i, flags 0x%x length 0x%x \n",
        operation, oid, device_id, flags, length);

    // display the buffer contents for debugging
    display_buffer((char *) received_header, sizeof(pimfor_header));
    display_buffer(((char *) received_header) + PIMFOR_HEADER_SIZE, length );
#endif

    // PIMFOR frame created by this driver, handle its contents
    // check whether the frame contains driver initialization data
    if (oid == GEN_OID_MACADDRESS)
    {
        // a MAC address response message, store its contents
        if ((operation == PIMFOR_OP_RESPONSE) && (length == 6))
        {
            // copy the MAC address received
            memcpy(nw_device->dev_addr, received_data, 6);
        }
    }
    else if (oid == OID_INL_MODE)
    {
        // the mode response message, store its contents
        if ((operation == PIMFOR_OP_RESPONSE) && (length == 4))
        {
            // copy the low byte which is at the first byte location
            private_config->mode = (int) *received_data;
        }
    }
    else if (oid == GEN_OID_LINKSTATE)
    {
        // the mode response message, store its contents
        if ((operation == PIMFOR_OP_RESPONSE) && (length == 4))
        {
            // copy the low byte which is at the first byte location
            private_config->linkstate = (int) *received_data;
        }
    }
	else if ( (oid >= DOT11_OID_DEAUTHENTICATE) && 
		(oid <= DOT11_OID_REASSOCIATEEX) )
	{
		// received an mlme reply or trap message
		islpci_mgt_mlme( oid, received_data );
	}

    // perform a check on the freeq whether the entry can be queued to other
    // queues or must be hold to make sure there are enough
    if( islpci_get_queue(private_config->remapped_device_base,
        &private_config->mgmt_rx_freeq, &new_entry) == 0)
    {
        // succeeded in getting a new entry from the freeq
        // refresh the device mgmt queue entry
        control_block->rx_data_mgmt[private_config->index_mgmt_rx].address =
            cpu_to_le32(new_entry->dev_address);

        // put the new entry in the mgmt rx shadow queue
        islpci_put_queue(private_config->remapped_device_base,
                &private_config->mgmt_rx_shadowq, new_entry);

        // deliver the held entry to the appropriate queue
        if (flags & PIMFOR_FLAG_APPLIC_ORIGIN)
        {
            // PIMFOR frame originated by higher application, send the entire
            // fragment up without processing

#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "PIMFOR Application Originated \n");
#endif

            // send the entry to the PIMFOR receive queue for further processing
            islpci_put_queue(private_config->remapped_device_base,
                &private_config->pimfor_rx_queue, entry);
        }
        else
        {
            // check whether the frame concerns a response, error or trap
            if (operation == PIMFOR_OP_TRAP)
            {
#if VERBOSE > SHOW_ERROR_MESSAGES
                DEBUG(SHOW_TRAPS,
                    "TRAP: oid 0x%lx, device %i, flags 0x%x length %i\n",
                    oid, device_id, flags, length);
#endif

#ifdef INTERSIL_EVENTS
                mgt_response( 0, src_seq++, DEV_NETWORK_WLAN, device_id,
                              PIMFOR_OP_TRAP, oid, received_data, length );
#endif
                // check whether the TRAP should be deleted or queued depending
                // on the queue size and whether the queue is read
                if( islpci_queue_size( private_config->remapped_device_base,
                    &private_config->trap_rx_queue ) > MAX_TRAP_RX_QUEUE )
                {
                    // free the entry to the freeq
                    islpci_put_queue(private_config->remapped_device_base,
                            &private_config->mgmt_rx_freeq, entry);
                }
                else
                {
                    // send the entry to the TRAP receive q
                    islpci_put_queue(private_config->remapped_device_base,
                        &private_config->trap_rx_queue, entry);

#ifdef WIRELESS_EVENTS
                    // signal user space application(s) that a TRAP is queued
                    // use the AP MAC address get ioctl for signalling
                        wrq.ap_addr.sa_family = ARPHRD_ETHER;
                            memcpy( wrq.ap_addr.sa_data, nw_device->dev_addr, 6 );
                            wireless_send_event( nw_device, SIOCGIWAP, &wrq, "ISIL" );
#endif // WIRELESS_EVENTS
                }
            }
            else
            {
                // received either a repsonse or an error
                // send the entry to the IOCTL receive q for further processing
                islpci_put_queue(private_config->remapped_device_base,
                        &private_config->ioctl_rx_queue, entry);

                // signal the waiting procs that a response has been received
                wake_up_interruptible(&mgmt_queue_wait);

#if VERBOSE > SHOW_ERROR_MESSAGES
                DEBUG(SHOW_TRACING, "Wake up interruptible Mgmt Queue\n" );
#endif
            }
        }
    }
    else
    {
        // not succeeded, use the held one and drop its contents
        // refresh the device mgmt queue entry
        control_block->rx_data_mgmt[private_config->index_mgmt_rx].address =
            cpu_to_le32(entry->dev_address);

        // put the old entry back in the mgmt rx shadow queue
        islpci_put_queue(private_config->remapped_device_base,
            &private_config->mgmt_rx_shadowq, entry);
    }

    // increment the driver read pointer
    add_le32p(&control_block->driver_curr_frag[ISL38XX_CB_RX_MGMTQ], 1);

    // trigger the device
//    pci_map_single( private_config->pci_device, private_config->control_block, 
//                    sizeof(struct isl38xx_cb), PCI_DMA_BIDIRECTIONAL );
    isl38xx_trigger_device( &private_config->powerstate, private_config->remapped_device_base );

    // increment and wrap the read index for the rx management queue
    private_config->index_mgmt_rx = (private_config->index_mgmt_rx + 1) %
        ISL38XX_CB_MGMT_QSIZE;

    return 0;
}

int islpci_mgt_response(islpci_private * private_config, long oid, int* oper,
                                 void **data, long *dlen, queue_entry **qentry )
{
    queue_entry *entry;
    pimfor_header *received_header;
    int q_size, counter, device_id, operation, flags, rlength;
    int temp = ISL38XX_MAX_WAIT_CYCLES;
    unsigned long received_oid;

    DECLARE_WAITQUEUE(wait, current);

    // check for other processes already waiting for response
    if (private_config->processes_in_wait == 0)
    {
        // no process in the wait queue, flush the queue
        while (islpci_queue_size(private_config->remapped_device_base,
            &private_config->ioctl_rx_queue))
        {
            // flush each entry remaining in the queue
            islpci_get_queue(private_config->remapped_device_base,
                &private_config->ioctl_rx_queue, &entry);
            islpci_put_queue(private_config->remapped_device_base,
                &private_config->mgmt_rx_freeq, entry);
        }
    }

    // trigger the transmission of the management shadowq before going to sleep
    islpci_mgt_transmit( private_config );

    // add the process to the wait queue and increment the process counter
    add_wait_queue(&mgmt_queue_wait, &wait);
    private_config->processes_in_wait++;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Process enter sleep mode\n");
#endif

    while (1)
    {
        // go to sleep with the alarm bells on sharp
        interruptible_sleep_on_timeout(&mgmt_queue_wait, ISL38XX_WAIT_CYCLE);

#if VERBOSE > SHOW_ERROR_MESSAGES
        DEBUG(SHOW_TRACING, "Woken up from a nap\n" );
#endif

        // hello there, I got woken up because there is either a response out
        // there or the bells went off, check whether the lock is up
        while (private_config->ioctl_queue_lock)
        {
            // the lock is up, another process is reading the ioctl rx queue
            schedule_timeout(1 * HZ / 10);
        }

        // It's my turn, set the lock rightaway
        private_config->ioctl_queue_lock = 1;

        // check the IOCTL receive queue for responses
        if( q_size = islpci_queue_size(private_config->remapped_device_base,
            &private_config->ioctl_rx_queue), q_size != 0 )
        {
            // something in the queue, check every entry in the queue
            for (counter = 0; counter < q_size; counter++)
            {
                // get the entry from the queue and read the header
                islpci_get_queue(private_config->remapped_device_base,
                    &private_config->ioctl_rx_queue, &entry);
                received_header = (pimfor_header *) entry->host_address;

                // check whether the oid response is for me
                if( pimfor_decode_header(received_header, &operation,
                    &received_oid, &device_id, &flags,&rlength ),
                    oid == received_oid )
                {
#if VERBOSE > SHOW_ERROR_MESSAGES
                    DEBUG(SHOW_TRACING, "IOCTL response found in queue.\n");
#endif

                    // found my response, return it all
                    *data = (void *) received_header + PIMFOR_HEADER_SIZE;
                    *qentry = entry;
                    *dlen = rlength;
		    *oper = operation;

#if VERBOSE > SHOW_ERROR_MESSAGES
                    DEBUG(SHOW_TRACING, "Response source 0x%p\n", *data );
#endif
                    temp = 0;

                    // continue and leave the sleepingroom
                    goto leave;
                }
                else
                {
#if VERBOSE > SHOW_ERROR_MESSAGES
                  DEBUG(SHOW_TRACING, "Skip received oid %lx \n", received_oid );
#endif
                }

                // put the entry back in the queue
                islpci_put_queue(private_config->remapped_device_base,
                    &private_config->ioctl_rx_queue, entry);
            }

            // checked all entries in the queue without success
            temp = -ETIMEDOUT;
            goto leave;
        }
        else
        {
            // nothing in the queue, that's got to be a timeout
            // decrement the retry timer stored in temp and check
            if( temp--, !temp )
            {
                // max number of retries reached, really a problem
                temp = -EBUSY;
                goto leave;
            }

            // try another wakup on the card and wait for response
            // when the device is sleeping
            if( private_config->powerstate == ISL38XX_PSM_POWERSAVE_STATE )
            {
                // device is in powersave, trigger the device for wakeup
#if VERBOSE > SHOW_ERROR_MESSAGES
                DEBUG(SHOW_TRACING, "Device wakeup retrigger %i\n", temp );
#endif
                // assert the Wakeup interrupt in the Device Interrupt Register
                writel( ISL38XX_DEV_INT_WAKEUP,
                    private_config->remapped_device_base + ISL38XX_DEV_INT_REG);
            }

            // release the lock
            private_config->ioctl_queue_lock = 0;
        }
    }

leave:
    private_config->processes_in_wait--;
    private_config->ioctl_queue_lock = 0;
    current->state = TASK_RUNNING;
    remove_wait_queue(&mgmt_queue_wait, &wait);
    return temp;
}

void islpci_mgt_release( islpci_private * private_config, queue_entry *entry )
{
    // free the entry is not NULL
    if( entry != NULL )
        islpci_put_queue(private_config->remapped_device_base,
            &private_config->mgmt_rx_freeq, entry );
}


int islpci_mgt_mlme( unsigned long oid, char *received_data )
{
    struct obj_mlme *mlme = (struct obj_mlme *) received_data;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG( SHOW_TRAPS, "mlme: oid %lx \n", oid );
    DEBUG( SHOW_TRAPS, "mlme: address %02x:%02x:%02x:%02x:%02x:%02x\n",
		mlme->address[0] & 0xFF, mlme->address[1] & 0xFF, mlme->address[2] & 0xFF, 
		mlme->address[3] & 0xFF, mlme->address[4] & 0xFF, mlme->address[5] & 0xFF);
    DEBUG( SHOW_TRAPS, "mlme: id %x, state %x, code %x \n", mlme->id, 
		mlme->state, mlme->code );
#endif
	
    switch( oid )
    {
        case DOT11_OID_AUTHENTICATE:
            // check whether a station is authenticating
            if( mlme->state == DOT11_STATE_AUTHING )
            {
                // STA is requesting to to be authenticated

                // check the code of the object and handle it

                // generate a response to the station
                // positive: generate an Authenticate Confirm frame
                // negative: generate a Deauthenicate Request frame
            }
            else
            {
                // notification that a STA has authenticated

                // update the STA list to be maintained in the driver
            }
            break;

        case DOT11_OID_DEAUTHENTICATE:
            // read the STA address from the mlme object structure

            // check the code of the object and handle it

            // update the STA list to be maintained in the driver

            break;

        case DOT11_OID_ASSOCIATE:
            // check whether a station is associating
            if( mlme->state == DOT11_STATE_ASSOCING )
            {
                // STA is requesting to to be authenticated

                // check the code of the object and handle it

                // generate a response to the station
                // positive: generate an Associate Confirm frame
                // negative: generate a Disassociate Request frame
            }
            else
            {
                // notification that a STA has authenticated

                // update the STA list to be maintained in the driver
            }

            break;


        case DOT11_OID_DISASSOCIATE:
            // read the STA address from the mlme object structure

            // update the STA list to be maintained in the driver

            break;

		case DOT11_OID_BEACON:
			// recevied a beacon indicate, do nothing
#if VERBOSE > SHOW_ERROR_MESSAGES
		    DEBUG( SHOW_TRAPS, "TRAP: Beacon Indicate\n" );
#endif
			break;
			
		case DOT11_OID_PROBE:
			// recevied a probe indicate, do nothing
#if VERBOSE > SHOW_ERROR_MESSAGES
		    DEBUG( SHOW_TRAPS, "TRAP: Probe Indicate\n" );
#endif
			break;
			
        default:
            // oid is not a TRAP from the MLME
            return 0;
    }

    return 0;

}

/******************************************************************************
        WDS Link Handlers
******************************************************************************/

#ifdef INTERSIL_EVENTS
int islpci_mgt_indication( unsigned int dev_type, unsigned int dev_seq,
                           unsigned int src_id, unsigned int src_seq,
                           unsigned int op, unsigned int oid, void *data, unsigned long len)
{
    islpci_private *private_config;
    struct net_device *dev;
    char *datap;
    unsigned long dlen;
    queue_entry *entry = NULL;
    int err = 0;
    int operation;

    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );
	
    private_config = dev->priv;
    
    islpci_mgt_queue(private_config, op, oid, 0, data, len, 0);
    if ( ( err = islpci_mgt_response( private_config, oid, &operation, 
    	(void **) &datap, &dlen, &entry ) ) < 0 ) {
        islpci_mgt_release( private_config, entry );
        printk("islpci_mgt_response returned %d\n", err );
        return -1;
    }

    if ( dlen > len) {
        dlen = len;
    }
    memcpy( data, datap, dlen );
    
    islpci_mgt_release( private_config, entry );

    if ( (mgt_response( src_id, src_seq, dev_type, dev_seq, operation,
    	 oid, data, dlen)) < 0)
        return -1;

    return 0;
}
#endif /* INTERSIL_EVENTS */

#ifdef WDS_LINKS

struct wds_link_msg {
    unsigned char mac[6];
    char name[32];
} __attribute__ ((packed));

int islpci_wdslink_add_hndl( unsigned int dev_type, unsigned int dev_seq,
                             unsigned int src_id, unsigned int src_seq,
                             unsigned int op, unsigned int oid, void *data, unsigned long len){
    struct net_device *dev;
    struct wds_link_msg *wlmp;
    islpci_private *private_config;
    char *datap;
    unsigned long dlen;
    queue_entry *entry = NULL;
    int err = 0;
    int operation;

    if ( len != sizeof(struct wds_link_msg)) 
        return -1;
    
    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    private_config = dev->priv;

    wlmp = (struct wds_link_msg *)data;
    
    printk ("islpci_wdslink_add_hndl(%x:%x:%x:%x:%x:%x)\n", wlmp->mac[0], 
    	wlmp->mac[1], wlmp->mac[2], wlmp->mac[3], wlmp->mac[4], wlmp->mac[5] );
    
    if ( add_wds_link(dev, private_config->wdsp, wlmp->mac, wlmp->name) < 0 ) {
        printk("islpci_wdslink_add_hndl:add_wds_link\n");
    }
    
    islpci_mgt_queue(private_config, op, oid, 0, wlmp->mac, 6, 0);
    if ( ( err = islpci_mgt_response( private_config, oid, &operation, 
    	(void **) &datap, &dlen, &entry ) ) < 0 ) {
        printk("islpci_mgt_response returned %d\n", err );
    }

    islpci_mgt_release( private_config, entry );
    
    if ( (mgt_response( src_id, src_seq, dev_type, dev_seq, operation, 
    	oid, data, len)) < 0)
        return -1;

    return 0;
}

int islpci_wdslink_del_hndl( unsigned int dev_type, unsigned int dev_seq,
                             unsigned int src_id, unsigned int src_seq,
                             unsigned int op, unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;
    struct wds_link_msg *wlmp;
    islpci_private *private_config;
    char *datap;
    unsigned long dlen;
    queue_entry *entry = NULL;
    int err = 0;
    int operation;

    if ( len != sizeof(struct wds_link_msg)) 
        return -1;
    
    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    private_config = dev->priv;

    wlmp = (struct wds_link_msg *)data;
    
    printk ("islpci_wdslink_del_hndl(%x:%x:%x:%x:%x:%x)\n", wlmp->mac[0], 
    	wlmp->mac[1], wlmp->mac[2], wlmp->mac[3], wlmp->mac[4], wlmp->mac[5] );
    
    if ( del_wds_link(dev, private_config->wdsp, wlmp->mac, wlmp->name ) < 0 ) {
        printk("islpci_wdslink_del_hndl:del_wds_link\n");
    }
    
    islpci_mgt_queue(private_config, op, oid, 0, wlmp->mac, 6, 0);
    if ( ( err = islpci_mgt_response( private_config, oid, &operation, 
    	(void **) &datap, &dlen, &entry ) ) < 0 ) {
        printk("islpci_mgt_response returned %d\n", err );
    }

    islpci_mgt_release( private_config, entry );
    
    if ( (mgt_response( src_id, src_seq, dev_type, dev_seq, operation, 
    	oid, data, len)) < 0)
        return -1;

    return 0;
}
#endif /* WDS_LINKS */

#ifdef INTERSIL_EVENTS

#define PDR_END              0x0000
#define PDR_INTERFACE_LIST   0x1001

struct pdr_struct {
   unsigned short   length;
   unsigned short   code;
   unsigned char    data[2];
} __attribute__ ((packed));

static struct pdr_struct *pda_find_pdr (unsigned char *pda, unsigned short code )
{
   unsigned short     offset = 0;
   struct pdr_struct *pdr;
   
   pdr = (struct pdr_struct *) pda;
   
   while (pdr->code != PDR_END) {
      if (pdr->code == code) {
         return pdr;
      } else {
         offset = pdr->length + 1;
         /* invalid PDA: past 8k or invalid PdrLength */
         if ((offset > 8192) || (pdr->length == 65535) || (pdr->length == 0)) {
             return (struct pdr_struct *)0;
         }
         pdr = (struct pdr_struct *)pda;
      }
   }

   if (code == PDR_END) {
       return pdr;
   } else {
     return (struct pdr_struct *)0;
   }
}

/* mgt_indication_handler ( DEV_NETWORK, dev->name, BLOB_OID_EXT_INTERFACE_LIST, islpci_interface_list_hndl ); */

int islpci_interface_list_hndl( unsigned int dev_type, unsigned int dev_seq,
                                unsigned int src_id, unsigned int src_seq,
                                unsigned int op, unsigned int oid, void *data, unsigned long len)
{
    struct net_device *dev;
    islpci_private *private_config;
    unsigned char *datap;
    unsigned long dlen;
    queue_entry *entry = NULL;
    struct pdr_struct *pdr;
    int err = 0;
    int operation;
printk("Find dev_get_by_index(%d)\n", dev_seq);
    if ( ! (dev = dev_get_by_index(dev_seq)) )
        return -1;
	dev_put( dev );

    private_config = dev->priv;

    printk ("islpci_pda_hndl()\n" );
    
    islpci_mgt_queue(private_config, PIMFOR_OP_GET, BLOB_OID_NVDATA, 0, 0, 0, 0);
    if ( ( err = islpci_mgt_response( private_config, oid, &operation, 
    	(void **) &datap, &dlen, &entry ) ) < 0 ) {
        printk("islpci_mgt_response returned %d\n", err );
    }
    
    if ( ( pdr = pda_find_pdr( datap, PDR_INTERFACE_LIST ) ) && ( pdr->length <= len ) ) {
        memcpy( data, pdr->data, pdr->length );
    } else {
        memset( data, 0, len );
    }

    islpci_mgt_release( private_config, entry );
    
    if ( (mgt_response( src_id, src_seq, dev_type, dev_seq, operation,
    	oid, data, len)) < 0)
        return -1;

    return 0;
}

#endif
