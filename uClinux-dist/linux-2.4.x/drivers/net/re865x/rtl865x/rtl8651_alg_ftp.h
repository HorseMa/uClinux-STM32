/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 
* Abstract : 
* Creator : 
* Author :  
* $Id: rtl8651_alg_ftp.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_ftp.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.7  2006/01/07 03:10:42  hao_yu
* *: modify function declarations
*
* Revision 1.6  2005/12/21 09:30:24  hao_yu
* *: add ALG QoS suppport
*
* Revision 1.5  2004/09/07 14:52:13  chenyl
* *: bug fix: napt-redirect fragment packet checksum-recalculate
* *: bug fix: conflict between protocol stack flow cache and protocol stack action
* *: bug fix: protocol stack action mis-process to UDP-zero-checksum packet
* *: separate the header file:
*         - internal : rtl8651_tblDrvFwdLocal.h
*         - external : rtl8651_tblDrvFwd.h
*
* Revision 1.4  2004/06/15 05:56:15  chhuang
* +: try to add Policy-Based QoS Path, but not complete. Because it is low priority
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
* Revision 1.6  2004/01/09 08:03:21  tony
* make the code architecture of ALG is able to support multiple dial session.
*
* Revision 1.5  2003/10/23 07:09:41  hiwu
* arrange include file sequence
*
* Revision 1.4  2003/10/02 10:39:52  hiwu
* fix header conflict problem
*
* Revision 1.3  2003/10/02 07:05:01  hiwu
* merge with new table driver
*
* Revision 1.2  2003/09/23 04:34:52  hiwu
* add #define _RTL8651_ALG_FTP
*
* Revision 1.1  2003/09/17 06:45:57  hiwu
* initial version
*
*
*
*/

#ifndef _RTL8651_ALG_FTP
#define _RTL8651_ALG_FTP
#include "rtl_types.h"
#include "assert.h"
#include "types.h"
#include "rtl_queue.h"
#include "rtl_errno.h"
#include "mbuf.h"
#include "rtl8651_tblDrvFwdLocal.h"
#include "rtl8651_tblDrvProto.h"
#include "rtl8651_tblDrvLocal.h"
#include "rtl8651_alg_init.h"
#include "rtl8651_alg_qos.h"

int32 rtl8651_l4AliasFtpInit(void *arg);
void rtl8651_l4AliasFtpReInit(void);
int32 rtl8651_l4AliasHandleFtpClientOutbound(struct rtl_pktHdr *indat, struct ip *inhdr,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *link);
int32 rtl8651_l4AliasHandleFtpClientInbound(struct rtl_pktHdr *indat, struct ip *inhdr,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *link);
int32 rtl8651_l4AliasHandleFtpServerOutbound(struct rtl_pktHdr *indat, struct ip *inhdr,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *link);
int32 rtl8651_l4AliasHandleFtpServerInbound(struct rtl_pktHdr *indat, struct ip *inhdr,rtl8651_tblDrv_naptTcpUdpFlowEntry_t *link);

#endif /* _RTL8651_ALG_FTP */
