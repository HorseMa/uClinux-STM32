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
 * Functions for warning users about errors and such.
 *
 * <hr>$Revision: 1.1 $<hr>
 *
 */
#ifndef __CVMX_WARN_H__
#define __CVMX_WARN_H__

#ifdef printf
extern void cvmx_warn(const char *format, ...);
#else
extern void cvmx_warn(const char *format, ...)
    __attribute__ ((format(printf, 1, 2)));
#endif

#define cvmx_warn_if(expression, format, ...) if (expression) cvmx_warn(format, ##__VA_ARGS__)

#endif /* __CVMX_WARN_H__ */
