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

/*
 *  Intersil Ethernet driver for MVC v2.
 *
 *  Based on skeleton ethernet driver by Donald Becker.
 *  Based on previous prism ethernet driver by M Schipper 2001.
 *
 *  Extended by WSM, Intersil 2002-2003.
 *
 *  Copyright 1993 United States Government as represented by the
 *  Director, National Security Agency.
 *  Copyright (C) 2001-2003 Intersil Americas Inc.
 */


/* Include files */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/irq.h>
#include <asm/arch/intctl.h> /* for uIRQ */

/* Intersil include files */
#include <linux_mvc_oids.h>
#include <asm/arch/blobcalls.h>
#include "islmvc.h"
#include "isl_mgt.h"
#include "islmvc_mgt.h"
#include "isl_wds.h"

#ifdef TEST_ON_POLDHU
#include <asm/arch/serial.h>
#include <asm/arch/gpio.h>
#endif /* TEST_ON_POLDHU */

/* End of include files */

/* DEFINES */

#define REQ_UNKNOWN (REQFRAMERX & REQFRAMERETURN & REQTRAP & REQFRAMEADD & REQSERVICE)

/* STATES */
#define PRISM_ETH_STOP  0
#define PRISM_ETH_HALT  1
#define PRISM_ETH_START 2
#define PRISM_ETH_RUN   3

/* defines for cache mask */
#ifdef TEST_ON_POLDHU
#define PRISM_CACHE_MASK 0x04000000
#endif /* TEST_ON_POLDHU */

/* DEFINES */
/* Number of TX retries */
#define TX_NR_RETRIES	10

/* Offset in RAM between cache-able and non-cache-able RAM
   point to the same physical RAM location. */
#ifdef TEST_ON_POLDHU
#define MAP_CACHED(data)   (data |  PRISM_CACHE_MASK)
#define MAP_UNCACHED(data) (data &=  ~PRISM_CACHE_MASK)
#endif /* TEST_ON_POLDHU */

static const char *version = "version 0.4.0.0";
/* The name of the card. Is used for messages and in the requests for irqs */
static const char* cardname = "Prism Embedded MVC v2 packet IF";

/* Static variable to indicate if a frame add failed. */
static int frame_add_failed = 0;

/* End of defines */

/* Probe function is called from Space.c */
int prism_mvc_probe(struct net_device *dev);

#ifdef TEST_ON_POLDHU
extern const unsigned char *blobtool_find_element(int id, int skip);
#endif

extern unsigned int src_seq;

/* Start of local mvc prototypes */
static int prism_eth_init(struct net_device *dev, int startp);
static int prism_eth_start ( struct net_device *dev );
static int prism_eth_run ( struct net_device *dev );
static int local_net_open(struct net_device *dev);
static void local_net_tx_timeout(struct net_device *dev);
static int local_net_send_packet(struct sk_buff *skb, struct net_device *dev);
static void local_net_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static int local_net_close(struct net_device *dev);
static inline void local_dev_handle_reqs(struct net_device *dev, int requests);

/* Start of frame prototypes */
static int  alloc_frame_pool(struct net_local *lp);
static inline void req_frame_rx(struct net_device *dev);
static inline void req_frame_return(struct net_device *dev);
static inline void req_trap_handling(struct net_device *dev);
static inline void req_frame_add(struct net_device *dev);
static inline void req_service(struct net_device *dev);
inline void copy_wds_to_frame(msg_frame *framep, unsigned char *WDS_Mac);
static struct msg_frame *skb_to_frame (struct sk_buff *skb);
static struct sk_buff *frame_to_skb (struct msg_frame *framep);
static struct sk_buff *dev_copy_expand_skb (struct net_device *dev, const struct sk_buff *skb);
static inline struct msg_frame *dev_alloc_frame (struct net_device *dev, int size);
static inline void dev_free_frame (struct net_device *dev, struct msg_frame *frame);

#ifdef BLOB_V2
static int llc_to_wlan (struct sk_buff *skb);
static int wlan_to_llc (struct sk_buff *skb);
#endif

/* Management prototypes */
static int local_set_mac_addr(struct net_device *dev, void *addr);
static struct net_device_stats *local_net_get_stats(struct net_device *dev);

/* Begin of debug function prototypes */
#if (NET_DEBUG & DBG_SKB)
static int  dbg_frame_filter_check (struct net_device *dev, struct msg_frame *framep);
static void dbg_dump_hex_assci (unsigned char *buf, int size);
static int  dbg_check_area (unsigned char *p, int size, char *area_name);
#endif

#if (NET_DEBUG & (DBG_TX | DBG_RX | DBG_ERROR))
static void dbg_print_frame (struct msg_frame *framep);
#endif

#if (NET_DEBUG & DBG_FRAME_SANITY)
static void check_frame_skb(struct msg_frame *framep, char *file, int line);
#endif

/* FIXME, temp here for character driver... */
extern void handle_blob_reqs(void);

static int islmvc_initialized = 0;

/****************************************************************
 *								*
 *    		    Begin of functions		                *
 *								*
 ***************************************************************/


/****************************************************************
 *								*
 *    		Begin of driver configurator functions		*
 *								*
 ***************************************************************/


/**********************************************************************
 *  prism_mvc_probe
 *
 *  DESCRIPTION: Loops through the MVC Information structure trying to find
 *               the interface that is encoded into the device base_addr.
 *               The number in the base_addr is mapped to an MVC interface.
 *               When the device is found, the device structure is initialized,
 *               the MAC address is read from the PDA and set, the device functions
 *               are set, the management handlers get initialized and the device
 *               is put into the READY state.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 0       - when the device is found, initialized and started correctly.
 *               -ENOMEM - when no memory could be allocated for the device.
 *               -ENODEV - when the MVC information structure could not be found;
 *                         when the device is not available;
 *                         when the MAC address could not be found,
 *               -EBUSY    when the device could not be started into the READY state.
 *
 *  NOTE:        Called from Space.c
 *
 **********************************************************************/
int prism_mvc_probe(struct net_device *dev)
{
    int    nr_mvc_devices;  /* Nr of devices present on the MVC. */
    unsigned char *mac_address;

    char type;
    int i;

    struct bis_dev_element *dev_elem = NULL;
    struct bis_dev_descr_element *dev_desc = NULL;
    struct net_local *lp = NULL;
    int dev_found = FALSE;
    static int dev_first_802_3_found = 0;

    islmvc_initialized = 0;

    DEBUG((DBG_FUNC_CALL | DBG_OPEN), "prism_mvc_probe, dev %p, dev->base_addr = %lu \n", dev, dev->base_addr);

    /*
       We have the following (static) mapping for the devices. This mapping ensures
       that the devices get the same device name, even when another device isn't available.

       eth0 - LAN ethernet interface. (1)
       eth1 - WLAN interface. (2)
       eth2 - WAN ethernet interface. This interface is a hardware option. (3)

       We know for which device this probe function is called because the dev->base_addr
       is filled in Space.c with the corresponding number.
    */
    if (dev->base_addr == 1)      /* Probe for the LAN interface */
        type = BIS_DT_802DOT3;
    else if (dev->base_addr == 2) /* Probe for the WLAN interface */
        type = BIS_DT_802DOT11;
#if 0
    else if (dev->base_addr == 3)
        type = BIS_DT_802DOT3;
#endif
    else
        return -ENODEV;

    /* Allocate a new 'dev' if needed. */
    if (dev == NULL)
    {
	/*
	 * Don't allocate the private data here, it is done later
	 * This makes it easier to free the memory when this driver
	 * is used as a module.
	 */
	dev = init_etherdev(0,0);
	if (dev == NULL)
	    return -ENOMEM;
    }

    /* Get the number of devices available in the MVC. */
    dev_elem = (struct bis_dev_element *) blobtool_find_element(BIS_DEVS, 0);

    if (!dev_elem)
    {
        printk(KERN_ALERT "No dev_element found!!!");
        return -ENODEV;
    }

#ifdef BLOB_V2
    nr_mvc_devices = dev_elem->devs; /* MVC v2 */
#else
    nr_mvc_devices = dev_elem->dev;  /* MVC v1 */
#endif /* TEST_ON_POLDHU */

    DEBUG(DBG_OPEN, "Nr of MVC devices = %i \n", nr_mvc_devices);

    /* Loop through the list and find the types of devices available... */
    for (i = 0; i < nr_mvc_devices && !dev_found; i++)
    {
        dev_desc = (struct bis_dev_descr_element *) blobtool_find_element (BIS_DEVDESCR, i);

        if (!dev_desc)
        {
            printk(KERN_ALERT "No dev_desc found!!!");
            return -ENODEV;
        }

        /* Let's find out if the MVC accomodates this type... */
        if (dev_desc->type == type)
        {
            /* For the WAN interface, we need the second device, so skip this device when we find the first... */
            if (dev->base_addr == 1) {
                dev_first_802_3_found = i;
            } else if (dev->base_addr == 3 && dev_first_802_3_found == i) {
                continue;
            }

            /* The MVC has a device with the right type for us... */
            DEBUG(DBG_OPEN, "Found MVC device with type nr %i to use for %s \n", i, dev->name);

            /* We (ab) use the base_addr field for storing the device number and device type for later reference. */

            ADD_DEV_TYPE(dev->base_addr, type);
            ADD_DEV_NR(dev->base_addr, i);

            /* Initialize the device structure. */
            if (dev->priv == NULL)
            {
            	dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
            	if (dev->priv == NULL)
            	    return -ENOMEM;
            }
            memset(dev->priv, 0, sizeof(struct net_local));
            lp = (struct net_local *)dev->priv;

            lp->wdsp = kmalloc(sizeof(struct wds_priv), GFP_KERNEL);
            if ( lp->wdsp == NULL)
                return -ENOMEM;
            memset(lp->wdsp, 0, sizeof(struct wds_priv));

            /* Set max mtu, head and tail sizes for this MVC packet device */
#ifdef BLOB_V2
            lp->maxhead  = (dev_elem->header + 63) & ~3; // extra headspace for zero copy to mini PCI
            lp->maxtail  = dev_elem->trailer;
            lp->maxmtu   = dev_elem->mtu;

            lp->maxnrframes = 200; /* FIXME, this should be part of MVC device descr
            			    * changed to 200 queues in mvc bigger.
            			    */
            lp->dev_elem = dev_elem;

            printk( "dev_elem, type %d, mtu %d, head %d, tail %d\n",
		            dev_desc->type, lp->dev_elem->mtu,
		            lp->dev_elem->header, lp->dev_elem->trailer);
#else
            lp->maxhead  = (dev_desc->header + 63) & ~3; // extra headspace for zero copy to mini PCI
            lp->maxtail  = dev_desc->trailer;
            lp->maxmtu   = dev_desc->mtu;

            lp->maxnrframes = 100; /* FIXME, this should be part of MVC device descr */
            lp->dev_desc = dev_desc;

            printk( "dev_descr, nr %d, type %d, mtu %d, head %d, tail %d\n",
		            lp->dev_desc->nr, lp->dev_desc->type, lp->dev_desc->mtu,
		            lp->dev_desc->header, lp->dev_desc->trailer);
#endif /* BLOB_V2 */

            dev_found = TRUE;
        }
    }

    if (!dev_found)
    {
        printk("Couldn't find an MVC device type (%ld) for %s \n", dev->base_addr, dev->name);
        return -ENODEV;
    }
    else
        printk(KERN_INFO "%s: %s %s found,", dev->name, cardname, version );

    /* For both ethernet interfaces (LAN & WAN) and the onboard radio the same MAC address is used.
       The MAC address of the WAN interface can be changed later to a different MAC address
       to support MAC cloning for the WAN interface. */

    /* Get the MAC address from the PDA. Function returns pointer to MACAddress. */
    mac_address = pda_get_mac_address();

    if (mac_address == NULL) /* If we couldn't get the MAC address from the PDA, exit... */
    {
        printk(KERN_ERR "Couldn't get MAC address for %s from PDA... aborting... \n", dev->name);
        kfree(lp);
        return -ENODEV;
    }

    memcpy(dev->dev_addr, mac_address, ETH_ALEN);
    printk("Macaddress = %s\n", hwaddr2string(mac_address));

    dev->open		    = local_net_open;
    dev->stop		    = local_net_close;
    dev->get_stats	    = local_net_get_stats;
    dev->hard_start_xmit    = local_net_send_packet;
    dev->irq                = IRQ_SOFINT;
#ifdef HAVE_TX_TIMEOUT
    dev->tx_timeout	    = local_net_tx_timeout;
    dev->watchdog_timeo	    = 5 * HZ;
#endif
#ifdef HAVE_SET_MAC_ADDR
    dev->set_mac_address    = local_set_mac_addr;
#endif

    /* Allocate the frame pool */
    if (alloc_frame_pool(lp) < 0)
    {
        kfree(lp);
        return -ENOMEM;
    }

    /* Fill in the fields of the device structure with ethernet values. */
    ether_setup(dev);

    /* Initialize the management handlers */
    islmvc_mgt_init(dev);

    /* Let's bring the device in the READY state */
    if (prism_eth_init( dev, PRISM_ETH_START) < 0 )
    {
        kfree(lp);
        printk(KERN_ERR "Couldn't start device %s \n", dev->name);
        return -EBUSY;
    }

    return 0;
}


