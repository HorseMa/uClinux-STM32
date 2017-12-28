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
#include <linux/kernel.h>
#include <linux/mii.h>
#include <net/dst.h>

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

extern int octeon_is_simulation(void);
extern cvmx_bootinfo_t *octeon_bootinfo;
extern int pow_send_group;
extern int always_use_pow;
extern char pow_send_list[];


/**
 * Get the low level ethernet statistics
 *
 * @param dev    Device to get the statistics from
 * @return Pointer to the statistics
 */
static struct net_device_stats *cvm_oct_common_get_stats(struct net_device *dev)
{
    cvmx_pip_port_status_t rx_status;
    cvmx_pko_port_status_t tx_status;
    cvm_oct_private_t* priv = (cvm_oct_private_t*)netdev_priv(dev);

    if (priv->port < CVMX_PIP_NUM_INPUT_PORTS)
    {
        if (octeon_is_simulation())
        {
            /* The simulator doesn't support statistics */
            memset(&rx_status, 0, sizeof(rx_status));
            memset(&tx_status, 0, sizeof(tx_status));
        }
        else
        {
            cvmx_pip_get_port_status(priv->port, 1, &rx_status);
            cvmx_pko_get_port_status(priv->port, 1, &tx_status);
        }

        priv->stats.rx_packets      += rx_status.inb_packets;
        priv->stats.tx_packets      += tx_status.packets;
        priv->stats.rx_bytes        += rx_status.inb_octets;
        priv->stats.tx_bytes        += tx_status.octets;
        priv->stats.multicast       += rx_status.multicast_packets;
        priv->stats.rx_crc_errors   += rx_status.inb_errors;
        priv->stats.rx_frame_errors += rx_status.fcs_align_err_packets;

        /* The drop counter must be incremented atomically since the RX
            tasklet also increments it */
#ifdef CONFIG_64BIT
        cvmx_atomic_add64_nosync(&priv->stats.rx_dropped, rx_status.dropped_packets);
#else
        cvmx_atomic_add32_nosync((int32_t*)&priv->stats.rx_dropped, rx_status.dropped_packets);
#endif
    }

    return &priv->stats;
}


/**
 * Set the multicast list. Currently unimplemented.
 *
 * @param dev    Device to work on
 */
static void cvm_oct_common_set_multicast_list(struct net_device *dev)
{
    cvmx_gmxx_prtx_cfg_t gmx_cfg;
    cvm_oct_private_t* priv = (cvm_oct_private_t*)netdev_priv(dev);
    int interface = INTERFACE(priv->port);
    int index = INDEX(priv->port);

    if ((interface < 2) && (cvmx_helper_interface_get_mode(interface) != CVMX_HELPER_INTERFACE_MODE_SPI))
    {
        cvmx_gmxx_rxx_adr_ctl_t control;
        control.u64 = 0;
        control.s.bcst = 1;     /* Allow broadcast MAC addresses */

        if (dev->mc_list || (dev->flags&IFF_ALLMULTI) || (dev->flags&IFF_PROMISC))
            control.s.mcst = 2; /* Force accept multicast packets */
        else
            control.s.mcst = 1; /* Force reject multicat packets */

        if(dev->flags&IFF_PROMISC)
            control.s.cam_mode = 0; /* Reject matches if promisc. Since CAM is shut off, should accept everything */
        else
            control.s.cam_mode = 1; /* Filter packets based on the CAM */

        gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
        cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64 & ~1ull);

        cvmx_write_csr(CVMX_GMXX_RXX_ADR_CTL(index, interface), control.u64);
        if (dev->flags&IFF_PROMISC)
            cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM_EN(index, interface), 0);
        else
            cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM_EN(index, interface), 1);

        cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64);
    }
}


/**
 * Set the hardware MAC address for a device
 *
 * @param dev    Device to change the MAC address for
 * @param addr   Address structure to change it too. MAC address is addr + 2.
 * @return Zero on success
 */
static int cvm_oct_common_set_mac_address(struct net_device *dev, void *addr)
{
    cvm_oct_private_t* priv = (cvm_oct_private_t*)netdev_priv(dev);
    cvmx_gmxx_prtx_cfg_t gmx_cfg;
    int interface = INTERFACE(priv->port);
    int index = INDEX(priv->port);

    memcpy(dev->dev_addr, addr + 2, 6);

    if ((interface < 2) && (cvmx_helper_interface_get_mode(interface) != CVMX_HELPER_INTERFACE_MODE_SPI))
    {
        int i;
        uint8_t *ptr = addr;
        uint64_t mac = 0;
        for (i=0; i<6; i++)
            mac = (mac<<8) | (uint64_t)(ptr[i+2]);

        gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
        cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64 & ~1ull);

        cvmx_write_csr(CVMX_GMXX_SMACX(index, interface), mac);
        cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM0(index, interface), ptr[2]);
        cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM1(index, interface), ptr[3]);
        cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM2(index, interface), ptr[4]);
        cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM3(index, interface), ptr[5]);
        cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM4(index, interface), ptr[6]);
        cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM5(index, interface), ptr[7]);
        cvm_oct_common_set_multicast_list(dev);
        cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64);
    }
    return 0;
}


