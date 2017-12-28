/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :  
* $Id: rtl8651_alg_pptp.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_pptp.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.7  2006/03/20 09:38:06  hao_yu
* +: Add new APIs rtl8651_l4AliasPptpAsicDump() and rtl8651_l4AliasPptpConfig()
*
* Revision 1.6  2005/10/20 14:11:03  jzchen
* +: Add debuging message enable/disable API
*
* Revision 1.5  2005/06/01 13:56:18  tony
* *: support alg reinit.
*
* Revision 1.4  2004/07/21 12:45:51  tony
* +: add new features: PPTP Pass-through support server in.
*
* Revision 1.3  2004/06/24 03:24:21  jzchen
* +: Add dump function for command line debugging purpose
*
* Revision 1.2  2004/04/20 03:44:03  tony
* if disable define "RTL865X_OVER_KERNEL" and "RTL865X_OVER_LINUX", __KERNEL__ and __linux__ will be undefined.
*
* Revision 1.1  2004/02/25 14:26:33  chhuang
* *** empty log message ***
*
* Revision 1.2  2004/02/25 14:24:52  chhuang
* *** empty log message ***
*
* Revision 1.2  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.1  2003/10/23 09:01:43  hiwu
* initial version
*
*
*/

#ifndef _RTL8651_ALG_PPTP
#define _RTL8651_ALG_PPTP

int32 rtl8651_l4AliasPptpInit(void *);
void rtl8651_l4AliasPptpReInit(void);
int32 rtl8651_l4AliasHandlePptpClientInbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandlePptpClientOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandlePptpServerInbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandlePptpServerOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);

void  rtl8651_l4AliasPptpHouseKeeping(void);
void rtl8651_l4AliasPptpMsg( int8 , int8 , int8 , int8 , int8 , int8 );
void rtl8651_l4AliasPptpDump(void);
void rtl8651_l4AliasPptpAsicDump(void);
void rtl8651_l4AliasPptpConfig( uint16, uint16, uint16, uint16 );


/* GRE NAPT plugin function */
extern int32 (*rtl8651_l4GreNaptAliasIn)(struct rtl_pktHdr *, struct ip *);
extern int32 (*rtl8651_l4GreNaptAliasOut)(struct rtl_pktHdr *, struct ip *);

#endif /* _RTL8651_ALG_PPTP */
