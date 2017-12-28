/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :  
* $Id: rtl8651_alg_mms.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_mms.h,v $
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
* Revision 1.5  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.4  2003/10/23 07:10:04  hiwu
* arrange include file sequence
*
* Revision 1.3  2003/10/02 10:40:05  hiwu
* fix header conflict problem
*
* Revision 1.2  2003/10/02 07:05:32  hiwu
* merge with new table driver
*
* Revision 1.1  2003/09/23 04:36:00  hiwu
* initial version
*
*
*/

#ifndef _RTL8651_ALG_MMS
#define _RTL8651_ALG_MMS



int32 rtl8651_l4AliasMmsInit(void *);
int32 rtl8651_l4AliasHandleMmsCleintOutbound(struct rtl_pktHdr *, struct ip *,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *);

#endif /* _RTL8651_ALG_MMS */