/**
 * Change the link MTU. Unimplemented
 *
 * @param dev     Device to change
 * @param new_mtu The new MTU
 * @return Zero on success
 */
static int cvm_oct_common_change_mtu(struct net_device *dev, int new_mtu)
{
    cvm_oct_private_t* priv = (cvm_oct_private_t*)netdev_priv(dev);
    int interface = INTERFACE(priv->port);
    int index = INDEX(priv->port);

    /* Limit the MTU to make sure the ethernet packets are between 64 bytes
        and 65535 bytes */
    if ((new_mtu + 14 + 4 < 64) || (new_mtu + 14 + 4 > 65392))
    {
        printk("MTU must be between %d and %d.\n", 64-14-4, 65392-14-4);
        return -EINVAL;
    }
    dev->mtu = new_mtu;

    if ((interface < 2) && (cvmx_helper_interface_get_mode(interface) != CVMX_HELPER_INTERFACE_MODE_SPI))
    {
        int max_packet = new_mtu + 14 + 4; /* Add ethernet header and possible VLAN */

        if (OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
        {
            /* Signal errors on packets larger than the MTU */
            cvmx_write_csr(CVMX_GMXX_RXX_FRM_MAX(index, interface), max_packet);
        }
        else
        {
            /* Set the hardware to truncate packets larger than the MTU and
                smaller the 64 bytes */
            cvmx_pip_frm_len_chkx_t frm_len_chk;
            frm_len_chk.u64 = 0;
            frm_len_chk.s.minlen = 64;
            frm_len_chk.s.maxlen = max_packet;
            cvmx_write_csr(CVMX_PIP_FRM_LEN_CHKX(interface), frm_len_chk.u64);
        }
        /* Set the hardware to truncate packets larger than the MTU. The
            jabber register must be set to a multiple of 8 bytes, so round up */
        cvmx_write_csr(CVMX_GMXX_RXX_JABBER(index, interface), (max_packet + 7) & ~7u);
    }
    return 0;
}


/**
 * Per network device initialization
 *
 * @param dev    Device to initialize
 * @return Zero on success
 */
int cvm_oct_common_init(struct net_device *dev)
{
    static int count = 0;
    char mac[8] = {0x00,0x00,
        octeon_bootinfo->mac_addr_base[0],
        octeon_bootinfo->mac_addr_base[1],
        octeon_bootinfo->mac_addr_base[2],
        octeon_bootinfo->mac_addr_base[3],
        octeon_bootinfo->mac_addr_base[4],
        octeon_bootinfo->mac_addr_base[5] + count};
    cvm_oct_private_t* priv = (cvm_oct_private_t*)netdev_priv(dev);


    /* Force the interface to use the POW send if always_use_pow was
        specified or it is in the pow send list */
    if ((pow_send_group != -1) && (always_use_pow || strstr(pow_send_list, dev->name)))
        priv->queue = -1;

    if (priv->queue != -1)
    {
        dev->hard_start_xmit= cvm_oct_xmit;
        if (USE_HW_TCPUDP_CHECKSUM)
            dev->features |= NETIF_F_IP_CSUM;
    }
    else
        dev->hard_start_xmit= cvm_oct_xmit_pow;
    count++;

    dev->get_stats          = cvm_oct_common_get_stats;
    dev->set_mac_address    = cvm_oct_common_set_mac_address;
    dev->set_multicast_list = cvm_oct_common_set_multicast_list;
    dev->change_mtu         = cvm_oct_common_change_mtu;
    dev->do_ioctl           = cvm_oct_ioctl;
    dev->features           |= NETIF_F_LLTX; /* We do our own locking, Linux doesn't need to */
    SET_ETHTOOL_OPS(dev, &cvm_oct_ethtool_ops);
#ifdef CONFIG_NET_POLL_CONTROLLER
    dev->poll_controller    = cvm_oct_poll_controller;
#endif

    cvm_oct_mdio_setup_device(dev);
    dev->set_mac_address(dev, mac);
    dev->change_mtu(dev, dev->mtu);

    /* Zero out stats for port so we won't mistakenly show counters from the
        bootloader */
    memset(dev->get_stats(dev), 0, sizeof(struct net_device_stats));

    return 0;
}

void cvm_oct_common_uninit(struct net_device *dev)
{
    /* Currently nothing to do */
}

