/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : RTL8651B authentication engine testing code
* Abstract : Compare the calculation result of authentication simulator and rtl8651b authentication engine
* $Id: authTest.c,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: authTest.c,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.5  2005/09/09 05:54:48  yjlou
* *: chnage printfByPolling() to rtlglue_printf().
*
* Revision 1.4  2004/06/23 15:36:12  yjlou
* *: removed duplicated code, and implemented original functions with the generic function:
*    rtl8651b_authEngine_md5(), rtl8651b_authEngine_sha1(),
*    rtl8651b_authEngine_hmacMd5(), rtl8651b_authEngine_hmacSha1(),
*    rtl8651b_cryptoEngine_ecb_encrypt(), rtl8651b_cryptoEngine_3des_ecb_encrypt(),
*    rtl8651b_cryptoEngine_cbc_encrypt(), rtl8651b_cryptoEngine_3des_cbc_encrypt()
* -: remove rtl8651b_cryptoEngine_cbc_encryptEmbIV() and rtl8651b_cryptoEngine_ede_cbc_encryptEmbIV()
*
* Revision 1.3  2004/06/23 14:07:45  yjlou
* *: fixed the measured time unit: diag code is 10ms; however, rtlglue_getmstime() is 1ms.
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
* Revision 1.7  2004/03/31 02:33:01  yjlou
* *: Type of SysUpTime has been changed from uint64 to uint32.
*
* Revision 1.6  2003/09/30 12:29:25  jzchen
* Change display type of random test
*
* Revision 1.5  2003/09/30 12:24:07  jzchen
* Add random test
*
* Revision 1.4  2003/09/29 09:05:19  jzchen
* Add authentication batch testing code
*
* Revision 1.3  2003/09/23 02:22:09  jzchen
* 1. Add throughput estimation command. 2. Test generic API
*
* Revision 1.2  2003/09/18 14:33:26  jzchen
* Add throughput evaluation function
*
* Revision 1.1  2003/09/17 11:33:09  jzchen
* Add authentication testing code
*
*
* --------------------------------------------------------------------
*/

#include <authTest.h>
#include <authSim.h>
#include <rtl8651b_authEngine.h>
#include <asicRegs.h>
#include <md5.h>	//For SW MD5
#include <sha1.h>	//For SW SHA1
#include <hmac.h>	//For SW HMAC
#include <rtl_glue.h>
#include <assert.h>

static uint8 *asicTestData, asicTestKey[1024];
static uint8 asicDigest[RTL8651B_SHA1_DIGEST_LENGTH], simDigest[RTL8651B_SHA1_DIGEST_LENGTH], swDigest[RTL8651B_SHA1_DIGEST_LENGTH];

void triggerGpio(void)	{
	REG32(0xbd01200c) &= 0x0fffffff;
	REG32(0xbd012010) |= 0xf0000000;
	REG32(0xbd012014) &= 0x0fffffff;
	REG32(0xbd012014) |= 0xf0000000;
}

