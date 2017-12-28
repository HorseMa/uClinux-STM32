#ifndef RTL8651_LOG_LOCALE_H
#define RTL8651_LOG_LOCALE_H

#include "../rtl865x/rtl8651_tblDrv.h"

#ifdef _RTL_NEW_LOGGING_MODEL

/******************************
	Descriptor
*******************************/
static char *log_desc[RTL8651_TOTAL_USERLOG_NO] =
{
	"[New TCP Outbound Flow]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptOutbound */
	"[New TCP Inbound Flow]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptInbound */
	"[New UDP Outbound Flow]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptOutbound */
	"[New UDP Inbound Flow]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptInbound */
	"[New ICMP Outbound Flow]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptOutbound */
	"[New ICMP Inbound Flow]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptInbound */
	"[Egress ACL Drop]",						/* RTL8651_LOG_ACL_EgressAclDropLog */
	"[Ingress ACL Drop]",						/* RTL8651_LOG_ACL_IngressAclDropLog */
	"[Match URL Filter]",						/* RTL8651_LOG_URL_MatchUrlFilter */
	"[IP Spoofing]",							/* RTL8651_LOG_DOS_IpSpoof */
	"[UDP Flood]",								/* RTL8651_LOG_DOS_UdpFlood */
	"[HOST Attack: UDP Flood]",					/* RTL8651_LOG_DOS_HostUdpFlood */
	"[UDP Land]",								/* RTL8651_LOG_DOS_UdpLand */
	"[UDP Bomb]",							/* RTL8651_LOG_DOS_UdpBomb */
	"[UDP Echo Chargen]",						/* RTL8651_LOG_DOS_UdpEchoChargen */
	"[ICMP Land]",							/* RTL8651_LOG_DOS_IcmpLand */
	"[ICMP Ping Of Death]",						/* RTL8651_LOG_DOS_IcmpPingOfDeath */
	"[ICMP Flood]",							/* RTL8651_LOG_DOS_IcmpFlood */
	"[HOST Attack: ICMP Flood]",				/* RTL8651_LOG_DOS_HostIcmpFlood */
	"[ICMP Smurf]",							/* RTL8651_LOG_DOS_IcmpSmurf */
	"[TCP SYN Flood]",							/* RTL8651_LOG_DOS_SynFlood */
	"[HOST Attack: TCP SYN Flood]",				/* RTL8651_LOG_DOS_HostSynFlood */
	"[TCP Stealth FIN Port Scan]",				/* RTL8651_LOG_DOS_StealthFin */
	"[HOST Attack: TCP Stealth FIN Port Scan]",	/* RTL8651_LOG_DOS_HostStealthFin */
	"[Land]",									/* RTL8651_LOG_DOS_TcpLand */
	"[TCP Scan]",								/* RTL8651_LOG_DOS_TcpScan */
	"[TCP SYN with Data]",						/* RTL8651_LOG_DOS_TcpSynWithData */
	"[IP TearDrop]",							/* RTL8651_LOG_DOS_TearDrop */
	"[SYN Scan]",								/* RTL8651_LOG_DOS_TcpUdpScan_SYN */
	"[FIN Scan]",								/* RTL8651_LOG_DOS_TcpUdpScan_FIN */
	"[ACK Scan]",								/* RTL8651_LOG_DOS_TcpUdpScan_ACK */
	"[UDP Scan]",								/* RTL8651_LOG_DOS_TcpUdpScan_UDP */
	"[Hybrid Scan]",							/* RTL8651_LOG_DOS_TcpUdpScan_HYBRID */
};
/******************************
	Action
*******************************/
static char *log_action[RTL8651_LOGACTION_NO] = 
{
	"No Action",
	"Drop",
	"Reset",
	"Forward"
};
/*******************************
	Direction
********************************/
static char *direction_action[RTL8651_DIRECTION_NO] =
{
	"LAN to WAN",
	"WAN to LAN"
};
/*******************************
	Log String Layout:
					1. time
					2. descriptor
					3. detail
*******************************/
#define RTL8651_LOG_STRING_ITEM_STRING	"%s %s %s"
#define RTL8651_LOG_STRING_ITEM_VARLIST	\
		msg_time, msg_desc, msg_detail
/******************************
	Time
*******************************/
#define RTL8651_LOG_TIME_STRING	"%04d-%02d-%02d %02d:%02d:%02d"
#define RTL8651_LOG_TIME_VARLIST	tm.tm_year,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec
/******************************
 log detail format definition
******************************/
/* <--- pkt ---> */
/* tcp */
#define RTL8651_LOG_INFO_PKT_TCPSTRING	"(TCP) %s %u.%u.%u.%u:%d->%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_TCPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_PKT_UDPSTRING	"(UDP) %s %u.%u.%u.%u:%d->%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_UDPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* icmp */
#define RTL8651_LOG_INFO_PKT_ICMPSTRING	"(ICMP type:%d) %s %u.%u.%u.%u->%u.%u.%u.%u(%d) [%s]"
#define RTL8651_LOG_INFO_PKT_ICMPVARLIST	\
	info->pkt_icmpType, \
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_icmpId, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_PKT_OTHERSTRING	"(Other) %s %u.%u.%u.%u->%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_PKT_OTHERVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]

/* <--- url ---> */
/* tcp */
#ifdef RTL865XB_URLFILTER_LOGMOREINFO
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s/%s matched %s %s ](TCP) %s %u.%u.%u.%u:%d->%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_TCPVARLIST	\
	(info->url_string?info->url_string:""),\
	(info->url_pathString?info->url_pathString:""),\
	(info->url_urlFilterString?info->url_urlFilterString:""),\
	(info->url_pathFilterString?info->url_pathFilterString:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s/%s matched %s %s ](UDP) %s %u.%u.%u.%u:%d->%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_UDPVARLIST	\
	(info->url_string?info->url_string:""),\
	(info->url_pathString?info->url_pathString:""),\
	(info->url_urlFilterString?info->url_urlFilterString:""),\
	(info->url_pathFilterString?info->url_pathFilterString:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s/%s matched %s %s ](Other) %s %u.%u.%u.%u->%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_URL_OTHERVARLIST	\
	(info->url_string?info->url_string:""),\
	(info->url_pathString?info->url_pathString:""),\
	(info->url_urlFilterString?info->url_urlFilterString:""),\
	(info->url_pathFilterString?info->url_pathFilterString:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]
#else	/* RTL865XB_URLFILTER_LOGMOREINFO */
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s matched](TCP) %s %u.%u.%u.%u:%d->%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_TCPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s matched](UDP) %s %u.%u.%u.%u:%d->%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_UDPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s matched](Other) %s %u.%u.%u.%u->%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_URL_OTHERVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]
#endif	/* RTL865XB_URLFILTER_LOGMOREINFO */

#endif /* _RTL_NEW_LOGGING_MODEL */
#endif /* RTL8651_LOG_LOCALE_H */
