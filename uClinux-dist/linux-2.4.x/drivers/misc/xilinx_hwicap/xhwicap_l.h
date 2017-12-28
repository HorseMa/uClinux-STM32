/* $Header: /devl/xcs/repo/env/Databases/ip2/processor/software/devel/hwicap/v1_00_a/src/xhwicap_l.h,v 1.5 2003/12/10 16:52:45 brandonb Exp $ */
/*****************************************************************************
*
*       XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS"
*       AS A COURTESY TO YOU, SOLELY FOR USE IN DEVELOPING PROGRAMS AND
*       SOLUTIONS FOR XILINX DEVICES.  BY PROVIDING THIS DESIGN, CODE,
*       OR INFORMATION AS ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE,
*       APPLICATION OR STANDARD, XILINX IS MAKING NO REPRESENTATION
*       THAT THIS IMPLEMENTATION IS FREE FROM ANY CLAIMS OF INFRINGEMENT,
*       AND YOU ARE RESPONSIBLE FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE
*       FOR YOUR IMPLEMENTATION.  XILINX EXPRESSLY DISCLAIMS ANY
*       WARRANTY WHATSOEVER WITH RESPECT TO THE ADEQUACY OF THE
*       IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OR
*       REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM CLAIMS OF
*       INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*       FOR A PARTICULAR PURPOSE.
*
*       (c) Copyright 2003 Xilinx Inc.
*       All rights reserved.
*
*****************************************************************************/
/****************************************************************************/
/**
*
* @file xhwicap_l.h
*
* This header file contains identifiers and low-level driver functions (or
* macros) that can be used to access the device.  High-level driver functions
* are defined in xhwicap.h.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00a bjb  11/14/03 First release
*
* </pre>
*
*****************************************************************************/

#ifndef XHWICAP_L_H_ /* prevent circular inclusions */
#define XHWICAP_L_H_ /* by using protection macros */

/***************************** Include Files ********************************/

#include "xbasic_types.h"
#include "xio.h"

/************************** Constant Definitions ****************************/

/* XHwIcap register offsets */

#define XHI_SIZE_REG_OFFSET        0x800L  /* Size of transfer, read & write */
#define XHI_BRAM_OFFSET_REG_OFFSET 0x804L  /* Offset into bram, read & write */
#define XHI_RNC_REG_OFFSET         0x808L  /* Read not Configure, direction of
                                                transfer.  Write only */
#define XHI_DONE_REG_OFFSET        0x80CL  /* Indicates transfer complete. Read
                                                only */

/* Constants for setting the RNC register */
#define XHI_CONFIGURE              0x0UL
#define XHI_READBACK               0x1UL

/* Constants for the Done register */
#define XHI_NOT_FINISHED           0x0UL
#define XHI_FINISHED               0x1UL

/**************************** Type Definitions ******************************/

/***************** Macros (Inline Functions) Definitions ********************/

/****************************************************************************/
/**
*
* Get the contents of the size register. 
*
* The size register holds the number of 32 bit words to transfer between
* bram and the icap (or icap to bram).
*
* @param    BaseAddress is the  base address of the device
*
* @return   A 32-bit value representing the contents of the size
* register.
*
* @note    
*
* u32 XHwIcap_mGetSizeReg(u32 BaseAddress);
*
*****************************************************************************/
#define XHwIcap_mGetSizeReg(BaseAddress) \
    ( XIo_In32((BaseAddress) + XHI_SIZE_REG_OFFSET) )

/****************************************************************************/
/**
*
* Get the contents of the bram offset register. 
*
* The bram offset register holds the starting bram address to transfer
* data from during configuration or write data to during readback.
*
* @param    BaseAddress is the  base address of the device
*
* @return   A 32-bit value representing the contents of the bram offset
* register.
*
* @note    
*
* u32 XHwIcap_mGetOffsetReg(u32 BaseAddress);
*
*****************************************************************************/
#define XHwIcap_mGetOffsetReg(BaseAddress) \
    ( XIo_In32((BaseAddress + XHI_BRAM_OFFSET_REG_OFFSET)) )

