/*
* Copyright c                  Realtek Semiconductor Corporation, 2003
* All rights reserved.
* 
* Program : Internal Header file for  fowrading engine rtl8651_tblDrvFwd.c
* Abstract : 
* Author :  Chun-Feng Liu(cfliu@realtek.com.tw)
* $Id: rtl8651_tblDrvFwdLocal.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_tblDrvFwdLocal.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.44  2006/09/15 09:49:35  darcy_lu
* +: add code for new port bouncing module
*
* Revision 1.43  2006/09/13 02:52:39  bo_zhao
* *: fix staticPortTranslate bug
*
* Revision 1.42  2006/07/17 06:26:12  hyking_liu
* +: add a RTL_NAPT_FRAG cache type for protocol stack action reverse
*
* Revision 1.41  2006/05/25 07:34:47  bo_zhao
* *: refine acl l4 Type and Dir match
*
* Revision 1.40  2006/05/19 07:13:32  bo_zhao
* +: support new Napt Module & add l4 process type
*
* Revision 1.39  2006/01/25 13:59:15  tony
* +: New Feature: URL Filter Trusted user must be logged.
*
* Revision 1.38  2006/01/18 02:37:34  bo_zhao
* +: add fragement cache type for DSCP remark
*
* Revision 1.37  2006/01/06 08:02:52  chenyl
* *: bug fix : Idle timer would be refreshed when PassThrough packet forwarded
*    by SW forwarding engine.
*
* Revision 1.36  2006/01/04 14:56:59  chenyl
* +: Layer 2 extension device VLAN TX filtering
* *: Don't modify TX packet (ex. add/strip/modify VLAN tag) for non-RomeDriverFwd packet
*    (Protocol Stack TX or Offload Engine TX)
* *: turn some debugging message OFF.
* *: When doing Tx checksum offloading, function _rtl8651_fwdEngineTxChecksumOffload()
*    need to check if "pip" being NULL to prevent from system crash.
*
* Revision 1.35  2006/01/02 02:52:14  chenyl
* +: VLAN-tag support extension port forwarding and software Rome Driver forwarding.
*
* Revision 1.34  2005/12/12 03:20:38  tony
* +: New Feature: support trusted user in URL filter.
*
* Revision 1.33  2005/10/12 12:02:05  chenyl
* *: set the HEADROOM same as what define in config.h
*
* Revision 1.32  2005/09/28 08:40:55  chenyl
* *: add internal function to set the permission of each forwarding decision
* 	- if it would apply SMAC filtering?
* 	- blocking this decision
* 	=> API:
* 		int32 _rtl8651_l34AdditionalExec_setParam(srcType,dstType,para)
* 		int32 _rtl8651_l34AdditionalExec_getParam(srcType,dstType,para)
*
* *: modify return value
* 	- WAN to WAN routing
* 		- if it be blocked, original return value is FWDENG_IP_WAN2WAN_ROUTING
* 			=> Modified to "FWDENG_DECISION_BLOCK"
*
* 	- Port bouncing
* 		- if it be blocked, original return value is FWDENG_L4_PORTBOUNCING
* 			=> Modified to "FWDENG_TO_GW"
*
* Revision 1.31  2005/09/25 16:58:42  shliu
* *: modify acl checking procedure prototype
*
* Revision 1.30  2005/08/25 04:01:36  chenyl
* *: rearrange code, add MUTEX lock/unlock for external functions, add `inline' property to some critical functions
*
* Revision 1.29  2005/08/23 03:08:14  chenyl
* *: add Mutex Lock/Unlock to forwarding engine's control API to prevent from processing fail.
*
* Revision 1.28  2005/08/18 09:14:08  chenyl
* *: add code to porting to other OSs
*
* Revision 1.27  2005/08/15 10:53:28  chenyl
* +: new TRIGGER PORT mechanism
* 	- sip check when activate Trigger Port.
* 	- age timer for Triggered Port Mapping.
*
* Revision 1.26  2005/07/29 09:43:59  chenyl
* +: dns module in ROMEDRV
* +: domain blocking (SDK + RomeDrv)
*
* Revision 1.25  2005/05/06 12:23:16  chenyl
* *: for outbound packet which match upnp mapping, ROME Drvier would use the mapped external ip/port
* 	to contribute this connection.
*
* *: tony: fix tcp upnp qos support problem.
*
* Revision 1.24  2005/05/04 15:35:32  chenyl
* *: modify code for TCP/UDP static port translation
*
* Revision 1.23  2005/05/04 13:19:06  chenyl
* *: bug fix: when turn the "TCP/UDP static port mapping" and the source IP/Port
* 	    is in uPnP/triggerPort map and the internal Port == External Port in that map,
* 	    then in these cases, static port mapping is still work.
*
* Revision 1.22  2005/04/30 04:19:49  chenyl
* +: support (L2-Packet-filling + L3-Forwarding) offload Engine
*
* Revision 1.21  2005/04/14 12:54:07  shliu
* *: Add a new user ACL rule matchType_: L4NEWFLOW.
*
* Revision 1.20  2005/03/28 12:17:17  chenyl
* +: Exception List for NAPT-Redirect system
*
* Revision 1.19  2005/03/28 06:01:51  chenyl
* *: NAPT redirect without DIp/Dport modification
*
* Revision 1.18  2005/03/14 15:33:12  chenyl
* *: modify IP fragment packet trapping mechanism for ProtocolStack cached flow
*
* Revision 1.17  2005/03/14 14:40:15  chenyl
* *: bug fix: protocol-stack flow cache, fragment packet forwarding
*
* Revision 1.16  2005/03/03 03:00:49  chenyl
* *: Only trap L3/4 processed IP packet to CPU for IP-Range ACL rules
*
* Revision 1.15  2005/02/16 02:15:12  chenyl
* +: Forwarding Engine Trapping Dispatch system:
* 	When one packet must to be trapped to upper-layer protocol stack,
* 	User can decide how to dispatch this packet into different "categories"
* 	according to its 'l4protocol/source IP/destination IP/source Port (TCP/UDP)/
* 	destination Port (TCP/UDP)' with wide-card allowed. The category
* 	number is decided by user and will be set into ph_category in packet header.
* 	Fragment packet's test is passed.
*
* +: Forwarding Engine QoS Send:
* 	Protocol stack packet send with high TX queue used.
*
* Revision 1.14  2005/02/02 01:37:57  chenyl
* *: fix L2TP ICCN/ZLB erroneous judgement problem
* +: forge L2TP header if ICCN has sent by protocol stack and sensed by ROMEDRV
*
* Revision 1.13  2005/01/26 08:27:57  chenyl
* *: fix PPTP/L2TP WAN type bugs:
*
* 	1. After decapsulating, and the DIP is broadcast IP (0xffffffff), we must let DMAC been ff-ff-ff-ff-ff-ff
* 	2. System MUST forbid L2 broadcasting if packet is from PPTP/L2TP loopback Port.
* 	3. Before trapping packet to protocol stack, system MUST modify packet's source VLAN if this packet is from
* 	   PPTP/L2TP loopback Port.
*
* Revision 1.12  2005/01/18 08:50:22  ghhuang
* +: Diffserv (DSCP) remarking support
*
* Revision 1.11  2004/12/22 05:19:15  tony
* +: support new features: make ALG(FTP,SIP,NetMeeting) support MNQueue.
*
* Revision 1.10  2004/12/14 08:29:17  yjlou
* *: add a delta parameter for _rtl8651_l4TcpAdjustMss().
*
* Revision 1.9  2004/12/10 08:45:20  yjlou
* +: support ACLDB and IPSEC (20Mbps)
*
* Revision 1.8  2004/12/08 10:20:24  chenyl
* *: rearrange variable/function declarations in rtl8651_tblDrvFwd.c
* +: apply reason-bit for Ingress ACL check --
* 	- If one Ingress ACL rule is matched by ASIC, the rules checked by ASIC(index less than matched rule)
* 	  will not be checked again
* 	- If All ingress ACL rules are checked and permitted by ASIC, then all ingress ACL rules will not be checked again.
*
* Revision 1.7  2004/11/29 07:35:43  chenyl
* *: fix bug : multiple-PPPoE MTU setting bug
* *: fix bug : Never add second session's ip into ASIC in multiple-PPPoE WAN type.
* *: fix bug : Tunekey abnormal behavior when multiple-PPPoE Dial-on-demand Dynamically turn ON/OFF
* *: in forwarding engine Init function, the parameter == 0xffffffff means user want to set as default value
* *: add Mutex-Lock/Unlock checking in testing code
*
* Revision 1.6  2004/11/10 07:40:18  chenyl
* *: fragment cache re-write (1. code re-arrange, 2. check org-sip/dip when getting cache)
*
* Revision 1.5  2004/10/04 02:16:23  chenyl
* +: pptp/l2tp header refill function
*
* Revision 1.4  2004/09/30 07:59:12  chenyl
* *: function mtuFragment() modification
* +: apply forwarding engine to generate ICMP-Host-Unreachable message for packets fragmented by gateway
* +: add the switch to turn on/off "always reply ICMP error message for all packets fragmented by gateway regardless of
* 	Dont fragment bit"
*
* Revision 1.3  2004/09/15 16:14:28  chenyl
* +: enable multicast forward cache
*
* Revision 1.2  2004/09/13 04:17:36  chenyl
* +: napt redirect inbound fragment cache
*
* Revision 1.1  2004/09/07 14:52:13  chenyl
* *: bug fix: napt-redirect fragment packet checksum-recalculate
* *: bug fix: conflict between protocol stack flow cache and protocol stack action
* *: bug fix: protocol stack action mis-process to UDP-zero-checksum packet
* *: separate the header file:
*         - internal : rtl8651_tblDrvFwdLocal.h
*         - external : rtl8651_tblDrvFwd.h
*
*/
#ifndef RTL8651_FWDENGINE_LOCAL_H
#define RTL8651_FWDENGINE_LOCAL_H

