#ifndef RTL8651_LOG_LOCALE_H
#define RTL8651_LOG_LOCALE_H

#include "../rtl865x/rtl8651_tblDrv.h"

#ifdef _RTL_NEW_LOGGING_MODEL

/******************************
	Descriptor
*******************************/
static char *log_desc[RTL8651_TOTAL_USERLOG_NO] =
{
	"[�����µ�TCP����]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptOutbound */
	"[�����µ�TCP����]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptInbound */
	"[�����µ�UDP����]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptOutbound */
	"[�����µ�UDP����]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptInbound */
	"[�����µ�ICMP����]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptOutbound */
	"[�����µ�ICMP����]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptInbound */
	"[�ɳ���֮ACL����]",					/* RTL8651_LOG_ACL_EgressAclDropLog */
	"[�����֮ACL����]",					/* RTL8651_LOG_ACL_IngressAclDropLog */
	"[URL�������]",						/* RTL8651_LOG_URL_MatchUrlFilter */
	"[��⵽ IP Spoofing����]",				/* RTL8651_LOG_DOS_IpSpoof */
	"[��⵽ UDP Flood����]",				/* RTL8651_LOG_DOS_UdpFlood */
	"[��⵽��ԴIP���� UDP Flood����]",			/* RTL8651_LOG_DOS_HostUdpFlood */
	"[��⵽ UDP Land����]",				/* RTL8651_LOG_DOS_UdpLand */
	"[��⵽ UDP Bomb����]",				/* RTL8651_LOG_DOS_UdpBomb */
	"[��⵽ UDP EchoChargen����]",				/* RTL8651_LOG_DOS_UdpEchoChargen */
	"[��⵽ ICMP Land����]",				/* RTL8651_LOG_DOS_IcmpLand */
	"[��⵽ Ping Of Death����]",				/* RTL8651_LOG_DOS_IcmpPingOfDeath */
	"[��⵽ ICMP Flood����]",				/* RTL8651_LOG_DOS_IcmpFlood */
	"[��⵽��ԴIP���� ICMP Flood����]",			/* RTL8651_LOG_DOS_HostIcmpFlood */
	"[��⵽ ICMP Smurf����]",				/* RTL8651_LOG_DOS_IcmpSmurf */
	"[��⵽ SYN Flood����]",				/* RTL8651_LOG_DOS_SynFlood */
	"[��⵽��ԴIP���� SYN Flood����]",			/* RTL8651_LOG_DOS_HostSynFlood */
	"[��⵽ Stealth FIN����]",				/* RTL8651_LOG_DOS_StealthFin */
	"[��⵽��ԴIP���� Stealth FIN����]",			/* RTL8651_LOG_DOS_HostStealthFin */
	"[��⵽ TCP Land����]",				/* RTL8651_LOG_DOS_TcpLand */
	"[��⵽ TCP ɨ��]",					/* RTL8651_LOG_DOS_TcpScan */
	"[��⵽ SYN with Data����]",				/* RTL8651_LOG_DOS_TcpSynWithData */
	"[��⵽ IP TearDrop����]",				/* RTL8651_LOG_DOS_TearDrop */
	"[��⵽ SYN ɨ��]",					/* RTL8651_LOG_DOS_TcpUdpScan_SYN */
	"[��⵽ FIN ɨ��]",					/* RTL8651_LOG_DOS_TcpUdpScan_FIN */
	"[��⵽ ACK ɨ��]",					/* RTL8651_LOG_DOS_TcpUdpScan_ACK */
	"[��⵽ UDP ɨ��]",					/* RTL8651_LOG_DOS_TcpUdpScan_UDP */
	"[��⵽����ɨ��]",						/* RTL8651_LOG_DOS_TcpUdpScan_HYBRID */
};
/******************************
	Action
*******************************/
static char *log_action[RTL8651_LOGACTION_NO] = 
{
	"�޶���",
	"�Ѷ���",
	"����������",
	"�Ѵ���"
};
/*******************************
	Direction
********************************/
static char *direction_action[RTL8651_DIRECTION_NO] =
{
	"�ڵ���",
	"�⵽��"
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
#define RTL8651_LOG_TIME_STRING	"��Ԫ%3d��%02d��%02d��(%s %02d:%02d:%02d)"
#define RTL8651_LOG_TIME_VARLIST	tm.tm_year,tm.tm_mon+1,tm.tm_mday,(tm.tm_hour>=12?"PM":"AM"), (tm.tm_hour%12),tm.tm_min,tm.tm_sec
/******************************
 log detail format definition
******************************/
/* <--- pkt ---> */
/* tcp */
#define RTL8651_LOG_INFO_PKT_TCPSTRING	"(TCP) %s ��ԴIP:%u.%u.%u.%u:%d Ŀ��IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_TCPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_PKT_UDPSTRING	"(UDP) %s ��ԴIP:%u.%u.%u.%u:%d Ŀ��IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_UDPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* icmp */
#define RTL8651_LOG_INFO_PKT_ICMPSTRING	"(ICMP ��ʽ:%d) %s ��ԴIP:%u.%u.%u.%u Ŀ��IP:%u.%u.%u.%u(%d)[%s]"
#define RTL8651_LOG_INFO_PKT_ICMPVARLIST	\
	info->pkt_icmpType, \
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_icmpId, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_PKT_OTHERSTRING	"(����) %s ��ԴIP:%u.%u.%u.%u Ŀ��IP:%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_PKT_OTHERVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]

/* <--- url ---> */
/* tcp */
#ifdef RTL865XB_URLFILTER_LOGMOREINFO
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s/%s ���� %s %s ](TCP) %s ��ԴIP:%u.%u.%u.%u:%d Ŀ��IP:%u.%u.%u.%u:%d [%s]"
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
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s/%s ���� %s %s ](UDP) %s ��ԴIP:%u.%u.%u.%u:%d Ŀ��IP:%u.%u.%u.%u:%d [%s]"
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
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s/%s ���� %s %s ](����) %s ��ԴIP:%u.%u.%u.%u Ŀ��IP:%u.%u.%u.%u [%s]"
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
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s ������](TCP) %s ��ԴIP:%u.%u.%u.%u:%d Ŀ��IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_TCPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s ������](UDP) %s ��ԴIP:%u.%u.%u.%u:%d Ŀ��IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_UDPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s ������](����) %s ��ԴIP:%u.%u.%u.%u Ŀ��IP:%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_URL_OTHERVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]
#endif	/* RTL865XB_URLFILTER_LOGMOREINFO */


#endif /* _RTL_NEW_LOGGING_MODEL */
#endif /* RTL8651_LOG_LOCALE_H */
