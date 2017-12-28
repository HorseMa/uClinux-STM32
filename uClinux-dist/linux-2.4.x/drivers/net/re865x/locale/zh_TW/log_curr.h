#ifndef RTL8651_LOG_LOCALE_H
#define RTL8651_LOG_LOCALE_H

#include "../rtl865x/rtl8651_tblDrv.h"

#ifdef _RTL_NEW_LOGGING_MODEL

/******************************
	Descriptor
*******************************/
static char *log_desc[RTL8651_TOTAL_USERLOG_NO] =
{
	"[�V�~�s��TCP�쵲]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptOutbound */
	"[�V���s��TCP�쵲]",					/* RTL8651_LOG_NEWFLOW_NewTcpNaptInbound */
	"[�V�~�s��UDP�쵲]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptOutbound */
	"[�V���s��UDP�쵲]",					/* RTL8651_LOG_NEWFLOW_NewUdpNaptInbound */
	"[�V�~�s��ICMP�쵲]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptOutbound */
	"[�V���s��ICMP�쵲]",					/* RTL8651_LOG_NEWFLOW_NewIcmpNaptInbound */
	"[�ѥX�f��ACL���]",					/* RTL8651_LOG_ACL_EgressAclDropLog */
	"[�ѤJ�f��ACL���]",					/* RTL8651_LOG_ACL_IngressAclDropLog */
	"[URL�s������]",						/* RTL8651_LOG_URL_MatchUrlFilter */
	"[������ IP Spoofing����]",				/* RTL8651_LOG_DOS_IpSpoof */
	"[������ UDP Flood����]",				/* RTL8651_LOG_DOS_UdpFlood */
	"[������ӷ�IP�i�� UDP Flood����]",			/* RTL8651_LOG_DOS_HostUdpFlood */
	"[������ UDP Land����]",				/* RTL8651_LOG_DOS_UdpLand */
	"[������ UDP Bomb����]",				/* RTL8651_LOG_DOS_UdpBomb */
	"[������ UDP EchoChargen����]",				/* RTL8651_LOG_DOS_UdpEchoChargen */
	"[������ ICMP Land����]",				/* RTL8651_LOG_DOS_IcmpLand */
	"[������ Ping Of Death����]",				/* RTL8651_LOG_DOS_IcmpPingOfDeath */
	"[������ ICMP Flood����]",				/* RTL8651_LOG_DOS_IcmpFlood */
	"[������ӷ�IP�i�� ICMP Flood����]",			/* RTL8651_LOG_DOS_HostIcmpFlood */
	"[������ ICMP Smurf����]",				/* RTL8651_LOG_DOS_IcmpSmurf */
	"[������ SYN Flood����]",				/* RTL8651_LOG_DOS_SynFlood */
	"[������ӷ�IP�i�� SYN Flood����]",			/* RTL8651_LOG_DOS_HostSynFlood */
	"[������ Stealth FIN����]",				/* RTL8651_LOG_DOS_StealthFin */
	"[������ӷ�IP�i�� Stealth FIN����]",			/* RTL8651_LOG_DOS_HostStealthFin */
	"[������ TCP Land����]",				/* RTL8651_LOG_DOS_TcpLand */
	"[������ TCP ���y]",					/* RTL8651_LOG_DOS_TcpScan */
	"[������ SYN with Data����]",				/* RTL8651_LOG_DOS_TcpSynWithData */
	"[������ IP TearDrop����]",				/* RTL8651_LOG_DOS_TearDrop */
	"[������ SYN ���y]",					/* RTL8651_LOG_DOS_TcpUdpScan_SYN */
	"[������ FIN ���y]",					/* RTL8651_LOG_DOS_TcpUdpScan_FIN */
	"[������ ACK ���y]",					/* RTL8651_LOG_DOS_TcpUdpScan_ACK */
	"[������ UDP ���y]",					/* RTL8651_LOG_DOS_TcpUdpScan_UDP */
	"[������h�����y]",						/* RTL8651_LOG_DOS_TcpUdpScan_HYBRID */
};
/******************************
	Action
*******************************/
static char *log_action[RTL8651_LOGACTION_NO] = 
{
	"�L�ʧ@",
	"�w���",
	"�w���]�쵲",
	"�w�ǰe"
};
/*******************************
	Direction
********************************/
static char *direction_action[RTL8651_DIRECTION_NO] =
{
	"����~",
	"�~�줺"
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
#define RTL8651_LOG_TIME_STRING	"����%3d�~%02d��%02d��(%s %02d:%02d:%02d)"
#define RTL8651_LOG_TIME_VARLIST	(tm.tm_year-1911),tm.tm_mon+1,tm.tm_mday,(tm.tm_hour>=12?"PM":"AM"), (tm.tm_hour%12),tm.tm_min,tm.tm_sec
/******************************
 log detail format definition
******************************/
/* <--- pkt ---> */
/* tcp */
#define RTL8651_LOG_INFO_PKT_TCPSTRING	"(TCP) %s �ӷ�IP:%u.%u.%u.%u:%d �ت�IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_TCPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_PKT_UDPSTRING	"(UDP) %s �ӷ�IP:%u.%u.%u.%u:%d �ت�IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_PKT_UDPVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->pkt_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_dport, \
	log_action[info->action]
/* icmp */
#define RTL8651_LOG_INFO_PKT_ICMPSTRING	"(ICMP �Φ�:%d) %s �ӷ�IP:%u.%u.%u.%u �ت�IP:%u.%u.%u.%u(%d)[%s]"
#define RTL8651_LOG_INFO_PKT_ICMPVARLIST	\
	info->pkt_icmpType, \
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	info->pkt_icmpId, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_PKT_OTHERSTRING	"(��L) %s �ӷ�IP:%u.%u.%u.%u �ت�IP:%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_PKT_OTHERVARLIST	\
	direction_action[info->pkt_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]

/* <--- url ---> */
/* tcp */
#ifdef RTL865XB_URLFILTER_LOGMOREINFO
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s/%s �d�I %s %s ](TCP) %s �ӷ�IP:%u.%u.%u.%u:%d �ت�IP:%u.%u.%u.%u:%d [%s]"
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
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s/%s �d�I %s %s ](UDP) %s �ӷ�IP:%u.%u.%u.%u:%d �ت�IP:%u.%u.%u.%u:%d [%s]"
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
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s/%s �d�I %s %s ](��L) %s �ӷ�IP:%u.%u.%u.%u �ت�IP:%u.%u.%u.%u [%s]"
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
#define RTL8651_LOG_INFO_URL_TCPSTRING	"[%s �Q�d�I](TCP) %s �ӷ�IP:%u.%u.%u.%u:%d �ت�IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_TCPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* udp */
#define RTL8651_LOG_INFO_URL_UDPSTRING	"[%s �Q�d�I](UDP) %s �ӷ�IP:%u.%u.%u.%u:%d �ت�IP:%u.%u.%u.%u:%d [%s]"
#define RTL8651_LOG_INFO_URL_UDPVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	info->url_sport, \
	dip[0], dip[1], dip[2], dip[3], \
	info->url_dport, \
	log_action[info->action]
/* other */
#define RTL8651_LOG_INFO_URL_OTHERSTRING	"[%s �Q�d�I](��L) %s �ӷ�IP:%u.%u.%u.%u �ت�IP:%u.%u.%u.%u [%s]"
#define RTL8651_LOG_INFO_URL_OTHERVARLIST	\
	(info->url_string?info->url_string:""),\
	direction_action[info->url_direction], \
	sip[0], sip[1], sip[2], sip[3], \
	dip[0], dip[1], dip[2], dip[3], \
	log_action[info->action]
#endif	/* RTL865XB_URLFILTER_LOGMOREINFO */


#endif /* _RTL_NEW_LOGGING_MODEL */
#endif /* RTL8651_LOG_LOCALE_H */
