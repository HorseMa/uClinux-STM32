/*
* ----------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/crypto/865xb/Attic/cryptoCmd.c,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
*
* Abstract: Crypto engine access command driver source code.
* $Author: davidm $
* $Log: cryptoCmd.c,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.6  2005/02/17 05:29:34  yjlou
* +: support CBC_AES (CLE only)
*
* Revision 1.5  2004/12/06 09:25:31  cfliu
* +Add crypto engine FPGA memcpy test cmd
*
* Revision 1.4  2004/07/12 05:16:09  yjlou
* *: fixed the bug of getting byte count parameter of 'init' (crypto nd hash)
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
* Revision 1.10  2003/09/30 12:23:54  jzchen
* Add random test
*
* Revision 1.9  2003/09/29 09:04:51  jzchen
* Add authentication batch command
*
* Revision 1.8  2003/09/25 10:00:29  jzchen
* Add batch testing code
*
* Revision 1.7  2003/09/23 02:21:43  jzchen
* Change layout of crypto and authenticaiton engine command
*
* Revision 1.6  2003/09/18 14:33:36  jzchen
* Add throughput evaluation commands
*
* Revision 1.5  2003/09/09 02:52:14  jzchen
* Add random test to comare software HMAC and simulator HMAC
*
* Revision 1.4  2003/09/02 14:59:05  jzchen
* Execute HMAC test
*
* Revision 1.3  2003/09/01 02:51:09  jzchen
* Porting test code for MD5 and SHA-1. Test software code ready.
*
* Revision 1.2  2003/08/28 02:51:30  jzchen
* Add crypto engine comparision test
*
* Revision 1.1  2003/08/21 07:06:12  jzchen
* Using openssl des testing code to test software des function
*
*
* ---------------------------------------------------------------
*/

#include <rtl_types.h>
#include <rtl_cle.h> 
#include <cryptoCmd.h>
#include <authTest.h>
#include <rtl_glue.h>
#include <crypto.h>
#include "aes.h"
#include "aes_1.h"
#include "aes_locl.h"

#define BUFFER_LEN		1600
#undef SUPPORT_AES_1 /* AES_1 is a slower implementation of AES */

int32 _rtl8651b_cryptoCmd(uint32 userId,  int32 argc,int8 **saved) {
	int32 size;
	int8 *nextToken;
	
	cle_getNextCmdToken(&nextToken,&size,saved); 

	if(!strcmp(nextToken, "init")){
		uint32 descNum;
		uint32 BurstSize;
		
		cle_getNextCmdToken(&nextToken,&size,saved); 
		descNum = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		BurstSize = U32_value(nextToken);	
		if( BurstSize == 32 ) 
		{	
			rtlglue_printf("Initialize crypto engine driver with %u descriptors working on 32-byte mode\n", descNum);
			rtl8651b_cryptoEngine_init(descNum, TRUE);
		}
		else 
		{	
			rtlglue_printf("Initialize crypto engine driver with %u descriptors working on 16-byte mode\n", descNum);
			rtl8651b_cryptoEngine_init(descNum, FALSE);
		}
		return SUCCESS;
	}
	else if(!strcmp(nextToken, "sim")){
#if 0
		rtlglue_printf("Run des and des simulator test\n");
		destest();
#else
		rtlglue_printf("destest not included\n");
#endif
		return SUCCESS;
	}
	else if(!strcmp(nextToken, "rand")){
		uint32 randSeed, roundLimit;
		cle_getNextCmdToken(&nextToken,&size,saved); 
		randSeed = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		roundLimit = U32_value(nextToken);	
		rtlglue_printf("Run DES random test seed %u and %u rounds\n", randSeed, roundLimit);
		runDes8651bGeneralApiRandTest(randSeed, roundLimit);
		return SUCCESS;
	}
	else if(!strcmp(nextToken, "asic")){
		uint32 round, funStart, funEnd, lenStart, lenEnd, offsetStart, offsetEnd;
		rtlglue_printf("Run des simulator and 8651b crypto engine test\n");
		cle_getNextCmdToken(&nextToken,&size,saved); 
		round = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		funStart = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		funEnd = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		lenStart = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		lenEnd = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		offsetStart = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		offsetEnd = U32_value(nextToken);	
		rtlglue_printf("Round %u Function %u - %u Len %u - %u Offset %u - %u\n", round, funStart, funEnd, lenStart, lenEnd, offsetStart, offsetEnd);
//		des8651bTest(round, funStart, funEnd, lenStart, lenEnd, offsetStart, offsetEnd);
		runDes8651bGeneralApiTest(round, funStart, funEnd, lenStart, lenEnd, offsetStart, offsetEnd);
		return SUCCESS;
	}
	else if(!strcmp(nextToken, "throughput")) {
		uint32 round, startMode, endMode, pktLen;
		cle_getNextCmdToken(&nextToken,&size,saved); 
		 if(!strcmp(nextToken, "asic")) {
			cle_getNextCmdToken(&nextToken,&size,saved); 
			round = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			startMode = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			endMode = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			pktLen = U32_value(nextToken);	
		 	des8651bAsicThroughput(round, startMode, endMode, pktLen);
			return SUCCESS;
		 }
		 else if(!strcmp(nextToken, "sw")) {
			cle_getNextCmdToken(&nextToken,&size,saved); 
			round = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			startMode = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			endMode = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			pktLen = U32_value(nextToken);	
		 	des8651bSwThroughput(round, startMode, endMode, pktLen);
			return SUCCESS;
		 }
	}

	return FAILED;
}

