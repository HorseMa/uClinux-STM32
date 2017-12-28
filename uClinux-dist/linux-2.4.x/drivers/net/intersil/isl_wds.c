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

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/isil_netlink.h>
#include <linux/netdevice.h>

#include "isl_wds.h"

#define DEBUG(x,args...)
#define _DEBUG(x, args...)


/* End of include files */

/****************************************************************
 *								*
 *    			Begin of WDS functions			*
 *								*
 ***************************************************************/


/**********************************************************************
 *  add_wds_link
 *
 *  DESCRIPTION: Add a WDSlink device to the system.
 * 		  On adding a WDSLINK to the MVC the following should be done:
 * 		  - Add a new device to the WDSlink devices list.
 * 		  - Create a device for this WDSlink (dev_alloc wds%d)
 * 		  - Register the just created dummy device (register_netdevice)
 *               - Bring the just created device up and running by calling dev_open.
 * 		  - Somehow the brigde needs to be told to accept the new interface.
 *
 *  PARAMETERS:  - MACAddress, MACAddress to add.
 *               - dev points to device WDSLink need to be added to.
 *
 *
 *  RETURN:	  0 if the device created OK, otherwise -1.
 *
 **********************************************************************/
int add_wds_link(struct net_device *dev, struct wds_priv *wdsp, unsigned char *mac_addr, char *brifname )
{  
    int err = 0;
    int i=0, j=0;

    struct net_device *wds_dev;

    DEBUG(DBG_FUNC_CALL | DBG_WDS, "add_wds_link \n");
    
    if ( ( dev == NULL ) || ( wdsp == NULL ) || ( mac_addr == NULL ) || ( brifname == NULL ) )
    {
        printk("add_wds_link(%lx, %lx, %lx, %lx) wrong parameter(s)!\n", 
               (unsigned long)dev, (unsigned long)wdsp, (unsigned long)mac_addr, (unsigned long)brifname );
        return -1;
    }
    /* If the parent device isn't bridged, the WDS will not work */
    if (dev->br_port == NULL)
    {
        printk("add_wds_link: parent device (%s) not bridged; can't add WDS link\n", dev->name );
        return -2;
    }
    /* First check if MAX_WDS_LINKS is reached */
    if (wdsp->current_nof_wds_links == MAX_WDS_LINKS)
    {
    	/* Max number of links reached */
    	printk( "add_wds_link: max number of WDS links (%d) reached!\n", MAX_WDS_LINKS );
    	return -3;
    }

    /* If this MAC address is our own mac address */
    if(memcmp(dev->dev_addr, mac_addr, 6) == 0)
    {
        DEBUG(DBG_WDS | DBG_ERROR,"add_wds_link: Cannot add WDS link for own MAC address: %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n",
                  mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        return -4;
    }
    
    /* Check if this MAC addres is already in the list. */
    for (i=0 ; i < MAX_WDS_LINKS; i++)
    {   
    	if (((struct wds_net_local*)(wdsp->wds_dev[i])) == NULL)
            continue;
        
        /* If this MAC address is already linked don't add new WDS device */
        if(memcmp(((struct wds_net_local*)(wdsp->wds_dev[i]->priv))->wds_mac, mac_addr, 6) == 0)
    	{
    	    DEBUG(DBG_WDS | DBG_ERROR,"add_wds_link: WDS link for MAC: %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x already active!\n",
    		      mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    	    return -5;
    	}
    }

    /* Find an empty field to add WDS device */
    for (i=0 ; i< MAX_WDS_LINKS; i++)
    {
    	if (wdsp->wds_dev[i] == NULL)
    	{
    	    /* We have found an empty structure
    	     * So we can actualy create a new device for this link
    	     */
            sprintf( brifname, "%s_%d", dev->name, i ); 
    	    if ( !(wdsp->wds_dev[i] = dev_alloc(brifname, &err) ) )
    	    {
                DEBUG(DBG_WDS | DBG_ERROR,"add_wds_link: dev_alloc() for WDS failed!\n");
    	        return -6;
    	    }
    	    /* Get a pointer to this WDS device */
    	    wds_dev = wdsp->wds_dev[i];
    
    	    /* May be overridden by piggyback drivers */
    	    ether_setup(wds_dev);
    
    	    /* Initialize the device priv structure. */
            wds_dev->priv = kmalloc(sizeof(struct wds_net_local), GFP_KERNEL);
            if (wds_dev->priv == NULL) {
                return -7;
            }
            memset(wds_dev->priv, 0, sizeof(struct wds_net_local));
    	    /* Set the parent device for this WDS link */
    	    ((struct wds_net_local*)(wds_dev->priv))->parent_dev = dev;
            
    	    /* Set WDS MAC in priv structure */
    	    printk("add_wds_link: wds_mac=");
    	    for (j = 0; j < 6; j++)
                printk(" %2.2x", ((struct wds_net_local*)(wds_dev->priv))->wds_mac[j] = mac_addr[j] );
    	    printk("\n");
    
    	    /* Retrieve and print the ethernet address. */
    	    printk(" eth_mac=");
    	    for (j = 0; j < 6; j++)
                printk(" %2.2x", wds_dev->dev_addr[j] = ((struct wds_net_local*)(wds_dev->priv))->parent_dev->dev_addr[j] );
    	    printk("\n");

    	    wds_dev->addr_len = 6;
    
    	    /* Override the network functions we need to use */
    	    wds_dev->open		        = wds_net_open;
    	    wds_dev->stop		        = wds_net_close;
    	    wds_dev->get_stats		    = wds_net_get_stats;
    	    wds_dev->hard_start_xmit	= wds_net_send_packet;
    	    wds_dev->do_ioctl		    = wds_net_ioctl;
    
    	    /* Make ifconfig display some details */
    	    wds_dev->base_addr = 0;
    
    	    /* No need to lock cause the net_ioctl is already locked */
    	    err = register_netdevice(wds_dev);
    	    if (err)
    	    {
        		DEBUG(DBG_WDS | DBG_ERROR,"add_wds_link: register_netdev() for WDS failed!\n");
        		return -8;
    	    }
    	    DEBUG(DBG_WDS, "add_wds_link: registered device name: %s\n", wds_dev->name);
    
    	    /* Bring the device up, when the root device is UP... */
    	    if (dev->flags & IFF_UP)
    	    {
        		err = dev_open(wds_dev);
        		if (err)
        		{
        		    DEBUG(DBG_WDS | DBG_ERROR,"add_wds_link: dev_open() for WDS failed!\n");
        		    return -9;
        		}
        		DEBUG(DBG_WDS,"add_wds_link: bringing up device name: %s\n", wds_dev->name);
    	    }
    	    else
    	    {
                DEBUG(DBG_WDS,"add_wds_link: NOT bringing up device name: %s, cause root device isn't up yet...\n", wds_dev->name);
    	    }

    	    wdsp->current_nof_wds_links++;
    	    return 0;
    	}
    }
    /* This should not happen*/
    DEBUG(DBG_WDS | DBG_ERROR,"add_wds_link: WDS link administration screwed\n");
    return -13;
}

/**********************************************************************
 *  del_wds_link
 *
 *  DESCRIPTION: Del a WDSlink device to the system.
 * 		On deleting a WDSLINK from the MVC the following should be done:
 *      - Bring the device down by calling dev_close.
 * 		- Unregister the asociated dummy device (unregister_netdevice)
 * 		- Free the allocated space for this device
 * 		- Remove this device from the WDSlink devices list.
 *
 *  PARAMETERS:	 - mac_addr, mac_addr to remove.
 *               - dev points to device WDSLink need to be removed from.
 *
 *
 *  RETURN:	 0 if the device created OK, otherwise 1.
 *
 **********************************************************************/
int del_wds_link(struct net_device *dev, struct wds_priv *wdsp, unsigned char *mac_addr, char *brifname )
{
    int err = 0;
    int i = 0;

    struct net_device *wds_dev;

    DEBUG(DBG_FUNC_CALL, "del_wds_link \n");
    
    if ( ( dev == NULL ) || ( wdsp == NULL ) || ( mac_addr == NULL ) || ( brifname == NULL ) )
    {
        printk("del_wds_link(%lx, %lx, %lx, %lx) wrong parameter(s)!\n", 
               (unsigned long)dev, (unsigned long)wdsp, (unsigned long)mac_addr, (unsigned long)brifname );
        return -1;
    }
    
    /* Check if this MAC addres is in the WDS link list. */
    for (i=0 ; i< MAX_WDS_LINKS; i++)
    {
    	if( (wdsp->wds_dev[i]) && 
            (memcmp(((struct wds_net_local*)(wdsp->wds_dev[i]->priv))->wds_mac, mac_addr, 6 ) == 0 ) )
    	{
            wds_dev = (struct net_device *)wdsp->wds_dev[i];
            strcpy( brifname, wds_dev->name ); 
            
            /* Bring the device down */
    	    err = dev_close(wds_dev);
    	    if (err)
    	    {
    	    	DEBUG(DBG_WDS | DBG_ERROR,"del_wds_link: dev_close() for WDS failed!\n");
    	    	return err;
    	    }
    	    /* No need to lock cause the net_ioctl is already locked */
    	    
            err = unregister_netdevice(wds_dev);
    	    if (err != 0)
    	    {
        		DEBUG(DBG_WDS | DBG_ERROR,"del_wds_link: MAC: %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x could not be unregistered!\n",
        			mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        		return err;
    	    }
            
    	    kfree(wds_dev->priv);
    	    kfree(wds_dev);
    	    wdsp->current_nof_wds_links--;
    	    wdsp->wds_dev[i] = NULL;
    
    	    DEBUG(DBG_WDS, "del_wds_link: WDSLink removed succesfully\n");
    	    return err;
    	}
    }
    DEBUG(DBG_WDS | DBG_ERROR,"del_wds_link: MAC: %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x is not a WDS link\n",
	      mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return -1;
}

/**********************************************************************
 *  open_wds_links
 *
 *  DESCRIPTION: Brings all associated WDS devices up and running.
 *
 *  PARAMETERS:	 dev points to device WDS links are put under.
 *
 *  RETURN:	 Nothing.
 *
 **********************************************************************/
int open_wds_links(struct wds_priv *wdsp)
{
    int i;
    
    DEBUG(DBG_FUNC_CALL, "open_wds_links \n");
    
    if ( wdsp == NULL ) {
        printk ("open_wds_links: no resources");
        return -1;
    }
    
    /* Bring the associated WDS devices up and RUNNING. */
    DEBUG(DBG_WDS, "Open all associated WDS links: ");
    
    for (i = 0; i < MAX_WDS_LINKS; i++) 
    {   
        if (wdsp->wds_dev[i] == NULL)
            continue;
        DEBUG(DBG_WDS, "WDS%d...",i);
        dev_open(wdsp->wds_dev[i]);
    }
    DEBUG(DBG_WDS, "\n");              
    return 0;
}

/**********************************************************************
 *  close_wds_links
 *
 *  DESCRIPTION: Closes all associated WDS links for the device.
 *
 *  PARAMETERS:	 dev points to device WDS links are put under.
 *
 *  RETURN:	 Nothing.
 *
 **********************************************************************/
int close_wds_links(struct wds_priv *wdsp)
{   
    int i;

    DEBUG(DBG_FUNC_CALL, "close_wds_links \n");
    
    if ( wdsp == NULL ) {
        printk ("close_wds_links: no resources");
        return -1;
    }

    DEBUG(DBG_WDS, "Close associated WDS links: ");
    for (i = 0; i < MAX_WDS_LINKS; i++) 
    {
        if (wdsp->wds_dev[i] == NULL)
            continue;
        DEBUG(DBG_WDS, "WDS%d...",i);
        dev_close(wdsp->wds_dev[i]);
    }
    DEBUG(DBG_WDS, "\n");
    return 0;
}


/**********************************************************************
 *  check_skb_for_wds
 *
 *  DESCRIPTION: Checks the skb->cb field for the WDS Link magic.
 *               It copies the MAC address into WDS_MAC.
 *
 *  PARAMETERS:	 pointer to sk_buff skb.
 *               WDS_Mac, is updated with the found WDS Link MAC address. 
 *
 *  RETURN:	 Return 1 if a WDS link is found, 0 otherwise
 *
 *  NOTE:        inline fucntion.
 *
 **********************************************************************/
inline int check_skb_for_wds(struct sk_buff *skb, unsigned char *wds_mac)
{
    /* If this skb came from a WDS device, the
     * skb->cb field will be filled with a WDSLink mac_addr.
     * Due to the reason the cb field is free to use, the MAC address
     * will be packed in the following keys "WDSL"<MAC ADDRESS>".
     */

    if(*(long*)&skb->cb[0] == cpu_to_le32(0x4c534457)) /* WDSL */
    {
        memcpy(wds_mac, skb->cb+4, 6);       
#if (NET_DEBUG & DBG_WDS)
    	printk("net_send_packet: found wdsmac in skb: %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x \n",
    		wds_mac[0],wds_mac[1],wds_mac[2],wds_mac[3],wds_mac[4],wds_mac[5]);
#endif
        /* Make the WDS data invalid */
        *(long*)&skb->cb[0] = 0;

        return(1); //WDS Link is available */
    }                                        
    return(0); /* WDS Link is unavailable */ 
}


/**********************************************************************
 *  wds_find_device
 *
 *  DESCRIPTION: Find the WDS device to use for receiving this data
 *
 *  PARAMETERS:	 unsigned char        *wds_mac - Pointer to the WDS MAC address
 *  		 struct net_device    *dev    - The device the skb was
 *                                              received on.
 *
 *  RETURN:	 *net_dev of the device found, otherwise NULL.
 *
 **********************************************************************/

struct net_device *wds_find_device( unsigned char *wds_mac, struct wds_priv *wdsp )
{
    int i=0;

    _DEBUG(DBG_FUNC_CALL | DBG_WDS | DBG_IRQ, "wds_find_device\n");
    if ( wdsp == NULL ) {
    	DEBUG(DBG_WDS, "wds_find_device: wds_priv not found, returning NULL\n");
        return NULL;
    }

    if (wds_mac[0] == 0 && wds_mac[1] == 0 && 
    	wds_mac[2] == 0 && wds_mac[3] == 0 &&
    	wds_mac[4] == 0 && wds_mac[5] == 0)
    {
    	_DEBUG(DBG_WDS | DBG_IRQ, "wds_find_device: wds_mac no MAC found, returning NULL\n");
    	return NULL;
    }
    for (i=0; i < MAX_WDS_LINKS; i++)
    {
    	if (wdsp->wds_dev[i] == NULL)
    	    continue;
            if(memcmp(((struct wds_net_local*)(wdsp->wds_dev[i]->priv))->wds_mac, wds_mac, 6) == 0)
    	{
    	    _DEBUG(DBG_WDS | DBG_IRQ, "wds_find_device: wds_mac MAC found, returning dev->name: %s\n",
    	    	   wdsp->wds_dev[i]->name);
    	    return ((struct net_device*)wdsp->wds_dev[i]);
    	}
    }
    _DEBUG(DBG_WDS | DBG_IRQ, "wds_find_device: WDS from mac %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x not in current config, returning NULL\n",
    	   wds_mac[0],wds_mac[1],wds_mac[2],wds_mac[3],wds_mac[4],wds_mac[5]);

    return NULL;
}


/**********************************************************************
 *  wds_net_open
 *
 *  DESCRIPTION: Bring the WDS dev up and running.
 * 		 The WDS device needs to be added to the bridge,
 * 		 where the parent dev is also bridged.
 *
 *  PARAMETERS:	 struct net_device    *dev    - The WDS device
 *
 *  RETURN:	 zero on succes, otherwise error (<0).
 *
 **********************************************************************/
int wds_net_open(struct net_device *dev)
{
    struct wds_net_local *wnlp = (struct wds_net_local *)dev->priv;
    int err = 0;
    
    DEBUG(DBG_FUNC_CALL | DBG_WDS, "wds_net_open \n");
    
    netif_start_queue(dev);

    /* Check if the parent device is bridged */
    if (wnlp->parent_dev->br_port == NULL)
    {
	DEBUG(DBG_WDS | DBG_ERROR, "wds_net_open: Error parent device (%s) is not bridged, can't add WDS link.\n",lp->parent_dev->name);
        return -1;
    }

    return err;
}

/**********************************************************************
 *  wds_net_send_packet
 *
 *  DESCRIPTION: Sends a WDS packet via the WDS' parent device.
 *               This function puts the WDS mac_addr in the skb->cb
 *               private field, alters the device to send from into the
 *               parent device for this WDS link and puts the skb into
 *               the transmit device queue.
 *
 *  PARAMETERS:	 struct sk_buff    *skb -  skbuff to send.
 *               struct net_device *dev -  wds device. 
 *
 *  RETURN:	 zero.
 *
 **********************************************************************/
int wds_net_send_packet(struct sk_buff *skb, struct net_device *dev)
{
    struct wds_net_local *wnl = (struct wds_net_local *)dev->priv;

    _DEBUG(DBG_FUNC_CALL | DBG_WDS, "wds_net_send_packet\n");
    
    if (wnl == NULL)
    {
    	printk("wds_net_send_packet: NULL pointer detected\n");
    	return 0;
    }
    if ((wnl->parent_dev->flags & IFF_UP) && (dev->flags & IFF_UP))
    {
        /* OK lets copy the WDSlink MACAddres in the skb->cb fields */
        memcpy(skb->cb, "WDSL", 4);
        memcpy(skb->cb+4, ((struct wds_net_local*)(dev->priv))->wds_mac, dev->addr_len);

        /* Set the "transmit" dev in this skb */
        skb->dev = wnl->parent_dev;

        /* Move this skb to the "transmit" devices queue */
        if(dev_queue_xmit(skb) < 0)
        {
            printk("wds_net_send_packet: dev_queue_xmit failed\n");
        }
        /* Update stats for this device */
        wnl->stats.tx_packets++;
        wnl->stats.tx_bytes += skb->len;
    }
    return 0;
}

/**********************************************************************
 *  wds_net_close
 *
 *  DESCRIPTION: Deletes the WDS device from the bridge.
 *
 *  PARAMETERS:	 struct net_device *dev - WDS device to remove.
 *
 *  RETURN:	 0 on success and -1 when parent device isn't bridged.
 *
 **********************************************************************/
int wds_net_close(struct net_device *dev)
{
    int err = 0;
    struct wds_net_local *lp = (struct wds_net_local *)dev->priv;

    DEBUG(DBG_FUNC_CALL | DBG_WDS, "wds_net_close \n");
    
    netif_stop_queue(dev);

    /* Check if the parent device is bridged */
    if (lp->parent_dev->br_port == NULL)
    {
	DEBUG(DBG_WDS | DBG_ERROR, "wds_net_close: Error parent device (%s) is not bridged, can't close WDS link.\n",lp->parent_dev->name);
        return -1;
    }

    return err;
}


/**********************************************************************
 *  wds_net_get_stats
 *
 *  DESCRIPTION: Gets the statistics for the WDS device. 
 *
 *  PARAMETERS:	 struct net_device *dev - WDS device to get statistics from.
 *
 *  RETURN:	 pointer to net_device_stats struct.
 *
 **********************************************************************/
struct net_device_stats* wds_net_get_stats(struct net_device *dev)
{
    struct wds_net_local *lp = (struct wds_net_local *)dev->priv;
    struct net_device_stats *stats = &lp->stats;

    _DEBUG(DBG_FUNC_CALL | DBG_WDS, "wds_net_get_stats \n");

    return stats;
}

/**********************************************************************
 *  wds_net_ioctl
 *
 *  DESCRIPTION: Ioctl for WDS devices are NOT supported. Tell them.
 *
 *  PARAMETERS:	 struct net_device *dev -
 *               struct ifreq      *ifr -
 *               int    cmd             -
 *
 *  RETURN:	 -EINVAL. 
 *
 **********************************************************************/
int wds_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    printk("wds_net_ioctl: Ioctls for WDS devices are NOT supported in this driver.\n");
    return -EINVAL;
}


/***************************************************************
*								*
*    			End of WDS functions			*
*								*
***************************************************************/

