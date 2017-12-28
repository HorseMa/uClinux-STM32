/*
* Copyright c                  Realtek Semiconductor Corporation, 2004
* All rights reserved.
* 
* Program : Header File of Mass Production Test Program (rtl865x_mpt.c)
* Abstract : 
* Author : Edward Jin-Ru Chen (jzchen@realtek.com.tw)
*/

#ifndef _RTL865X_MPT_H
#define _RTL865X_MPT_H

void rtl865x_MPTest_setRTKProcotolMirrorFlag(int8 flag);
int32  rtl865x_MPTest_Process(struct rtl_pktHdr* pPkt);

#endif /* _RTL865X_MPT_H */
