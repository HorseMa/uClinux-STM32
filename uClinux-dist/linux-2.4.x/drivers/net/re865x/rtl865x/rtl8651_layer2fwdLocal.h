/*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program : Internal Header file for fowrading engine rtl8651_layer2fwd.c
* Abstract : 
* Author :  Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/
#ifndef RTL8651_L2FWD_LOCAL_H
#define RTL8651_L2FWD_LOCAL_H

#include "rtl_types.h"
#include "rtl_glue.h"
#include "rtl8651_tblDrvLocal.h"
#include "rtl8651_layer2fwd.h"

/*********************************************************************************************************
	Main functions
**********************************************************************************************************/
int32 _rtl8651_layer2fwd_init(void);
void _rtl8651_layer2fwd_reinit(void);

/*********************************************************************************************************
	Extension device process
**********************************************************************************************************/
#define _RTL8651_EXTDEV_DEVCOUNT		16

/* we translate idx and linkID using these MACROs : to make sure LINKID > 0 (0 is reserved for broadcast) */
#define _RTL8651_EXTDEV_IDX2LINKID(idx)	((idx) + 1)
#define _RTL8651_EXTDEV_LINKID2IDX(linkId)	((linkId) - 1)


typedef struct _rtl8651_extDevice_s {
#if 0
#if defined(CONFIG_BRIDGE) ||defined(CONFIG_BRIDGE_MODULE)
       uint32 portStat;           /*portStat for STP*/ 
#endif
#endif
	uint32 linkID;			/* LinkID of this extension device */
	uint32 portMask;		/* port mask of this extension device  */
	void *extDevice;		/* user own pointer for specific extension device */
} _rtl8651_extDevice_t;

#define _RTL8651_EXTDEV_BCAST_LINKID	0		/* LinkID 0 is reserved for broadcast */

void _rtl8651_arrangeAllExtDeviceVlanMbr(void);

/*********************************************************************************************************
	VLAN process
**********************************************************************************************************/
#define VLAN_TAGGED(mdata)		( *(uint16*)(&((uint8*)mdata)[12]) == htons(0x8100))
#define VLAN_ID(mdata)			((ntohs(*((uint16 *)(mdata + 14))) & 0xe000) & 0x0fff)
#define VLAN_PRIORITY(mdata)	((ntohs(*((uint16 *)(mdata + 14))) & 0xe000) >> 13)


/*********************************************************************************************************
	Normal L2 forwarding process
**********************************************************************************************************/
int32 _rtl8651_fwdEngineSend(	uint32 property,
									void *data,
									int16 dvid,
									int32 iphdrOffset);

int32 _rtl8651_fwdEngineL2Input(	struct rtl_pktHdr *pkthdrPtr,
									rtl8651_tblDrv_vlanTable_t *local_vlanp,
									uint8 *m_data,
									uint16 ethtype);

#endif /* RTL8651_L2FWD_LOCAL_H */
