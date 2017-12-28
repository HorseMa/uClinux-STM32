/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : 8651B authentication engine driver code
* Abstract : 
* $Id: rtl8651b_authEngine.c,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: rtl8651b_authEngine.c,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.13  2006/01/09 11:37:22  yjlou
* *: apart alloc/free functions from init/exit functions.
*
* Revision 1.12  2006/01/04 13:58:40  yjlou
* *: fixed the bug of memory leak: rtl8651b_cryptoEngine_init() and rtl8651b_authEngine_init() will free allocated memory before allocating memory.
*
* Revision 1.11  2005/12/05 13:20:58  yjlou
* *: add static to local variables to avoid stack overflow.
*
* Revision 1.10  2005/08/23 14:38:26  chenyl
* +: apply prioirty IRAM/DRAM usage
*
* Revision 1.9  2005/07/20 15:29:37  yjlou
* +: porting Model Code to Linux Kernel: check RTL865X_MODEL_KERNEL.
*
* Revision 1.8  2005/05/26 07:18:46  yjlou
* *: fixed the bug that causes PCI shared pin become GPIO pin.
*
* Revision 1.7  2005/01/28 12:07:58  yjlou
* *: replace rtl8651b_authEngine_paddat[] into DRAM section.
*
* Revision 1.6  2004/12/27 01:44:45  yjlou
* +: support RTL8651_AUTH_NON_BLOCKING
* *: use RTL8651_AUTH_IOPAD_READY, instead of IOPAD_READY.
*
* Revision 1.5  2004/12/22 11:38:08  yjlou
* +: support IOPAD_READY to pre-compute ipad and opad.
*
* Revision 1.4  2004/12/22 08:48:08  yjlou
* +: add 16 NOPs when polling AuthEngine
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
* Revision 1.8  2003/10/28 03:50:40  jzchen
* Using 8651b defined interrupt layout
*
* Revision 1.7  2003/09/30 12:23:22  jzchen
* Add interrupt mode driver and remove data dump
*
* Revision 1.6  2003/09/29 09:04:24  jzchen
* Add authentication batch api
*
* Revision 1.5  2003/09/23 02:19:52  jzchen
* 1. Fix generic API length calculation error. 2. Provide dump funciton 3. Using All Done interrupt
*
* Revision 1.4  2003/09/18 14:31:21  jzchen
* Add general api
*
* Revision 1.3  2003/09/17 11:32:22  jzchen
* Change initialization process, iopad preparation and able to specify lexra bus mode at initial time
*
* Revision 1.2  2003/09/17 01:29:31  jzchen
* 1.Using 8-Word for each descriptor 2. Fix control address error bug
*
* Revision 1.1  2003/09/09 07:11:22  jzchen
* First version of authentication engine driver
*
*
* --------------------------------------------------------------------
*/

#define RTL8651B
#include <rtl8651b_authEngine.h>
#include <linux/autoconf.h>
#include <asicRegs.h>
#include <md5.h>
#include <hmac.h>
#include <sha1.h>
#include <assert.h>

/*
Assumption:
1. UNCACHED_MALLOC can get 4-byte aligned memory block
*/

//RTL8651B interface control
static volatile uint32 * authDescPtrArray;		//For DESDCR
static uint32 * authDescriptorArray;	//Each descriptor have 16-byte
static uint32 authDescNum, curAuthDescPos;
//RTL8651B driver packaging for ASIC
static uint8 * rtl8651b_authEngineInputPad, * rtl8651b_authEngineOutputPad;
static uint8 * rtl8651b_authEngineMd5TempKey, *rtl8651b_authEngineSha1TempKey;
static uint8 ** rtl8651b_authEngineBatchInputPad, ** rtl8651b_authEngineBatchOutputPad;
static uint8 ** rtl8651b_authEngineBatchMd5TempKey, ** rtl8651b_authEngineBatchSha1TempKey;
//Counters for interrupt
static uint32 authDoneIntCounter, authAllDoneIntCounter;