int32 _rtl8651b_authenticationCmd(uint32 userId,  int32 argc,int8 **saved) {
	int32 size;
	int8 *nextToken;
	
	cle_getNextCmdToken(&nextToken,&size,saved); 

	if(!strcmp(nextToken, "init")){
		uint32 descNum;
		uint32 BurstSize;
		cle_getNextCmdToken(&nextToken,&size,saved); 
		descNum = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		BurstSize = U32_value(nextToken);	
		if(BurstSize == 32) 
		{	
			rtlglue_printf("Initialize authentication engine driver with %u descriptors working on 32-byte mode\n", descNum);
			rtl8651b_authEngine_init(descNum, TRUE);
		}
		else 
		{	
			rtlglue_printf("Initialize authentication engine driver with %u descriptors working on 16-byte mode\n", descNum);
			rtl8651b_authEngine_init(descNum, FALSE);
		}
		return SUCCESS;
	}
	else if(!strcmp(nextToken, "sim")){
		rtlglue_printf("Run MD5 and MD5 simulator test\n");
		md5test();
		rtlglue_printf("Run SHA-1 and SHA-1 simulator test\n");
		sha1test();
		rtlglue_printf("Run HMAC MD5 and HMAC MD5 simulator test\n");
		hmactest();
		rtlglue_printf("Run HMAC random test\n");
		hmacSimRandTest(2);
		return SUCCESS;
	}
	else if(!strcmp(nextToken, "rand")){
		uint32 randSeed, roundLimit;
		cle_getNextCmdToken(&nextToken,&size,saved); 
		randSeed = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		roundLimit = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		rtlglue_printf("Run %s random test seed %u for %u rounds\n",nextToken, randSeed, roundLimit);
		if(!strcmp(nextToken, "asic"))
			runAuth8651bGeneralApiRandTest(randSeed, roundLimit);
		else
			hmacSimRandTest(roundLimit);
		return SUCCESS;
	}
	else if(!strcmp(nextToken, "asic")) {
		uint32 round, funStart, funEnd, lenStart, lenEnd, keyLenStart, keyLenEnd, offsetStart, offsetEnd;
		rtlglue_printf("Run auth simulator and 8651b auth engine test\n");
		cle_getNextCmdToken(&nextToken,&size,saved); 
		round = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		funStart = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		funEnd = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		lenStart = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		lenEnd = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		keyLenStart = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		keyLenEnd = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		offsetStart = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		offsetEnd = U32_value(nextToken);	
		rtlglue_printf("Round %u Function %u - %u Len %u - %u keyLen %u - %u Offset %u - %u \n", round, funStart, funEnd, lenStart, lenEnd, keyLenStart, keyLenEnd, offsetStart, offsetEnd);
		runAuth8651bGeneralApiTest(round, funStart, funEnd, lenStart, lenEnd, keyLenStart, keyLenEnd, offsetStart, offsetEnd);
		return SUCCESS;
	}
	else if(!strcmp(nextToken, "throughput")) {
		uint32 round, startMode, endMode, pktLen;
		cle_getNextCmdToken(&nextToken,&size,saved); 
		 if(!strcmp(nextToken, "asic")) {
			cle_getNextCmdToken(&nextToken,&size,saved); 
			round = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			startMode = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			endMode = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			pktLen = U32_value(nextToken);	
		 	auth8651bAsicThroughput(round, startMode, endMode, pktLen);
			return SUCCESS;
		 }
		 else if(!strcmp(nextToken, "sw")) {
			cle_getNextCmdToken(&nextToken,&size,saved); 
			round = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			startMode = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			endMode = U32_value(nextToken);	
			cle_getNextCmdToken(&nextToken,&size,saved); 
			pktLen = U32_value(nextToken);	
		 	auth8651bSwThroughput(round, startMode, endMode, pktLen);
			return SUCCESS;
		 }
	}
	return FAILED;
}



