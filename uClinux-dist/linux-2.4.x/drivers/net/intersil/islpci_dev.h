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

#ifndef _ISLPCI_DEV_H
#define _ISLPCI_DEV_H


#include <linux/netdevice.h>

#ifdef CONFIG_ARCH_ISL3893
#define INTERSIL_EVENTS 
#define WDS_LINKS
#endif

#include "isl_38xx.h"
#ifdef WDS_LINKS
#include "isl_wds.h"
#endif

typedef struct
{
    u32 host_address;                   // mem address for the host
    u32 dev_address;                    // mem address for the device
    u16 size;                           // size of memory block or queue
    u16 fragments;                      // nr of fragments in data part
    void *next;                         // entry pointer to next in list
    void *last;                         // for quick queue handling
} queue_entry, queue_root;

struct net_local
{
    // PCI bus allocation & configuration members
    u8 pci_bus;                         // PCI bus id location
    u8 pci_dev_fn;                      // PCI bus func location
    u8 pci_irq;                         // PCI irq line
    u16 pci_dev_id;	    	    	    // PCI device id

    struct pci_dev *pci_device;         // PCI structure information
    void *remapped_device_base;         // remapped device base address
    dma_addr_t device_host_address;     // host memory address for dev
    dma_addr_t device_psm_buffer;   	// host memory for PSM buffering
    void *driver_mem_address;           // memory address for the driver
    void *q_base_addr[ALLOC_MAX_PAGES]; // base addresses of page allocs
    dma_addr_t q_bus_addr[ALLOC_MAX_PAGES]; // bus addresses of page allocs
    void *device_id;                    // unique ID for shared IRQ

    // kernel configuration members
    struct net_device *next_module;
    struct net_device *my_module;       // makes life earier....

    // device queue interface members
    struct isl38xx_cb *control_block;   // device control block
    u32 index_mgmt_rx;                  // real index mgmt rx queue
    u32 index_mgmt_tx;              	// read index mgmt tx queue
    u32 free_data_rx;                 	// free pointer data rx queue
    u32 free_data_tx;                	// free pointer data tx queue
    u32 data_low_tx_full;               // full detected flag

    // root free queue definitions
    queue_root mgmt_tx_freeq;           // queue entry container mgmt tx
    queue_root mgmt_rx_freeq;           // queue entry container mgmt rx

    // driver queue definitions
    queue_root mgmt_tx_shadowq;         // shadow queue mgmt tx
    queue_root mgmt_rx_shadowq;         // shadow queue mgmt rx
    queue_root ioctl_rx_queue;          // ioctl rx queue
    queue_root trap_rx_queue;           // TRAP pimfor rx queue
    queue_root pimfor_rx_queue;         // PIMFOR generated rx queue
	struct sk_buff *data_low_tx[ISL38XX_CB_TX_QSIZE];
	struct sk_buff *data_low_rx[ISL38XX_CB_RX_QSIZE];
	dma_addr_t pci_map_tx_address[ISL38XX_CB_TX_QSIZE];
	dma_addr_t pci_map_rx_address[ISL38XX_CB_RX_QSIZE];
	
    // driver network interface members
    struct net_device_stats statistics;

    // driver operational members
    int mode;                           // device operation mode
    int powerstate;                     // device power save state
    int linkstate;                      // device link state
    int ioctl_queue_lock;               // flag for locking queue read
    int processes_in_wait;              // process wait counter
    int wds_link_count;             	// number of wds link entries in table
    int resetdevice;                    // reset the device flag
	spinlock_t slock;		            // spinlock

#ifdef WDS_LINKS
    struct wds_priv *wdsp;
#endif
//    address_entry wds_link_table[ISL38XX_MAX_WDS_LINKS]; // WDS Link table
};

typedef struct net_local    islpci_private;

#define ISLPCI_TX_TIMEOUT               (2*HZ)

void islpci_interrupt(int, void *, struct pt_regs * );

int islpci_open( struct net_device * );
int islpci_close( struct net_device * );
int islpci_reset( islpci_private *, char * );
void islpci_set_multicast_list( struct net_device * );
struct net_device_stats *islpci_statistics( struct net_device * );

int islpci_alloc_memory( islpci_private *, int );
int islpci_free_memory( islpci_private *, int );
struct net_device * islpci_probe(struct net_device *nwdev, struct pci_dev *pci_device, long ioaddr, int irq);

#endif  // _ISLPCI_DEV_H
