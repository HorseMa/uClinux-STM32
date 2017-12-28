/*
* Copyright c                  Realtek Semiconductor Corporation, 2004  
* All rights reserved.
* 
* Program : Control MII & MDC/MDIO connected 8305Sx
* Abstract : 
* Author : Edward Jin-Ru Chen (jzchen@realtek.com.tw)               
* $Id: rtl8305s.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
*/

#ifndef _RTL8305S_H
#define _RTL8305S_H

#include "rtl_types.h"

#define RTL8305S_PORT_NUMBER	5
#define RTL8305S_ETHER_AUTO_100FULL	0x01
#define RTL8305S_ETHER_AUTO_100HALF	0x02
#define RTL8305S_ETHER_AUTO_10FULL	0x03
#define RTL8305S_ETHER_AUTO_10HALF	0x04
#define RTL8305S_IDLE_TIMEOUT		100000000

void rtl8305s_smiRead(uint8 phyad, uint8 regad, uint16 * data);
void rtl8305s_smiWrite(uint8 phyad, uint8 regad, uint16 data);
void rtl8305s_smiReadBit(uint8 phyad, uint8 regad, uint8 bit, uint8 * value);
void rtl8305s_smiWriteBit(uint8 phyad, uint8 regad, uint8 bit, uint8 value);

int32 rtl8305s_setAsicEthernetPHY(uint32 port, int8 autoNegotiation, uint32 advCapability, uint32 speed, int8 fullDuplex);
int32 rtl8305s_getAsicEthernetPHY(uint32 port, int8 *autoNegotiation, uint32 *advCapability, uint32 *speed, int8 *fullDuplex);
int32 rtl8305s_getAsicPHYLinkStatus(uint32 port, int8 *linkUp);
int32 rtl8305s_getAsicPHYAutoNegotiationDone(uint32 port, int8 *done);
int32 rtl8305s_setAsicPHYLoopback(uint32 port, int8 enabled);
int32 rtl8305s_getAsicPHYLoopback(uint32 port, int8 *enabled);


int32 rtl8305s_init(void);

#endif