/* Louis note: if you want to use __DRAM, remember to initial this variable in init(). */
__DRAM_CRYPTO static uint8 rtl8651b_authEngine_paddat[RTL8651B_MAX_MD_CBLOCK] = {
	0x80,	0,	0,	0,	0,	0,	0,	0,
	0,		0,	0,	0,	0,	0,	0,	0,
	0,		0,	0,	0,	0,	0,	0,	0,
	0,		0,	0,	0,	0,	0,	0,	0,
	0,		0,	0,	0,	0,	0,	0,	0,
	0,		0,	0,	0,	0,	0,	0,	0,
	0,		0,	0,	0,	0,	0,	0,	0,
	0,		0,	0,	0,	0,	0,	0,	0,	
};


int32 rtl8651b_authEngine_alloc(uint32 descNum) 
{
	uint32 i;
	
	/* Before allocate new memory, we must free allocated memory. */
	rtl8651b_authEngine_free();
	
	authDescNum = descNum;
	authDescPtrArray = (uint32 *)UNCACHED_MALLOC(descNum * sizeof(uint32));
	authDescriptorArray = (uint32 *)UNCACHED_MALLOC(descNum * 8 * sizeof(uint32));//Per descriptor have 5 words(32-bit)
	rtl8651b_authEngineInputPad = (uint8 *)UNCACHED_MALLOC(RTL8651B_MAX_MD_CBLOCK*sizeof(uint8));
	rtl8651b_authEngineOutputPad = (uint8 *)UNCACHED_MALLOC(RTL8651B_MAX_MD_CBLOCK*sizeof(uint8));
	rtl8651b_authEngineMd5TempKey = (uint8 *)UNCACHED_MALLOC(RTL8651B_MD5_DIGEST_LENGTH*sizeof(uint8));
	rtl8651b_authEngineSha1TempKey = (uint8 *)UNCACHED_MALLOC(RTL8651B_SHA1_DIGEST_LENGTH*sizeof(uint8));
	rtl8651b_authEngineBatchInputPad = (uint8 **)rtlglue_malloc(descNum * sizeof(uint8 *));
	rtl8651b_authEngineBatchOutputPad = (uint8 **)rtlglue_malloc(descNum * sizeof(uint8 *));
	rtl8651b_authEngineBatchMd5TempKey = (uint8 **)rtlglue_malloc(descNum * sizeof(uint8 *));
	rtl8651b_authEngineBatchSha1TempKey = (uint8 **)rtlglue_malloc(descNum * sizeof(uint8 *));
	for(i=0; i<descNum; i++) {
		rtl8651b_authEngineBatchInputPad[i] = (uint8 *)UNCACHED_MALLOC(RTL8651B_MAX_MD_CBLOCK*sizeof(uint8));
		rtl8651b_authEngineBatchOutputPad[i] = (uint8 *)UNCACHED_MALLOC(RTL8651B_MAX_MD_CBLOCK*sizeof(uint8));
		rtl8651b_authEngineBatchMd5TempKey[i] = (uint8 *)UNCACHED_MALLOC(RTL8651B_MD5_DIGEST_LENGTH*sizeof(uint8));
		rtl8651b_authEngineBatchSha1TempKey[i] = (uint8 *)UNCACHED_MALLOC(RTL8651B_SHA1_DIGEST_LENGTH*sizeof(uint8));
	}
	
	for(i=0; i<descNum; i++) 
		authDescPtrArray[i] = ((uint32)&authDescriptorArray[(i<<3)] ) & 0xfffffffc;//Clear last 2-bit
	authDescPtrArray[descNum-1] |= DESC_WRAP;

	return SUCCESS;
}


