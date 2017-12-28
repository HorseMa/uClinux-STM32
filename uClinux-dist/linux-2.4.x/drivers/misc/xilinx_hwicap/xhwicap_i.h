/* $Header: /devl/xcs/repo/env/Databases/ip2/processor/software/devel/hwicap/v1_00_a/src/xhwicap_i.h,v 1.4 2003/12/23 23:15:03 brandonb Exp $ */
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
* @file xhwicap_i.h
*
* This head file contains internal identifiers, which are those shared
* between the files of the driver.  It is intended for internal use
* only.
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

#ifndef XHWICAP_I_H_ /* prevent circular inclusions */
#define XHWICAP_I_H_ /* by using protection macros */

/***************************** Include Files ********************************/

#include "xhwicap.h"


/************************** Constant Definitions ****************************/


#define XHI_PAD_FRAMES              0x1

/* Mask for calculating configuration packet headers */
#define XHI_WORD_COUNT_MASK_TYPE_1  0x7FFUL
#define XHI_WORD_COUNT_MASK_TYPE_2  0x1FFFFFUL
#define XHI_TYPE_MASK               0x7
#define XHI_REGISTER_MASK           0xF
#define XHI_OP_MASK                 0x3
#define XHI_FAR_BLOCK_MASK          0x3
#define XHI_FAR_MAJOR_FRAME_MASK    0xFF
#define XHI_FAR_MINOR_FRAME_MASK    0xFF

#define XHI_TYPE_SHIFT              29
#define XHI_REGISTER_SHIFT          13
#define XHI_OP_SHIFT                27
#define XHI_FAR_BLOCK_SHIFT         25
#define XHI_FAR_MAJOR_FRAME_SHIFT   17
#define XHI_FAR_MINOR_FRAME_SHIFT   9

#define XHI_TYPE_1                  1
#define XHI_TYPE_2                  2
#define XHI_OP_WRITE                2
#define XHI_OP_READ                 1

/* Address Block Types */
#define XHI_FAR_CLB_BLOCK           0
#define XHI_FAR_BRAM_BLOCK          1
#define XHI_FAR_BRAM_INT_BLOCK      2

/* Addresses of the Configuration Registers */
#define XHI_CRC                     0
#define XHI_FAR                     1
#define XHI_FDRI                    2
#define XHI_FDRO                    3
#define XHI_CMD                     4
#define XHI_CTL                     5
#define XHI_MASK                    6
#define XHI_STAT                    7
#define XHI_LOUT                    8
#define XHI_COR                     9
#define XHI_MFWR                    10
#define XHI_FLR                     11
#define XHI_KEY                     12
#define XHI_CBC                     13
#define XHI_IDCODE                  14
#define XHI_NUM_REGISTERS           15

/* Configuration Commands */
#define XHI_CMD_WCFG                1
#define XHI_CMD_MFW                 2
#define XHI_CMD_DGHIGH              3
#define XHI_CMD_RCFG                4
#define XHI_CMD_START               5
#define XHI_CMD_RCAP                6
#define XHI_CMD_RCRC                7
#define XHI_CMD_AGHIGH              8
#define XHI_CMD_SWITCH              9
#define XHI_CMD_GRESTORE            10
#define XHI_CMD_SHUTDOWN            11
#define XHI_CMD_GCAPTURE            12
#define XHI_CMD_DESYNCH             13

/* Packet constants */
#define XHI_SYNC_PACKET             0xAA995566UL
#define XHI_DUMMY_PACKET            0xFFFFFFFFUL
#define XHI_NOOP_PACKET             (XHI_TYPE_1 << XHI_TYPE_SHIFT)
#define XHI_TYPE_2_READ ( (XHI_TYPE_2 << XHI_TYPE_SHIFT) | \
        (XHI_OP_READ << XHI_OP_SHIFT) ) 

#define XHI_TYPE_2_WRITE ( (XHI_TYPE_2 << XHI_TYPE_SHIFT) | \
        (XHI_OP_WRITE << XHI_OP_SHIFT) )