#include "assert.h"
#include "types.h"
#ifndef CYGWIN
#ifdef __linux__
#include <linux/config.h>
#endif /* __linux__ */
#endif /* CYGWIN */
#include "rtl_queue.h"
#include "rtl_errno.h"
#include "mbuf.h"

#include "rtl8651_tblDrvFwd_utility.h"
#include "rtl8651_tblDrvProto.h"
#include "rtl8651_tblDrvLocal.h"
#include "rtl8651_layer2fwdLocal.h"
#include "rtl8651_tblDrvFwd.h"
#include "rtl8651_ipQueue_local.h"
#include "rtl8651_multicast_local.h"

/*********************************************************************************************************
	host type and decision table

		=> Users MUST modify this variable very carefully.
**********************************************************************************************************/
/* ip type */
enum _RTL8651_HOSTTYPE {
	_RTL8651_HOST_NPI = 0,
	_RTL8651_HOST_NI,
	_RTL8651_HOST_LP,
	_RTL8651_HOST_RP,
	_RTL8651_HOST_NPE,
	_RTL8651_HOST_NE,
	_RTL8651_HOST_MAX,
	_RTL8651_HOST_NONE,
};

/* check valid hostType */
#define _RTL8651_HOSTTYPE_ISVALID(type) \
	(((type) >= _RTL8651_HOST_NPI) && ((type) < _RTL8651_HOST_MAX))

