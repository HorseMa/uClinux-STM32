
/*	@doc RTL8651_DOMAINADVROUTE_API

	@module rtl8651_dns_domainAdvRoute.c - RTL8651 Home gateway controller domainAdvRoute API documentation	|
	This document explains the external API interface for domainAdvRoute module.Functions with rtl8651_ prefix are external functions.
	@normal Gateway team (jiucai_wang@realsil.com.cn) <2006-05-15>
	Copyright <cp>2006 Realtek<tm> Semiconductor Cooperation, All Rights Reserved.

	 @head3 List of Symbols |
 	Here is a list of all functions and variables in this module.

 	@index | RTL8651_DOMAINADVROUTE_API
*/

#include "../../rtl865x/rtl_types.h"
#include "../../rtl865x/assert.h"
#include "../../rtl865x/types.h"
#include "../../rtl865x/rtl_errno.h"
#include "../../rtl865x/rtl_glue.h"
#include "../../rtl865x/mbuf.h"
#include "../../rtl865x/rtl8651_dns.h"
#include "../../rtl865x/rtl8651_layer2.h"
#include "../../rtl865x/rtl8651_tblDrv.h"
#include "../../rtl865x/rtl8651_tblDrvProto.h"
#include "rtl8651_dns_domainAdvRoute_local.h"

static _rtl8651_domainAdvRouteDstIpList_t * _rtl8651_domainAdvRoute_addDstIpToDstIpList(_rtl8651_domainAdvRouteRouteList_t *,ipaddr_t);
static _rtl8651_domainAdvRouteRouteList_t *_rtl8651_domainAdvRoute_addDomainToRouteList(char *);
static int32 _rtl8651_domainAdvRoute_addDomainEntry(char *, rtl8651_domainAdvRouteProperty_t *);
static int32 _rtl8651_domainAdvRoute_delDomainEntry(char *, rtl8651_domainAdvRouteProperty_t *);
static int32 _rtl8651_domainAdvRoute_changeDomainProperty(char *, rtl8651_domainAdvRouteProperty_t *);
static int32 _rtl8651_domainAdvRoute_destroyAllRouteList(void);
static int32 _rtl8651_domainAdvRoute_delRouteList(_rtl8651_domainAdvRouteRouteList_t *, uint8);
static int32 _rtl8651_domainAdvRoute_delDstIpList(_rtl8651_domainAdvRouteDstIpList_t *, uint8 );
static void _rtl8651_domainAdvRoute_getDstIpListHeaderAndTail(_rtl8651_domainAdvRouteDstIpList_t *);
static int32 _rtl8651_domainAdvRoute_addDomainPolicyOrDemandRoute(_rtl8651_domainAdvRouteRouteList_t *,_rtl8651_domainAdvRouteDstIpList_t *);
static int32 _addDomainBasedPolicyRouteToRomeDriver(_rtl8651_domainAdvRouteRouteList_t *,_rtl8651_domainAdvRouteDstIpList_t *);
static int32 _addDomainBasedDemandRouteToRomeDriver(_rtl8651_domainAdvRouteRouteList_t *,_rtl8651_domainAdvRouteDstIpList_t *);
static int32 _rtl8651_domainAdvRoute_delDomainPolicyOrDemandRoute(_rtl8651_domainAdvRouteRouteList_t *,_rtl8651_domainAdvRouteDstIpList_t *);
static int32 _delDomainBasedPolicyRouteToRomeDriver(_rtl8651_domainAdvRouteRouteList_t *,_rtl8651_domainAdvRouteDstIpList_t *);
static int32 _delDomainBasedDemandRouteToRomeDriver(_rtl8651_domainAdvRouteRouteList_t *,_rtl8651_domainAdvRouteDstIpList_t *);
static void _rtl8651_domainAdvRoute_arrangeAllPolicyOrDemandRoute(void);
static int32 _rtl8651_domainAdvRoute_changeSessionProperty(rtl8651_domainAdvRouteProperty_t *);
static int32 _rtl8651_domainAdvRoute_delPolicyOrDemandRouteBySessionId(uint8);
static int32 _rtl8651_domainAdvRoute_flushSessionEntry(uint8);
/* ==========================================================================
  *
  *		DNS Domain Route:
  *
  *			Route special DNS domain by ROMEDRV.
  *
  *			1. Get the IP information about Routed Domain by DNS request / reply.
  *			2. Map these domain to  the special IP address.
  *			3. Add policy route or demand route into Rome Drvie 
  *
  * ========================================================================== */

static uint32 dns_domainAdvRoute_routeEntryMaxNum;
static uint32 dns_domainAdvRoute_dstIpEntryMaxNum;
static uint32 dns_domainAdvRoute_dstIpEntryTimeout;

static uint8								 _rtl8651_domainAdvRoute_procSequence		= 0;
static uint32								 _rtl8651_domainAdvRoute_dstIpTimeout		= 0;
static uint32								 _rtl8651_domainAdvRoute_dstIpEntryMaxNum= 0;

static _rtl8651_domainAdvRouteDstIpList_t 		*_rtl8651_domainAdvRouteDstIpList_header	= NULL;
static _rtl8651_domainAdvRouteDstIpList_t 		*_rtl8651_domainAdvRouteDstIpList_tail		= NULL;
static _rtl8651_domainAdvRouteDstIpList_t		*_rtl8651_domainAdvRouteDstIpList_freeList	= NULL;
static _rtl8651_domainAdvRouteDstIpList_t		*_rtl8651_domainAdvRouteDstIpList_freeListBackUp= NULL;
static _rtl8651_domainAdvRouteRouteList_t		*_rtl8651_domainAdvRoute_routeList_header	= NULL;
static _rtl8651_domainAdvRouteRouteList_t		*_rtl8651_domainAdvRoute_routeList_tail 	= NULL;
static _rtl8651_domainAdvRouteRouteList_t		*_rtl8651_domainAdvRoute_routeList_freeList = NULL;
static _rtl8651_domainAdvRouteRouteList_t		*_rtl8651_domainAdvRoute_routeList_freeListBackUp = NULL;

/*
	Procedure to register Domain Route module into DNS module.
*/
int32 _rtl8651_registerDomainAdvRoute(void)
{
	return rtl8651_dns_registerProcess(	&_rtl8651_domainAdvRoute_procSequence,
										RTL8651_DNSDB_PROPERTY_KEYWORDMATCH,
										_rtl8651_domainAdvRoute_init,
										_rtl8651_domainAdvRoute_reInit,
										_rtl8651_domainAdvRoute_procMapping,
										_rtl8651_domainAdvRoute_timeUpdate,
										_rtl8651_domainAdvRoute_routeListDestructor);

}

/* =========================================================================
  *
  *										Call back functions
  *
  * ========================================================================= */

#define RTL8651_DOMAINADVROUTE_CALLBACKFUNCTION

