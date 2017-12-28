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
 * Support library for the hardware Packet Output unit.
 *
 * <hr>$Revision: 1.1 $<hr>
 */
#include "executive-config.h"
#include "cvmx-config.h"
#include "cvmx.h"
#include "cvmx-pko.h"
#include "cvmx-sysinfo.h"
#include "cvmx-helper.h"

/**
 * Internal state of packet output
 */

#ifdef CVMX_ENABLE_PKO_FUNCTIONS

/**
 * Call before any other calls to initialize the packet
 * output system.  This does chip global config, and should only be
 * done by one core.
 */

void cvmx_pko_initialize_global(void)
{
    int i;
    uint64_t priority = 8;
    cvmx_pko_pool_cfg_t config;

    /* Set the size of the PKO command buffers to an odd number of 64bit
        words. This allows the normal two word send to stay aligned and never
        span a comamnd word buffer. */
    config.u64 = 0;
    config.s.pool = CVMX_FPA_OUTPUT_BUFFER_POOL;
    config.s.size = CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE / 8 - 1;

    cvmx_write_csr(CVMX_PKO_REG_CMD_BUF, config.u64);

    for (i=0; i<CVMX_PKO_MAX_OUTPUT_QUEUES; i++)
        cvmx_pko_config_port(CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID, i, 1, &priority);

    /* If we aren't using all of the queues optimize PKO's internal memory */
    if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        int num_interfaces = cvmx_helper_get_number_of_interfaces();
        int last_port = cvmx_helper_get_last_ipd_port(num_interfaces-1);
        int max_queues = cvmx_pko_get_base_queue(last_port) + cvmx_pko_get_num_queues(last_port);
        if (OCTEON_IS_MODEL(OCTEON_CN38XX))
        {
            if (max_queues <= 32)
                cvmx_write_csr(CVMX_PKO_REG_QUEUE_MODE, 2);
            else if (max_queues <= 64)
                cvmx_write_csr(CVMX_PKO_REG_QUEUE_MODE, 1);
        }
        else
        {
            if (max_queues <= 64)
                cvmx_write_csr(CVMX_PKO_REG_QUEUE_MODE, 2);
            else if (max_queues <= 128)
                cvmx_write_csr(CVMX_PKO_REG_QUEUE_MODE, 1);
        }
    }
}

/**
 * This function does per-core initialization required by the PKO routines.
 * This must be called on all cores that will do packet output, and must
 * be called after the FPA has been initialized and filled with pages.
 *
 * @return 0 on success
 *         !0 on failure
 */
int cvmx_pko_initialize_local(void)
{
#if CVMX_PKO_USE_FAU_FOR_OUTPUT_QUEUES
    /* Preallocate a command buffer into the designated scratch memory location */
    cvmx_fpa_async_alloc(CVMX_SCR_OQ_BUF_PRE_ALLOC, CVMX_FPA_OUTPUT_BUFFER_POOL);
    CVMX_SYNCIOBDMA;
    return(!cvmx_scratch_read64(CVMX_SCR_OQ_BUF_PRE_ALLOC));
#else
    /* Nothing to do */
    return 0;
#endif
}
#endif

/**
 * Enables the packet output hardware. It must already be
 * configured.
 */
void cvmx_pko_enable(void)
{
    cvmx_pko_reg_flags_t flags;

    flags.u64 = cvmx_read_csr(CVMX_PKO_REG_FLAGS);
    if (flags.s.ena_pko)
        cvmx_dprintf("Warning: Enabling PKO when PKO already enabled.\n");

    flags.s.ena_dwb = 1;
    flags.s.ena_pko = 1;
    flags.s.store_be =1;  /* always enable big endian for 3-word command. Does nothing for 2-word */ 
    cvmx_write_csr(CVMX_PKO_REG_FLAGS, flags.u64);
}


/**
 * Disables the packet output. Does not affect any configuration.
 */
void cvmx_pko_disable(void)
{
    cvmx_pko_reg_flags_t pko_reg_flags;
    pko_reg_flags.u64 = cvmx_read_csr(CVMX_PKO_REG_FLAGS);
    pko_reg_flags.s.ena_pko = 0;
    cvmx_write_csr(CVMX_PKO_REG_FLAGS, pko_reg_flags.u64);
}


#ifdef CVMX_ENABLE_PKO_FUNCTIONS
/**
 * @INTERNAL
 * Reset the packet output.
 */
static void __cvmx_pko_reset(void)
{
    cvmx_pko_reg_flags_t pko_reg_flags;
    pko_reg_flags.u64 = cvmx_read_csr(CVMX_PKO_REG_FLAGS);
    pko_reg_flags.s.reset = 1;
    cvmx_write_csr(CVMX_PKO_REG_FLAGS, pko_reg_flags.u64);
}


