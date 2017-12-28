/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :  
*/


#ifndef _RTL8651_DOS_LOCAL_H
#define _RTL8651_DOS_LOCAL_H

#include "rtl8651_tblDrvFwdLocal.h"

#undef DOS_DEBUG

/*
	Default values
*/
#define RTL8651_DOS_SYN_THRESHOLD_DEFAULT	50
#define RTL8651_DOS_FIN_THRESHOLD_DEFAULT	50
#define RTL8651_DOS_UDP_THRESHOLD_DEFAULT	50
#define RTL8651_DOS_ICMP_THRESHOLD_DEFAULT	50

#define RTL8651_DOS_PERSRC_SYN_THRESHOLD_DEFAULT	30
#define RTL8651_DOS_PERSRC_FIN_THRESHOLD_DEFAULT	30
#define RTL8651_DOS_PERSRC_UDP_THRESHOLD_DEFAULT	30
#define RTL8651_DOS_PERSRC_ICMP_THRESHOLD_DEFAULT	30

#define RTL8651_DOS_TCPCONN_THRESHOLD_DEFAULT		2048
#define RTL8651_DOS_UDPCONN_THRESHOLD_DEFAULT		2048
#define RTL8651_DOS_TCPUDPCONN_THRESHOLD_DEFAULT	4096

#define RTL8651_DOS_PERSRC_TCPCONN_THRESHOLD_DEFAULT		512
#define RTL8651_DOS_PERSRC_UDPCONN_THRESHOLD_DEFAULT		512
#define RTL8651_DOS_PERSRC_TCPUDPCONN_THRESHOLD_DEFAULT	1024

#define RTL8651_DOS_SYN_TRACKCNT			30
#define RTL8651_DOS_FIN_TRACKCNT			30
#define RTL8651_DOS_UDP_TRACKCNT			40
#define RTL8651_DOS_ICMP_TRACKCNT			20
#define RTL8651_DOS_TOTAL_TRACKCNT		120

#define RTL8651_DOS_TCPCONN_TRACKCNT		60
#define RTL8651_DOS_UDPCONN_TRACKCNT	60
#define RTL8651_DOS_TOTALCONN_TRACKCNT	120

#define RTL8651_DOS_SCAN_TOTAL_TRACKCNT					32
#define RTL8651_DOS_SCAN_MIN_MONITOR_WIN				3
#define RTL8651_DOS_SCAN_MIN_THRESHOLD					3
/*
	determine the numberator/denominator when decide what's the scan type
	ex. numberator = 4, denominator = 5 -> so the weight is 4/5 = 0.8

	1. if SYN Scan count exceeds (0.8 * Total Scan Count)	-> SYN Scan
	2. if FIN Scan count exceeds  (0.8 * Total Scan Count)	-> FIN Scan
	3. if ACK Scan count exceeds  (0.8 * Total Scan Count)	-> ACK Scan
	4. if UDP Scan count exceeds  (0.8 * Total Scan Count)	-> UDP Scan
	5. if no any scan count exceeds (0.8 * Total Scan Count)	-> Hybrid Scan

	Note : numerator/denominator must greater than 0.5 !
*/
#define RTL8651_DOS_SCAN_DECIDE_WEIGHT_NUMERATOR		4
#define RTL8651_DOS_SCAN_DECIDE_WEIGHT_DENOMINATOR	5
/*
	DOS process default values
*/
#define RTL8651_DOSPROC_SIPBLOCK_CNT			30
#define RTL8651_DOSPROC_SIPBLOCK_TIMEOUT		120	/* 2 mins */

/*
	Extern variable
*/
extern uint32		_rtl8651_dos_enable;
extern uint32		_rtl8651_dos_sw[RTL8651_MAX_DIALSESSION];
/*
        Internal function declaration
*/
void _rtl8651_dos_init(rtl8651_fwdEngineInitPara_t *);
void _rtl8651_dos_Reinit(void);

/*
	DOS process functions declaration
*/
/*
	called by DOS system
*/
int32 _rtl8651_procDoSPkt_init(rtl8651_fwdEngineInitPara_t *);
void _rtl8651_procDosPkt_reinit(void);
void _rtl8651_procDoSPkt(uint32 item, uint32 dsid, uint16 vid, struct ip*ip, uint8 direction, struct rtl_pktHdr *);
void _rtl8651_procDoSIP(uint32 item, uint32 dsid, uint16 vid, ipaddr_t ip);
/*
	called by forwarding engine
*/
int32 _rtl8651_dosProc_blockSip_check(uint16 vid, ipaddr_t sip);
/* scan tracking */
void _rtl8651_dosScanTracking(struct rtl_pktHdr*pkt, struct ip *ip, uint8 l4Type, void*l4hdr);
int32 _rtl8651_dosScanCheckComplain(uint16 dsid, struct ip *ip);
int32 _rtl8651_dosScanCheckTcpOutboundFailUnusual(uint16 dsid, struct ip *ip, struct tcphdr *tc);
/* Ignore Type Check */
inline uint8 _rtl8651_dosIgnoreTypeCheck(uint8 direction);
/*
	called by tbl driver
*/
// ACL
uint32 _rtl8651_addAclRuleForSipBlock(uint16 vid, uint32 aclStart, uint32 aclEnd, int8 *enoughFlag);
// timeUpdate
void _rtl8651_dosProc_blockSip_discharge(uint32);
/* Connection Control System */
/* per-source */
int32 _rtl8651_dosNaptConnCheck(uint8 isTcp, ipaddr_t ip, uint8 direction);
inline void _rtl8651_dosSrcConnCtrlUpdateConnection(uint8 isDecrease, uint8 isTcp, ipaddr_t ip);
/* Remove source tracking entry by IP (and FlowType) */
int32 _rtl8651_connSrcTrackingRemoveByFlowtypeIp(uint8, ipaddr_t);
int32 _rtl8651_connSrcTrackingRemoveByIp(ipaddr_t);

#endif /* _RTL8651_DOS_LOCAL_H */