/* decision */
enum _RTL8651_DECISIONTYPE {
	_RTL8651_DEC_NONE = 0,
/* <---- L3 ----> */
	_RTL8651_DEC_RT_LR,		/* L3 Routing -- Local Public to Remote Host */
	_RTL8651_DEC_RT_RL,		/* L3 Routing -- Remote host to Local Public */
	_RTL8651_DEC_RT_LNP,		/* L3 Routing -- Local Public to NAPT Internal */
	_RTL8651_DEC_RT_NPL,		/* L3 Routing -- NAPT Internal to Local Public */
	_RTL8651_DEC_RT_LN,		/* L3 Routing -- Local Public to NAT Internal */
	_RTL8651_DEC_RT_NL,		/* L3 Routing -- NAT Internal to Local Public */
	_RTL8651_DEC_RT_LL,		/* L3 Routing -- Local Public to Local Public */
	_RTL8651_DEC_RT_NN,		/* L3 Routing -- NAT Internal to NAT Internal */
	_RTL8651_DEC_RT_NPNP,		/* L3 Routing -- NAPT Internal to NAPT Internal */
	_RTL8651_DEC_RT_NPN,		/* L3 Routing -- NAPT Internal to NAT Internal */
	_RTL8651_DEC_RT_NNP,		/* L3 Routing -- NAT Internal to NAPT Internal */
	_RTL8651_DEC_RT_RR,		/* L3 Routing -- Remote Public to Remote Public (Wan Side routing) */
/* <---- L4 ----> */
	_RTL8651_DEC_NT,			/* L4 NAT - outbound */
	_RTL8651_DEC_NTR,			/* L4 NAT - inbound */
	_RTL8651_DEC_NTBC_NP,		/* port bouncing -- NAPT Internal to NE */
	_RTL8651_DEC_NTBC_N,		/* port bouncing -- NAT Internal to NE */
	_RTL8651_DEC_NTBC_L,		/* port bouncing -- Local Public to NE */
	_RTL8651_DEC_NPT,			/* L4 NAPT - outbound */
	_RTL8651_DEC_NPTR,		/* L4 NAPT - inbound */
	_RTL8651_DEC_NPTBC_NP,	/* port bouncing -- NAPT Internal to NPE */
	_RTL8651_DEC_NPTBC_N,		/* port bouncing -- NAT Internal to NPE */
	_RTL8651_DEC_NPTBC_L,		/* port bouncing -- Local Public to NPE */
/* <---- Other ----> */
	_RTL8651_DEC_DP,		/* decide to drop packet */
	/* =============================================================================== */
	_RTL8651_DEC_MAX
};

/* get the operation layer of decision */
#define _RTL8651_OPTLAYER(decision)	\
	(((decision) >= _RTL8651_DEC_RT_LR) && ((decision) <= _RTL8651_DEC_RT_RR))?	\
		(_RTL8651_OPERATION_LAYER3): \
		((((decision) >= _RTL8651_DEC_NT) && ((decision) <= _RTL8651_DEC_NPTBC_L))?	\
			(_RTL8651_OPERATION_LAYER4): \
			(_RTL8651_OPERATION_OTHER))

/* check valid decision */
#define _RTL8651_DECISION_ISVALID(decision) \
	(((decision) > _RTL8651_DEC_NONE) && ((decision) < _RTL8651_DEC_MAX))


/*********************************************************************************************************
	Time update function
**********************************************************************************************************/
void _rtl8651_fwdEngineTimeUpdate(uint32 secPassed);

/*********************************************************************************************************
	ACL
**********************************************************************************************************/
/*int32 _rtl8651_ingressAcl(uint32 dsid,uint32 startIdx, rtl8651_tblDrv_vlanTable_t *pVlan,int32 vid, uint8 *m_data, uint8 *ip, uint32 matchType, uint32 optLayer);	*/
int32 _rtl8651_ingressAcl(uint32 startIdx, rtl8651_tblDrv_vlanTable_t *pVlan, int32 vid, uint8 *m_data, uint8 *ip, _rtl8651_tblDrvAclLookupInfo_t *info);
int32 _rtl8651_egressAcl(uint32 dsid,rtl8651_tblDrv_vlanTable_t *pVlan,int32 vid, uint8 *ethHdr, uint8 *ip);

/*********************************************************************************************************
	L2 forward
**********************************************************************************************************/
/* Tx process : these bits are mutual exclusive */
#define FWDENG_FROMDRV		((uint32)(1)<<1)
#define FWDENG_FROMPS			((uint32)(1)<<2)
#define FWDENG_OFFLOAD		((uint32)(1)<<3)
/* Process Layer : L2 or L34 or Tunnel TX/RX or Other(for Protocol Stack TX (Offload Engine is L34 process)) */
#define FWDENG_L2PROC			((uint32)(1)<<4)
#define FWDENG_L34PROC			((uint32)(1)<<5)
#define FWDENG_TUNNELTX		((uint32)(1)<<6)
#define FWDENG_TUNNELRX		((uint32)(1)<<7)
#define FWDENG_OTHERPROC		((uint32)(1)<<8)

 /*********************************************************************************************************
	L3 routing
**********************************************************************************************************/
int32 _rtl8651_l3FastActRouting(uint32 property, struct rtl_pktHdr *pkthdrPtr, ipaddr_t dst_ip, struct ip *ip, void **);

/*********************************************************************************************************
	L34 additional execution
**********************************************************************************************************/
#define _RTL8651_L34ADDTIONALEXEC_BLOCK			0x01	/* Block this packet from forwarding */
#define _RTL8651_L34ADDTIONALEXEC_SMACFILTER	0x02	/* Do source filtering */

typedef struct _rtl8651_l34AdditionalExec_s
{
	uint8 property;
} _rtl8651_l34AdditionalExec_t;

/* set parameters of L34 Additional Exec */
int32 _rtl8651_l34AdditionalExec_setParam(uint32 srcType, uint32 dstType, _rtl8651_l34AdditionalExec_t *para);
/* get parameters of L34 Additional Exec */
int32 _rtl8651_l34AdditionalExec_getParam(uint32 srcType, uint32 dstType, _rtl8651_l34AdditionalExec_t *para);

/*********************************************************************************************************
	Fragment Shortest Path Forwarding
**********************************************************************************************************/
#define RTL8651_FRAGMENT_SHORTESTPATH_FWD
#ifdef RTL8651_FRAGMENT_SHORTESTPATH_FWD

#define RTL8651_FRAGMENT_SHORTESTPATH_STATUS_HALFOK	1
#define RTL8651_FRAGMENT_SHORTESTPATH_STATUS_SETOK	2

#define RTL8651_FRAGMENT_SHORTESTPATH_MODE_MODIFYSIP	0
#define RTL8651_FRAGMENT_SHORTESTPATH_MODE_MODIFYDIP	1

