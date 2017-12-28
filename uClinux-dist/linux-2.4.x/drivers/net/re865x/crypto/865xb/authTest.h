/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : RTL8651B authentication engine testing code
* Abstract : Compare the calculation result of authentication simulator and rtl8651b authentication engine
* $Id: authTest.h,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: authTest.h,v $
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
* Revision 1.2  2003/09/29 09:03:39  jzchen
* Test authentication batch command
*
* Revision 1.1  2003/09/23 02:23:20  jzchen
* Add authentication testing code
*
*
* --------------------------------------------------------------------
*/

#ifndef AUTHTEST_H
#define AUTHTEST_H

#include <rtl_types.h>

int32 authTest_hashTest(void);
int32 authTest_hmacTest(void);
int32 runAuth8651bGeneralApiTest(uint32 round, uint32 funStart, uint32 funEnd, uint32 lenStart, uint32 lenEnd, uint32 keyLenStart, uint32 keyLenEnd, uint32 offsetStart, uint32 offsetEnd);
int32 runAuth8651bBatchApiTest(uint32 round, uint32 funStart, uint32 funEnd, uint32 lenStart, uint32 lenEnd, uint32 keyLenStart, uint32 keyLenEnd, uint32 offsetStart, uint32 offsetEnd, uint32 batchStart, uint32 batchEnd);

#endif