/*
	INIT function: Allocate memory for route freelist and dstIp freelist here 
*/
int32 _rtl8651_domainAdvRoute_init(void *para)
{

	dns_domainAdvRoute_routeEntryMaxNum = RTL8651_DOMAINADVROUTE_DEFAULT_ROUTEENTRYMAXNUM;
	dns_domainAdvRoute_dstIpEntryMaxNum = RTL8651_DOMAINADVROUTE_DEFAULT_DSTIPENTRYMAXNUM;
	dns_domainAdvRoute_dstIpEntryTimeout = RTL8651_DOMAINADVROUTE_DEFAULT_DSTIPENTRYTIMEOUT;
	
	_rtl8651_domainAdvRoute_dstIpTimeout 	= 0;

	_rtl8651_domainAdvRouteDstIpList_header	= NULL;
	_rtl8651_domainAdvRouteDstIpList_tail		= NULL;
	_rtl8651_domainAdvRouteDstIpList_freeList 	= NULL;
	_rtl8651_domainAdvRoute_routeList_header	= NULL;
	_rtl8651_domainAdvRoute_routeList_tail 		= NULL;
	_rtl8651_domainAdvRoute_routeList_freeList	= NULL;

	if ((dns_domainAdvRoute_routeEntryMaxNum == 0) ||(dns_domainAdvRoute_dstIpEntryMaxNum == 0)||(dns_domainAdvRoute_dstIpEntryTimeout == 0))
	{
		DNS_DOMAINROUTE_INFO("Disable domain routing module:\n\trouteEntryMaxNum (%d) ,dstIpEntryMaxNum (%d),dstIpEntryTimeout (%d).\n",
								dns_domainAdvRoute_routeEntryMaxNum,
								dns_domainAdvRoute_dstIpEntryMaxNum,
								dns_domainAdvRoute_dstIpEntryTimeout);
		return SUCCESS;
	}

	MEM_ALLOC(_rtl8651_domainAdvRouteDstIpList_freeList, sizeof(_rtl8651_domainAdvRouteDstIpList_t)*(dns_domainAdvRoute_dstIpEntryMaxNum)*(dns_domainAdvRoute_routeEntryMaxNum), FAILED);
	_rtl8651_domainAdvRouteDstIpList_freeListBackUp = _rtl8651_domainAdvRouteDstIpList_freeList;
	DNS_LIST_INIT(_rtl8651_domainAdvRouteDstIpList_freeList,_rtl8651_domainAdvRouteDstIpList_t, ((dns_domainAdvRoute_dstIpEntryMaxNum)*(dns_domainAdvRoute_routeEntryMaxNum)), next);

	MEM_ALLOC(_rtl8651_domainAdvRoute_routeList_freeList, sizeof(_rtl8651_domainAdvRouteRouteList_t)*(dns_domainAdvRoute_routeEntryMaxNum), FAILED);
	_rtl8651_domainAdvRoute_routeList_freeListBackUp = _rtl8651_domainAdvRoute_routeList_freeList;
	DNS_LIST_INIT(_rtl8651_domainAdvRoute_routeList_freeList, _rtl8651_domainAdvRouteRouteList_t,(dns_domainAdvRoute_routeEntryMaxNum), next);

	_rtl8651_domainAdvRoute_dstIpTimeout = dns_domainAdvRoute_dstIpEntryTimeout;
	_rtl8651_domainAdvRoute_dstIpEntryMaxNum = dns_domainAdvRoute_dstIpEntryMaxNum;

	return SUCCESS;	

}

/* 
	REINIT function : here we move all of the route list and dstIp list to freelist,
				also need to delete the relative policy route or demand route from rome driver 
*/
void _rtl8651_domainAdvRoute_reInit(void)
{

	while (_rtl8651_domainAdvRoute_routeList_header)
	{
		_rtl8651_domainAdvRoute_delRouteList(_rtl8651_domainAdvRoute_routeList_header, TRUE);
	}
}

/* 
	PROC function : get the domain<-->mapped ip information from the DNS module,
				and cach them in the route list of domainAvdRoute module  
*/
int32 _rtl8651_domainAdvRoute_procMapping(char *orig_domain,
													char *mapped_domain,
													ipaddr_t mapped_ip,
													void *_info)
{
	_rtl8651_domainAdvRouteRouteList_t *pRouteList = NULL;
	_rtl8651_domainAdvRouteDstIpList_t *pDstIpList;
	rtl8651_domainAdvRouteProperty_t *propertyInfo = NULL;

	/* check if any orig_domain<-->mapped ip information has been cached in the routlist*/
	if((pRouteList = _rtl8651_domainAdvRoute_findDomainMatchInRouteList(pRouteList,orig_domain))!=NULL)
	{
		/* 
		ok,found a domain name (by keyword match) in the route list,
		then check if cach the same mapped ip entry in this routelist.
		*/
		if(_info != NULL)
		{
			propertyInfo = (rtl8651_domainAdvRouteProperty_t *) _info;
			memcpy(&(pRouteList->property), propertyInfo, sizeof(rtl8651_domainAdvRouteProperty_t));
		}
		
		if((pDstIpList = _rtl8651_domainAdvRoute_findDstIpMatchInDstIpList(pRouteList->dstIpList, mapped_ip))!=NULL)
		{
			/*
			now, there is orig_domain<-->mapped ip information has been cached in the routlist,
			then just update it.
			*/
			pDstIpList->updateTime = (uint32)rtl8651_getSysUpSeconds;
			/*
			add Policy route or demand route reduplicately in order to update the entry's updatetime in rome driver.
			*/
			if((pRouteList->property.connState == 0)&&(pRouteList->property.addDemandRoute)&&(pDstIpList->addDemandRouteSuccess==TRUE))
			{
				pDstIpList->addDemandRouteSuccess = FALSE;
			}			
			if((pRouteList->property.connState )&&(pRouteList->property.addPolicyRoute)&&(pDstIpList->addPolicyRouteSuccess==TRUE))
			{
				pDstIpList->addPolicyRouteSuccess = FALSE;
			}
			_rtl8651_domainAdvRoute_addDomainPolicyOrDemandRoute(pRouteList, pDstIpList);
			_rtl8651_domainAdvRoute_getDstIpListHeaderAndTail(pRouteList->dstIpList);
			HTLIST_MOVE2TAIL(pDstIpList, next, prev, _rtl8651_domainAdvRouteDstIpList_header, _rtl8651_domainAdvRouteDstIpList_tail);
			DNS_DOMAINROUTE_INFO("\nredundant entry found in route list,movie it to dstIp_tail,tail->origDomain=%s,tail->dstIp=%d.%d.%d.%d\n",
									pRouteList->orig_domain,
									((_rtl8651_domainAdvRouteDstIpList_tail->dstIp)>>24)&0xFF,
									((_rtl8651_domainAdvRouteDstIpList_tail->dstIp)>>16)&0xFF,
									((_rtl8651_domainAdvRouteDstIpList_tail->dstIp)>>8 )&0xFF,
									((_rtl8651_domainAdvRouteDstIpList_tail->dstIp)>>0)&0xFF);
		}
		else
		{
			/*
			just found matched domain,but no matched mappedIp,
			then add the new mapped ip to the domain entry,and 
			update the rome drvier information.
			*/
			if((pDstIpList = _rtl8651_domainAdvRoute_addDstIpToDstIpList(pRouteList,mapped_ip)) == NULL)
				{
				DNS_DOMAINROUTE_ERR("Error in dstIp freelist!");
				return FAILED;
				}
			_rtl8651_domainAdvRoute_addDomainPolicyOrDemandRoute(pRouteList,pDstIpList);
			DNS_DOMAINROUTE_INFO("\nJust found domain name in routeList,but not corresponding dstIp,add to dstIp_tail,tail->origDomain=%s,tail->dstIp=%d.%d.%d.%d,dstIpNum=%d\n",
									pRouteList->orig_domain,	
									((_rtl8651_domainAdvRouteDstIpList_tail->dstIp)>>24)&0xFF,
									((_rtl8651_domainAdvRouteDstIpList_tail->dstIp)>>16)&0xFF,
									((_rtl8651_domainAdvRouteDstIpList_tail->dstIp)>>8)&0xFF,
									((_rtl8651_domainAdvRouteDstIpList_tail->dstIp)>>0)&0xFF,
									pRouteList->dstIpEntryNum);
		}
		/*
		move pRouteList to route list tail,because at this time, it is the newest entry.
		*/
		HTLIST_MOVE2TAIL(pRouteList, next, prev, _rtl8651_domainAdvRoute_routeList_header, _rtl8651_domainAdvRoute_routeList_tail);
	}
	else
	{
		/*
		there is no domain matched in the route list,
		then add new orig_domain<-->mappedIp entry to the route list tail.
		and update the rome driver information.
		*/
		if((pRouteList = _rtl8651_domainAdvRoute_addDomainToRouteList(orig_domain))==NULL)
		{
			return FAILED;
		}
		DNS_DOMAINROUTE_INFO("\nno matched entry<%s>here,then add the new entry to route list.added domain name = %s,dstIp number=%d\n",
								orig_domain,pRouteList->orig_domain,pRouteList->dstIpEntryNum);
		if(_info != NULL)
		{
			propertyInfo = (rtl8651_domainAdvRouteProperty_t *) _info;
			memcpy(&(pRouteList->property), propertyInfo, sizeof(rtl8651_domainAdvRouteProperty_t));
		}
		
		if((pDstIpList = _rtl8651_domainAdvRoute_addDstIpToDstIpList(pRouteList,mapped_ip)) == NULL)
		{
			return FAILED;
		}
		DNS_DOMAINROUTE_INFO("\nadd the dstIp to the new entry,dstIp= %d.%d.%d.%d,dstIp number = %d \n",
								((pDstIpList->dstIp)>>24)&0xFF,
								((pDstIpList->dstIp)>>16)&0xFF,
								((pDstIpList->dstIp)>>8)&0xFF,
								((pDstIpList->dstIp)>>0)&0xFF,
								pRouteList->dstIpEntryNum);
		_rtl8651_domainAdvRoute_addDomainPolicyOrDemandRoute(pRouteList,pDstIpList);
	}

	/*
		Call  rtl8651_addPolicyRoute()  or  rtl8651_addDemandRoute() to add route to rome driver
	*/
	_rtl8651_domainAdvRoute_arrangeAllPolicyOrDemandRoute();
	DNS_DOMAINROUTE_INFO("\nadd the entry by procMapping function successfully, dstIpNum = %d",pRouteList->dstIpEntryNum);
	return SUCCESS;
}
		

