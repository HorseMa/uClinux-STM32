/*
*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program :	rtl8651_dns_local.h
* Abstract :	Internal Header file for DNS related process in ROMEDRV
* Creator :	Yi-Lun Chen (chenyl@realtek.com.tw)
* Author :	Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/

#ifndef _RTL8651_DNS_LOCAL_H
#define _RTL8651_DNS_LOCAL_H

#include "rtl8651_tblDrvFwd.h"
#include "rtl8651_dns.h"
#include "rtl8651_dns_proto.h"

/* =============================================================
	Default values
    ============================================================= */
#define RTL8651_DNSDB_DEFAULT_TABLESIZE				24
#define RTL8651_DNSDB_DEFAULT_PROCESSCNT			5

#define RTL8651_DNSDB_DEFAULT_CNAMEMAPTABLESIZE	16
#define RTL8651_DNSDB_DEFAULT_CNAMEMAPTIMEOUT		6400


/* ================================================================================
		Internal Data Structure
    ================================================================================ */
#define RTL8651_DNS_DOMAIN_MAXLENGTH	128	/* maximum length of Domain name */

/* DNS database */

typedef struct _rtl8651_dnsDB_s {
	char domain[RTL8651_DNS_DOMAIN_MAXLENGTH];		/* domain name */
	void *procData;									/* point to the Process-Specific data structure */

	/* <--- data structure ---> */
	struct _rtl8651_dnsDB_s **hdr;
	struct _rtl8651_dnsDB_s *prev;
	struct _rtl8651_dnsDB_s *next;
} _rtl8651_dnsDB_t;

typedef struct _rtl8651_dnsFuncCtrl_s {
	uint32 property;
	void (*reinit)(void);
	int32 (*proc)(char *orig_domain, char *domain, ipaddr_t ip, void *data);
	void (*timeUpdate)(uint32 secpassed);
	int32 (*destructor)(void *data);
} _rtl8651_dnsFuncCtrl_t;

typedef struct _rtl8651_dnsDB_ctrl_s {
	uint8					dnsDB_unusedProcCnt;
	_rtl8651_dnsDB_t			**rtl8651_inusedDB;
	_rtl8651_dnsDB_t			*freeDnsDB;
	_rtl8651_dnsFuncCtrl_t		*callBack;
	rtl8651_fwdEngineInitPara_t	initPara;			/* backup for init parameter */
} _rtl8651_dnsDB_ctrl_t;

typedef enum _rtl8651_dns_dnsDomain_e {
	_RTL8651_DNS_GET_TYPE_INET_IPV4 = 1,
	_RTL8651_DNS_GET_TYPE_DOMAIN
} _rtl8651_dns_dnsDomain_t;

/* CNAME mapping */

typedef struct _rtl8651_dns_cnameMapping_s {
	char addr[RTL8651_DNS_DOMAIN_MAXLENGTH];

	uint32 updateTime;

	struct _rtl8651_dns_cnameMapping_s *hdr_prev;
	struct _rtl8651_dns_cnameMapping_s *hdr_next;
	struct _rtl8651_dns_cnameMapping_s *lru_prev;
	struct _rtl8651_dns_cnameMapping_s *lru_next;
	struct _rtl8651_dns_cnameMapping_s *prev;
	struct _rtl8651_dns_cnameMapping_s *next;
} _rtl8651_dns_cnameMapping_t;


/* =============================================================
	Funtion declarations to other modules
    ============================================================= */
/* for FWDENG */
int32 _rtl8651_dns_init(rtl8651_fwdEngineInitPara_t *para);
void _rtl8651_dns_reinit(void);
int32 _rtl8651_dns_procDnsPkt(int32 ruleNo, struct rtl_pktHdr *pktHdr, struct ip *pip, void *usrDefined);

/* for TBLDRV */
void _rtl8651_dns_timeUpdate(uint32 secPassed);
uint32 _rtl8651_tryToAddAclRuleForDns(uint32 aclStart, uint32 aclEnd, int8 *enoughFlag);

/* for other MODULES who want to use DNS MODULES */
int32 _rtl8651_dns_registerProcess(	uint8 * process,
										uint32 property,
										int32 (*init)(void *para),
										void (*reinit)(void),
										int32 (*proc)(char *orig_domain, char *domain, ipaddr_t ip, void *data),
										void (*timeUpdate)(uint32 secpassed),
										int32 (*destructor)(void *data));

int32 _rtl8651_dns_addProcessEntry(uint8 process, char *domain, void *procData, void **entryPtr);
int32 _rtl8651_dns_delProcessEntry(uint8 process, uint8 byDbPtr, void *db_p, char *domain, void *procData);
int32 _rtl8651_dns_findProcessEntry(uint8 process, char *domain, void *procData, char **returnedDomain, void **returnedProcData, void **start);
char *_rtl8651_dns_getProcessEntryDomainInfo(void *db_p);

uint32 _rtl8651_tryToAddAclRuleForDns(uint32 aclStart, uint32 aclEnd, int8 *enoughFlag);


/* =======================================================================================
		Utility
    ======================================================================================== */

/*
	rem:	remain total length of data
	ptr:		current pointer
	shift:	shift length
	exit:		if current length is not enough to shift, then jump to 'exit'
*/
#define PTR_SHIFT_EXIT(rem, ptr, shift, exit) \
	do { \
		if ((rem) < (shift)) \
		{ \
			RTL_DNS_WARN("%d < %d, failed", rem, shift); \
			goto exit; \
		} \
		rem = (uint32)rem - (shift); \
		ptr = (void*)((uint32)(ptr) + (shift)); \
	} while (0);

#define PTR_CHK_SHIFT_EXIT(rem, ptr, shift, exit) \
	do { \
		if ((rem) < (shift)) \
		{ \
			RTL_DNS_WARN("%d < %d, failed", rem, shift); \
			goto exit; \
		} \
		ptr = (void*)((uint32)(ptr) + (shift)); \
	} while (0);
	
 #define DNS_UNLOCK_ERROR_CHECK(condition, success, passThrough, reason)\
	do { \
		int32 _retval; \
		if ((_retval = condition) != success)\
		{ \
			RTL_DNS_WARN("check result : %d failed", _retval); \
			return ((passThrough==TRUE)?_retval:reason); \
		} \
	} while (0);

#define DNS_UNLOCK_ERROR_CHECK_JMP(condition, success, label)\
	do { \
		if ((condition) != (success))\
		{ \
			goto label; \
		} \
	} while (0);

#define DNS_UNLOCK_COND_JMP(condition, label)\
	do { \
		if (condition)\
		{ \
			goto label; \
		} \
	} while (0);

#endif /* _RTL8651_DNS_LOCAL_H */
