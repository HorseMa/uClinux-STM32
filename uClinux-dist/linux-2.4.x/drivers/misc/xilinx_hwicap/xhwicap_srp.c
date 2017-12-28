/* $Header: /var/cvs/uClinux-2.4.x/drivers/misc/xilinx_hwicap/xhwicap_srp.c,v 1.2 2006/11/12 22:18:52 jwilliams Exp $ */
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
* @file xhwicap_srp.c
*
* This file contains SRP (self reconfigurable platform) driver
* functions.
*
* The SRP contains functions that allow low level access to
* configuration memory through the ICAP port.  This API provide methods
* for reading and writing data, frames, and partial bitstreams to the
* ICAP port.
*
* @note 
*
* Only Virtex 2 and Virtex 2 Pro devices are supported as they are the
* only devices that contain the VIRTEX2_ICAP component.
*
*
* <pre> 
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes 
* ----- ---- -------- ------------------------------------------------------- 
* 1.00a bjb  11/17/03 First release
*
* </pre>
*
*****************************************************************************/

/***************************** Include Files ********************************/

#include <linux/autoconf.h>
#include <xbasic_types.h>
#include <xstatus.h>
#include "xhwicap_i.h"
#include "xhwicap.h"

/************************** Constant Definitions ****************************/


/**************************** Type Definitions ******************************/


/***************** Macros (Inline Functions) Definitions ********************/


/************************** Variable Definitions ****************************/


/************************** Function Prototypes *****************************/


