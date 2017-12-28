/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Cavium Networks
 */
#ifndef __CAVIUM_OCTEON_HAL_READ_WRITE_H__
#define __CAVIUM_OCTEON_HAL_READ_WRITE_H__

#include "cvmx.h"


/**
 * Write a 32bit value to the Octeon NPI register space
 *
 * @param address Address to write to
 * @param val     Value to write
 */
static inline void octeon_npi_write32(uint64_t address, uint32_t val)
{
	cvmx_write64_uint32(address ^ 4, val);
	cvmx_read64_uint32(address ^ 4);
}


/**
 * Read a 32bit value from the Octeon NPI register space
 *
 * @param address Address to read
 * @return The result
 */
static inline uint32_t octeon_npi_read32(uint64_t address)
{
	return cvmx_read64_uint32(address ^ 4);
}

#endif