#define XHI_TYPE_1_PACKET_MAX_WORDS 2047UL
#define XHI_TYPE_1_HEADER_BYTES     4
#define XHI_TYPE_2_HEADER_BYTES     8

/* Indicates how many bytes will fit in a buffer. (1 BRAM) */
#define XHI_MAX_BUFFER_BYTES        2048
#define XHI_MAX_BUFFER_INTS         512

/* Number of frames in different tile types */
#define XHI_GCLK_FRAMES             4
#define XHI_IOB_FRAMES              4
#define XHI_IOI_FRAMES              22
#define XHI_CLB_FRAMES              22
#define XHI_BRAM_FRAMES             64
#define XHI_BRAM_INT_FRAMES         22

/* The number of words reserved for the header in the storage buffer. */
#define XHI_HEADER_BUFFER_WORDS     20
#define XHI_HEADER_BUFFER_BYTES     (XHI_HEADER_BUFFER_WORDS << 2)

/* CLB major frames start at 3 for the first column (since we are using 
 * column numbers that start at 1, when the column is added to this offset, 
 * that first one will be 3 as required. */
#define XHI_CLB_MAJOR_FRAME_OFFSET  2


/* File access and error constants */
#define XHI_DEVICE_READ_ERROR       -1
#define XHI_DEVICE_WRITE_ERROR      -2
#define XHI_BUFFER_OVERFLOW_ERROR   -3

#define XHI_DEVICE_READ             0x1
#define XHI_DEVICE_WRITE            0x0

/* Constants for checking transfer status */
#define XHI_CYCLE_DONE              0
#define XHI_CYCLE_EXECUTING         1

/* Constant to use for CRC check when CRC has been disabled */
#define XHI_DISABLED_AUTO_CRC       0x0000DEFCUL

/* Major Row Offset */
#define XHI_CLB_MAJOR_ROW_OFFSET 96+(32*XHI_HEADER_BUFFER_WORDS)-1

/* Number of times to poll the done regsiter */
#define XHI_MAX_RETRIES     1000

/**************************** Type Definitions *******************************/


/***************** Macros (Inline Functions) Definitions *********************/

/****************************************************************************/
/**
* 
* Generates a Type 1 packet header that reads back the requested configuration
* register.
*
* @param    Register is the address of the register to be read back.
*           Register constants are defined in this file.
*
* @return   Type 1 packet header to read the specified register
*
* @note     None.
*
*****************************************************************************/
#define XHwIcap_Type1Read(Register) \
    ( (XHI_TYPE_1 << XHI_TYPE_SHIFT) | (Register << XHI_REGISTER_SHIFT) | \
    (XHI_OP_READ << XHI_OP_SHIFT) )

/****************************************************************************/
/**
* 
* Generates a Type 1 packet header that writes to the requested
* configuration register.
*
* @param    Register is the address of the register to be written to.
*           Register constants are defined in this file.
*
* @return   Type 1 packet header to write the specified register
*
* @note     None.
*
*****************************************************************************/
#define XHwIcap_Type1Write(Register) \
    ( (XHI_TYPE_1 << XHI_TYPE_SHIFT) | (Register << XHI_REGISTER_SHIFT) | \
    (XHI_OP_WRITE << XHI_OP_SHIFT) )

/****************************************************************************/
/**
* 
* Generates a Type 1 packet header that writes to the FAR (Frame Address
* Register).
*
* @param    Block - Address Block Type (CLB or BRAM address space)
*
* @param    MajorAddress - CLB or BRAM column
*
* @param    MinorAdderss - Frame within column
*
* @return   Type 1 packet header to write the FAR
*
* @note     None.
*
*****************************************************************************/
#define XHwIcap_SetupFar(Block, MajorAddress, MinorAddress)  \
    ((Block << XHI_FAR_BLOCK_SHIFT) | \
     (MajorAddress << XHI_FAR_MAJOR_FRAME_SHIFT) | \
     (MinorAddress << XHI_FAR_MINOR_FRAME_SHIFT))

/************************** Function Prototypes ******************************/


/************************** Variable Definitions ****************************/

/* the configuration table */
extern XHwIcap_Config XHwIcap_ConfigTable[];

#endif
