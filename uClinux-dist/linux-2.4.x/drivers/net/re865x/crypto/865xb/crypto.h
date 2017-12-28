/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003
* All rights reserved.
*
* Program : Header file for all crypto sources
* Abstract : This file includes all common definitions of crypto engine.
* $Id: crypto.h,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: crypto.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.3  2005/09/08 14:07:00  yjlou
* *: fixed the porting bugS of software DES in re865x/crypto: We always use Linux kernel's DES library.
*
* Revision 1.2  2004/12/22 08:48:08  yjlou
* +: add 16 NOPs when polling AuthEngine
*
* Revision 1.1  2004/06/23 09:18:57  yjlou
* +: support 865xB CRYPTO Engine
*   +: CONFIG_RTL865XB_EXP_CRYPTOENGINE
*   +: basic encry/decry functions (DES/3DES/SHA1/MAC)
*   +: old-fashion API (should be removed in next version)
*   +: batch functions (should be removed in next version)
*
*
*
* --------------------------------------------------------------------
*/

#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#define UNCACHED_MALLOC(x)  (void *) (0xa0000000 | (uint32) rtlglue_malloc(x))
#define UNCACHED_FREE(x)  rtlglue_free( (void *) (~0x20000000 & (uint32) x ) )

#include <rtl_glue.h>

#define IDLE_CPU() \
	do {\
		asm( "\
			nop; nop; nop; nop; nop; nop; nop; nop; \
			nop; nop; nop; nop; nop; nop; nop; nop; \
			"); \
	} while (0) 

#endif// __CRYPTO_H__