/**********************************************************************
 *  prism_eth_init
 *
 *  DESCRIPTION: Initialise the MVC device to a specific state.
 *
 *  PARAMETERS:	 dev    - pointer to a linux net device struct.
 * 		 startp - the operation to perform on the MVC
 *
 *  RETURN:	 0       - when the device is properly initialized.
 *               -EBUSY  - when the device is not in the state it should
 *                         be in, in order to change to the desired state.
 *               -EINVAL - when the state or device type is not known;
 *                       - when changing the state did not succeed.
 *
 *  NOTE:	 Depending on the current state some values should
 * 		 possibly be reinitialised in the MVC.
 *
 **********************************************************************/
static int prism_eth_init( struct net_device *dev, int startp )
{
    long state;
#ifdef BLOB_V2
    struct msg_start start;
#endif

    long dev_nr = GET_DEV_NR(dev->base_addr);
    long type   = GET_DEV_TYPE(dev->base_addr);
    int error   = 0;

    DEBUG(DBG_FUNC_CALL | DBG_OPEN, "prism_eth_init, dev %s, startp %d, (dev_nr = %ld) \n", dev->name, startp, dev_nr);

    state = dev_state (dev_nr);
    DEBUG(DBG_OPEN, "dev_state, %ld...", state);

    switch (startp)
    {
	case PRISM_ETH_STOP:  /* MVC State will end up being STSTOPPED */
	    /*
	     * In this case state doesn't really matter,
	     * if not already stopped stop now!
	     */
	    if(state != STSTOPPED)
	    {
		DEBUG(DBG_OPEN, "dev_stop...");
		error = dev_stop (dev_nr);
	    }
	break;
	case PRISM_ETH_HALT:  /* MVC State will end up being STREADY */
	    /*
	     * Man can only HALT a device if it's RUNNING.
	     */
	    if(state == STRUNNING)
	    {
		DEBUG(DBG_OPEN, "dev_halt...");
		error = dev_halt (dev_nr);
	    }
	    else
	    {
		error = -EBUSY;
	    }
	break;
	case PRISM_ETH_START: /* MVC State will end up being STREADY */
	    /*
	     * Man can only START a device if it's STOPPED.
             * When it's started for the first time, the state is == BER_UNSUP.
	     */
	    if(state == STSTOPPED || state == BER_UNSUP)
	    {
            DEBUG(DBG_OPEN, "dev_start...");

#ifdef BLOB_V2
            /* We can tell the MVC what device number we want to use. Use the one we figured out during the probe... */
            start.nr    = dev_nr;

            /* Determine the mode to start the device depending on it's type. */
            if (type == BIS_DT_802DOT11) {
                //start.mode  = MODE_AP;
                start.mode = MODE_CLIENT;
                
                
            }
            else if (type == BIS_DT_802DOT3) {
                start.mode  = MODE_PROMISCUOUS;
            }
            else {
                printk(KERN_ERR "Unknown device type in prism_eth_init.\n");
                return -EINVAL;
            }

            error = dev_start(dev_nr, &start);
#else
            error = dev_start(dev_nr, MODE_PROMISCUOUS);
#endif /* BLOB_V2 */

            if (error == BER_NONE)
                error = prism_eth_start ( dev );
	    }
	    else
	    {
		DEBUG(DBG_OPEN, "state != STOPPED\n");
                error = -EBUSY;
	    }
	break;
	case PRISM_ETH_RUN:   /* MVC State will end up being RUNNING */
	    /*
	     * Man can only RUN a device if it's READY to run.
	     */
	    if(state == STREADY)
	    {
		DEBUG(DBG_OPEN, "dev_run...");
		error = prism_eth_run ( dev );
		if (error == 0)
   	            error = dev_run (dev_nr);
	    }
	    else
	    {
		error = -EBUSY;
	    }
	break;
        default:
	    error = -EINVAL;
    }
    if (error != 0)
    {
        DEBUG((DBG_OPEN | DBG_ERROR), "error %d\n", error);
        return -EINVAL;
    }

#if (NET_DEBUG & DBG_OPEN)
    else
    {
	state = dev_state (dev_nr);
	printk ("dev_state changed to %ld...\n", state);
    }
#endif

    return 0;
}

/**********************************************************************
 *  prism_eth_start
 *
 *  DESCRIPTION: This starts the device. It performs the following operations:
 *                  Sets the device MAC address on the MVC.
 *                  For a WLAN device, the MLME autonomy mode is set,
 *                  and some calibration setting are taken from the PDA
 *                  (Production Data Area) and send to the MVC.
 *                  The device requests are handled.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 0       - when starting the device succeeded.
 *               -EINVAL - when setting a setting failed.
 *
 **********************************************************************/
static int prism_eth_start ( struct net_device *dev )
{
    int requests;
    long mode;
    int error;
    int i;
    struct obj_scan scan;	
    
    DEBUG(DBG_FUNC_CALL | DBG_OPEN, "prism_eth_start, dev->name %s\n", dev->name);

    /* set the MAC address */
    if (islmvc_send_mvc_msg(dev, OPSET, GEN_OID_MACADDRESS, (long*)dev->dev_addr, ETH_ALEN) < BER_NONE)
        return -EINVAL;

    DEBUG(DBG_OPEN, "MAC set..");

    /* for the WLAN device, set the MLME mode (and calibration settings for MVC v2.) */
    if (GET_DEV_TYPE(dev->base_addr) == BIS_DT_802DOT11)
    {
#ifdef BLOB_V2
        /* Send some calibration values from the PDA to the MVC for MVC v2. */
        set_rssi_linear_approximation_db(dev);
        set_zif_tx_iq_calibr_data(dev);
        set_hw_interface_variant(dev);
        //set_calibration_data(dev);
        	
	printk("\n\n\t\t DUETTE APDK CLIENT MODE \n\n");
		
	/* Enable active scanning for every frequency in the scan structure. */
	if ((error = islmvc_send_mvc_msg(dev, OPGET, DOT11_OID_SCAN, (long*)&scan, sizeof(struct obj_scan))) < 0)
	     return -EINVAL;
	
	for (i = 0; i < scan.nr; i++) {
	     scan.mhz[i] |= 0x8000;
	}
	scan.sweep = 0;
	
	if ((error = islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_SCAN, (long*)&scan, sizeof(struct obj_scan))) < 0)
	     return -EINVAL;
	       
	//FIXME, what mode to use for CLIENT MODE?
	//mode = DOT11_MLME_INTERMEDIATE;
	mode = DOT11_MLME_AUTO;
#else
        mode = DOT11_MLME_SIMPLE;
#endif
        if ((error = islmvc_send_mvc_msg(dev, OPSET, DOT11_OID_MLMEAUTOLEVEL, &mode, sizeof(long))) < 0)
            return -EINVAL;

  	DEBUG(DBG_OPEN, "MLME mode set..");

    }

    /* handle all device requests (= add frames) */
    while ((requests = dev_reqs (GET_DEV_NR(dev->base_addr))) > BER_NONE)
    {
	local_dev_handle_reqs (dev, requests);
    }
    DEBUG(DBG_OPEN, "requests handled..\n");

    return 0;
}


/**********************************************************************
 *  prism_eth_run
 *
 *  DESCRIPTION: Empty function.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 0
 *
 **********************************************************************/
static int prism_eth_run ( struct net_device *dev )
{
    DEBUG(DBG_FUNC_CALL | DBG_OPEN, "prism_eth_run, dev %s \n", dev->name);

#if (defined(TEST_ON_POLDHU) && !defined(CONFIG_FREE_WLAN_PINS))
    /* if we are starting the WLAN device, then... */
    if (GET_DEV_TYPE(dev->base_addr) == BIS_DT_802DOT11)
    {
	/* don't disable the UART completely, this crashes the system ?! */
	uUART->CR &= ~0x78;	/* disable UART interrupts */
	/* make the UART pins free for the radio */
	uGPIO->FSEL = (GPIOFSELUARTTx | GPIOFSELUARTRx) << 16;
    }
#endif /* TEST_ON_POLDHU && CONFIG_FREE_WLAN_PINS */

    return 0;
}

#define MAX_DEVICES 16

struct mvc_irq_table
{
    int num_devs;
    struct net_device *dev_ptr[MAX_DEVICES];
    int dev_nr[MAX_DEVICES];
};


/**********************************************************************
 *  register_mvc_irq
 *
 *  DESCRIPTION: This registers the device to the internal device list.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 pointer to the irq table on success.
 *               NULL when unable to register the device.
 *
 **********************************************************************/
struct mvc_irq_table *register_mvc_irq(struct net_device *dev)
{
    unsigned long flags;