#define RTL8651_FRAGMENT_SHORTESTPATH_TIMEOUT			3

typedef struct _rtl8651_FragmentShortestPathFwdCache_s
{
	/* information to search flow entry */
	uint8 status;
	ipaddr_t sip;
	ipaddr_t dip;
	uint16 id;
	/* information to forward matched packet */
	uint8 mode;
	ipaddr_t aliasIp;
//	rtl8651_tblDrv_vlanTable_t *dst_vlan;
	struct ether_addr smac;
	struct ether_addr dmac;
	uint16 vlanIdx;
	uint16 portList;
	uint8 extPortList;
	uint8 srcExtPortNum;
	uint16 pppoeIdx;
	uint16 flags;		/* checksum & pppoe_auto_add */
	/* timeout */
	uint8 age;
} _rtl8651_FragmentShortestPathFwdCache_t;

#define RTL8651_FRAGMENTSHORTESTPATH_TBLSIZE	32	/* must be power-of-2 */

inline void _rtl8651_fragmentShortestPathCacheSet_secondHalf(struct rtl_pktHdr *pkthdr, uint8 *m_data, struct ip*pip);
extern _rtl8651_FragmentShortestPathFwdCache_t fragmentShortestPathCache[RTL8651_FRAGMENTSHORTESTPATH_TBLSIZE];
#endif /* RTL8651_FRAGMENT_SHORTESTPATH_FWD */

/*********************************************************************************************************
	ICMP reply
**********************************************************************************************************/
// define parameters passed to fwd-engine ICMP reply functions
typedef struct _rtl8651_IcmpPktField_s{
	uint8	ic_type;
	uint8	ic_code;
	union{
		uint16	next_mtu;
	} ic_hun;
	#define ic_mtu	ic_hun.next_mtu
} _rtl8651_IcmpPktField_t;
int32 _rtl8651_drvIcmpErrorGeneration(struct rtl_pktHdr *, struct ip*, _rtl8651_IcmpPktField_t *, uint16);

/*********************************************************************************************************
	UPnP
**********************************************************************************************************/
#ifdef _RTL_UPNP
struct _RTL8651_UPNP_ENTRY{
	ipaddr_t remote_ip;
	uint16 remote_port; 
	ipaddr_t alias_ip;
	uint16 alias_port;
	ipaddr_t local_ip;
	uint16 local_port;	
	
	uint32 flags;
	uint32 age; /* second */
	uint32 algIdx; /* used by ALG QoS */
};

/*
	uPnP's internal control flags

	we MUST make sure that the internal control flags are in the mask of UPNP_INTERNAL_FLAG (defined in rtl8651_tblDrvFwd.h)
*/
#define UPNP_ALG_QOS	ALG_QOS

#define UPNP_ITEM		60
#define UPNP_AGE		120

void _rtl8651_addAlgQosUpnpMap(uint32, ipaddr_t, uint16, ipaddr_t, uint16, ipaddr_t, uint16, uint32);
#endif /* _RTL_UPNP */

/*********************************************************************************************************
	Trigger port
**********************************************************************************************************/
#ifdef _RTL_TRIGGER_PORT
#define RTL8651_TRIGGER_PORT_DEFAULT_ENTRYCNT	64		/* default count of trigger port entry */
#define RTL8651_TRIGGER_PORT_DEFAULT_CACHESIZE	128		/* default count of trigger port cache */
#define RTL8651_TRIGGER_PORT_HASH_CACHE_TBLSIZE	8		/* Note: Must keep it as Power of 2 */

#define RTL8651_TRIGGER_PORT_DEFAULT_TIMEOUT	3600	/* For backward compatible, new external API (rtl8651_addTriggerPortMappingRule) always request to age */

struct _rtl8651_triggerPort_cache_s;

typedef struct _rtl8651_triggerPort_entry_s
{
	uint32	netIntfId;

	uint8	inType;
	uint8	outType;
	uint16	inPortStart; 
	uint16	inPortEnd;
	uint16	outPortStart; 
	uint16	outPortEnd;
	ipaddr_t	triggeringInsideLocalIpAddrStart;
	ipaddr_t	triggeringInsideLocalIpAddrEnd;

	/*
		Age time of cache-flow, value '0' means the cache-flow triggered by this rule would not be timed-out by age timer.
		BUT flow ALWAYS can be removed due to LRU replacement.
	*/
	uint32 cacheAge;

	/* <----------- structure control ------------> */
	/* entry */
	struct _rtl8651_triggerPort_entry_s **hdr;
	struct _rtl8651_triggerPort_entry_s *next;
	struct _rtl8651_triggerPort_entry_s *prev;

	/* normal cache */
	struct _rtl8651_triggerPort_cache_s *normalCache;
} _rtl8651_triggerPort_entry_t;

typedef struct _rtl8651_triggerPort_cache_s
{
	uint32 netIntfId;
	ipaddr_t localIpAddr;
	ipaddr_t ExternalIpAddr;

	uint8 inType;
	uint16 inPortStart;
	uint16 inPortEnd;

	/*
		There are 4 different situations in TRIGGER-PORT control module:

			isStatic		age									outcome
		==================================================================================================
			TRUE		= 0				The cache entry would NEVER timeout and NEVER be removed by LRU replacement.
										It would always exist in system until user reinit trigger port system or call API to flush all trigger port entries.

			TRUE		> 0				This cache entry would not be removed by LRU replacement but could be timed-out.

			FALSE		= 0				This cache entry would NEVER timeout, but it can be replaced by newly cache addition.

			FALSE		> 0				This cache entry can be removed by both of LRU replacement mechanism and age timeout.
	*/

	uint8 isStatic;					/* If isStatic == TRUE, this cache ALWAYS CAN NOT removed by LRU replacement mechanism. */
	uint32 age;						/* age for this trigger port entry. Value '0' means this cache would not be timed-out */
	uint32 maxAge;					/* to store the max age count (mainly we use for STATIC cache, but we use it to dynamic cache, too. ) */

	/* <----------- structure control ------------> */
	/* cache */
	struct _rtl8651_triggerPort_cache_s **hdr;
	struct _rtl8651_triggerPort_cache_s *prev;
	struct _rtl8651_triggerPort_cache_s *next;

	/* normal entry */
	struct _rtl8651_triggerPort_entry_s *entryHdr;
	struct _rtl8651_triggerPort_cache_s *entryList_prev;
	struct _rtl8651_triggerPort_cache_s *entryList_next;

	/* LRU */
	struct _rtl8651_triggerPort_cache_s *lru_prev;
	struct _rtl8651_triggerPort_cache_s *lru_next;

} _rtl8651_triggerPort_cache_t;

