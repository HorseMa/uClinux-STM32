/******************************************************************************
*
*     Author: Xilinx, Inc.
*     
*     
*     This program is free software; you can redistribute it and/or modify it
*     under the terms of the GNU General Public License as published by the
*     Free Software Foundation; either version 2 of the License, or (at your
*     option) any later version.
*     
*     
*     XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
*     COURTESY TO YOU. BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
*     ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD,
*     XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE
*     FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING
*     ANY THIRD PARTY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
*     XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
*     THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY
*     WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM
*     CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND
*     FITNESS FOR A PARTICULAR PURPOSE.
*     
*     
*     Xilinx hardware products are not intended for use in life support
*     appliances, devices, or systems. Use in such applications is
*     expressly prohibited.
*     
*     
*     (c) Copyright 2002-2004 Xilinx Inc.
*     All rights reserved.
*     
*     
*     You should have received a copy of the GNU General Public License along
*     with this program; if not, write to the Free Software Foundation, Inc.,
*     675 Mass Ave, Cambridge, MA 02139, USA.
*
******************************************************************************/
/*****************************************************************************/
/*
*
* @file xpacket_fifo_l_v2_00_a.h
*
* This header file contains identifiers and low-level (Level 0) driver
* functions (or macros) that can be used to access the FIFO.  High-level driver
* (Level 1) functions are defined in xpacket_fifo_v2_00_a.h.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- ------------------------------------------------------
* 2.00a rpm  10/22/03  First release. Moved most of Level 1 driver functions
*                      into this layer.
* 2.00a rmm  02/24/04  Added L0WriteDre function.
* </pre>
*
*****************************************************************************/
#ifndef XPACKET_FIFO_L_V200A_H	/* prevent circular inclusions */
#define XPACKET_FIFO_L_V200A_H	/* by using protection macros */

/***************************** Include Files *********************************/

#include "xbasic_types.h"
#include "xstatus.h"

/************************** Constant Definitions *****************************/

/*
 * These constants specify the FIFO type and are mutually exclusive
 */
#define XPF_V200A_READ_FIFO_TYPE      0	/* a read FIFO */
#define XPF_V200A_WRITE_FIFO_TYPE     1	/* a write FIFO */

/*
 * These constants define the offsets to each of the registers from the
 * register base address, each of the constants are a number of bytes
 */
#define XPF_V200A_RESET_REG_OFFSET            0UL
#define XPF_V200A_MODULE_INFO_REG_OFFSET      0UL
#define XPF_V200A_COUNT_STATUS_REG_OFFSET     4UL

/*
 * This constant is used with the Reset Register
 */
#define XPF_V200A_RESET_FIFO_MASK             0x0000000A

/*
 * These constants are used with the Occupancy/Vacancy Count Register. This
 * register also contains FIFO status
 */
#define XPF_V200A_COUNT_MASK                  0x00FFFFFF
#define XPF_V200A_DEADLOCK_MASK               0x20000000
#define XPF_V200A_ALMOST_EMPTY_FULL_MASK      0x40000000
#define XPF_V200A_EMPTY_FULL_MASK             0x80000000
#define XPF_V200A_VACANCY_SCALED_MASK         0x10000000

/*
 * This constant is used to mask the Width field
 */
#define XPF_V200A_FIFO_WIDTH_MASK             0x0E000000

/*
 * These constants are used with the Width field
 */
#define XPF_V200A_FIFO_WIDTH_LEGACY_TYPE      0x00000000
#define XPF_V200A_FIFO_WIDTH_8BITS_TYPE       0x02000000
#define XPF_V200A_FIFO_WIDTH_16BITS_TYPE      0x04000000
#define XPF_V200A_FIFO_WIDTH_32BITS_TYPE      0x06000000
#define XPF_V200A_FIFO_WIDTH_64BITS_TYPE      0x08000000
#define XPF_V200A_FIFO_WIDTH_128BITS_TYPE     0x0A000000
#define XPF_V200A_FIFO_WIDTH_256BITS_TYPE     0x0C000000
#define XPF_V200A_FIFO_WIDTH_512BITS_TYPE     0x0E000000

/* Width of a FIFO word */
#define XPF_V200A_32BIT_FIFO_WIDTH_BYTE_COUNT       4
#define XPF_V200A_64BIT_FIFO_WIDTH_BYTE_COUNT       8

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

XStatus XPacketFifoV200a_L0Read(u32 RegBaseAddress,
				u32 DataBaseAddress,
				u8 * ReadBufferPtr, u32 ByteCount);

XStatus XPacketFifoV200a_L0Write(u32 RegBaseAddress,
				 u32 DataBaseAddress,
				 u8 * WriteBufferPtr, u32 ByteCount);

XStatus XPacketFifoV200a_L0WriteDre(u32 RegBaseAddress,
				    u32 DataBaseAddress,
				    u8 * BufferPtr, u32 ByteCount);

#endif				/* end of protection macro */