    struct net_local *lp = (struct net_local *) dev->priv;

    static struct mvc_irq_table stat_irq_table =
        { 0, { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL } };

    DEBUG(DBG_FUNC_CALL | DBG_OPEN, "register_mvc_irq %s\n", dev->name);

    if (dev == NULL)
        return NULL;

    if (stat_irq_table.num_devs >= MAX_DEVICES)
        return NULL;

    save_flags_cli (flags);

    stat_irq_table.dev_ptr[stat_irq_table.num_devs] = dev;
    stat_irq_table.dev_nr[stat_irq_table.num_devs] = GET_DEV_NR(dev->base_addr);
    stat_irq_table.num_devs++;

    restore_flags(flags);

    lp->mvcirq_table = &stat_irq_table;

    return &stat_irq_table;

}


/**********************************************************************
 *  unregister_mvc_irq
 *
 *  DESCRIPTION:  This unregisters the device from the internal device
 *                list.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 0      - on success,
 *               -1     - when unable to unregister the device.
 *
 **********************************************************************/
int unregister_mvc_irq(struct net_device *dev)
{
    int i;
    unsigned long flags;

    int j = 0;
    struct net_local *lp = (struct net_local *) dev->priv;

    DEBUG(DBG_FUNC_CALL | DBG_OPEN, "unregister_mvc_irq %s\n", dev->name);

    if (dev == NULL) {
        printk("unregister_mvc_irq: dev == NULL, can't unregister.\n");
        return -1;
    }

    save_flags_cli (flags);

    for (i=0; i < MAX_DEVICES; i++) {
        if (lp->mvcirq_table->dev_ptr[i] == dev) {
            j++;
        }

        lp->mvcirq_table->dev_ptr[i] = lp->mvcirq_table->dev_ptr[j];
        lp->mvcirq_table->dev_nr[i] = lp->mvcirq_table->dev_nr[j];
        j++;
    }

    lp->mvcirq_table->num_devs--;

    restore_flags(flags);

    return 0;
}



/**********************************************************************
 *  local_net_open
 *
 *  DESCRIPTION:  Open/initialize the board. This is called (in the current kernel)
 *                sometime after booting when the 'ifconfig' program is run.
 *                This routine should set everything up anew at each open, even
 *                registers that "should" only need to be set once at boot, so that
 *                there is non-reboot way to recover if something goes wrong.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 0      - on success,
 *               -EBUSY - when unable to get IRQ or setting the device into
 *                        running mode.
 *
 **********************************************************************/


static int local_net_open(struct net_device *dev)
{
    long linkstate;

    int retval = 0;

#ifdef BLOB_V2
    struct mvc_irq_table *irq_table = NULL;
#endif

    DEBUG(DBG_FUNC_CALL | DBG_OPEN, "local_net_open %s\n", dev->name);

    /* Reset the hardware and set the card MAC address. */
    if (prism_eth_init(dev, PRISM_ETH_RUN) < 0)
    {
        printk("Couldn't reset hardware \n");
        return -EBUSY;
    }

    netif_start_queue (dev);

#ifdef BLOB_V2
    /* Register the IRQ in the mvc irq table. */
    if ((irq_table = register_mvc_irq(dev)) == NULL) {
        printk(KERN_ERR "Registering the IRQ in the internal table failed.\n");
        return -EBUSY;
    }

    /* Dispatching the IRQ for the different devices is done in the local net interrupt function,
       so we register the IRQ only once now. Still use SA_SHIRQ, as it's possible that another device
       then the one we registered under frees the IRQ. */
    if (irq_table->num_devs == 1)
        retval = request_irq(dev->irq, &local_net_interrupt, SA_SHIRQ | SA_INTERRUPT, dev->name, irq_table);
#else
    retval = request_irq(dev->irq, &local_net_interrupt, SA_SHIRQ | SA_INTERRUPT,
		    dev->name, dev);
#endif

    if (retval) {
	printk("%s: unable to get IRQ %d (retval=%d).\n",
	       dev->name, dev->irq, retval);
	netif_stop_queue (dev);
        unregister_mvc_irq(dev);
        return -EBUSY;
    }

    /* determine initial linkstate for Ethernet interface (LAN or WAN) */
    if (GET_DEV_TYPE(dev->base_addr) == BIS_DT_802DOT3) {
        islmvc_send_mvc_msg(dev, OPGET, GEN_OID_LINKSTATE, &linkstate, sizeof(long));
        if (linkstate == 0) {
            netif_carrier_off(dev);
        }
    }

    /* If the current device is host device for WDS devices, bring the WDS devices up and running. */
    if (GET_DEV_TYPE(dev->base_addr) == BIS_DT_802DOT11)
        open_wds_links(((struct net_local *)dev->priv)->wdsp);

    return 0;
}


/**********************************************************************
 *  local_net_tx_timeout
 *
 *  DESCRIPTION: Called when the kernel signals a tx timeout.
 *               This functions checks the linkstate of the device.
 *               If the link is down, the carrier flag of the devive is
 *               put down.
 *               If the link is not reported down, the interface is resetted.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:      Nothing.
 *
 **********************************************************************/
#ifdef HAVE_TX_TIMEOUT
static void local_net_tx_timeout(struct net_device *dev)
{
    long linkstate;

    struct net_local *lp = (struct net_local *)dev->priv;

    DEBUG(DBG_FUNC_CALL | DBG_TX, "local_net_tx_timeout, dev %s \n", dev->name);

    islmvc_send_mvc_msg(dev, OPGET, GEN_OID_LINKSTATE, &linkstate, sizeof(long));

    if (linkstate == 0)
    {
	printk(KERN_WARNING "%s: link down, network cable problem?\n", dev->name);

	netif_carrier_off (dev);
    }
    else
    {
	
	printk(KERN_WARNING "%s: reset interface\n", dev->name);

	/* Try to restart the device. */
	prism_eth_init(dev, PRISM_ETH_HALT);
        prism_eth_init(dev, PRISM_ETH_RUN);
	lp->stats.tx_errors++;
        netif_wake_queue(dev);
    }
}
#endif /* HAVE_TX_TIMEOUT */


/**********************************************************************
 *  local_net_send_packet
 *
 *  DESCRIPTION: This function puts an skbuf into an MVC frame,
 *               updates the statistics and sends the frame to the local MVC.
 *
 *  PARAMETERS:	 skb - pointer to a linux packet buffer.
 *               dev - pointer to a linux net device struct.
 *
 *  RETURN:	 0
 *
 **********************************************************************/
static int local_net_send_packet(struct sk_buff *skb, struct net_device *dev)
{
    //FIXME, still needed for MVC v2?
    static int tries = 1000000; /* give the first packet a lot of retries */

    unsigned char *src;
    struct sk_buff *skb2;
    struct msg_frame *framep;
    int error;
    int WDSLink;
    long linkstate;
    unsigned char WDS_Mac[ETH_ALEN];

    struct net_local *lp = (struct net_local *)dev->priv;
    long dev_nr = GET_DEV_NR(dev->base_addr);

    DEBUG((DBG_FUNC_CALL | DBG_TX), "local_net_send_packet, skb %p, dev %s \n", skb, dev->name);

    /* If this skb came from a WDS device, the
       skb->cb field will be filled with a WDSLink MACaddress.
       Since the skb->cb field won't be cloned, we check it here. */
    WDSLink = check_skb_for_wds(skb, WDS_Mac);

    /* if the head or tail room doesn't comply with the device or
       the skb is cloned .. */
#ifdef BLOB_V2
    if (skb_headroom (skb) < lp->dev_elem->header ||
	skb_tailroom (skb) < lp->dev_elem->trailer ||
	skb_cloned (skb))
#else
    if (skb_headroom (skb) < lp->dev_desc->header ||
        skb_tailroom (skb) < lp->dev_desc->trailer ||
        skb_cloned (skb))
#endif
    {
#if (NET_DEBUG & DBG_TX)
        if (!skb_cloned (skb))
	{
            DEBUG(DBG_TX, "no head/tail room, head:(%d,%d),tail(%d,%d)..",
		skb_headroom (skb), lp->dev_elem->header,
		skb_tailroom (skb), lp->dev_elem->trailer);
	}
#endif

        /* see if we can shift the data in the buffer */
	if (!skb_cloned (skb) &&
	    skb_headroom (skb) + skb_tailroom (skb) >=
#ifdef BLOB_V2
            lp->dev_elem->header + lp->dev_elem->trailer)
#else
            lp->dev_desc->header + lp->dev_desc->trailer)
#endif
	{

#ifdef BLOB_V2
            DEBUG(DBG_TX, "shift data: %d..", lp->dev_elem->header - skb_headroom (skb));
#else
            DEBUG(DBG_TX, "shift data: %d..", lp->dev_desc->header - skb_headroom (skb));
#endif

	    src = skb->data;
#ifdef BLOB_V2
            skb_reserve (skb, lp->dev_elem->header - skb_headroom (skb));
#else
            skb_reserve (skb, lp->dev_desc->header - skb_headroom (skb));
#endif
	    memmove (skb->data, src, skb->len);
            lp->tx_misaligned++;

	}
	else	/* copy to a new buffer with enough head and tail room */
	{
#if (NET_DEBUG & DBG_TX)
	    if (skb_cloned (skb))
		printk ("cloned sk_buff.. ");
	    printk ("copy data.. ");
#endif

	    skb2 = dev_copy_expand_skb (dev, skb);
	    dev_kfree_skb (skb);
	    if (skb2 == NULL)
            {
                lp->stats.tx_dropped++;
		return 0;
            }
	    skb = skb2;
	}
    }

    if ((framep = skb_to_frame (skb)) == NULL)
    {
	DEBUG((DBG_TX | DBG_ERROR), "NO frame!!!\n");
	dev_kfree_skb (skb);
	lp->stats.tx_dropped++;
        return 0;
    }

    /* send the frame to the local MVC */

    DEBUG(DBG_TX, "send: size %d, headroom %d..", framep->size,
            skb_headroom (framep->handle));
#if (NET_DEBUG & DBG_TX)
    dbg_print_frame( framep );
