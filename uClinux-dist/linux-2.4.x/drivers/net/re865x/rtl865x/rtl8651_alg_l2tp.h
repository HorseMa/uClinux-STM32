/*
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : L2TP ALG
* Abstract : 
* Creator : 
* Author :
* $Id: rtl8651_alg_l2tp.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_l2tp.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.3  2004/04/20 03:44:02  tony
* if disable define "RTL865X_OVER_KERNEL" and "RTL865X_OVER_LINUX", __KERNEL__ and __linux__ will be undefined.
*
* Revision 1.2  2004/03/19 08:12:53  tony
* make IPSec, PPTP, L2TP are able to set session numbers dynamically by user.
*
* Revision 1.1  2004/02/25 14:26:33  chhuang
* *** empty log message ***
*
* Revision 1.2  2004/02/25 14:24:52  chhuang
* *** empty log message ***
*
* Revision 1.4  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.3  2003/12/31 04:59:26  tony
* fix bug: turn L2TP ALG on/off dynamically.
*
* Revision 1.2  2003/12/16 12:07:22  tony
* fix bug: make DirectX ALG support game AOE2.
*
* Revision 1.1  2003/12/08 03:37:26  tony
* add new ALG: L2TP v2 multiple-session
*
*
*
*/

#ifndef _RTL8651_ALG_L2TP
#define _RTL8651_ALG_L2TP



int32 rtl8651_l4AliasL2tpInit(void*);
int32 rtl8651_l4AliasHandleL2tpClientInbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleL2tpClientOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);

struct l2tp_header
{
	uint16 flag;
	uint16 length;
	uint16 tunnel_id;
	uint16 session_id;
};

struct l2tp_session
{
	uint32 redirect_ip;
	uint16 tunnel_id;
};


#endif /* _RTL8651_ALG_L2TP */
