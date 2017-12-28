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

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/isil_netlink.h>
#include <net/sock.h>

#include "isl_mgt.h"

#define IND_TABLE_SZ    15
#define RESP_TABLE_SZ   15

#ifndef NULL
#   define  NULL ((void *)0)
#endif

// #define DRIVER_DEBUG 1

unsigned int src_seq = 0;

static struct ind_rec {
    unsigned int   dev_type;
    char           name[IFNAMSIZ];
    unsigned int   dev_seq;
    unsigned int   oid;
    mgt_indication_t ind;
} ind_table[IND_TABLE_SZ];

static struct resp_rec {
    unsigned int   src_id;
    char           name[IFNAMSIZ];
    unsigned int   src_seq;
    unsigned int   oid;
    mgt_response_t resp;
} resp_table[RESP_TABLE_SZ];

static mgt_confirm_t cnf_hndl = NULL;

static int mgt_initialised = 0;
static struct sock *nl_sock = NULL; /* netlink socket */

static int ntl_confirm( unsigned int src_id, unsigned int src_seq,
                        unsigned int dev_type, unsigned int dev_seq,
                        unsigned int op, unsigned int oid, void *data, unsigned long len);
static int ntl_transmit(pid_t pid, int seq, int operation,
                        int oid, long *data, unsigned long data_len,
                        int dev_type, int dev_id );
static void ntl_receive(struct sock *sk, int len);

static int pimfor_encode_header(int operation, unsigned long oid, int device_id,
                                int flags, int length, struct pimfor_hdr *header);
static int pimfor_decode_header(struct pimfor_hdr *header, int *version,
                                int *operation, unsigned long *oid,
                                int *device_id, int *flags, unsigned long *length);


/********************************************************************
*                      Management interfaces
*********************************************************************/

int mgt_ind_table_init = 0;

int mgt_request( unsigned int dev_type, unsigned int dev_seq,
                 unsigned int src_id,   unsigned int src_seq,
                 unsigned int op, int oid, void *data, unsigned long len)
{
    int i;
    int err = -EPFNOSUPPORT;
    struct net_device *dev;
    struct ind_rec *pi;
    mgt_indication_t ind_oid = NULL;
    mgt_indication_t ind_def = NULL;

#ifdef DRIVER_DEBUG
    printk (KERN_ERR "mgt_request(%d, %d, %d, %d, %d, %x, <data>, %d)\n",
            dev_type, dev_seq, src_id, src_seq, op, oid, (unsigned int)len );
#endif

    if ( mgt_ind_table_init != -1 ) {

        if ( ! (dev = dev_get_by_index( dev_seq ) ) )
            return -1;

        for ( i = 0, pi = ind_table; ( pi->ind != NULL ) && ( i < IND_TABLE_SZ ); i++, pi++ ) {
           if ( ( pi->dev_seq == 0 ) && ( strncmp ( pi->name, dev->name, IFNAMSIZ ) == 0 ) ) {
               mgt_ind_table_init++;
               pi->dev_seq = dev_seq;
           }
        }

        if ( i == mgt_ind_table_init ) {
            mgt_ind_table_init = -1;
        }

        dev_put( dev );
    }

    for ( i = 0, pi = ind_table; i < IND_TABLE_SZ; i++, pi++ ) {
        if ( ( pi->dev_type == dev_type ) && ( pi->dev_seq == dev_seq ) && ( pi->ind != NULL ) ) {
            if ( pi->oid == oid ) {
                ind_oid = pi->ind;
                break;
            } else if ( pi->oid == 0 ) {
                ind_def = pi->ind;
            }
        }
    }

    /* only one request allowed (confirm); specific handlers have priority over default handler */
    if ( ind_oid ) {
        err = (*ind_oid)( dev_type, dev_seq, src_id, src_seq, op, oid, data, len );
    } else if ( ind_def ) {
        err = (*ind_def)( dev_type, dev_seq, src_id, src_seq, op, oid, data, len );
    }

    return err;
}

int mgt_resp_table_init = 0;