#endif /* _RTL_TRIGGER_PORT */


/*********************************************************************************************************
	Port bouncing
**********************************************************************************************************/
#ifdef _RTL_PORT_BOUNCING
#define PORT_BOUNCING_ITEM 64
struct _RTL8651_PORT_BOUNCING_ENTRY{
	
	ipaddr_t clientIpAddr;
	ipaddr_t gatewayIpAddr;	
	ipaddr_t serverIpAddr;
	uint16 clientPort; 
	uint16 gatewayPort; 	
	uint16 serverPort;
	uint16 id;					// for ICMP flow: l4 id field
	uint16 flowType;
	uint32 tcpFlag;				// state for tcp flow
	uint32 age;
};
#endif /* _RTL_PORT_BOUNCING */

/*********************************************************************************************************
	Fragment cache
**********************************************************************************************************/
#ifdef _RTL_NAPT_FRAG
#define DEFAULT_MAX_FRAGPKT_NUM				32
#define DEFAULT_MAX_FRAGCACHE_AGE			5
#define INBOUND_FRAG							0
#define OUTBOUND_FRAG							1
#define BOUNCING_FRAG							2
#define ICMP_REPLY_FRAG						3
#define PROTOCOL_STACK_FRAG					4
#define NAPT_REDIRECT_FRAG						5
#define NAPT_REDIRECT_FRAG_INBOUND			6
#define TRAP_DISPATCH_FRAG					7
#define DSCP_FRAG								8
#define NAPT_CHANGEPORT_FRAG					9	/* Hyking Cache Pkt which need to modify SPort when PSA Reserver*/
#define RTL8651_FRAGCACHE_TYPE_COUNT			10	/* max count of fragment cache */
typedef struct _rtl8651_frag {
	/* <--- Informations ---> */
	ipaddr_t		new_ip;
	int8			routeIdx;
	void 		*napt_entry_t;
	uint8		dsid;
	/* <--- Control Informations ---> */
	uint8		age;
	/* <--- Index ---> */
	uint16		ip_id;
	ipaddr_t		org_sip;
	ipaddr_t		org_dip;
} _rtl8651_frag_t;
#endif /* _RTL_NAPT_FRAG */


/*********************************************************************************************************
	Pppoe active session table
**********************************************************************************************************/
typedef struct _rtl8651_pppoeActiveSession_entry_s {
	uint16 sessionId;
	uint16 keepAlive;//When value down-count to zero, this entry becomes invalid
	struct _rtl8651_pppoeActiveSession_entry_s * next;
} _rtl8651_pppoeActiveSession_entry_t;

/*********************************************************************************************************
	mbuf related
**********************************************************************************************************/
#ifdef CONFIG_RTL865X_MBUF_HEADROOM
#define FWDENG_MBUF_HEADROOM	(CONFIG_RTL865X_MBUF_HEADROOM)
#else
#define FWDENG_MBUF_HEADROOM	128  /*default headroom reserved*/
#endif

/*********************************************************************************************************
	forwarding engine initiation
**********************************************************************************************************/
extern rtl8651_fwdEngineInitPara_t rtl8651_fwdEnginePara;
#define FWD_INIT_CHECK(expr) do {\
	if((expr)!=SUCCESS){\
		rtlglue_printf("Error >>> initialize failed at line %d!!!\n", __LINE__);\
		rtlglue_drvMutexUnlock();\
		return FAILED;\
	}\
}while(0)

/*********************************************************************************************************
	protocol stack action
**********************************************************************************************************/
//#define RTL8651_MAX_PROTO_STACK_USED_UDP_PORTS 8
//#define RTL8651_MAX_PROTO_STACK_SERVER_USED_TCP_PORTS 8


/*********************************************************************************************************
	Trapping Dispatch system
**********************************************************************************************************/
#ifdef _RTL_TRAPPING_DISPATCH
#define RTL8651_TRAPPING_DISPATCH_DEFAULT_ENTRYCNT		128

typedef struct _rtl8651_trapping_dispatch_entry_s
{
	uint8 l4Proto;			/* IPPROTO_TCP, IPPROTO_UDP, IPPROTO_ICMP ... etc */

	/* Layer 3 : for IP layer */
	ipaddr_t srcIp;
	ipaddr_t dstIp;

	/* Layer 4 : for TCP and UDP */
	int32 srcPort;
	int32 dstPort;

	/* the category of this specific dispatch entry */
	uint16 category;

	/* <--- data structure ---> */
	struct _rtl8651_trapping_dispatch_entry_s *prev;
	struct _rtl8651_trapping_dispatch_entry_s *next;
} _rtl8651_trapping_dispatch_entry_t;

typedef struct _rtl8651_trapping_dispatch_cache_s
{
	uint8 l4Proto;
	ipaddr_t srcIp;
	ipaddr_t dstIp;
	uint16 srcPort;
	uint16 dstPort;

	_rtl8651_trapping_dispatch_entry_t *ptr;

} _rtl8651_trapping_dispatch_cache_t;

#endif /* _RTL_TRAPPING_DISPATCH */

/*********************************************************************************************************
	ARP Queue
**********************************************************************************************************/
#ifdef _RTL_QUEUE_ARP_PACKET

#define		QUEUE_PACKET_ITEM	32
#define		Q_ARP					1
#define		Q_TIME					3

