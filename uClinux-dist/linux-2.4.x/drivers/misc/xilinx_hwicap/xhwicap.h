/* $Header: /devl/xcs/repo/env/Databases/ip2/processor/software/devel/hwicap/v1_00_a/src/xhwicap.h,v 1.9 2004/01/07 01:23:21 brandonb Exp $ */
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
* @file xhwicap.h
*
* This file contains the software API definition of the Xilinx Hardware
* ICAP (hwicap) component.
*
* The Xilinx Hardware ICAP controller is designed to allow
* reconfiguration of select FPGA resources as well as loading partial
* bitstreams from system memory through the Internal Configuration
* Access Port (ICAP).  
*
* The source code for the XHwIcap_SetClbBits and XHwIcap_GetClbBits
* functions  are not included.  These functions are delivered as .o
* files.  Libgen uses the appropriate .o files for the target processor.
* This is specified by the hwicap_v2_1_0.tcl file in the data directory. 
*
* @note 
*
* There are a few items to be aware of when using this driver. 1) Only
* Virtex 2 and Virtex 2 Pro devices are supported as they are the only
* devices that contain the ICAP_VIRTEX2 component.  2) The ICAP port is
* disabled when the configuration mode, via the MODE pins, is set to
* Boundary Scan/JTAG. The ICAP is enabled in all other configuration
* modes and it is possible to configure the device via JTAG in all
* configuration modes. 3) Reading or writing to columns containing
* SRL16's or LUT RAM's can cause corruption of data in those elements.
* Avoid reading or writing to columns containing SRL16's or LUT RAM's.
*
*
* <pre> MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes ----- ---- --------
* ------------------------------------------------------- 1.00a bjb
* 11/17/03 First release
*
* </pre>
*
*****************************************************************************/

#ifndef XHWICAP_H_ /* prevent circular inclusions */
#define XHWICAP_H_ /* by using protection macros */

/***************************** Include Files ********************************/

#include "xhwicap_l.h"
#include <xstatus.h>

/************************** Constant Definitions ****************************/

/* Virtex 2 Device constants. The IDCODE for this device. */
#define XHI_XC2V40      0x01008093UL
#define XHI_XC2V80      0x01010093UL
#define XHI_XC2V250     0x01018093UL
#define XHI_XC2V500     0x01020093UL
#define XHI_XC2V1000    0x01028093UL
#define XHI_XC2V1500    0x01030093UL
#define XHI_XC2V2000    0x01038093UL
#define XHI_XC2V3000    0x01040093UL
#define XHI_XC2V4000    0x01050093UL
#define XHI_XC2V6000    0x01060093UL
#define XHI_XC2V8000    0x01070093UL

/* Virtex2 Pro Device constants. The IDCODE for this device. */
#define XHI_XC2VP2      0x01226093UL
#define XHI_XC2VP4      0x0123E093UL
#define XHI_XC2VP7      0x0124A093UL
#define XHI_XC2VP20     0x01266093UL
#define XHI_XC2VP30     0x0127E093UL
#define XHI_XC2VP40     0x01292093UL
#define XHI_XC2VP50     0x0129E093UL
#define XHI_XC2VP70     0x012BA093UL
#define XHI_XC2VP100    0x012D6093UL
#define XHI_XC2VP125    0x012F2093UL

/* ERROR Codes - if needed */

/************************** Type Definitions ********************************/

/**
* This typedef contains configuration information for the device.
*/
typedef struct
{
    u16 DeviceId;          /**< Unique ID  of device */
    u32 BaseAddress;       /**< Register base address */

} XHwIcap_Config;

/** 
* The XHwIcap driver instance data.  The user is required to allocated a
* variable of this type for every opb_hwicap device in the system. A
* pointer to a variable of this type is then passed to the driver API
* functions.
*
* Note - Virtex2/Pro devices only have one ICAP port so there should
* be at most only one opb_hwicap instantiated (per FPGA) in a system.
*/
typedef struct
{
    u32 BaseAddress;    /* Base address of this component */
    u32 IsReady;        /* Device is initialized and ready */
    u32 DeviceIdCode;   /* IDCODE of targeted device */
    u16 DeviceId;       /* User assigned ID for this component */
    u32 Rows;           /* Number of CLB rows */
    u32 Cols;           /* Number of CLB cols */
    u32 BramCols;       /* Number of BRAM cols */
    u32 BytesPerFrame;  /* Number of Bytes per minor Frame */
    u32 WordsPerFrame;  /* Number of Words per minor Frame */
    u32 ClbBlockFrames; /* Number of CLB type minor Frames */
    u32 BramBlockFrames;     /* Number of Bram type minor Frames */
    u32 BramIntBlockFrames;  /* Number of BramInt type minor Frames */
} XHwIcap;




