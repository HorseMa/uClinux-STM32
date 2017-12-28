/*
 * ftpp_include.h
 *
 * Copyright (C) 2004 Sourcefire,Inc
 * Steven A. Sturges <ssturges@sourcefire.com>
 * Daniel J. Roelker <droelker@sourcefire.com>
 * Marc A. Norton <mnorton@sourcefire.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 *
 * Global definitions for the FTPTelnet preprocessor.
 *
 * NOTES:
 * - 16.09.04:  Initial Development.  SAS
 *
 */
#ifndef __FTP_INCLUDE_H__
#define __FTP_INCLUDE_H__

#ifndef INLINE

#ifdef WIN32
#define INLINE __inline
#else
#define INLINE inline
#endif

#endif /* endif for INLINE */

#include "sf_snort_packet.h"
#include "sf_dynamic_preprocessor.h"

#define GENERATOR_SPP_FTPP_FTP                     125
#define GENERATOR_SPP_FTPP_TELNET                  126
extern DynamicPreprocessorData _dpd;

#endif
