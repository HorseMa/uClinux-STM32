/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : Packet Capturer Header File of ROME Driver
* Abstract : 
* Author : Yung-Chieh Lo (yjlou@realtek.com.tw)               
* $Id: romereal.h,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $
*/

#ifndef _ROMEREAL_H_
#define _ROMEREAL_H_

#include "rtl865x/rtl_types.h"
#include "rtl865x/mbuf.h"

typedef struct rtl_mBuf MBUF;
typedef struct rtl_pktHdr PKTHDR;

#if 0
/*-------------------------------------------------------*/
/* should be removed when porting to ROME driver */
#ifndef RTL_TYPES
typedef unsigned long long uint64;
typedef signed long long int64;
typedef unsigned int uint32;
typedef signed int int32;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned char uint8;
typedef signed char int8;
#endif/* RTL_TYPES */
#ifndef NULL
#define NULL 0
#endif/*NULL*/

#ifndef SUCCESS
#define SUCCESS 0
#define FAILED -1
#endif/*SUCCESS*/

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif/*TRUE*/

struct MBUF_s
{
	char *m_data;
};
typedef struct MBUF_s MBUF;

struct PKTHDR_s
{
	uint32 ph_len;
	MBUF *ph_mbuf;
};
typedef struct PKTHDR_s PKTHDR;
/*-------------------------------------------------------*/
#endif/*0*/


#define ROMEREAL_MODE_DEFUALT 0
#define ROMEREAL_MODE_CIRCULAR 1

#define ROMEREAL_FILTER_NIC_RX (1<<0)
#define ROMEREAL_FILTER_NIC_TX (1<<1)

/* file header of dump file */
struct rtl8651_fileHdr_s
{
	uint32 magicNum; /* 0xd4c3b2a1 */
	uint16 majorVer; /* 0x0002 */
	uint16 minorVer; /* 0x0004 */
	uint32 timezoneOffset; /* 0 */
	uint32 timestampAccu; /* 0 */
	uint32 snapshotLen;
	uint32 linkLayerType; /* 1 for ethernet, 9 for ppp */
};
typedef struct rtl8651_fileHdr_s rtl8651_fileHdr_t;

/* frame header of each packet */
struct rtl8651_frameHdr_s
{
	uint64 timestamp;
	uint32 capturedSize;
	uint32 actualSize;
	uint32 recordPoint;
	PKTHDR* pktHdr;
	MBUF* mbuf;
	uint8* cluster;
	struct rtl8651_frameHdr_s* next;
};

typedef struct rtl8651_frameHdr_s rtl8651_frameHdr_t;

/* compute frame size (frame header+pktHdr+mbuf+cluster) */
#define ROMEREAL_FRAMESIZE( frame ) \
	( sizeof(rtl8651_frameHdr_t) + \
	  sizeof(PKTHDR) + \
	  sizeof(MBUF) + \
	  (frame)->capturedSize )

#define UP_TO_4BYTE_ALIGN(addr) ((((uint32)(addr))+sizeof(uint32)-1)&~(sizeof(uint32)-1))
#define DOWN_TO_4BYTE_ALIGN(addr) (((uint32)(addr))&~(sizeof(uint32)-1))


/*** API ***/
int32 rtl8651_romerealInit( void* buffer, uint32 size, uint32 mode );
int32 rtl8651_romerealEnable( int8 enable );
int32 rtl8651_romerealRecord( PKTHDR* pkt, uint32 recordPoint );
rtl8651_frameHdr_t* rtl8651_romerealNext( rtl8651_frameHdr_t* curr );
int32 rtl8651_romerealSaveAsFile( char* filename );
int32 rtl8651_romerealDump( void );


#endif/* _ROMEREAL_H_ */