/***************** Macro (Inline Functions) Definitions *********************/

/****************************************************************************/
/**
* 
* Converts a CLB SliceX coordinate to a column coordinate used by the
* XHwIcap_GetClbBits and XHwIcap_SetClbBits functions.
*
* @param    X - the SliceX coordinate to be converted
*
* @return   Column
*
* @note
*
* u32 XHwIcap_mSliceX2Col(u32 X);
*
*****************************************************************************/
#define XHwIcap_mSliceX2Col(X) \
    ( (X >> 1) + 1)

/****************************************************************************/
/**
* 
* Converts a CLB SliceY coordinate to a row coordinate used by the
* XHwIcap_GetClbBits and XHwIcap_SetClbBits functions.
*
* @param    InstancePtr - a pointer to the XHwIcap instance to be worked on.
*
* @param    Y - the SliceY coordinate to be converted
*
* @return   Row
*
* @note
*
* u32 XHwIcap_mSliceY2Row(XHwIcap *InstancePtr, u32 Y);
*
*****************************************************************************/
#define XHwIcap_mSliceY2Row(InstancePtr, Y) \
    ( (InstancePtr)->Rows - (Y >> 1) )

/****************************************************************************/
/**
* 
* Figures out which slice in a CLB is targeted by a given
* (SliceX,SliceY) pair.  This slice value is used for indexing in
* resource arrays.
*
* @param    X - the SliceX coordinate to be converted
*
* @param    Y - the SliceY coordinate to be converted
*
* @return   Slice index
*
* @note
*
* u32 XHwIcap_mSliceXY2Slice(u32 X, u32 Y);
*
*****************************************************************************/
#define XHwIcap_mSliceXY2Slice(X,Y) \
    ( ((X % 2) << 1) + (Y % 2) )


/************************** Function Prototypes *****************************/


/* These functions are the ones defined in the lower level
 * Self-Reconfiguration Platform (SRP) API.
 */

/* Initializes a XHwIcap instance.. */
XStatus XHwIcap_Initialize(XHwIcap *InstancePtr,  u16 DeviceId,
                           u32 DeviceIdCode); 

/* Reads integers from the device into the storage buffer. */
XStatus XHwIcap_DeviceRead(XHwIcap *InstancePtr, u32 Offset, 
                           u32 NumInts);

/* Writes integers to the device from the storage buffer. */
XStatus XHwIcap_DeviceWrite(XHwIcap *InstancePtr, u32 Offset, 
                            u32 NumInts);

/* Writes bytes to the device from the storage buffer. */
XStatus XHwIcap_DeviceWriteBytes(XHwIcap *InstancePtr, u32 Offset, 
                            u32 Numbytes);

/* Writes word to the storage buffer. */
void XHwIcap_StorageBufferWrite(XHwIcap *InstancePtr, u32 Address, 
                                u32 Data);

/* Reads word from the storage buffer. */
u32 XHwIcap_StorageBufferRead(XHwIcap *InstancePtr, u32 Address);

/* Reads one frame from the device and puts it in the storage buffer. */
XStatus XHwIcap_DeviceReadFrame(XHwIcap *InstancePtr, s32 Block, 
                               s32 MajorFrame, s32 MinorFrame); 

/* Writes one frame from the storage buffer and puts it in the device. */
XStatus XHwIcap_DeviceWriteFrame(XHwIcap *InstancePtr, s32 Block, 
                                s32 MajorFrame, s32 MinorFrame); 

/* Loads a partial bitstream from system memory. */
XStatus XHwIcap_SetConfiguration(XHwIcap *InstancePtr, u32 *Data, 
                                u32 Size);

/* Sends a DESYNC command to the ICAP */
XStatus XHwIcap_CommandDesync(XHwIcap *InstancePtr); 

/* Sends a CAPTURE command to the ICAP */
XStatus XHwIcap_CommandCapture(XHwIcap *InstancePtr);

/* Reconfigures the specified resource to the specified value. */
XStatus XHwIcap_SetClbBits(XHwIcap *InstancePtr, s32 Row, s32 Col,
      const u8 Resource[][2], const u8 Value[], s32 NumBits);

/* Reads the current configuration of the specified resource. */
XStatus XHwIcap_GetClbBits(XHwIcap *InstancePtr, s32 Row, s32 Col,
      const u8 Resource[][2], u8 Value[], s32 NumBits);
      
/* Pointer to a function that returns XHwIcap_Config info. */
XHwIcap_Config *XHwIcap_LookupConfig(u16 DeviceId);


/************************** Variable Declarations ***************************/


#endif
