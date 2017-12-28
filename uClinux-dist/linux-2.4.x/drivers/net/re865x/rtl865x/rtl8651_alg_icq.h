/*
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :
* $Id: rtl8651_alg_icq.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_icq.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.5  2005/12/23 11:28:32  tony
* *: add new feature: support ICQ v5 audio call from WAN.
*
* Revision 1.4  2004/07/23 13:19:57  tony
* *: remove all warning messages.
*
* Revision 1.3  2004/04/20 03:44:02  tony
* if disable define "RTL865X_OVER_KERNEL" and "RTL865X_OVER_LINUX", __KERNEL__ and __linux__ will be undefined.
*
* Revision 1.2  2004/03/18 09:22:26  tony
* add alg reinit function, it will be called when fwdEng ReInit.
*
* Revision 1.1  2004/02/25 14:26:33  chhuang
* *** empty log message ***
*
* Revision 1.2  2004/02/25 14:24:52  chhuang
* *** empty log message ***
*
* Revision 1.5  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.4  2003/12/22 07:21:53  tony
* fix bug: unalignment issue appears in ICQ ALG.
*
* Revision 1.3  2003/12/16 12:07:22  tony
* fix bug: make DirectX ALG support game AOE2.
*
* Revision 1.2  2003/11/27 12:14:30  tony
* update ICQ ALG: support for ICQ 2003b
*
* Revision 1.1  2003/11/13 12:28:53  tony
* add ALG ICQ (for version 2000b~2003b)
*
*
*/

#ifndef _RTL8651_ALG_ICQ
#define _RTL8651_ALG_ICQ
#define _RTL8651_ALG_ICQ_MAX_CLIENT 5



int32 rtl8651_l4AliasHandleIcqUdpClientOutbound(struct rtl_pktHdr *pkthdrPtr, struct ip *pip,	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *tb);

int32 rtl8651_l4AliasHandleIcqClientOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);		
int32 rtl8651_l4AliasHandleIcqServerOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);		
void  rtl8651_l4AliasIcqReInit(void);

struct icq_command
{
	uint8 id; // always 2a
	uint8 channel; //always 02
	uint16 seq;
	
	uint16 length;
	uint16 snac1; // always 01
	
	uint16 snac2; // always 1e
	uint16 flag;
	
	uint32 reference;
	uint32 tlv6; // always 00 06 00 04
	uint32 status;
	uint32 tlv8; // always 00 08 00 02
	uint16 error;
	uint16 tlv12; // always 00 0c
	
	uint16 cli2cli;
	uint16 ip[2]; //for 4 bytes alignment
	uint16 rsv; //always 00 00 
	
	uint16 port;
	uint8 tcpflag; // 01: firewall , 02: SOCKS4/5 , 04: 'normal'
	uint8 xxa;
};

struct icq_command2
{
	uint8 id; // always 2a
	uint8 channel; //always 02
	uint16 seq;
	
	uint16 length;
	uint16 snac1; // always 01
	
	uint16 snac2; // always 1e
	uint16 xxa;
};

#endif /* _RTL8651_ALG_ICQ */
