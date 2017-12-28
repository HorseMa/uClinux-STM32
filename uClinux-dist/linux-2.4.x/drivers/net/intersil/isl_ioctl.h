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

#ifndef _ISL_IOCTL_H
#define _ISL_IOCTL_H


#include <linux/wireless.h>

#include <islpci_mgt.h>
#include <bloboid.h>      // additional types and defs for 38xx firmware


#define SUPPORTED_WIRELESS_EXT                  15


int isl_ioctl_getrange( struct net_device *, struct iw_range * );
int isl_ioctl_setrange( struct net_device *, struct iw_range * );

#if WIRELESS_EXT > 11

int isl_ioctl_getstats( struct net_device *, struct iw_statistics *);
int isl_ioctl_setstats( struct net_device *, struct iw_statistics *);

#endif

int isl_ioctl_getauthenten( struct net_device *, struct iwreq * );
int isl_ioctl_setauthenten( struct net_device *, struct iwreq * );
int isl_ioctl_getprivfilter( struct net_device *, struct iwreq * );
int isl_ioctl_setprivfilter( struct net_device *, struct iwreq * );
int isl_ioctl_getprivinvoke( struct net_device *, struct iwreq * );
int isl_ioctl_setprivinvoke( struct net_device *, struct iwreq * );
int isl_ioctl_getdefkeyid( struct net_device *, struct iwreq * );
int isl_ioctl_setdefkeyid( struct net_device *, struct iwreq * );
int isl_ioctl_getdefkeyx( struct net_device *, struct iwreq * );
int isl_ioctl_setdefkeyx( struct net_device *, struct iwreq * );
int isl_ioctl_getprivstat( struct net_device *, struct iwreq * );
int isl_ioctl_getsuprates( struct net_device *, struct iwreq * );
int isl_ioctl_getfixedrate( struct net_device *, struct iwreq * );
int isl_ioctl_setfixedrate( struct net_device *, struct iwreq * );
int isl_ioctl_getbeaconper( struct net_device *, struct iwreq * );
int isl_ioctl_setbeaconper( struct net_device *, struct iwreq * );
int isl_ioctl_getrate( struct net_device *, struct iw_param * );
int isl_ioctl_wdslinkadd( struct net_device *, struct iwreq * );
int isl_ioctl_wdslinkdel( struct net_device *, struct iwreq * );
int isl_ioctl_eapauthen( struct net_device *, struct iwreq * );
int isl_ioctl_eapunauth( struct net_device *, struct iwreq * );
int isl_ioctl_getdot1xen( struct net_device *, struct iwreq * );
int isl_ioctl_setdot1xen( struct net_device *, struct iwreq * );
int isl_ioctl_getstakeyx( struct net_device *, struct iwreq * );
int isl_ioctl_setstakeyx( struct net_device *, struct iwreq * );
int isl_ioctl_getessid( struct net_device *, struct iw_point * );
int isl_ioctl_setessid( struct net_device *, struct iw_point * );
int isl_ioctl_getbssid( struct net_device *, char * );
int isl_ioctl_setbssid( struct net_device *, char * );
int isl_ioctl_getmode( struct net_device *, int * );
int isl_ioctl_setmode( struct net_device *, int * );
int isl_ioctl_getfreq( struct net_device *, struct iw_freq * );
int isl_ioctl_setfreq( struct net_device *, struct iw_freq * );
int isl_ioctl_checktraps( struct net_device *, struct iwreq * );

int isl_ioctl( struct net_device *, struct ifreq *, int );


#endif  // _ISL_IOCTL_H
