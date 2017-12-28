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
 * Small helper utilities.
 *
 * <hr>$Revision: 1.1 $<hr>
 */
#include "executive-config.h"
#include "cvmx-config.h"
#include "cvmx.h"
#include "cvmx-bootmem.h"
#include "cvmx-fpa.h"
#include "cvmx-pip.h"
#include "cvmx-pko.h"
#include "cvmx-ipd.h"
#include "cvmx-asx.h"
#include "cvmx-gmx.h"
#include "cvmx-spi.h"
#include "cvmx-sysinfo.h"
#include "cvmx-helper.h"
#include "cvmx-helper-util.h"
#include "cvmx-version.h"

#ifdef CVMX_ENABLE_HELPER_FUNCTIONS

/**
 * Get the version of the CVMX libraries.
 *
 * @return Version string. Note this buffer is allocated statically
 *         and will be shared by all callers.
 */
const char *cvmx_helper_get_version(void)
{
    return OCTEON_SDK_VERSION_STRING;
}


/**
 * Convert a interface mode into a human readable string
 *
 * @param mode   Mode to convert
 *
 * @return String
 */
const char *cvmx_helper_interface_mode_to_string(cvmx_helper_interface_mode_t mode)
{
    switch (mode)
    {
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:   return "DISABLED";
        case CVMX_HELPER_INTERFACE_MODE_RGMII:      return "RGMII";
        case CVMX_HELPER_INTERFACE_MODE_GMII:       return "GMII";
        case CVMX_HELPER_INTERFACE_MODE_SPI:        return "SPI";
        case CVMX_HELPER_INTERFACE_MODE_PCIE:       return "PCIE";
        case CVMX_HELPER_INTERFACE_MODE_XAUI:       return "XAUI";
        case CVMX_HELPER_INTERFACE_MODE_SGMII:      return "SGMII";
        case CVMX_HELPER_INTERFACE_MODE_PICMG:      return "PICMG";
        case CVMX_HELPER_INTERFACE_MODE_NPI:        return "NPI";
        case CVMX_HELPER_INTERFACE_MODE_LOOP:       return "LOOP";
    }
    return "UNKNOWN";
}


/**
 * Debug routine to dump the packet structure to the console
 *
 * @param work   Work queue entry containing the packet to dump
 * @return
 */
int cvmx_helper_dump_packet(cvmx_wqe_t *work)
{
    uint64_t        count;
    uint64_t        remaining_bytes;
    cvmx_buf_ptr_t  buffer_ptr;
    uint64_t        start_of_buffer;
    uint8_t *       data_address;
    uint8_t *       end_of_data;

    cvmx_dprintf("Packet Length:   %u\n", work->len);
    cvmx_dprintf("    Input Port:  %u\n", work->ipprt);
    cvmx_dprintf("    QoS:         %u\n", work->qos);
    cvmx_dprintf("    Buffers:     %u\n", work->word2.s.bufs);

    if (work->word2.s.bufs == 0)
    {
        cvmx_ipd_wqe_fpa_queue_t wqe_pool;
        wqe_pool.u64 = cvmx_read_csr(CVMX_IPD_WQE_FPA_QUEUE);
        buffer_ptr.u64 = 0;
        buffer_ptr.s.pool = wqe_pool.s.wqe_pool;
        buffer_ptr.s.size = 128;
        buffer_ptr.s.addr = cvmx_ptr_to_phys(work->packet_data);
        if (cvmx_likely(!work->word2.s.not_IP))
        {
            if (work->word2.s.is_v6)
                buffer_ptr.s.addr += 2;
            else
                buffer_ptr.s.addr += 6;
        }
    }
    else
        buffer_ptr = work->packet_ptr;
    remaining_bytes = work->len;

    while (remaining_bytes)
    {
        start_of_buffer = ((buffer_ptr.s.addr >> 7) - buffer_ptr.s.back) << 7;
        cvmx_dprintf("    Buffer Start:%llx\n", (unsigned long long)start_of_buffer);
        cvmx_dprintf("    Buffer I   : %u\n", buffer_ptr.s.i);
        cvmx_dprintf("    Buffer Back: %u\n", buffer_ptr.s.back);
        cvmx_dprintf("    Buffer Pool: %u\n", buffer_ptr.s.pool);
        cvmx_dprintf("    Buffer Data: %llx\n", (unsigned long long)buffer_ptr.s.addr);
        cvmx_dprintf("    Buffer Size: %u\n", buffer_ptr.s.size);

        cvmx_dprintf("\t\t");
        data_address = (uint8_t *)cvmx_phys_to_ptr(buffer_ptr.s.addr);
        end_of_data = data_address + buffer_ptr.s.size;
        count = 0;
        while (data_address < end_of_data)
        {
            if (remaining_bytes == 0)
                break;
            else
                remaining_bytes--;
            cvmx_dprintf("%02x", (unsigned int)*data_address);
            data_address++;
            if (remaining_bytes && (count == 7))
            {
                cvmx_dprintf("\n\t\t");
                count = 0;
            }
            else
                count++;
        }
        cvmx_dprintf("\n");

        if (remaining_bytes)
            buffer_ptr = *(cvmx_buf_ptr_t*)cvmx_phys_to_ptr(buffer_ptr.s.addr - 8);
    }
    return 0;
}


