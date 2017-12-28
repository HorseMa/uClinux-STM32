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

#ifndef _ISL_MGT_H
#define _ISL_MGT_H

#include <linux/isil_netlink.h>

#define DEV_NETWORK      0                              /* seq of DEV_NETWORK: net_device ifindex */

/* Map dev_type on trapgroup. */
#define DEV_NETWORK_ETH          TRAPGRP_ETH_TRAPS      /* seq of DEV_NETWORK: net_device ifindex */
#define DEV_NETWORK_WLAN         TRAPGRP_WLAN_TRAPS     /* seq of DEV_NETWORK: net_device ifindex */
#define DEV_NETWORK_BLOB         TRAPGRP_BLOBDEV_TRAPS  /* seq of DEV_NETWORK: 0 */

#define USR_NETLINK      1      

typedef int (*mgt_indication_t)(unsigned int dev_type, unsigned int dev_seq, 
                                unsigned int src_id, unsigned int src_seq, 
                                unsigned int op, unsigned int oid, void *data, unsigned long len);
typedef int (*mgt_response_t)(  unsigned int src_id, unsigned int src_seq,
                                unsigned int dev_type, unsigned int dev_seq, 
                                unsigned int op, unsigned int oid, void *data, unsigned long len);
typedef int (*mgt_confirm_t)(   unsigned int src_id, unsigned int src_seq,
                                unsigned int dev_type, unsigned int dev_seq, 
                                unsigned int op, unsigned int oid, void *data, unsigned long len);

int mgt_request( unsigned int dev_type, unsigned int dev_seq,              
                 unsigned int src_id,   unsigned int src_seq,              
                 unsigned int op, int oid, void *data, unsigned long len);
int mgt_response( unsigned int src_id, unsigned int src_seq,
                  unsigned int dev_type, unsigned int dev_seq, 
                  unsigned int op, int oid, void *data, unsigned long len);
int mgt_confirm( unsigned int src_id, unsigned int src_seq,
                  unsigned int dev_type, unsigned int dev_seq, 
                  unsigned int op, int oid, void *data, unsigned long len);

int mgt_indication_handler( unsigned int dev_type, char *dev_name, 
                            unsigned int oid, mgt_indication_t ind );
int mgt_response_handler( unsigned int src_id, char *dev_name, 
                            unsigned int oid, mgt_response_t resp );
int mgt_confirm_handler( mgt_confirm_t cnf );

int mgt_initialize( void );
int mgt_dump_table( void );
int mgt_cleanup( void );


#endif /* _ISL_MGT_H */
