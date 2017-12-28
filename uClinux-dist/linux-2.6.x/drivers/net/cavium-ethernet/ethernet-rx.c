/*************************************************************************
* Cavium Octeon Ethernet Driver
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/string.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <net/dst.h>
#ifdef CONFIG_XFRM
#include <linux/xfrm.h>
#include <net/xfrm.h>
#endif  /* CONFIG_XFRM */

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

extern int pow_receive_group;
extern struct net_device *cvm_oct_device[];
static struct tasklet_struct cvm_oct_tasklet[NR_CPUS];

/**
 * Interrupt handler. The interrupt occurs whenever the POW
 * transitions from 0->1 packets in our group.
 *
 * @param cpl
 * @param dev_id
 * @param regs
 * @return
 */
irqreturn_t cvm_oct_do_interrupt(int cpl, void *dev_id)
{
    /* Acknowledge the interrupt */
    if (INTERRUPT_LIMIT)
        cvmx_write_csr(CVMX_POW_WQ_INT, 1<<pow_receive_group);
    else
        cvmx_write_csr(CVMX_POW_WQ_INT, 0x10001<<pow_receive_group);
    preempt_disable();
    tasklet_schedule(cvm_oct_tasklet + smp_processor_id());
    preempt_enable();
    return IRQ_HANDLED;
}


#ifdef CONFIG_NET_POLL_CONTROLLER
/**
 * This is called when the kernel needs to manually poll the
 * device. For Octeon, this is simply calling the interrupt
 * handler. We actually poll all the devices, not just the
 * one supplied.
 *
 * @param dev    Device to poll. Unused
 */
void cvm_oct_poll_controller(struct net_device *dev)
{
    preempt_disable();
    tasklet_schedule(cvm_oct_tasklet + smp_processor_id());
    preempt_enable();
}
#endif


/**
 * Tasklet function that is scheduled on a core when an interrupt occurs.
 *
 * @param unused
 */
