/*
* Copyright c                  Realtek Semiconductor Corporation, 2003
* All rights reserved.
* 
* Program : Switch core table driver rtl8651_multicast.h
* Abstract : 
* Author : Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/

#ifndef RTL8651_MULTICASTLOCAL_H
#define RTL8651_MULTICASTLOCAL_H

#include "assert.h"
#include "types.h"
#ifndef CYGWIN
#ifdef __linux__
#include <linux/config.h>
#endif /* __linux__ */
#endif /* CYGWIN */
#include "rtl_queue.h"
#include "rtl_errno.h"

#include "rtl8651_timer.h"
#include "rtl8651_tblDrvProto.h"
#include "rtl8651_tblDrvLocal.h"
#include "rtl8651_multicast.h"
#include "rtl8651_tblDrvFwdLocal.h"

/*********************************************
			System Basic Setting
 *********************************************/
// debug message
#undef RTL8651_MCAST_DEBUG
#undef RTL8651_MCAST_DATA_DEBUG

// set Pkthdr Property
#define PKTHDR_ASLOOPBACK	0x01	/* Set packet property as it is loopback from physical port */
#define RTL8651_MCAST_RTK_QQIC	/* Enable Realtek own QQIC calculation instead of LINUX's */
/*********************************************
			Basic Setting
 *********************************************/
#define	IGMPV1						1
#define	IGMPV2						2
#define	IGMPV3						3

/* IGMPv3 */

/* filter mode */
#define	IGMP_FILTER_NONE			0
#define	IGMP_FILTER_EXCLUDE		1
#define	IGMP_FILTER_INCLUDE		2

/* exponential field decoding */
#ifdef RTL8651_MCAST_RTK_QQIC
#define IGMPV3_QQIC2QQI(qqic, threshold, nbmant, nbexp) \
			(((qqic) < (threshold))?(qqic): \
				((qqic)&((1<<(nbmant))-1)|0x10)<<((((qqic)>>(nbmant))&((1<<(nbexp))-1))+(nbexp)))
#define IGMPV3_QQI2QQIC(qqi, threshold, nbmant, nbexp) \
			(((qqi) < (threshold))?(qqi):(MCast_multicastQqic_Fix2Ft((qqi), (nbmant), (nbexp), 1)))

#define IGMPV3_QQIC(value)			IGMPV3_QQI2QQIC((value), 0x80, 4, 3)
#define IGMPV3_MRC(value)			IGMPV3_QQI2QQIC((value), 0x80, 4, 3)
#else
	/* Linux's QQIC calculation */
#define	IGMPV3_MASK(value, nb)		((nb)>=32 ? (value) : ((1<<(nb))-1) & (value))
#define	IGMPV3_EXP(thresh, nbmant, nbexp, value) \
			((value) < (thresh) ? (value) : \
			((IGMPV3_MASK(value, nbmant) | (1<<(nbmant+nbexp))) << \
			(IGMPV3_MASK((value) >> (nbmant), nbexp) + (nbexp))))
#define	IGMPV3_QQIC(value)			IGMPV3_EXP(0x80, 4, 3, value)
#define	IGMPV3_MRC(value)			IGMPV3_EXP(0x80, 4, 3, value)
#endif	/* RTL8651_MCAST_RTK_QQIC */

/* Report Types */
#define	IGMP_MODE_NONE			0
#define	IGMP_MODE_ISIN			1
#define	IGMP_MODE_ISEX			2
#define	IGMP_MODE_TOIN			3
#define	IGMP_MODE_TOEX			4
#define	IGMP_MODE_ALLOW			5
#define	IGMP_MODE_BLOCK			6

/* function utilities definition */
// constraint
#define IGMP_SRCLIST_CONSTRAINT_NONE				0		// no constraint
#define IGMP_SRCLIST_CONSTRAINT_NONEXPIRE		1		// srclist with timer which is not expire
#define IGMP_SRCLIST_CONSTRAINT_EXPIRE			2		// srclist with expired timer
// action
#define IGMP_SRCLIST_ACTION_NONE					0		// no action
#define IGMP_SRCLIST_ACTION_SETLMQT				1		// set this entry with new timeout LMQT


#define RTL8651_MCAST_IGMP_MAX_VERSION				IGMPV3	/* Current supported Version of IGMP proxy */
/*
	Memory Allocation Related Variable
*/
#define RTL8651_DefaultDrvMaxMCastCnt					20	/* Max multicast group count */
#define RTL8651_DefaultDrvMaxMCastMemberCnt			6	/* Max member count of each multicast group (0: no limit) */
#define RTL8651_DefaultDrvMaxTotalMemberCnt			120	/* Max total member count of igmp proxy (0: no limit) */
#define RTL8651_DefaultDrvMaxTotalSourceCnt				200	/* Max total source count of igmp proxy (0: no limit) */

