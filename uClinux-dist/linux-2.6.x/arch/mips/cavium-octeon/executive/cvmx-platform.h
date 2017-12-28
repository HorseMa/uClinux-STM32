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
 * This file is resposible for including all system dependent
 * headers for the cvmx-* files.
 *
*/

#ifndef __CVMX_PLATFORM_H__
#define __CVMX_PLATFORM_H__

#include "cvmx-abi.h"

/* This file defines macros for use in determining the current
    building environment. It defines a single CVMX_BUILD_FOR_*
    macro representing the target of the build. The current
    possibilities are:
	CVMX_BUILD_FOR_UBOOT
	CVMX_BUILD_FOR_LINUX_KERNEL
	CVMX_BUILD_FOR_LINUX_USER
	CVMX_BUILD_FOR_LINUX_HOST
	CVMX_BUILD_FOR_VXWORKS
	CVMX_BUILD_FOR_STANDALONE */
/* We are in the Linux kernel on Octeon */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <stdarg.h>

#endif /* __CVMX_PLATFORM_H__ */