/* 
	TimeUpdate function: Scan the route list to check if there is any dstIpList is overtime,
					if Yes, flush the dstIpList entry,and delete the relative route from
					rome driver.
*/
void _rtl8651_domainAdvRoute_timeUpdate(uint32 secPassed)
{

	_rtl8651_domainAdvRouteRouteList_t *ptr;

	ptr = _rtl8651_domainAdvRoute_routeList_header;
	while(ptr)
	{
		_rtl8651_domainAdvRoute_getDstIpListHeaderAndTail(ptr->dstIpList);

		while(_rtl8651_domainAdvRouteDstIpList_header && 
			((_rtl8651_domainAdvRouteDstIpList_header->updateTime + _rtl8651_domainAdvRoute_dstIpTimeout) < (uint32)rtl8651_getSysUpSeconds))
		{
			/*
				delete the relative policy route or demand route from rome driver before delete the dstIp 
			*/
				
			_rtl8651_domainAdvRoute_delDomainPolicyOrDemandRoute(ptr,_rtl8651_domainAdvRouteDstIpList_header);

			/*
				delete the timeup dstIp from the dstIpList
			*/			
			_rtl8651_domainAdvRoute_delDstIpList(_rtl8651_domainAdvRouteDstIpList_header, FALSE);
			ptr->dstIpEntryNum--;

		}

		ptr->dstIpList = _rtl8651_domainAdvRouteDstIpList_header;
		ptr = ptr->next;
	}
	_rtl8651_domainAdvRoute_arrangeAllPolicyOrDemandRoute();
}


/* 
	destrutcor function: delete the route list and delete all the relative route information
					from rome driver, also free the memory of the route list.
*/
int32 _rtl8651_domainAdvRoute_routeListDestructor(void *pNULL)
{
	/*
	this function would be call by DNS module to delete the relative entry from DNS module database
	or domain advance route module database.
	In this module, wo need not to call this function. the relative entry would be deleted by other funtions.
	*/
	return SUCCESS;
}


/* =========================================================================
  *
  *
  *										Internal used functions.
  *
  *
  * ========================================================================= */
#define RTL8651_DOMAINROUTE_INTERNAL

_rtl8651_domainAdvRouteRouteList_t *_rtl8651_domainAdvRoute_returnRouteListHeader(void)
{
	return _rtl8651_domainAdvRoute_routeList_header;
}

/*
	Find domain routed list entry in domain routing system.
*/
_rtl8651_domainAdvRouteRouteList_t * _rtl8651_domainAdvRoute_findDomainMatchInRouteList(_rtl8651_domainAdvRouteRouteList_t *pRouteList,char *domain)
{
	_rtl8651_domainAdvRouteRouteList_t *ptr;
	void *strMatchPos = NULL;

	if(!domain)
	{
		return NULL;
	}

	ptr = (pRouteList)?(pRouteList->next):_rtl8651_domainAdvRoute_routeList_header;

	while (ptr)
	{
		if ((strMatchPos=(void *)strstr(domain,ptr->orig_domain))!=NULL)
		{
			return ptr;   /*ok,found it and return it*/
		}
		ptr = ptr->next;
	}
	return NULL;
}

/*
	Find dstIp match entry in dstIp list
*/
_rtl8651_domainAdvRouteDstIpList_t *_rtl8651_domainAdvRoute_findDstIpMatchInDstIpList(_rtl8651_domainAdvRouteDstIpList_t *pDstIpList,ipaddr_t ip)
{
	_rtl8651_domainAdvRouteDstIpList_t * ptr;
	_rtl8651_domainAdvRoute_getDstIpListHeaderAndTail(pDstIpList);
	ptr = _rtl8651_domainAdvRouteDstIpList_header;

	if(!pDstIpList)
	{
		return NULL;
	}

	while(ptr)
	{
		if(ptr->dstIp == ip)
		{
			return ptr;
		}
		ptr =  ptr->next;
	}
	return NULL;
}

/*
	Add dstIp entry to the appropriate dstIp list
*/
static _rtl8651_domainAdvRouteDstIpList_t * _rtl8651_domainAdvRoute_addDstIpToDstIpList(_rtl8651_domainAdvRouteRouteList_t *pRouteList,ipaddr_t ip)
{
	_rtl8651_domainAdvRouteDstIpList_t *ptrFree;
	_rtl8651_domainAdvRouteDstIpList_t *ptr;
	
	if(pRouteList->dstIpEntryNum >= _rtl8651_domainAdvRoute_dstIpEntryMaxNum)
	{
		DNS_DOMAINROUTE_INFO("dstIp list of domain[%s] is full,then delete the oldest one to store the new one!",pRouteList->orig_domain);
		ptr = pRouteList->dstIpList;
		_rtl8651_domainAdvRoute_getDstIpListHeaderAndTail(ptr);
		
		DNS_DOMAINROUTE_INFO("pRoute = %p, dstIpList_header = %p",pRouteList,_rtl8651_domainAdvRouteDstIpList_header);
		_rtl8651_domainAdvRoute_delDomainPolicyOrDemandRoute(pRouteList, _rtl8651_domainAdvRouteDstIpList_header);
		_rtl8651_domainAdvRoute_delDstIpList(_rtl8651_domainAdvRouteDstIpList_header, FALSE);
		pRouteList->dstIpList = _rtl8651_domainAdvRouteDstIpList_header;
		pRouteList->dstIpEntryNum--;
	}

	if((ptrFree = _rtl8651_domainAdvRouteDstIpList_freeList) == NULL)
	{
		DNS_DOMAINROUTE_ERR("Err in dstIpList_freeList.\n");
		return NULL;
	}	
	
	/*
	OK,there is dstIpList freeList entry to be used
	*/
	_rtl8651_domainAdvRouteDstIpList_freeList = ptrFree->next;
	ptrFree->next = NULL;
	ptrFree->prev = NULL;
	ptrFree->dstIp = ip;
	ptrFree->updateTime = (uint32)rtl8651_getSysUpSeconds;
	ptrFree->addDemandRouteSuccess = FALSE;
	ptrFree->addPolicyRouteSuccess = FALSE;
	
	ptr = pRouteList->dstIpList;
	_rtl8651_domainAdvRoute_getDstIpListHeaderAndTail(ptr);
	HTLIST_ADD2TAIL(ptrFree, next, prev, _rtl8651_domainAdvRouteDstIpList_header, _rtl8651_domainAdvRouteDstIpList_tail);
	pRouteList->dstIpList = _rtl8651_domainAdvRouteDstIpList_header;
	pRouteList->dstIpEntryNum++;

	return ptrFree;
}

