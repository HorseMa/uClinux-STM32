/***********************license start***************
 * Copyright (c) 2003-2008  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
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
 *
 *
 * For any questions regarding licensing please contact marketing@caviumnetworks.com
 *
 ***********************license end**************************************/






/**
 * @file
 *
 * Functions for XAUI initialization, configuration,
 * and monitoring.
 *
 * <hr>$Revision: 1.1 $<hr>
 */
#include "executive-config.h"
#include "cvmx-config.h"
#ifdef CVMX_ENABLE_PKO_FUNCTIONS

#include "cvmx.h"
#include "cvmx-helper.h"


/**
 * @INTERNAL
 * Probe a XAUI interface and determine the number of ports
 * connected to it. The XAUI interface should still be down
 * after this call.
 *
 * @param interface Interface to probe
 *
 * @return Number of ports on the interface. Zero to disable.
 */
int __cvmx_helper_xaui_probe(int interface)
{
    int i;
    cvmx_gmxx_hg2_control_t gmx_hg2_control;

    __cvmx_helper_setup_gmx(interface, 1);

    /* Setup PKO to support 16 ports for HiGig2 virtual ports. We're pointing
        all of the PKO packet ports for this interface to the XAUI. This allows
        us to use HiGig2 backpressure per port */
    for (i=0; i<16; i++)
    {
        cvmx_pko_mem_port_ptrs_t pko_mem_port_ptrs;
        pko_mem_port_ptrs.u64 = 0;
        /* We set each PKO port to have equal priority in a round robin
            fashion */
        pko_mem_port_ptrs.s.static_p = 0;
        pko_mem_port_ptrs.s.qos_mask = 0xff;
        /* All PKO ports map to the same XAUI hardware port */
        pko_mem_port_ptrs.s.eid = interface*4;
        pko_mem_port_ptrs.s.pid = interface*16 + i;
        cvmx_write_csr(CVMX_PKO_MEM_PORT_PTRS, pko_mem_port_ptrs.u64);
    }

    /* If HiGig2 is enabled return 16 ports, otherwise return 1 port */
    gmx_hg2_control.u64 = cvmx_read_csr(CVMX_GMXX_HG2_CONTROL(interface));
    if (gmx_hg2_control.s.hg2tx_en)
        return 16;
    else
        return 1;
}


/**
 * @INTERNAL
 * Bringup and enable a XAUI interface. After this call packet
 * I/O should be fully functional. This is called with IPD
 * enabled but PKO disabled.
 *
 * @param interface Interface to bring up
 *
 * @return Zero on success, negative on failure
 */