#if CVMX_PKO_USE_FAU_FOR_OUTPUT_QUEUES
static CVMX_SHARED uint8_t cvmx_pko_queue_map[CVMX_PKO_MAX_OUTPUT_QUEUES_STATIC];  /* Used to track queue usage for shutdown */
#endif
/**
 * Shutdown and free resources required by packet output.
 */
void cvmx_pko_shutdown(void)
{
    cvmx_pko_queue_cfg_t config;
    int queue;

    cvmx_pko_disable();

    for (queue=0; queue<CVMX_PKO_MAX_OUTPUT_QUEUES; queue++)
    {
        config.u64          = 0;
        config.s.tail       = 1;
        config.s.index      = 0;
        config.s.port       = CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID;
        config.s.queue      = queue & 0x7f;
        config.s.qos_mask   = 0;
        config.s.buf_ptr    = 0;
        if (!OCTEON_IS_MODEL(OCTEON_CN3XXX))
        {
            cvmx_pko_reg_queue_ptrs1_t config1;
            config1.u64 = 0;
            config1.s.qid7 = queue >> 7;
            cvmx_write_csr(CVMX_PKO_REG_QUEUE_PTRS1, config1.u64);
        }
        cvmx_write_csr(CVMX_PKO_MEM_QUEUE_PTRS, config.u64);
#if CVMX_PKO_USE_FAU_FOR_OUTPUT_QUEUES
        if(cvmx_pko_queue_map[queue])
        {
            uint64_t buf_addr = cvmx_pko_update_queue_index(queue, 0) >> CVMX_PKO_INDEX_BITS;
            if (buf_addr)
            {
                cvmx_fpa_free(cvmx_phys_to_ptr(buf_addr), CVMX_FPA_OUTPUT_BUFFER_POOL, 0);
            }
        }
#else
        cvmx_cmd_queue_shutdown(CVMX_CMD_QUEUE_PKO(queue));
#endif
    }
    __cvmx_pko_reset();
}


/**
 * Configure a output port and the associated queues for use.
 *
 * @param port       Port to configure.
 * @param base_queue First queue number to associate with this port.
 * @param num_queues Number of queues to associate with this port
 * @param priority   Array of priority levels for each queue. Values are
 *                   allowed to be 0-8. A value of 8 get 8 times the traffic
 *                   of a value of 1.  A value of 0 indicates that no rounds
 *                   will be participated in. These priorities can be changed
 *                   on the fly while the pko is enabled. A priority of 9
 *                   indicates that static priority should be used.  If static
 *                   priority is used all queues with static priority must be
 *                   contiguous starting at the base_queue, and lower numbered
 *                   queues have higher priority than higher numbered queues.
 *                   There must be num_queues elements in the array.
 */
