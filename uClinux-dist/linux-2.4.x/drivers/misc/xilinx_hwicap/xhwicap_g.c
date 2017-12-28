/* $Id: xhwicap_g.c,v 1.1 2006/11/09 22:33:52 jwilliams Exp $ */
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
*       (c) Copyright 2002 Xilinx Inc.
*       All rights reserved.
*
*****************************************************************************/
/****************************************************************************/
/**
*
* @file xhwicap_g.c
*
* This file contains a configuration table that specifies the configuration of
* opb_hwicap devices in the system. Each device in the system should have an
* entry in the table.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00a bjb  12/08/03 First release
* </pre>
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xhwicap.h"
#include <linux/autoconf.h>

/************************** Constant Definitions *****************************/


/**************************** Type Definitions *******************************/


/***************** Macros (Inline Functions) Definitions *********************/


/************************** Function Prototypes ******************************/


/************************** Variable Prototypes ******************************/

/**
 * The configuration table for opb_hwicap devices
 */
XHwIcap_Config XHwIcap_ConfigTable[CONFIG_XILINX_HWICAP_NUM_INSTANCES] =
{
    {
        0,    /* Unique ID of device */
        CONFIG_XILINX_HWICAP_0_BASEADDR,     /* Device base address */
    },
};


