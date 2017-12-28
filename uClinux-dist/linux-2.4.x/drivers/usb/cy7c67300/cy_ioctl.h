/*******************************************************************************
 *
 * Copyright (c) 2003 Cypress Semiconductor
 *
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
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef CY_IOCTL
#define CY_IOCTL


/******************************************************************************
 * IOCTL commands specific to Cypress Design Examples 1, 3 and 4 for EZ-HOST
 ******************************************************************************/

typedef enum ioctl_cmds
{
    IOCTL_OTG_STATE,
    IOCTL_OTG_DEBUG,
    IOCTL_ENABLE_HNP,
    IOCTL_ACCEPT_HNP,
    IOCTL_END_HNP,
    IOCTL_REQUEST_SRP,
    IOCTL_END_SESSION,
    IOCTL_GET_DE_RPT,
    IOCTL_SEND_DE_RPT,
    IOCTL_GET_HID_RPT,
    IOCTL_SEND_HID_RPT,
    IOCTL_IS_CONNECTED,
	IOCTL_UNSUPPORTED_DEVICE,
	IOCTL_DEVICE_NOT_RESPOND,
} ioctl_cmds;


#endif

