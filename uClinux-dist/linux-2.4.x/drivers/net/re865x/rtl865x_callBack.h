/*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program : rtl8651_callBack.h
* Abstract : 
* Creator : 
* Author :  
*/
#ifndef _RTL865X_CALLBACK_H
#define _RTL865X_CALLBACK_H

#include "rtl865x/rtl_glue.h"

/* ================================================================================
		System setting
    ================================================================================ */

#define LAN_DEVNAME	"eth1"

/* ================================================================================
		External function decalarations
    ================================================================================ */
int32 rtl865x_callBack_init (void);

/* ================================================================================
		Debug message
    ================================================================================ */
#define RTL865X_CALLBACK_DEBUG

#ifdef RTL865X_CALLBACK_DEBUG
#define RTL865X_CALLBACK_MSG_MASK		0xfffffffc	/* Tune ON all debug messages except INFO and WARN */
#define RTL865X_CALLBACK_MSG_INFO			(1<<0)
#define RTL865X_CALLBACK_MSG_WARN		(1<<1)
#define RTL865X_CALLBACK_MSG_ERR			(1<<2)
#endif

#if (RTL865X_CALLBACK_MSG_MASK & RTL865X_CALLBACK_MSG_INFO)
#define RTL865X_CALLBACK_INFO(fmt, args...) \
		do {rtlglue_printf("[%s-%d]-info-: " fmt "\n", __FUNCTION__, __LINE__, ## args);} while (0);
#else
#define RTL865X_CALLBACK_INFO(fmt, args...)
#endif

#if (RTL865X_CALLBACK_MSG_MASK & RTL865X_CALLBACK_MSG_WARN)
#define RTL865X_CALLBACK_WARN(fmt, args...) \
		do {rtlglue_printf("[%s-%d]-warning-: " fmt "\n", __FUNCTION__, __LINE__, ## args);} while (0);
#else
#define RTL865X_CALLBACK_WARN(fmt, args...)
#endif

#if (RTL865X_CALLBACK_MSG_MASK & RTL865X_CALLBACK_MSG_ERR)
#define RTL865X_CALLBACK_ERR(fmt, args...) \
		do {rtlglue_printf("[%s-%d]-error-: " fmt "\n", __FUNCTION__, __LINE__, ## args);} while (0);
#else
#define RTL865X_CALLBACK_ERR(fmt, args...)
#endif

/* ================================================================================
		Registration related
    ================================================================================ */
typedef int32 (*_dummyFunc)(void*);
typedef struct _rtl865x_callBack_registrationInfo_s {
	void *registerFunc;
	void *callBackFunc;
	char *functionName;
} _rtl865x_callBack_registrationInfo_t;

/* ================================================================================
		MISC
    ================================================================================ */

#define RTL865X_CALLBACK_FUNCCALL(func, retval, errval) \
	do { \
		int32 _retval; \
		if (( _retval = (func) ) != (retval)) { \
			RTL865X_CALLBACK_ERR("Function call return %d", _retval); \
			return (errval); \
		} \
	} while (0);

#ifndef IPHDR_LEN
#define IPHDR_LEN(iphdr)					(((*(uint8*)iphdr)&0xf) << 2)					/* get the ip length */
#endif
#ifndef L4HDR_ADDR
#define L4HDR_ADDR(iphdr)				((uint8 *)iphdr + (((*(uint8*)iphdr)&0xf) << 2))	/* calculate L4 header address by ip header */
#endif
#ifndef IS_FRAGMENT_REMAIN
#define IS_FRAGMENT_REMAIN(off)	(ntohs((uint16)(off)) & IP_OFFMASK)
#endif

#endif /* _RTL865X_CALLBACK_H */
