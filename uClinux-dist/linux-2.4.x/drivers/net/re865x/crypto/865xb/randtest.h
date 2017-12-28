/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : Random number generator testing code
* Abstract : 
* $Id: randtest.h,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: randtest.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.2  2004/06/23 10:15:45  yjlou
* *: convert DOS format to UNIX format
*
* Revision 1.1  2004/06/23 09:18:57  yjlou
* +: support 865xB CRYPTO Engine
*   +: CONFIG_RTL865XB_EXP_CRYPTOENGINE
*   +: basic encry/decry functions (DES/3DES/SHA1/MAC)
*   +: old-fashion API (should be removed in next version)
*   +: batch functions (should be removed in next version)
*
* Revision 1.1  2003/10/07 11:10:46  jzchen
* Add random number generator testing code
*
*
*/

#ifndef RANDTEST_H
#define RANDTETS_H

#include <rtl_types.h>
#include <rtl_glue.h>

int32 rtl8651b_randTestCmd(uint32 userId,  int32 argc,int8 **saved);

#endif