typedef struct AES_test_pattern_s {
	int32	klen;
	uint8	Key[32];
	uint8	Input[16];
	uint8	cipher[16];
} AES_test_pattern_t;

uint8 *buffer1, *buffer2, *plainText;
//static uint8 buffer1[BUFFER_LEN], buffer2[BUFFER_LEN];
//static uint8 plainText[BUFFER_LEN];

static void AES_printit(AES_test_pattern_t *pt, uint8 *en, uint8 *de) {
	int32 n;

	rtlglue_printf("\nPlain Text: ");
	for(n=0; n<AES_BLOCK_SIZE; n++)
		rtlglue_printf("%02x", de[n]);
	rtlglue_printf("\nKey:        ");
	for(n=0; n<(pt->klen>>3); n++)
		rtlglue_printf("%02x", pt->Key[n]);
	rtlglue_printf("\nCipher:     ");
	for(n=0; n<AES_BLOCK_SIZE; n++)
		rtlglue_printf("%02x", en[n]);
	/* Make sure the correct result */	
	for(n=0; n<AES_BLOCK_SIZE; n++)
		if (pt->cipher[n] != en[n]) {
			rtlglue_printf("\nAES_ENCRYPT: failure!!!\n");
			return;
		}
	for(n=0; n<AES_BLOCK_SIZE; n++)
		if (pt->Input[n] != de[n]) {
			 rtlglue_printf("\nAES_DECRYPT: failure!!!\n");
			return;
		}
	rtlglue_printf("\nok......\n\n");
}