/****************************************************************************/
/**
*
* Get the contents of the done register. 
*
* The done register is set to zero during configuration or readback.
* When the current configuration or readback completes the done register
* is set to one.
*
* @param    BaseAddress is the  base address of the device
*
* @return   A 32-bit value representing the contents of the bram offset
* register.
*
* @note
*
* u32 XHwIcap_mGetDoneReg(u32 BaseAddress);
*
*****************************************************************************/

#define XHwIcap_mGetDoneReg(BaseAddress) \
    ( XIo_In32((BaseAddress + XHI_DONE_REG_OFFSET)) )

/****************************************************************************/
/**
* Reads data from the storage buffer bram.
*
* A bram is used as a configuration memory cache.  One frame of data can
* be stored in this "storage buffer".
*
* @param    BaseAddress - contains the base address of the component.
*
* @param    Offset - The offset into which the data should be read.
*
* @return   The value of the specified offset in the bram.
*
* @note
*
* u32 XHwIcap_mGetBram(u32 BaseAddress, u32 Offset);
*
*****************************************************************************/
#define XHwIcap_mGetBram(BaseAddress, Offset) \
    ( XIo_In32((BaseAddress+(Offset<<2))) )



/****************************************************************************/
/**
* Set the size register.
*
* The size register holds the number of 8 bit bytes to transfer between
* bram and the icap (or icap to bram).
*
* @param    BaseAddress - contains the base address of the device.
*
* @param    Data - The size in bytes.
*
* @return   None.
*
* @note
*
* void XHwIcap_mSetSizeReg(u32 BaseAddress, u32 Data);
*
*****************************************************************************/
#define XHwIcap_mSetSizeReg(BaseAddress, Data) \
    ( XIo_Out32((BaseAddress) + XHI_SIZE_REG_OFFSET, (Data)) )

/****************************************************************************/
/**
* Set the bram offset register.
*
* The bram offset register holds the starting bram address to transfer
* data from during configuration or write data to during readback.
*
* @param    BaseAddress contains the base address of the device.
* 
* @param    Data is the value to be written to the data register.
*
* @return   None.
*
* @note 
*
* void XHwIcap_mSetOffsetReg(u32 BaseAddress, u32 Data);
*
*****************************************************************************/
#define XHwIcap_mSetOffsetReg(BaseAddress, Data) \
    ( XIo_Out32((BaseAddress) + XHI_BRAM_OFFSET_REG_OFFSET, (Data)) )

/****************************************************************************/
/**
* Set the RNC (Readback not Configure) register.
*
* The RNC register determines the direction of the data transfer.  It
* controls whether a configuration or readback take place.  Writing to
* this register initiates the transfer.  A value of 1 initiates a
* readback while writing a value of 0 initiates a configuration.
*
* @param    BaseAddress contains the base address of the device.
*
* @param    Data is the value to be written to the data register.
*
* @return   None.
*
* @note
*
* void XHwIcap_mSetRncReg(u32 BaseAddress, u32 Data);
*
*****************************************************************************/
#define XHwIcap_mSetRncReg(BaseAddress, Data) \
    ( XIo_Out32((BaseAddress) + XHI_RNC_REG_OFFSET, (Data)) )

/****************************************************************************/
/**
* Write data to the storage buffer bram.
*
* A bram is used as a configuration memory cache.  One frame of data can
* be stored in this "storage buffer".
*
* @param    BaseAddress - contains the base address of the component.
*
* @param    Offset - The offset into which the data should be written.
*
* @param    Data - The value to be written to the bram offset.
*
* @return   None.
*
* @note
*
* void XHwIcap_mSetBram(u32 BaseAddress, u32 Offset, u32 Data);
*
*****************************************************************************/
#define XHwIcap_mSetBram(BaseAddress, Offset, Data) \
    ( XIo_Out32((BaseAddress+(Offset<<2)), (Data)) )


#endif         /* end of protection macro */
