/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :  
* $Id: rtl8651_alg_ipsec.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_ipsec.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
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
* Revision 1.2  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.1  2003/10/27 08:07:59  hiwu
* initial version
*
*/

#ifndef _RTL8651_ALG_IPSEC
#define _RTL8651_ALG_IPSEC

int32 rtl8651_l4AliasIpsecInit(void *);
int32 rtl8651_l4AliasHandleIpsecClientInbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleIpsecClientOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);		
void  rtl8651_l4AliasIpsecHouseKeeping(void);
void rtl8651_l4AliasIpsecReInit(void);

/* ESP NAPT plugin function */
extern int32 (*rtl8651_l4EspNaptAliasIn)(struct rtl_pktHdr *, struct ip *);
extern int32 (*rtl8651_l4EspNaptAliasOut)(struct rtl_pktHdr *, struct ip *);

#endif /* _RTL8651_ALG_IPSEC */