/**
 * Setup Random Early Drop on a specific input queue
 *
 * @param queue  Input queue to setup RED on (0-7)
 * @param pass_thresh
 *               Packets will begin slowly dropping when there are less than
 *               this many packet buffers free in FPA 0.
 * @param drop_thresh
 *               All incomming packets will be dropped when there are less
 *               than this many free packet buffers in FPA 0.
 * @return Zero on success. Negative on failure
 */
int cvmx_helper_setup_red_queue(int queue, int pass_thresh, int drop_thresh)
{
    cvmx_ipd_qos_red_marks_t red_marks;
    cvmx_ipd_red_quex_param_t red_param;

    /* Set RED to begin dropping packets when there are pass_thresh buffers
        left. It will linearly drop more packets until reaching drop_thresh
        buffers */
    red_marks.u64 = 0;
    red_marks.s.drop = drop_thresh;
    red_marks.s.pass = pass_thresh;
    cvmx_write_csr(CVMX_IPD_QOSX_RED_MARKS(queue), red_marks.u64);

    /* Use the actual queue 0 counter, not the average */
    red_param.u64 = 0;
    red_param.s.prb_con = (255ul<<24) / (red_marks.s.pass - red_marks.s.drop);
    red_param.s.avg_con = 1;
    red_param.s.new_con = 255;
    red_param.s.use_pcnt = 1;
    cvmx_write_csr(CVMX_IPD_RED_QUEX_PARAM(queue), red_param.u64);
    return 0;
}


/**
 * Setup Random Early Drop to automatically begin dropping packets.
 *
 * @param pass_thresh
 *               Packets will begin slowly dropping when there are less than
 *               this many packet buffers free in FPA 0.
 * @param drop_thresh
 *               All incomming packets will be dropped when there are less
 *               than this many free packet buffers in FPA 0.
 * @return Zero on success. Negative on failure
 */
int cvmx_helper_setup_red(int pass_thresh, int drop_thresh)
{
    cvmx_ipd_portx_bp_page_cnt_t page_cnt;
    cvmx_ipd_bp_prt_red_end_t ipd_bp_prt_red_end;
    cvmx_ipd_red_port_enable_t red_port_enable;
    int queue;
    int interface;
    int port;

    /* Disable backpressure based on queued buffers. It needs SW support */
    page_cnt.u64 = 0;
    page_cnt.s.bp_enb = 0;
    page_cnt.s.page_cnt = 100;
    for (interface=0; interface<2; interface++)
    {
        for (port=cvmx_helper_get_first_ipd_port(interface); port<cvmx_helper_get_last_ipd_port(interface); port++)
            cvmx_write_csr(CVMX_IPD_PORTX_BP_PAGE_CNT(port), page_cnt.u64);
    }

    for (queue=0; queue<8; queue++)
        cvmx_helper_setup_red_queue(queue, pass_thresh, drop_thresh);

    /* Shutoff the dropping based on the per port page count. SW isn't
        decrementing it right now */
    ipd_bp_prt_red_end.u64 = 0;
    ipd_bp_prt_red_end.s.prt_enb = 0;
    cvmx_write_csr(CVMX_IPD_BP_PRT_RED_END, ipd_bp_prt_red_end.u64);

    red_port_enable.u64 = 0;
    red_port_enable.s.prt_enb = 0xfffffffffull;
    red_port_enable.s.avg_dly = 10000;
    red_port_enable.s.prb_dly = 10000;
    cvmx_write_csr(CVMX_IPD_RED_PORT_ENABLE, red_port_enable.u64);

    return 0;
}


/**
 * @INTERNAL
 * Setup the common GMX settings that determine the number of
 * ports. These setting apply to almost all configurations of all
 * chips.
 *
 * @param interface Interface to configure
 * @param num_ports Number of ports on the interface
 *
 * @return Zero on success, negative on failure
 */
