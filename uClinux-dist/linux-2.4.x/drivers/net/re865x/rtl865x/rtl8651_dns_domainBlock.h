/*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program :	rtl8651_domainBlock.h
* Abstract :	External Header file for DNS based domain block in ROMEDRV
* Creator :	Yi-Lun Chen (chenyl@realtek.com.tw)
* Author :	Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/

#ifndef _RTL8651_DNS_DOMAINBLOCK_H
#define _RTL8651_DNS_DOMAINBLOCK_H

/* =======================================================================================
		External data structure
    ======================================================================================== */

typedef struct rtl8651_domainBlockingInfo_s {
	ipaddr_t sip;
	ipaddr_t sipMask;
} rtl8651_domainBlockingInfo_t;

/* =======================================================================================
		External functions for USERs
    ======================================================================================== */

void rtl8651_domainBlocking_dump(void);
int32 rtl8651_domainBlocking_addDomainBlockingEntry(char *domain_name, rtl8651_domainBlockingInfo_t *usrInfo);
int32 rtl8651_domainBlocking_delDomainBlockingEntry(char *domain_name, rtl8651_domainBlockingInfo_t *usrInfo);
int32 rtl8651_domainBlocking_flushDomainBlockingEntry(void);

#endif /* _RTL8651_DNS_DOMAINBLOCK_H */
