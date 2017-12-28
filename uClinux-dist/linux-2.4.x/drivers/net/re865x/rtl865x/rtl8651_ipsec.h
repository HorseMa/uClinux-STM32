
/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : IPSec-related header file
* Abstract : 
* Author : Louis Yung-Chieh Lo (yjlou@realtek.com.tw)               
* $Id: rtl8651_ipsec.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
*/

#ifndef RTL8651_IPSEC_H
#define RTL8651_IPSEC_H

#include "rtl_types.h"
#include "mbuf.h"
#include "rtl8651_tblDrvProto.h"

/*
 *  Function Convention:
 *   uint32 -- for unsigned data and bitmask (ex: IP_Address/SPI/flags ...)
 *   uint32 -- for enum definition (ex: Proto/Type ...), which 0 means undefined.
 *   int32  -- for number (ex: lufetime, replayWindow ...)
 *
 *  The primary index for SPI is [edstIp,spi,proto].
 *  The primary index for EROUTE is [srcIp,srcMask,dstIp,dstMask].
 *
 */

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * -=  Initial Function
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
int32 rtl8651_flushIpsec( void );

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * -=  SPI Function
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
int32 rtl8651_addIpsecSpi( ipaddr_t edstIp, uint32 spi, uint32 proto,
                           ipaddr_t srcIp, uint32 cryptoType, void* cryptoKey, uint32 authType, void* authKey,
                           ipaddr_t dstIp, int32 lifetime, int32 replayWindow,uint32 nattPort, uint32 flags );

int32 rtl8651_delIpsecSpi( ipaddr_t edstIp, uint32 spi, uint32 proto );

int32 rtl8651_updateIpsecSpi( ipaddr_t edstIp, uint32 spi, uint32 proto,
                              ipaddr_t srcIp, uint32 cryptoType, void* cryptoKey, uint32 authType, void* authKey,
                              ipaddr_t dstIp, int32 lifetime, int32 replayWindow,uint32 nattPort, uint32 flags );

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * -=  SPIGRP Function
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
int32 rtl8651_addIpsecSpiGrp( ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                              ipaddr_t edstIp2, uint32 spi2, uint32 proto2,
                              ipaddr_t edstIp3, uint32 spi3, uint32 proto3,
                              ipaddr_t edstIp4, uint32 spi4, uint32 proto4,
                              uint32 flags );

int32 rtl8651_delIpsecSpiGrp( ipaddr_t edstIp1, uint32 spi1, uint32 proto1 );

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * -=  EROUTE Function
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
int32 rtl8651_addIpsecERoute( ipaddr_t srcIp, ipaddr_t srcIpMask,
                              ipaddr_t dstIp, ipaddr_t dstIpMask,
                              ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                              uint32 flags );

int32 rtl8651_delIpsecERoute( ipaddr_t srcIp, ipaddr_t srcIpMask,
                              ipaddr_t dstIp, ipaddr_t dstIpMask );

int32 rtl8651_updateIpsecERoute( ipaddr_t srcIp, ipaddr_t srcIpMask,
                                 ipaddr_t dstIp, ipaddr_t dstIpMask,
                                 ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                                 uint32 flags );


/* cryptoType */
#define IPSEC_CBC_DES		0x01
#define IPSEC_CBC_3DES		0x02
#define IPSEC_ECB_DES		0x03
#define IPSEC_ECB_3DES		0x04
#define IPSEC_CBC_AES128	0x05
#define IPSEC_CBC_AES192	0x06
#define IPSEC_CBC_AES256	0x07

/* authType */
#define IPSEC_HASH_MD5	0x01
#define IPSEC_HASH_SHA1	0x02
#define IPSEC_HMAC_MD5	0x03
#define IPSEC_HMAC_SHA1	0x04

/* flags for rtl8651_addIpsecSpi() */
#define IPSEC_AUTH_DATA_PRESENT			( 1 << 0 )
#define IPSEC_I_HAVE_ANTI_REPLAY			( 1 << 1 )
#define IPSEC_NATTRAVERSAL_PRESENT		( 1 << 2 )
#define IPSEC_IPCOMPINDEX				(1<<31|1<<30)

/* protocol */
#define IPSEC_AH						( 0x00 )
#define IPSEC_ESP						( 0x01 )
#define IPSEC_IPIP					( 0x02 )
#define IPSEC_IPCOMP					( 0x03 )

extern int32 rtl865x_ipsecMtu;

#endif/*RTL8651_IPSEC_H*/


