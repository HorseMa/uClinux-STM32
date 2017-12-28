/***********************license start***************
 * Author: Cavium Networks 
 * 
 * Contact: support@caviumnetworks.com 
 * This file is part of the OCTEON SDK
 * 
 * Copyright (c) 2003-2008 Cavium Networks 
 * 
 * This file is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License, Version 2, as published by 
 * the Free Software Foundation. 
 * 
 * This file is distributed in the hope that it will be useful, 
 * but AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or NONINFRINGEMENT. 
 * See the GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this file; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/. 
 * 
 * This file may also be available under a different license from Cavium. 
 * Contact Cavium Networks for more information
 ***********************license end**************************************/

/**
 * @file
 *
 * This file defines macros for use in determining the current calling ABI.
 *
 * <hr>$Revision: 1.1 $<hr>
*/

#ifndef __CVMX_ABI_H__
#define __CVMX_ABI_H__

/* Check for N32 ABI, defined for 32-bit Simple Exec applications
   and Linux N32 ABI.*/
#if (defined _ABIN32 && _MIPS_SIM == _ABIN32)
#define CVMX_ABI_N32
/* Check for N64 ABI, defined for 64-bit Linux toolchain. */
#elif (defined _ABI64 && _MIPS_SIM == _ABI64)
#define CVMX_ABI_N64
/* Check for O32 ABI, defined for Linux 032 ABI, not supported yet. */
#elif (defined _ABIO32 && _MIPS_SIM == _ABIO32)
#define CVMX_ABI_O32
/* Check for EABI ABI, defined for 64-bit Simple Exec applications. */
#else
#define CVMX_ABI_EABI
#endif

#ifndef __BYTE_ORDER
#if defined(__BIG_ENDIAN) && !defined(__LITTLE_ENDIAN)
#define __BYTE_ORDER __BIG_ENDIAN
#elif !defined(__BIG_ENDIAN) && defined(__LITTLE_ENDIAN)
#define __BYTE_ORDER __LITTLE_ENDIAN
#elif !defined(__BIG_ENDIAN) && !defined(__LITTLE_ENDIAN)
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __BIG_ENDIAN
#else
#error Unable to determine Endian mode
#endif
#endif

#endif /* __CVMX_ABI_H__ */
