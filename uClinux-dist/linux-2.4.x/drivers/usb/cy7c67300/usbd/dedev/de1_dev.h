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

#ifndef DE1_DEV_H
#define DE1_DEV_H

/** include files **/

#include "../../cy_ioctl.h"


/** local definitions **/
#define DE1_DEVICE_NAME "de1"
#define DRIVER_NAME "DVK1_peripheral"

typedef unsigned short uint16;
typedef unsigned char  uint8;

/* default settings */

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/
#define DVK1_DEVICE_CLASS       0x80
#define DVK1_DEVICE_SUBCLASS    0x01
#define DVK1_DEVICE_PROTOCOL    0

#define DVK1_VENDOR_ID          0x04b4
#define DVK1_PRODUCT_ID         0xde01

#define DVK1_MANUFACTURER_STR   "Cypress Semiconductor"
#define DVK1_PRODUCT_STR        "EZ-HOST"
#define DVK1_SERIAL_NUMBER_STR  "2003.01.16"


/** private data **/
#define CONTROL_EP          0x0
#define BULK_IN_EP          0x1
#define BULK_OUT_EP         0x2
#define INT_IN_EP           0x3
#define INT_OUT_EP          0x4


#define MAX_DEVICE_NUM      16

#define TOTAL_ENDPOINTS     5
#define TWO_K               1024 * 2
#define EP_BUFFER_SIZE      TWO_K
#define MAX_RETRY           3

#define BULK_IN_BUFF_SZ     1024
#define INT_IN_BUFF_SZ      64

#define USB_DE1_MAJOR       0
#define USB_DE1_MINOR_BASE  0
#define MAX_DEVICES         1

/** public functions **/

/** private functions **/


#endif
