/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005-2007 Cavium Networks
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include "cvmx.h"
#include "cvmx-bootmem.h"
#include "cvmx-cmd-queue.h"
#include "cvmx-helper.h"
#include "cvmx-helper-board.h"
#include "cvmx-helper.h"
#include "cvmx-pko.h"
#include "cvmx-spi.h"
#include "cvmx-sysinfo.h"
#include "cvmx-warn.h"

extern CVMX_SHARED __cvmx_cmd_queue_all_state_t *__cvmx_cmd_queue_state_ptr;

/* Exports for cvmx-bootmem.c */
EXPORT_SYMBOL(cvmx_bootmem_alloc);
EXPORT_SYMBOL(cvmx_bootmem_alloc_address);
EXPORT_SYMBOL(cvmx_bootmem_alloc_range);
EXPORT_SYMBOL(cvmx_bootmem_alloc_named);
EXPORT_SYMBOL(cvmx_bootmem_alloc_named_address);
EXPORT_SYMBOL(cvmx_bootmem_alloc_named_range);
EXPORT_SYMBOL(cvmx_bootmem_free_named);
EXPORT_SYMBOL(cvmx_bootmem_find_named_block);
EXPORT_SYMBOL(cvmx_bootmem_available_mem);

/* Exports for cvmx-cmd-queue.c */
EXPORT_SYMBOL(cvmx_cmd_queue_initialize);
EXPORT_SYMBOL(cvmx_cmd_queue_shutdown);
EXPORT_SYMBOL(cvmx_cmd_queue_length);
EXPORT_SYMBOL(cvmx_cmd_queue_buffer);
EXPORT_SYMBOL(__cvmx_cmd_queue_state_ptr);

/* Exports for cvmx-helper.c */
EXPORT_SYMBOL(cvmx_helper_ipd_and_packet_input_enable);
EXPORT_SYMBOL(cvmx_helper_initialize_packet_io_global);
EXPORT_SYMBOL(cvmx_helper_initialize_packet_io_local);
EXPORT_SYMBOL(cvmx_helper_ports_on_interface);
EXPORT_SYMBOL(cvmx_helper_get_number_of_interfaces);
EXPORT_SYMBOL(cvmx_helper_interface_get_mode);
EXPORT_SYMBOL(cvmx_helper_link_autoconf);
EXPORT_SYMBOL(cvmx_helper_link_get);
EXPORT_SYMBOL(cvmx_helper_link_set);

/* Exports for cvmx-helper-board.c */
EXPORT_SYMBOL(cvmx_helper_board_get_mii_address);

/* Exports for cvmx-helper-util.c */
EXPORT_SYMBOL(cvmx_helper_interface_mode_to_string);
EXPORT_SYMBOL(cvmx_helper_dump_packet);
EXPORT_SYMBOL(cvmx_helper_setup_red_queue);
EXPORT_SYMBOL(cvmx_helper_setup_red);
EXPORT_SYMBOL(cvmx_helper_get_version);
EXPORT_SYMBOL(cvmx_helper_get_ipd_port);
EXPORT_SYMBOL(cvmx_helper_get_interface_num);
EXPORT_SYMBOL(cvmx_helper_get_interface_index_num);

/* Exports for cvmx-pko.c */
EXPORT_SYMBOL(cvmx_pko_initialize_global);
EXPORT_SYMBOL(cvmx_pko_initialize_local);
EXPORT_SYMBOL(cvmx_pko_enable);
EXPORT_SYMBOL(cvmx_pko_disable);
EXPORT_SYMBOL(cvmx_pko_shutdown);
EXPORT_SYMBOL(cvmx_pko_config_port);

/* Exports for cvmx-spi.c and cvmx-spi4000.c */
EXPORT_SYMBOL(cvmx_spi_restart_interface);
EXPORT_SYMBOL(cvmx_spi4000_check_speed);

/* Exports for cvmx-sysinfo.c */
EXPORT_SYMBOL(cvmx_sysinfo_get);

/* Exports for cvmx-warn.c */
EXPORT_SYMBOL(cvmx_warn);
