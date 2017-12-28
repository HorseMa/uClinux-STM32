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

#ifndef __DE3_DRV_H_
#define __DE3_DRV_H_

#include "../cy_ioctl.h"


#define CONTROL_EP          	0x0
#define BULK_IN_EP          	0x1
#define BULK_OUT_EP         	0x2
#define BULK_OUT2_EP        	0x3

#define MAX_DEVICES		  		16
#define MAX_BULK_IN_EP     		4
#define MAX_BULK_OUT_EP    		4
#define MAX_ISOC_IN_EP     		4
#define MAX_ISOC_OUT_EP    		4
#define MAX_INT_IN_EP      		4
#define MAX_INT_IN_EP      		4

#define MAX_INT_BUF        		5
#define MAX_BULK_IN_SIZE  		64
#define MAX_BULK_OUT_SIZE 		64	


#define DE3_DEVICE_CLASS		0x0
#define DE3_DEVICE_SUBCLASS		0x0
#define DE3_DEVICE_PROTOCOL		0x0
#define DE3_VENDOR_ID			0x04B4
#define DE3_PRODUCT_ID			0xDE04
#define DE3_MANUFACTURER_STR	"Cypress Semiconductor"
#define DE3_PRODUCT_STR			"Design Example 3"
#define DE3_SERIAL_NUMBER_STR	"123456789"

#define HID_REPORT_SIZE			0x8
#define HID_REPORT_VALID	 	0x1
#define HID_REPORT_INVALID	 	0x0

/* BOOT PROTOCOL KEYBOARD HID REPORT STRUCT */
typedef struct hid_keyboard_report_struct
{
	int valid;
	unsigned char report[HID_REPORT_SIZE];

} hid_keyboard_report_t;

#endif /* __DE3_DRV_H_ */

