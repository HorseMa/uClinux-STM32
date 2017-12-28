/*************************************************************************
*
* Author: Cavium Networks info@caviumnetworks.com
*
* Copyright (c) 2003-2007  Cavium Networks (support@cavium.com). All rights
* reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*
*     * Neither the name of Cavium Networks nor the names of
*       its contributors may be used to endorse or promote products
*       derived from this software without specific prior written
*       permission.
*
* This Software, including technical data, may be subject to U.S.  export
* control laws, including the U.S.  Export Administration Act and its
* associated regulations, and may be subject to export or import regulations
* in other countries.  You warrant that You will comply strictly in all
* respects with all such regulations and acknowledge that you have the
* responsibility to obtain licenses to export, re-export or import the
* Software.
*
* TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
* AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
* OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
* RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
* REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
* DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
* OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
* PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
* POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
* OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
*************************************************************************/

/**
 * @file
 * External interface for the Cavium Octeon ethernet driver.
 *
 * $Id: cavium-ethernet.h,v 1.1 2008-10-28 22:32:37 gerg Exp $
 *
 */
#ifndef CAVIUM_ETHERNET_H
#define CAVIUM_ETHERNET_H

/**
 * These enumerations are the return codes for the Ethernet
 * driver intercept callback. Depending on the return code,
 * the ethernet driver will continue processing in different
 * ways.
 */
typedef enum
{
    CVM_OCT_PASS,               /**< The ethernet driver will pass the packet
                                    to the kernel, just as if the intercept
                                    callback didn't exist */
    CVM_OCT_DROP,               /**< The ethernet driver will drop the packet,
                                    cleaning of the work queue entry and the
                                    skbuff */
    CVM_OCT_TAKE_OWNERSHIP_WORK,/**< The intercept callback takes over
                                    ownership of the work queue entry. It is
                                    the responsibility of the callback to free
                                    the work queue entry and all associated
                                    packet buffers. The ethernet driver will
                                    dispose of the skbuff without affecting the
                                    work queue entry */
    CVM_OCT_TAKE_OWNERSHIP_SKB  /**< The intercept callback takes over
                                    ownership of the skbuff. The work queue
                                    entry and packet buffer will be disposed of
                                    in a way keeping the skbuff valid */
} cvm_oct_callback_result_t;


/**
 * The is the definition of the Ethernet driver intercept
 * callback. The callback receives three parameters and
 * returns a cvm_oct_callback_result_t code.
 *
 * The first parameter is the linux device for the ethernet
 * port the packet came in on.
 * The second parameter is the raw work queue entry from the
 * hardware.
 * Th third parameter is the packet converted into a Linux
 * skbuff.
 */
typedef cvm_oct_callback_result_t (*cvm_oct_callback_t)(struct net_device *dev, void *work_queue_entry, struct sk_buff *skb);

/**
 * This is the definition of the Ethernet driver's private
 * driver state stored in netdev_priv(dev).
 */
typedef struct
{
    int                     port;           /* PKO hardware output port */
    int                     queue;          /* PKO hardware queue for the port */
    int                     fau;            /* Hardware fetch and add to count outstanding tx buffers */
    int                     imode;          /* Type of port. This is one of the enums in cvmx_helper_interface_mode_t */
    struct sk_buff_head     tx_free_list[16];/* List of outstanding tx buffers per queue */
    struct net_device_stats stats;          /* Device statistics */
    struct mii_if_info      mii_info;       /* Generic MII info structure */
    cvm_oct_callback_t      intercept_cb;   /* Optional intecept callback defined above */
    uint64_t                link_info;      /* Last negotiated link state */
    void (*poll)(struct net_device *dev);   /* Called periodically to check link status */
} cvm_oct_private_t;


/**
 * Registers a intercept callback for the names ethernet
 * device. It returns the Linux device structure for the
 * ethernet port. Usign a callback of NULL will remove
 * the callback. Note that this callback must not disturb
 * scratch. It will be called with SYNCIOBDMAs in progress
 * and userspace may be using scratch. It also must not
 * disturb the group mask.
 *
 * @param device_name
 *                 Device name to register for. (Example: "eth0")
 * @param callback Intercept callback to set.
 * @return Device structure for the ethernet port or NULL on failure.
 */
struct net_device *cvm_oct_register_callback(const char *device_name, cvm_oct_callback_t callback);


/**
 * Free a work queue entry received in a intercept callback.
 *
 * @param work_queue_entry
 *               Work queue entry to free
 * @return Zero on success, Negative on failure.
 */
int cvm_oct_free_work(void *work_queue_entry);


/**
 * Transmit a work queue entry out of the ethernet port. Both
 * the work queue entry and the packet data can optionally be
 * freed. The work will be freed on error as well.
 *
 * @param dev     Device to transmit out.
 * @param work_queue_entry
 *                Work queue entry to send
 * @param do_free True if the work queue entry and packet data should be
 *                freed. If false, neither will be freed.
 * @param qos     Index into the queues for this port to transmit on. This
 *                is used to implement QoS if their are multiple queues per
 *                port. This parameter must be between 0 and the number of
 *                queues per port minus 1. Values outside of this range will
 *                be change to zero.
 *
 * @return Zero on success, negative on failure.
 */
int cvm_oct_transmit_qos(struct net_device *dev, void *work_queue_entry, int do_free, int qos);


/**
 * Transmit a work queue entry out of the ethernet port. Both
 * the work queue entry and the packet data can optionally be
 * freed. The work will be freed on error as well. This simply
 * wraps cvmx_oct_transmit_qos() for backwards compatability.
 *
 * @param dev     Device to transmit out.
 * @param work_queue_entry
 *                Work queue entry to send
 * @param do_free True if the work queue entry and packet data should be
 *                freed. If false, neither will be freed.
 *
 * @return Zero on success, negative on failure.
 */
static inline int cvm_oct_transmit(struct net_device *dev, void *work_queue_entry, int do_free)
{
    return cvm_oct_transmit_qos(dev, work_queue_entry, do_free, 0);
}

#endif
