#ifndef RTL8651_LOG_LOCALE_H
#define RTL8651_LOG_LOCALE_H

#include "../rtl865x/rtl8651_tblDrv.h"

#ifdef _RTL_NEW_LOGGING_MODEL

/******************************
	Descriptor
*******************************/
static char *log_desc[RTL8651_TOTAL_USERLOG_NO] =
{
	"[向外新的TCP鏈結]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptOutbound */
	"[向內新的TCP鏈結]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptInbound */
	"[向外新的UDP鏈結]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptOutbound */
	"[向內新的UDP鏈結]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptInbound */
	"[向外新的ICMP鏈結]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptOutbound */
	"[向內新的ICMP鏈結]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptInbound */
	"[由出口之ACL丟棄]",					/* RTL8651_LOG_ACL_EgressAclDropLog */
	"[由入口之ACL丟棄]",					/* RTL8651_LOG_ACL_IngressAclDropLog */
	"[URL連結偵測]",						/* RTL8651_LOG_URL_MatchUrlFilter */
	"[偵測到 IP Spoofing攻擊]",				/* RTL8651_LOG_DOS_IpSpoof */
	"[偵測到 UDP Flood攻擊]",				/* RTL8651_LOG_DOS_UdpFlood */
	"[偵測到來源IP進行 UDP Flood攻擊]",			/* RTL8651_LOG_DOS_HostUdpFlood */
	"[偵測到 UDP Land攻擊]",				/* RTL8651_LOG_DOS_UdpLand */
	"[偵測到 UDP Bomb攻擊]",				/* RTL8651_LOG_DOS_UdpBomb */
	"[偵測到 UDP EchoChargen攻擊]",				/* RTL8651_LOG_DOS_UdpEchoChargen */
	"[偵測到 ICMP Land攻擊]",				/* RTL8651_LOG_DOS_IcmpLand */
	"[偵測到 Ping Of Death攻擊]",				/* RTL8651_LOG_DOS_IcmpPingOfDeath */
	"[偵測到 ICMP Flood攻擊]",				/* RTL8651_LOG_DOS_IcmpFlood */
	"[偵測到來源IP進行 ICMP Flood攻擊]",			/* RTL8651_LOG_DOS_HostIcmpFlood */
	"[偵測到 ICMP Smurf攻擊]",				/* RTL8651_LOG_DOS_IcmpSmurf */
	"[偵測到 SYN Flood攻擊]",				/* RTL8651_LOG_DOS_SynFlood */
	"[偵測到來源IP進行 SYN Flood攻擊]",			/* RTL8651_LOG_DOS_HostSynFlood */
	"[偵測到 Stealth FIN攻擊]",				/* RTL8651_LOG_DOS_StealthFin */
	"[偵測到來源IP進行 Stealth FIN攻擊]",			/* RTL8651_LOG_DOS_HostStealthFin */
	"[偵測到 TCP Land攻擊]",				/* RTL8651_LOG_DOS_TcpLand */
	"[偵測到 TCP 掃描]",					/* RTL8651_LOG_DOS_TcpScan */
	"[偵測到 SYN with Data攻擊]",				/* RTL8651_LOG_DOS_TcpSynWithData */
	"[偵測到 IP TearDrop攻擊]",				/* RTL8651_LOG_DOS_TearDrop */
	"[偵測到 SYN 掃描]",					/* RTL8651_LOG_DOS_TcpUdpScan_SYN */
	"[偵測到 FIN 掃描]",					/* RTL8651_LOG_DOS_TcpUdpScan_FIN */
	"[偵測到 ACK 掃描]",					/* RTL8651_LOG_DOS_TcpUdpScan_ACK */
	"[偵測到 UDP 掃描]",					/* RTL8651_LOG_DOS_TcpUdpScan_UDP */
	"[偵測到多重掃描]",						/* RTL8651_LOG_DOS_TcpUdpScan_HYBRID */
};
/******************************
	Action
*******************************/
static char *log_action[RTL8651_LOGACTION_NO] = 
{
	"無動作",
	"已丟棄",
	"已重設鏈結",
	"已傳送"
};
/*******************************
	Direction
********************************/
static char *direction_action[RTL8651_DIRECTION_NO] =
{
	"內到外",
	"外到內"
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
#define RTL8651_LOG_TIME_STRING	"民國%3d年%02d月%02d日(%s %02d:%02d:%02d)"
#define RTL8651_LOG_TIME_VARLIST	(tm.tm_year-1911),tm.tm_mon+1,tm.tm_mday,(tm.tm_hour>=12?"PM":"AM"), (tm.tm_hour%12),tm.tm_min,tm.tm_sec
/******************************
 log detail format definition
******************************/
/* <--- pkt ---> */
/* tcp */
#define RTL8651_LOG_INFO_PKT_TCPSTRING	"(TCP) %s 來源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_TCPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_PKT_UDPSTRING	"(UDP) %s 來源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_UDPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* icmp */
#define RTL8651_LOG_INFO_PKT_ICMPSTRING	"(ICMP 形式:%d) %s 來源IP:%u.%u.%u.%u 目的IP:%u.%u.%u.%u(%d)[%s]"
#define RTL8651_LOG_INFO_PKT_ICMPVARLIST	\
	info->pkt_icmpType, \
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_icmpId, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_PKT_OTHERSTRING	"(其他) %s 來源IP:%u.%u.%u.%u 目的IP:%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_PKT_OTHERVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]

/* <--- url ---> */
/* tcp */
#ifdef RTL865XB_URLFILTER_LOGMOREINFO
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s/%s 攔截 %s %s ](TCP) %s 來源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
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
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s/%s 攔截 %s %s ](UDP) %s 來源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
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
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s/%s 攔截 %s %s ](其他) %s 來源IP:%u.%u.%u.%u 目的IP:%u.%u.%u.%u [%s]"
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
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s 被攔截](TCP) %s 來源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_TCPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s 被攔截](UDP) %s 來源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_UDPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s 被攔截](其他) %s 來源IP:%u.%u.%u.%u 目的IP:%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_URL_OTHERVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]
#endif	/* RTL865XB_URLFILTER_LOGMOREINFO */


#endif /* _RTL_NEW_LOGGING_MODEL */
#endif /* RTL8651_LOG_LOCALE_H */