/****************************************************************************/
/**
*
* Initialize a XHwIcap instance..
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @param    BaseAddress - Base Address of the instance of this
*                         component.
*
* @param    DeviceId - User defined ID for the instance of this
*                      component.
*
* @param    DeviceIdCode - IDCODE of the FPGA device.  
*
* @return   XST_SUCCESS or XST_INVALID_PARAM.
*
* @note     Virtex2/Pro devices only have one ICAP port so there should
*           only be one opb_hwicap instantiated (per FPGA) in a system.
*
*****************************************************************************/
XStatus XHwIcap_Initialize(XHwIcap *InstancePtr, u16 DeviceId,
        u32 DeviceIdCode) 
{
    XHwIcap_Config *HwIcapConfigPtr;
    u32 Rows;
    u32 Cols;
    u32 BramCols;
    /*
     * Assert validates the input arguments
     */
    XASSERT_NONVOID(InstancePtr != NULL);

    /*
     * If the device is ready, disallow the initialize and return a status
     * indicating it is started.  This allows the user to stop the device
     * and reinitialize, but prevents a user from inadvertently initializing.
     */

    if (InstancePtr->IsReady == XCOMPONENT_IS_READY)
    {
        return XST_DEVICE_IS_STARTED;
    }

    /* Default value until component is ready */
    InstancePtr->IsReady = 0;

    /*
     * Lookup the device configuration in the configuration table. Use this
     * configuration info when initializing this component.
     */
    HwIcapConfigPtr = XHwIcap_LookupConfig(DeviceId);

    if (HwIcapConfigPtr == (XHwIcap_Config *)NULL)
    {
        return XST_DEVICE_NOT_FOUND;
    }
    

    switch (DeviceIdCode) 
    {
        case XHI_XC2V40:
            Rows = 8;
            Cols = 8;
            BramCols = 2;
            break;
        case XHI_XC2V80:
            Rows = 16;
            Cols = 8;
            BramCols = 2;
            break;
        case XHI_XC2V250:
            Rows = 24;
            Cols = 16;
            BramCols = 4;
            break;
        case XHI_XC2V500:
            Rows = 32;
            Cols = 24;
            BramCols = 4;
            break;
        case XHI_XC2V1000:
            Rows = 40;
            Cols = 32;
            BramCols = 4;
            break;
        case XHI_XC2V1500:
            Rows = 48;
            Cols = 40;
            BramCols = 4;
            break;
        case XHI_XC2V2000:
            Rows = 56;
            Cols = 48;
            BramCols = 4;
            break;
        case XHI_XC2V3000:
            Rows = 64;
            Cols = 56;
            BramCols = 6;
            break;
        case XHI_XC2V4000:
            Rows = 80;
            Cols = 72;
            BramCols = 6;
            break;
        case XHI_XC2V6000:
            Rows = 96;
            Cols = 88;
            BramCols = 6;
            break;
        case XHI_XC2V8000:
            Rows = 112;
            Cols = 104;
            BramCols = 6;
            break;
        case XHI_XC2VP2:
            Rows = 16;
            Cols = 22;
            BramCols = 4;
            break;
        case XHI_XC2VP4:
            Rows = 40;
            Cols = 22;
            BramCols = 4;
            break;
        case XHI_XC2VP7:
            Rows = 40;
            Cols = 34;
            BramCols = 6;
            break;
        case XHI_XC2VP20:
            Rows = 56;
            Cols = 46;
            BramCols = 8;
            break;
        case XHI_XC2VP30:
            Rows = 80;
            Cols = 46;
            BramCols = 8;
            break;
        case XHI_XC2VP40:
            Rows = 88;
            Cols = 58;
            BramCols = 10;
            break;
        case XHI_XC2VP50:
            Rows = 88;
            Cols = 70;
            BramCols = 12;
            break;
        case XHI_XC2VP70:
            Rows = 104;
            Cols = 82;
            BramCols = 14;
            break;
        case XHI_XC2VP100:
            Rows = 120;
            Cols = 94;
            BramCols = 16;
            break;
        case XHI_XC2VP125:
            Rows = 136;
            Cols = 106;
            BramCols = 18;
            break;
        default :
            return XST_INVALID_PARAM;
            break;
    }

    InstancePtr->BaseAddress = HwIcapConfigPtr->BaseAddress;
    InstancePtr->DeviceId = DeviceId;
    InstancePtr->DeviceIdCode = DeviceIdCode;

    InstancePtr->Rows = Rows;
    InstancePtr->Cols = Cols;
    InstancePtr->BramCols = BramCols;

    InstancePtr->BytesPerFrame = ((96*2+80*Rows)/8);
    InstancePtr->WordsPerFrame = (InstancePtr->BytesPerFrame/4);
    InstancePtr->ClbBlockFrames = (4 +22*2 + 4*2 + 22*Cols);
    InstancePtr->BramBlockFrames = (64*BramCols);
    InstancePtr->BramIntBlockFrames = (22*BramCols);

    InstancePtr->IsReady = XCOMPONENT_IS_READY;

    return XST_SUCCESS;
} /* end XHwIcap_Initialize() */

/****************************************************************************/
/**
*
* Stores data in the storage buffer at the specified address.
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @param    Address - bram word address
*
* @param    Data - data to be stored at address
*
* @return   None.
*
* @note     None.
*
*****************************************************************************/
void XHwIcap_StorageBufferWrite(XHwIcap *InstancePtr, u32 Address,
                                u32 Data)
{
    /*
     * Assert validates the input arguments
     */
    XASSERT_VOID(InstancePtr != NULL);
    XASSERT_VOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    /* Check range of address. */
    XASSERT_VOID(Address<XHI_MAX_BUFFER_INTS);

    /* Write data to storage buffer. */
    XHwIcap_mSetBram(InstancePtr->BaseAddress, Address, Data);
        
}

/****************************************************************************/
/**
*
* Read data from the specified address in the storage buffer..
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @param    Address - bram word address
*
* @return   Data.
*
* @note     None.
*
*****************************************************************************/
u32 XHwIcap_StorageBufferRead(XHwIcap *InstancePtr, u32 Address) 
{
    /*
     * Assert validates the input arguments
     */
    XASSERT_NONVOID(InstancePtr != NULL);
    XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    /* Check range of address. */
    XASSERT_NONVOID(Address<XHI_MAX_BUFFER_INTS);

    /* Read data from address. Multiply Address by 4 since 4 bytes per
     * word.*/
    return XHwIcap_mGetBram(InstancePtr->BaseAddress, Address);
}