int __cvmx_helper_setup_gmx(int interface, int num_ports)
{
    cvmx_gmxx_tx_prts_t gmx_tx_prts;
    cvmx_gmxx_rx_prts_t gmx_rx_prts;
    cvmx_pko_reg_gmx_port_mode_t pko_mode;

    /* Tell GMX the number of TX ports on this interface */
    gmx_tx_prts.u64 = cvmx_read_csr(CVMX_GMXX_TX_PRTS(interface));
    gmx_tx_prts.s.prts = num_ports;
    cvmx_write_csr(CVMX_GMXX_TX_PRTS(interface), gmx_tx_prts.u64);

    /* Tell GMX the number of RX ports on this interface.  This only
    ** applies to *GMII and XAUI ports */
    if (cvmx_helper_interface_get_mode(interface) == CVMX_HELPER_INTERFACE_MODE_RGMII
        || cvmx_helper_interface_get_mode(interface) == CVMX_HELPER_INTERFACE_MODE_SGMII
        || cvmx_helper_interface_get_mode(interface) == CVMX_HELPER_INTERFACE_MODE_GMII
        || cvmx_helper_interface_get_mode(interface) == CVMX_HELPER_INTERFACE_MODE_XAUI)
    {
        if (num_ports > 4)
        {
            cvmx_dprintf("__cvmx_helper_setup_gmx: Illegal num_ports\n");
            return(-1);
        }

        gmx_rx_prts.u64 = cvmx_read_csr(CVMX_GMXX_RX_PRTS(interface));
        gmx_rx_prts.s.prts = num_ports;
        cvmx_write_csr(CVMX_GMXX_RX_PRTS(interface), gmx_rx_prts.u64);
    }

    /* Skip setting CVMX_PKO_REG_GMX_PORT_MODE on 30XX and 31XX */
    if (OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX))
        return 0;

    /* Tell PKO the number of ports on this interface */
    pko_mode.u64 = cvmx_read_csr(CVMX_PKO_REG_GMX_PORT_MODE);
    if (interface == 0)
    {
        if (num_ports == 1)
            pko_mode.s.mode0 = 4;
        else if (num_ports == 2)
            pko_mode.s.mode0 = 3;
        else if (num_ports <= 4)
            pko_mode.s.mode0 = 2;
        else if (num_ports <= 8)
            pko_mode.s.mode0 = 1;
        else
            pko_mode.s.mode0 = 0;
    }
    else
    {
        if (num_ports == 1)
            pko_mode.s.mode1 = 4;
        else if (num_ports == 2)
            pko_mode.s.mode1 = 3;
        else if (num_ports <= 4)
            pko_mode.s.mode1 = 2;
        else if (num_ports <= 8)
            pko_mode.s.mode1 = 1;
        else
            pko_mode.s.mode1 = 0;
    }
    cvmx_write_csr(CVMX_PKO_REG_GMX_PORT_MODE, pko_mode.u64);
    return 0;
}


/**
 * Returns the IPD/PKO port number for a port on teh given
 * interface.
 *
 * @param interface Interface to use
 * @param port      Port on the interface
 *
 * @return IPD/PKO port number
 */
int cvmx_helper_get_ipd_port(int interface, int port)
{
    switch (interface)
    {
        case 0: return port;
        case 1: return port + 16;
        case 2: return port + 32;
        case 3: return port + 36;
    }
    return -1;
}


/**
 * Returns the interface number for an IPD/PKO port number.
 *
 * @param ipd_port IPD/PKO port number
 *
 * @return Interface number
 */
int cvmx_helper_get_interface_num(int ipd_port)
{
    if (ipd_port < 16)
        return 0;
    else if (ipd_port < 32)
        return 1;
    else if (ipd_port < 36)
        return 2;
    else if (ipd_port < 40)
        return 3;
    else
        cvmx_dprintf("cvmx_helper_get_interface_num: Illegal IPD port number\n");

    return -1;
}


/**
 * Returns the interface index number for an IPD/PKO port
 * number.
 *
 * @param ipd_port IPD/PKO port number
 *
 * @return Interface index number
 */
int cvmx_helper_get_interface_index_num(int ipd_port)
{
    if (ipd_port < 32)
        return ipd_port & 15;
    else if (ipd_port < 36)
        return ipd_port & 3;
    else if (ipd_port < 40)
        return ipd_port & 3;
    else
        cvmx_dprintf("cvmx_helper_get_interface_index_num: Illegal IPD port number\n");

    return -1;
}

#endif /* CVMX_ENABLE_HELPER_FUNCTIONS */