int __cvmx_helper_xaui_enable(int interface)
{
    cvmx_gmxx_inf_mode_t          mode;
    cvmx_gmxx_prtx_cfg_t          gmx_cfg;
    cvmx_pcsxx_control1_reg_t     xauiCtl;
    cvmx_pcsxx_misc_ctl_reg_t     xauiMiscCtl;
    cvmx_gmxx_tx_xaui_ctl_t       gmxXauiTxCtl;
    cvmx_gmxx_rxx_int_en_t        gmx_rx_int_en;
    cvmx_gmxx_tx_int_en_t         gmx_tx_int_en;
    cvmx_pcsxx_int_en_reg_t       pcsx_int_en_reg;

    /* (1) Enable the interface. */
    mode.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(interface));
    mode.s.en = 1;
    cvmx_write_csr(CVMX_GMXX_INF_MODE(interface), mode.u64);

    /* (2) Disable GMX. */
    xauiMiscCtl.u64 = cvmx_read_csr(CVMX_PCSXX_MISC_CTL_REG(interface));
    xauiMiscCtl.s.gmxeno = 1;
    cvmx_write_csr (CVMX_PCSXX_MISC_CTL_REG(interface), xauiMiscCtl.u64);

    /* (3) Disable GMX and PCSX interrupts. */
    gmx_rx_int_en.u64 = cvmx_read_csr(CVMX_GMXX_RXX_INT_EN(0,interface));
    cvmx_write_csr(CVMX_GMXX_RXX_INT_EN(0,interface), 0x0);
    gmx_tx_int_en.u64 = cvmx_read_csr(CVMX_GMXX_TX_INT_EN(interface));
    cvmx_write_csr(CVMX_GMXX_TX_INT_EN(interface), 0x0);
    pcsx_int_en_reg.u64 = cvmx_read_csr(CVMX_PCSXX_INT_EN_REG(interface));
    cvmx_write_csr(CVMX_PCSXX_INT_EN_REG(interface), 0x0);

    /* (4) Bring up the PCSX and GMX reconciliation layer. */
    /* (4)a Set polarity and lane swapping. */
    /* (4)b */
    gmxXauiTxCtl.u64 = cvmx_read_csr (CVMX_GMXX_TX_XAUI_CTL(interface));
    gmxXauiTxCtl.s.dic_en = 0;
    gmxXauiTxCtl.s.uni_en = 0;
    cvmx_write_csr (CVMX_GMXX_TX_XAUI_CTL(interface), gmxXauiTxCtl.u64);

    /* (4)c Aply reset sequence */
    xauiCtl.u64 = cvmx_read_csr (CVMX_PCSXX_CONTROL1_REG(interface));
    xauiCtl.s.lo_pwr = 0;
    xauiCtl.s.reset  = 1;
    cvmx_write_csr (CVMX_PCSXX_CONTROL1_REG(interface), xauiCtl.u64);

    /* Wait for PCS to come out of reset */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_PCSXX_CONTROL1_REG(interface), cvmx_pcsxx_control1_reg_t, reset, ==, 0, 10000))
        return -1;
    /* Wait for PCS to be aligned */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_PCSXX_10GBX_STATUS_REG(interface), cvmx_pcsxx_10gbx_status_reg_t, alignd, ==, 1, 10000))
        return -1;
    /* Wait for RX to be ready */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_GMXX_RX_XAUI_CTL(interface), cvmx_gmxx_rx_xaui_ctl_t, status, ==, 0, 10000))
        return -1;

    /* (6) Configure GMX */
    gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(0, interface));
    gmx_cfg.s.en = 0;
    cvmx_write_csr(CVMX_GMXX_PRTX_CFG(0, interface), gmx_cfg.u64);

    /* Wait for GMX RX to be idle */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_GMXX_PRTX_CFG(0, interface), cvmx_gmxx_prtx_cfg_t, rx_idle, ==, 1, 10000))
        return -1;
    /* Wait for GMX TX to be idle */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_GMXX_PRTX_CFG(0, interface), cvmx_gmxx_prtx_cfg_t, tx_idle, ==, 1, 10000))
        return -1;

    /* GMX configure */
    gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(0, interface));
    gmx_cfg.s.speed = 1;
    gmx_cfg.s.speed_msb = 0;
    gmx_cfg.s.slottime = 1;
    cvmx_write_csr(CVMX_GMXX_TX_PRTS(interface), 1);
    cvmx_write_csr(CVMX_GMXX_TXX_SLOT(0, interface), 512);
    cvmx_write_csr(CVMX_GMXX_TXX_BURST(0, interface), 8192);
    cvmx_write_csr(CVMX_GMXX_PRTX_CFG(0, interface), gmx_cfg.u64);

    /* (7) Clear out any error state */
    cvmx_write_csr(CVMX_GMXX_RXX_INT_REG(0,interface), cvmx_read_csr(CVMX_GMXX_RXX_INT_REG(0,interface)));
    cvmx_write_csr(CVMX_GMXX_TX_INT_REG(interface), cvmx_read_csr(CVMX_GMXX_TX_INT_REG(interface)));
    cvmx_write_csr(CVMX_PCSXX_INT_REG(interface), cvmx_read_csr(CVMX_PCSXX_INT_REG(interface)));

    /* Wait for receive link */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_PCSXX_STATUS1_REG(interface), cvmx_pcsxx_status1_reg_t, rcv_lnk, ==, 1, 10000))
        return -1;
    if (CVMX_WAIT_FOR_FIELD64(CVMX_PCSXX_STATUS2_REG(interface), cvmx_pcsxx_status2_reg_t, xmtflt, ==, 0, 10000))
        return -1;
    if (CVMX_WAIT_FOR_FIELD64(CVMX_PCSXX_STATUS2_REG(interface), cvmx_pcsxx_status2_reg_t, rcvflt, ==, 0, 10000))
        return -1;

    cvmx_write_csr(CVMX_GMXX_RXX_INT_EN(0,interface), gmx_rx_int_en.u64);
    cvmx_write_csr(CVMX_GMXX_TX_INT_EN(interface), gmx_tx_int_en.u64);
    cvmx_write_csr(CVMX_PCSXX_INT_EN_REG(interface), pcsx_int_en_reg.u64);

    cvmx_helper_link_autoconf(cvmx_helper_get_ipd_port(interface, 0));

    /* (8) Enable packet reception */
    xauiMiscCtl.s.gmxeno = 0;
    cvmx_write_csr (CVMX_PCSXX_MISC_CTL_REG(interface), xauiMiscCtl.u64);

    gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(0, interface));
    gmx_cfg.s.en = 1;
    cvmx_write_csr(CVMX_GMXX_PRTX_CFG(0, interface), gmx_cfg.u64);
    return 0;
}

