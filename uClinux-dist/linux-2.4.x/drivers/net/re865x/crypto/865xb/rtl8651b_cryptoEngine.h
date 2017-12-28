/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : 8651B crypto engine driver header
* Abstract : 
* $Id: rtl8651b_cryptoEngine.h,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: rtl8651b_cryptoEngine.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.8  2006/01/09 11:37:22  yjlou
* *: apart alloc/free functions from init/exit functions.
*
* Revision 1.7  2006/01/04 13:58:40  yjlou
* *: fixed the bug of memory leak: rtl8651b_cryptoEngine_init() and rtl8651b_authEngine_init() will free allocated memory before allocating memory.
*
* Revision 1.6  2005/01/26 08:21:10  cfliu
* +:Commit Generic DMA engine code
*
* Revision 1.5  2004/12/27 01:43:31  yjlou
* +: support RTL8651_CRYPTO_NON_BLOCKING
*
* Revision 1.4  2004/07/13 07:08:10  danwu
* + document of API
*
* Revision 1.3  2004/06/23 15:36:12  yjlou
* *: removed duplicated code, and implemented original functions with the generic function:
*    rtl8651b_authEngine_md5(), rtl8651b_authEngine_sha1(),
*    rtl8651b_authEngine_hmacMd5(), rtl8651b_authEngine_hmacSha1(),
*    rtl8651b_cryptoEngine_ecb_encrypt(), rtl8651b_cryptoEngine_3des_ecb_encrypt(),
*    rtl8651b_cryptoEngine_cbc_encrypt(), rtl8651b_cryptoEngine_3des_cbc_encrypt()
* -: remove rtl8651b_cryptoEngine_cbc_encryptEmbIV() and rtl8651b_cryptoEngine_ede_cbc_encryptEmbIV()
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
* Revision 1.5  2003/09/30 12:24:28  jzchen
* Add interrupt counter get api
*
* Revision 1.4  2003/09/23 02:23:46  jzchen
* Add batch API
*
* Revision 1.3  2003/09/09 11:36:49  jzchen
* Add generic crypto function, which use mode to avoid 6 combination
*
* Revision 1.2  2003/09/08 04:40:39  jzchen
* Add initial function definition
*
* Revision 1.1  2003/08/28 02:52:07  jzchen
* Initial 8651b crypto engine driver code
*
*
* --------------------------------------------------------------------
*/

#ifndef RTL8651B_CRYPTOENGINE_H
#define RTL8651B_CRYPTOENGINE_H

#include <rtl_types.h>
#include <crypto.h>

int32 rtl8651b_cryptoEngine_init(uint32 descNum, int8 mode32Bytes);
int32 rtl8651b_cryptoEngine_exit( void );
int32 rtl8651b_cryptoEngine_alloc(uint32 descNum);
int32 rtl8651b_cryptoEngine_exit( void );

/*
@func void	| rtl8651b_cryptoEngine_des	| envoke crypto engine
@parm uint32 	| mode		| Crypto mode
@parm int8 * 	| data	| Pointer to input data
@parm uint32 	| dataLen		| Length of input data
@parm int8 *		| key	| Pointer to key
@parm int8 *		| iv	| Pointer to iv
@rdesc None
@rvalue SUCCESS		| 	Encryption/decryption done as requested
@rvalue FAILED	| 	Invalid parameter
@comm User does not need to reverse the sequence of keys when applying for 3DES.
That is, when data is encrypted with keys A/B/C, decrypt it with A/B/C, instead of C/B/A.
 */
//Bit 0: 0:DES 1:3DES
//Bit 1: 0:CBC 1:ECB
//Bit 2: 0:Decrypt 1:Encrypt
#define DECRYPT_CBC_DES		0x00
#define DECRYPT_CBC_3DES		0x01
#define DECRYPT_ECB_DES		0x02
#define DECRYPT_ECB_3DES		0x03
#define ENCRYPT_CBC_DES		0x04
#define ENCRYPT_CBC_3DES		0x05
#define ENCRYPT_ECB_DES		0x06
#define ENCRYPT_ECB_3DES		0x07
#define RTL8651_CRYPTO_NON_BLOCKING	0x08
#define RTL8651_CRYPTO_GENERIC_DMA	0x10


//data, key and iv does not have 4-byte alignment limitatiuon
int32 rtl8651b_cryptoEngine_des(uint32 mode, int8 *data, uint32 len, int8 *key, int8 *iv );

#define rtl8651b_cryptoEngine_ecb_encrypt(input,len,key,encrypt) rtl8651b_cryptoEngine_des(encrypt?ENCRYPT_ECB_DES:DECRYPT_ECB_DES,input,len,key,NULL)
#define rtl8651b_cryptoEngine_3des_ecb_encrypt(input,len,key,encrypt) rtl8651b_cryptoEngine_des(encrypt?ENCRYPT_ECB_3DES:DECRYPT_ECB_3DES,input,len,key,NULL)
#define rtl8651b_cryptoEngine_cbc_encrypt(input,len,key,iv,encrypt) rtl8651b_cryptoEngine_des(encrypt?ENCRYPT_CBC_DES:DECRYPT_CBC_DES,input,len,key,iv)
#define rtl8651b_cryptoEngine_3des_cbc_encrypt(input,len,key,iv,encrypt) rtl8651b_cryptoEngine_des(encrypt?ENCRYPT_CBC_3DES:DECRYPT_CBC_3DES,input,len,key,iv)


void rtl8651b_cryptoEngineGetIntCounter(uint32 * doneCounter, uint32 * allDoneCounter);

#endif

