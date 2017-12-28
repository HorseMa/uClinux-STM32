/*
* Copyright c                  Realtek Semiconductor Corporation, 2004  
* All rights reserved.
* 
* Program : Control MII & MDC/MDIO connected 8305SB/8305SC commands
* Abstract : 
* Author : Edward Jin-Ru Chen (jzchen@realtek.com.tw)               
* $Id: rtl8305s_cmd.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
*/

#ifndef _RTL8305S_H
#define _RTL8305S_H

#include "rtl_types.h"

extern cle_exec_t rtl8305s_cmds[];

#ifdef CONFIG_RTL8305S
#define CMD_RTL8305S_CMD_NUM		3
#endif

#ifdef CONFIG_RTL8305SB
#undef CMD_RTL8305S_CMD_NUM
#define CMD_RTL8305S_CMD_NUM		3
#endif

#ifdef CONFIG_RTL8305SC
#undef CMD_RTL8305S_CMD_NUM
#define CMD_RTL8305S_CMD_NUM		7
#endif

#endif