#endif

    /* Copy the found WDS MAC address in the msg_frame */
    if (WDSLink == 1) 
    	copy_wds_to_frame(framep, WDS_Mac);

    do
    {
        error = dev_frame_tx (dev_nr, framep);
        if (error == BER_OVERFLOW)
            dev_service (dev_nr); /* Give the device some time to process */
        tries--;
    }
    /* We can do this while:
     * <BER_ERROR: 	If the frame is not queued.
     * !BER_DSTUNKN: 	Destination isn't unknown (no need to retry, the 
     * 			dev will not know it the next time either).
     * !BER_UNSPEC:	The frame is not allowed by frame class 
     *			(for instance, data to authenticated but not 
     *			 associated clients) or by privacy settings
     *			(for instance, no key installed while exclude
     *			 unencrypted is set).
     * tries:		Untill the retry counter is zero.
     */
    while (error < BER_NONE &&
           error != BER_DSTUNKN && 
           error != BER_UNSPEC && 
           tries);
    tries = TX_NR_RETRIES;

    if (error < BER_NONE)
    {
    	/* On errors not unreachable or not allowed to send
    	 * check if the linkstate is down and turn carrier off if so.
    	 * When the error is queue overflow stop the queue in order
    	 * to give the device some time to process the full queues.
    	 * When other tx-ed frames return in the system the queue
    	 * will be enabled.
    	 */
        if (error != BER_DSTUNKN && error != BER_UNSPEC)
        {
	    _DEBUG((DBG_TX | DBG_ERROR),
	           KERN_INFO "TX dev: %s, NOT accepted!!\n", dev->name);

            /* test if the link is down */
            islmvc_send_mvc_msg(dev, OPGET, GEN_OID_LINKSTATE, &linkstate, sizeof(long));
            if (linkstate == 0)		        /* Disable carrier and queues ? */
                netif_carrier_off (dev);
            else if (error == BER_OVERFLOW)     /* The device queue is full, backoff ! */  
                netif_stop_queue (dev);

        }
        /* Count all error codes as dropped */
	lp->stats.tx_dropped++;

	/* abort the frame */
	dev_free_frame (dev, framep);
    }
    else {
        DEBUG((DBG_TX | DBG_WDS), "OK!!\n");
    }
    dev->trans_start = jiffies;
 
    return 0;
}


/**********************************************************************
 *  local_net_interrupt
 *
 *  DESCRIPTION: The typical workload of the driver:
 *               Handle the network interface interrupts.
 *
 *  PARAMETERS:	 irq    - interrupt number, not used.
 *               dev_id - pointer to the net_device struct, assigned when
 *                        the IRQ was registered.
 *               regs   - Not used.
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:        When frame_add_failed is set, we leave the while loop
 *               to give the system some time to allocate memory.
 *
 **********************************************************************/
#ifdef BLOB_V2
static void local_net_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
    int			    requests;
    int                     i;
    int                     bit;
    int                     dev_nr;
    unsigned int            reqs_pending;
    struct mvc_irq_table    *irq_table;
    struct net_local        *lp;
    char                    *irq_stats;
    struct net_device       *first_dev;

    struct net_device	    *dev = NULL;

    uIRQ->Soft = 0;	    /* remove soft irq */

    DEBUG(DBG_FUNC_CALL | DBG_IRQ, "local_net_interrupt, irq %d, dev_id %p, regs %p \n", irq, dev_id, regs);

    if (dev_id == NULL) {
	printk(KERN_WARNING "%s: irq %d for unknown device.\n", cardname, irq);
	return;
    }

    irq_table = (struct mvc_irq_table*) dev_id;

    /* Get a pointer to the MVC irq stats. */
    if ((first_dev = irq_table->dev_ptr[0]) == NULL)
        BUG(); /* Should never happen! */

    lp = (struct net_local*) first_dev->priv;
    irq_stats = lp->dev_elem->irq_stats;

    reqs_pending = ( (1 << (irq_table->num_devs)) -1);

    /* Reset this variable */
    frame_add_failed = 0;

    while (reqs_pending && frame_add_failed != 1)
    {
        for (i=0; i < irq_table->num_devs; i++)
        {
            dev_nr = irq_table->dev_nr[i];

            if (irq_stats[dev_nr]) {
                irq_stats[dev_nr] = 0;
                if ((dev = irq_table->dev_ptr[i]) == NULL)
                    BUG(); /* Should never happen! */
                if ((requests = dev_reqs(dev_nr)) > BER_NONE)
                    local_dev_handle_reqs(dev, requests);
            }
            else {
                bit = (1 << i);
                reqs_pending &= ~bit;
            }
        }
    }

    handle_blob_reqs();

    return;
}

#else /* !BLOB_V2 */

static void local_net_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
    int			requests;

    struct net_device	*dev = NULL;

    uIRQ->Soft = 0;	    /* remove soft irq */

    _DEBUG(DBG_FUNC_CALL | DBG_IRQ, "local_net_interrupt, irq %d, dev_id %p, regs %p \n", irq, dev_id, regs);

    if (dev_id == NULL)
    {
	printk(KERN_WARNING "%s: irq %d for unknown device.\n", cardname, irq);
	return;
    }

    dev = (struct net_device *) dev_id;

    /* We handle blob requests within the Ethernet device */
    if (GET_DEV_TYPE(dev-> base_addr) == BIS_DT_802DOT3)
	handle_blob_reqs();

    /* Reset this variable */
    frame_add_failed = 0;

    if ((requests = dev_reqs (GET_DEV_NR(dev->base_addr))) > BER_NONE)
    	local_dev_handle_reqs (dev, requests);

    return;
}

#endif /* BLOB_V2 */

/**********************************************************************
 *  local_net_close
 *
 *  DESCRIPTION: The inverse routine to local_net_open().
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 0
 *
 **********************************************************************/
static int local_net_close(struct net_device *dev)
{
    struct msg_frame *framep;

    long dev_nr = GET_DEV_NR(dev->base_addr);
    struct net_local *lp = (struct net_local *) dev->priv;

    DEBUG(DBG_FUNC_CALL | DBG_CLOSE, "local_net_close, dev %s \n", dev->name);

    /* If the current device is host device for WDS devices,
       bring the associated WDS devices DOWN. */
    if (GET_DEV_TYPE(dev->base_addr) == BIS_DT_802DOT11)
        close_wds_links(((struct net_local *)dev->priv)->wdsp);

    netif_stop_queue(dev);

    if (dev_halt(dev_nr) >= BER_NONE)
    {
        while ((framep = dev_frame_return(dev_nr)) != NULL) {
            dev_free_frame(dev, framep);
        }
    }

    /* Dispatching the IRQ for the different devices is done in the local net interrupt function,
       so we free the IRQ only for the last device. */
    if (lp->mvcirq_table->num_devs == 1)
        free_irq(dev->irq, dev);

    unregister_mvc_irq(dev);

    return 0;
}


/****************************************************************
 *								*
 *    		Begin of frame handling block			*
 *              (driver configurator module)			*
 *								*
 ***************************************************************/


/**********************************************************************
 *  req_frame_rx
 *
 *  DESCRIPTION: Handle the frame rx requests.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:	 Inline function.
 *
 **********************************************************************/
static inline void req_frame_rx(struct net_device *dev)
{
    struct msg_frame        *framep;
    struct net_device       *wds_dev;
    struct wds_net_local    *wds_lp;
    struct sk_buff          *skb;

    struct net_local        *lp = (struct net_local *) dev->priv;

    DEBUG(DBG_IRQ, "..RX");

    while ((framep = dev_frame_rx (GET_DEV_NR(dev->base_addr))) != NULL)
    {
        if (GET_DEV_TYPE(dev->base_addr) == BIS_DT_802DOT11)
        {
            /* Check if this frame contains one of the WDS addresses */
#ifdef BLOB_V2
            wds_dev = wds_find_device(framep->wds, ((struct net_local *)dev->priv)->wdsp);
#else
            wds_dev = wds_find_device(framep->wds_link, ((struct net_local *)dev->priv)->wdsp);
#endif
        }
        else
            wds_dev = NULL;

        dev->last_rx = jiffies;
        if (framep->size < 1569 && framep->status < 10)
        {

#if (NET_DEBUG & DBG_RX)
            dbg_print_frame (framep);
#endif
#if (NET_DEBUG & DBG_SKB)
            if (dbg_frame_filter_check (dev, framep))
#endif
            {
                if ((skb = frame_to_skb (framep)) != NULL)
                {
                    skb->protocol = eth_type_trans( skb, dev );
                    lp->stats.rx_packets++;
                    lp->stats.rx_bytes += skb->len;

                    /* This will change in the new WDS implementation. */
                    if (wds_dev != NULL)
                    {
                        wds_lp = (struct wds_net_local *) wds_dev->priv;
                        wds_lp->stats.rx_packets++;
                        wds_lp->stats.rx_bytes += skb->len;
                        skb->dev = wds_dev;
                    }
                    /* Give the data to the Linux kernel for further processing. */
                    netif_rx(skb);
                }
                else
                {
                    /* No valid data in frame. It's dropped.*/
                    lp->stats.rx_dropped++;
                }
            }
        }
        else
            printk (KERN_ERR " RX, size %d, status %d!!\n",
                    framep->size, framep->status);
    }

}


/**********************************************************************
 *  req_frame_return
 *
 *  DESCRIPTION: Handle the frame return requests.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:	 Inline function.
 *
 **********************************************************************/
static inline void req_frame_return(struct net_device *dev)
{
    struct msg_frame    *framep;

    struct net_local        *lp = (struct net_local *) dev->priv;

    while ((framep = dev_frame_return (GET_DEV_NR(dev->base_addr))) != NULL)
    {
        _DEBUG(DBG_IRQ, "..RET");
        if (framep->status <= FSRETURNED) /* Check these defines carefully with frame state defines.  */
        {
            if (framep->status <= FSTXSUCCESSFUL)
            {
                if (framep->status == FSTXSUCCESSFUL)
                {
                    lp->stats.tx_packets++;
                    lp->stats.tx_bytes += framep->size;
                }
                else
                {
                    _DEBUG(DBG_TX, " TX error!!. status %d\n",
                            framep->status);
                    lp->stats.tx_errors++;
                }
                netif_wake_queue (dev);
            }
            dev_free_frame (dev, framep);
        }
        else
            printk (KERN_ERR " RET, size %d, status %d!!\n",
                    framep->size, framep->status);
    }
}


/**********************************************************************
 *  req_trap_handling
 *
 *  DESCRIPTION: Handle traps from the MVC.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:	 Inline function.
 *
 **********************************************************************/
/*
 *  The trap handling mechanism is different for Blob V1 and Blob V2.
 *
 */
