/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : HMAC header files
* Abstract : 
* $Id: hmac.h,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: hmac.h,v $
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
* Revision 1.2  2003/09/02 14:58:16  jzchen
* HMAC emulation API
*
* --------------------------------------------------------------------
*/

#ifndef HEADER_HMAC_H
#define HEADER_HMAC_H

#include <rtl_types.h>
#include <crypto.h>

#define HMAC_MAX_MD_CBLOCK	64

int32 HMACMD5(uint8 * data, uint32 dataLen, uint8 * key, uint32 keyLen, uint8 * digest);
int32 HMACSHA1(uint8 * data, uint32 dataLen, uint8 * key, uint32 keyLen, uint8 * digest);

#endif
