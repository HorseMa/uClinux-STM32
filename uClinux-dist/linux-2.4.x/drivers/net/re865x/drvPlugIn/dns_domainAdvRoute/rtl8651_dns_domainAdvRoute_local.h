/*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program :	rtl8651_DomainAdvRoute_local.h
* Abstract :	External Header file for DNS based domain route
* Creator :	Jiucai Wang (jiucai_wang@realsil.com.cn)
* Author :		Jiucai Wang (jiucai_wang@realsil.com.cn)
*
*/

#ifndef _RTL8651_DNS_DOMAINADVROUTE_LOCAL_H
#define _RTL8651_DNS_DOMAINADVROUTE_LOCAL_H

#include "../../rtl865x/rtl8651_tblDrvFwd_utility.h"
#include "../../rtl865x/rtl8651_tblDrvFwd.h"
#include "../../crypto/rtl_cryptGlue.h"
#include "rtl8651_dns_domainAdvRoute.h"

/* ================================================================================
		Debug message
    ================================================================================ */
#define DNS_DOMAINROUTE_DEBUG

#ifdef DNS_DOMAINROUTE_DEBUG
#define DNS_DOMAINROUTE_MSG_MASK			0xFFFFFFF8	/*Tune Off debug messages*/
#define DNS_DOMAINROUTE_MSG_INFO			(1<<0)
#define DNS_DOMAINROUTE_MSG_WARN			(1<<1)
#define DNS_DOMAINROUTE_MSG_ERR			(1<<2)
#endif

#if (DNS_DOMAINROUTE_MSG_MASK & DNS_DOMAINROUTE_MSG_INFO)
#define DNS_DOMAINROUTE_INFO(fmt, args...) \
		do {rtlglue_printf("[%s-%d]-info-: " fmt "\n", __FUNCTION__, __LINE__, ## args);} while (0);
#else
#define DNS_DOMAINROUTE_INFO(fmt, args...)
#endif

#if (DNS_DOMAINROUTE_MSG_MASK & DNS_DOMAINROUTE_MSG_WARN)
#define DNS_DOMAINROUTE_WARN(fmt, args...) \
		do {rtlglue_printf("[%s-%d]-warning-: " fmt "\n", __FUNCTION__, __LINE__, ## args);} while (0);
#else
#define DNS_DOMAINROUTE_WARN(fmt, args...)
#endif

#if (DNS_DOMAINROUTE_MSG_MASK & DNS_DOMAINROUTE_MSG_ERR)
#define DNS_DOMAINROUTE_ERR(fmt, args...) \
		do {rtlglue_printf("[%s-%d]-error-: " fmt "\n", __FUNCTION__, __LINE__, ## args);} while (0);
#else
#define DNS_DOMAINROUTE_ERR(fmt, args...)
#endif

/* =============================================================
	variables and internal data structure
    ============================================================= */

#define MEM_FREE(p) \
	do { rtlglue_free((p));} while (0);

/* block related information set by user */
typedef struct _rtl8651_domainAdvRouteDstIpList_s{
	uint8   addPolicyRouteSuccess;					/*TRUE=yes,FALSE=no*/
	uint8   addDemandRouteSuccess;
	uint8   reserv[2];
	ipaddr_t dstIp;
	uint32 updateTime;
	
	struct _rtl8651_domainAdvRouteDstIpList_s *prev;
	struct _rtl8651_domainAdvRouteDstIpList_s *next;
}_rtl8651_domainAdvRouteDstIpList_t;

typedef struct _rtl8651_domainAdvRouteRouteList_s {
	uint8 dstIpEntryNum;						 		/*record the entry number for the special orig_domain*/
        uint8 reserv[3];
	char orig_domain[RTL8651_DNS_DOMAIN_MAXLENGTH];       /*original domain*/

	_rtl8651_domainAdvRouteDstIpList_t *dstIpList;      		/*mapped ip information*/
	rtl8651_domainAdvRouteProperty_t property;
	
	struct _rtl8651_domainAdvRouteRouteList_s	*prev;
	struct _rtl8651_domainAdvRouteRouteList_s	*next;
} _rtl8651_domainAdvRouteRouteList_t;

/* =======================================================================================
		External functions for DNS module
    ======================================================================================== */

/* for FWDENG */
int32 _rtl8651_registerDomainAdvRoute(void);

/* callback functions for DNS MODULE */
int32 _rtl8651_domainAdvRoute_init(void *);
void _rtl8651_domainAdvRoute_reInit(void);
int32 _rtl8651_domainAdvRoute_procMapping(char *,	char *,ipaddr_t ,void *);
void _rtl8651_domainAdvRoute_timeUpdate(uint32);
int32 _rtl8651_domainAdvRoute_routeListDestructor(void *);
_rtl8651_domainAdvRouteRouteList_t * _rtl8651_domainAdvRoute_findDomainMatchInRouteList(_rtl8651_domainAdvRouteRouteList_t *,char *);
_rtl8651_domainAdvRouteDstIpList_t *_rtl8651_domainAdvRoute_findDstIpMatchInDstIpList(_rtl8651_domainAdvRouteDstIpList_t *,ipaddr_t);
_rtl8651_domainAdvRouteRouteList_t *_rtl8651_domainAdvRoute_returnRouteListHeader(void);
#endif