#ifdef BLOB_V2
static inline void req_trap_handling(struct net_device *dev)
{
    struct msg_conf     trapmsg;
    int                 dev_grp;
    int                 dev_nr         = GET_DEV_NR(dev->base_addr);
    int                 dev_type       = GET_DEV_TYPE(dev->base_addr);
    unsigned char       *new_buf       = NULL;
    unsigned char       local_buf[64];
    long 		led_wlan_config_on  = 5;
    long 		led_eth_config_on  = 2;
    long		led_config_off = 0;

    _DEBUG(DBG_IRQ, "..TRAP");

    /* Most traps are the size of an obj_mlme struct or less. If this does not fit the trap size
       requirement, we are told so and will malloc a new buffer for the trap data. */
    trapmsg.data = (long*) &local_buf[0];
    trapmsg.size = sizeof(local_buf);

    /* Get the trap from the MVC... */
    if (dev_trap (dev_nr, &trapmsg) == BER_OVERFLOW)
    {
        /* The size of the buffer we passed was too small. Malloc a new buffer with correct size. */
        if ((new_buf = kmalloc(trapmsg.size, GFP_ATOMIC)) == NULL)
        {
            printk(KERN_ERR "Couldn't malloc new trap buffer. \n");
            return;
        }

        trapmsg.data = (long *) new_buf;

        /* Okay, try again... */
        if (dev_trap (dev_nr, &trapmsg) == BER_OVERFLOW)
        {
            printk(KERN_WARNING "Couldn't get trap message for the second time. Stopped trying... \n");
            return;
        }
    }

    /* The Linkstate trap is partly handled in the driver. */
    if (trapmsg.oid == GEN_OID_LINKSTATE)
    {
	if ( *trapmsg.data ) /* LINK State is UP... */
	{
	    /* Check the state of the link, and set/clear the corresponding LED accordingly*/
	    if (islmvc_send_mvc_msg(dev, OPSET, GEN_OID_LEDCONFIG, 
		    ((dev_type == BIS_DT_802DOT3) ? (long*)&led_eth_config_on : (long*)&led_wlan_config_on), 
		    sizeof(led_eth_config_on)) < 0)
		printk("req_trap_handling, couldn't set led config for dev %s\n", dev->name);
	    printk ("%s: link up\n", dev->name);
	    netif_carrier_on (dev);
        }
	else /* LINK State is DOWN... */
	{
	    if (islmvc_send_mvc_msg(dev, OPSET, GEN_OID_LEDCONFIG, (long*)&led_config_off, sizeof(led_config_off)) < 0)
		printk("req_trap_handling, couldn't set led config 0 for dev %s\n", dev->name);		
	    printk ("%s: link down\n", dev->name);
	    netif_carrier_off (dev);
	}
    }

    /* Determine the device group based on the device type. */
    if (dev_type == BIS_DT_802DOT3)
        dev_grp = DEV_NETWORK_ETH;
    else if (dev_type == BIS_DT_802DOT11)
        dev_grp = DEV_NETWORK_WLAN;
    else
        dev_grp = 0; /* Unknown device. Use no trapgrp. */

    /* We handle all other traps in the generic part, and also send the GEN_OID_LINKSTATE up */
    mgt_response( 0, src_seq++, dev_grp, dev->ifindex,
       PIMFOR_OP_TRAP, trapmsg.oid, trapmsg.data, trapmsg.size);

    /* When we malloced a new buffer, free it here. */
    if (new_buf)
        kfree(new_buf);
}

#else /* !BLOB_V2 */

static inline void req_trap_handling(struct net_device *dev)
{
    struct msg_conf     *trapmsg;
    int                 dev_grp;

    int                 dev_nr      = GET_DEV_NR(dev->base_addr);
    int                 dev_type    = GET_DEV_TYPE(dev->base_addr);

    _DEBUG(DBG_IRQ, "..TRAP");

    /* Get the trap from the MVC... */
    if ((trapmsg = dev_trap (dev_nr)) != NULL)
    {
        /* This is an embedded MVC specific trap, so we handle it partly here... */
        if (trapmsg->oid == GEN_OID_LINKSTATE && dev_type == BIS_DT_802DOT3)
        {

#ifdef TEST_ON_POLDHU
            /* Set/Clear the Ethernet LED */
            long mask = 1;
            struct msg_conf config;

            config.operation = OPSET;
            config.data = &mask;

            if( *trapmsg->data ) /* LINK State is UP... */
            {
                config.oid = BLOB_OID_LEDSET;
                printk ("%s: link up\n", dev->name);
                netif_carrier_on (dev);
            }
            else                 /* LINK State is DOWN... */
            {
                config.oid = BLOB_OID_LEDCLEAR;
                printk ("%s: link down\n", dev->name);
                netif_carrier_off (dev);
            }

            blob_conf( &config ); /* For setting/clearing the led */
#endif /* TEST_ON_POLDHU */
        }

        /* Determine the device group based on the device type. */
        if (dev_type == BIS_DT_802DOT3)
            dev_grp = DEV_NETWORK_ETH;
        else if (dev_type == BIS_DT_802DOT11)
            dev_grp = DEV_NETWORK_WLAN;
        else
            dev_grp = 0; /* Unknown device. Use no trapgrp. */

        /* We handle all other traps in the generic part, and also send the GEN_OID_LINKSTATE up */
        mgt_response( 0, src_seq++, dev_grp, dev->ifindex,
           PIMFOR_OP_TRAP, trapmsg->oid, trapmsg->data, 100 ); /* No trapsize parameter in BLOB V1 */
    }
}
#endif /* BLOB_V2 */


/**********************************************************************
 *  req_frame_add
 *
 *  DESCRIPTION: Add frames to the device. If allocating a frame failed,
 *               the static variable frame_add_failed is set to 1. We end
 *               the interrupt service routine when this variable is set,
 *               to give the system some time to allocate memory.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:	 Inline function.
 *
 **********************************************************************/
static inline void req_frame_add(struct net_device *dev)
{
    struct msg_frame    *framep;
    int                 nr_frames = 1;

    _DEBUG((DBG_IRQ | DBG_FRAME), "..ADD");

    while (nr_frames > 0) {
        if ((framep = dev_alloc_frame (dev, 0)) != NULL) {
	    _DEBUG(DBG_IRQ, "dev_alloc_frame returned %p pointer..", framep);
	    if ((nr_frames = dev_frame_add (GET_DEV_NR(dev->base_addr), framep)) < BER_NONE) {
	        dev_free_frame(dev, framep);
	    }
	}
	else {
            _DEBUG(DBG_IRQ, "dev_alloc_frame returned NULL pointer.\n");
            frame_add_failed = 1;
            return;
        }
    }
}

/**********************************************************************
 *  req_service
 *
 *  DESCRIPTION: Service the device.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:	 Inline function.
 *
 **********************************************************************/
static inline void req_service(struct net_device *dev)
{
    _DEBUG(DBG_IRQ, "..service");
    dev_service (GET_DEV_NR(dev->base_addr));
    dev_service (DEVICE_DEBUG); /* We service the debug device here. */
}

/**********************************************************************
 *  req_watchdog_handling
 *
 *  DESCRIPTION: Watchdog handling the device.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:	 Inline function.
 *
 **********************************************************************/
static inline void req_watchdog_handling(struct net_device *dev)
{
    _DEBUG(DBG_IRQ, "..watchdog request");
    /*
     * Here one can handle watchdog pending requests. The only device
     * which watchdog is enabled by default is the BLOB device,
     * and is handled in islmvc_blob.c.
     * If you would like to use the watchdog function from any other
     * device you should change the code here the make this function.
     * and set the GEN_OID_WACHDOG for that specific device. Using this
     * mechanism one can proxy watchdog even in user space applications.
     */
    //dev_watchdog (GET_DEV_NR(dev->base_addr));
}

/**********************************************************************
 *  local_dev_handle_reqs
 *
 *  DESCRIPTION: Handle requests 'requests' for device 'dev'.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *		 requests - requests to handle
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:	 Also called from the interrupt service routine.
 *
 **********************************************************************/
static inline void local_dev_handle_reqs(struct net_device *dev, int requests)
{
    _DEBUG(DBG_IRQ, "dev_handle_reqs, dev %s, requests %d\n", dev->name, requests);

    if (requests & REQ_UNKNOWN) {
        printk (" called with unknown requests: %d!!\n", requests);
        return;
    }
    if (requests & REQFRAMERX) {
        req_frame_rx(dev);
    }
    if (requests & REQFRAMEADD) {
        req_frame_add(dev);
    }
    if (requests & REQFRAMERETURN) {
        req_frame_return(dev);
    }
    if (requests & REQSERVICE) {
        req_service(dev);
    }
    if (requests & REQTRAP) {
        req_trap_handling(dev);
    }
    if (requests & REQWATCHDOG) {
	req_watchdog_handling(dev);
    }
}


/**********************************************************************
 *  alloc_frame_pool
 *
 *  DESCRIPTION: Allocates a frame pool for the local MVC.
 *               The frame pool is allocated and the free frame list
 *               is allocated with these frames.
 *
 *  PARAMETERS:	 Pointer to the net_local structure lp.
 *
 *  RETURN:	 Zero on success, or -ENOMEM when malloc failed.
 *
 **********************************************************************/
static int alloc_frame_pool(struct net_local *lp)
{
    int i;

    DEBUG((DBG_FUNC_CALL | DBG_FRAME), "alloc_frame_pool, lp %p \n", lp);

    /* allocate frame pool */
    lp->frame_pool = kmalloc(sizeof(struct msg_frame) * lp->maxnrframes,
 			    GFP_KERNEL);
    if (lp->frame_pool == NULL)
	return -ENOMEM;

    /* add all frames to the free frame list */
    lp->frame_freelist = NULL;
    for (i = 0; i < lp->maxnrframes; i++)
    {
	lp->frame_pool[i].handle = lp->frame_freelist;
	lp->frame_freelist = &lp->frame_pool[i];
    }

    return 0;
}

/**********************************************************************
 *  frame_alloc
 *
 *  DESCRIPTION: Allocates a frame from the free frames list.
 *               The wds info in the frame is zeroed.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 framep pointer to allocated frame or NULL when alloc failed.
 *
 *  NOTE:        inline function.
 *
 **********************************************************************/
static inline struct msg_frame *frame_alloc (struct net_device *dev)
{
    struct msg_frame *framep;
    unsigned long flags;
    msg_frame *wdsframep;

    _DEBUG((DBG_FUNC_CALL | DBG_FRAME), "frame_alloc, dev %s \n", dev->name);

    /* We could change this for a spinlock mechanism */
    save_flags_cli (flags);

    framep = ((struct net_local *)dev->priv)->frame_freelist;
    if (framep)
	((struct net_local *)dev->priv)->frame_freelist = framep->handle;

    restore_flags(flags);

    /* Make sure the wds info in the frame is zeroed */
    if (framep)
    {
        wdsframep = (msg_frame*)framep;
#ifdef BLOB_V2
        memset(wdsframep->wds, 0, ETH_ALEN);
#else
        memset(wdsframep->wds_link, 0, ETH_ALEN);
#endif /* TEST_ON_POLDHU */
    }

    return framep;
}


/**********************************************************************
 *  frame_free
 *
 *  DESCRIPTION: Frees the frame pointed to by the framep.
 *               It returns the framep to the free frames list.
 *
 *  PARAMETERS:	 dev    - pointer to a linux net device struct.
 *               framep - pointer to msg_frame struct.
 *
 *  RETURN:	 Nothing.
 *
 *  NOTE:        Inline function.
 *
 **********************************************************************/
static inline void frame_free (struct net_device *dev, struct msg_frame *framep)
{
    unsigned long flags;

    _DEBUG((DBG_FUNC_CALL | DBG_FRAME), "frame_free, dev %s, framep %p \n", dev->name, framep);

    if (!framep)
        return;

    save_flags_cli (flags);

    framep->handle = ((struct net_local *)dev->priv)->frame_freelist;
    ((struct net_local *)dev->priv)->frame_freelist = framep;

    restore_flags(flags);
}


