
/*
* Copyright c                  Realtek Semiconductor Corporation, 2004
* All rights reserved.
* 
* Program : RealAudio UDP data channel
* Abstract : 
* Creator : 
* Author :
* $Id: rtl8651_alg_rtsp.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_rtsp.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.2  2006/07/03 03:05:04  chenyl
* *: convert files from DOS format to UNIX format
*
* Revision 1.1  2004/09/30 14:13:27  chhuang
* +:add rtsp
*
* Revision 1.1  2004/09/14 13:24:29  chhuang
* +: RTSP alg support
*
*
*/

#ifndef _RTL8651_ALG_RTSP
#define _RTL8651_ALG_RTSP



int32 rtl8651_l4AliasHandleRTSPClientOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleRTSPClientInbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
void rtl8651_l4AliasRtspHouseKeeping(void);
int32 rtl8651_l4AliasRTSPInit(void *arg);

#endif /* _RTL8651_ALG_RTSP */