/*
	add domain entry into route list. ATTENTION: before call this function, 
	be sure that there is no redundant domain in the route list! 
*/
static _rtl8651_domainAdvRouteRouteList_t *_rtl8651_domainAdvRoute_addDomainToRouteList(char *domain)
{
	_rtl8651_domainAdvRouteRouteList_t *ptrFree;

	if(!domain)
	{
		return NULL;
	}

	if((ptrFree =_rtl8651_domainAdvRoute_routeList_freeList)==NULL)
	{
		DNS_DOMAINROUTE_WARN("add faild because the route list is full.\n");
		return NULL;
	}
	/*ok,ptrFree is the valid free route list*/
	_rtl8651_domainAdvRoute_routeList_freeList = _rtl8651_domainAdvRoute_routeList_freeList->next;

	memset(ptrFree, 0, sizeof(_rtl8651_domainAdvRouteRouteList_t));
	strcpy(ptrFree->orig_domain,domain);
	HTLIST_ADD2TAIL(ptrFree,next,prev,_rtl8651_domainAdvRoute_routeList_header,_rtl8651_domainAdvRoute_routeList_tail);

	return ptrFree;		
}

/*
	Delete routing list entry in domain routing system,also delete the relative
	route information from rome driver.
*/
static int32 _rtl8651_domainAdvRoute_delRouteList(_rtl8651_domainAdvRouteRouteList_t *ptr, uint8 refreshAction)
{

	if (ptr == NULL)
	{
		return FAILED;
	}

	/*delete the dstIp list of _rtl8651_domainAdvRoute_routeList_header*/
	if(ptr->dstIpList)
		{
		/*
			get the list header and tail,and store them in the global parameter:
			_rtl8651_domainAdvRouteDstIpList_header,_rtl8651_domainAdvRouteDstIpList_tail
		*/
			_rtl8651_domainAdvRoute_getDstIpListHeaderAndTail(ptr->dstIpList);
		
			while(_rtl8651_domainAdvRouteDstIpList_header)
			{
				_rtl8651_domainAdvRoute_delDomainPolicyOrDemandRoute(ptr, _rtl8651_domainAdvRouteDstIpList_header);

				/*remove the dstIP list*/
				_rtl8651_domainAdvRoute_delDstIpList(_rtl8651_domainAdvRouteDstIpList_header, FALSE);
				ptr->dstIpEntryNum--;
			}
			ptr->dstIpList = _rtl8651_domainAdvRouteDstIpList_header;
		}
		
	/* 
		now the dstIpList of the entry is empty,then remove entry from route list 
	*/
	HTLIST_REMOVE(ptr, next, prev, _rtl8651_domainAdvRoute_routeList_header, _rtl8651_domainAdvRoute_routeList_tail);
	memset(ptr, 0, sizeof(_rtl8651_domainAdvRouteRouteList_t));
	HLIST_ADD2HDR(ptr, next, prev, _rtl8651_domainAdvRoute_routeList_freeList);

	if (refreshAction == TRUE)
	{
		_rtl8651_domainAdvRoute_arrangeAllPolicyOrDemandRoute();
	}
	return SUCCESS;
}

/*
	delete the dstIp entry from the dstIpList which it belongs to
*/
static int32 _rtl8651_domainAdvRoute_delDstIpList(_rtl8651_domainAdvRouteDstIpList_t *ptr, uint8 refreshAction)
{
	if(NULL==ptr)
	{
		return FAILED;
	}
	
	/*remove the ptr entry from the list*/
	HTLIST_REMOVE(ptr, next, prev, _rtl8651_domainAdvRouteDstIpList_header, _rtl8651_domainAdvRouteDstIpList_tail);
	memset(ptr, 0, sizeof(_rtl8651_domainAdvRouteDstIpList_t));
	HLIST_ADD2HDR(ptr, next, prev, _rtl8651_domainAdvRouteDstIpList_freeList);

	if(TRUE == refreshAction)
	{
		_rtl8651_domainAdvRoute_arrangeAllPolicyOrDemandRoute();
	}
	return SUCCESS;
}
/*
	Get a dstIp list's header and tail,and store them in global parameters:
		_rtl8651_domainAdvRouteDstIpList_header,_rtl8651_domainAdvRouteDstIpList_tail
*/
void _rtl8651_domainAdvRoute_getDstIpListHeaderAndTail(_rtl8651_domainAdvRouteDstIpList_t *ptr)
{

	if(NULL == ptr)
	{
		_rtl8651_domainAdvRouteDstIpList_header = ptr;
		_rtl8651_domainAdvRouteDstIpList_tail = ptr;
		return;
	}
	
	_rtl8651_domainAdvRouteDstIpList_header=ptr;
	while(_rtl8651_domainAdvRouteDstIpList_header->prev)
	{
		_rtl8651_domainAdvRouteDstIpList_header=_rtl8651_domainAdvRouteDstIpList_header->prev;
	}
	
	_rtl8651_domainAdvRouteDstIpList_tail=ptr;
	while(_rtl8651_domainAdvRouteDstIpList_tail->next)
	{
		_rtl8651_domainAdvRouteDstIpList_tail=_rtl8651_domainAdvRouteDstIpList_tail->next;
	}
}


int32 _rtl8651_domainAdvRoute_addDomainPolicyOrDemandRoute(_rtl8651_domainAdvRouteRouteList_t *pRouteList,_rtl8651_domainAdvRouteDstIpList_t *pDstIpList)
{
	int32 rtVal = FAILED;
	if((!pRouteList)||(!pDstIpList))
	{
		return FAILED;
	}

	if((pRouteList->property.connState )&&(pRouteList->property.addPolicyRoute)&&(pDstIpList->addPolicyRouteSuccess==FALSE))
	{
		if(_addDomainBasedPolicyRouteToRomeDriver(pRouteList,pDstIpList) ==SUCCESS)
		{
			DNS_DOMAINROUTE_INFO("=====>add policy route successfully!dstIp = %d.%d.%d.%d.\n",
									((pDstIpList->dstIp)>>24)&0xFF,
									((pDstIpList->dstIp)>>16)&0xFF,
									((pDstIpList->dstIp)>>8)&0xFF,
									((pDstIpList->dstIp)>>0)&0xFF);
			pDstIpList->addPolicyRouteSuccess= TRUE;
			rtVal = SUCCESS;
		}
		else
		{
			DNS_DOMAINROUTE_INFO("=====>failed to add policy route!dstIp = %d.%d.%d.%d.\n",
									((pDstIpList->dstIp)>>24)&0xFF,
									((pDstIpList->dstIp)>>16)&0xFF,
									((pDstIpList->dstIp)>>8)&0xFF,
									((pDstIpList->dstIp)>>0)&0xFF);
		}
	}

	if((pRouteList->property.connState == FALSE)&&(pRouteList->property.addDemandRoute)&&(pDstIpList->addDemandRouteSuccess==FALSE))
	{
		if(_addDomainBasedDemandRouteToRomeDriver(pRouteList,pDstIpList) ==SUCCESS)
		{
			DNS_DOMAINROUTE_INFO("=====>add demand route successfully!dstIp = %d.%d.%d.%d.\n",
									((pDstIpList->dstIp)>>24)&0xFF,
									((pDstIpList->dstIp)>>16)&0xFF,
									((pDstIpList->dstIp)>>8)&0xFF,
									((pDstIpList->dstIp)>>0)&0xFF);
			pDstIpList->addDemandRouteSuccess = TRUE;
			rtVal = SUCCESS;
		}
		else
		{
			DNS_DOMAINROUTE_INFO("=====>failed to add demand route!dstIp = %d.%d.%d.%d.\n",
									((pDstIpList->dstIp)>>24)&0xFF,
									((pDstIpList->dstIp)>>16)&0xFF,
									((pDstIpList->dstIp)>>8)&0xFF,
									((pDstIpList->dstIp)>>0)&0xFF);
		}
	}
	return rtVal;
}