static void AES_timeTrial(int32 sel, int8 *mode, int32 keylen, int32 blkSize, int32 c) {
	//uint8 plainText[BUFFER_LEN];
	uint8 key[] =  { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b ,0x1c, 0x1d, 0x1e, 0x1f };
	uint8 ivec[16];
	uint8 *p, *p1;
	AES_KEY aesKey; /* for AES-0 */
	aes_encrypt_ctx aes_ctx;/* for AES-1 */
	uint32 p_time, l_time;
	uint32 enRate, deRate, encryptRate, decryptRate, encyptKeyExpand, decyptKeyExpand;
	uint32 en_dif, de_dif, dif;
	int32 i, n;

	buffer1 = (uint8 *)rtlglue_malloc(BUFFER_LEN);
	buffer2 = (uint8 *)rtlglue_malloc(BUFFER_LEN);
	plainText = (uint8 *)rtlglue_malloc(BUFFER_LEN);
	assert(buffer1 && buffer2 && plainText);
	
	if (blkSize > BUFFER_LEN || (blkSize%16)!=0)
		return;
	for(i=0; i<BUFFER_LEN; i++)
		plainText[i] = i&0xff;
	for(i=0; i<16; i++)
		ivec[i] = (((i*100 + 4)<<2) & 0xff);
	
	if(!strcmp(mode, "ebc")) {
#if 0	
		/* (1) Estimate the key expansion time for encryption */
		if (sel == 1) {
			rtlglue_getmstime(&p_time);
			for(i=0; i<10000; i++)
				AES_set_encrypt_key(key, keylen, &aesKey);
			rtlglue_getmstime(&l_time);
		} else {
			gen_tabs();
			rtlglue_getmstime(&p_time);
			for(i=0; i<10000; i++)
				aes_encrypt_key(key, keylen, &aes_ctx);
			rtlglue_getmstime(&l_time);
		}
		encyptKeyExpand = (l_time - p_time +1 );
#endif
		/* (2) Estimate Encryption time not include key expansion time */
		if (sel == 1) {
			AES_set_encrypt_key(key, keylen, &aesKey);
			memset(&(aesKey.rd_key), 0, 240);
			rtlglue_printf("Encrypt=>> round: %d\n", aesKey.rounds);
			rtlglue_getmstime(&p_time);
			for(i=0; i<50000; i++) 
				AES_ecb_encrypt(plainText, buffer1, &aesKey, AES_ENCRYPT);
			rtlglue_getmstime(&l_time);
		} else {
#ifdef SUPPORT_AES_1
			rtlglue_getmstime(&p_time);
			for(i=0; i<50000; i++)
				aes_encrypt(plainText, buffer1, &aes_ctx);
			rtlglue_getmstime(&l_time);
#endif
		}
		en_dif = l_time-p_time; 
		encryptRate = (50000*128)/(en_dif*10);

#if 0		
		/* (3) Estimate the key expansion time for decryption */
		if (sel == 1) {
			rtlglue_getmstime(&p_time);
			for(i=0; i<10000; i++)
				AES_set_decrypt_key(key, keylen, &aesKey);
			rtlglue_getmstime(&l_time);
		} else {
			rtlglue_getmstime(&p_time);
			for(i=0; i<10000; i++)
				aes_decrypt_key(key, keylen, &aes_ctx);
			rtlglue_getmstime(&l_time);
		}
		decyptKeyExpand = (l_time - p_time +1 );
#endif

		/* (4) Estimate Decryption time not include key expansion time */
		if (sel == 1) {
			AES_set_decrypt_key(key, keylen, &aesKey);
			memset(&(aesKey.rd_key), 0, 240);
			rtlglue_printf("Decrypt=>> round: %d\n", aesKey.rounds);
			rtlglue_getmstime(&p_time);
			for(i=0; i<50000; i++)
				AES_ecb_encrypt(plainText, buffer1, &aesKey, AES_DECRYPT);
			rtlglue_getmstime(&l_time);
		} else  {
#ifdef SUPPORT_AES_1
			rtlglue_getmstime(&p_time);
			for(i=0; i<50000; i++)
				aes_decrypt(plainText, buffer1, &aes_ctx);
			rtlglue_getmstime(&l_time);
#endif
		}
		de_dif = l_time - p_time;
		decryptRate = (50000*128)/(de_dif*10);

#if 0				
		/* (5) Estimate the time for AES encryption */
		if (sel == 1) {
			rtlglue_getmstime(&p_time);
			for(i=0; i<c; i++) {
				AES_set_encrypt_key(key, keylen, &aesKey);
				for(n=blkSize>>4,p=plainText, p1=buffer1; n>=1; n--, p+=16, p1+=16)
					AES_ecb_encrypt(p, p1, &aesKey, AES_ENCRYPT);
			}
			rtlglue_getmstime(&l_time);
		} else {
			rtlglue_getmstime(&p_time);
			for(i=0; i<c; i++) {
				aes_encrypt_key(key, keylen, &aes_ctx);
				for(n=blkSize>>4,p=plainText, p1=buffer1; n>=1; n--, p+=16, p1+=16)
					aes_encrypt(p, p1, &aes_ctx);
			}
			rtlglue_getmstime(&l_time);
		}
		dif = l_time - p_time;
		enRate = (c*blkSize*8)/(dif*10);
		
		/* (6) Estimate the time for AES decryption */
		if (sel == 1) {
			rtlglue_getmstime(&p_time);
			for(i=0; i<c; i++) {
				AES_set_decrypt_key(key, keylen, &aesKey);
				for(n=blkSize>>4,p=buffer1, p1=buffer2; n>=1; n--, p+=16, p1+=16) 
					AES_ecb_encrypt(p, p1, &aesKey, AES_DECRYPT);
			}
			rtlglue_getmstime(&l_time);
		} else {
			rtlglue_getmstime(&p_time);
			for(i=0; i<c; i++) {
				aes_decrypt_key(key, keylen, &aes_ctx);
				for(n=blkSize>>4,p=buffer1, p1=buffer2; n>=1; n--, p+=16, p1+=16) 
					aes_decrypt(p, p1, &aes_ctx);
			}
			rtlglue_getmstime(&l_time);
		}
		dif = l_time - p_time;
		deRate = (c*blkSize*8)/(dif*10);	
#endif		
		rtlglue_printf("Encrypt Only: %8u Kbit(s)/sec, %4u(ms)\n", encryptRate, en_dif*10);
		rtlglue_printf("Decrypt Only: %8u Kbit(s)/sec, %4u(ms)\n", decryptRate, de_dif*10);
		rtlglue_printf("Encryption Key Expansion : Decryption Key Expansion <=> %u : %u\n\n", encyptKeyExpand , decyptKeyExpand);

	} else if (!strcmp(mode, "cbc")) {
		/* CBC Mode encryption */
		if (sel == 1) {
			rtlglue_getmstime(&p_time);
			for(i=0; i<c; i++) {
				AES_set_encrypt_key(key, keylen, &aesKey);
				AES_cbc_encrypt(plainText, buffer1, blkSize, &aesKey, ivec, AES_ENCRYPT);
			}
			rtlglue_getmstime(&l_time);
		} else {
#ifdef SUPPORT_AES_1
			rtlglue_getmstime(&p_time);
			for(i=0; i<c; i++) {
				aes_encrypt_key(key, keylen, &aes_ctx);
				aes_cbc_encrypt(plainText, buffer1, blkSize, &aes_ctx, ivec, AES_ENCRYPT);
			}
			rtlglue_getmstime(&l_time);
#endif
		}
		dif = l_time - p_time;
		enRate = (c*blkSize*8)/(dif*10);
		
		for(i=0; i<16; i++)
			ivec[i] = (((i*100 + 4)<<2) & 0xff);
		/* CBC Mode decryption */
		if (sel == 1) {
			rtlglue_getmstime(&p_time);
			for(i=0; i<c; i++) {
				AES_set_decrypt_key(key, keylen, &aesKey);
				AES_cbc_encrypt(buffer1, buffer2, blkSize, &aesKey, ivec, AES_DECRYPT);
			}		
			rtlglue_getmstime(&l_time);
		} else {
#ifdef SUPPORT_AES_1
			rtlglue_getmstime(&p_time);
			for(i=0; i<c; i++) {
				aes_decrypt_key(key, keylen, &aes_ctx);
				aes_cbc_encrypt(plainText, buffer1, blkSize, &aes_ctx, ivec, AES_DECRYPT);
			}
			rtlglue_getmstime(&l_time);
#endif
		}
		dif = l_time - p_time;
		deRate = (c*blkSize*8)/(dif* 10);
		
		/* gen correct results */
		for(i=0; i<16; i++)
			ivec[i] = (((i*100 + 4)<<2) & 0xff);
		if (sel == 1) {
			AES_set_encrypt_key(key, keylen, &aesKey);
			AES_cbc_encrypt(plainText, buffer1, blkSize, &aesKey, ivec, AES_ENCRYPT);
		} else {
#ifdef SUPPORT_AES_1
			aes_encrypt_key(key, keylen, &aes_ctx);
			aes_cbc_encrypt(plainText, buffer1, blkSize, &aes_ctx, ivec, AES_ENCRYPT);
#endif
		}
		for(i=0; i<16; i++)
			ivec[i] = (((i*100 + 4)<<2) & 0xff);
		if (sel == 1) {
			AES_set_decrypt_key(key, keylen, &aesKey);
			AES_cbc_encrypt(buffer1, buffer2, blkSize, &aesKey, ivec, AES_DECRYPT);
		} else {
#ifdef SUPPORT_AES_1
			aes_decrypt_key(key, keylen, &aes_ctx);
			aes_cbc_encrypt(buffer1, buffer2, blkSize, &aes_ctx, ivec, AES_DECRYPT);
#endif
		}
		
	}
	rtlglue_free(buffer1); rtlglue_free(buffer2); rtlglue_free(plainText);
	rtlglue_printf("Encryption: %8u Kbit(s)/sec\n", enRate);
	rtlglue_printf("Decryption: %8u Kbit(s)/sec\n", deRate);
	for(i=0; i<blkSize; i++) {
		if (buffer2[i] != plainText[i]) {
			rtlglue_printf("failure......\n");
			return;
		}
	}
}