int mgt_response( unsigned int src_id, unsigned int src_seq,
                  unsigned int dev_type, unsigned int dev_seq,
                  unsigned int op, int oid, void *data, unsigned long len)
{
    int i;
    int err = 0;
    struct net_device *dev;
    struct resp_rec *pr;
    mgt_response_t resp_oid = NULL;
    mgt_response_t resp_def = NULL;

#ifdef DRIVER_DEBUG
    printk (KERN_ERR "mgt_response(%d, %d, %d, %d, %d, %x, <data>, %d)\n",
            src_id, src_seq, dev_type, dev_seq, op, oid, (unsigned int)len );
#endif

    if ( mgt_resp_table_init != -1 ) {

        if ( ! (dev = dev_get_by_index( dev_seq ) ) )
            return -1;

        for ( i = 0, pr = resp_table; ( pr->resp != NULL ) && ( i < RESP_TABLE_SZ ); i++, pr++ ) {
           if ( ( pr->src_seq == 0 ) && ( strncmp ( pr->name, dev->name, IFNAMSIZ ) == 0 ) ) {
               mgt_resp_table_init++;
               pr->src_seq = dev_seq;
           }
        }

        if ( i == mgt_resp_table_init ) {
            mgt_resp_table_init = -1;
        }

        dev_put( dev );
    }

    for ( i = 0, pr = resp_table; i < RESP_TABLE_SZ; i++, pr++ ) {
        if ( ( pr->src_id == src_id ) && ( pr->src_seq == src_seq ) && ( pr->resp != NULL ) ) {
            if ( pr->oid == oid ) {
                resp_oid = pr->resp;
                break;
            } else if ( pr->oid == 0 ) {
                resp_def = pr->resp;
            }
        }
    }

    /* only one response allowed (indication); specific handlers have priority over default handler */
    if ( resp_oid ) {
        if ( (*resp_oid)( src_id, src_seq, dev_type, dev_seq, op, oid, data, len ) < 0)
            err = -EPFNOSUPPORT;
    } else if ( cnf_hndl ) {
        if ( ( *cnf_hndl )( src_id, src_seq, dev_type, dev_seq, op, oid, data, len ) < 0 )
            err = -EPFNOSUPPORT;
    }

    return err;
}

int mgt_confirm( unsigned int src_id, unsigned int src_seq,
                  unsigned int dev_type, unsigned int dev_seq,
                  unsigned int op, int oid, void *data, unsigned long len)
{
    return ntl_confirm( src_id, src_seq, dev_type, dev_seq, op, oid, data, len);
}

int mgt_indication_handler( unsigned int dev_type, char *dev_name,
                            unsigned int oid, mgt_indication_t ind )
{
    int i;
    struct ind_rec *pi;

    if ( ind ) {
        for ( i = 0, pi = ind_table; i < IND_TABLE_SZ; i++, pi++ ) {
           if ( pi->ind == 0 ) {
              pi->dev_type = dev_type;
              strncpy( pi->name, dev_name, IFNAMSIZ );
              pi->dev_seq  = 0;
              pi->oid      = oid;
              pi->ind      = ind;
              return 0;
           }
        }
    }

    return -EPFNOSUPPORT;
}

int mgt_response_handler( unsigned int src_id, char *dev_name,
                            unsigned int oid, mgt_response_t resp )
{
    int i;
    struct resp_rec *pr;

    if ( resp ) {
        for ( i = 0, pr = resp_table; i < RESP_TABLE_SZ; i++, pr++ ) {
           if ( pr->resp == 0 ) {
              pr->src_id   = src_id;
              strncpy( pr->name, dev_name, IFNAMSIZ );
              pr->src_seq  = 0;
              pr->oid      = oid;
              pr->resp     = resp;
              return 0;
           }
        }
    }

    return -EPFNOSUPPORT;
}

int mgt_confirm_handler( mgt_confirm_t cnf )
{
    if ( cnf ) {
        cnf_hndl = cnf;
        return 0;
    }

    return -EPFNOSUPPORT;
}

int mgt_ind_table_dump_hndl ( unsigned int dev_id,   unsigned int dev_seq,
                              unsigned int src_id,   unsigned int src_seq,
                              unsigned int op,       unsigned int oid, void *data, unsigned long len )
{
    int i;
    struct ind_rec *pi;

    printk (KERN_ERR "mgt_ind_table_dump_hndl(%d, %d, %d, %d, %d, %x, <data>, %d)\n",
            dev_id, dev_seq, src_id, src_seq, op, oid, (unsigned int)len );

    for ( i = 0, pi = ind_table; ( pi->ind != NULL ) && ( i < IND_TABLE_SZ ); i++, pi++ ) {
        printk ( KERN_ERR "%d - %s - %d - %x - %p\n",
                 pi->dev_type, pi->name, pi->dev_seq, pi->oid, pi->ind );
    }
}


