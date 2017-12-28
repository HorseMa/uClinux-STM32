/* $Header: /devl/xcs/repo/env/Databases/ip2/processor/software/devel/hwicap/v1_00_a/src/xhwicap_device_write_frame.c,v 1.4 2003/12/10 00:03:53 brandonb Exp $ */
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
* @file xhwicap_device_write_frame.c
*
* This file contains the function that writes the frame stored in the
* bram storage buffer to the device (ICAP).
*
* @note none.
*
* <pre> 
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes 
* ----- ---- -------- ------------------------------------------------------- 
* 1.00a bjb  11/20/03 First release
*
* </pre>
*
*****************************************************************************/

/***************************** Include Files ********************************/

#include "xhwicap.h"
#include "xhwicap_i.h"
#include <xbasic_types.h>
#include <xstatus.h>

/************************** Constant Definitions ****************************/


/**************************** Type Definitions ******************************/


/***************** Macros (Inline Functions) Definitions ********************/


/************************** Variable Definitions ****************************/


/************************** Function Prototypes *****************************/


/****************************************************************************/
/**
*
* Writes one frame from the storage buffer and puts it in the device
* (ICAP).
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @param    Block - Block Address (XHI_FAR_CLB_BLOCK,
*           XHI_FAR_BRAM_BLOCK, XHI_FAR_BRAM_INT_BLOCK)
*
* @param    MajorFrame - selects the column
*
* @param    MinorFrame - selects frame inside column
*
* @return   XST_SUCCESS, XST_BUFFER_TOO_SMALL or XST_INVALID_PARAM.
*
* @note     None.
*
*****************************************************************************/
XStatus XHwIcap_DeviceWriteFrame(XHwIcap *InstancePtr, s32 Block, 
                                 s32 MajorFrame, s32 MinorFrame) 
{

    s32 HeaderBytes;
    s32 Packet; 
    s32 Data; 
    s32 TotalWords; 
    XStatus Status;


    /* Make sure we aren't trying to write more than what we have room
     * for. */
    if (InstancePtr->BytesPerFrame > 
            (XHI_MAX_BUFFER_BYTES-XHI_HEADER_BUFFER_BYTES)) 
    {
        return XST_BUFFER_TOO_SMALL;
    }

    /* DUMMY and SYNC */
    XHwIcap_StorageBufferWrite(InstancePtr, 0, XHI_DUMMY_PACKET);
    XHwIcap_StorageBufferWrite(InstancePtr, 1, XHI_SYNC_PACKET);

    /* Reset CRC */
    Packet = XHwIcap_Type1Write(XHI_CMD) | 1;
    Data = XHI_CMD_RCRC;
    XHwIcap_StorageBufferWrite(InstancePtr, 2,Packet);
    XHwIcap_StorageBufferWrite(InstancePtr, 3,Data);

    /* ID register */
    Packet = XHwIcap_Type1Write(XHI_IDCODE) | 1;
    Data = InstancePtr->DeviceIdCode;
    XHwIcap_StorageBufferWrite(InstancePtr, 4,Packet);
    XHwIcap_StorageBufferWrite(InstancePtr, 5,Data);

    /* Bypass CRC */
    Packet = XHwIcap_Type1Write(XHI_COR) | 1;
    Data = 0x20053FE5;
    XHwIcap_StorageBufferWrite(InstancePtr, 6,Packet);
    XHwIcap_StorageBufferWrite(InstancePtr, 7,Data);

    /* Setup CMD register */
    Packet = XHwIcap_Type1Write(XHI_CMD) | 1;
    Data = XHI_CMD_WCFG;
    XHwIcap_StorageBufferWrite(InstancePtr, 8,Packet);
    XHwIcap_StorageBufferWrite(InstancePtr, 9,Data);

    /* Setup FAR */
    Packet = XHwIcap_Type1Write(XHI_FAR) | 1;
    Data = XHwIcap_SetupFar(Block, MajorFrame, MinorFrame);
    XHwIcap_StorageBufferWrite(InstancePtr, 10,Packet);
    XHwIcap_StorageBufferWrite(InstancePtr, 11,Data);

    /* Setup Packet header. */
    TotalWords = InstancePtr->WordsPerFrame << 1; /* mult by 2 */
    if (TotalWords < XHI_TYPE_1_PACKET_MAX_WORDS) 
    {
        /* Create Type 1 Packet. */
        Packet = XHwIcap_Type1Write(XHI_FDRI) | TotalWords;
        XHwIcap_StorageBufferWrite(InstancePtr, 12,Packet);

        HeaderBytes = 52;  /* 13*4 */
    }
    else 
    {
        /* Create Type 2 Packet. */
        Packet = XHwIcap_Type1Write(XHI_FDRI);
        XHwIcap_StorageBufferWrite(InstancePtr, 12,Packet);

        Packet = XHI_TYPE_2_WRITE | TotalWords;
        XHwIcap_StorageBufferWrite(InstancePtr, 13,Packet);

        HeaderBytes = 56;  /* 14*4 */
    }

    /* append auto CRC word. */
    XHwIcap_StorageBufferWrite(InstancePtr, (XHI_HEADER_BUFFER_WORDS+
                InstancePtr->WordsPerFrame), XHI_DISABLED_AUTO_CRC);

    /* Write out Header. Div by 4 for words*/
    Status = XHwIcap_DeviceWrite(InstancePtr, 0, (HeaderBytes>>2)); 
    if (Status != XST_SUCCESS) 
    {
        return Status;
    }

    /* Write out data. */
    Status = XHwIcap_DeviceWrite(InstancePtr, XHI_HEADER_BUFFER_WORDS, 
                                    InstancePtr->WordsPerFrame);
    if (Status != XST_SUCCESS) 
    {
        return Status;
    }

    /* Write out pad frame. */
    Status = XHwIcap_DeviceWrite(InstancePtr, XHI_HEADER_BUFFER_WORDS, 
                                    InstancePtr->WordsPerFrame + 1);
    if (Status != XST_SUCCESS) 
    {
        return Status;
    }

    /* send DESYNC command */
    Status = XHwIcap_CommandDesync(InstancePtr);
    if (Status != XST_SUCCESS) 
    {
        return Status;
    }

   return XST_SUCCESS;
};

