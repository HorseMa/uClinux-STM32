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

#ifndef _ISL_WDS_H
#define _ISL_WDS_H

#include <linux/netdevice.h>

#ifndef MAX_WDS_LINKS
#define MAX_WDS_LINKS         8
#endif

struct wds_priv
{
    struct net_device  *wds_dev[MAX_WDS_LINKS];
    unsigned int 	    current_nof_wds_links;
};

struct wds_net_local
{
    struct net_device_stats	 stats;		/* tx/rx statistics */
    struct net_device 	    *parent_dev;
    unsigned char 		     wds_mac[6];
};

/* WDS */
int add_wds_link(struct net_device *dev, struct wds_priv *wdsp, unsigned char *mac_addr, char *brifname );
int del_wds_link(struct net_device *dev, struct wds_priv *wdsp, unsigned char *mac_addr, char *brifname );
int open_wds_links(struct wds_priv *wdsp);
int close_wds_links(struct wds_priv *wdsp);
struct net_device *wds_find_device( unsigned char *wds_mac, struct wds_priv *wdsp );
inline int check_skb_for_wds(struct sk_buff *skb, unsigned char *mac);
int wds_net_open(struct net_device *dev);
int wds_net_send_packet(struct sk_buff *skb, struct net_device *dev);
int wds_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
struct net_device_stats *wds_net_get_stats(struct net_device *dev);
int wds_net_close(struct net_device *dev);

#endif /* _ISL_WDS_H */

