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

#include <linux/version.h>
#ifdef MODULE
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#else
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT
#endif

#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include "isl_gen.h"
#include "isl_38xx.h"
#include "islpci_eth.h"

#ifdef WDS_LINKS
#include "isl_mgt.h"
#include "isl_wds.h"
#endif

/******************************************************************************
        Global variable definition section
******************************************************************************/
extern int pc_debug;
extern int init_wds;

/******************************************************************************
    Network Interface functions
******************************************************************************/
void islpci_eth_cleanup_transmit( islpci_private *private_config,
    isl38xx_control_block *control_block )
{
	struct sk_buff *skb;
	u32 index;
	
	// compare the control block read pointer with the free pointer
	while( private_config->free_data_tx !=
		le32_to_cpu( control_block->device_curr_frag[ISL38XX_CB_TX_DATA_LQ]) )
	{
		// read the index of the first fragment to be freed
		index = private_config->free_data_tx % ISL38XX_CB_TX_QSIZE;

		// check for holes in the arrays caused by multi fragment frames 
		// searching for the last fragment of a frame
    	if( private_config->pci_map_tx_address[index] != (dma_addr_t) NULL )
		{
			// entry is the last fragment of a frame
			// free the skb structure and unmap pci memory
			skb = private_config->data_low_tx[index];

#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG( SHOW_TRACING, "cleanup skb %p skb->data %p skb->len %u truesize %u\n ", 
                skb, skb->data, skb->len, skb->truesize );
#endif

			pci_unmap_single( private_config->pci_device, 
                private_config->pci_map_tx_address[index], 
				skb->len, PCI_DMA_TODEVICE );
       		dev_kfree_skb( skb );
		}
		
		// increment the free data low queue pointer
		private_config->free_data_tx++;
	}		
}

int islpci_eth_transmit(struct sk_buff *skb, struct net_device *nwdev)
{
    islpci_private *private_config = nwdev->priv;
    isl38xx_control_block *control_block = private_config->control_block;
    isl38xx_fragment *fragment;
    u32 index, counter, pci_map_address;
    int fragments;
    int frame_size;
    int offset;
    struct sk_buff *newskb;
    int newskb_offset;
	unsigned long flags;
	unsigned char wds_mac[6];
#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_eth_transmit \n");
#endif

    // lock the driver code
    driver_lock( &private_config->slock, &flags );

    // check whether the deviec is running
//      if( !netif_running( nwdev ) )
//      {
//              DEBUG( SHOW_ERROR_MESSAGES, "%s: Tx on stopped device!\n",
//                     nwdev->name);
//              return 1;
//      }

//      tx_timeout_check( nwdev, islpci_eth_tx_timeout );
//      skb_tx_check( nwdev, skb );

    // determine the amount of fragments needed to store the frame
    memset (wds_mac, 0, 6);
#ifdef WDS_LINKS
    check_skb_for_wds(skb, wds_mac);
#ifdef ISLPCI_ETH_DEBUG
    if ( wds_mac[0] || wds_mac[1] || wds_mac[2] || wds_mac[3] || wds_mac[4] || wds_mac[5] ) {
        printk("islpci_eth_transmit:check_skb_for_wds ! %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n", 
               wds_mac[0], wds_mac[1], wds_mac[2], wds_mac[3], wds_mac[4], wds_mac[5] );
    } else {
        printk("islpci_eth_transmit:check_skb_for_wds %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n", 
               wds_mac[0], wds_mac[1], wds_mac[2], wds_mac[3], wds_mac[4], wds_mac[5] );
    }
#endif
#endif
    
    frame_size = skb->len < ETH_ZLEN ? ETH_ZLEN : skb->len;
    if( init_wds ) frame_size += 6;
    fragments = frame_size / MAX_FRAGMENT_SIZE;
    fragments += frame_size % MAX_FRAGMENT_SIZE == 0 ? 0 : 1;


#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Fragments needed for frame %i\n", fragments);
#endif

    // check whether the destination queue has enough fragments for the frame
    if (ISL38XX_CB_TX_QSIZE - isl38xx_in_queue( control_block, 
		ISL38XX_CB_TX_DATA_LQ ) < fragments )
    {
        // Error, cannot add the frame because the queue is full
        DEBUG(SHOW_ERROR_MESSAGES, "Error: Queue [%i] not enough room\n",
	        ISL38XX_CB_TX_DATA_LQ);

        // trigger the device by setting the Update bit in the Device Int reg
        writel(ISL38XX_DEV_INT_UPDATE, private_config->remapped_device_base + 
            ISL38XX_DEV_INT_REG);
        udelay(ISL38XX_WRITEIO_DELAY);

        // unlock the driver code
        driver_unlock( &private_config->slock, &flags );
        return -EBUSY;
    }

    // Check alignment and WDS frame formatting. The start of the packet should
    // be aligned on a 4-byte boundary. If WDS is enabled add another 6 bytes
    // and add WDS address information
    if( ((int) skb->data & 0x03) | init_wds )
    {
        // get the number of bytes to add and re-allign
        offset = (int) skb->data & 0x03;
		offset += init_wds ? 6 : 0;		

		// check whether the current skb can be used 
        if (!skb_cloned(skb) && (skb_tailroom(skb) >= offset))
        {
            unsigned char *src = skb->data;

#if VERBOSE > SHOW_ERROR_MESSAGES 
	        DEBUG(SHOW_TRACING, "skb offset %i wds %i\n", offset, init_wds );
#endif

			// allign the buffer on 4-byte boundary
            skb_reserve(skb, (int) skb->data & 0x03 );
            if( init_wds )
            {
            	// wds requires an additional address field of 6 bytes
				skb_put( skb, 6 ); 
#ifdef ISLPCI_ETH_DEBUG
                printk("islpci_eth_transmit:wds_mac\n" );
#endif			    
                memmove( skb->data+6, src, skb->len );
                memcpy( skb->data, wds_mac, 6 );
            }
            else
			{
                memmove( skb->data, src, skb->len );
			}
			
#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG(SHOW_TRACING, "memmove %p %p %i \n", skb->data, src,
                skb->len );
#endif
        }
        else
        {
            newskb = dev_alloc_skb( init_wds ? skb->len+6 : skb->len );
            newskb_offset = (int) newskb->data & 0x03;

            // Check if newskb->data is aligned
            if( newskb_offset )
                skb_reserve(newskb, newskb_offset);

            skb_put(newskb, init_wds ? skb->len+6 : skb->len );
            if( init_wds )
            {
                memcpy( newskb->data+6, skb->data, skb->len );
                memcpy( newskb->data, wds_mac, 6 );
#ifdef ISLPCI_ETH_DEBUG
                printk("islpci_eth_transmit:wds_mac\n" );
#endif
            }
            else
                memcpy( newskb->data, skb->data, skb->len );

#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG(SHOW_TRACING, "memcpy %p %p %i wds %i\n", newskb->data,
                skb->data, skb->len, init_wds );
#endif

            newskb->dev = skb->dev;
            dev_kfree_skb(skb);
            skb = newskb;
        }
    }

    // display the buffer contents for debugging
