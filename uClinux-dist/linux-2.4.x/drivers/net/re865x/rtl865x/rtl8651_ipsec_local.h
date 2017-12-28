/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : IPSec-related header file for local include
* Abstract : 
* Author : Louis Yung-Chieh Lo (yjlou@realtek.com.tw)               
* $Id: rtl8651_ipsec_local.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
*/

#ifndef RTL8651_IPSEC_LOCAL_H
#define RTL8651_IPSEC_LOCAL_H

#include "rtl_queue.h"
#include "rtl8651_ipsec.h"
#include "rtl8651b_authEngine.h"
#include "rtl8651b_cryptoEngine.h"

#define RTL8651_MAX_IPSEC_KEY_LENGTH 32
#define MAX_IPSEC_FLOW 8
#define MAX_WINDOW_SIZE 512


/* forwarding declaration */
typedef struct ipsecSpi_s ipsecSpi_t;
typedef struct ipsecSpiGrp_s ipsecSpiGrp_t;
typedef struct ipsecSpiGrpSA_s ipsecSpiGrpSA_t;
typedef struct ipsecERoute_s ipsecERoute_t;
typedef struct ipsecEntryList_s ipsecEntryList_t;
typedef struct ipsecCounter_s ipsecCounter_t;
typedef struct ipsecSpiPort_s ipsecSpiPort_t;

struct ipsecCounter_s
{
	int32 cntSuccess;
	int32 cntInvalidEroute;
	int32 cntRouteFailed;
	int32 cntErrorSeq;
	int32 cntDupSeq;
	int32 cntErrorAuth;
	int32 cntCryptoEngineError;
	int32 cntAuthEngineError;
	int32 cntSourceAddressNotMatched;
	int32 cntFragError;
	int32 cntNotAnESPPacket;
};

struct ipsecSpiPort_s
{
	uint16 sPort;
	uint16 dPort;
};

struct ipsecSpi_s
{
	uint32 valid:1;
	uint32 outbound:1; /* inbound or outbound spi */
	uint32 authDataPresent:1;
	uint32 IHaveAntiReplay:1;
	uint32 natTraveralPresent:1;
	uint32 ipCompIndex:2;

	/* primary key */
	ipaddr_t edstIp;
	uint32 spi;
	uint32 proto:2;
	
	ipaddr_t srcIp;
	ipaddr_t dstIp; /* for IPIP only */

	/* for ESP only */
	uint8 ipad[RTL8651B_MAX_MD_CBLOCK]; /* we assist the next field is opad !!! */
	uint8 opad[RTL8651B_MAX_MD_CBLOCK]; /* we assist the previous field is ipad !!! */
 	uint32 cryptoType:3;
	uint8 cryptoKey[RTL8651_MAX_IPSEC_KEY_LENGTH];
	uint32 authType:3;
	uint8 authKey[RTL8651_MAX_IPSEC_KEY_LENGTH];

	int32 lifetime;
	int32 replayWindow;
	uint32 recvWindow[MAX_WINDOW_SIZE/(sizeof(uint32)*8)];
	uint32 seq;
	int32 aclRule;
	ipsecSpiPort_t nattPort; 

	ipsecSpiGrp_t *spiGrp; /* NULL, if this entry has not belonged to one group. */

	CTAILQ_ENTRY( ipsecSpi_s ) next;
};

struct ipsecSpiGrpSA_s
{
	uint32 valid:1;
	uint32 proto:2;
	ipaddr_t ip;
	uint32 spi;
	ipsecSpi_t *linkedSpi; /* NULL, if this entry has not link to someone. */
};

struct ipsecSpiGrp_s
{
	uint32 valid:1;

	/* entry[0] is the primary key, see man 'ipsec_spigrp'. */
	ipsecSpiGrpSA_t entry[4];

	CTAILQ_ENTRY( ipsecSpiGrp_s ) next;
};

struct ipsecERoute_s
{
	uint32 valid:1;
	uint32 outbound:1;

	/* primary key */
	ipaddr_t srcIp;
	ipaddr_t srcIpMask;
	ipaddr_t dstIp;
	ipaddr_t dstIpMask;
	
	ipaddr_t edstIp;
	uint32 spi;
	uint32 proto:2;
	ipsecSpi_t *linkedSpi; /* NULL, if this entry has not link to someone. */

	ipsecCounter_t counter;

