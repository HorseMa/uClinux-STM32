/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :  
* $Id: rtl8651_alg_h323.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_h323.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.6  2005/07/19 07:58:35  chenyl
* +: H.323 some RAS support (tested by OpenH323 Gatekeeper)
*
* Revision 1.5  2005/06/30 13:57:11  tony
* *: support for Vizufon VoIP Phone(H323).
*
* Revision 1.4  2005/06/01 13:56:18  tony
* *: support alg reinit.
*
* Revision 1.3  2004/10/04 05:08:54  tony
* *: fix bug for NetMeeting Video in/out.
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
* Revision 1.8  2004/01/15 07:18:19  tony
* support incoming call for H323 ALG.
*
* Revision 1.7  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.6  2003/10/23 07:09:41  hiwu
* arrange include file sequence
*
* Revision 1.5  2003/10/20 05:12:06  hiwu
* add new h245 alg support
*
* Revision 1.4  2003/10/17 08:40:29  hiwu
* merge new function type
*
* Revision 1.3  2003/10/02 10:39:52  hiwu
* fix header conflict problem
*
* Revision 1.2  2003/10/02 10:01:16  hiwu
* fix h323 tcp parser bug
*
* Revision 1.1  2003/10/02 07:08:11  hiwu
* initial version
*
*/

#ifndef _RTL8651_ALG_H323
#define _RTL8651_ALG_H323

void rtl8651_l4AliasH323ReInit(void);
int32 rtl8651_l4AliasH323Init(void *);

int32 rtl8651_l4AliasHandleClientH225RASIn(struct rtl_pktHdr *, struct ip *,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *);
int32 rtl8651_l4AliasHandleClientH225RASOut(struct rtl_pktHdr *, struct ip *,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *);		
int32 rtl8651_l4AliasHandleServerH225RASOut(struct rtl_pktHdr *, struct ip *,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *);
int32 rtl8651_l4AliasHandleServerH225RASIn(struct rtl_pktHdr *, struct ip *,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *);

int32 rtl8651_l4AliasHandleClientH225CSIn(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleClientH225CSOut(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);		
int32 rtl8651_l4AliasHandleServerH225CSOut(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleServerH225CSIn(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);


int32 rtl8651_l4AliasHandleClientH245Out(struct rtl_pktHdr *, struct ip *,	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleClientH245In(struct rtl_pktHdr *, struct ip *,	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleServerH245Out(struct rtl_pktHdr *, struct ip *,	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleServerH245In(struct rtl_pktHdr *, struct ip *,	struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);

/* ESP NAPT plugin function */
extern int32 (*rtl8651_l4RsvpNaptAliasIn)(struct rtl_pktHdr *, struct ip *);
extern int32 (*rtl8651_l4RsvpNaptAliasOut)(struct rtl_pktHdr *, struct ip *);


#endif /* _RTL8651_ALG_H323 */