// For Multicast Filter
#define RTL8651_MCastFilterHashSize						32	/* Hash table size of Multicast group filter */
#define RTL8651_DefaultDrvMaxMCastFilterEntryCnt		32	/* Default Multicast filter entry count */

/*
	Time related Variable
*/
#define RTL8651_DefaultDrvMcastMemberTimeout			180	/* Default timeout for each port-based member of each multicast group */
//	=> if UpStream Timeout == 0xffffffff, system will ignore Upstream timeout and only expire if there is no member for this multicast group
#define RTL8651_DefaultDrvMCastUpstreamTimeout			125	/* Default timeout for each multicast entry which upstream has no respond (0xffffffff for infinite) */
// For periodic query
#define RTL8651_IGMP_QI									125	/* General Query Interval of gateway */
// For report of previous query
#define RTL8651_IGMP_QRI								10	/* Query Response Interval (sec) */
// For IGMPv2 Querier Election
// by RFC2236: OQPI = ((the Robustness Variable) times (the Query Interval)) plus (one half of one Query Response Interval)
#define RTL8651_IGMP_OQPI								255	/* Other Querier Present Interval */
// For System Start-up
//	=> if default upstream is set, it will not broadcast to default Upstream
#define RTL8651_IGMP_STARTUP_QUERY_DELAY			10	/* Start to send Query after this interval from system start-up */
#define RTL8651_IGMP_SQC								3	/* Startup Query Count */
#define RTL8651_IGMP_SQI								25	/* Startup/General Query Interval (sec) RFC2236: ((1/4) * Query Interval) */
// For Member expire
#define RTL8651_IGMP_LMQC								2	/* Last Member Query Count */
#define RTL8651_IGMP_LMQI								5	/* Last Member Query Interval (sec) */
// V3
#define RTL8651_IGMP_GMI								135	/* Group Member Interval */
// For tolerance design
#define RTL8651_IGMP_GENERAL_QUERY					FALSE/* System will periodically query all interfaces except default upstream if this bit set */
#define RTL8651_IGMP_GROUP_QUERY_INTERVAL			125	/* Group-specific query interval */
#define RTL8651_IGMP_RESPONSE_TOLERANCE_DELAY		5	/* Added Delay (sec.) to wait for downstream response */

/*
	For multicast process control
*/
#define RTL8651_MCAST_PROCOPT_DEFAULT				0	/* default: we don't apply any process option */
/*********************************************
			Data Structure
 *********************************************/
struct rtl8651_tblDrvMCastMember_s;
struct rtl8651_tblDrvMCast_s;
struct rtl8651_tblDrvMCastMember_s;

/* Source list entry of each Multicast Member */
typedef struct rtl8651_tblDrvMCastSource_s {
	uint8								_type;	// type for memory allocation/free, must be the FIRST entry in structure

	ipaddr_t								source;	// source address	
	struct rtl8651_timer_list						timer;	// timeout	(used when MEMBER FilterMode is INCLUDE)

	struct rtl8651_tblDrvMCastMember_s *	mbr;	// member
	struct rtl8651_tblDrvMCastSource_s *		prev;	// prev source
	struct rtl8651_tblDrvMCastSource_s *		next;	// next source
} rtl8651_tblDrvMCastSource_t;

/* Member list entry of Multicast Group Address */
typedef struct rtl8651_tblDrvMCastMember_s {
	uint8						_type;// type for memory allocation/free, must be the FIRST entry in structure

	/* Member identifier */
	rtl8651_tblDrv_vlanTable_t *		vlan;							// vlan entry for these members
	uint16						portMask;						// subscriber-port of this member (including Extension port)

	/* data */
	uint8						MulticastRouterVersion;			// used IGMP version
	struct rtl8651_timer_list				timer_filterMode;				// EXCLUDE filter mode expire time
	struct rtl8651_timer_list				timer_state;						// expire timer for state
	struct rtl8651_timer_list				timer_query;					// expire timer for query sending
	struct rtl8651_timer_list				timer_query_expire;				// set this timer when querying, if no any host reply, then expire this member
	uint8						state;							// state of this member
	uint8						remainingQuery;					// remaining Query count when state is IGMP_MEMBER_STATE_WILLTIMEOUT
	uint8						isForwarder;					// for IGMPv2/3, indicate if we are forwarder or not
	uint8						FilterMode;						// current filter mode of source list

	/* linked list */
	struct rtl8651_tblDrvMCast_s *			m_entry;				// multicast entry of this member
	struct rtl8651_tblDrvMCastMember_s *	prev;					// prev member
	struct rtl8651_tblDrvMCastMember_s *	next;					// next member
	struct rtl8651_tblDrvMCastSource_s *		slist;					// source list
} rtl8651_tblDrvMCastMember_t;
// Member state
#define	IGMP_MEMBER_STATE_NORMAL						1	// normal state
#define	IGMP_MEMBER_STATE_WILLTIMEOUT					2	// after member expire and start to send query


