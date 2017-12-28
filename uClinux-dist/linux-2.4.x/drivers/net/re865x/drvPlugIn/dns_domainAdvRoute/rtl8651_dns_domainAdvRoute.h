/*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program :	rtl8651_DomainAdvRoute.h
* Abstract :	External Header file for DNS based domain route
* Creator :	Jiucai Wang (jiucai_wang@realsil.com.cn)
* Author :		Jiucai Wang (jiucai_wang@realsil.com.cn)
*
*/

#ifndef _RTL8651_DNS_DOMAINADVROUTE_H
#define _RTL8651_DNS_DOMAINADVROUTE_H

/* =============================================================
	variables and internal data structure
    ============================================================= */

#define RTL8651_DOMAINADVROUTE_DEFAULT_ROUTEENTRYMAXNUM			6		/*User set the max number of the route list entry here*/
#define RTL8651_DOMAINADVROUTE_DEFAULT_DSTIPENTRYMAXNUM			24		/*User set the max number of distinate Ip entry for each route list entry here*/
#define RTL8651_DOMAINADVROUTE_DEFAULT_DSTIPENTRYTIMEOUT			3600		/*User set the timeout value of dstIp entry*/
#define RTL8651_DNS_DOMAIN_MAXLENGTH	128	/* maximum length of Domain name */

typedef struct rtl8651_domainAdvRouteProperty_s{
	uint8  sessionId;	
	uint8 addPolicyRoute;							/*FALSE=don't need add , TRUE=add*/
	uint8 addDemandRoute;							/*FALSE=don't need add , TRUE=add*/
	uint8 connState;                             					/*FALSE=disconnected or TRUE=connected*/
	ipaddr_t sessionIp;
	int (*pDemandCallback)(unsigned int);
} rtl8651_domainAdvRouteProperty_t;

/* =======================================================================================
		External functions & date for USERs
    ======================================================================================== */

/*DNS domain routing*/
int32 rtl8651_domainAdvRoute_addDomainEntry(char *, rtl8651_domainAdvRouteProperty_t *);
int32 rtl8651_domainAdvRoute_delDomainEntry(char *, rtl8651_domainAdvRouteProperty_t *);
int32 rtl8651_domainAdvRoute_delPolicyOrDemandRouteBySessionId(uint8);
int32 rtl8651_domainAdvRoute_changeDomainProperty(char *, rtl8651_domainAdvRouteProperty_t *);
int32 rtl8651_domainAdvRoute_changeSessionProperty(rtl8651_domainAdvRouteProperty_t *);
int32 rtl8651_domainAdvRoute_flushSessionEntry(uint8);
void rtl8651_domainAdvRoute_rinitRouteList(void);

#endif
