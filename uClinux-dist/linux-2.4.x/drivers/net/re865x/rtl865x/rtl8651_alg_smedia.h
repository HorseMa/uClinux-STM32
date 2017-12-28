/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :  
* $Id: rtl8651_alg_smedia.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_smedia.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
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
* Revision 1.4  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.3  2003/10/24 04:21:40  hiwu
* add RTSP alg support
*
* Revision 1.2  2003/10/23 07:10:04  hiwu
* arrange include file sequence
*
* Revision 1.1  2003/10/17 06:16:13  hiwu
* initial version
*
*/

#ifndef _RTL8651_ALG_SMEDIA
#define _RTL8651_ALG_SMEDIA

#define RTSP_CONTROL_PORT_NUMBER_1 554
#define RTSP_CONTROL_PORT_NUMBER_2 7070

int32 rtl8651_l4AliasSmediaInit(void *arg);
int32 rtl8651_l4AliasHandleSmediaCleintOutbound(struct rtl_pktHdr *, struct ip *,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *);

#endif /* _RTL8651_ALG_SMEDIA */ 
