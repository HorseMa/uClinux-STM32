/*
* Copyright c                  Realtek Semiconductor Corporation, 2006
* All rights reserved.
* 
* Program : Patching Switch core table driver generic header file
* Abstract : 
* Author : Yi-Lun Chen (chenyl@realtek.com.tw)               
*
*/


#ifndef RTL8651_TBLDRV_PATCH_H
#define RTL8651_TBLDRV_PATCH_H

#ifdef CONFIG_RTL865XB
	#include "rtl865xB_tblDrvPatch.h"
#endif

#ifdef CONFIG_RTL865XC
	#include "rtl865xC_tblDrvPatch.h"
#endif

#endif /* RTL8651_TBLDRV_PATCH_H */