int32 _addDomainBasedPolicyRouteToRomeDriver(_rtl8651_domainAdvRouteRouteList_t *ptr,_rtl8651_domainAdvRouteDstIpList_t *ptrDstIpList)
{
	rtl8651_tblDrvPolicyRoute_t pAddPolicyRoute;

	if((ptr == NULL)||(ptrDstIpList ==NULL))
	{
		DNS_DOMAINROUTE_INFO("FAILED:add policy route failed because of NULL pointer!");
		return FAILED;
	}
	if(ptr->property.sessionIp == 0)
	{
		return FAILED;
	}

	memset(&pAddPolicyRoute, 0, sizeof(rtl8651_tblDrvPolicyRoute_t));
	pAddPolicyRoute.type = DYNAMIC_POLICY_ROUTE|TRIGGER_DSTIP;
	pAddPolicyRoute.ip_start = ptrDstIpList->dstIp;
	pAddPolicyRoute.ip_end  = ptrDstIpList->dstIp;
	pAddPolicyRoute.ip_proto = 0;
	pAddPolicyRoute.ip_alias = ptr->property.sessionIp;
	pAddPolicyRoute.age	= RTL8651_DOMAINADVROUTE_DEFAULT_DSTIPENTRYTIMEOUT;
			
	return rtl8651_addPolicyRoute(&pAddPolicyRoute);
}

int32 _addDomainBasedDemandRouteToRomeDriver(_rtl8651_domainAdvRouteRouteList_t *ptr,_rtl8651_domainAdvRouteDstIpList_t *ptrDstIpList)
{
	if(ptrDstIpList ==NULL)
	{
		DNS_DOMAINROUTE_INFO("FAILED:add demande route failed because of NULL pointer!");
		return FAILED;
	}

	rtl8651_tblDrvDemandRoute_t  pAddDemandRoute;
	
	memset(&pAddDemandRoute,0,sizeof(rtl8651_tblDrvDemandRoute_t));
	pAddDemandRoute.type = TRIGGER_DSTIP;
	pAddDemandRoute.ip_start = ptrDstIpList->dstIp;
	pAddDemandRoute.ip_end = ptrDstIpList->dstIp;
	pAddDemandRoute.ip_proto = 0;

	return rtl8651_addDemandRoute(&pAddDemandRoute,ptr->property.sessionId,ptr->property.pDemandCallback);
}


int32 _rtl8651_domainAdvRoute_delDomainPolicyOrDemandRoute(_rtl8651_domainAdvRouteRouteList_t *pRouteList,_rtl8651_domainAdvRouteDstIpList_t *pDstIpList)
{

	if((!pRouteList)||(!pDstIpList))
	{
		return FALSE;
	}
	if((pRouteList->property.connState )&&(pRouteList->property.addPolicyRoute)&&(pDstIpList->addPolicyRouteSuccess==TRUE))
	{
		if(_delDomainBasedPolicyRouteToRomeDriver(pRouteList,pDstIpList) ==SUCCESS)
		{
			pDstIpList->addPolicyRouteSuccess= FALSE;
		}			
	}

	if((pRouteList->property.connState == FALSE)&&(pRouteList->property.addDemandRoute)&&(pDstIpList->addDemandRouteSuccess==TRUE))
	{
		if(_delDomainBasedDemandRouteToRomeDriver(pRouteList,pDstIpList) ==SUCCESS)
		{
			pDstIpList->addDemandRouteSuccess = FALSE;
		}
	}
	return SUCCESS;
}

int32 _delDomainBasedPolicyRouteToRomeDriver(_rtl8651_domainAdvRouteRouteList_t *ptr,_rtl8651_domainAdvRouteDstIpList_t *ptrDstIpList)
{
	rtl8651_tblDrvPolicyRoute_t pDelPolicyRoute;
	
	memset(&pDelPolicyRoute, 0, sizeof(rtl8651_tblDrvPolicyRoute_t));
	pDelPolicyRoute.type = DYNAMIC_POLICY_ROUTE|TRIGGER_DSTIP;
	pDelPolicyRoute.ip_start = ptrDstIpList->dstIp;
	pDelPolicyRoute.ip_end  = ptrDstIpList->dstIp;
	pDelPolicyRoute.ip_proto = 0;
	pDelPolicyRoute.ip_alias = ptr->property.sessionIp;
	pDelPolicyRoute.age	= RTL8651_DOMAINADVROUTE_DEFAULT_DSTIPENTRYTIMEOUT;
			
	return rtl8651_delPolicyRoute(&pDelPolicyRoute);
}

int32 _delDomainBasedDemandRouteToRomeDriver(_rtl8651_domainAdvRouteRouteList_t *ptr,_rtl8651_domainAdvRouteDstIpList_t *ptrDstIpList)
{
	rtl8651_tblDrvDemandRoute_t pDelDemandRoute;
	
	memset(&pDelDemandRoute,0,sizeof(rtl8651_tblDrvDemandRoute_t));
	pDelDemandRoute.type = TRIGGER_DSTIP;
	pDelDemandRoute.ip_start = ptrDstIpList->dstIp;
	pDelDemandRoute.ip_end = ptrDstIpList->dstIp;
	pDelDemandRoute.ip_proto = 0;

	return rtl8651_delDemandRoute(&pDelDemandRoute);
}

void _rtl8651_domainAdvRoute_arrangeAllPolicyOrDemandRoute(void)
{
	_rtl8651_domainAdvRouteRouteList_t *pRouteList;
	_rtl8651_domainAdvRouteDstIpList_t *pDstIpList;

	pRouteList = _rtl8651_domainAdvRoute_routeList_header;

	while(pRouteList)
		{
		pDstIpList = pRouteList->dstIpList;
		while(pDstIpList)
			{
		/*
			delete void policy route or demand route
		*/
			if((pRouteList->property.addDemandRoute == FALSE)&&(pDstIpList->addDemandRouteSuccess == TRUE))
			{
				pDstIpList->addDemandRouteSuccess = ((_delDomainBasedDemandRouteToRomeDriver(pRouteList,pDstIpList)==SUCCESS)?FALSE:TRUE);
			}
			if((pRouteList->property.addPolicyRoute == FALSE)&&(pDstIpList->addPolicyRouteSuccess== TRUE))
			{
				pDstIpList->addPolicyRouteSuccess = ((_delDomainBasedPolicyRouteToRomeDriver(pRouteList,pDstIpList)==SUCCESS)?FALSE:TRUE);
			}

		/*
			add policy route or demand route
		*/	
			if((pRouteList->property.connState )&&(pRouteList->property.addPolicyRoute)&&(pDstIpList->addPolicyRouteSuccess==FALSE))
			{
				if(_addDomainBasedPolicyRouteToRomeDriver(pRouteList,pDstIpList) ==SUCCESS)
				{
					DNS_DOMAINROUTE_INFO("=====>add policy route successfully!dstIp = %d.%d.%d.%d;    sessionIp=%d.%d.%d.%d. \n",
												((pDstIpList->dstIp)>>24)&0xFF,
												((pDstIpList->dstIp)>>16)&0xFF,
												((pDstIpList->dstIp)>>8)&0xFF,
												((pDstIpList->dstIp)>>0)&0xFF,
												((pRouteList->property.sessionIp)>>24)&0xFF,
												((pRouteList->property.sessionIp)>>16)&0xFF,
												((pRouteList->property.sessionIp)>>8)&0xFF,
												((pRouteList->property.sessionIp)>>0)&0xFF);
					pDstIpList->addPolicyRouteSuccess= TRUE;
				}
				else
				{
					DNS_DOMAINROUTE_INFO("=====>failed to add policy route!dstIp = %d.%d.%d.%d;    sessionIp=%d.%d.%d.%d. \n",
												((pDstIpList->dstIp)>>24)&0xFF,
												((pDstIpList->dstIp)>>16)&0xFF,
												((pDstIpList->dstIp)>>8)&0xFF,
												((pDstIpList->dstIp)>>0)&0xFF,
												((pRouteList->property.sessionIp)>>24)&0xFF,
												((pRouteList->property.sessionIp)>>16)&0xFF,
												((pRouteList->property.sessionIp)>>8)&0xFF,
												((pRouteList->property.sessionIp)>>0)&0xFF);
				}				
			}
								
			if((pRouteList->property.connState == FALSE)&&(pRouteList->property.addDemandRoute)&&(pDstIpList->addDemandRouteSuccess==FALSE))
			{
				if(_addDomainBasedDemandRouteToRomeDriver(pRouteList,pDstIpList) ==SUCCESS)
				{
					DNS_DOMAINROUTE_INFO("=====>add demande route successfully!dstIp = %d.%d.%d.%d;    sessionIp=%d.%d.%d.%d. \n",
												((pDstIpList->dstIp)>>24)&0xFF,
												((pDstIpList->dstIp)>>16)&0xFF,
												((pDstIpList->dstIp)>>8)&0xFF,
												((pDstIpList->dstIp)>>0)&0xFF,
												((pRouteList->property.sessionIp)>>24)&0xFF,
												((pRouteList->property.sessionIp)>>16)&0xFF,
												((pRouteList->property.sessionIp)>>8)&0xFF,
												((pRouteList->property.sessionIp)>>0)&0xFF);
					pDstIpList->addDemandRouteSuccess = TRUE;
				}
				else
				{
					DNS_DOMAINROUTE_INFO("=====>failed to add demand route!dstIp = %d.%d.%d.%d;    sessionIp=%d.%d.%d.%d. \n",
												((pDstIpList->dstIp)>>24)&0xFF,
												((pDstIpList->dstIp)>>16)&0xFF,
												((pDstIpList->dstIp)>>8)&0xFF,
												((pDstIpList->dstIp)>>0)&0xFF,
												((pRouteList->property.sessionIp)>>24)&0xFF,
												((pRouteList->property.sessionIp)>>16)&0xFF,
												((pRouteList->property.sessionIp)>>8)&0xFF,
												((pRouteList->property.sessionIp)>>0)&0xFF);
				}
			}
			pDstIpList = pDstIpList->next;
			}
		pRouteList = pRouteList->next;
		}
}


