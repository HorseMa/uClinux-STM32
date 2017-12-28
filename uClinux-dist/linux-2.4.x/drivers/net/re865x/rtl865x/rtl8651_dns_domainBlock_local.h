/*
*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program :	rtl8651_domainBlock_local.h
* Abstract :	Internal Header file for DNS based domain blocking
* Creator :	Yi-Lun Chen (chenyl@realtek.com.tw)
* Author :	Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/

#ifndef _RTL8651_DNS_DOMAINBLOCK_LOCAL_H
#define _RTL8651_DNS_DOMAINBLOCK_LOCAL_H

#include "rtl8651_tblDrvFwd_utility.h"
#include "rtl8651_tblDrvFwd.h"
#include "rtl8651_dns_domainBlock.h"

/* =============================================================
	variables and internal data structure
    ============================================================= */
#define RTL8651_DOMAINBLOCKING_DEFAULT_TABLESIZE			24
#define RTL8651_DOMAINBLOCKING_DEFAULT_BLKENTRYCNT			12
#define RTL8651_DOMAINBLOCKING_DEFAULT_BLKENTRYTIMEOUT	3600

/* block related information set by user */
typedef struct _rtl8651_domainBlockingInfo_s {
	rtl8651_domainBlockingInfo_t info;

	void								*ctrlEntry;
	struct _rtl8651_domainBlockingInfo_s	*prev;
	struct _rtl8651_domainBlockingInfo_s	*next;
} _rtl8651_domainBlockingInfo_t;

/* BLOCK list (actually set in ACL rule) */
typedef struct _rtl8651_domainBlockingBlockList_s {
	_rtl8651_domainBlockingInfo_t *info;
	uint32 updateTime;
	ipaddr_t dip;

	struct _rtl8651_domainBlockingBlockList_s	*prev;
	struct _rtl8651_domainBlockingBlockList_s	*next;
} _rtl8651_domainBlockingBlockList_t;


/* =======================================================================================
		External functions for ROMEDRV
    ======================================================================================== */

/* for FWDENG */
int32 _rtl8651_registerDomainBlock(void);

/* callback functions for DNS MODULE */
int32 _rtl8651_domainBlocking_init(void *para);
void _rtl8651_domainBlocking_flushBlockList(void);
int32 _rtl8651_domainBlocking_procMapping(char *orig_domain, char *mapped_domain, ipaddr_t mapped_ip, void *info);
void _rtl8651_domainBlocking_timeUpdate(uint32 secPassed);
int32 _rtl8651_domainBlocking_blockInfoDestructor(void *info);

/* for TBLDRV to add acl rules */
uint32 _rtl8651_tryToAddAclRuleForDnsDomainBlocking(rtl8651_tblDrv_vlanTable_t *vlanPtr, uint32 aclStart, uint32 aclEnd, int8 *enoughFlag);

/* for other functions in ROMEDRV to add/delete domain blocking entry */
int32 _rtl8651_domainBlocking_addDomainBlockingEntry(char *domain_name, rtl8651_domainBlockingInfo_t *usrInfo);
int32 _rtl8651_domainBlocking_delDomainBlockingEntry(char *domain_name, rtl8651_domainBlockingInfo_t *usrInfo);
int32 _rtl8651_domainBlocking_flushDomainBlockingEntry(void);

uint32 _rtl8651_tryToAddAclRuleForDnsDomainBlocking(rtl8651_tblDrv_vlanTable_t *vlanPtr, uint32 aclStart, uint32 aclEnd, int8 *enoughFlag);

#endif /* _RTL8651_DNS_DOMAINBLOCK_LOCAL_H */