/****************************************************************************/
/**
*
* Reads bytes from the device (ICAP) and puts it in the storage buffer.
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @param    Offset - The storage buffer start address.
*
* @param    NumInts - The number of words (32 bit) to read from the
*           device (ICAP).
*
*@return    XStatus - XST_SUCCESS or XST_DEVICE_BUSY or XST_INVALID_PARAM
*
* @note     None.
*
*****************************************************************************/
XStatus XHwIcap_DeviceRead(XHwIcap *InstancePtr, u32 Offset, 
                           u32 NumInts) 

{

    s32 Retries = 0;

    /*
     * Assert validates the input arguments
     */
    XASSERT_NONVOID(InstancePtr != NULL);
    XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    /* Check range of address. */
    XASSERT_NONVOID((Offset+NumInts)<XHI_MAX_BUFFER_INTS);

    if ((Offset+NumInts)<XHI_MAX_BUFFER_INTS)
    {
        /* setSize NumInts*4 to get bytes. */ 
        XHwIcap_mSetSizeReg((InstancePtr->BaseAddress),(NumInts<<2));  
        XHwIcap_mSetOffsetReg((InstancePtr->BaseAddress), Offset);
        XHwIcap_mSetRncReg((InstancePtr->BaseAddress), XHI_READBACK);

        while (XHwIcap_mGetDoneReg(InstancePtr->BaseAddress)==XHI_NOT_FINISHED) 
        {
            Retries++;
            if (Retries > XHI_MAX_RETRIES) 
            {
                return XST_DEVICE_BUSY;
            }
        }
    } else 
    {
        return XST_INVALID_PARAM;
    }
    return XST_SUCCESS;

};


/****************************************************************************/
/**
*
* Writes bytes from the storage buffer and puts it in the device (ICAP).
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @param    Offset - The storage buffer start address.
*
* @param    NumInts - The number of words (32 bit) to read from the
*           device (ICAP).
*
*@return    XStatus - XST_SUCCESS or XST_DEVICE_BUSY or XST_INVALID_PARAM
*
* @note     None.
*
*****************************************************************************/
XStatus XHwIcap_DeviceWrite(XHwIcap *InstancePtr, u32 Offset, 
                            u32 NumInts) 
{

    s32 Retries = 0;

    /*
     * Assert validates the input arguments
     */
    XASSERT_NONVOID(InstancePtr != NULL);
    XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    /* Check range of address. */
    XASSERT_NONVOID((Offset+NumInts)<XHI_MAX_BUFFER_INTS);

    if ((Offset+NumInts)<XHI_MAX_BUFFER_INTS)
    {
        XHwIcap_mSetSizeReg((InstancePtr->BaseAddress),NumInts<<2);  
        XHwIcap_mSetOffsetReg((InstancePtr->BaseAddress), Offset);
        XHwIcap_mSetRncReg((InstancePtr->BaseAddress), XHI_CONFIGURE);

        while (XHwIcap_mGetDoneReg(InstancePtr->BaseAddress)==XHI_NOT_FINISHED) 
        {
            Retries++;
            if (Retries > XHI_MAX_RETRIES) 
            {
                return XST_DEVICE_BUSY;
            }
        }
    } else 
    {
        return XST_INVALID_PARAM;
    }
    return XST_SUCCESS;

};

XStatus XHwIcap_DeviceWriteBytes(XHwIcap *InstancePtr, u32 Offset, 
                            u32 NumBytes) 
{

    s32 Retries = 0;

    /*
     * Assert validates the input arguments
     */
    XASSERT_NONVOID(InstancePtr != NULL);
    XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    /* Check range of address. */
    XASSERT_NONVOID((Offset+NumBytes)<XHI_MAX_BUFFER_BYTES);

    if ((Offset+NumBytes)<XHI_MAX_BUFFER_BYTES)
    {
        XHwIcap_mSetSizeReg((InstancePtr->BaseAddress), NumBytes);
        XHwIcap_mSetOffsetReg((InstancePtr->BaseAddress), Offset);
        XHwIcap_mSetRncReg((InstancePtr->BaseAddress), XHI_CONFIGURE);

        while (XHwIcap_mGetDoneReg(InstancePtr->BaseAddress)==XHI_NOT_FINISHED) 
        {
            Retries++;
            if (Retries > XHI_MAX_RETRIES) 
            {
                return XST_DEVICE_BUSY;
            }
        }
    } else 
    {
        return XST_INVALID_PARAM;
    }
    return XST_SUCCESS;

};