/* =========================================================================
  *
  *
  *										External functions.
  *
  *			1. for other functions in DRIVER	- without MUTEX protection
  *			2. for USERs					- with MUTEX protection
  *
  *
  * ========================================================================= */
#define RTL8651_DOMAINROUTE_EXTERNAL

/*
@func int32							| rtl8651_domainAdvRoute_addDomainEntry | Add domain routing entry
@parm char*							| domain_name	| Domain name keyword to route.
@parm _rtl8651_domainAdvRouteProperty_t	| property			| domain's property information such as sessionId,
													    sessionIp,connstate and addPolicyRoute or demandRoute flags.
@rvalue SUCCESS						| Entry addition success.
@rvalue FAILED							| Entry addition failed.if any one of DNS module and domain advance route module database
									   is fully, the addition failed and the entry would NOT be added into BOTH DNS module and 
									   domain advance route module database
@comm
	Add domain routing entry into both DNS module and domain advance route module database to route the target domain.
	This is keyword match, so when user define 'abc' in <p domain_name> , all domain names with the keyword 'abc' would hit..
	By the way, this domain routing function provide <p property> to allow user set more information to domain name,such as 
	the relative sessionId,sessionIp,	connstate and addPolicyRoute or demandRoute flags.
@devnote
*/
int32 rtl8651_domainAdvRoute_addDomainEntry(char *domain_name, rtl8651_domainAdvRouteProperty_t *property)
{
	int32 retval;

	if (!domain_name)
	{
		return FAILED;
	}

	rtlglue_drvMutexLock();
	retval = _rtl8651_domainAdvRoute_addDomainEntry(domain_name, property);
	rtlglue_drvMutexUnlock();

	return retval;
}

int32 _rtl8651_domainAdvRoute_addDomainEntry(char *domain_name, rtl8651_domainAdvRouteProperty_t *property)
{
	void *tmp = NULL;
	char *returnDomain = NULL;
	rtl8651_domainAdvRouteProperty_t *returnProperty = NULL;
	rtl8651_domainAdvRouteProperty_t *delProperty = NULL;
	rtl8651_domainAdvRouteProperty_t *addProperty = NULL;
	_rtl8651_domainAdvRouteRouteList_t *pRouteList = NULL;
	_rtl8651_domainAdvRouteDstIpList_t *pDstIpList = NULL;

	if (!domain_name)
	{
		return FAILED;
	}

	/* check redundant setting */
	while (rtl8651_dns_findProcessEntry(	_rtl8651_domainAdvRoute_procSequence,
											domain_name,
											NULL,
											(char**)&returnDomain,
											(void*)&returnProperty,
											(void*)&tmp) == TRUE)
	{
		assert(returnProperty);
		if (strcmp(returnDomain, domain_name) == 0)
		{
			/*
			now find the redundant domain_name,then check relative the sessionID,
			if sessionID is equal, then found the redundant entry and return seccess,
			else,modify the sessionID to accord with the sessionID  wanted to be added. 
			*/			

			DNS_DOMAINROUTE_INFO("the redundant entry  found.\nIt's domain name is %s,and it's sessionId is %d.\n", returnDomain,returnProperty->sessionId);
			/*
				check if the redundant entry in domain route list, if find,update it's property;if find none, add it
			*/
			if((pRouteList = _rtl8651_domainAdvRoute_findDomainMatchInRouteList(pRouteList,domain_name))==NULL)
			{
				DNS_DOMAINROUTE_INFO("no any redundant entry here,  Add the new entry to domain advance route .\n");
				if((pRouteList = _rtl8651_domainAdvRoute_addDomainToRouteList(domain_name))==NULL)
				{
					DNS_DOMAINROUTE_ERR("Add the new entry to domain advance route error!\n");
					/*
						add failed because of full domain route list, then we must delete the relative entry from
						DNS module.
					*/
					delProperty = returnProperty;

					if(rtl8651_dns_delProcessEntry(	_rtl8651_domainAdvRoute_procSequence,
													FALSE,
													NULL,
													returnDomain,
													returnProperty) != SUCCESS)
					{
						DNS_DOMAINROUTE_ERR("error in deleting entry %s from DNS module ", returnDomain);
						return FAILED;
					}
					MEM_FREE(delProperty);
					return FAILED;
				}
				memcpy(&(pRouteList->property), property,sizeof(rtl8651_domainAdvRouteProperty_t));
				DNS_DOMAINROUTE_INFO("Add the new entry to domain advance route successfully!\n");
			}
			else
			{
				/*
					if the sessionId of redundant entry is changed when the relative entry try to add into 
					domain route module, we MUST delete the old policy route or demand route in tblDrv,
					and then add the new policy route or demand route again.
				*/
				if((property->sessionId) != (pRouteList->property.sessionId))
				{
					pDstIpList = pRouteList->dstIpList;
					while(pDstIpList)
					{
						_rtl8651_domainAdvRoute_delDomainPolicyOrDemandRoute(pRouteList, pDstIpList);
						pDstIpList = pDstIpList->next;
					}
				}
				
				memcpy(&(pRouteList->property), property,sizeof(rtl8651_domainAdvRouteProperty_t));
								
				pDstIpList = pRouteList->dstIpList;
				while(pDstIpList)
				{
					_rtl8651_domainAdvRoute_addDomainPolicyOrDemandRoute(pRouteList, pDstIpList);
					pDstIpList = pDstIpList->next;
				}				
			}
			
			memcpy(returnProperty, property,sizeof(rtl8651_domainAdvRouteProperty_t));
			_rtl8651_domainAdvRoute_arrangeAllPolicyOrDemandRoute();			
			return SUCCESS;
			
		}
	}
	/*here need to add this domain entry information to DNS Advance module */
	/*
		ATTENTION:the memory allocate here will be free in the function 
		_rtl8651_domainAdvRoute_delDomainEntry
	*/
	MEM_ALLOC(addProperty, sizeof(rtl8651_domainAdvRouteProperty_t), FALSE);  
	memcpy(addProperty, property, sizeof(rtl8651_domainAdvRouteProperty_t));
	
	if (rtl8651_dns_addProcessEntry(	_rtl8651_domainAdvRoute_procSequence,
										domain_name,
										(void*)addProperty,
										NULL) == SUCCESS)
	{
		DNS_DOMAINROUTE_INFO("the entry '%s' have been added to DNS module successfully.\n",domain_name);
	/*
		add the domain name into the domain Advance route module routlist
	*/

	/* check if any orig_domain<-->mapped ip information has been cached in the routlist*/
		DNS_DOMAINROUTE_INFO("try to  Add the new entry to domain advance route .\n");
		if((pRouteList = _rtl8651_domainAdvRoute_findDomainMatchInRouteList(pRouteList,domain_name))==NULL)
		{
			DNS_DOMAINROUTE_INFO("no any redundant entry here,  Add the new entry to domain advance route .\n");
			if((pRouteList = _rtl8651_domainAdvRoute_addDomainToRouteList(domain_name))==NULL)
			{
				DNS_DOMAINROUTE_ERR("Add the new entry to domain advance route error!\n");
				/*
					add failed because of full domain route list, then we must delete the relative entry from
					DNS module.
				*/
				delProperty = addProperty;

				if(rtl8651_dns_delProcessEntry(	_rtl8651_domainAdvRoute_procSequence,
												FALSE,
												NULL,
												domain_name,
												addProperty) != SUCCESS)
				{
					DNS_DOMAINROUTE_ERR("error in deleting entry %s from DNS module ", domain_name);
					return FAILED;
				}
				MEM_FREE(delProperty);
				return FAILED;
			}
			memcpy(&(pRouteList->property), property,sizeof(rtl8651_domainAdvRouteProperty_t));
			DNS_DOMAINROUTE_INFO("Add the new entry to domain advance route successfully!\n");
		}
		else
		{
			/*
				if the sessionId of redundant entry is changed when the relative entry try to add into 
				domain route module, we MUST delete the old policy route or demand route in tblDrv,
				and then add the new policy route or demand route again.
			*/
			if((property->sessionId) != (pRouteList->property.sessionId))
			{
				pDstIpList = pRouteList->dstIpList;
				while(pDstIpList)
				{
					_rtl8651_domainAdvRoute_delDomainPolicyOrDemandRoute(pRouteList, pDstIpList);
					pDstIpList = pDstIpList->next;
				}
			}
			
			memcpy(&(pRouteList->property), property,sizeof(rtl8651_domainAdvRouteProperty_t));
							
			pDstIpList = pRouteList->dstIpList;
			while(pDstIpList)
			{
				_rtl8651_domainAdvRoute_addDomainPolicyOrDemandRoute(pRouteList, pDstIpList);
				pDstIpList = pDstIpList->next;
			}				
		}
		_rtl8651_domainAdvRoute_arrangeAllPolicyOrDemandRoute();
		return SUCCESS;
	}
	MEM_FREE(addProperty);
	return FAILED;
}