int mgt_initialize( void )
{
    int i;
    struct ind_rec  *ip;
    struct resp_rec *pr;

#ifdef DRIVER_DEBUG
    printk ( KERN_ERR "mgt_initialize(%d)\n", mgt_initialised );
#endif

    if( mgt_initialised )
        return 0;

    mgt_initialised = 1;

    for ( i = 0, ip = ind_table; i < IND_TABLE_SZ; i++, ip++ ) {
        ip->dev_type = 0;
        ip->dev_seq  = 0;
        ip->oid      = 0;
        ip->ind      = NULL;
    }

    for ( i = 0, pr = resp_table; i < RESP_TABLE_SZ; i++, pr++ ) {
        pr->src_id   = 0;
        pr->src_seq  = 0;
        pr->oid      = 0;
        pr->resp     = NULL;
    }

    cnf_hndl = NULL;

    /* Open the netlink socket */
    if (nl_sock == NULL) {
        if ( (nl_sock = netlink_kernel_create(NETLINK_ISIL, ntl_receive)) == NULL )
            printk("netlink_kernel_create failed\n");
    }

    mgt_confirm_handler( ntl_confirm );

    return 0;
}

int mgt_cleanup( void )
{
    if ( nl_sock ) {
		sock_release( nl_sock->socket );
    }

    return 0;
}


/*********************************************************************
*                        Netlink interface
*********************************************************************/

static int ntl_confirm( unsigned int src_id, unsigned int src_seq,
                        unsigned int dev_type, unsigned int dev_seq,
                        unsigned int op, unsigned int oid, void *data, unsigned long len)
{
    return ntl_transmit(src_id, src_seq, op, oid, (long*)data, len, dev_type, dev_seq );
}

static int ntl_transmit(pid_t pid, int seq, int operation, int oid, long *data, unsigned long data_len,
                        int dev_type, int dev_id )
{
    unsigned char *old_tail;
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    struct pimfor_hdr *pimfor_hdr;
    size_t size;

    int err = 0;
    /* Trap group is mapped to dev_type. */
    unsigned int trapgrp = dev_type;

#ifdef DRIVER_DEBUG
    printk(KERN_INFO "ntl_transmit: pid %d, seq %d, oper %d, oid %d, data ptr %p, data_len %lu, dev_id %d \n",
                       pid, seq, operation, oid, data, data_len, dev_id);
#endif

    if ( nl_sock == NULL ) {
        printk(KERN_ERR "ntl_transmit: no nl_sock\n");
        return -EPFNOSUPPORT;
    }

    size = NLMSG_LENGTH(data_len + sizeof(struct pimfor_hdr));
    skb = alloc_skb(size, GFP_ATOMIC);

    if ( skb ) {

        old_tail = skb->tail;
        nlh = NLMSG_PUT(skb, pid, seq, NETLINK_TYPE_PIMFOR, size - NLMSG_ALIGN(sizeof(struct nlmsghdr)));
        pimfor_hdr = (struct pimfor_hdr *)NLMSG_DATA(nlh);

        pimfor_encode_header(operation, oid, dev_id, 0 /*flags*/, data_len, pimfor_hdr);
        /* Add data after the PIMFOR header */
        memcpy(PIMFOR_DATA(pimfor_hdr), data, data_len);

        nlh->nlmsg_len = skb->tail - old_tail;
        NETLINK_CB(skb).dst_groups = 0;
    } else {

        printk(KERN_ERR "ntl_transmit: alloc_skb failed\n");
        return -EPFNOSUPPORT;
    }

    if (pid != 0 ) {
        err = netlink_unicast(nl_sock, skb, pid, MSG_DONTWAIT);
    } else {
        if (trapgrp) {
            netlink_broadcast(nl_sock, skb, pid, trapgrp ,GFP_ATOMIC );
        }
        else {
            printk(KERN_WARNING "No trap group defined, drop packet.\n");
            kfree_skb(skb);
        }
    }

    return err;

nlmsg_failure:
    if ( skb )
            kfree_skb(skb);
    printk(KERN_ERR "ntl_transmit: NLMSG_PUT failed\n");
    return -EPFNOSUPPORT;
}