struct _RTL8651_QUEUE_PACKET_ENTRY
{
	uint8 q_type;
	uint8 q_time;
	uint16 q_id;
	uint32 q_waitIp;
	struct rtl_pktHdr * q_pkthdrPtr;
	struct ip * q_ip;
};
#endif /* _RTL_QUEUE_ARP_PACKET */

/*********************************************************************************************************
	Checksum
**********************************************************************************************************/
uint16  _rtl8651_ipChecksum(struct ip * pip);
uint16 _rtl8651_icmpChecksum(struct ip * pip);
uint16 _rtl8651_tcpChecksum(struct ip * pip);	
void _rtl8651_fwdEngineTxChecksumOffload(struct ip *pip, uint32 l3Cksum, uint32 l4Cksum);

/* Checksum adjustment */
#define	_RTL8651_ADJUST_CHECKSUM(acc, cksum) \
	do { \
		acc += ntohs(cksum); \
		if (acc < 0) { \
			acc = -acc; \
			acc = (acc >> 16) + (acc & 0xffff); \
			acc += acc >> 16; \
			cksum = htons((uint16) ~acc); \
		} else { \
			acc = (acc >> 16) + (acc & 0xffff); \
			acc += acc >> 16; \
			cksum = htons((uint16) acc); \
		} \
	} while (0)

#define _RTL8651_DATA_ADJUST_CHECKSUM(data_mod, data_org, cksum) \
	do { \
		int32 accumulate; \
		accumulate = ((data_org) - (data_mod)); \
		_RTL8651_ADJUST_CHECKSUM( accumulate, (cksum)); \
	} while(0)
	
#define _RTL8651_L4_ADJUST_CHECKSUM(ip_mod, ip_org, port_mod, port_org, cksum) \
	do { \
		int32 accumulate = 0; \
		if (((ip_mod) != 0) && ((ip_org) != 0)){ \
			accumulate = ((ip_org) & 0xffff); \
			accumulate += (( (ip_org) >> 16 ) & 0xffff); \
			accumulate -= ((ip_mod) & 0xffff); \
			accumulate -= (( (ip_mod) >> 16 ) & 0xffff); \
		} \
		if (((port_mod) != 0) && ((port_org) != 0)){ \
			accumulate += (port_org); \
			accumulate -= (port_mod); \
		} \
		_RTL8651_ADJUST_CHECKSUM(accumulate, (cksum)); \
	}while(0)

#define _RTL8651_L3_ADJUST_CHECKSUM(ip_mod, ip_org, cksum) \
	do {\
		_RTL8651_L4_ADJUST_CHECKSUM((ip_mod), (ip_org), 0, 0, (cksum)); \
	}while(0)

/*********************************************************************************************************
	GRE/PPTP
**********************************************************************************************************/
#define hton8(x)  (x)
#define ntoh8(x)  (x)
#define hton16(x) htons(x)
#define ntoh16(x) ntohs(x)
#define hton32(x) htonl(x)
#define ntoh32(x) ntohl(x)

#if 0
#define PPTP_GRE_PROTO  0x880B
#define PPTP_GRE_VER    0x1

#define PPTP_GRE_FLAG_C	0x80
#define PPTP_GRE_FLAG_R	0x40
#define PPTP_GRE_FLAG_K	0x20
#define PPTP_GRE_FLAG_S	0x10
#define PPTP_GRE_FLAG_A	0x80

#define PPTP_GRE_IS_C(f) ((f)& PPTP_GRE_FLAG_C)
#define PPTP_GRE_IS_R(f) ((f)& PPTP_GRE_FLAG_R)
#define PPTP_GRE_IS_K(f) ((f)& PPTP_GRE_FLAG_K)
#define PPTP_GRE_IS_S(f) ((f)& PPTP_GRE_FLAG_S)
#define PPTP_GRE_IS_A(f) ((f)& PPTP_GRE_FLAG_A)
#endif

struct wan_gre_header {
  uint8 flags;			/* bitfield */
  uint8 ver;			/* should be PPTP_GRE_VER (enhanced GRE) */
  uint16 protocol;		/* should be PPTP_GRE_PROTO (ppp-encaps) */
  uint16 payload_len;	/* size of ppp payload, not inc. gre header */
  uint16 call_id;		/* peer's call_id for this session */
  uint32 seq;			/* sequence number.  Present if S==1 */
  uint32 ack;			/* seq number of highest packet recieved by */
  						/*  sender in this session */
};
#if 0
struct _RTL8651_PPTP_SHARE_DATA {
		uint32 seq;
		uint32 ack; 
		uint32 seq_recv;	
		uint32 seq_sent;
		uint32 ack_sent;
		uint32 ack_recv;
		
		uint32 tunnel_dst_ip;
		uint32 tunnel_src_ip;
		
		uint16 pptp_gre_call_id;
		uint16 pptp_gre_peer_call_id;
		
		uint8 reserv[128];
};
#endif

/*********************************************************************************************************
	L2TP
**********************************************************************************************************/
struct wan_l2tp_header {

  uint16 flags_type:1;
  uint16 flags_length:1;
  uint16 flags_rsv1:2;
  uint16 flags_sequence:1;
  uint16 flags_rsv2:1;
  uint16 flags_offset:1;
  uint16 flags_priority:1;  
  uint16 flags_rsv3:4;  
  uint16 flags_version:4;
  
  uint16 length;
  uint16 tunnel_id;
  uint16 session_id;  
};

/* AVP attribute type */
#define L2TP_AVP_ATTRIBUTE_CTRLMSG		0

/*
	L2TP control message type
	RFC 2661 (3.2)
*/
#define L2TP_CTRLMSG_RESV1					0	/* reserved */
#define L2TP_CTRLMSG_SCCRQ					1	/* Start-Control-Connection-Request */
#define L2TP_CTRLMSG_SCCRP					2	/* Start-Control-Connection-Reply */
#define L2TP_CTRLMSG_SCCCN					3	/* Start-Control-Connection-Connected */
#define L2TP_CTRLMSG_StopCCN				4	/* Stop-Control-Connection-Notification */
#define L2TP_CTRLMSG_RESV2					5	/* reserved */
#define L2TP_CTRLMSG_HELLO					6	/* Hello */

