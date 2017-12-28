#ifndef RTL8651_TBLASICDRV_H
#define RTL8651_TBLASICDRV_H

#ifdef CONFIG_RTL865XC
#include "rtl865xC_tblAsicDrv.h"
#elif defined CONFIG_RTL865XB
#include "rtl865xB_tblAsicDrv.h"
#else
#error "rtl8651_tblAsicDrv.h : Unknown Platform"
#endif

#endif

