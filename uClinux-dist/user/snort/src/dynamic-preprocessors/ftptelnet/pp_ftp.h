/*
 * pp_ftp.h
 *
 * Copyright (C) 2004 Sourcefire,Inc
 * Steven A. Sturges <ssturges@sourcefire.com>
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
 * Header file for FTPTelnet FTP Module
 *  
 * This file defines the ftp checking functions
 *  
 * NOTES:
 *  - 20.09.04:  Initial Development.  SAS
 *
 */
#ifndef __PP_FTP_H__
#define __PP_FTP_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

//#include "decode.h"
#include "ftpp_ui_config.h"
#include "ftpp_si.h"

/* list of function prototypes for this preprocessor */
extern int check_ftp(FTP_SESSION  *Session, SFSnortPacket *p, int iMode);

extern int initialize_ftp(FTP_SESSION *Session, SFSnortPacket *p, int iMode);

#endif 
