/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :  
* $Id: rtl8651_alg_dx7play.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_dx7play.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.4  2005/06/01 13:56:18  tony
* *: support alg reinit.
*
* Revision 1.3  2004/11/30 07:09:53  tony
* *: support ALG:DirectX for game AOE II
*
* Revision 1.2  2004/04/20 03:44:02  tony
* if disable define "RTL865X_OVER_KERNEL" and "RTL865X_OVER_LINUX", __KERNEL__ and __linux__ will be undefined.
*
* Revision 1.1  2004/02/25 14:26:33  chhuang
* *** empty log message ***
*
* Revision 1.2  2004/02/25 14:24:52  chhuang
* *** empty log message ***
*
* Revision 1.7  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.6  2003/12/16 12:07:22  tony
* fix bug: make DirectX ALG support game AOE2.
*
* Revision 1.5  2003/11/28 11:07:19  tony
* make DirectX 7 ALG is able to work correctly.
* (only for Client in LAN case)
*
* Revision 1.4  2003/10/23 07:09:41  hiwu
* arrange include file sequence
*
* Revision 1.3  2003/10/17 08:45:51  hiwu
* merge new function type
*
* Revision 1.2  2003/10/02 10:39:52  hiwu
* fix header conflict problem
*
* Revision 1.1  2003/10/02 10:01:49  hiwu
* initial version
*
*/

#ifndef _RTL8651_ALG_DX7PLAY
#define _RTL8651_ALG_DX7PLAY 
#define _RTL8651_ALG_DX7PLAY_MAX_CLIENT 5
void rtl8651_l4AliasHandleDx7playReInit(void);
int32 rtl8651_l4AliasHandleDx7playClientOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleDx7playClientOutbound2(struct rtl_pktHdr *pkthdrPtr, struct ip *pip,	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *link);
int32 rtl8651_l4AliasHandleDx7playServerInbound2(struct rtl_pktHdr *pkthdrPtr, struct ip *pip,	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *link);
int32 rtl8651_l4AliasHandleDx7playServerInbound(struct rtl_pktHdr *pkthdrPtr, struct ip *pip,	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *link);

#endif /* _RTL8651_ALG_DX7PLAY */ 