static int8 nullPattern[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static void displayDigestMismatch(int8 * title, uint8 * data, uint32 dataLen, uint8 * key, uint32 keyLen, 
				int8 * digest1Title, uint8 * digest1, int8 *digest2Title, uint8 * digest2, uint32 digestLen) {
	uint32 i;
	
	rtlglue_printf("\n%s Data length %u Key length %u\n%s Digest:\n", title, dataLen, keyLen, digest1Title);
	for(i=0; i<digestLen; i++) {
		rtlglue_printf("%02x ", digest1[i]&0xFF);
		if(i%8==7 || i==(digestLen-1))
			rtlglue_printf("\n");
	}
	rtlglue_printf("%s Digest:\n", digest2Title);
	for(i=0; i<digestLen; i++) {
		rtlglue_printf("%02x ", digest2[i]&0xFF);
		if(i%8==7 || i==(digestLen-1))
			rtlglue_printf("\n");
	}
}

int32 authTest_hashTest(void) {
	uint32 i, dataLen;
	
	for(i=0; i<2048; i++)
		asicTestData[i] = 0xFF;//(uint8)taurand(256);

	for(dataLen = 8; dataLen<2048; dataLen++) {
		rtlglue_printf("\r Hashing 0xFF pattern data length %4u", dataLen);
		rtl8651b_authEngine_md5(asicTestData, dataLen, asicDigest);
		authSim_md5(asicTestData, dataLen, simDigest);
		if(memcmp(asicDigest, simDigest, RTL8651B_MD5_DIGEST_LENGTH)) {
			displayDigestMismatch("Hash MD5 Digest mismatch", asicTestData, dataLen, NULL, 0, "ASIC", asicDigest, "SIM", simDigest, RTL8651B_MD5_DIGEST_LENGTH);
			if(memcmp(asicDigest, nullPattern, RTL8651B_MD5_DIGEST_LENGTH) != 0)
				triggerGpio();
			break;
		}
	}
	for(dataLen = 8; dataLen<2048; dataLen++) {
		rtlglue_printf("\r Hashing 0xFF pattern data length %4u", dataLen);
		rtl8651b_authEngine_sha1(asicTestData, dataLen, asicDigest);
		authSim_sha1(asicTestData, dataLen, simDigest);
		if(memcmp(asicDigest, simDigest, RTL8651B_SHA1_DIGEST_LENGTH)) {
			displayDigestMismatch("Hash SHA1 Digest mismatch", asicTestData, dataLen, NULL, 0, "ASIC", asicDigest, "SIM", simDigest, RTL8651B_SHA1_DIGEST_LENGTH);
			if(memcmp(asicDigest, nullPattern, RTL8651B_SHA1_DIGEST_LENGTH) != 0)
				triggerGpio();
			break;
		}
	}
	rtlglue_printf("\nHash SHA-1 ASIC test PASS\n");
	
	return SUCCESS;
}
//	tauset(58, 19);
//	taurand(2040);
int32 authTest_hmacTest(void) {
	uint32 i, dataLen, keyLen;
	
	dataLen = 2048;
	keyLen = 8;
	for(i=0; i<2048; i++)
		asicTestData[i] = 0xFF;//(uint8)taurand(256);
	for(i=0; i<keyLen; i++)
		asicTestKey[i] = 0x01;//(uint8)taurand(256);
	for(dataLen = 8; dataLen<2048; dataLen++) {	
		rtlglue_printf("\r HMAC MD5 0xFF pattern data length %4u", dataLen);
		rtl8651b_authEngine_hmacMd5(asicTestData, dataLen, asicTestKey, keyLen, asicDigest);
		authSim_hmacMd5(asicTestData, dataLen, asicTestKey, keyLen, simDigest);
		if(memcmp(asicDigest, simDigest, RTL8651B_MD5_DIGEST_LENGTH)) {
			displayDigestMismatch("HMAC MD5 Digest mismatch", asicTestData, dataLen, asicTestKey, keyLen, "ASIC", asicDigest, "SIM", simDigest, RTL8651B_MD5_DIGEST_LENGTH);
			if(memcmp(asicDigest, nullPattern, RTL8651B_MD5_DIGEST_LENGTH) != 0)
				triggerGpio();
			break;
		}
	}
	for(dataLen = 8; dataLen<2048; dataLen++) {
		rtlglue_printf("\r HMAC SHA-1 0xFF pattern data length %4u", dataLen);
		rtl8651b_authEngine_hmacSha1(asicTestData, dataLen, asicTestKey, keyLen, asicDigest);
		authSim_hmacSha1(asicTestData, dataLen, asicTestKey, keyLen, simDigest);
		if(memcmp(asicDigest, simDigest, RTL8651B_SHA1_DIGEST_LENGTH)) {
			displayDigestMismatch("HMAC SHA1 Digest mismatch", asicTestData, dataLen, asicTestKey, keyLen, "ASIC", asicDigest, "SIM", simDigest, RTL8651B_SHA1_DIGEST_LENGTH);
			if(memcmp(asicDigest, nullPattern, RTL8651B_SHA1_DIGEST_LENGTH) != 0)
				triggerGpio();
			break;
		}
	}
	return SUCCESS;
}

static int8 * authModeString[] = {
	"HASH_MD5 ",
	"HASH_SHA1",
	"HMAC_MD5 ",
	"HMAC_SHA1",
};

int32 auth8651bAsicThroughput(uint32 round, uint32 startMode, uint32 endMode, uint32 pktLen) {
	uint32 testRound, modeIdx, i, keyLen, bps;
	uint32 sTime, eTime;

	if(endMode> 3)
		endMode = 3;
	if(startMode>3)
		startMode = 3;
	
	asicTestData = (uint8 *)UNCACHED_MALLOC(4352*sizeof(uint32));
	keyLen = 8;
	for(i=0; i<pktLen; i++)
		asicTestData[i] = 0xFF;//(uint8)taurand(256);
	for(i=0; i<keyLen; i++)
		asicTestKey[i] = 0x01;//(uint8)taurand(256);
	rtlglue_printf("Evaluate 8651b throughput\n");
	for(modeIdx=startMode;modeIdx<=endMode;modeIdx++) {
		rtlglue_getmstime(&sTime);
		for(testRound=0; testRound<=round; testRound++) 
		{
			if ( rtl8651b_authEngine(modeIdx,  asicTestData, pktLen, asicTestKey, keyLen, asicDigest) != SUCCESS )
			{
				rtlglue_printf("testRound=%d, rtl8651b_authEngine(modeIdx=%d) failed...\n", testRound, modeIdx );
				return FAILED;
			}
		}
		rtlglue_getmstime(&eTime);

		if ( eTime - sTime == 0 )
		{
			rtlglue_printf("round is too small to measure throughput, try larger round number!\n");
			return FAILED;
		}
		else
		{
			bps = pktLen*8*1000/((uint32)(eTime - sTime))*round;
			if(bps>1000000)
				rtlglue_printf("%s round %u len %u time %u throughput %u.%02u mbps\n", authModeString[modeIdx], round, pktLen, (uint32)(eTime - sTime), bps/1000000, (bps%1000000)/10000);
			else if(bps>1000)
				rtlglue_printf("%s round %u len %u time %u throughput %u.%02u kbps\n", authModeString[modeIdx], round, pktLen, (uint32)(eTime - sTime), bps/1000, (bps%1000)/10);
			else
				rtlglue_printf("%s round %u len %u time %u throughput %u bps\n", authModeString[modeIdx], round, pktLen, (uint32)(eTime - sTime), bps);			
		}
	}

	UNCACHED_FREE(asicTestData);
	
	return SUCCESS;
}

int32 auth8651bSwThroughput(uint32 round, uint32 startMode, uint32 endMode, uint32 pktLen) {
	uint32 testRound, modeIdx, i, keyLen, bps;
	uint32 sTime, eTime;

	if(endMode> 3)
		endMode = 3;
	if(startMode>3)
		startMode = 3;
	
	asicTestData = (uint8 *)UNCACHED_MALLOC(4352*sizeof(uint32));
	keyLen = 8;
	for(i=0; i<pktLen; i++)
		asicTestData[i] = 0xFF;//(uint8)taurand(256);
	for(i=0; i<keyLen; i++)
		asicTestKey[i] = 0x01;//(uint8)taurand(256);
	rtlglue_printf("Evaluate software simulation throughput\n");
	for(modeIdx=startMode;modeIdx<=endMode;modeIdx++) {
		rtlglue_getmstime(&sTime);
		for(testRound=0; testRound<=round; testRound++) 
		{
			if ( authSim(modeIdx,  asicTestData, pktLen, asicTestKey, keyLen, asicDigest) != SUCCESS )
			{
				rtlglue_printf("testRound=%d, authSim(modeIdx=%d) failed...\n", testRound, modeIdx );
				return FAILED;
			}
		}
		rtlglue_getmstime(&eTime);

		if ( eTime - sTime == 0 )
		{
			rtlglue_printf("round is too small to measure throughput, try larger round number!\n");
			return FAILED;
		}
		else
		{
			bps = pktLen*8*1000/((uint32)(eTime - sTime))*round;
			if(bps>1000000)
				rtlglue_printf("%s round %u len %u time %u throughput %u.%02u mbps\n", authModeString[modeIdx], round, pktLen, (uint32)(eTime - sTime), bps/1000000, (bps%1000000)/10000);
			else if(bps>1000)
				rtlglue_printf("%s round %u len %u time %u throughput %u.%02u kbps\n", authModeString[modeIdx], round, pktLen, (uint32)(eTime - sTime), bps/1000, (bps%1000)/10);
			else
				rtlglue_printf("%s round %u len %u time %u throughput %u bps\n", authModeString[modeIdx], round, pktLen, (uint32)(eTime - sTime), bps);			
		}
	}
	UNCACHED_FREE(asicTestData);
	return SUCCESS;
}

static int32 auth8651bGeneralApiTestItem(uint32 mode, uint8 * input, uint32 pktLen, uint8 * key, uint32 keyLen) {
	MD5_CTX md5Context;
	SHA1_CTX sha1Context;

	if(rtl8651b_authEngine(mode, input, pktLen, key, keyLen, asicDigest) == FAILED)
		return FAILED;//When authentication engine cannot calculate digest, the testing is surely failed
	authSim(mode, input, pktLen, key, keyLen, simDigest);
	if(memcmp(simDigest, asicDigest, (mode&0x1)? RTL8651B_SHA1_DIGEST_LENGTH: RTL8651B_MD5_DIGEST_LENGTH) != 0) {
		switch(mode) {
			case HASH_MD5:
				MD5Init(&md5Context);
				MD5Update(&md5Context, input, pktLen);
				MD5Final(swDigest, &md5Context);
			break;
			case HASH_SHA1:
				SHA1Init(&sha1Context);
				SHA1Update(&sha1Context, input, pktLen);
				SHA1Final(swDigest, &sha1Context);
			break;
			case HMAC_MD5:
				HMACMD5(input, pktLen, key, keyLen, swDigest);
			break;
			case HMAC_SHA1:
				HMACSHA1(input, pktLen, key, keyLen, swDigest);
			break;
		}
		if(memcmp(simDigest, swDigest, (mode&0x1)? RTL8651B_SHA1_DIGEST_LENGTH: RTL8651B_MD5_DIGEST_LENGTH) != 0)
			displayDigestMismatch(authModeString[mode], input, pktLen, key, keyLen, "SW", swDigest, "SIM", simDigest, (mode&0x1)? RTL8651B_SHA1_DIGEST_LENGTH: RTL8651B_MD5_DIGEST_LENGTH);
		else	 {
			displayDigestMismatch(authModeString[mode], input, pktLen, key, keyLen, "SIM", simDigest, "ASIC", asicDigest, (mode&0x1)? RTL8651B_SHA1_DIGEST_LENGTH: RTL8651B_MD5_DIGEST_LENGTH);
			triggerGpio();
			//rtl8651b_authEngineData(mode, input, pktLen, key, keyLen);
		}
		return FAILED;
	}
	return SUCCESS;	
}

int32 runAuth8651bGeneralApiTest(uint32 round, uint32 funStart, uint32 funEnd, uint32 lenStart, uint32 lenEnd, uint32 keyLenStart, uint32 keyLenEnd, uint32 offsetStart, uint32 offsetEnd) {
	uint32 i, j, pktLen, keyLen, roundIdx, errCount, offset;
	uint32 doneCounter, allDoneCounter;

	asicTestData = (uint8 *)UNCACHED_MALLOC(4352*sizeof(uint32));
	keyLen = 8;
	for(i=0; i<keyLen; i++)
		asicTestKey[i] = 0x01;//(uint8)taurand(256);
	errCount=0;
	for(roundIdx=0; roundIdx<round; roundIdx++)	
		for(i=funStart; i<=funEnd; i++)
			for(pktLen=lenStart; pktLen<=lenEnd; pktLen+=8) 
				for(offset=offsetStart; offset<=offsetEnd; offset++) {
					for(j=0; j<pktLen; j++)
						asicTestData[j+offset] = (j&0xFF);//(uint8)taurand(256);
					rtl8651b_authEngineGetIntCounter(&doneCounter, &allDoneCounter);
					rtlglue_printf("\r[%05u, %03u]Mode: %s Packet length %04u Offset %u (%u, %u)", roundIdx, errCount, authModeString[i], pktLen, offset, doneCounter, allDoneCounter);
					if(auth8651bGeneralApiTestItem(i, &asicTestData[offset], pktLen, asicTestKey, keyLen) == FAILED) {
						errCount++;
						break;
					}
			}
	UNCACHED_FREE(asicTestData);
	return SUCCESS;
}

int32 runAuth8651bGeneralApiRandTest(uint32 seed, uint32 round) {
	uint32 i, pktLen, keyLen, roundIdx, modeIdx, errCount, offset;
	uint32 doneCounter, allDoneCounter;

	tauset(58, (int32)seed);
	asicTestData = (uint8 *)UNCACHED_MALLOC(4352*sizeof(uint32));
	errCount = 0;
	for(roundIdx=0; roundIdx<round; roundIdx++)	{
		keyLen = taurand(1024)&0x3FF;
		for(i=0; i<keyLen; i++)
			asicTestKey[i] = taurand(256)&0xFF;
		modeIdx = taurand(4)&0x3;
		pktLen = taurand(4024);	
		offset = taurand(4);
		for(i=0; i<pktLen; i++)
			asicTestData[i+offset] = taurand(256)&0xFF;
		rtl8651b_authEngineGetIntCounter(&doneCounter, &allDoneCounter);
		rtlglue_printf("\r[%05u, %03u]Mode: %s Packet length %04u Offset %u (%u, %u)", roundIdx, errCount, authModeString[modeIdx], pktLen, offset, doneCounter, allDoneCounter);
		if(auth8651bGeneralApiTestItem(modeIdx, &asicTestData[offset], pktLen, asicTestKey, keyLen) == FAILED)
			errCount++;
	}
	UNCACHED_FREE(asicTestData);
	return SUCCESS;
}

static uint8 **asicBatchTestData, **asicBatchTestDataPtr, **asicBatchTestKey;
static uint8 **asicBatchDigest, **simBatchDigest;
static uint32 *asicBatchDataLen, *asicBatchKeyLen;

