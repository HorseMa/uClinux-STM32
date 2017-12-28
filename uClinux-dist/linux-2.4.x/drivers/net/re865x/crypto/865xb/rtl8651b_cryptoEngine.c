/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : 8651B crypto engine driver code
* Abstract : 
* $Id: rtl8651b_cryptoEngine.c,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: rtl8651b_cryptoEngine.c,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.11  2006/12/04 07:30:55  alva_zhang
* replace all printk with rtlglue_printf
*
* Revision 1.10  2006/01/09 11:37:22  yjlou
* *: apart alloc/free functions from init/exit functions.
*
* Revision 1.9  2006/01/04 13:58:40  yjlou
* *: fixed the bug of memory leak: rtl8651b_cryptoEngine_init() and rtl8651b_authEngine_init() will free allocated memory before allocating memory.
*
* Revision 1.8  2005/07/20 15:29:37  yjlou
* +: porting Model Code to Linux Kernel: check RTL865X_MODEL_KERNEL.
*
* Revision 1.7  2005/01/26 08:20:45  cfliu
* +: Commit Generic DMA engine with Crypto engine code
*
* Revision 1.6  2004/12/27 01:43:31  yjlou
* +: support RTL8651_CRYPTO_NON_BLOCKING
*
* Revision 1.5  2004/12/06 09:25:31  cfliu
* +Add crypto engine FPGA memcpy test cmd
*
* Revision 1.4  2004/09/16 04:26:22  cfliu
* Add polling mode for DES engine
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
* Revision 1.14  2003/10/28 03:50:30  jzchen
* Using 8651b defined interrupt layout
*
* Revision 1.13  2003/09/30 12:22:59  jzchen
* Add interrupt mode driver
*
* Revision 1.12  2003/09/25 09:58:25  jzchen
* Solve batch encryption bug which does not mod descriptor number and start working
*
* Revision 1.11  2003/09/24 13:39:02  jzchen
* Fix command setting error bug
*
* Revision 1.10  2003/09/17 11:29:51  jzchen
* 1. Tunning initialization sequence 2. CBC embedded IV mode IV point to data
*
* Revision 1.9  2003/09/17 01:28:52  jzchen
* Command setting error bug fix
*
* Revision 1.8  2003/09/15 03:48:07  jzchen
* Add DES batch operation
*
* Revision 1.7  2003/09/09 11:28:42  jzchen
* Add generic crypto function, which use mode to avoid 6 combination
*
* Revision 1.6  2003/09/09 07:11:48  jzchen
* Add assumption description
*
* Revision 1.5  2003/09/08 09:21:24  jzchen
* Rename for unique naming
*
* Revision 1.4  2003/09/02 08:28:12  jzchen
* Change to descriptor based polling mode (Descriptor number larger than 1)
*
* Revision 1.3  2003/09/01 13:27:19  jzchen
* Try 32-byte burst mode and work
*
* Revision 1.2  2003/08/28 14:28:13  jzchen
* decriptor IV should always set to correct value
* OWN bit may not return to correct value as soon as interrupt happen
*
* Revision 1.1  2003/08/28 02:50:03  jzchen
* Initial version of 8651b crypto engine driver code
*
*
* --------------------------------------------------------------------
*/

#define RTL8651B
#include <rtl8651b_cryptoEngine.h>
#include <linux/autoconf.h>
#include <asicRegs.h>
#include <rtl_glue.h>
#include <assert.h>



/*
Assumption:
1. UNCACHED_MALLOC can get 4-byte aligned memory block
*/
static volatile uint32 * desDescPtrArray;		//For DESDCR
static uint32 * desDescriptorArray;	//Each descriptor have 16-byte
static uint32 desDescNum, curDesDescPos;
static int8 * desKey, *desIv;
static int8 ** desBatchKey, **desBatchIv;
//Counters for interrupt
static uint32 cryptoDoneIntCounter, cryptoAllDoneIntCounter;


