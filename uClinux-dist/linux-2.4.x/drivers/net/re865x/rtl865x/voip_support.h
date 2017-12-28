/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : Header File for VoIP Supporting in ROME Driver
* Abstract : 
* $Id: voip_support.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
*
*/

#ifndef _VOIP_SUPPORT_
#define _VOIP_SUPPORT_

#include "rtl_types.h"
#include "rtl_queue.h"
#include "mbuf.h"


typedef int32 ( *tick_callback )( uint32 chid, int32 msPassed );
typedef int32 ( *pktRx_callback )( uint32 chid, uint32 mid, void* packet, uint32 pktLen, uint32 flags );
typedef int32 ( *pcmInt_callback )( uint32 pcm_isr );


struct voip_callback_s
{
	tick_callback fpTick;
	int32 msTick;
	int32 msRemain;
	pktRx_callback fpPktRx;
	pcmInt_callback fpPcmInt;

	uint32 chid;
	
	SLIST_ENTRY(voip_callback_s) nextCallback;
};
typedef struct voip_callback_s voip_callback_t;

struct voip_rtp_port_s
{
	uint32 isTcp;
	uint32 remIp;
	uint16 remPort;
	uint32 extIp;
	uint16 extPort;

	uint32 chid;
	uint32 mid;
	
	SLIST_ENTRY(voip_rtp_port_s) nextRtp;
};
typedef struct voip_rtp_port_s voip_rtp_port_t;


/*
 * Used in the 'flags' field of voip_RxRtpPkt() and voip_TxRtpPkt().
 */
enum VOIP_PKT_TYPE
{
	TYPE_NONE=0,
	TYPE_INCLUDE_IPHDR=1,
};

int32 voip_init( int32 numChannel, int32 numRtp );
int32 voip_collect(void);
int32 voip_register_callback( uint32 chid, voip_callback_t *pCallback );
void voip_dump_callback( void );
int32 voip_ms_routine( int32 msPassed );
int32 voip_pollPcmInt( void );
int32 voip_query_RTPport( uint32 *chid, uint32 *mid, voip_rtp_port_t *pRtp );
int32 voip_register_RTPport( uint32 chid, uint32 mid, voip_rtp_port_t* pRtp );
int32 voip_pktRx( struct rtl_pktHdr *pPkt );
int32 voip_TxRtpPkt( uint32 chid, uint32 mid, void* packet, uint32 pktLen, enum VOIP_PKT_TYPE flags );
void voip_dump_RTPport( void );
int32 voip_setDefaultTosValue( uint8 tosValue );
int32 voip_getDefaultTosValue( uint8* pTosValue );


/*
 *  The range of category
 */
#define VOIP_CATEGORY_BASE (0x00001000)
#define VOIP_CATEGORY_VALID( cat ) ( (cat>=VOIP_CATEGORY_BASE) && (cat<=VOIP_CATEGORY_BASE+0x0fff) )
#define VOIP_CATEGORY( chid, mid ) (VOIP_CATEGORY_BASE+(chid<<4)+mid)
#define VOIP_CATEGORY_MAP( cat, pChid, pMid ) \
	do { \
		if ( VOIP_CATEGORY_VALID( cat ) ) \
		{ \
			*(pChid) = ((cat)-VOIP_CATEGORY_BASE)>>4; \
			*(pMid) = ((cat)-VOIP_CATEGORY_BASE)&0xF; \
		} \
	} while(0)

#endif