/**********************************************************************
 *  copy_wds_to_frame
 *
 *  DESCRIPTION: This copies the WDS_Mac address into the msg_frame structure.
 *
 *  PARAMETERS:	 frame pointer to msg_frame structure
 *               WDS MAC address to put into the frame.
 *
 *  RETURN:	 Nothing
 *
 *  NOTE:        inline function.
 *
 **********************************************************************/
#ifdef BLOB_V2
inline void copy_wds_to_frame(msg_frame *framep, unsigned char *WDS_Mac)
{
    /* Copy the found MAC addres in the msg_frame. */
    memcpy(framep->wds, WDS_Mac, ETH_ALEN); /* MVC v1 */
}
#else
inline void copy_wds_to_frame(msg_frame *framep, unsigned char *WDS_Mac)
{
    memcpy(framep->wds_link, WDS_Mac, ETH_ALEN); /* MVC v2 */
}
#endif


/**********************************************************************
 *  skb_to_frame
 *
 *  DESCRIPTION: Attach a prism frame structure to linux sk_buff 'skb'
 *		 and initialize all fields of the frame structure with
 *		 the current information of 'skb'.
 *
 *  PARAMETERS:	 skb - pointer to linux packet buffer.
 *
 *  RETURN:	 Pointer to the attached frame, or NULL if there wasn't
 *		 enough memory.
 *
 **********************************************************************/
/*
 * The handling of frames is changed between MVC v1 and v2.
 *
 */
#ifdef BLOB_V2
static struct msg_frame *skb_to_frame (struct sk_buff *skb)
{
    struct msg_frame *framep;

    int type = GET_DEV_TYPE(skb->dev->base_addr);

    _DEBUG((DBG_FUNC_CALL | DBG_FRAME), "skb_to_frame, skb %p, dev %s \n", skb, skb->dev->name);

    /* first malloc memory for msg_frame */
    framep = frame_alloc (skb->dev);
    if (framep != NULL)
    {
        framep->handle	 = (void *) skb;

        /* Use this mpdu mapping for empty frames. */
        if (skb->len < (2*ETH_ALEN)) {
            framep->mpdu	 = (unsigned char *) skb->data;
        }
        else {
            /* Need to do some formatting depending on the device type. */
            if (type == BIS_DT_802DOT3) {
                /*
                    Nothing special to do for an 802.3 device...
                    NOTE: This only works when the 802.3 device was started in MODE_PROMISCUOUS.
                          When started in MODE_NORMAL need to add Source and Destination address to start of skb.
                */
                _DEBUG(DBG_TX, "skb_to_frame, 802.3...");
                framep->mpdu	 = (unsigned char *) skb->data;
            }
            else if (type == BIS_DT_802DOT11) {
                /*
                    For 802.11 devices we need to do some frame formatting:
                        - Remove Destination and Source address from mpdu data.
                        - Add LLC + SNAP header to beginning of mpdu data.
                */

                /* Copy Destination and Source address into frame dest and src. */
                memcpy(framep->dest, skb->data           , ETH_ALEN);
                memcpy(framep->src,  skb->data + ETH_ALEN, ETH_ALEN);

                /* Remove the Destination and Source Address from the skb. */
                skb_pull(skb, 2 * ETH_ALEN);

                /* convert the ethernet frame to WLAN */
                llc_to_wlan( skb );

                /* Okay, we're done. Point mpdu pointer to start of skb data. */
                framep->mpdu	 = (unsigned char *) skb->data;
            }
            else {
                printk(KERN_ERR "skb_to_frame: Unknown device type.\n");
                /* Don't know how to format. Callers responsibility to free the skb. */
                return NULL;
            }
        }

        framep->size	 = skb->len;
	framep->priority = -1; /* Priority isn't implemented yet.
                                  We should map the skb->priority to the frame priority here. */
    }
    else
        printk(KERN_WARNING "Frame pool full? \n");

#if (NET_DEBUG & DBG_SKB)
    memset (skb->head, 0, skb_headroom(skb));
    memset (skb->tail, 0, skb_tailroom(skb));
#endif

#if (NET_DEBUG & DBG_FRAME_SANITY)
    check_frame_skb(framep, __FILE__, __LINE__);
#endif

    return framep;
}

#else /* !BLOB_V2 */

static struct msg_frame *skb_to_frame (struct sk_buff *skb)
{
    struct msg_frame *framep;

    _DEBUG((DBG_FUNC_CALL | DBG_FRAME), "skb_to_frame, skb %p \n", skb);

    /* first malloc memory for msg_frame */
    framep = frame_alloc (skb->dev);
    if (framep != NULL)
    {
	framep->handle	 = (void *) skb;
	framep->mpdu	 = (unsigned char *) MAP_CACHED((unsigned)skb->data);
        framep->size	 = skb->len;
	framep->priority = -1; /* Priority isn't implemented yet.
                                  We should map the skb->priority to the frame priority here. */
        framep->bc_domain = 0;
    }
    else
        printk(KERN_WARNING "Frame pool full? \n");

#if (NET_DEBUG & DBG_SKB)
    memset (skb->head, 0, skb_headroom(skb));
    memset (skb->tail, 0, skb_tailroom(skb));
#endif
     return framep;
}
#endif /* BLOB_V2 */

/**********************************************************************
 *  frame_to_skb
 *
 *  DESCRIPTION: Update the fields of the linux sk_buff assigned to
 *		 frame 'framep' just received from the MVC.
  *		 The frame is freed.
 *
 *  PARAMETERS:	 framep - pointer to frame received from MVC.
 *
 *  RETURN:	 Pointer to the updated sk_buff that was assigned to the
 *		 frame.
 *
 **********************************************************************/
/*
 * The handling of frames is changed between MVC v1 and v2.
 *
 */
#ifdef BLOB_V2
static struct sk_buff *frame_to_skb (struct msg_frame *framep)
{
    struct sk_buff *skb;
    int offset, type;
    unsigned char *src;

    _DEBUG((DBG_FUNC_CALL | DBG_FRAME | DBG_IRQ), "frame_to_skb, framep %p \n", framep);

#if (NET_DEBUG & DBG_FRAME_SANITY)
    check_frame_skb(framep, __FILE__, __LINE__);
#endif

    skb = framep->handle;

    type = GET_DEV_TYPE(skb->dev->base_addr);

#if (NET_DEBUG & DBG_SKB)
    /* Some sanity checking on the frame. */
    if ((unsigned)framep->size > 1600 )
        printk("Frame size (%d) is > 1600. Ok? \n", framep->size);
    if (!framep->mpdu)
        printk("framep->mpdu == NULL !!!\n");
#endif

    /* determine shift in framep->mpdu between add and rx */
    offset = framep->mpdu - skb->data;

    skb_reserve (skb, offset);
    skb_put (skb, framep->size);

#if (NET_DEBUG & DBG_SKB)
    if (skb_headroom(skb) < 2*ETH_ALEN)
        printk("frame_to_skb, not enough headroom (%d) to add SA and DA\n", skb_headroom(skb));
#endif

    /* Depending on the device type, we need to do some frame formatting. */
    if (type == BIS_DT_802DOT3) {
        _DEBUG(DBG_RX, "frame_to_skb, 802.3...");
        /* Nothing special to do. */
    }
    else if (type == BIS_DT_802DOT11) {
        _DEBUG(DBG_RX, "frame_to_skb, 802.11...");

        /* Convert the WLAN frame back to ethernet LLC format */
        wlan_to_llc( skb );

        /* Add Source and Destination address to front of the skb. */
        memcpy(skb_push(skb, ETH_ALEN), framep->src, ETH_ALEN);
        memcpy(skb_push(skb, ETH_ALEN), framep->dest, ETH_ALEN);
    }
    else {
        printk(KERN_ERR "frame_to_skb: Unknown device type.\n");
        /* Don't know how to format. Drop it. Free frame & skb. */
        frame_free (skb->dev, framep);
        dev_kfree_skb(skb);
        return NULL;
    }

    /* determine offset between IP header and a word boundary */
    offset = (int) (skb->data + 6 + 6 + 2) & 0x03;

    /* if there is any, shift the data */
    if (offset)
    {
	struct net_local *lp = (struct net_local *)skb->dev->priv;
	lp->rx_misaligned++;

	if (skb_headroom (skb) < offset)
	{
	    if (skb_tailroom (skb) < (4 - offset))
		printk(KERN_NOTICE "Unaligned skb\n");
	    else
		offset = 4 - offset;
	}
	else
	    offset = -offset;
#if (NET_DEBUG & DBG_RX)
	printk ("frame_to_skb, need to shift data %d\n", offset);
#endif
	src = skb->data;
	skb_reserve (skb, offset);
	memmove (skb->data, src, skb->len);
    }
    skb->priority = -1; /* Priority isn't implemented yet. Add mapping between skb and frame priority here. */

#if (NET_DEBUG & DBG_FRAME_SANITY)
    check_frame_skb(framep, __FILE__, __LINE__);
#endif

    frame_free (skb->dev, framep);

    return skb;
}

#else /* !BLOB_V2 */

static struct sk_buff *frame_to_skb (struct msg_frame *framep)
{
     struct sk_buff *skb;
    int offset;
    unsigned char *src;

    _DEBUG((DBG_FUNC_CALL | DBG_FRAME), "frame_to_skb, framep %p\n", framep);

    skb = framep->handle;

    /* first map pointer back in uncached area */
    MAP_UNCACHED((unsigned)framep->mpdu);

    /* determine shift in framep->mpdu between add and rx */
    offset = framep->mpdu - skb->data;

    skb_reserve (skb, offset);
    skb_put (skb, framep->size);

    /* determine offset between IP header and a word boundary */
    offset = (int) (skb->data + 6 + 6 + 2) & 0x03;

    /* if there is any, shift the data */
    if (offset)
    {
	struct net_local *lp = (struct net_local *)skb->dev->priv;
	lp->rx_misaligned++;

	if (skb_headroom (skb) < offset)
	{
	    if (skb_tailroom (skb) < (4 - offset))
		printk(KERN_NOTICE "Unaligned skb\n");
	    else
		offset = 4 - offset;
	}
	else
	    offset = -offset;
#if (NET_DEBUG & DBG_RX)
	printk ("frame_to_skb, need to shift data %d\n", offset);
#endif
	src = skb->data;
	skb_reserve (skb, offset);
	memmove (skb->data, src, skb->len);
    }
    skb->priority = -1; /* Priority isn't implemented yet. Add mapping between skb and frame priority here. */

    frame_free (skb->dev, framep);

    return skb;
}
#endif /* BLOB_V2 */

/**********************************************************************
 *  dev_alloc_frame
 *
 *  DESCRIPTION: Allocate a frame and a sk_buff with MTU size 'size' for
 *		 device 'dev'. Set all fields of the frame and the
 *		 sk_buff. If size equals 0 then the maximum MTU size is
 *		 used. The head and the tail room for the frame (and
 *		 sk_buff) is equal to the maximum head and maximum tail
 *		 room for all devices.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *		 size - the MTU size for the frame.
 *
 *  RETURN:	 Pointer to the allocated frame or NULL on error.
 *
 **********************************************************************/