int32 rtl8651b_cryptoEngine_alloc(uint32 descNum) 
{
	uint32 i;

	/* Before allocate new memory, we must free allocated memory. */
	rtl8651b_cryptoEngine_free();
	
	desDescNum = descNum;
	desDescPtrArray = (uint32 *)UNCACHED_MALLOC(descNum * sizeof(uint32));
	desDescriptorArray = (uint32 *)UNCACHED_MALLOC(descNum * 4 * sizeof(uint32));
	desKey = (int8 *)UNCACHED_MALLOC(24 * sizeof(int8));
	desIv = (int8 *)UNCACHED_MALLOC(8 * sizeof(int8));
	desBatchKey = (int8 **)UNCACHED_MALLOC(descNum * sizeof(int8 **));
	desBatchIv = (int8 **)UNCACHED_MALLOC(descNum * sizeof(int8 **));
	for(i=0; i<descNum; i++) {
		desDescPtrArray[i] = ((uint32)&desDescriptorArray[(i<<2)] ) & 0xfffffffc;//Clear last 2-bit
		desBatchKey[i] = (int8 *)UNCACHED_MALLOC(24 * sizeof(int8));
		desBatchIv[i] = (int8 *)UNCACHED_MALLOC(8 * sizeof(int8));
	}
	desDescPtrArray[descNum-1] |= DESC_WRAP;

	return SUCCESS;
}


int32 rtl8651b_cryptoEngine_init(uint32 descNum, int8 mode32Bytes) {

	rtl8651b_cryptoEngine_alloc(descNum);

	//Initial crypto-engine
	if(mode32Bytes == TRUE) {
		REG32(DESSCR) = 0x10400000;//Reset all descriptors, soft reset and Lexra bus burst size is 32 bytes
		REG32(DESSCR) = 0xd0000000;//Enable tx/rx and Lexra bus burst size is 32 bytes
	}
	else {
		REG32(DESSCR) = 0x00400000;//Reset all descriptors, soft reset and Lexra bus burst size is 16 bytes
		REG32(DESSCR) = 0xc0000000;//Enable tx/rx and Lexra bus burst size is 16 bytes
	}
	REG32(DESDCR) = (uint32)desDescPtrArray;

	REG32(DESEIMR) = 0;


	curDesDescPos = 0;

	return SUCCESS;
}


int32 rtl8651b_cryptoEngine_free( void )
{
	uint32 i;
	
	/* free pointer array before desBatchKey and desBatchIv are freed. */
	for(i=0; i<desDescNum; i++) 
	{
		if ( desBatchKey[i] )
		{
			UNCACHED_FREE( desBatchKey[i] );
			desBatchKey[i] = NULL;
		}
		if ( desBatchIv[i] )
		{
			UNCACHED_FREE( desBatchIv[i] );
			desBatchIv[i] = NULL;
		}
	}
	if ( desDescPtrArray )
	{
		UNCACHED_FREE( desDescPtrArray );
		desDescPtrArray = NULL; 
	}
	if ( desDescriptorArray )
	{
		UNCACHED_FREE( desDescriptorArray );
		desDescriptorArray = NULL; 
	}
	if ( desKey )
	{
		UNCACHED_FREE( desKey );
		desKey = NULL; 
	}
	if ( desIv )
	{
		UNCACHED_FREE( desIv );
		desIv = NULL; 
	}
	if ( desBatchKey )
	{
		UNCACHED_FREE( desBatchKey );
		desBatchKey = NULL; 
	}
	if ( desBatchIv )
	{
		UNCACHED_FREE( desBatchIv );
		desBatchIv = NULL; 
	}
	
	return SUCCESS;
}


int32 rtl8651b_cryptoEngine_exit( void )
{
	rtl8651b_cryptoEngine_free();
	
	return SUCCESS;
}


int32 rtl8651b_cryptoMemcpy(void *dest){
	uint32 destination= (uint32)dest & 0x0fffffff;
	WRITE_MEM32(TTLCR,(READ_MEM32(TTLCR)&0xf0000000)|((uint32)destination));
	//rtlglue_printf("DMA dest=%08x, ttlcr=%08x\n",(uint32) dest, (uint32)READ_MEM32(TTLCR)); 
	return 0;
}

