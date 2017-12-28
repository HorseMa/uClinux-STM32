/*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program : TFTP ALG
* Abstract : 
* Creator : Yi-Lun Chen (chenyl@realtek.com.tw)
* Author : Yi-Lun Chen (chenyl@realtek.com.tw)
*/

#ifndef _RTL8651_ALG_TFTP
#define _RTL8651_ALG_TFTP

#define TFTP_UDP_DPORT				69		/* RFC 1350, chap 4  */
#define TFTP_FLOW_CACHE_TIMEOUT	3		/* The flow cache's age timer for outbound TFTP connection */

/* To Handle Outbound TFTP client */
int32 rtl8651_l4AliasHandleTftpClientOutbound (
	struct rtl_pktHdr *pkthdrPtr,
	struct ip *pip,
	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *tb);


#endif /* _RTL8651_ALG_TFTP */