/* Multicast Group entry */
typedef struct rtl8651_tblDrvMCast_s {
	uint8						_type;// type for memory allocation/free, must be the FIRST entry in structure

	ipaddr_t						mcast_addr;						// multicast address

	/* upstream */
	struct rtl8651_timer_list				timer_upstream;					// expire timer for entry without upstream
	rtl8651_tblDrv_vlanTable_t *		up_vlan;						// upstream vlan entry
	uint16						up_portMask;					// upstream port
	uint8						MulticastRouterVersion;			// upstream IGMP version

	uint8						FilterMode;						// filter mode of upstream interface
	rtl8651_tblDrvMCastSource_t *	slist;							// source list of upstream interface

	/* downstream */
	rtl8651_tblDrvMCastMember_t *	member;						// member list
	uint32						memberCnt;						// count of member
	
	/* linked list */
	struct rtl8651_tblDrvMCast_s *		prev;							// prev MCast entry
	struct rtl8651_tblDrvMCast_s *		next;							// next MCast entry
} rtl8651_tblDrvMCast_t;


/*
	For routing
*/
typedef struct rtl8651_tblDrvMCastRouteInfo_s {
	ipaddr_t							m_addr;
	rtl8651_tblDrv_vlanTable_t 			*vlan;
	uint8							isPortMask;
	uint16							port;
	rtl8651_tblDrv_networkIntfTable_t 	*netif;
	uint32							ifInfo;	/* Interface information : for vlan type: ip Addr ; for PPPoE/PPTP/L2TP: sessionid */
} rtl8651_tblDrvMCastRouteInfo_t;

/*
	For Default Upstream
*/
typedef struct rtl8651_tblDrvMCastUpStreamInfo_s {
	uint8							isSet;
	rtl8651_tblDrv_networkIntfTable_t 	*netif;
	rtl8651_tblDrv_vlanTable_t			*vlan;
	uint16							portMask;
	union {
		ipaddr_t						intfIp;
		rtl8651_tblDrvSession_t			*pppoesessionPtr;
		rtl8651_tblDrvSession_t			*sessionPtr;		
	} MUpStreamInfo_un;

	#define	M_intfIp					MUpStreamInfo_un.intfIp
	#define	M_sessionPtr			MUpStreamInfo_un.sessionPtr
	#define	M_pppoeSessionPtr		MUpStreamInfo_un.pppoesessionPtr

} rtl8651_tblDrvMCastUpStreamInfo_t;

/*
	For System Variable (excepts Timer Control/Counter)
*/
typedef struct rtl8651_tblDrvMCastGlobal_s {
	struct rtl8651_timer_list					query_timer;	// timer of general Query broadcasting
	uint8							remainingQuery;	// remaining Query count when system is initiated
	rtl8651_tblDrvMCastUpStreamInfo_t	MCastUpStream;	// upstream information
} rtl8651_tblDrvMCastGlobal_t;


/*
	For Multicast address-filter
*/
typedef struct rtl8651_tblDrvMCastFilter_s {
	ipaddr_t							gip;				// filtered group ip
	uint32							action;			// action of this filter entry
	/* linked list */
	struct rtl8651_tblDrvMCastFilter_s	*next;
} rtl8651_tblDrvMCastFilter_t;

/* if ALL of errCombine bits are set in action, error occurs. */
#define MCASTFILTER_UNLOCK_ACTIONCHECK(action, errCombine, reason) \
	do { \
		if (((action) & (errCombine)) == (errCombine)) \
		{ \
			return reason; \
		} \
	} while (0);


/******************************************************
	Function called by system initiate function
******************************************************/
int32 _rtl8651_multicastInit(rtl8651_fwdEngineInitPara_t *);
int32 _rtl8651_multicast_FilterInit(rtl8651_fwdEngineInitPara_t *);
/******************************************************
	Function called by forwarding engine
******************************************************/
int32 _rtl8651_igmp_process(struct rtl_pktHdr *, struct ip *, struct igmp *);
int32 _rtl8651_multicast_process(struct rtl_pktHdr *, struct ip *);
int32 _rtl8651_multicast_Fastfwd(struct rtl_pktHdr *, struct ip *, rtl8651_tblDrv_vlanTable_t *);

#endif /* RTL8651_MULTICASTLOCAL_H */
