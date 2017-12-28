/* $Header: /var/cvs/uClinux-2.4.x/drivers/misc/xilinx_hwicap/xhwicap_set_configuration.c,v 1.1 2006/11/09 22:33:52 jwilliams Exp $ */
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
* @file xhwicap_set_configuration.c
*
* This file contains the function that loads a partial bitstream located
* in system memory into the device (ICAP).
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

#define XHI_BUFFER_START 0

/**************************** Type Definitions ******************************/


/***************** Macros (Inline Functions) Definitions ********************/


/************************** Variable Definitions ****************************/


/************************** Function Prototypes *****************************/

/****************************************************************************/
/**
*
* Loads a partial bitstream from system memory.  
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @param    Data - Address of the data representing the partial bitstream
*
* @param    Size - the size of the partial bitstream in 32 bit words.
*
* @return   XST_SUCCESS, XST_BUFFER_TOO_SMALL or XST_INVALID_PARAM.
*
* @note     None.
*
*****************************************************************************/
XStatus XHwIcap_SetConfiguration(XHwIcap *InstancePtr, u32 *Data, 
                                u32 Size)
{
    XStatus Status;
    s32 BufferCount=0;
    s32 NumWrites=0;
    u32 Dirty=FALSE; 
    s32 I;

    /* Loop through all the data */
    for (I=0,BufferCount=0;I<Size;I++) 
    {

        /* Copy data to bram */
        XHwIcap_StorageBufferWrite(InstancePtr, BufferCount, Data[I]);
        Dirty=TRUE;

        if (BufferCount == XHI_MAX_BUFFER_INTS-1) 
        {
            /* Write data to ICAP */
            Status = XHwIcap_DeviceWrite(InstancePtr, XHI_BUFFER_START, 
                                            XHI_MAX_BUFFER_INTS);
            if (Status != XST_SUCCESS) 
            {
                return Status;
            }

            BufferCount=0;
            NumWrites++;
            Dirty=FALSE;
        } else 
        {
         BufferCount++;
        }
    }

   /* Write unwritten data to ICAP */
   if (Dirty==TRUE) 
   {
      /* Write data to ICAP */
      Status = XHwIcap_DeviceWrite(InstancePtr, XHI_BUFFER_START, 
                                    BufferCount+1);
      if (Status == XST_SUCCESS) 
      {
          return Status;
      }
   }
   return XST_SUCCESS;
};