static inline struct msg_frame *dev_alloc_frame (struct net_device *dev, int size)
{
    struct sk_buff *skb;
    struct msg_frame *framep;
    int bufsize;
    struct net_local *lp;

    _DEBUG((DBG_FUNC_CALL | DBG_IRQ | DBG_FRAME), "dev_alloc_frame, dev %s, size %d \n", dev->name, size);

    /* first allocate a sk_buff with enough head and tail room */
    lp = (struct net_local *)dev->priv;
    bufsize = lp->maxhead + 2 +
		    (size ? size : lp->maxmtu) + lp->maxtail;
    if ((skb = dev_alloc_skb (bufsize)) == NULL) {
        _DEBUG(DBG_IRQ | DBG_FRAME, "dev_alloc_frame, dev_alloc_skb failed... \n");
        return NULL;
    }

    /* fill in some sk_buff fields */
    skb->dev = dev;
    skb_reserve (skb, lp->maxhead + 2); /* word align IP header */

    if ((framep = skb_to_frame(skb)) == NULL)
        dev_kfree_skb(skb);

    return framep;
}


/**********************************************************************
 *  dev_free_frame
 *
 *  DESCRIPTION: Free MVC frame 'frame' and the attached sk_buff.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *               frame - address of frame to free
 *
 *  RETURN:	 Nothing.
 *
 **********************************************************************/
static inline void dev_free_frame (struct net_device *dev, struct msg_frame *frame)
{
    _DEBUG((DBG_FUNC_CALL | DBG_FRAME), "dev_free_frame, dev %s, frame %p \n", dev->name, frame);

#if (NET_DEBUG & DBG_FRAME_SANITY)
    check_frame_skb(frame, __FILE__, __LINE__);
#endif

    if (frame->handle)
	dev_kfree_skb_any (frame->handle);
    frame_free (dev, frame);
}

/**********************************************************************
 *  dev_copy_expand_skb
 *
 *  DESCRIPTION: Allocate a new sk_buff and copy the data of sk_buff
 *		 'skb'. The new sk_buff will have the required head and
 *		 tail room for device 'dev'.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *		 skb - the sk_buff to copy the data from
 *
 *  RETURN:	 Pointer to the new allocated sk_buff or NULL on error.
 *
 **********************************************************************/
static struct sk_buff *dev_copy_expand_skb (struct net_device *dev,
				const struct sk_buff *skb)
{
    int bufsize;
    struct sk_buff *newskb;
    struct net_local *lp = (struct net_local *)dev->priv;

    _DEBUG((DBG_FUNC_CALL | DBG_FRAME), "dev_copy_expand_skb, dev %s, skb %p \n", dev->name, skb);

#ifdef BLOB_V2
    bufsize = skb->len + lp->dev_elem->header + lp->dev_elem->trailer;
#else
    bufsize = skb->len + lp->dev_desc->header + lp->dev_desc->trailer;
#endif
    if ((newskb = dev_alloc_skb (bufsize)) == NULL)
	return NULL;

    /* reserve head room */
#ifdef BLOB_V2
    skb_reserve (newskb, lp->dev_elem->header);
#else
    skb_reserve (newskb, lp->dev_desc->header);
#endif

    /* set the tail pointer and length */
    skb_put(newskb, skb->len);

    /* copy the data */
    memcpy(newskb->data, skb->data, skb->len);
    newskb->priority = skb->priority;

    newskb->dev = dev;
    return newskb;
}

/**********************************************************************
 *  llc_to_wlan
 *
 *  DESCRIPTION: Converts the ethernet LLC frame to SNAP for WLAN
 *
 *  PARAMETERS:	 skb - buffer with ethernet frame to be converted
 *
 *  RETURN:	 Nothing.
 *
 **********************************************************************/
/*
 * The conversion of frames is removed from MVC v2
 *
 */
#ifdef BLOB_V2
static int llc_to_wlan (struct sk_buff *skb)
{
    unsigned int ethertype;

    DEBUG((DBG_FUNC_CALL | DBG_FRAME), "llc_to_wlan, skb %p\n", skb);

    ethertype = (unsigned int) ((skb->data[0] << 8) | skb->data[1]);

    /*
        Ethertype check should be 1500, but with 802.1Q it is possible that frames are
        actually longer than 1500 bytes and thus regarded as ethertype frames. 1536 leaves
        enough space for long frames and 1536 (XEROX NS IDP) is the first registered ethertype
    */
    if( ethertype >= 1536 )
    {
        DEBUG((DBG_FRAME), "llc_to_wlan, Ethernet II type %04x, converting to SNAP\n", ethertype);

        /*
            Make room for LLC (3 bytes) and for OUI field (3 bytes) of SNAP header.
            Leave SNAP Type field unchanged.
        */
        skb_push(skb, 6);

        /* LLC */
        skb->data[0] = 0xAA; /* DSAP (1 byte) */
        skb->data[1] = 0xAA; /* SSAP (1 byte) */
        skb->data[2] = 0x03; /* Control (1 byte) */

        /* SNAP */
        skb->data[3] = 0x00; /* OUI (3 bytes) */
        skb->data[4] = 0x00;

        /*
            Consult the Selective Translation Table (802.1H), which contains
            ethernet types 8137 and 80F3
        */
        if( ethertype == 0x8137 || ethertype == 0x80F3 )
            /* Bridge tunnel encapsulation */
            skb->data[5] = 0xF8;
        else
            skb->data[5] = 0x00;
    }
    else
    {
        DEBUG((DBG_FRAME), "llc_to_wlan, Length %04x, Found 802.2, 802.3 or SNAP [%02x%02x]\n",
            ethertype, skb->data[2], skb->data[3] );

        /* Remove the length field from the skb. */
        skb_pull(skb, 2);
    }

    return 0;
}
#endif

/**********************************************************************
 *  wlan_to_llc
 *
 *  DESCRIPTION: Converts the WLAN (SNAP) frame to ethernet LLC
 *
 *  PARAMETERS:	 skb - buffer with WLAN frame
 *
 *  RETURN:	 Nothing.
 *
 **********************************************************************/
/*
 * The conversion of frames is removed from MVC v2
 *
 */
#ifdef BLOB_V2
static int wlan_to_llc (struct sk_buff *skb)
{
    _DEBUG((DBG_FUNC_CALL | DBG_FRAME), "wlan_to_llc, frame %p, skb %p\n", framep, skb);

    /*
        802.1H: Check whether the WLAN frame has a SNAP header.
        SNAP can always be converted to Ethernet II except for the protocols
        in the Selective Translation Table
    */
    if( (skb->data[0] == 0xAA) && (skb->data[1] == 0xAA) && (skb->data[2] == 0x03) &&
        (skb->data[3] == 0x00) && (skb->data[4] == 0x00) &&
        ((skb->data[5] == 0x00) || (skb->data[5] == 0xF8)))
    {
        /*  Check the Selective Translation Table */
        if( ((skb->data[6] == 0x81) && (skb->data[7] == 0x37)) ||
            ((skb->data[6] == 0x80) && (skb->data[7] == 0xF3)) )
        {
            /* Check also the Bridge Tunnel Encapsulation */
            if( skb->data[5] == 0xF8 )
            {
                /*
                    Remove LLC (3 bytes) + OUI field (3 bytes) of SNAP header.
                    Don't remove the type field (2 bytes) of the SNAP header.
                */
                skb_pull(skb, 6);
            }
            else
            {
                /*  Add the length of the buffer */
                skb_push( skb, 2);
                skb->data[0] = (unsigned char) ((skb->len-2)>>8);
                skb->data[1] = (unsigned char) (skb->len-2);
            }
        }
        else
        {
            /*
                Remove LLC (3 bytes) + OUI field (3 bytes) of SNAP header.
                Don't remove the type field (2 bytes) of the SNAP header.
            */
            skb_pull(skb, 6);
        }

        DEBUG((DBG_FRAME), "wlan_to_llc, Removed SNAP header, Ethertype still present\n");

    }
    else
    {
        DEBUG((DBG_FRAME), "wlan_to_llc, Ethertype %02x%02x, preappend Length %04x\n",
            skb->data[0], skb->data[1], skb->len );

        /*  Preappend the length of the buffer */
        skb_push( skb, 2);
        skb->data[0] = (unsigned char) ((skb->len-2)>>8);
        skb->data[1] = (unsigned char) (skb->len-2);
    }

    return 0;
}
#endif

/****************************************************************
*								*
*    		End of frame handling block                     *
*               (driver configurator module)			*
*								*
***************************************************************/


/****************************************************************
 *								*
 *    		End of driver configurator functions		*
 *								*
 ***************************************************************/

/**********************************************************************
 *  local_set_mac_addr
 *
 *  DESCRIPTION: This sets/changes the MAC address on the interface.
 *               A MAC address containing all zeroes is used to reset
 *               the MAC address to it's default.
 *
 *  PARAMETERS:	 dev  - pointer to a linux net device struct.
 *               addr - pointer to new MAC address
 *
 *  RETURN:	 0 on succes.
 *               -EINVAL on error.
 *
 **********************************************************************/
#ifdef HAVE_SET_MAC_ADDR
static int local_set_mac_addr(struct net_device *dev, void *addr)
{
    unsigned char mac_addr[ETH_ALEN];
    unsigned char mac_null_addr[ETH_ALEN];

    DEBUG(DBG_FUNC_CALL | DBG_MGMT, "local_set_mac_addr, dev %s, addr %s \n", dev->name, (char*) addr);

    memcpy(mac_addr, addr, ETH_ALEN);
    memset(mac_null_addr, 0, ETH_ALEN);

    /* A MAC address containing all zeroes is used to reset the MAC address to it's default. */
    if (memcmp(mac_addr, mac_null_addr, ETH_ALEN) == 0)
    {

        /* Get the MAC address from the PDA. Function returns pointer to MACAddress. */
        memcpy(mac_addr, pda_get_mac_address(), ETH_ALEN);

        if (mac_addr == NULL)
        {
            printk(KERN_ERR "Couldn't get MAC address for %s from PDA...\n", dev->name);
            return -EINVAL;
        }
    }

    /* We don't allow multicast/broadcast MAC addresses. */
    if (mac_addr[0] & 1)
        return -EINVAL;

    /* Send a message to the MVC that we want to use this MAC address... */
    if (islmvc_send_mvc_msg(dev, OPSET, GEN_OID_MACADDRESS, (long*)mac_addr, ETH_ALEN) < 0);
    {
        printk("local_set_mac_addr, couldn't change MAC address %s for dev %s ",
               hwaddr2string(mac_addr), dev->name);
        return -EINVAL;
    }

    /* Okay, we succeeded, so use this MAC address from now on... */
    memcpy(dev->dev_addr, mac_addr, ETH_ALEN);
    DEBUG(DBG_MGMT, "MAC address changed to %s. ", hwaddr2string(mac_addr));

    return 0;
}
#endif /* HAVE_SET_MAC_ADDR */


