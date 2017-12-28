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

/*******************************************************************
 *
 *    Description: Function Driver header file for Design Example 
 *    Three.
 *
 *******************************************************************/


#ifndef DE3_DEV_H
#define DE3_DEV_H

#include "../../cy_ioctl.h"


typedef unsigned short uint16;
typedef unsigned char  uint8;

#define DE3_DEVICE_NAME           "de3"
#define DRIVER_NAME               "DE3 Function Driver"

#define DE3_DEVICE_CLASS          0x00
#define DE3_DEVICE_SUBCLASS       0x00
#define DE3_DEVICE_PROTOCOL       0x00

#define DE3_VENDOR_ID             0x04b4
#define DE3_PRODUCT_ID            0xDE03

#define DE3_MANUFACTURER_STR   "Cypress Semiconductor"
#define DE3_PRODUCT_STR        "Cypress Keyboard"
#define DE3_SERIAL_NUMBER_STR  "2003.01.16"


#define CONTROL_EP               0x0
#define INT_IN_EP                0x1
#define TOTAL_ENDPOINTS          2

#define USB_DE3_MINOR_BASE       0
#define MAX_DEVICES              1


/* BOOT PROTOCOL KEYBOARD HID REPORT STRUCT */

#define HID_REPORT_SIZE          0x8

typedef struct hid_keyboard_report_struct
{
    int valid;
    unsigned char report[HID_REPORT_SIZE];

} hid_keyboard_report_t;


#endif // DE3_DEV_H

