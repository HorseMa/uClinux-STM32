#ifndef RTL8651_LOG_LOCALE_H
#define RTL8651_LOG_LOCALE_H

#include "../rtl865x/rtl8651_tblDrv.h"

#ifdef _RTL_NEW_LOGGING_MODEL

/******************************
	Descriptor
*******************************/
static char *log_desc[RTL8651_TOTAL_USERLOG_NO] =
{
	"[向外新的TCP链结]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptOutbound */
	"[向内新的TCP链结]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptInbound */
	"[向外新的UDP链结]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptOutbound */
	"[向内新的UDP链结]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptInbound */
	"[向外新的ICMP链结]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptOutbound */
	"[向内新的ICMP链结]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptInbound */
	"[由出口之ACL丢弃]",					/* RTL8651_LOG_ACL_EgressAclDropLog */
	"[由入口之ACL丢弃]",					/* RTL8651_LOG_ACL_IngressAclDropLog */
	"[URL连结侦测]",						/* RTL8651_LOG_URL_MatchUrlFilter */
	"[侦测到 IP Spoofing攻击]",				/* RTL8651_LOG_DOS_IpSpoof */
	"[侦测到 UDP Flood攻击]",				/* RTL8651_LOG_DOS_UdpFlood */
	"[侦测到来源IP进行 UDP Flood攻击]",			/* RTL8651_LOG_DOS_HostUdpFlood */
	"[侦测到 UDP Land攻击]",				/* RTL8651_LOG_DOS_UdpLand */
	"[侦测到 UDP Bomb攻击]",				/* RTL8651_LOG_DOS_UdpBomb */
	"[侦测到 UDP EchoChargen攻击]",				/* RTL8651_LOG_DOS_UdpEchoChargen */
	"[侦测到 ICMP Land攻击]",				/* RTL8651_LOG_DOS_IcmpLand */
	"[侦测到 Ping Of Death攻击]",				/* RTL8651_LOG_DOS_IcmpPingOfDeath */
	"[侦测到 ICMP Flood攻击]",				/* RTL8651_LOG_DOS_IcmpFlood */
	"[侦测到来源IP进行 ICMP Flood攻击]",			/* RTL8651_LOG_DOS_HostIcmpFlood */
	"[侦测到 ICMP Smurf攻击]",				/* RTL8651_LOG_DOS_IcmpSmurf */
	"[侦测到 SYN Flood攻击]",				/* RTL8651_LOG_DOS_SynFlood */
	"[侦测到来源IP进行 SYN Flood攻击]",			/* RTL8651_LOG_DOS_HostSynFlood */
	"[侦测到 Stealth FIN攻击]",				/* RTL8651_LOG_DOS_StealthFin */
	"[侦测到来源IP进行 Stealth FIN攻击]",			/* RTL8651_LOG_DOS_HostStealthFin */
	"[侦测到 TCP Land攻击]",				/* RTL8651_LOG_DOS_TcpLand */
	"[侦测到 TCP 扫描]",					/* RTL8651_LOG_DOS_TcpScan */
	"[侦测到 SYN with Data攻击]",				/* RTL8651_LOG_DOS_TcpSynWithData */
	"[侦测到 IP TearDrop攻击]",				/* RTL8651_LOG_DOS_TearDrop */
	"[侦测到 SYN 扫描]",					/* RTL8651_LOG_DOS_TcpUdpScan_SYN */
	"[侦测到 FIN 扫描]",					/* RTL8651_LOG_DOS_TcpUdpScan_FIN */
	"[侦测到 ACK 扫描]",					/* RTL8651_LOG_DOS_TcpUdpScan_ACK */
	"[侦测到 UDP 扫描]",					/* RTL8651_LOG_DOS_TcpUdpScan_UDP */
	"[侦测到多重扫描]",						/* RTL8651_LOG_DOS_TcpUdpScan_HYBRID */
};
/******************************
	Action
*******************************/
static char *log_action[RTL8651_LOGACTION_NO] = 
{
	"无动作",
	"已丢弃",
	"已重设链结",
	"已传送"
};
/*******************************
	Direction
********************************/
static char *direction_action[RTL8651_DIRECTION_NO] =
{
	"内到外",
	"外到内"
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
#define RTL8651_LOG_TIME_STRING	"公元%3d年%02d月%02d日(%s %02d:%02d:%02d)"
#define RTL8651_LOG_TIME_VARLIST	tm.tm_year,tm.tm_mon+1,tm.tm_mday,(tm.tm_hour>=12?"PM":"AM"), (tm.tm_hour%12),tm.tm_min,tm.tm_sec
/******************************
 log detail format definition
******************************/
/* <--- pkt ---> */
/* tcp */
#define RTL8651_LOG_INFO_PKT_TCPSTRING	"(TCP) %s 来源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_TCPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_PKT_UDPSTRING	"(UDP) %s 来源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_UDPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* icmp */
#define RTL8651_LOG_INFO_PKT_ICMPSTRING	"(ICMP 形式:%d) %s 来源IP:%u.%u.%u.%u 目的IP:%u.%u.%u.%u(%d)[%s]"
#define RTL8651_LOG_INFO_PKT_ICMPVARLIST	\
	info->pkt_icmpType, \
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_icmpId, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_PKT_OTHERSTRING	"(其他) %s 来源IP:%u.%u.%u.%u 目的IP:%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_PKT_OTHERVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]

/* <--- url ---> */
/* tcp */
#ifdef RTL865XB_URLFILTER_LOGMOREINFO
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s/%s 拦截 %s %s ](TCP) %s 来源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
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
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s/%s 拦截 %s %s ](UDP) %s 来源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
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
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s/%s 拦截 %s %s ](其他) %s 来源IP:%u.%u.%u.%u 目的IP:%u.%u.%u.%u [%s]"
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
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s 被拦截](TCP) %s 来源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_TCPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s 被拦截](UDP) %s 来源IP:%u.%u.%u.%u:%d 目的IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_UDPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s 被拦截](其他) %s 来源IP:%u.%u.%u.%u 目的IP:%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_URL_OTHERVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]
#endif	/* RTL865XB_URLFILTER_LOGMOREINFO */


#endif /* _RTL_NEW_LOGGING_MODEL */
#endif /* RTL8651_LOG_LOCALE_H */
