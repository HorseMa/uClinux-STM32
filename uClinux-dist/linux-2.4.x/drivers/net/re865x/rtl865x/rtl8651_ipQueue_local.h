/*
* Copyright c                  Realtek Semiconductor Corporation, 2003
* All rights reserved.
* 
* Program : Switch core table driver rtl8651_ipQueue_local.h
* Abstract : 
* Author : Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/

#ifndef RTL8651_IPQUEUELOCAL_H
#define RTL8651_IPQUEUELOCAL_H

#include "rtl8651_tblDrvFwdLocal.h"
#include "rtl8651_ipQueue.h"

/******************************************************
		Queue System Global variable setting
******************************************************/
// Default Value : For static / non-static system both
#define RTL8651_DefaultDrvMaxFragTimeOut				3	/* Max fragment timeout for fragment packet */
#define RTL8651_DefaultDrvMaxNegativeListTimeOut		1	/* Max negative list entry timeout for fragment packet */
#define RTL8651_DefaultDrvMaxNegativeListCnt			20	/* Max negative entry count in queue system */
#define RTL8651_DefaultDrvMaxPositiveListCnt			60	/* Max positive entry count in queue system */
#define RTL8651_DefaultDrvMaxFragSubPktCnt			0	/* Max frame count of each fragment packet  (0: no limit) */
#define RTL8651_DefaultDrvMaxFragPktCnt				10	/* Max packet count */
#define RTL8651_DefaultDrvMaxFragPoolCnt			60	/* Max fragment Pool count (0: no limit) */


/******************************************************
	Function called by system initiate function
******************************************************/
int32 _rtl8651_IpFragQueueInit(rtl8651_fwdEngineInitPara_t *para);
/******************************************************
	Utilities for forwarding engine
******************************************************/
inline int32 _rtl8651_frag_NeedFragmentProcess(uint16 id, ipaddr_t sip, ipaddr_t dip, uint8 l4proto);
#endif /* RTL8651_IPQUEUELOCAL_H */
