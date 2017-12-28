/*
* Copyright c                  Realtek Semiconductor Corporation, 2003
* All rights reserved.
* 
* Program : Switch core table driver rtl8651_multicast.h
* Abstract : 
* Author : Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/

#ifndef RTL8651_MULTICAST_H
#define RTL8651_MULTICAST_H

#define CONFIG_RTL865X_MULTICAST	/* Always enable multicast function */
//#define RTL8651_MCAST_DEBUG

struct ip;
struct igmp;

/*
	Utilities
*/
#define IGMP_ALL_HOSTS				(0xE0000001L)
#define IGMP_ALL_ROUTER			(0xE0000002L)
#define IGMP_LOCAL_GROUP			(0xE0000000L)
#define IGMP_LOCAL_GROUP_MASK		(0xFFFFFF00L)
#define IGMP_V3_REPORT_ADDR		(0xE0000016L)

/*
	Macros
*/
#define IN_CLASSA(addr)				((((ipaddr_t)(addr)) & 0x80000000) == 0x00000000)
#define IN_CLASSB(addr)				((((ipaddr_t)(addr)) & 0xc0000000) == 0x80000000)
#define IN_CLASSC(addr)				((((ipaddr_t)(addr)) & 0xe0000000) == 0xc0000000)
#define IN_CLASSD(addr)				((((ipaddr_t)(addr)) & 0xf0000000) == 0xe0000000)
#define IN_MULTICAST_RESV(addr)	((((ipaddr_t)(addr)) & IGMP_LOCAL_GROUP_MASK) == 0xe0000000)	// 224.0.0.x
#define IS_V3_REPORT_ADDR(addr)	((ipaddr_t)(addr) == IGMP_V3_REPORT_ADDR)

#define IN_MULTICAST(addr)			IN_CLASSD(addr)
#define IN_UNICAST(addr)			((((ipaddr_t)(addr)) & 0xf0000000) < 0xe0000000)	/* < Class D */
//#define IN_UNICAST(addr)			(IN_CLASSA(addr) || IN_CLASSB(addr) || IN_CLASSC(addr))

/* map IP to MAC */
#define MCAST_IP2MAC(addr,pMAC)	\
	do { \
		((uint8*)(pMAC))[0] = 0x01; \
		((uint8*)(pMAC))[1] = 0x00; \
		((uint8*)(pMAC))[2] = 0x5e; \
		((uint8*)(pMAC))[3] = (uint8)((((uint32)(addr)) >> 16) & 0xff); \
		((uint8*)(pMAC))[4] = (uint8)((((uint32)(addr)) >> 8) & 0xff); \
		((uint8*)(pMAC))[5] = (uint8)(((uint32)(addr)) & 0xff); \
		((uint8*)(pMAC))[3] &= 0x7f; \
	}while(0);

/*
	For System Timer/Counter Setting
*/
typedef struct rtl8651_tblDrvMCastPara_s {
	uint32	MCastMember_ExpireTime;		/* timeout of member (>0) */
	uint32	MCastUpstream_Timeout;			/* timeout of multicast entry without upstream (>0) */
	uint32	igmp_qi;						/* Query interval (>0) */
	uint32	igmp_qri;						/* Query Response Interval (>0) */
	uint32	igmp_oqpi;						/* Other Querier Present Interval (>=0) */
	uint32	igmp_sqc;						/* System StartUp Query Count (>=0), only useful when system in Startup phase */
	uint32	igmp_sqi;						/* System StartUp Query Interval (>=0) */
	uint32	igmp_lmqc;						/* Per-port Last Member Query Count (>=0) */
	uint32	igmp_lmqi;						/* Per-port Last Member Query Interval (>=0) */
	uint32	igmp_gmi;						/* Group Member Interval (>0) */
	uint32	igmp_group_query_interval;		/* If periodic query is turned on, it's the periodic query interval (>0) */
	uint32	igmp_response_tolerance_delay;	/* IGMP report-tolerance delay (>=0) */
} rtl8651_tblDrvMCastPara_t;

/*
	For multicast group filter

	* (MCAST_FILTER_ACT_NOPROC) and (MCAST_FILTER_ACT_L2FWD) are mutual exclusive.

*/
#define	MCAST_FILTER_ACT_MIRROR			(1<<0)		/* mirror this packet to protocol stack */
#define	MCAST_FILTER_ACT_NOPROC			(1<<1)		/* don't proc this packet */
#define	MCAST_FILTER_ACT_L2FWD			(1<<2)		/* do L2 forwarding for Multicast packets. (both Data/IGMP packets) */
#define	MCAST_FILTER_ACT_ALL				0xffffffff	/* bitmask of all actions */

/*
	For multicast process control
*/
#define MCAST_PROC_EXPIRE_STATE_DONT_FWD	0x00000001	/* don't forward to downstream if this downstream in expire state (ex. leave msg received, timeout..) */
/*********************************************
		External Functions Declaration
 *********************************************/
void MCast_DumpMCast(void);
/* multicast group filter */
int32 rtl8651_multicast_AddFilter(ipaddr_t gip, uint32 action);
int32 rtl8651_multicast_RemoveFilter(ipaddr_t gip, uint32 action);
/**************************
	Initialize
**************************/
void rtl8651_multicastReinit(void);
void rtl8651_multicast_FilterReinit(void);
int32 rtl8651_multicastSetPara(rtl8651_tblDrvMCastPara_t *);
int32 rtl8651_multicastSetUpStream(int8*, uint32);
int32 rtl8651_multicastPeriodicQuery(int8 enable);
#endif /* RTL8651_MULTICAST_H */