static void ntl_receive(struct sock *sk, int len)
{
    int err;
    struct sk_buff *skb;
    pid_t pid;
    struct nlmsghdr *nl_header;
    __u32 seq;
    struct pimfor_hdr *header;
    char *data;
    int version;
    int operation;
    unsigned long oid;
    int dev_id;
    int flags;
    unsigned long length;
  	struct net_device *dev;

#ifdef DRIVER_DEBUG
    printk(KERN_INFO "ntl_receive: sock %p, len %d \n", sk, len);
#endif

    do
    {
        if (rtnl_shlock_nowait())
                return;

        while ( (skb = skb_dequeue(&sk->receive_queue)) != NULL )
        {
            pid = NETLINK_CB(skb).pid;
            nl_header = (struct nlmsghdr*) skb->data;
            seq = nl_header->nlmsg_seq;
            header = (struct pimfor_hdr*)(skb->data+(sizeof(struct nlmsghdr)));
            data = PIMFOR_DATA(header);

            if ( nl_header->nlmsg_type == NETLINK_TYPE_PIMFOR )
            {
                pimfor_decode_header(header, &version, &operation, &oid, &dev_id, &flags, &length);

                if (version == PIMFOR_VERSION_1) {

                    err = mgt_request( DEV_NETWORK, dev_id, pid, seq, operation, oid, data, length );

                    if ( err < 0 ) {
                        printk(KERN_INFO "ntl_receive: mgt_request(%d, %d, %d, %x, %d, %x, %d) returned %d\n",
                               DEV_NETWORK, dev_id, pid, seq, operation, oid, length, err );
                        netlink_ack(skb, nl_header, -EOPNOTSUPP );
                    }

                } else {
                    printk(KERN_ERR "ntl_receive: version (%d) != PIMFOR_VERSION_1\n", version );
                    netlink_ack(skb, nl_header, -EOPNOTSUPP );
                }
            } else {
                printk(KERN_ERR "nl_header->nlmsg_type (%d) != NETLINK_TYPE_PIMFOR\n", nl_header->nlmsg_type );
                netlink_ack(skb, nl_header, -EOPNOTSUPP );
            }

            kfree_skb(skb);
        }
        up(&rtnl_sem);

    } while (nl_sock && nl_sock->receive_queue.qlen);
}


/*********************************************************************
*                        PIMFOR
*********************************************************************/

/*
 * PIMFOR (Proprietary Intersil Mechanism For Object Relay). PIMFOR is a simple
 * request-response protocol used to query and set items of management information
 * residing in the MVC. It is used to provide an object interface to device firmware.
 *
 * PIMFOR is also used for communication between applications and the driver.
 * For the uAP, PIMFOR is used for communication between the driver and the MVC.
 *
 */


/**********************************************************************
 *  pimfor_encode_header
 *
 *  DESCRIPTION: XXX
 *
 *  PARAMETERS:	 XXX
 *
 *  RETURN:	 XXX
 *
 **********************************************************************/
static int pimfor_encode_header(int operation, unsigned long oid, int device_id,
                                int flags, int length, struct pimfor_hdr *header)
{
    //printk(KERN_INFO "pimfor_encode_header: oper %d, oid %d, dev_id %d, flags %d, length %d header ptr %p \n",
    //                  operation, oid, device_id, flags, length, header);

    if( !header )
            return -EPFNOSUPPORT;

    memset(header, 0, sizeof(struct pimfor_hdr));

    /* byte oriented members */
    header->version = PIMFOR_VERSION_1;
    header->operation = operation;
    header->device_id = device_id;
    header->flags = flags;

    /* word oriented members with byte order depending on the flags */
    if (flags & PIMFOR_FLAG_LITTLE_ENDIAN) {
            header->oid = cpu_to_le32(oid);
            header->length = cpu_to_le32(length);
    } else {
            header->oid = cpu_to_be32(oid);
            header->length = cpu_to_be32(length);
    }
    return 0;
}

/**********************************************************************
 *  pimfor_decode_header
 *
 *  DESCRIPTION: XXX
 *
 *  PARAMETERS:	 XXX
 *
 *  RETURN:	 XXX
 *
 **********************************************************************/
static int pimfor_decode_header(struct pimfor_hdr *header, int *version,
                                int *operation, unsigned long *oid,
                                int *device_id, int *flags, unsigned long *length)
{
    //printk(KERN_INFO "pimfor_decode_header: header ptr %p, version %d, oper %d, oid %d, device_id %d, flags %d, length %d \n",
    //                      header, version, operation, oid, device_id, flags, length);

    if(!header)
            return -EPFNOSUPPORT;

    /* byte oriented members */
    *version   = header->version;
    *operation = header->operation;
    *device_id = header->device_id;
    *flags = header->flags;

    /* word oriented members with byte order depending on the flags */
    if (*flags & PIMFOR_FLAG_LITTLE_ENDIAN) {
            *oid = le32_to_cpu(header->oid);
            *length = le32_to_cpu(header->length);
    } else {
            *oid = be32_to_cpu(header->oid);
            *length = be32_to_cpu(header->length);
    }

    return 0;
}


EXPORT_SYMBOL(mgt_initialize);
EXPORT_SYMBOL(mgt_response);
EXPORT_SYMBOL(mgt_indication_handler);