void cvm_oct_tasklet_rx(unsigned long unused)
{
    const int           coreid = cvmx_get_core_num();
    uint64_t            old_group_mask;
    uint64_t            old_scratch;
    int                 rx_count = 0;
    int                 number_to_free;
    int                 packet_not_copied;

    /* Prefetch cvm_oct_device since we know we need it soon */
    CVMX_PREFETCH(cvm_oct_device, 0);

    if (USE_ASYNC_IOBDMA)
    {
        /* Save scratch in case userspace is using it */
        CVMX_SYNCIOBDMA;
        old_scratch = cvmx_scratch_read64(CVMX_SCR_SCRATCH);
    }

    /* Only allow work for our group (and preserve priorities) */
    old_group_mask = cvmx_read_csr(CVMX_POW_PP_GRP_MSKX(coreid));
    cvmx_write_csr(CVMX_POW_PP_GRP_MSKX(coreid),
        (old_group_mask & ~0xFFFFull) | 1<<pow_receive_group);

    if (USE_ASYNC_IOBDMA)
        cvmx_pow_work_request_async(CVMX_SCR_SCRATCH, CVMX_POW_NO_WAIT);

    while (1)
    {
        void *start_of_buffer;
        struct sk_buff *skb;
        cvm_oct_callback_result_t callback_result;
        cvmx_wqe_t *work;
        if (USE_ASYNC_IOBDMA)
        {
            work = cvmx_pow_work_response_async(CVMX_SCR_SCRATCH);
        }
        else
        {
            if ((INTERRUPT_LIMIT == 0) || likely(rx_count < 60))
                work = cvmx_pow_work_request_sync(CVMX_POW_NO_WAIT);
            else
                work = NULL;
        }
        if (work == NULL)
            break;

        /* Limit each core to processing 60 packets without a break. This way
            the RX can't starve the TX task. */
        if (USE_ASYNC_IOBDMA)
        {
            if ((INTERRUPT_LIMIT == 0) || likely(rx_count < 60))
                cvmx_pow_work_request_async_nocheck(CVMX_SCR_SCRATCH, CVMX_POW_NO_WAIT);
            else
            {
                cvmx_scratch_write64(CVMX_SCR_SCRATCH, 0x8000000000000000ull);
                cvmx_pow_tag_sw_null_nocheck();
            }
        }
        rx_count++;

        CVMX_PREFETCH(cvm_oct_device[work->ipprt], 0);
        start_of_buffer = cvm_oct_get_buffer_ptr(work->packet_ptr);
        CVMX_PREFETCH(start_of_buffer, -8); /* Should be -sizeof(void*), but you can't use that with this macro. */

        /* Immediately throw away all packets with receive errors */
        if (unlikely(work->word2.snoip.rcv_error))
        {
            if ((work->word2.snoip.err_code == 10) && (work->len <= 64))
            {
                /* Ignore length errors on min size packets. Some equipment
                    incorrectly pads packets to 64+4FCS instead of 60+4FCS.
                    Note these packets still get counted as frame errors. */
            }
            else if (USE_10MBPS_PREAMBLE_WORKAROUND && ((work->word2.snoip.err_code == 5) || (work->word2.snoip.err_code == 7)))
            {
                /* We received a packet with either an alignment error or a
                    FCS error. This may be signalling that we are running
                    10Mbps with GMXX_RXX_FRM_CTL[PRE_CHK} off. If this is the
                    case we need to parse the packet to determine if we can
                    remove a non spec preamble and generate a correct packet */
                int interface = cvmx_helper_get_interface_num(work->ipprt);
                int index = cvmx_helper_get_interface_index_num(work->ipprt);
                cvmx_gmxx_rxx_frm_ctl_t gmxx_rxx_frm_ctl;
                gmxx_rxx_frm_ctl.u64 = cvmx_read_csr(CVMX_GMXX_RXX_FRM_CTL(index, interface));
                if (gmxx_rxx_frm_ctl.s.pre_chk == 0)
                {
                    uint8_t *ptr = cvmx_phys_to_ptr(work->packet_ptr.s.addr);
                    int i = 0;
                    while (i<work->len-1)
                    {
                        if (*ptr != 0x55)
                            break;
                        ptr++;
                        i++;
                    }
                    if (*ptr == 0xd5)
                    {
                        //DEBUGPRINT("Port %d received 0xd5 preamble\n", work->ipprt);
                        work->packet_ptr.s.addr += i+1;
                        work->len -= i+5;
                    }
                    else if ((*ptr & 0xf) == 0xd)
                    {
                        //DEBUGPRINT("Port %d received 0x?d preamble\n", work->ipprt);
                        work->packet_ptr.s.addr += i;
                        work->len -= i+4;
                        for (i=0; i<work->len; i++)
                        {
                            *ptr = ((*ptr&0xf0)>>4) | ((*(ptr+1)&0xf)<<4);
                            ptr++;
                        }
                    }
                    else
                    {
                        DEBUGPRINT("Port %d unknown preamble, packet dropped\n", work->ipprt);
                        //cvmx_helper_dump_packet(work);
                        cvm_oct_free_work(work);
                        continue;
                    }
                }
            }
            else
            {
                DEBUGPRINT("Port %d receive error code %d, packet dropped\n", work->ipprt, work->word2.snoip.err_code);
                cvm_oct_free_work(work);
                continue;
            }
        }

        /* We can only use the zero copy path if skbuffs are in the FPA pool
            and the packet fit in a single buffer */
        if (USE_SKBUFFS_IN_HW && likely(work->word2.s.bufs == 1))
        {
            skb = *(struct sk_buff **)(start_of_buffer - sizeof(void*));
            /* This calculation was changed in case the skb header is using a
                different address aliasing type than the buffer. It doesn't make
                any differnece now, but the new one is more correct */
            skb->data = skb->head + work->packet_ptr.s.addr - cvmx_ptr_to_phys(skb->head);
            CVMX_PREFETCH(skb->data, 0);
            skb->len = work->len;
            skb->tail = skb->data + skb->len;
            packet_not_copied = 1;
        }
        else
        {
            /* We have to copy the packet. First allocate an skbuff for it */
            skb = dev_alloc_skb(work->len);
            if (!skb)
            {
                DEBUGPRINT("Port %d failed to allocate skbuff, packet dropped\n", work->ipprt);
                cvm_oct_free_work(work);
                continue;
            }

            /* Check if we've received a packet that was entirely stored the
                work entry. This is untested */
            if (unlikely(work->word2.s.bufs == 0))
            {
                uint8_t *ptr = work->packet_data;
                if (cvmx_likely(!work->word2.s.not_IP))
                {
                    /* The beginning of the packet moves for IP packets */
                    if (work->word2.s.is_v6)
                        ptr += 2;
                    else
                        ptr += 6;
                }
                memcpy(skb_put(skb, work->len), ptr, work->len);
                /* No packet buffers to free */
            }
            else
            {
                int segments = work->word2.s.bufs;
                cvmx_buf_ptr_t segment_ptr = work->packet_ptr;
                int len = work->len;
                while (segments--)
                {
                    cvmx_buf_ptr_t next_ptr = *(cvmx_buf_ptr_t*)cvmx_phys_to_ptr(segment_ptr.s.addr-8);
                    /* Octeon Errata PKI-100: The segment size is wrong. Until it
                        is fixed, calculate the segment size based on the packet
                        pool buffer size. When it is fixed, the following line should
                        be replaced with this one:
                            int segment_size = segment_ptr.s.size; */
                    int segment_size = CVMX_FPA_PACKET_POOL_SIZE - (segment_ptr.s.addr - (((segment_ptr.s.addr >> 7) - segment_ptr.s.back) << 7));
                    /* Don't copy more than what is left in the packet */
                    if (segment_size > len)
                        segment_size = len;
                    /* Copy the data into the packet */
                    memcpy(skb_put(skb, segment_size), cvmx_phys_to_ptr(segment_ptr.s.addr), segment_size);
                    /* Reduce the amount of bytes left to copy */
                    len -= segment_size;
                    segment_ptr = next_ptr;
                }
            }
            packet_not_copied = 0;
        }

        if (likely((work->ipprt < TOTAL_NUMBER_OF_PORTS) && cvm_oct_device[work->ipprt]))
        {
            struct net_device *dev = cvm_oct_device[work->ipprt];
            cvm_oct_private_t* priv = (cvm_oct_private_t*)netdev_priv(dev);

            /* Only accept packets for devices that are currently up */
            if (likely(dev->flags & IFF_UP))
            {
                skb->protocol = eth_type_trans(skb, dev);
                skb->dev = dev;

                if (unlikely(work->word2.s.not_IP || work->word2.s.IP_exc || work->word2.s.L4_error))
                    skb->ip_summed = CHECKSUM_NONE;
                else
                    skb->ip_summed = CHECKSUM_UNNECESSARY;
                /* Increment RX stats for virtual ports */
                if (work->ipprt >= CVMX_PIP_NUM_INPUT_PORTS)
                {
#ifdef CONFIG_64BIT
                    cvmx_atomic_add64_nosync(&priv->stats.rx_packets, 1);
                    cvmx_atomic_add64_nosync(&priv->stats.rx_bytes, skb->len);
#else
                    cvmx_atomic_add32_nosync((int32_t*)&priv->stats.rx_packets, 1);
                    cvmx_atomic_add32_nosync((int32_t*)&priv->stats.rx_bytes, skb->len);
#endif
                }

                if (priv->intercept_cb)
                {
                    callback_result = priv->intercept_cb(dev, work, skb);
                    switch (callback_result)
                    {
                        case CVM_OCT_PASS:
                            netif_receive_skb(skb);
                            break;
                        case CVM_OCT_DROP:
                            dev_kfree_skb_irq(skb);
#ifdef CONFIG_64BIT
                            cvmx_atomic_add64_nosync(&priv->stats.rx_dropped, 1);
#else
                            cvmx_atomic_add32_nosync((int32_t*)&priv->stats.rx_dropped, 1);
#endif
                            break;
                        case CVM_OCT_TAKE_OWNERSHIP_WORK:
                            /* Interceptor stole our work, but we need to free
                                the skbuff */
                            if (USE_SKBUFFS_IN_HW && likely(packet_not_copied))
                            {
                                /* We can't free the skbuff since its data is
                                    the same as the work. In this case we don't
                                    do anything */
                            }
                            else
                                dev_kfree_skb_irq(skb);
                            break;
                        case CVM_OCT_TAKE_OWNERSHIP_SKB:
                            /* Interceptor stole our packet */
                            break;
                    }
                }
                else
                {
                    netif_receive_skb(skb);
                    callback_result = CVM_OCT_PASS;
                }
            }
            else
            {
                /* Drop any packet received for a device that isn't up */
                //DEBUGPRINT("%s: Device not up, packet dropped\n", dev->name);
#ifdef CONFIG_64BIT
                cvmx_atomic_add64_nosync(&priv->stats.rx_dropped, 1);
#else
                cvmx_atomic_add32_nosync((int32_t*)&priv->stats.rx_dropped, 1);
#endif
                dev_kfree_skb_irq(skb);
                callback_result = CVM_OCT_DROP;
            }
        }
        else
        {
            /* Drop any packet received for a device that doesn't exist */
            DEBUGPRINT("Port %d not controlled by Linux, packet dropped\n", work->ipprt);
            dev_kfree_skb_irq(skb);
            callback_result = CVM_OCT_DROP;
        }

        /* We only need to free the work if the interceptor didn't take over
            ownership of it */
        if (callback_result != CVM_OCT_TAKE_OWNERSHIP_WORK)
        {
            /* Check to see if the skbuff and work share the same packet buffer */
            if (USE_SKBUFFS_IN_HW && likely(packet_not_copied))
            {
                /* This buffer needs to be replaced, increment the number of buffers we need to free by one */
                cvmx_fau_atomic_add32(FAU_NUM_PACKET_BUFFERS_TO_FREE, 1);
                cvmx_fpa_free(work, CVMX_FPA_WQE_POOL, DONT_WRITEBACK(1));
            }
            else
                cvm_oct_free_work(work);
        }
    }

    /* Restore the original POW group mask */
    cvmx_write_csr(CVMX_POW_PP_GRP_MSKX(coreid), old_group_mask);
    if (USE_ASYNC_IOBDMA)
    {
        /* Restore the scratch area */
        cvmx_scratch_write64(CVMX_SCR_SCRATCH, old_scratch);
    }

    if (USE_SKBUFFS_IN_HW)
    {
        /* Refill the packet buffer pool */
        number_to_free = cvmx_fau_fetch_and_add32(FAU_NUM_PACKET_BUFFERS_TO_FREE, 0);
        if (number_to_free>0)
        {
            cvmx_fau_atomic_add32(FAU_NUM_PACKET_BUFFERS_TO_FREE, -number_to_free);
            cvm_oct_mem_fill_fpa(CVMX_FPA_PACKET_POOL, CVMX_FPA_PACKET_POOL_SIZE, number_to_free);
        }
    }
}



void cvm_oct_rx_initialize(void)
{
    int i;
    /* Initialize all of the tasklets */
    for (i=0; i<NR_CPUS; i++)
        tasklet_init(cvm_oct_tasklet + i, cvm_oct_tasklet_rx, 0);
}

void cvm_oct_rx_shutdown(void)
{
    int i;
    /* Shutdown all of the tasklets */
    for (i=0; i<NR_CPUS; i++)
        tasklet_kill(cvm_oct_tasklet + i);
}