/****************************************************************************/
/**
*
* Sends a DESYNC command to the ICAP port.
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
*@return    XStatus - XST_SUCCESS or XST_DEVICE_BUSY or XST_INVALID_PARAM
*
* @note     None.
*
*****************************************************************************/
XStatus XHwIcap_CommandDesync(XHwIcap *InstancePtr) 
{
    XStatus Status;

    /*
     * Assert validates the input arguments
     */
    XASSERT_NONVOID(InstancePtr != NULL);
    XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    XHwIcap_StorageBufferWrite(InstancePtr, 0, 
                        (XHwIcap_Type1Write(XHI_CMD) | 1));
    XHwIcap_StorageBufferWrite(InstancePtr, 1, XHI_CMD_DESYNCH);
    XHwIcap_StorageBufferWrite(InstancePtr, 2, XHI_DUMMY_PACKET);
    XHwIcap_StorageBufferWrite(InstancePtr, 3, XHI_DUMMY_PACKET);
    Status = XHwIcap_DeviceWrite(InstancePtr, 0, 4);  /* send four words */

    XASSERT_NONVOID(Status == XST_SUCCESS);

    return Status;  
}

/****************************************************************************/
/**
*
* Sends a CAPTURE command to the ICAP port.  This command caputres all
* of the flip flop states so they will be available during readback.
* One can use this command instead of enabling the CAPTURE block in the
* design.
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @return    XStatus - XST_SUCCESS or XST_DEVICE_BUSY or XST_INVALID_PARAM
*
* @note     None.
*
*****************************************************************************/
XStatus XHwIcap_CommandCapture(XHwIcap *InstancePtr) 
{
    XStatus Status;

    /*
     * Assert validates the input arguments
     */
    XASSERT_NONVOID(InstancePtr != NULL);
    XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    /* DUMMY and SYNC */
    XHwIcap_StorageBufferWrite(InstancePtr, 0, XHI_DUMMY_PACKET);
    XHwIcap_StorageBufferWrite(InstancePtr, 1, XHI_SYNC_PACKET);
    XHwIcap_StorageBufferWrite(InstancePtr, 2, 
                        (XHwIcap_Type1Write(XHI_CMD) | 1));
    XHwIcap_StorageBufferWrite(InstancePtr, 3, XHI_CMD_GCAPTURE);
    XHwIcap_StorageBufferWrite(InstancePtr, 4, XHI_DUMMY_PACKET);
    XHwIcap_StorageBufferWrite(InstancePtr, 5, XHI_DUMMY_PACKET);
    Status = XHwIcap_DeviceWrite(InstancePtr, 0, 6);  /* send six words */

    XASSERT_NONVOID(Status == XST_SUCCESS);

    return Status;  
}

/****************************************************************************
*
* Looks up the device configuration based on the unique device ID.  The table
* HwIcapConfigTable contains the configuration info for each device in the
* system.
*
* @param DeviceId is the unique device ID to match on.
*
* @return
*
* A pointer to the configuration data for the device, or NULL if no match
* was found.
*
* @note
*
* None.
*
******************************************************************************/
XHwIcap_Config *XHwIcap_LookupConfig(u16 DeviceId)
{
    XHwIcap_Config *CfgPtr = NULL;
    int i;

    for (i=0; i < CONFIG_XILINX_HWICAP_NUM_INSTANCES; i++)
    {
        if (XHwIcap_ConfigTable[i].DeviceId == DeviceId)
        {
            CfgPtr = &XHwIcap_ConfigTable[i];
            break;
        }
    }

    return CfgPtr;
}