//data, key and iv does not have 4-byte alignment limitatiuon
//mode bits:
// 0x01: 3DES
// 0x02: ECB
// 0x04: 
// 0x08: 
// 0x10: Generic DMA

 int32 rtl8651b_cryptoEngine_des(uint32 mode, int8 *data, uint32 len, int8 *key, int8 *iv ) {
	//rtlglue_printf("Mode: %x, Data: %08x, len:%d, Key:%08x, IV:%08x\n", mode, (uint32) data, len, (uint32) key, (uint32) iv);	
	if(len & 0x7)
		return FAILED;//Unable to process not 8-byte aligned data
	assert((desDescPtrArray[curDesDescPos] & DESC_OWNED_BIT) == DESC_RISC_OWNED);//Co-processor mode should guarantee function returned with RISC own descriptor
	desDescriptorArray[(curDesDescPos<<2)+0] = DES_RUNCIPHER | DES_DESCIV | (len & DES_DMA_LEN_MASK); //CTRL_LEN
	if(mode & DECRYPT_CBC_3DES) {
		desDescriptorArray[(curDesDescPos<<2)+0] |= DES_ALGO_3DES;
		if((mode &RTL8651_CRYPTO_GENERIC_DMA)==0)
			memcpy(desKey, key, 24);
	}
	else
		memcpy(desKey, key, 8);
	if(mode & DECRYPT_ECB_DES)
		desDescriptorArray[(curDesDescPos<<2)+0] |= DES_ECB;
	if((mode &RTL8651_CRYPTO_GENERIC_DMA)==0)
		memcpy(desIv, iv, 8);
	if(mode & ENCRYPT_CBC_DES)
		desDescriptorArray[(curDesDescPos<<2)+0] |= DES_ENCRYPTION;
	
	if(mode&RTL8651_CRYPTO_GENERIC_DMA){ //use crypto engine to do memcpy
		//rtlglue_printf("direct DMA\n");
		WRITE_MEM32(TTLCR,READ_MEM32(TTLCR)|(uint32)EN_MEM2MEM_DMA);
	}else{
		uint32 value;
		rtl8651b_cryptoMemcpy(data); //specify destination=source address
		value = READ_MEM32(TTLCR);
		value &=~EN_MEM2MEM_DMA;
		WRITE_MEM32(TTLCR,value);
	}
	desDescriptorArray[(curDesDescPos<<2)+1] = (uint32)data; //DMA_addr
	//assert(((uint32)desKey&0x3) == 0);
	desDescriptorArray[(curDesDescPos<<2)+2] = (uint32)desKey; //key_addr
	//assert(((uint32)desIv&0x3) == 0);
	desDescriptorArray[(curDesDescPos<<2)+3] = (uint32)desIv; //IV_addr is not used
	desDescPtrArray[curDesDescPos] |= DESC_ENG_OWNED;//Change owner to crypto-engine
#ifdef ISRMODEL
		{
			uint32 cryptoAllDoneIntPreCounter;
			cryptoAllDoneIntPreCounter = cryptoAllDoneIntCounter;
			REG32(DESEISR) = 0;//Clear interrupt status
			REG32(DESSCR) |= DES_FETCH; //Notify crypto-engine to get data to operate
			while((cryptoAllDoneIntCounter -cryptoAllDoneIntPreCounter)==0);
		}
#else
	REG32(DESEISR) = 0;//Clear interrupt status
	//rtlglue_printf("%02x trigger Crypto engine %08x=[%08x]\n", mode, (unsigned int)DESSCR, REG32(DESSCR) );
	REG32(DESSCR) |= DES_FETCH; //Notify crypto-engine to get data to operate
	//rtlglue_printf("ooo\n");	
	if(mode &RTL8651_CRYPTO_NON_BLOCKING) //non-blocking
		return SUCCESS;
	//rtlglue_printf("waiting...\n");
	while((REG32(DESEISR) & DES_OPALLDONE) != DES_OPALLDONE);//Wait until crypto-done
#endif
	while((desDescPtrArray[curDesDescPos] & DESC_OWNED_BIT) != DESC_RISC_OWNED);//Co-processor mode should guarantee function return with RISC own descriptor
	curDesDescPos = (curDesDescPos+1)%desDescNum;

	return SUCCESS;
}

 int32 rtl8651b_cryptoEngine_des_poll(int32 freq){
	uint32 poll=0, total=0;
	//if((REG32(DESEISR) & DES_OPALLDONE)== DES_OPALLDONE){
	//	if((desDescPtrArray[curDesDescPos] & DESC_OWNED_BIT) == DESC_RISC_OWNED){
	//		curDesDescPos = (curDesDescPos+1)%desDescNum;
	//		return SUCCESS;
	//	}
	//}
	while(1){
		poll++;
		if(poll>freq){
		  total+=poll;
		  if(((REG32(DESEISR) & DES_OPALLDONE) == DES_OPALLDONE)&&
		     ((desDescPtrArray[curDesDescPos] & DESC_OWNED_BIT) == DESC_RISC_OWNED))
		   	break;
		  poll=0;
		}
	}
	curDesDescPos = (curDesDescPos+1)%desDescNum;
	return -poll;
}
void rtl8651b_cryptoEngineGetIntCounter(uint32 * doneCounter, uint32 * allDoneCounter) {
	*doneCounter = cryptoDoneIntCounter;
	*allDoneCounter = cryptoAllDoneIntCounter;
}


