/*
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :
* $Id: rtl8651_alg_irc_dcc.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_irc_dcc.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
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
* Revision 1.3  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.2  2003/11/28 02:53:17  tony
* remove unused irc_init function
*
* Revision 1.1  2003/11/27 03:26:20  tony
* Support ALG for mIRC DCC (Direct Client Connect: SEND and CHAT)
*
*
*/

#ifndef _RTL8651_ALG_IRC_DCC
#define _RTL8651_ALG_IRC_DCC

#define IRC_DCC_SEND 1
#define IRC_DCC_CHAT 2

int32 rtl8651_l4AliasHandleIrcDccClientOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);


#endif /* _RTL8651_ALG_IRC_DCC */