cvmx_pko_status_t cvmx_pko_config_port(uint64_t port, uint64_t base_queue, uint64_t num_queues, const uint64_t priority[])
{
    cvmx_pko_status_t          result_code;
    uint64_t                   queue;
    cvmx_pko_queue_cfg_t       config;
    cvmx_pko_reg_queue_ptrs1_t config1;
    int static_priority_base = -1;
    int static_priority_end = -1;


    if ((port >= CVMX_PKO_NUM_OUTPUT_PORTS) && (port != CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID))
    {
        cvmx_dprintf("ERROR: cvmx_pko_config_port: Invalid port %llu\n", (unsigned long long)port);
        return CVMX_PKO_INVALID_PORT;
    }

    if (base_queue + num_queues > CVMX_PKO_MAX_OUTPUT_QUEUES)
    {
        cvmx_dprintf("ERROR: cvmx_pko_config_port: Invalid queue range %llu\n", (unsigned long long)(base_queue + num_queues));
        return CVMX_PKO_INVALID_QUEUE;
    }

    if (port != CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID)
    {
        /* Validate the static queue priority setup and set static_priority_base and static_priority_end
        ** accordingly. */
        for (queue = 0; queue < num_queues; queue++)
        {
            /* Find first queue of static priority */
            if (static_priority_base == -1 && priority[queue] == CVMX_PKO_QUEUE_STATIC_PRIORITY)
                static_priority_base = queue;
            /* Find last queue of static priority */
            if (static_priority_base != -1 && static_priority_end == -1 && priority[queue] != CVMX_PKO_QUEUE_STATIC_PRIORITY && queue)
                static_priority_end = queue - 1;
            else if (static_priority_base != -1 && static_priority_end == -1 && queue == num_queues - 1)
                static_priority_end = queue;  /* all queues are static priority */
            /* Check to make sure all static priority queues are contiguous.  Also catches some cases of
            ** static priorites not starting at queue 0. */
            if (static_priority_end != -1 && (int)queue > static_priority_end && priority[queue] == CVMX_PKO_QUEUE_STATIC_PRIORITY)
            {
                cvmx_dprintf("ERROR: cvmx_pko_config_port: Static priority queues aren't contiguous or don't start at base queue. q: %d, eq: %d\n", (int)queue, static_priority_end);
                return CVMX_PKO_INVALID_PRIORITY;
            }
        }
        if (static_priority_base > 0)
        {
            cvmx_dprintf("ERROR: cvmx_pko_config_port: Static priority queues don't start at base queue. sq: %d\n", static_priority_base);
            return CVMX_PKO_INVALID_PRIORITY;
        }
#if 0
        cvmx_dprintf("Port %d: Static priority queue base: %d, end: %d\n", port, static_priority_base, static_priority_end);
#endif
    }
    /* At this point, static_priority_base and static_priority_end are either both -1,
    ** or are valid start/end queue numbers */

    result_code = CVMX_PKO_SUCCESS;

#ifdef PKO_DEBUG
    cvmx_dprintf("num queues: %d (%lld,%lld)\n", num_queues, CVMX_PKO_QUEUES_PER_PORT_INTERFACE0, CVMX_PKO_QUEUES_PER_PORT_INTERFACE1);
#endif

    for (queue = 0; queue < num_queues; queue++)
    {
        uint64_t  *buf_ptr = NULL;

        config1.u64         = 0;
        config1.s.idx3      = queue >> 3;
        config1.s.qid7      = (base_queue + queue) >> 7;

        config.u64          = 0;
        config.s.tail       = queue == (num_queues - 1);
        config.s.index      = queue;
        config.s.port       = port;
        config.s.queue      = base_queue + queue;

        if (!cvmx_octeon_is_pass1())
        {
            config.s.static_p   = static_priority_base >= 0;
            config.s.static_q   = (int)queue <= static_priority_end;
            config.s.s_tail     = (int)queue == static_priority_end;
        }
        /* Convert the priority into an enable bit field. Try to space the bits
            out evenly so the packet don't get grouped up */
        switch ((int)priority[queue])
        {
            case 0: config.s.qos_mask = 0x00; break;
            case 1: config.s.qos_mask = 0x01; break;
            case 2: config.s.qos_mask = 0x11; break;
            case 3: config.s.qos_mask = 0x49; break;
            case 4: config.s.qos_mask = 0x55; break;
            case 5: config.s.qos_mask = 0x57; break;
            case 6: config.s.qos_mask = 0x77; break;
            case 7: config.s.qos_mask = 0x7f; break;
            case 8: config.s.qos_mask = 0xff; break;
            case CVMX_PKO_QUEUE_STATIC_PRIORITY:
                if (!cvmx_octeon_is_pass1()) /* Pass 1 will fall through to the error case */
                {
                    config.s.qos_mask = 0xff;
                    break;
                }
            default:
                cvmx_dprintf("ERROR: cvmx_pko_config_port: Invalid priority %llu\n", (unsigned long long)priority[queue]);
                config.s.qos_mask = 0xff;
                result_code = CVMX_PKO_INVALID_PRIORITY;
                break;
        }

        if (port != CVMX_PKO_MEM_QUEUE_PTRS_ILLEGAL_PID)
        {
#if CVMX_PKO_USE_FAU_FOR_OUTPUT_QUEUES
            buf_ptr = (uint64_t*)cvmx_fpa_alloc(CVMX_FPA_OUTPUT_BUFFER_POOL);
            if (!buf_ptr)
            {
                cvmx_dprintf("ERROR: cvmx_pko_config_port: Unable to allocate output buffer.\n");
                return(CVMX_PKO_NO_MEMORY);
            }

            /* Set initial command buffer address and index in FAU register for queue */
            cvmx_pko_set_queue_index(base_queue + queue, cvmx_ptr_to_phys(buf_ptr) << CVMX_PKO_INDEX_BITS);
            cvmx_pko_queue_map[base_queue + queue] = 1;
#else
            cvmx_cmd_queue_result_t cmd_res = cvmx_cmd_queue_initialize(CVMX_CMD_QUEUE_PKO(base_queue + queue),
                                                                        CVMX_PKO_MAX_QUEUE_DEPTH,
                                                                        CVMX_FPA_OUTPUT_BUFFER_POOL,
                                                                        CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE - CVMX_PKO_COMMAND_BUFFER_SIZE_ADJUST*8);
            if (cmd_res != CVMX_CMD_QUEUE_SUCCESS)
            {
                cvmx_dprintf("ERROR: cvmx_pko_config_port: Unable to allocate output buffer.\n");
                return(CVMX_PKO_NO_MEMORY);
            }

            buf_ptr = (uint64_t*)cvmx_cmd_queue_buffer(CVMX_CMD_QUEUE_PKO(base_queue + queue));
#endif
            config.s.buf_ptr = cvmx_ptr_to_phys(buf_ptr);
        }
        else
            config.s.buf_ptr = 0;

        CVMX_SYNCWS;

        if (!OCTEON_IS_MODEL(OCTEON_CN3XXX))
        {
            cvmx_write_csr(CVMX_PKO_REG_QUEUE_PTRS1, config1.u64);
        }
        cvmx_write_csr(CVMX_PKO_MEM_QUEUE_PTRS, config.u64);
    }

    return result_code;
}