int32 rtl8651b_authEngine_init(uint32 descNum, int8 mode32Bytes) {
	uint32 i;

	rtl8651b_authEngine_alloc( descNum );
	
	//Initial authentication-engine
	if(mode32Bytes == TRUE) {
		REG32(AECR) = 0x10400000;//Reset all descriptor, soft reset and Lexra bus burst size is 32 bytes
		REG32(AECR) = 0x10000000;//Enable and Lexra bus burst size is 32 bytes
	}
	else {
		REG32(AECR) = 0x00400000;//Reset all descriptors, soft reset and Lexra bus burst size is 16 bytes
		REG32(AECR) = 0x00000000;//Enable and Lexra bus burst size is 16 bytes
	}	
	REG32(ADCR) = (uint32)authDescPtrArray;
	REG32(AEIMR) = 0;
	curAuthDescPos = 0;

	/*
	 * Louis note:
	 * Once you enable __DRAM for rtl8651b_authEngine_paddat[],
	 *   you MUST re-initialize rtl8651b_authEngine_paddat[] again.
	 */
	rtl8651b_authEngine_paddat[0] = 0x80;
	for( i = 1; i < RTL8651B_MAX_MD_CBLOCK; i++ )
	{
		rtl8651b_authEngine_paddat[i] = 0x0;
	}

	return SUCCESS;
}


int32 rtl8651b_authEngine_free(void)
{
	uint32 i;

	/* free pointer array before rtl8651b_authEngineBatchInputPad, rtl8651b_authEngineBatchOutputPad, rtl8651b_authEngineBatchMd5TempKey and rtl8651b_authEngineBatchSha1TempKey are freed. */
	for( i = 0; i < authDescNum; i++ )
	{
		if ( rtl8651b_authEngineBatchInputPad[i] )
		{
			UNCACHED_FREE( rtl8651b_authEngineBatchInputPad[i] );
			rtl8651b_authEngineBatchInputPad[i] = NULL; 
		}
		if ( rtl8651b_authEngineBatchOutputPad[i] )
		{
			UNCACHED_FREE( rtl8651b_authEngineBatchOutputPad[i] );
			rtl8651b_authEngineBatchOutputPad[i] = NULL; 
		}
		if ( rtl8651b_authEngineBatchMd5TempKey[i] )
		{
			UNCACHED_FREE( rtl8651b_authEngineBatchMd5TempKey[i] );
			rtl8651b_authEngineBatchMd5TempKey[i] = NULL; 
		}
		if ( rtl8651b_authEngineBatchSha1TempKey[i] )
		{
			UNCACHED_FREE( rtl8651b_authEngineBatchSha1TempKey[i] );
			rtl8651b_authEngineBatchSha1TempKey[i] = NULL; 
		}
	}
	if ( authDescPtrArray )
	{
		UNCACHED_FREE( authDescPtrArray );
		authDescPtrArray = NULL; 
	}
	if ( authDescriptorArray )
	{
		UNCACHED_FREE( authDescriptorArray );
		authDescriptorArray = NULL; 
	}
	if ( rtl8651b_authEngineInputPad )
	{
		UNCACHED_FREE( rtl8651b_authEngineInputPad );
		rtl8651b_authEngineInputPad = NULL; 
	}
	if ( rtl8651b_authEngineOutputPad )
	{
		UNCACHED_FREE( rtl8651b_authEngineOutputPad );
		rtl8651b_authEngineOutputPad = NULL; 
	}
	if ( rtl8651b_authEngineMd5TempKey )
	{
		UNCACHED_FREE( rtl8651b_authEngineMd5TempKey );
		rtl8651b_authEngineMd5TempKey = NULL; 
	}
	if ( rtl8651b_authEngineSha1TempKey )
	{
		UNCACHED_FREE( rtl8651b_authEngineSha1TempKey );
		rtl8651b_authEngineSha1TempKey = NULL; 
	}
	if ( rtl8651b_authEngineBatchInputPad )
	{
		rtlglue_free( rtl8651b_authEngineBatchInputPad );
		rtl8651b_authEngineBatchInputPad = NULL; 
	}
	if ( rtl8651b_authEngineBatchOutputPad )
	{
		rtlglue_free( rtl8651b_authEngineBatchOutputPad );
		rtl8651b_authEngineBatchOutputPad = NULL; 
	}
	if ( rtl8651b_authEngineBatchMd5TempKey )
	{
		rtlglue_free( rtl8651b_authEngineBatchMd5TempKey );
		rtl8651b_authEngineBatchMd5TempKey = NULL; 
	}
	if ( rtl8651b_authEngineBatchSha1TempKey )
	{
		rtlglue_free( rtl8651b_authEngineBatchSha1TempKey );
		rtl8651b_authEngineBatchSha1TempKey = NULL; 
	}
	
	return SUCCESS;
}