#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG( SHOW_BUFFER_CONTENTS, "\ntx %p ", skb->data );
    display_buffer((char *) skb->data, skb->len );
#endif

	// map the skb buffer to pci memory for DMA operation
	pci_map_address = pci_map_single( private_config->pci_device, 
		(void *) skb->data, skb->len, PCI_DMA_TODEVICE );
    if( pci_map_address == (dma_addr_t) NULL )
	{
    	// error mapping the buffer to device accessable memory address
	    DEBUG(SHOW_ERROR_MESSAGES, "Error map DMA addr, data lost\n" );

	    // free the skbuf structure before aborting
    	dev_kfree_skb(skb);

        // unlock the driver code
        driver_unlock( &private_config->slock, &flags );
	    return -EIO;
    }

	// place each fragment in the control block structure and store in the last
	// needed fragment entry of the pci_map_tx_address and data_low_tx arrays 
	// the skb frame information
    for (counter = 0; counter < fragments; counter++ )
	{
		// get a pointer to the target control block fragment
	    index = (counter + le32_to_cpu(
	        control_block->driver_curr_frag[ISL38XX_CB_TX_DATA_LQ])) %
    	    ISL38XX_CB_TX_QSIZE;			
	    fragment = &control_block->tx_data_low[index];
	
		// check whether this frame fragment is the last one
		if( counter == fragments-1 )
		{
			// the fragment is the last one, add the streaming DMA mapping for 
			// proper PCI bus operation
    		private_config->pci_map_tx_address[index] = 
                (dma_addr_t) pci_map_address;
			
			// store the skb address for future freeing 
			private_config->data_low_tx[index] = skb;
		}
		else
		{
			// the fragment will be followed by more fragments
			// clear the pci_map_tx_address and data_low_tx entries
		    private_config->pci_map_tx_address[index] = (dma_addr_t) NULL;
			private_config->data_low_tx[index] = NULL;
		}
			
		// set the proper fragment start address and size information
        fragment->address = cpu_to_le32( pci_map_address + 
			counter*MAX_FRAGMENT_SIZE );
        fragment->size = cpu_to_le16( frame_size > MAX_FRAGMENT_SIZE ?
            MAX_FRAGMENT_SIZE : frame_size );
        fragment->flags = cpu_to_le16( frame_size > MAX_FRAGMENT_SIZE ? 1 : 0 );
        frame_size -= MAX_FRAGMENT_SIZE;
	}

	// ready loading all fragements to the control block and setup pci mapping
	// update the control block interface information
    add_le32p(&control_block->driver_curr_frag[ISL38XX_CB_TX_DATA_LQ],
        fragments);

    // check whether the data tx queue is full now
    if( ISL38XX_CB_TX_QSIZE - isl38xx_in_queue( control_block, 
		ISL38XX_CB_TX_DATA_LQ ) < ISL38XX_MIN_QTHRESHOLD )
    {
        // stop the transmission of network frames to the driver
        netif_stop_queue(nwdev);

        // set the full flag for the data low transmission queue
        private_config->data_low_tx_full = 1;
    }

    // trigger the device
    isl38xx_trigger_device( &private_config->powerstate,
        private_config->remapped_device_base );

    // set the transmission time
    nwdev->trans_start = jiffies;
    private_config->statistics.tx_packets++;
    private_config->statistics.tx_bytes += skb->len;

    // cleanup the data low transmit queue
    islpci_eth_cleanup_transmit( private_config, private_config->control_block );

    // unlock the driver code
    driver_unlock( &private_config->slock, &flags );
    return 0;
}