static int32 _crypto_aesThoughput() {
	int32 bksize, keylen;

	for(keylen=192; keylen<=256; keylen+=64) {
		for(bksize=16; bksize<1600; bksize+=48) {
			rtlglue_printf("%d-bit key length, %d-byte block size:\n", keylen, bksize);
			AES_timeTrial(1, "cbc", keylen, bksize, 20000);
			rtlglue_printf("\n");
		}
	}
}


static int32 _crypto_aesTestCmd(uint32 userId, int32 argc, int8 **saved) {
	int8 *nextToken, sel = 0;
	int32 size, i;
	static AES_KEY aesKey; /* for AES-0 */
	static aes_encrypt_ctx aes_ctx;/* for AES-1 */
	static AES_test_pattern_t AES_pattern[] = 
{
	/* Testing Patterns from "A Specification for The AES Algorithm" */
	{ 	128, 
		{ 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
		{ 0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d, 0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34 },
		{ 0x39, 0x25, 0x84, 0x1d, 0x02, 0xdc, 0x09, 0xfb, 0xdc, 0x11, 0x85, 0x97, 0x19, 0x6a, 0x0b, 0x32 }
	},
	{ 	192, 
		{ 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c, 0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5 },
		{ 0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d, 0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34 },
		{ 0xf9, 0xfb, 0x29, 0xae, 0xfc, 0x38, 0x4a, 0x25, 0x03, 0x40, 0xd8, 0x33, 0xb8, 0x7e, 0xbc, 0x00 }
	},
	{ 	256, 
		{ 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c, 0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5, 0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe },
		{ 0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d, 0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34 },
		{ 0x1a, 0x6e, 0x6c, 0x2c, 0x66, 0x2e, 0x7d, 0xa6, 0x50, 0x1f, 0xfb, 0x62, 0xbc, 0x9e, 0x93, 0xf3 }
	},

	/* Testing Patterns from chhuang */
	{	128,
		{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
		{ 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff },
		{ 0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30, 0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a }
	},
	{	192,
		{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 },
		{ 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff },
		{ 0xdd, 0xa9, 0x7c, 0xa4, 0x86, 0x4c, 0xdf, 0xe0, 0x6e, 0xaf, 0x70, 0xa0, 0xec, 0x0d, 0x71, 0x91 }
	},
	{	256,
		{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b ,0x1c, 0x1d, 0x1e, 0x1f },
		{ 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff },
		{ 0x8e, 0xa2, 0xb7, 0xca, 0x51, 0x67, 0x45, 0xbf, 0xea, 0xfc, 0x49, 0x90, 0x4b, 0x49, 0x60, 0x89 },
	},
	
	/* Testing Patterns Generated by Dr. Brian Gladman */
	


	
};

#define NUM_PATTERN		(sizeof(AES_pattern)/sizeof(struct AES_test_pattern_s))
	

	cle_getNextCmdToken(&nextToken,&size,saved); 
	if (!strcmp("aes-0", nextToken))
		sel = 1;
	else if (!strcmp("aes-1", nextToken))
		sel = 2;
	else return FAILED;

	cle_getNextCmdToken(&nextToken,&size,saved); 
	if (!strcmp("verify", nextToken)) {

		buffer1 = (uint8 *)rtlglue_malloc(BUFFER_LEN);
		buffer2 = (uint8 *)rtlglue_malloc(BUFFER_LEN);
		plainText = (uint8 *)rtlglue_malloc(BUFFER_LEN);
		assert(buffer1 && buffer2 && plainText);
	
		for(i=0; i<NUM_PATTERN; i++) {
			if (sel == 1) {
				AES_set_encrypt_key(AES_pattern[i].Key, AES_pattern[i].klen, &aesKey);
				AES_ecb_encrypt(AES_pattern[i].Input, buffer1, &aesKey, AES_ENCRYPT);
				AES_set_decrypt_key(AES_pattern[i].Key, AES_pattern[i].klen, &aesKey);
				AES_ecb_encrypt(buffer1, buffer2, &aesKey, AES_DECRYPT);
			} else if (sel == 2) {
#ifdef SUPPORT_AES_1
				gen_tabs();
				aes_encrypt_key(AES_pattern[i].Key, AES_pattern[i].klen, &aes_ctx);
				aes_encrypt(AES_pattern[i].Input, buffer1, &aes_ctx);
				aes_decrypt_key(AES_pattern[i].Key, AES_pattern[i].klen, &aes_ctx);
				aes_decrypt(buffer1, buffer2, &aes_ctx);
#endif
			}
			AES_printit(&AES_pattern[i], buffer1, buffer2);			
		}		

		rtlglue_free(buffer1); rtlglue_free(buffer2); rtlglue_free(plainText);

	} else if (!strcmp("time-trial", nextToken)) {
		int32 blkSize, repeat, keylen;
		int8 mode[4] = { 0x00, 0x00, 0x00, 0x00 };

		cle_getNextCmdToken(&nextToken,&size,saved); 
		mode[0] = nextToken[0]; mode[1] = nextToken[1]; mode[2] = nextToken[2];
		cle_getNextCmdToken(&nextToken,&size,saved); 
		keylen = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		blkSize = U32_value(nextToken);	
		cle_getNextCmdToken(&nextToken,&size,saved); 
		repeat = U32_value(nextToken);	
		AES_timeTrial(sel, mode, keylen, blkSize, repeat);
	} else if (!strcmp("throughput", nextToken)) {
		_crypto_aesThoughput();
	}
	
	//print_aes_table();	
	return SUCCESS;

	

}


cle_exec_t rtl865x_crypt_cmds[] = 
{
	{   "crypto",
		"Crypto engine commands",
		" { init %d'descNum' %d'burst-size' | sim |"
		"   rand %d'seed' %d'round' |"
		"   asic %d'round' %d'funStart' %d'funEnd' %d'lenStart' %d'lenEnd' %d'offsetStart' %d'offsetEnd' |"
		"   batch %d'round' %d'funStart' %d'funEnd' %d'lenStart' %d'lenEnd' %d'offsetStart' %d'offsetEnd' %d'batchStart' %d'batchEnd' |"
		"   throughput { asic | sw } %d'round' %d'StartMode' %d'EndMode' %d'PktLen' }",
		_rtl8651b_cryptoCmd,
		CLE_USECISCOCMDPARSER,
		NULL,
		NULL
	},
	{   "auth",
		"Auth engine commands",
		" { init %d'descNum' | sim |"
		"   rand %d'seed' %d'round' { asic | sw } |"
		"   asic %d'round' %d'funStart' %d'funEnd' %d'lenStart' %d'lenEnd' %d'keyStart' %d'keyEnd' %d'offsetStart' %d'offsetEnd' |"
		"   batch %d'round' %d'funStart' %d'funEnd' %d'lenStart' %d'lenEnd' %d'keyStart' %d'keyEnd' %d'offsetStart' %d'offsetEnd' %d'batchStart' %d'batchEnd' |"
		"   throughput { asic | sw } %d'round' %d'StartMode' %d'EndMode' %d'PktLen' }",
		_rtl8651b_authenticationCmd,
		CLE_USECISCOCMDPARSER,
		NULL,
		NULL
	},
	{	"aes",
		"AES algorithm.",
		" { { aes-0'AES implemented by openSSL.' | aes-1'AES implemented by Brain Gladman.' } { verify'Testing pattern' | throughput'Throughput' | time-trial'Performance measure.'  { ebc | cbc } %d'Key length' %d'block size(byte)' %d'repeat count' } ",
		_crypto_aesTestCmd,
		CLE_USECISCOCMDPARSER,
		NULL,
		NULL
	},
};
