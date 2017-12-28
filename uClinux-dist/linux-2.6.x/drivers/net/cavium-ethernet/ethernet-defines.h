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

/*
 * A few defines are used to control the operation of this driver:
 *  CONFIG_CAVIUM_RESERVE32
 *      This kernel config options controls the amount of memory configured
 *      in a wired TLB entry for all processes to share. If this is set, the
 *      driver will use this memory instead of kernel memory for pools. This
 *      allows 32bit userspace application to access the buffers, but also
 *      requires all received packets to be copied.
 *  CONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS
 *      This kernel config option allows the user to control the number of
 *      packet and work queue buffers allocated by the driver. If this is zero,
 *      the driver uses the default from below.
 *  NUM_PACKET_BUFFERS
 *      This define is set based on CONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS. It
 *      is used by the driver to determine the number of packet and work queue
 *      buffers.
 *  USE_SKBUFFS_IN_HW
 *      Tells the driver to populate the packet buffers with kernel skbuffs.
 *      This allows the driver to receive packets without copying them. It also
 *      means that 32bit userspace can't access the packet buffers.
 *  USE_32BIT_SHARED
 *      This define tells the driver to allocate memory for buffers from the
 *      32bit sahred region instead of the kernel memory space.
 *  USE_HW_TCPUDP_CHECKSUM
 *      Controls if the Octeon TCP/UDP checksum engine is used for packet
 *      output. If this is zero, the kernel will perform the checksum in
 *      software.
 *  USE_MULTICORE_RECEIVE
 *      Process receive interrupts on multiple cores. This spreads the network
 *      load across the first 8 processors. If ths is zero, only one core
 *      processes incomming packets.
 *  USE_ASYNC_IOBDMA
 *      Use asynchronous IO access to hardware. This uses Octeon's asynchronous
 *      IOBDMAs to issue IO accesses without stalling. Set this to zero
 *      to disable this. Note that IOBDMAs require CVMSEG.
 *  REUSE_SKBUFFS_WITHOUT_FREE
 *      Allows the TX path to free an skbuff into the FPA hardware pool. This
 *      can significantly improve performance for forwarding and bridging, but
 *      may be somewhat dangerous. Checks are made, but if any buffer is reused
 *      without the proper Linux cleanup, the networking stack may have very
 *      bizarre bugs.
 */
#ifndef CONFIG_CAVIUM_RESERVE32
#define CONFIG_CAVIUM_RESERVE32 0
#endif

#if CONFIG_CAVIUM_RESERVE32
    #define USE_32BIT_SHARED            1
    #define USE_SKBUFFS_IN_HW           0
    #define REUSE_SKBUFFS_WITHOUT_FREE  0
#else
    #define USE_32BIT_SHARED            0
    #define USE_SKBUFFS_IN_HW           1
    #define REUSE_SKBUFFS_WITHOUT_FREE  0
#endif

#if defined(CONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS) && CONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS
    #define NUM_PACKET_BUFFERS      CONFIG_CAVIUM_OCTEON_NUM_PACKET_BUFFERS
#else
    #define NUM_PACKET_BUFFERS      1024
#endif

#define INTERRUPT_LIMIT             10000   /* Max interrupts per second per core */
//#define INTERRUPT_LIMIT             0       /* Don't limit the number of interrupts */
#define USE_HW_TCPUDP_CHECKSUM      1
#define USE_MULTICORE_RECEIVE       1
#define USE_RED                     1	/* Enable Random Early Dropping under load */
#define USE_ASYNC_IOBDMA            (CONFIG_CAVIUM_OCTEON_CVMSEG_SIZE > 0)
#define USE_10MBPS_PREAMBLE_WORKAROUND 1    /* Allow SW based preamble removal at 10Mbps to workaround PHYs giving us bad preambles */
#define DONT_WRITEBACK(x)           (x) /* Use this to have all FPA frees also tell the L2 not to write data to memory */
//#define DONT_WRITEBACK(x)         0   /* Use this to not have FPA frees control L2 */

#ifndef CONFIG_SMP
#undef USE_MULTICORE_RECEIVE
#define USE_MULTICORE_RECEIVE 0
#endif

#define IP_PROTOCOL_TCP             6
#define IP_PROTOCOL_UDP             0x11
#define FAU_NUM_PACKET_BUFFERS_TO_FREE (CVMX_FAU_REG_END - sizeof(uint32_t))
#define TOTAL_NUMBER_OF_PORTS       (CVMX_PIP_NUM_INPUT_PORTS+1)

