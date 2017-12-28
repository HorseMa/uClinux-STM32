/*
* ----------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/crypto/865xb/Attic/cryptoCmd.h,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
*
* Abstract: Crypto engine access command driver source code.
* $Author: davidm $
* $Log: cryptoCmd.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.3  2005/02/17 05:29:34  yjlou
* +: support CBC_AES (CLE only)
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
* Revision 1.2  2003/09/17 01:36:16  jzchen
* Add authentication command
*
* Revision 1.1  2003/08/21 07:06:40  jzchen
* Using openssl des testing code to test software des function
*
*
* ---------------------------------------------------------------
*/

#ifndef CRYPTOCMD_H
#define CRYPTOCMD_H

int32 destest(void);
int32 _rtl8651b_cryptoCmd(uint32 userId,  int32 argc,int8 **saved);
int32 _rtl8651b_authenticationCmd(uint32 userId,  int32 argc,int8 **saved);

extern cle_exec_t rtl865x_crypt_cmds[];

#define CMD_RTL865XB_CRYPT_CMD_NUM 3

#endif