/*
@func int32							| rtl8651_domainAdvRoute_delDomainEntry | Delete domain routing entry
@parm char*							| domain_name	| Domain name keyword to route.
@parm _rtl8651_domainAdvRouteProperty_t	| property			| Related domain property such as sessionId,
													    sessionIp,connstate and addPolicyRoute or demandRoute flags.
@rvalue SUCCESS						| Entry deletion success (might entry is not found!).
@rvalue FAILED							| Entry deletion failed.
@comm
	Delete the existent domain routing entry from both DNS module and domain advance route module database.
	Note that user MUST set appropriate information (both <p domain_name> and <p property>) to delete entry. this function 
	would delete the relative entry according to both <p domain_name> and sessionId in <p property>.
@devnote
*/

int32 rtl8651_domainAdvRoute_delDomainEntry(char *domain_name, rtl8651_domainAdvRouteProperty_t *property)
{
	int32 retval;

	if (!domain_name)
	{
		return FAILED;
	}
	
	rtlglue_drvMutexLock();
	retval = _rtl8651_domainAdvRoute_delDomainEntry(domain_name, property);
	rtlglue_drvMutexUnlock();

	return retval;
}


int32 _rtl8651_domainAdvRoute_delDomainEntry(char *domain_name, rtl8651_domainAdvRouteProperty_t *property)
{
	void *tmp = NULL;
	char *returnDomain;
	
	rtl8651_domainAdvRouteProperty_t *returnProperty = NULL;
	rtl8651_domainAdvRouteProperty_t *delProperty = NULL;
	_rtl8651_domainAdvRouteRouteList_t * delRouteList = NULL;

	if (domain_name == NULL)
	{
		return FAILED;
	}

	/* find entry to delete */
	while (rtl8651_dns_findProcessEntry(	_rtl8651_domainAdvRoute_procSequence,
											domain_name,
											NULL,
											(char**)&returnDomain,
											(void*)&returnProperty,
											(void*)&tmp) == TRUE)
	{
		assert(returnProperty);
	
		if (	(strcmp(returnDomain, domain_name) == 0) &&
			(((rtl8651_domainAdvRouteProperty_t *)returnProperty)->sessionId == property->sessionId))
		{	
			/* ok, found the entry,then delete it */
			delProperty = returnProperty;

			if(rtl8651_dns_delProcessEntry(	_rtl8651_domainAdvRoute_procSequence,
											FALSE,
											NULL,
											returnDomain,
											returnProperty) != SUCCESS)
			{
				DNS_DOMAINROUTE_ERR("error in deleting entry %s from DNS module ", domain_name);
				return FAILED;
			}
			DNS_DOMAINROUTE_INFO("delete entry %s from DNS module successfully !", domain_name);
			MEM_FREE(delProperty);

			/*delete this entry from DNS Advance module*/
			if((delRouteList = _rtl8651_domainAdvRoute_findDomainMatchInRouteList(NULL,domain_name))!= NULL)
			{
				if(_rtl8651_domainAdvRoute_delRouteList(delRouteList, FALSE) != SUCCESS)
				{
				DNS_DOMAINROUTE_ERR("\n Error in deleting exist entry %s form domain advance route database!\n", domain_name);
				return FAILED;
				}
				else
					{
					DNS_DOMAINROUTE_INFO("delete entry %s from domain route module successfully !", domain_name);
					}
			}

		}
	}	
			
	return SUCCESS;
}

/*
@func int32							| rtl8651_domainAdvRoute_changeDomainProperty | Change relative domain's property
@parm char*							| domain_name	| Domain name keyword.
@parm _rtl8651_domainAdvRouteProperty_t	| property			| Property would be set. Property information include sessionId,sessionIp,
													   connstate,addPolicyRoute and addDemandRoute flags.
@rvalue SUCCESS						| domain's property be chenged successfully (might relative entry is not found!).
@rvalue FAILED							| change property failed.
@comm
	change the property of relative domain whose orig_domain is keyword match <p domain_name>.
	This function would try to find the entry which orig_domain accord with the <p domain_name>,and change it's property,such as sessionId,sessionIp,
	connstate,addPolicyRoute and addDemandRoute flags..if find none,it would do nothing.
@devnote
*/
int32 rtl8651_domainAdvRoute_changeDomainProperty(char *domain_name, rtl8651_domainAdvRouteProperty_t *property)
{
	int32 retval;
	
	if (!domain_name)
	{
		return FAILED;
	}

	rtlglue_drvMutexLock();
	retval = _rtl8651_domainAdvRoute_changeDomainProperty(domain_name, property);
	rtlglue_drvMutexUnlock();

	return retval;
}