int islpci_eth_receive(islpci_private * private_config)
{
    struct net_device *nw_device = private_config->my_module;
    struct net_device *wds_dev = NULL;
    struct wds_net_local *wds_lp;
    isl38xx_control_block *control_block = private_config->control_block;
#ifdef WDS_LINKS
    struct wds_priv *wdsp = private_config->wdsp;
#endif    
    struct sk_buff *skb;
    u16 size;
    u32 index, offset;
    unsigned char *src;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_eth_receive \n");
#endif

    // the device has written an Ethernet frame in the data area
    // of the sk_buff without updating the structure, do it now
	index = private_config->free_data_rx % ISL38XX_CB_RX_QSIZE;
    size = le16_to_cpu(control_block->rx_data_low[index].size);
	skb = private_config->data_low_rx[index];
    offset = ((u32) le32_to_cpu(control_block->rx_data_low[index].address) - 
        (u32) skb->data ) & 3;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG( SHOW_TRACING, "frq->addr %x skb->data %p skb->len %u offset %u truesize %u\n ", 
        control_block->rx_data_low[private_config->free_data_rx].address,
		skb->data, skb->len, offset, skb->truesize );
#endif

    // delete the streaming DMA mapping before processing the skb
    pci_unmap_single( private_config->pci_device, 
        private_config->pci_map_rx_address[index],
        MAX_FRAGMENT_SIZE+2, PCI_DMA_FROMDEVICE );

	// update the skb structure and allign the buffer
    skb_put( skb, size );
    if( offset )
    {
		// shift the buffer allocation offset bytes to get the right frame
        skb_pull( skb, 2 );
		skb_put( skb, 2 );
    }
    
#if VERBOSE > SHOW_ERROR_MESSAGES 
    // display the buffer contents for debugging
    DEBUG( SHOW_BUFFER_CONTENTS, "\nrx %p ", skb->data );
    display_buffer((char *) skb->data, skb->len );
#endif

    // check whether WDS is enabled and whether the data frame is a WDS frame

    if( init_wds )
    {
        // WDS enabled, check for the wds address on the first 6 bytes of the buffer
#ifdef WDS_LINKS
        wds_dev = wds_find_device( skb->data, wdsp ); 
#ifdef ISLPCI_ETH_DEBUG
        if ( wds_dev ) {
            printk("islpci_eth_receive:wds_find_device ! %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n", 
                   skb->data[0], skb->data[1], skb->data[2], skb->data[3], skb->data[4], skb->data[5] );
        } else {
            printk("islpci_eth_receive:wds_find_device %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n", 
                   skb->data[0], skb->data[1], skb->data[2], skb->data[3], skb->data[4], skb->data[5] );
        }
#endif
#endif
        src = skb->data+6;
        memmove( skb->data, src, skb->len-6 );
        skb_trim( skb, skb->len-6 );
    }

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Fragment size %i in skb at %p\n", size, skb);
    DEBUG(SHOW_TRACING, "Skb data at %p, length %i\n", skb->data, skb->len);

    // display the buffer contents for debugging
    DEBUG( SHOW_BUFFER_CONTENTS, "\nrx %p ", skb->data );
    display_buffer((char *) skb->data, skb->len );
#endif

    // do some additional sk_buff and network layer parameters
    skb->dev = nw_device;
    skb->protocol = eth_type_trans(skb, nw_device);
    skb->ip_summed = CHECKSUM_NONE;
    private_config->statistics.rx_packets++;
    private_config->statistics.rx_bytes += size;

#ifdef WDS_LINKS
        if ( wds_dev != NULL )
        {
            wds_lp = (struct wds_net_local *) wds_dev->priv;
            wds_lp->stats.rx_packets++;
            wds_lp->stats.rx_bytes += skb->len;            
            skb->dev = wds_dev;
        }
#endif
    // deliver the skb to the network layer
#ifdef ISLPCI_ETH_DEBUG
    printk("islpci_eth_receive:netif_rx %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n", 
           skb->data[0], skb->data[1], skb->data[2], skb->data[3], skb->data[4], skb->data[5] );
#endif
    netif_rx(skb);

    // increment the read index for the rx data low queue
    private_config->free_data_rx++;

    // add one or more sk_buff structures
    while( index = le32_to_cpu( 
		control_block->driver_curr_frag[ISL38XX_CB_RX_DATA_LQ] ), 
		index - private_config->free_data_rx < ISL38XX_CB_RX_QSIZE )
    {
        // allocate an sk_buff for received data frames storage
	    // include any required allignment operations
        if (skb = dev_alloc_skb(MAX_FRAGMENT_SIZE+2), skb == NULL)
        {
            // error allocating an sk_buff structure elements
            DEBUG(SHOW_ERROR_MESSAGES, "Error allocating skb \n");
            break;
        }

		// store the new skb structure pointer
		index = index % ISL38XX_CB_RX_QSIZE;
		private_config->data_low_rx[index] = skb;

#if VERBOSE > SHOW_ERROR_MESSAGES 
        DEBUG( SHOW_TRACING, "new alloc skb %p skb->data %p skb->len %u index %u truesize %u\n ", 
            skb, skb->data, skb->len, index, skb->truesize );
#endif

        // set the streaming DMA mapping for proper PCI bus operation
		private_config->pci_map_rx_address[index] = pci_map_single(
			private_config->pci_device, (void *) skb->data, MAX_FRAGMENT_SIZE+2, 
			PCI_DMA_FROMDEVICE );
        if( private_config->pci_map_rx_address[index] == (dma_addr_t) NULL )
        {
            // error mapping the buffer to device accessable memory address
            DEBUG(SHOW_ERROR_MESSAGES, "Error mapping DMA address\n");

            // free the skbuf structure before aborting
            dev_kfree_skb((struct sk_buff *) skb );
            break;
        }

        // update the fragment address
        control_block->rx_data_low[index].address = cpu_to_le32( (u32)
		 	private_config->pci_map_rx_address[index] );

        // increment the driver read pointer
        add_le32p(&control_block->driver_curr_frag[ISL38XX_CB_RX_DATA_LQ], 1 );
    }

    // trigger the device
    isl38xx_trigger_device( &private_config->powerstate,
        private_config->remapped_device_base );

    return 0;
}

void islpci_eth_tx_timeout(struct net_device *nwdev)
{
    islpci_private *private_config = nwdev->priv;
    struct net_device_stats *statistics = &private_config->statistics;
//    unsigned long flags;
//    int error = 0;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_eth_tx_timeout \n");
#endif

    // lock the driver code
    //driver_lock( &private_config->slock, &flags );

    // increment the transmit error counter
    statistics->tx_errors++;

/*
    // reset the device and restore its configuration
    if (error = isl38xx_reset(private_config), error != 0)
    {
        DEBUG(SHOW_ERROR_MESSAGES, "Error %d on TX timeout card reset!\n",
            error);
    }
    else
    {
        DEBUG(SHOW_ERROR_MESSAGES, "Driver stopped TX/RX ......\n");

//        nwdev->trans_start = jiffies;
//        netif_wake_queue( nwdev );
    }
*/

    // unlock the driver code
    //driver_unlock( &private_config->slock, &flags );
    return;
}
