/*
* Copyright c                  Realtek Semiconductor Corporation, 2004 
* All rights reserved.
* 
* Program : CuSeeMe
* Abstract : 
* Creator : 
* Author :
* $Id: rtl8651_alg_cuseeme.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
* $Log: rtl8651_alg_cuseeme.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.4  2004/04/20 03:44:02  tony
* if disable define "RTL865X_OVER_KERNEL" and "RTL865X_OVER_LINUX", __KERNEL__ and __linux__ will be undefined.
*
* Revision 1.3  2004/03/16 07:53:50  tony
* support endian free for test module.
*
* Revision 1.2  2004/03/16 03:52:18  tony
* fix bug: CUseeMe ALG support endian free for test module
*
* Revision 1.1  2004/03/12 07:34:44  tony
* add new ALG for CUseeMe
*
*
*
*/

#ifndef _RTL8651_ALG_CUSEEME
#define _RTL8651_ALG_CUSEEME



struct cu_header{
	uint16 	dest_family;
	uint16 	dest_port;
	uint32  	dest_addr;
	int16 	family;
	uint16 	port;
	uint32	addr;
	uint32	seq;
	uint16 	msg;
	uint16	data_type;
	uint16	packet_len;
} ;

/* Open Continue Header */
struct oc_header{
	uint16 	dest_family;
	uint16 	dest_port;
	uint32  	dest_addr;
	int16 	family;
	uint16 	port;
	uint32	addr;
	uint32	seq;
	uint16 	msg;
	uint16	data_type;
	uint16	packet_len;
	uint16 	client_count; 
	uint32	seq_no;
	char		user_name[20];
	char		stuff[4];
};

/* client info structures */
struct client_info{
	uint32		address; 
	char	       	stuff[8];
};


int32 rtl8651_l4AliasHandleCUseeMeClientOutbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);
int32 rtl8651_l4AliasHandleCUseeMeClientInbound(struct rtl_pktHdr *, struct ip *,struct rtl8651_tblDrv_naptTcpUdpFlowEntry_s *);


#endif /* _RTL8651_ALG_CUSEEME */