int32 rtl8651b_authEngine_exit(void)
{
	rtl8651b_authEngine_free();
	
	return SUCCESS;
}


/*
@func int32	| rtl8651b_authEngine	| Provide authentication function
@parm uint32	| HASH_MD5/HASH_SHA1/HMAC_MD5/HMAC_SHA1, and bit-wised IOPAD_READY when ipad/opad are pre-computed.
@parm uint8* 	| data	| data to be authenticated.
@parm uint32	| dataLen	| data length
@parm uint8* 	| key		| key (when IOPAD_READY is set, key is pointed to ipad and opad)
@parm uint32	| keyLen	| key length
@parm uint8 * 	| digest	| output digest data
@rvalue SUCCESS	| A pair of <p extIp> and <p extPort> selected. 
@rvalue FAILED 	| Failed to select either <p extIp> or <p extPort>.
@comm 
@devnote
 */
int32 rtl8651b_authEngine(uint32 mode, uint8 * data, uint32 dataLen, uint8 * key, uint32 keyLen, uint8 * digest) {
	static MD5_CTX md5Context;
	static SHA1_CTX sha1Context;
	static uint32 i, dmaLen;
	static uint64 len64;
	static uint8 *uint8Ptr = (uint8 *)&len64;
	static uint8 *ipad, *opad;

	assert(data && digest);//Either Hashing or key must exist
	//Calculating ipad and opad
	if(mode&0x2) {//HMAC
		assert(key);
		if(keyLen > HMAC_MAX_MD_CBLOCK) {
			if(mode&0x1) {
				SHA1Init(&sha1Context);
				SHA1Update(&sha1Context, key, keyLen);
				SHA1Final(rtl8651b_authEngineSha1TempKey, &sha1Context);
				key = rtl8651b_authEngineSha1TempKey;
				keyLen = SHA_DIGEST_LENGTH;
			}
			else {
				MD5Init(&md5Context);
				MD5Update(&md5Context, key, keyLen);
				MD5Final(rtl8651b_authEngineMd5TempKey, &md5Context);
				key = rtl8651b_authEngineMd5TempKey;
				keyLen = MD5_DIGEST_LENGTH;
			}
		}
		
		if ( mode & RTL8651_AUTH_IOPAD_READY )
		{
			/* ipad and opad is pre-computed. */
			ipad = key;
			opad = key + RTL8651B_MAX_MD_CBLOCK;
		}
		else
		{
			memset(rtl8651b_authEngineInputPad, 0x36, RTL8651B_MAX_MD_CBLOCK);
			memset(rtl8651b_authEngineOutputPad, 0x5c, RTL8651B_MAX_MD_CBLOCK);
			for(i=0; i<keyLen; i++) {
				rtl8651b_authEngineInputPad[i] ^= key[i];
				rtl8651b_authEngineOutputPad[i] ^= key[i];
			}
			ipad = rtl8651b_authEngineInputPad;
			opad = rtl8651b_authEngineOutputPad;
		}
	}
	//Do padding for MD5 and SHA-1
	i = RTL8651B_MAX_MD_CBLOCK - (dataLen&0x3F);
	if(i>8) {
		memcpy((void *)(data + dataLen), (void *)rtl8651b_authEngine_paddat, i-8);
		dmaLen = (dataLen+64)&0xFFFFFFC0;
	}
	else {
		memcpy((void *)(data + dataLen), (void *)rtl8651b_authEngine_paddat, i+56);
		dmaLen = (dataLen+72)&0xFFFFFFC0;
	}
	if(dmaLen & 0xFFFFF000)
		return FAILED;//DMA length larger than 8651b can support
	if(mode&0x2) //HMAC
		len64 = (dataLen+64)<<3;//First padding length is
	else
		len64 = dataLen<<3;//First padding length is
	for(i=0; i<8; i++) {
		if(mode&0x1) //SHA-1
			data[dmaLen-8+i] = uint8Ptr[i];
		else
			data[dmaLen-i-1] = uint8Ptr[i];
	}
	//Trigger authentication to do hashing operation
	assert((REG32(AECR) & AUTH_FETCH)==0);//Fetch command should return to zero when previous job finished
	assert((authDescPtrArray[curAuthDescPos] & DESC_OWNED_BIT) == DESC_RISC_OWNED);//Co-processor mode should guarantee function returned with RISC own descriptor
	authDescriptorArray[(curAuthDescPos<<3)+0] = (dmaLen & DES_DMA_LEN_MASK); //CTRL_LEN
	if(mode&0x2)
		authDescriptorArray[(curAuthDescPos<<3)+0] |= AUTH_HMAC;
	else
		authDescriptorArray[(curAuthDescPos<<3)+0] |= (AUTH_HASH_INIT | AUTH_HASH_UPD | AUTH_HASH_FIN);
	if(mode&0x1)
		authDescriptorArray[(curAuthDescPos<<3)+0] |= AUTH_SHA1;
	authDescriptorArray[(curAuthDescPos<<3)+1] = (uint32)data; //DMA_addr
	authDescriptorArray[(curAuthDescPos<<3)+2] = (uint32)ipad; //ipad_addr
	authDescriptorArray[(curAuthDescPos<<3)+3] = (uint32)opad; //opad_addr
	if(mode&0x1)
		authDescriptorArray[(curAuthDescPos<<3)+4] = (uint32)rtl8651b_authEngineSha1TempKey; //digest_addr
	else
		authDescriptorArray[(curAuthDescPos<<3)+4] = (uint32)rtl8651b_authEngineMd5TempKey; //digest_addr
	authDescPtrArray[curAuthDescPos] |= DESC_ENG_OWNED;//Change owner to crypto-engine

	REG32(AEISR) = 0;//Clear interrupt status
	REG32(AECR) |= AUTH_FETCH; //Notify crypto-engine to get data to operate
	if ( mode & RTL8651_AUTH_NON_BLOCKING ) goto no_wait;
	while((REG32(AEISR) & AUTH_OPALLDONE) != AUTH_OPALLDONE) IDLE_CPU();//Wait until authentication-done

	while((authDescPtrArray[curAuthDescPos] & DESC_OWNED_BIT) != DESC_RISC_OWNED) IDLE_CPU();//Co-processor mode should guarantee function return with RISC own descriptor
	assert((REG32(AECR) & AUTH_FETCH)==0);//Fetch command should return to zero when previous job finished

no_wait:
	/*curAuthDescPos = (curAuthDescPos+1)%authDescNum;*/
	curAuthDescPos++;
	if ( curAuthDescPos >= authDescNum ) curAuthDescPos -= authDescNum;

	if(mode&0x1)
		memcpy(digest, rtl8651b_authEngineSha1TempKey, RTL8651B_SHA1_DIGEST_LENGTH);//Avoid 4-byte alignment limitation
	else
		memcpy(digest, rtl8651b_authEngineMd5TempKey, RTL8651B_MD5_DIGEST_LENGTH);//Avoid 4-byte alignment limitation
	
	return SUCCESS;
}


void rtl8651b_authEngineGetIntCounter(uint32 * doneCounter, uint32 * allDoneCounter) {
	*doneCounter = authDoneIntCounter;
	*allDoneCounter = authAllDoneIntCounter;
}