#define L2TP_CTRLMSG_OCRQ					7	/* Outgoing-Call-Request */
#define L2TP_CTRLMSG_OCRP					8	/* Outgoing-Call-Reply */
#define L2TP_CTRLMSG_OCCN					9	/* Outgoing-Call-Connected */
#define L2TP_CTRLMSG_ICRQ					10	/* Incoming-Call-Request */
#define L2TP_CTRLMSG_ICRP					11	/* Incoming-Call-Reply */
#define L2TP_CTRLMSG_ICCN					12	/* Incoming-Call-Connected */
#define L2TP_CTRLMSG_RESV3					13	/* reserved */
#define L2TP_CTRLMSG_CDN					14	/* Call-Disconnect-Notify */

#define L2TP_CTRLMSG_WEN					15	/* WAN-Error-Notify */

#define L2TP_CTRLMSG_SLI					16	/* Set-Link-Info */


/*********************************************************************************************************
	New Napt Process Module
**********************************************************************************************************/
#ifdef	_RTL_NEW_NAPT_MODULE
/* This struct only used by NaptFlow, so define it here. */
typedef struct {
	int8		outbound;
	int8		isTcp;		/* this Byte is also used to indicate getFromIp */
	int8 		isPortBouncingFlow;
	int16	assigned;
	uint16	srcPort;
	uint16	dstPort;
	uint16	aliasPort;	
	ipaddr_t	srcIp;
	ipaddr_t	dstIp;
	ipaddr_t	aliasIp;
}	rtl8651_tblDrvFwd_naptFlowInfo;

typedef struct {
	int32	flags;
	int32	(*func)(rtl8651_tblDrvFwd_naptFlowInfo *info,struct rtl_pktHdr *phdr, struct ip *pip);
}	rtl8651_tblDrvFwd_naptFlowOp;

#define	NAPT_FLOW_GETIP_FLAG		-0x7f
#define	NAPT_FLOW_IS_GETIP(__ptInfo__)	(((__ptInfo__)->isTcp==NAPT_FLOW_GETIP_FLAG)?TRUE:FALSE)

#define	NAPT_FLOW_MAX_FUNC_NUM	6
#define	NAPT_FLOW_INBOUND_MAX_FUNC_NUM		5
#define	NAPT_FLOW_OUTBOUND_MAX_FUNC_NUM	3

#define	FWDENG_PRI_IN_TOO_MUCH		1
#define	FWDENG_PRI_IN_NONEXIT		2
#define	FWDENG_PRI_IN_LASTNOTGETIP	3
#define	FWDENG_PRI_OUT_TOO_MUCH	4
#define	FWDENG_PRI_OUT_NONEXIT		5
#define	FWDENG_PRI_OUT_LASTISPS		6

#define	NAPT_FLOW_DIR_INBOUND	0x10
#define	NAPT_FLOW_DIR_OUTBOUND	0x20
#define	NAPT_FLOW_DIR_MASK		0x30
#define	NAPT_FLOW_DIR_OFFSET	0x4


#define	NAPT_FLOW_GETIP_POLICYRT	0x1f
#define	NAPT_FLOW_GETIP_NORMAL	0x1e
/*	
	this defination is associated with the default priority:
	it's means the default priority is :
for Inboud
	server port > trigger port > upnp > protocal stack action > dmz
for Outbound
	server port > upnp > getip
	If the default priority is changed, this defination should be changed too.
*/

#define	NAPT_FLOW_PRIORITY_TYPE_MAKS	0xf
#define	NAPT_FLOW_PRIORITY_TYPE_OFFSET	0x0
/* NAPT_FLOW_DIR_INBOUND|NAPT_FLOW_DIR_OUTBOUND|0x1 */
#define	NAPT_FLOW_PRIORITY_SERVERPORT		0x31
/* NAPT_FLOW_DIR_INBOUND|0x2 */
#define	NAPT_FLOW_PRIORITY_TRIGGERPORT	0x12
/* NAPT_FLOW_DIR_INBOUND|NAPT_FLOW_DIR_OUTBOUND|0x3 */
#define	NAPT_FLOW_PRIORITY_UPNP			0x33
/* NAPT_FLOW_DIR_INBOUND|0x4 */
#define	NAPT_FLOW_PRIORITY_PSCHECK			0x14
/* NAPT_FLOW_DIR_INBOUND|0x5 */
#define	NAPT_FLOW_PRIORITY_DMZ				0x15
/* NAPT_FLOW_DIR_OUTBOUND|0x6 */
#define	NAPT_FLOW_PRIORITY_GETEXTIP		0x26


#endif


/*********************************************************************************************************
	Port Scan
**********************************************************************************************************/
#if 0
//for ACK Scan, SYN Scan, FIN Scan
#define PORT_SCAN_WAIT_RST 2
struct portScanInfo {
	ipaddr_t sip;
	ipaddr_t dip;
	uint32 dsid;
	uint16 sport;
	uint16 dport;
	uint8 flags;
	uint8 age; 
};
#endif

/*********************************************************************************************************
	NAPT Redirect
**********************************************************************************************************/
/* time out of napt redirect cache */
#define RTL8651_NAPTREDIRECT_CACHETIMEOUT				32
#define RTL8651_NAPTREDIRECT_DEFAULT_TABLESIZE			16
#define RTL8651_NAPTREDIRECT_DEFAULT_FLOWCACHESIZE		32

/* Redirect entry */
typedef struct _rtl8651_naptRedirectEntry_s
{
	uint8	isExceptionList;
	uint8	isTcp;
	ipaddr_t	sip;
	uint16	sport;
	ipaddr_t	dip;
	uint16	dport;
	ipaddr_t	newDip;
	int32	newDport;
	/******* linked list *******/
	struct _rtl8651_naptRedirectEntry_s *next;
} _rtl8651_naptRedirectEntry_t;