/**
 * @INTERNAL
 * Return the link state of an IPD/PKO port as returned by
 * auto negotiation. The result of this function may not match
 * Octeon's link config if auto negotiation has changed since
 * the last call to cvmx_helper_link_set().
 *
 * @param ipd_port IPD/PKO port to query
 *
 * @return Link state
 */
cvmx_helper_link_info_t __cvmx_helper_xaui_link_get(int ipd_port)
{
    int interface = cvmx_helper_get_interface_num(ipd_port);
    cvmx_gmxx_tx_xaui_ctl_t gmxx_tx_xaui_ctl;
    cvmx_gmxx_rx_xaui_ctl_t gmxx_rx_xaui_ctl;
    cvmx_helper_link_info_t result;

    gmxx_tx_xaui_ctl.u64 = cvmx_read_csr(CVMX_GMXX_TX_XAUI_CTL(interface));
    gmxx_rx_xaui_ctl.u64 = cvmx_read_csr(CVMX_GMXX_RX_XAUI_CTL(interface));
    result.u64 = 0;

    /* Only return a link if both RX and TX are happy */
    if ((gmxx_tx_xaui_ctl.s.ls == 0) && (gmxx_rx_xaui_ctl.s.status == 0))
    {
        result.s.link_up = 1;
        result.s.full_duplex = 1;
        result.s.speed = 10000;
    }
    else
    {
        /* Disable GMX and PCSX interrupts. */
        cvmx_write_csr (CVMX_GMXX_RXX_INT_EN(0,interface), 0x0);
        cvmx_write_csr (CVMX_GMXX_TX_INT_EN(interface), 0x0);
        cvmx_write_csr (CVMX_PCSXX_INT_EN_REG(interface), 0x0);
    }
    return result;
}


/**
 * @INTERNAL
 * Configure an IPD/PKO port for the specified link state. This
 * function does not influence auto negotiation at the PHY level.
 * The passed link state must always match the link state returned
 * by cvmx_helper_link_get(). It is normally best to use
 * cvmx_helper_link_autoconf() instead.
 *
 * @param ipd_port  IPD/PKO port to configure
 * @param link_info The new link state
 *
 * @return Zero on success, negative on failure
 */
int __cvmx_helper_xaui_link_set(int ipd_port, cvmx_helper_link_info_t link_info)
{
    int interface = cvmx_helper_get_interface_num(ipd_port);
    cvmx_gmxx_tx_xaui_ctl_t gmxx_tx_xaui_ctl;
    cvmx_gmxx_rx_xaui_ctl_t gmxx_rx_xaui_ctl;

    gmxx_tx_xaui_ctl.u64 = cvmx_read_csr(CVMX_GMXX_TX_XAUI_CTL(interface));
    gmxx_rx_xaui_ctl.u64 = cvmx_read_csr(CVMX_GMXX_RX_XAUI_CTL(interface));

    /* If the link shouldn't be up, then just return */
    if (!link_info.s.link_up)
        return 0;

    /* Do nothing if both RX and TX are happy */
    if ((gmxx_tx_xaui_ctl.s.ls == 0) && (gmxx_rx_xaui_ctl.s.status == 0))
        return 0;

    /* Bring the link up */
    return __cvmx_helper_xaui_enable(interface);
}

#endif /* CVMX_ENABLE_PKO_FUNCTIONS */