int32 _rtl8651_domainAdvRoute_changeDomainProperty(char *domain_name, rtl8651_domainAdvRouteProperty_t *property)
{
	_rtl8651_domainAdvRouteRouteList_t *pFind = NULL;
	
	if (!domain_name)
	{
		return FAILED;
	}

	while( (pFind = _rtl8651_domainAdvRoute_findDomainMatchInRouteList(pFind,domain_name))!= NULL)
	{
		
			/*
			now find the redundant domain_name,modify the sessionID to 
			accord with the sessionID  wanted to be added. 
			*/
			DNS_DOMAINROUTE_INFO("before change: entry %s sessionId = %d,connectstate = %d.\n",domain_name,pFind->property.sessionId,pFind->property.connState);
			memcpy(&(pFind->property), property, sizeof(rtl8651_domainAdvRouteProperty_t));	
			DNS_DOMAINROUTE_INFO("after change: entry %s sessionId = %d,connectstate = %d.\n",domain_name,pFind->property.sessionId,pFind->property.connState);
	}
	return SUCCESS;
}

/*
@func int32							| rtl8651_domainAdvRoute_changeSessionProperty | Change the property of all entry whose sessionId equal property->sessionId.
@parm _rtl8651_domainAdvRouteProperty_t	| property			| Property would be set,such as sessionId,sessionIp,connstate,addPolicyRoute and addDemandRoute flags.
@rvalue SUCCESS						| relative entry's property be chenged successfully (might relative entry is not found!).
@rvalue FAILED							| change property failed.
@comm
	change the entry's property of relative entry.
	This function would try to find the entry which sessionid accord with the sessionId of <p property>,and change it's(or their) property to be <p property>.if find none,it would do nothing.
@devnote
*/
int32 rtl8651_domainAdvRoute_changeSessionProperty(rtl8651_domainAdvRouteProperty_t *property)
{
	int32 retval = FAILED;

	rtlglue_drvMutexLock();
	retval = _rtl8651_domainAdvRoute_changeSessionProperty(property);
	rtlglue_drvMutexUnlock();

	return retval;
}
int32 _rtl8651_domainAdvRoute_changeSessionProperty(rtl8651_domainAdvRouteProperty_t *property)
{

	_rtl8651_domainAdvRouteRouteList_t *ptrRouteList = NULL;
	ptrRouteList = _rtl8651_domainAdvRoute_routeList_header;

	while(ptrRouteList)
	{
		if(ptrRouteList->property.sessionId == property->sessionId)
		{
			memcpy(&(ptrRouteList->property), property, sizeof(rtl8651_domainAdvRouteProperty_t));
		}
		ptrRouteList = ptrRouteList->next;
	}
	return SUCCESS;
}

/*
@func int32							| rtl8651_domainAdvRoute_delPolicyOrDemandRouteBySessionId | delete the policy route or demand route which belong to <p sessionId>
@parm uint8							| sessionId		| sessionId,route entry belongs to this sessionId would be relatived.
@rvalue SUCCESS						| delete successfully (might relative entry is not found!).
@rvalue FAILED							| void session Id.
@comm
	delete the relative policy route or demand route that belong to <p sessionId>.This function would try to find the entry that belong to <p sessionId>
	and delete the relatvie route information from Rome Drvier.
@devnote
*/
int32 rtl8651_domainAdvRoute_delPolicyOrDemandRouteBySessionId(uint8 sessionId)
{
	int32 retval = FAILED;

	rtlglue_drvMutexLock();
	retval = _rtl8651_domainAdvRoute_delPolicyOrDemandRouteBySessionId(sessionId);
	rtlglue_drvMutexUnlock();

	return retval;
}

int32 _rtl8651_domainAdvRoute_delPolicyOrDemandRouteBySessionId(uint8 sessionId)
{
	_rtl8651_domainAdvRouteRouteList_t *ptrRouteList = NULL;
	_rtl8651_domainAdvRouteDstIpList_t *ptrDstIpList=NULL;
	ptrRouteList = _rtl8651_domainAdvRoute_routeList_header;

	while(ptrRouteList)
	{
		if(ptrRouteList->property.sessionId == sessionId)
		{
		/*delete the route from rome driver accord with old property*/
			ptrDstIpList = ptrRouteList->dstIpList;
			while(ptrDstIpList)
			{
				_rtl8651_domainAdvRoute_delDomainPolicyOrDemandRoute(ptrRouteList, ptrDstIpList);
				ptrDstIpList = ptrDstIpList->next;
			}
		}
		ptrRouteList = ptrRouteList->next;
	}
	return SUCCESS;
}
/*
@func int32							| rtl8651_domainAdvRoute_flushSessionEntry | Flush all entrys belong to <p sessionId>
@parm uint8							| sessionId		| sessionId,entry belongs to this session would be flushed.
@rvalue SUCCESS						| relative entry be flushed successfully (might relative entry is not found!).
@rvalue FAILED							| flush entry failed.
@comm
	flush all the entrys belong to <p sessionId>.
	This function would try to find the entrys which sessionId accord with the <p sessionId>,and flush them.if find none,it would do nothing.
@devnote
*/
int32 rtl8651_domainAdvRoute_flushSessionEntry(uint8 sessionId)
{
	int32 retval = FAILED;

	rtlglue_drvMutexLock();
	retval = _rtl8651_domainAdvRoute_flushSessionEntry(sessionId);
	rtlglue_drvMutexUnlock();

	return retval;
}

int32 _rtl8651_domainAdvRoute_flushSessionEntry(uint8 sessionId)
{
	int32 retval = FAILED;
	_rtl8651_domainAdvRouteRouteList_t *ptrRouteList = NULL, *pRouteListTmp = NULL;
	
	ptrRouteList = _rtl8651_domainAdvRoute_routeList_header;
	while(ptrRouteList)
	{
		if(ptrRouteList->property.sessionId == sessionId)
		{
			DNS_DOMAINROUTE_INFO("found the entry belongs to session%d,domain name: %s, then delete it!\n",sessionId,ptrRouteList->orig_domain);
			pRouteListTmp = ptrRouteList->next;
			_rtl8651_domainAdvRoute_delRouteList(ptrRouteList, FALSE);
			retval = SUCCESS;
			ptrRouteList = pRouteListTmp;
		}
		else
		{
			ptrRouteList = ptrRouteList->next;
		}
	}
	return retval;
}

/*
@func void							| rtl8651_domainAdvRoute_reInitRouteList | reinit all routeList and dstIpList.
@comm
	reinit the routeList and dstIpList, After this action,the state of domain advance route module database would be the same
	as that of initialization.
@devnote
*/
void rtl8651_domainAdvRoute_reInitRouteList(void)
{
	rtlglue_drvMutexLock();
	
	_rtl8651_domainAdvRoute_reInit();
	DNS_DOMAINROUTE_INFO("the route list has been reinited!\n");
	
	rtlglue_drvMutexUnlock();	
}

/*
@func int32							| rtl8651_domainAdvRoute_destroyAllRouteList  | destruct all routeList and dstIpList.
@rvalue SUCCESS						| destruct routeList and dstIpList successfully.
@rvalue FAILED							| destruct failed.
@comm
	destruct the route list and dstIp List, and free the relative memory.
@devnote
*/
int32 rtl8651_domainAdvRoute_destroyAllRouteList(void)
{
	int32 retval = FAILED;

	rtlglue_drvMutexLock();
	retval = _rtl8651_domainAdvRoute_destroyAllRouteList();
	rtlglue_drvMutexUnlock();
	return retval;
}
int32 _rtl8651_domainAdvRoute_destroyAllRouteList(void)
{
	/*
		delete all the route information and move routeList and dstIpList
		to routeList_freeList and dstIpList_freeList
	*/
	_rtl8651_domainAdvRoute_reInit();

	/*
		free the routeList_freeList and dstIpList_freeList
	*/
	_rtl8651_domainAdvRouteDstIpList_freeList 	= NULL;
	_rtl8651_domainAdvRoute_routeList_freeList	= NULL;
	MEM_FREE(_rtl8651_domainAdvRouteDstIpList_freeListBackUp);
	MEM_FREE(_rtl8651_domainAdvRoute_routeList_freeListBackUp);

	DNS_DOMAINROUTE_INFO("all route list and dstIp list freed\n");
	return SUCCESS;
}
/* end  code*/



