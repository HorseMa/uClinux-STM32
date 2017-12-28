/*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program : Header file for  fowrading engine rtl8651_layer2fwd.c
* Abstract : 
* Author :  Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/
#ifndef RTL8651_L2FWD_H
#define RTL8651_L2FWD_H

#include "rtl_types.h"
#include "rtl_glue.h"

/*********************************************************************************************************
	ARP Process
**********************************************************************************************************/
void rtl8651_installArpAgent(int32  (*arpAgent)(struct rtl_pktHdr *pkthdrPtr, struct rtl_mBuf *mbufPtr, uint16 vid));

/*********************************************************************************************************
	Packet pre-/post- process
**********************************************************************************************************/
int32 rtl8651_rxPktPreprocess(struct rtl_pktHdr * pPkthdr);
int32 rtl8651_txPktPostProcessing(struct rtl_pktHdr * pPkt);

/*********************************************************************************************************
	Extension device process
**********************************************************************************************************/

/* Registration, Query */
int32 rtl8651_fwdEngineRegExtDevice(	uint32 portNumber,
											uint16 defaultVID,
											uint32 *linkID_p,
											void *extDevice);
int32 rtl8651_fwdEngineUnregExtDevice(uint32 linkID);
int32 rtl8651_fwdEngineGetLinkIDByExtDevice(void *extDevice);

int32 rtl8651_fwdEngineSetExtDeviceVlanProperty(uint32 linkID, uint16 vid, uint32 property);
#define RTL8651_EXTDEV_VLANPROPER_ISMBR			0x01
#define RTL8651_EXTDEV_VLANPROPER_NOTMBR		0x02

int32 rtl8651_fwdEngineDumpExtDev(void);

int32 rtl8651_fwdEngineEnableWDS (int32 flag);

int32 rtl8651_fwdEngineRemoveHostsOnExtLinkID (uint32 linkID);

int32 rtl8651_fwdEngineAddWlanSTA(uint8 *smac, uint16 myvid, uint32 myportmask, uint32 linkId);
int32 rtl8651_fwdEngineDelWlanSTA(uint8 *mac, uint16 myvid);
int32 rtl8651_fwdEngineExtPortUcastFastRecv(	struct rtl_pktHdr *pkt,
													uint16 myvid,
													uint32 myportmask);

int32 rtl8651_fwdEngineExtPortRecv(	void *id,
										uint8 *data,
										uint32 len,
										uint16 myvid,
										uint32 myportmask,
										uint32 linkID);

/*********************************************************************************************************
	PPTP / L2TP process
**********************************************************************************************************/
#define RTL8651_MAX_PPTPL2TP_BUFFERSIZE	60

void rtl8651_pptpPppCompress(int enable);

#if 0
#if defined(CONFIG_BRIDGE) ||defined(CONFIG_BRIDGE_MODULE)
//void* rtl8651_fwdEngineGetExtList(int* num);
void* rtl8651_fwdEngineGetExtDevFromLinkid(int link_id);
//int     rtl8651_fwdEngineGetLinkIDFromExtDev(void* extDev);
int32  rtl8651_fwdEngineSetExtDevFwdState(void * extDev, int state);
#endif

void rtl8651_fwdEngineSetBPDUPathFlag(int32 flag);
int32 rtl8651_fwdEngineGetBPDUPathFlag(void);  

#endif
#endif /* RTL8651_L2FWD_H */