#ifdef PKO_DEBUG
/**
 * Show map of ports -> queues for different cores.
 */
void cvmx_pko_show_queue_map()
{
    int core, port;
    int pko_output_ports = 36;

    cvmx_dprintf("port");
    for(port=0; port<pko_output_ports; port++)
        cvmx_dprintf("%3d ", port);
    cvmx_dprintf("\n");

    for(core=0; core<CVMX_MAX_CORES; core++)
    {
        cvmx_dprintf("\n%2d: ", core);
        for(port=0; port<pko_output_ports; port++)
        {
            cvmx_dprintf("%3d ", cvmx_pko_get_base_queue_per_core(port, core));
        }
    }
    cvmx_dprintf("\n");
}
#endif


/**
 * Rate limit a PKO port to a max packets/sec. This function is only
 * supported on CN57XX, CN56XX, CN55XX, and CN54XX.
 *
 * @param port      Port to rate limit
 * @param packets_s Maximum packet/sec
 * @param burst     Maximum number of packets to burst in a row before rate
 *                  limiting cuts in.
 *
 * @return Zero on success, negative on failure
 */
int cvmx_pko_rate_limit_packets(int port, int packets_s, int burst)
{
    cvmx_pko_mem_port_rate0_t pko_mem_port_rate0;
    cvmx_pko_mem_port_rate1_t pko_mem_port_rate1;

    pko_mem_port_rate0.u64 = 0;
    pko_mem_port_rate0.s.pid = port;
    pko_mem_port_rate0.s.rate_pkt = cvmx_sysinfo_get()->cpu_clock_hz / packets_s / 16;
    /* No cost per word since we are limited by packets/sec, not bits/sec */
    pko_mem_port_rate0.s.rate_word = 0;

    pko_mem_port_rate1.u64 = 0;
    pko_mem_port_rate1.s.pid = port;
    pko_mem_port_rate1.s.rate_lim = ((uint64_t)pko_mem_port_rate0.s.rate_pkt * burst) >> 8;

    cvmx_write_csr(CVMX_PKO_MEM_PORT_RATE0, pko_mem_port_rate0.u64);
    cvmx_write_csr(CVMX_PKO_MEM_PORT_RATE1, pko_mem_port_rate1.u64);
    return 0;
}


/**
 * Rate limit a PKO port to a max bits/sec. This function is only
 * supported on CN57XX, CN56XX, CN55XX, and CN54XX.
 *
 * @param port   Port to rate limit
 * @param bits_s PKO rate limit in bits/sec
 * @param burst  Maximum number of bits to burst before rate
 *               limiting cuts in.
 *
 * @return Zero on success, negative on failure
 */
int cvmx_pko_rate_limit_bits(int port, uint64_t bits_s, int burst)
{
    cvmx_pko_mem_port_rate0_t pko_mem_port_rate0;
    cvmx_pko_mem_port_rate1_t pko_mem_port_rate1;
    uint64_t clock_rate = cvmx_sysinfo_get()->cpu_clock_hz;
    uint64_t tokens_per_bit = clock_rate*16 / bits_s;

    pko_mem_port_rate0.u64 = 0;
    pko_mem_port_rate0.s.pid = port;
    /* Each packet has a 12 bytes of interframe gap, an 8 byte preamble, and a
        4 byte CRC. These are not included in the per word count. Multiply
        by 8 to covert to bits and divide by 256 for limit granularity */
    pko_mem_port_rate0.s.rate_pkt = (12 + 8 + 4) * 8 * tokens_per_bit / 256;
    /* Each 8 byte word has 64bits */
    pko_mem_port_rate0.s.rate_word = 64 * tokens_per_bit;

    pko_mem_port_rate1.u64 = 0;
    pko_mem_port_rate1.s.pid = port;
    pko_mem_port_rate1.s.rate_lim = tokens_per_bit * burst / 256;

    cvmx_write_csr(CVMX_PKO_MEM_PORT_RATE0, pko_mem_port_rate0.u64);
    cvmx_write_csr(CVMX_PKO_MEM_PORT_RATE1, pko_mem_port_rate1.u64);
    return 0;
}

#endif /* CVMX_ENABLE_PKO_FUNCTIONS */

