/*
* Copyright c                  Realtek Semiconductor Corporation, 2004  
* All rights reserved.
* 
* Program : Control MII & MDC/MDIO connected 8305SC
* Abstract : 
* Author : Edward Jin-Ru Chen (jzchen@realtek.com.tw)               
* $Id: rtl8305sc.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
*/

#ifndef _RTL8305SC_H
#define _RTL8305SC_H

#include "rtl_types.h"

#define RTL8305SC_RTCT_NORMAL	0x01
#define RTL8305SC_RTCT_SHORT	0x02
#define RTL8305SC_RTCT_OPEN	0x03

/* VLAN related API*/
int32 rtl8305sc_setAsicVlanEnable(int8 enabled);
int32 rtl8305sc_getAsicVlanEnable(int8 *enabled);
int32 rtl8305sc_setAsicVlanTagAware(int8 enabled);
int32 rtl8305sc_getAsicVlanTagAware(int8 *enabled);
int32 rtl8305sc_setAsicVlanIngressFilter(int8 enabled);
int32 rtl8305sc_getAsicVlanIngressFilter(int8 *enabled);
int32 rtl8305sc_setAsicVlanTaggedOnly(int8 enabled);
int32 rtl8305sc_getAsicVlanTaggedOnly(int8 *enabled);
int32 rtl8305sc_setAsicVlanPriorityThreshold(uint16 highPriorityThreshold);
int32 rtl8305sc_getAsicVlanPriorityThreshold(uint16 * highPriorityThreshold);
int32 rtl8305sc_setAsicVlan(uint16 vlanIndex, uint16 vid, uint16 memberPortMask);
int32 rtl8305sc_getAsicVlan(uint16 vlanIndex, uint16 *vid, uint16 *memberPortMask);
int32 rtl8305sc_setAsicPortVlanIndex(uint16 port, uint16 vlanIndex);
int32 rtl8305sc_getAsicPortVlanIndex(uint16 port, uint16 *vlanIndex);



/* MAC address table related API */
int32 rtl8305sc_getAsicMacAddressTableEntry(uint16 index, int8 * macAddress, uint16 *age, int8 * isStatic, uint16 * port, int8 * isValid);
int32 rtl8305sc_setAsicMacAddressTableEntry(int8 * macAddress, uint16 age, int8 isStatic, uint16 port);

/* Misc APIs*/
int32 rtl8305sc_setAsicLoopDetection(int8 enabled);
int32 rtl8305sc_getAsicLoopDetection(int8 *enabled);
int32 rtl8305sc_asicSoftReset(void);
int32 rtl8305sc_setAsicAgressiveBackoff(int8 enabled);
int32 rtl8305sc_getAsicAgressiveBackoff(int8 *enabled);
int32 rtl8305sc_setAsicLongReceivedPacketLength(int8 enabled1552);
int32 rtl8305sc_getAsicLongReceivedPacketLength(int8 *enabled1552);
int32 rtl8305sc_setAsicBroadcastInputDrop(int8 enabled);
int32 rtl8305sc_getAsicBroadcastInputDrop(int8 *enabled);
int32 rtl8305sc_setAsicForward1dReservedAddress(int8 enabled);
int32 rtl8305sc_getAsicForward1dReservedAddress(int8 *enabled);
int32 rtl8305sc_setAsicLeakyVlan(int8 enabled);
int32 rtl8305sc_getAsicLeakyVlan(int8 *enabled);
int32 rtl8305sc_setAsicArpVlan(int8 enabled);
int32 rtl8305sc_getAsicArpVlan(int8 *enabled);
int32 rtl8305sc_setAsicBroadcastStormControl(int8 enabled);
int32 rtl8305sc_getAsicBroadcastStormControl(int8 *enabled);

int32 rtl8305sc_asicRealtekCableTester(uint16 port, int8 isTxDir, uint32 * cableStatus, uint32 * length);

#endif

