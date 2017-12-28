/*
 * ftp_server.h
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
 * Header file for FTPTelnet FTP Server Module
 * 
 * This file defines the server structure and functions to access server
 * inspection.
 * 
 * NOTES:
 * - 16.09.04:  Initial Development.  SAS
 *
 */
#ifndef __FTP_SERVER_H__
#define __FTP_SERVER_H__

#include "ftpp_include.h"

typedef struct s_FTP_SERVER_RSP
{
    char *rsp_line;
    unsigned int  rsp_line_size;

    char *rsp_begin;
    char *rsp_end;
    unsigned int  rsp_size;

    char *msg_begin;
    char *msg_end;
    unsigned int msg_size;

    char *pipeline_req;
    int state;
} FTP_SERVER_RSP;

typedef struct s_FTP_SERVER
{
    FTP_SERVER_RSP response;
} FTP_SERVER;

int ftp_server_inspection(void *S, unsigned char *data, int dsize);

#endif