/* Redirected flow */
typedef struct _rtl8651_naptRedirectFlow_s
{
	uint8	isTcp;
	ipaddr_t	sip;
	uint16	sport;
	ipaddr_t	dip;
	uint16	dport;
	ipaddr_t	newDip;
	uint16	newDport;
	uint32	age;
	uint32	queryCnt;			/* query count from protocol stack */
	/******* linked list *******/
	struct _rtl8651_naptRedirectFlow_s *next;
} _rtl8651_naptRedirectFlow_t;


/*********************************************************************************************************
	PPTP-Header Cache
**********************************************************************************************************/
#ifdef _RTL_PPTPL2TP_HDR_REFILL
typedef struct _rtl8651_pptpHdrCacheEntry_s
{
	/* <-- data to be cached --> */
	uint8 type;
	uint32 seq;
	uint32 ack;
	/* <-- key - we only support ip-in-GRE type, so we can store id/offset in ip-header as the key to be hashed --> */
	uint16 id;
	uint16 offset;
	/* <-- for linked list -->*/
	struct _rtl8651_pptpHdrCacheEntry_s **hdr;
	struct _rtl8651_pptpHdrCacheEntry_s *prev;
	struct _rtl8651_pptpHdrCacheEntry_s *next;
	struct _rtl8651_pptpHdrCacheEntry_s *lru_prev;
	struct _rtl8651_pptpHdrCacheEntry_s *lru_next;
} _rtl8651_pptpHdrCacheEntry_t;

#define RTL8651_PPTPHDR_SEQ_SET	0x01
#define RTL8651_PPTPHDR_ACK_SET	0x02

#define RTL8651_PPTPHDR_CACHE_HASH_TBLSIZE	64
#define RTL8651_PPTPHDR_CACHE_DEFAULT_ENTRYCNT	192
/* easy hash --> hash table size MUST be power of 2 */
#define RTL8651_IP_ID_EASYHASH(sip, dip, id, hashTblSize)	(((uint32)(sip)+(uint32)(dip)+(uint32)(htons(id))) & (hashTblSize-1))

void _rtl8651_pptpHdrCache_Add(uint16 ip_id, uint16 ip_off, uint32 seq, uint32 ack, uint8 type);
#endif /* _RTL_PPTPL2TP_HDR_REFILL */

/*********************************************************************************************************
	mtu
**********************************************************************************************************/
int32 _rtl8651_mtuFragment(uint32 property, struct rtl_pktHdr *pkthdrPtr, uint32 mtu, uint16 dvid, int8 isNapt, uint16 srcPortList);

/*********************************************************************************************************
	misc
**********************************************************************************************************/
/* hash function */
#define TWO_ITEM_EASYHASH(item1,item2,hashTblSize)		(((uint32)(item1) + (uint32)(item2)) & ((hashTblSize) - 1))
#define THREE_ITEM_EASYHASH(item1,item2,item3,hashTblSize)		(((uint32)(item1) + (uint32)(item2) + (uint32)(item3)) & ((hashTblSize) - 1))
/* fragment check */
#define DONT_FRAG(off)				(ntohs((uint16)(off)) & (IP_DF))
#define IS_FRAGMENT(off)				(ntohs((uint16)(off)) & (IP_OFFMASK|IP_MF))
#define IS_FRAGMENT_FIRST(off)		((ntohs((uint16)(off)) & (IP_OFFMASK|IP_MF)) == IP_MF)
#define IS_FRAGMENT_REMAIN(off)	(ntohs((uint16)(off)) & IP_OFFMASK)
/* ip header length related */
#define IPHDRLEN_IS_TWENTY(iphdr)		(((*(uint8*)iphdr)&0xf) == 5)					/* check IP header Length is 20 or not */
#define IPHDR_LEN(iphdr)					(((*(uint8*)iphdr)&0xf) << 2)					/* get the ip length */
#define L4HDR_ADDR(iphdr)				((uint8 *)iphdr + (((*(uint8*)iphdr)&0xf) << 2))	/* calculate L4 header address by ip header */
#define TCPDATA_ADDR(tcpHdr)			((uint32)tcpHdr + (uint32)((((struct tcphdr*)tcpHdr)->th_off_x & 0xf0) >> 2))	/* calculate TCP Data address by tcp header */
#define _RTL8651_TTL_DEC_1(ip_ttl, ip_sum) \
	do { \
		int32 ttl_mod = 0x0100; \
		(ip_ttl) --; \
		_RTL8651_ADJUST_CHECKSUM(ttl_mod, (ip_sum)); \
	} while(0)


/* set broadcast mac */
#define BCAST_MAC(pMAC) \
		do { \
			memset(pMAC, 0xff, 6); \
		} while (0);

/* extern variables */
extern int8 rtl8651_drvMulticastProcessEnable;
extern int8 rtl8651_drvPacketFastSend;
extern int8 rtl8651_drvIcmpRoutingMsgEnable;
extern int8 rtl8651_drvSoftwareBroadcastEnable[RTL8651_VLAN_NUMBER];	/* enable/disable SW broadcast */
extern rtl8651_tblDrvFwdEngineCounts_t rtl8651_fwdEngineCounter;

/*********************************************************************************************************
	ALG
**********************************************************************************************************/
inline void _rtl8651_l4TcpAdjustMss(struct ip *ip, struct tcphdr *tc, ipaddr_t intfIp, int32 delta);
int32 _rtl8651_isdigit(int32);
int32 _rtl8651_isspace(int32);
void _rtl8651_l4AddSeq(struct ip *pip, rtl8651_tblDrv_naptTcpUdpFlowEntry_t *tb, int32 delta);
int32 _rtl8651_l4GetDeltaSeqOut(struct ip * pip, struct tcphdr *tc, rtl8651_tblDrv_naptTcpUdpFlowEntry_t  *tb);

/*********************************************************************************************************
	Diffserv callback function
**********************************************************************************************************/
inline  int32 _rtl8651_setDSCPEnable(int8 enable);
int32 _rtl8651_markDiffserv( int32, struct rtl_pktHdr *, struct ip *, void * );

#endif /* RTL8651_FWDENGINE_LOCAL_H */
#ifdef _RTL_PORT_BOUNCING_ALG
int32 rtl8651_PortBouncing_ALG_init(void);
#endif