	CTAILQ_ENTRY( ipsecERoute_s ) next;
};


struct ipsecEntryList_s
{
	struct 
	{
		CTAILQ_HEAD(_freeSpiEntry, ipsecSpi_s) spi;
		CTAILQ_HEAD(_freeSpiGrpEntry, ipsecSpiGrp_s) spigrp;
		CTAILQ_HEAD(_freeERouteEntry, ipsecERoute_s) eroute;
	} freeList;
	struct 
	{
		CTAILQ_HEAD(_InuseSpiEntry, ipsecSpi_s) spi;
		CTAILQ_HEAD(_InuseSpiGrpEntry, ipsecSpiGrp_s) spigrp;
		CTAILQ_HEAD(_InuseERouteEntry, ipsecERoute_s) eroute;
	} inuseList;
};


extern uint16 rtl865x_ipsecNatTraversalUdpPort;


int32 _rtl8651_initIpsec( void );
int32 _rtl8651_allocIpsec( int32 spiNum, int32 spiGrpNum, int32 eRouteNum );
int32 _rtl8651_dumpIpsec( void );

int32 _rtl8651_findIpsecSpi( ipaddr_t edstIp, uint32 spi, uint32 proto,
                             ipsecSpi_t **pRet );
int32 _rtl8651_addIpsecSpi( ipaddr_t edstIp, uint32 spi, uint32 proto,
                            ipaddr_t srcIp, uint32 cryptoType, void* cryptoKey, uint32 authType, void* authKey,
                            ipaddr_t dstIp, int32 lifetime, int32 replayWindow, uint32 nattPort, uint32 flags,
                            ipsecSpi_t **pRet );
int32 _rtl8651_delIpsecSpi( ipaddr_t edstIp, uint32 spi, uint32 proto );

int32 _rtl8651_findIpsecSpiGrp( ipaddr_t edstIp, uint32 spi, uint32 proto,
                                ipsecSpiGrp_t **pRet );
int32 _rtl8651_addIpsecSpiGrp( ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                               ipaddr_t edstIp2, uint32 spi2, uint32 proto2,
                               ipaddr_t edstIp3, uint32 spi3, uint32 proto3,
                               ipaddr_t edstIp4, uint32 spi4, uint32 proto4,
                               uint32 flags,
                               ipsecSpiGrp_t **pRet );
int32 _rtl8651_delIpsecSpiGrp( ipaddr_t edstIp1, uint32 spi1, uint32 proto1 );

int32 _rtl8651_findIpsecERoute( ipaddr_t srcIp, ipaddr_t srcIpMask,
                                ipaddr_t dstIp, ipaddr_t dstIpMask,
                                ipsecERoute_t **pRet );
int32 _rtl8651_findIpsecERouteBySrcDstIp( ipaddr_t srcIp, ipaddr_t dstIp, ipsecERoute_t **pRet );
int32 _rtl8651_findIpsecERouteBySrcEdstIp( ipaddr_t srcIp, ipaddr_t edstIp, ipsecERoute_t **pRet );
int32 _rtl8651_addIpsecERoute( ipaddr_t srcIp, ipaddr_t srcIpMask,
                               ipaddr_t dstIp, ipaddr_t dstIpMask,
                               ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                               uint32 flags,
                               ipsecERoute_t **pRet );
int32 _rtl8651_delIpsecERoute( ipaddr_t srcIp, ipaddr_t srcIpMask,
                               ipaddr_t dstIp, ipaddr_t dstIpMask );

int32 _rtl8651_flushIpsec( void );

int32 _rtl8651_ipsecAutoLink( void );




int32 _rtl8651_encapIpsec( int32 ruleNo, struct rtl_pktHdr *pkthdr, struct ip *pip, void *userDefined );
int32 _rtl8651_decapIpsec( int32 ruleNo, struct rtl_pktHdr *pkthdr, struct ip *pip, void *userDefined );



extern ipsecEntryList_t IpsecEntry;
extern int32 rtl865x_ipsecMaxConcurrentSpi; /* Maximum concurrent SPI number */
extern int32 rtl865x_ipsecMaxConcurrentSpiGrp; /* Maximum concurrent SPI Group number */
extern int32 rtl865x_ipsecMaxConcurrentERoute; /* Maximum concurrent ERoute number */


#endif/*RTL8651_IPSEC_LOCAL_H*/