/**********************************************************************
 *  local_net_get_stats
 *
 *  DESCRIPTION: Get the current statistics.
 *               This may be called with the card open or closed.
 *
 *  PARAMETERS:	 dev - pointer to a linux net device struct.
 *
 *  RETURN:	 struct net_device stats pointer
 *
 **********************************************************************/
static struct net_device_stats *local_net_get_stats(struct net_device *dev)
{
    long mvc_errors;

    int dev_nr = GET_DEV_NR(dev->base_addr) - 1;
    static long old_rx_failed[3]  = { 0, 0, 0 };
    static long old_rx_dropped[3] = { 0, 0, 0 };

    struct net_local *lp = (struct net_local *)dev->priv;
    struct net_device_stats *stats = &lp->stats;

    _DEBUG(DBG_FUNC_CALL | DBG_MGMT, "local_net_get_stats, dev %s \n", dev->name);

    /*
        Most statistics are updated in the driver code.
        However, we can't update error counters for received packets that
        were dropped or erroneous, since we don't get those packets from the MVC.
        So, we ask the MVC for those errors...
    */
    if (!islmvc_send_mvc_msg(dev, OPGET, GEN_OID_MSDURXFAILED, &mvc_errors, sizeof(long))) {
        stats->rx_errors += (mvc_errors - old_rx_failed[dev_nr]); /* Only add the difference from the previous count. */
        old_rx_failed[dev_nr] = mvc_errors;
    }

    if (!islmvc_send_mvc_msg(dev, OPGET, GEN_OID_MSDURXDROPPED, &mvc_errors, sizeof(long))) {
        stats->rx_dropped += (mvc_errors - old_rx_dropped[dev_nr]); /* Only add the difference from the previous count. */
        old_rx_dropped[dev_nr] = mvc_errors;
    }

    return stats;
}



/***************************************************************
*								*
*    		Start of local debugging functions  		*
*								*
***************************************************************/

#if (NET_DEBUG & (DBG_TX | DBG_RX | DBG_ERROR))
/**********************************************************************
 *  dbg_print_frame
 *
 *  DESCRIPTION: Print the fields of prism frame 'framep' and the data.
 *
 *  PARAMETERS:	 framep - pointer to a prism frame
 *
 *  RETURN:	 Nothing.
 *
 **********************************************************************/
static void dbg_print_frame (struct msg_frame *framep)
{
    int offset;
    struct sk_buff *skb;

    skb = framep->handle;
    printk ("framep: size %d, status %d, mpdu %p, skb->data %p, skb %p, skb->len %d \n",
	    framep->size, framep->status, framep->mpdu, skb->data, skb, skb->len);
    printk ("pr_frame: skb->head %p, skb->tail %p, skb->dev->name %s \n",
            skb->head, skb->tail, skb->dev->name);
    for (offset = 0; offset < framep->size; offset++)
	printk ("%02X ", framep->mpdu[offset]);
    printk ("\n");
}
#endif /* (NET_DEBUG & (DBG_TX | DBG_RX | DBG_ERROR)) */


#if (NET_DEBUG & DBG_SKB)
/**********************************************************************
 *  dbg_frame_filter_check
 *
 *  DESCRIPTION: Filter frames not of interest for the Linux IP stack.
 *		 (unicasts not containing our MAC address and Intersil
 *		 debug frames). Perform some checks on the frame and
 *		 print warnings if something suspious is found.
 *
 *  PARAMETERS:	 dev    - pointer to a linux net device struct.
 *		 framep - pointer to the received prism frame
 *
 *  RETURN:	 0 if the frame is absorbed by this function, otherwise 1.
 *
 **********************************************************************/
static int dbg_frame_filter_check (struct net_device *dev, struct msg_frame *framep)
{
    struct sk_buff *skb;
    int offset;
    unsigned char *src;
    int mtu;

    /* filer out unicast frames not directed to our MAC address */
    if ((framep->mpdu[0] & 0x01) == 0)
    {
	for (offset = 0; offset < ETH_ALEN; offset++)
	{
	    if (framep->mpdu[offset] != dev->dev_addr[offset])
	    {
		dev_free_frame (dev, framep);
		return 0;
	    }
	}
    }

    /* filer out Intersil debug frames */
    if ((framep->mpdu[12] == 0x88) && (framep->mpdu[13] == 0x27))
    {
	dev_free_frame (dev, framep);
	return 0;
    }

    skb = framep->handle;

    /* first map pointer back in 0x08000000 area */
#ifdef TEST_ON_POLDHU
    MAP_UNCACHED((unsigned)framep->mpdu);
#endif /* TEST_ON_POLDHU */

    /* determine shift in framep->mpdu between add and rx */
    offset = framep->mpdu - skb->data;

    if (offset)
    {
	printk ("frame_to_skb: blob changed mpdu: %d!!!\n", offset);
	dbg_print_frame (framep);
    }
    if (skb_tailroom (skb) < framep->size)
	printk ("frame_to_skb: NOT enough room!! need %d, tailroom %d, len %d\n",
		framep->size, skb_tailroom (skb), skb->len);

#ifdef BLOB_V2
    mtu = ((struct net_local *)dev->priv)->dev_elem->mtu;
#else
    mtu = ((struct net_local *)dev->priv)->dev_desc->mtu;
#endif
    offset = dbg_check_area (skb->head, framep->mpdu - skb->head, "header");
    for (src = skb->end-1; (*src == 0) && (src > framep->mpdu+mtu); src--)
	;
    if (src > framep->mpdu+mtu)
    {
	dbg_check_area (framep->mpdu+mtu, (src - framep->mpdu+mtu) + 1, "trailer");
	offset = 1;
    }
    if (offset)
	dbg_print_frame (framep);
    return 1;
}


/**********************************************************************
 *  dbg_dump_hex_assci
 *
 *  DESCRIPTION: Dump the content of buffer 'buf' to the screen. The
 *		 data is presented in hexadecimal and ASSCI form, 16
 *		 bytes on each line.
 *
 *  PARAMETERS:	 buf  - pointer to buffer to display
 *		 size - size of the buffer
 *
 *  RETURN:	 Nothing.
 *
 **********************************************************************/
static void dbg_dump_hex_assci (unsigned char *buf, int size)
{
    int linesize, i;
    unsigned char *p;

    while (size)
    {
	linesize = size > 16 ? 16 : size;
	size -= linesize;
	for (p = buf, i = 0; i < linesize; i++, p++)
	    printk (" %02X", *p);
	for (; i < 17; i++)
	    printk ("	");
	for (i = 0; i < linesize; i++, buf++)
	{
	    if (*buf > 31 && *buf < 128)
		printk ("%c", *buf);
	    else
		printk (".");
	}
	printk ("\n");
    }
}


/**********************************************************************
 *  dbg_check_area
 *
 *  DESCRIPTION: Check if a memory area contains all zero's. If not,
 *		 print a warning, the address that isn't zero and dump
 *		 the content of the rest of the area on the screen.
 *
 *  PARAMETERS:	 p	   - start address of the area to check
 *		 size	   - size of the area
 *		 area_name - name of the area that is displayed in the
 *			     warning message
 *
 *  RETURN:	 0 if the whole area contains zero's, otherwise 1.
 *
 **********************************************************************/
static int dbg_check_area (unsigned char *p, int size, char *area_name)
{
    while (size)
    {
	if (*p)
	{
	    printk ("WARNING: %s changed at %p!\n", area_name, p);
	    dbg_dump_hex_assci (p, size);
	    return 1;
	}
	p++;
	size--;
    }
    return 0;
}
#endif /* (NET_DEBUG & DBG_SKB) */

#if (NET_DEBUG & DBG_FRAME_SANITY)

#define LINUX_VALID_ADDR(addr)  ((unsigned long)addr > 0x00040000 && (unsigned long)addr < 0x00800000)

/**********************************************************************
 *  check_frame_skb
 *
 *  DESCRIPTION: Do some sanity checking on the frame. This function
 *               checks if the pointers and lengths are in a valid range.
 *
 *  PARAMETERS:	 framep	   - pointer to frame struct to check.
 *		 file	   - name of the file the function was called from.
 *		 line      - line number from wich this function is called.
 *
 *  RETURN:	 0 if the whole area contains zero's, otherwise 1.
 *
 **********************************************************************/
static void check_frame_skb(struct msg_frame *framep, char *file, int line)
{
        struct sk_buff *skb;
        int err = 0;
        int print_size;

        if(!LINUX_VALID_ADDR(framep)) {
                printk("check_frame_skb (%s:%d): illegal framep %p\n", file, line,
                        framep);
                err = 1;
        }

        if(!LINUX_VALID_ADDR(framep->handle)) {
                printk("check_frame_skb (%s:%d): illegal framep->handle %p\n", file, line,
                        framep->handle);
                err = 1;
        }

        if(!LINUX_VALID_ADDR(framep->mpdu)) {
                printk("check_frame_skb (%s:%d): illegal framep->mpdu %p\n", file, line,
                        framep->mpdu);
                err = 1;
        }

        skb = framep->handle;

        if(!LINUX_VALID_ADDR(skb->head)) {
                printk("check_frame_skb (%s:%d): illegal skb->head %p\n", file, line,
                        skb->head);
                err = 1;
        }

        if(!LINUX_VALID_ADDR(skb->tail)) {
                printk("check_frame_skb (%s:%d): illegal skb->tail %p\n", file, line,
                        skb->tail);
                err = 1;
        }

        if(!LINUX_VALID_ADDR(skb->data)) {
                printk("check_frame_skb (%s:%d): illegal skb->data %p\n", file, line,
                        skb->data);
                err = 1;
        }

        if(skb->len > 1600 || framep->size > 1600) {
                printk("check_frame_skb (%s:%d): illegal length skb->len %d framep->len %d\n",
                        file, line, skb->len, framep->size);
                err = 1;
        }

        if(err) {
                int offset;

                printk ("framep: size %d, status %d, mpdu %p, skb %p\n",
                        framep->size, framep->status, framep->mpdu, skb);
                printk ("skb->head %p, skb->data %p, skb->tail %p, skb->len %d, skb->dev->name %s \n",
                        skb->head, skb->data, skb->tail, skb->len, skb->dev->name);

                if(framep->size < 20)
                        print_size = 20;
                else if(framep->size > 1600)
                        print_size = 1600;
                else
                        print_size = framep->size;

                for (offset = 0; offset < print_size; offset++)
                        printk ("%02X ", framep->mpdu[offset]);
                printk ("\n");

                __bug(file, line, NULL);
        }
}

#endif /* (NET_DEBUG & DBG_FRAME_SANITY) */


/***************************************************************
*								*
*    		End of local debugging functions  		*
*								*
***************************************************************/

