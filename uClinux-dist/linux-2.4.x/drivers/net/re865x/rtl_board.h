/*
 * Copyright c                Realtek Semiconductor Corporation, 2003
 * All rights reserved.
 * 
 * $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/Attic/rtl_board.h,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $
 *
 * $Author: davidm $
 *
 * Abstract: rtl_board.h -- board config init implementations
 *
 * $Log: rtl_board.h,v $
 * Revision 1.1.2.1  2007/09/28 14:42:22  davidm
 * #12420
 *
 * Pull in all the appropriate networking files for the RTL8650,  try to
 * clean it up a best as possible so that the files for the build are not
 * all over the tree.  Still lots of crazy dependencies in there though.
 *
 * Revision 1.20  2007/01/12 03:30:29  chihhsing
 * +: add Multiple BSSID access policy feature
 *
 * Revision 1.19  2007/01/05 09:27:29  hf_shi
 * add enable bit for radvd dnsv6 dhcpv6
 *
 * Revision 1.18  2007/01/03 07:04:13  alva_zhang
 * *: refine for Save/Restore GW-Settings
 *
 * Revision 1.17  2007/01/02 10:08:43  alva_zhang
 * +:  Add GW Setting Save/Restore Feature
 *
 * Revision 1.16  2006/12/27 04:28:39  cary_lee
 * 6to4
 *
 * Revision 1.15  2006/12/18 08:59:42  hf_shi
 * web control for dnsv6 dhcpv6 and radvd
 *
 * Revision 1.14  2006/12/15 10:09:05  chihhsing
 * +: Add Multiple BSSID(for 8185B) support
 *
 * Revision 1.13  2006/11/28 02:34:17  hyking_liu
 * *:Modify function related with usrDefineTunnel
 *
 * Revision 1.12  2006/11/17 01:33:48  qy_wang
 * *:replace the magic number with macro
 *
 * Revision 1.11  2006/10/24 06:05:41  cary_lee
 * IPv6:address, tunnel and tunnel array, route and route table
 *
 * Revision 1.10  2006/10/20 03:46:41  chihhsing
 * +: add repeaterssid for wireless configuration
 *
 * Revision 1.9  2006/09/27 07:55:32  bo_zhao
 * *:replace numeric value by a macro
 *
 * Revision 1.8  2006/09/26 06:52:38  bo_zhao
 * *:add ip-mask for pptpserver
 *
 * Revision 1.7  2006/09/11 09:06:32  bo_zhao
 * *:support pptpserver
 *
 * Revision 1.6  2006/08/23 09:52:14  chihhsing
 * *: add "RateRange" to struct qosCfgParam_s for QoS configuration
 *
 * Revision 1.5  2006/08/11 02:50:48  alva_zhang
 * *: fix dhcpl2tp/l2tp idle timeout issue
 *
 * Revision 1.4  2006/08/09 04:26:08  alva_zhang
 * *: add enum define for Manual hangup Event
 *
 * Revision 1.3  2006/08/07 07:15:04  jiucai_wang
 * +:add data structure for IPCOMP and AH
 *
 * Revision 1.2  2006/07/31 02:05:41  alva_zhang
 * *: refine coding style
 *
 * Revision 1.1  2006/05/19 05:59:53  chenyl
 * *: modified new SDK framework.
 *
 * Revision 1.14  2006/03/24 05:45:04  alva_zhang
 * *: rename full-cone-nat as nat-mapping
 *
 * Revision 1.13  2006/03/23 08:10:37  alva_zhang
 * +: add flash table item for full-cone nat
 *
 * Revision 1.12  2005/12/12 03:24:12  tony
 * +: new feature: support trusted user in url-filter.
 *
 * Revision 1.11  2005/12/09 06:21:28  rupert
 * +: Add IPSEC NAT Traversal Configuration
 *
 * Revision 1.10  2005/09/26 08:14:22  tony
 * +: add SNMP Daemon application.
 *
 * Revision 1.9  2005/07/29 12:42:33  chenyl
 * *: bug fix: init domain block when system init
 *
 * Revision 1.8  2005/07/29 09:44:00  chenyl
 * +: dns module in ROMEDRV
 * +: domain blocking (SDK + RomeDrv)
 *
 * Revision 1.7  2005/07/28 13:01:30  tony
 * *: bug fixed: Time Zone - in New York , add PPTP Server UI
 *
 * Revision 1.6  2005/07/06 12:02:36  tony
 * +: New Features: Gaming
 *
 * Revision 1.5  2005/06/29 05:47:07  chenyl
 * *: In configuration system, we ALWAYS store whole configurations regardless of
 * 	compile flags. (ex. PPTP/L2TP, IPSec...)
 *
 * Revision 1.4  2005/06/29 05:40:05  chenyl
 * *: set static Table ID for each item
 *
 * Revision 1.3  2005/06/29 05:22:19  chenyl
 * +: TLV based configuration setting.
 *
 * Revision 1.2  2005/05/04 02:34:24  shliu
 * *: extern modeCfgFactoryDefaultFunction
 *
 * Revision 1.1  2005/04/19 04:58:15  tony
 * +: BOA web server initial version.
 *
 * Revision 1.42  2004/12/28 09:50:48  rupert
 * +: Add IPSEC Support
 *
 * Revision 1.41  2004/11/29 07:42:16  chenyl
 * +: add Semaphore protection into tunekey when ip Up/Down events occur.
 *
 * Revision 1.40  2004/11/16 02:30:30  shliu
 * *** empty log message ***
 *
 * Revision 1.39  2004/11/01 06:09:04  cfliu
 * add CONFIG_CHANGE_RTL8185 to add wireless version info in status webpage
 *
 * Revision 1.38  2004/10/05 09:20:42  chenyl
 * +: web page to turn ON/OFF ip-multicast system
 *
 * Revision 1.37  2004/10/01 07:52:55  yjlou
 * +: add enable/disable PPPoE Passthru and IPv6 Passthru
 *
 * Revision 1.36  2004/09/03 03:00:10  chhuang
 * +: add new feature: pseudo VLAN
 *
 * Revision 1.35  2004/08/18 05:39:59  chenyl
 * +: napt special option web-based setting
 *
 * Revision 1.34  2004/07/14 13:55:59  chenyl
 * +: web page for MN queue
 *
 * Revision 1.33  2004/07/07 05:12:36  chhuang
 * +: add a new WAN type (DHCP+L2TP). But not complete yet!!
 *
 * Revision 1.32  2004/07/06 06:20:33  chhuang
 * +: add rate limit
 *
 * Revision 1.31  2004/06/08 10:57:40  cfliu
 * +: Add WLAN dual mode support. Not yet completed...
 *
 * Revision 1.30  2004/05/28 09:49:16  yjlou
 * +: support Protocol-Based NAT
 *
 * Revision 1.29  2004/05/20 08:43:55  chhuang
 * add Web Page for QoS
 *
 * Revision 1.28  2004/05/12 05:15:05  tony
 * support PPTP/L2TP in RTL865XB.
 *
 * Revision 1.27  2004/04/08 13:18:12  tony
 * add PPTP/L2TP routine for MII lookback port.
 *
 * Revision 1.26  2004/03/31 01:59:36  tony
 * add L2TP wan type UI pages.
 *
 * Revision 1.25  2004/03/03 03:43:32  tony
 * add static routing table in turnkey.
 *
 * Revision 1.24  2004/02/03 08:14:34  tony
 * add UDP Blocking web UI configuration.
 *
 * Revision 1.23  2004/01/29 09:46:34  tony
 * modify serverp_tableDriverAccess(), make it support multiple session.
 * add protocol type in server port UI.
 *
 * Revision 1.22  2004/01/08 12:13:45  tony
 * add Port Range into Server Port.
 * support Server Port for multiple session UI.
 *
 * Revision 1.21  2004/01/07 09:10:04  tony
 * add PPTP Client UI in Config Wizard.
 *
 * Revision 1.20  2003/12/19 04:33:01  tony
 * add Wireless Lan config pages: Basic Setting, Advance Setting, Security, Access Control, WDS
 *
 * Revision 1.19  2003/12/01 12:35:52  tony
 * make ALG is able to be configured by users(Web GUI).
 *
 * Revision 1.18  2003/10/24 10:25:58  tony
 * add DoS attack interactive webpage,
 * FwdEngine is able to get WAN status by rtl8651_wanStatus(0:disconnect,1:connect)
 *
 * Revision 1.17  2003/10/03 12:27:35  tony
 * add NTP time sync routine in management web page.
 *
 * Revision 1.16  2003/10/01 09:12:03  tony
 * move all the extern declare of factory deafult function to rtl_board.h
 *
 * Revision 1.15  2003/10/01 05:57:31  tony
 * add URL Filter routine
 *
 * Revision 1.14  2003/09/30 12:19:49  tony
 * arrange board_ipUpEventTableDriverAccess(), let upnp,ddns,dmz be able to start after dhcpc/pppoe started.
 *
 * Revision 1.13  2003/09/30 08:56:29  tony
 * remove newServerpCfgParam[] from flash, rename ram PPPoeCfg to ramPppoeCfgParam
 *
 * Revision 1.12  2003/09/29 12:58:34  tony
 * add DDNS start/stop routine.
 *
 * Revision 1.11  2003/09/29 05:06:27  orlando
 * add board_ipUp/DownEventTableDriverAccess:
 *
 * Revision 1.10  2003/09/25 02:15:32  orlando
 * Big Change
 *
 * Revision 1.9  2003/09/24 07:10:30  rupert
 * rearrange init sequence
 *
 * Revision 1.8  2003/09/23 11:44:33  tony
 * add UPNP daemon start/stop routine.
 *
 * Revision 1.7  2003/09/23 03:47:29  tony
 * add ddnsCfgParam,ddnsDefaultFactory,ddns_init
 *
 * Revision 1.6  2003/09/22 08:01:45  tony
 * add UPnP Web Configuration Function Routine
 *
 * Revision 1.5  2003/09/22 06:33:37  tony
 * add syslogd process start/stop by CGI.
 * add eventlog download/clear routine.
 *
 * Revision 1.4  2003/09/19 06:06:51  tony
 * *** empty log message ***
 *
 * Revision 1.3  2003/09/19 01:43:50  tony
 * add dmz routine
 *
 * Revision 1.2  2003/09/18 02:05:45  tony
 * modify pppoeCfgParam to array
 *
 * Revision 1.1.1.1  2003/08/27 06:20:15  rupert
 * uclinux initial
 *
 * Revision 1.1.1.1  2003/08/27 03:08:53  rupert
 *  initial version 
 *
 * Revision 1.13  2003/07/03 05:59:55  tony
 * add configChanged global param.
 *
 * Revision 1.12  2003/07/01 10:25:00  orlando
 * call table driver server port and acl API in board.c
 *
 * Revision 1.11  2003/07/01 03:09:06  tony
 * add aclGlobalCfgParam structure.
 * modify aclCfgParam and serverpCfgParam structure.
 *
 * Revision 1.10  2003/06/30 07:46:11  tony
 * add ACL and Server_Port structure
 *
 * Revision 1.9  2003/06/23 10:57:23  elvis
 * change include path of  rtl_types.h
 *
 * Revision 1.8  2003/06/20 12:59:45  tony
 * add dhcp client
 *
 * Revision 1.7  2003/06/16 08:08:30  tony
 * add dhcps & dns function
 *
 * Revision 1.6  2003/06/06 11:57:12  orlando
 * add pppoe cfg.
 *
 * Revision 1.5  2003/06/06 06:31:11  idayang
 * add mgmt table in cfgmgr.
 *
 * Revision 1.4  2003/06/03 10:57:05  orlando
 * add nat table in cfgmgr.
 *
 * Revision 1.3  2003/05/20 08:44:35  elvis
 * change the include path of "rtl_types.h"
 *
 * Revision 1.2  2003/05/05 05:02:45  orlando
 * make ifCfgParam[] not extern.
 *
 * Revision 1.1  2003/05/02 08:51:08  orlando
 * merge cfgmgr with old board.c/board.h.
 *
 */

#ifndef RTL_BOARD_H
#define RTL_BOARD_H

#if defined(__KERNEL__) && defined(__linux__)
	/* kernel mode */
	#include "rtl865x/rtl_types.h"
	#include "rtl865x/asicRegs.h"
	#ifdef CONFIG_RTL865XC
		#define IS_865XC() (1)
		#define IS_865XB() (0)
		#define IS_865XA() (0)
	#else
		#define IS_865XC() (0)
		#define IS_865XB() ( ( REG32( CRMR ) & 0xffff ) == 0x5788 )
		#define IS_865XA() ( !IS_865XB() )
	#endif
	#define FLASH_BASE          (IS_865XA()?0xBFC00000:(IS_865XB()?0xBE000000:0xBD000000))
	#define CRMR_ADDR          CRMR
	#define WDTCNR_ADDR        WDTCNR
#else
	/* goahead */
	#include <rtl_types.h>
	#include <linux/config.h>
	#include <asicRegs.h>
	#ifdef __uClinux__
	/* non MMU */
	#ifdef CONFIG_RTL865XC
		#define IS_865XC() (1)
		#define IS_865XB() (0)
		#define IS_865XA() (0)
	#else
		#define IS_865XC() (0)
		#define IS_865XB() ( ( REG32( CRMR ) & 0xffff ) == 0x5788 )
		#define IS_865XA() ( !IS_865XB() )
	#endif
	#define FLASH_BASE          (IS_865XA()?0xBFC00000:(IS_865XB()?0xBE000000:0xBD000000))
	#define CRMR_ADDR          CRMR
	#define WDTCNR_ADDR        WDTCNR
	#else
	/* MMU */
	#ifdef CONFIG_RTL865XC
		#define IS_865XC() (1)
		#define IS_865XB() (0)
		#define IS_865XA() (0)
	#else
		#define IS_865XC() (0)
		#define IS_865XB() (1)
		#define IS_865XA() (0)
	#endif
	extern uint32 __flash_base;
	#define FLASH_BASE          __flash_base
	#define FLASH_BASE_REAL     (IS_865XA()?0xBFC00000:(IS_865XB()?0xBE000000:0xBD000000))
	extern uint32 __crmr_addr;
	extern uint32 __wdtcnr_addr;
	#define CRMR_ADDR          __crmr_addr
	#define WDTCNR_ADDR        __wdtcnr_addr
	int rtl865x_mmap(int base,int length);
	#endif
#endif

#undef _SUPPORT_LARGE_FLASH_ /* to support single 8MB/16MB flash */


/*
	Define flash device 
*/

#define VLAN_LOAD_COUNT		8
#define MAX_PORT_NUM		9   
#define MAX_IP_INTERFACE	VLAN_LOAD_COUNT
#define MAX_ROUTE		MAX_IP_INTERFACE
#define MAX_STATIC_ARP		10
#define MAX_MGMT_USER		1
#define MAX_ACL			8
#define MAX_SERVER_PORT	        8
#define MAX_SPECIAL_AP		8
#define MAX_GAMING		8
#define MAX_PPPOE_SESSION	2
#define MAX_WLAN_CARDS		2
#define MAX_WLAN_VAP		4
#define MAX_URL_FILTER		8
#define MAX_WLAN_AC		8
#define MAX_WLAN_WDS		8
#define MAX_WLAN_CRYPT_MIX_TYPE	2		/* WPA2 mix : allow WPA/WPA2 -- 2 types */
#define	MAX_IP_STRING_LEN	48
#define MAX_ROUTING	        5
#define MAX_QOS			12
#define MAX_RATIO_QOS		10
#define MAX_PBNAT		4
#define MAX_NAT_MAPPING         6 // define for NAT Mapping
#define MAX_USER_SEMAPHORE	2
#define ROMECFGFILE         "/var/romecfg.txt"
#define ALG_QOS_TYPES		3
#define MAX_DOMAIN_BLOCKING	8
#define MAX_ALLOW_DOMAINNAME_LEN	(32+1)
#define MAX_IPSEC_SESSION 	3
#define PHY0	            0x00000001	
#define PHY1	            0x00000002
#define PHY2	            0x00000004
#define PHY3	            0x00000008
#define PHY4	            0x00000010
#define PHYMII				0x00000020
#define MAX_PHYS	        5
#define EXTPT0	   0x00000040
#define EXTPT1	   0x00000080
#define EXTPT2	   0x00000100
#define _RTL8651_EXTDEV_DEVCOUNT 16


/*define user define tunnel vlan*/
#define VLAN_USRDEFINETUNNEL	10


/* define semaphore types */
#define SEMAPHORE_IPUPDOWN		0	/* for IP Up/Down only */
#define SEMAPHORE_IPEVENT		1	/* for IP events in board.c */

/* add routing functions  constant*/
#define error_msg(fmt,args...) 
#define ENOSUPP(fmt,args...) 
#define RTACTION_ADD   1
#define RTACTION_DEL   2
#define RTACTION_HELP  3
#define RTACTION_FLUSH 4
#define RTACTION_SHOW  5
#define E_LOOKUP	-1
#define E_OPTERR	-2
#define E_SOCK	-3
#define mask_in_addr(x) (((struct sockaddr_in *)&((x).rt_genmask))->sin_addr.s_addr)
#define full_mask(x) (x)

#define ACL_SINGLE_HANDLE_ADD 1
#define ACL_SINGLE_HANDLE_DEL 2

#define SHM_PROMECFGPARAM 0x1000

/* define NAPT-mode/Router-mode/Bridge-mode constant. */
#define RTL865XB_L4GATEWAYMODE	4
#define RTL865XB_L3ROUTERMODE		3
#define RTL865XB_L2BRIDGEMODE		2

/* define Remote Management Configuration-related constant */
#define RTL865XB_MULTIPLE_PPPOE_PRIVATE_LAN		0
#define RTL865XB_MULTIPLE_PPPOE_UNNUMBER_LAN 	1

/*define max prefix supported*/
#define MAX_PREFIX_NUM 2
#define IFNAMESIZE 32
#define NAMSIZE 32
#define MAX_DNAME_SIZE 256
#define RR_MAX_NUM 4


/* ====================================================================================== 
		Data Structure of Configurations
   ====================================================================================== */

typedef struct vlanCfgParam_s
{
	uint32          vid;        /* VLAN index */
	uint32          memberPort; /* member port list */
	uint32	   enablePorts; /*Enabled Port Numbers*/
	uint32          mtu;        /* layer 3 mtu */
	uint32          valid;      /* valid */
	ether_addr_t	gmac;	    /* clone gateway mac address */
	uint8           rsv[2];     /* for 4 byte alignment */
} vlanCfgParam_t;


enum ifCfgParam_connType_e {
	IFCFGPARAM_CONNTYPE_STATIC = 0,
	IFCFGPARAM_CONNTYPE_PPPOE,
	IFCFGPARAM_CONNTYPE_DHCPC,
	IFCFGPARAM_CONNTYPE_PPPOE_UNNUMBERED,
	IFCFGPARAM_CONNTYPE_PPPOE_MULTIPLE_PPPOE,
	IFCFGPARAM_CONNTYPE_PPTP,	
	IFCFGPARAM_CONNTYPE_L2TP,	
	IFCFGPARAM_CONNTYPE_DHCPL2TP,
	IFCFGPARAM_CONNTYPE_3G,
	IFCFGPARAM_CONNTYPE_USB_ETHERNET,
};

typedef struct ifCfgParam_s	/* Interface Configuration */
{
	uint32		if_unit;	/* interface unit; 0, 1(vl0, vl1)... */
	uint8		ipAddr[4];	/* ip address */
	uint8		ipAddr1[4];	/* ip address 1: this is for multiple DMZ host spec. */
	uint8		ipAddr2[4];	/* ip address 2: this is for multiple DMZ host spec. */
	uint8		ipMask[4];	/* network mask */
	uint8		ipMask1[4];	/* network mask 1: this is for multiple DMZ host spec.  */	
	uint8		ipMask2[4];	/* network mask 2: this is for multiple DMZ host spec.  */	
	uint8		gwAddr[4];  /* default gateway address */
	uint8		dnsPrimaryAddr[4]; /* dns address */
	uint8		dnsSecondaryAddr[4]; /* dns address */
	uint8		fwdWins;		/* propagate Wins server setting to LAN */
	ipaddr_t		winsPrimaryAddr;	/* wins primary server address */
	ipaddr_t		winsSecondaryAddr;	/* wins secondary server address */
	uint32		flags;	    /* flags for interface attributes */
	uint32		valid;	    /* valid */
	uint32		connType;	/* connection type: 0->static ip, 1->pppoe, 2->dhcp */
} ifCfgParam_t;



enum pptpCfgParam_dialState_e {
	PPTPCFGPARAM_DIALSTATE_OFF=0,
	PPTPCFGPARAM_DIALSTATE_DIALED_WAITING,			
	PPTPCFGPARAM_DIALSTATE_DIALED_TRYING,
	PPTPCFGPARAM_DIALSTATE_DIALED_SUCCESS,
	PPTPCFGPARAM_DIALSTATE_DISCONNECTING
};

enum l2tpCfgParam_dialState_e {
	L2TPCFGPARAM_DIALSTATE_OFF=0,
	L2TPCFGPARAM_DIALSTATE_DIALED_WAITING,			
	L2TPCFGPARAM_DIALSTATE_DIALED_TRYING,
	L2TPCFGPARAM_DIALSTATE_DIALED_SUCCESS,
	L2TPCFGPARAM_DIALSTATE_DISCONNECTING,
	L2TPCFGPARAM_DIALSTATE_DIALED_DHCP_DISCOVER,
	L2TPCFGPARAM_DIALSTATE_DIALED_MANUAL_HANGUP,
	L2TPCFGPARAM_DIALSTATE_DIALED_IDLE_HANGUP
};

enum pppParam_authType {
	PPPPARAM_AUTHTYPE_PAP=0,
	PPPPARAM_AUTHTYPE_CHAP	
};

typedef struct pptpCfgParam_s	/* PPTP Configuration */
{
	uint8	ipAddr[4];	/* ip address */
	uint8	ipMask[4];	/* network mask */
	uint8	svAddr[4];  /* pptp server ip address */
	uint8	gwAddr[4];  /* pptp default gw ip address */
	int8		username[32];  /* pptp server username */
	int8		password[32];  /* pptp server password */
	uint32	mtu;
	
	uint8			manualHangup;        /* only meaningful for ram version, to record a manual hangup event from web ui */	
	uint8			autoReconnect; /* 1/0 */
	uint8			demand;		   /* 1/0 */	
	uint8			dialState;	   /*  only meaningful for ram version, dial state */
	
	uint32			silentTimeout; /* in seconds */	
	uint32			authType;

} pptpCfgParam_t;

typedef struct pptpServerCfgParam_s	/* PPTP Configuration */
{
	int8		username[32];  /* pptp server username */
	int8		password[32];  /* pptp server password */
	uint8	range1;	/* start ip address */
	uint8	range2;	/* end ip address */
	uint8	mapIp;	/* ipaddress mapped to lan */
	uint8 ipmask;	/* IP mask */
	uint8	status; /* 0:Stop , 1:Start */
	uint8	reserved;
} pptpServerCfgParam_t;

typedef struct l2tpCfgParam_s	/* L2TP Configuration */
{
	uint8	ipAddr[4];	/* ip address */
	uint8	ipMask[4];	/* network mask */
	uint8	svAddr[4];  /* l2tp server ip address */
	uint8	gwAddr[4];  /* l2tp default gw ip address */
	int8		username[32];  /* pppoe server username */
	int8		password[32];  /* pppoe server password */
	uint32	mtu;
	
	uint8			manualHangup;        /* only meaningful for ram version, to record a manual hangup event from web ui */	
	uint8			autoReconnect; /* 1/0 */
	uint8			demand;		   /* 1/0 */	
	uint8			dialState;	   /*  only meaningful for ram version, dial state */
	
	uint32			silentTimeout; /* in seconds */	
	uint32			authType;	

} l2tpCfgParam_t;


typedef struct routeCfgParam_s	/* Route Configuration */
{
	uint32          if_unit;	  /* interface unit; 0, 1(vl0, vl1)... */
	uint8           ipAddr[4];	  /* ip network address */
	uint8           ipMask[4];	  /* ip network mask */
	uint8           ipGateway[4]; /* gateway address */
	uint32          valid; 		  /* valid */
} routeCfgParam_t;

typedef struct arpCfgParam_s /* Static ARP Configuration */
{
	uint32          if_unit;	  /* interface unit; 0, 1(vl0, vl1)... */
	uint8           ipAddr[4];	  /* ip host address */
	uint32          port;
	uint32          valid;
	ether_addr_t	mac;
	uint8           rsv[2];       /* for 4 byte alignment */
} arpCfgParam_t;

typedef struct natCfgParam_s /* nat Configuration */
{
	uint8			enable;			/* 0: disable, 1: enable */	
	uint8			hwaccel;			/* 1:hardware acceleration,0:no hw accel */
	uint8			ipsecPassthru;	/* 1: passthru, 0: no passthru */
	uint8			pptpPassthru;	/* 1: passthru, 0: no passthru */
	uint8			l2tpPassthru;	/* 1: passthru, 0: no passthru */
	uint8			natType;
	uint8			rsv[2];			/* for 4 byte alignment */
} natCfgParam_t;

typedef struct natMappingCfgParam_s
{
    ipaddr_t IntIp;
    ipaddr_t ExtIp;
    uint8 Enabled;
    uint8 Empty;
    int16 Status;
}natMappingCfgParam_t; 

enum ppppoeCfgParam_dialState_e {
	PPPOECFGPARAM_DIALSTATE_OFF=0,
	PPPOECFGPARAM_DIALSTATE_DIALED_WAITING,			
	PPPOECFGPARAM_DIALSTATE_DIALED_TRYING,
	PPPOECFGPARAM_DIALSTATE_DIALED_SUCCESS
};

enum ppppoeCfgParam_destnetType_e {
	PPPOECFGPARAM_DESTNETTYPE_NONE=0,
	PPPOECFGPARAM_DESTNETTYPE_IP,
	PPPOECFGPARAM_DESTNETTYPE_DOMAIN,
	PPPOECFGPARAM_DESTNETTYPE_TCPPORT,
	PPPOECFGPARAM_DESTNETTYPE_UDPPORT,	
};

enum ppppoeCfgParam_lanType_e {
	PPPOECFGPARAM_LANTYPE_NAPT=0,
	PPPOECFGPARAM_LANTYPE_UNNUMBERED,
};

typedef struct pppoeCfgParam_s
{
	uint8			enable;        /* 1:used/enabled, 0:no pppoe */
	uint8			defaultSession;   	/* default pppoe session  */
	uint8			unnumbered;		/* unnumbered pppoe */
	uint8			autoReconnect; /* 1/0 */
	
	uint8			demand;		   /* 1/0 */	
	uint8			dialState;	   /*  only meaningful for ram version, dial state */
	uint16			sessionId;	   /* pppoe session id */
	
	uint32			silentTimeout; /* in seconds */	
		
	uint8           ipAddr[4];	   /* ip address, for multiple pppoe only */
	uint8           ipMask[4];	   /* network mask, for multiple pppoe only */
	uint8           gwAddr[4];     /* default gateway address, for multiple pppoe only */
	uint8           dnsAddr[4];    /* dns address, for multiple pppoe only */	
	
	uint8           localHostIp[4]; /* nat's local host ip, for pppoe/nat case only */
	
	int8			username[32];  /* pppoe server username */

	int8			password[32];  /* pppoe server password */

	int8 			acName[32];

	int8			serviceName[32];  /* pppoe service name */
	
	int8			destnet[4][32];  /* pppoe destination network route 1~4 */
	
	int8			destnetType[4];  /* pppoe destination network type 1~4 */
	
	uint8			unnumberedIpAddr[4];  /* global IP assign by ISP */
	
	uint8			unnumberedIpMask[4];  /* global Network mask assign by ISP */	
	
	uint8           svrMac[6];     /* pppoe server mac address */
    uint8           pppx; /* i am pppx (0,1,2,3,...: ppp0,ppp1,ppp2,...) */    
    uint8           unnumberedNapt;
    
	uint8           whichPppObjId; /* only meaningful for ram version, and for pppoeCfgParam[0] only, to keep track of who is the up/down pppObjId */
	uint8           manualHangup; /* only meaningful for ram version, to record a manual hangup event from web ui */	
	uint16	mtu;

	uint32	lanType;
	uint32	authType;	

} pppoeCfgParam_t;

typedef struct dhcpsCfgParam_s
{
	uint8           enable;     /* 1:used/enabled, 0:disable */
	uint8           startIp;    /* DHCP Server IP Pool Start IP */
	uint8           endIp;      /* DHCP Server IP Pool End IP */
	uint8	   rsv;     	/* for 4 byte alignment */
	uint8	   realDNSenable;	/* dhcp server provide real-DNS server ip address to dhcp client */
	uint8	   dhcpDomainFwd;	/* forward domain name got from WAN DHCP server */
	char		   domain[64];	/* domain name provided by dhcp server */
	uint32	   manualIp[2];
	uint8	   hardwareAddr[2][6];
} dhcpsCfgParam_t;

typedef struct dhcpcCfgParam_s
{
	uint32      cmacValid; 	    /* cmac valid */	
	uint8		cloneMac[6];	/* clone mac address */
	uint8       rsv[2]; 		    /* for alignment */
} dhcpcCfgParam_t;

/*IPv6 Addr*/
typedef struct addrIPv6_s
{
	int	   enable; 	/* 0:off 1:on */
	int	   prefix_len[2];
	uint16	   addrIPv6[2][8];
} addrIPv6_t;

/*IPv6 in IPv4 tunnel*/
typedef struct tunnel6in4_s
{	
	char strTunnelName[12];
	uint8 	addrRemote[4];	
	uint8	addrLocal[4];	
	uint16	tunnelAddrIPv6[8];		
	uint8	maskRemote[4];
	uint8 	maskLocal[4];
} tunnel6in4_t;

typedef struct tunnel6in4array_s
{
	int  iTunnelNum;
	tunnel6in4_t tunnel6in4[10];
}tunnel6in4array_t;

typedef struct tunnel6to4_s
{
	int  flag_6to4;	
}tunnel6to4_t;

/*IPv6 Route*/
typedef struct routeIPv6_s
{
	char	 	dev[8];
	int	   prefix_len;
	uint16	   addrIPv6[8];	
	
} routeIPv6_t;

typedef struct routeTableIPv6_s
{
	int	   iRouteNum; 	
	routeIPv6_t	   routeIPv6[10];
} routeTableIPv6_t;

/*radvd*/

struct AdvPrefix {
        uint16    		Prefix[8];
        uint8                  PrefixLen;
        int                     AdvOnLinkFlag;
        int                     AdvAutonomousFlag;
        uint32                AdvValidLifetime;
        uint32                AdvPreferredLifetime;
 
        /* Mobile IPv6 extensions */
        int                     AdvRouterAddr;
 
        /* 6to4 extensions */
        char                    if6to4[IFNAMESIZE];
        int                     enabled;
};


struct Interface {
	  char                    Name[IFNAMESIZE]; /* interface name */
        uint32                  MaxRtrAdvInterval;
        uint32                  MinRtrAdvInterval;
        uint32                  MinDelayBetweenRAs;
        uint8                    AdvManagedFlag;
        uint8                    AdvOtherConfigFlag;
        uint32                   AdvLinkMTU;
        uint32			   AdvReachableTime;
        uint32                  AdvRetransTimer;
        uint8                   AdvCurHopLimit;
        uint16                  AdvDefaultLifetime;
        char                    AdvDefaultPreference[IFNAMESIZE];
        int                       AdvSourceLLAddress;
        int                       UnicastOnly;
	 struct AdvPrefix prefix[MAX_PREFIX_NUM];
};

typedef struct radvdCfgParam_s
{
	uint8 enabled;
       /*support eth1 only*/
	struct Interface interface;
}radvdCfgParam_t;

/*dnsv6*/
struct rrResource
{
	char domainName[MAX_DNAME_SIZE];
	uint16 address[8];
};

typedef struct dnsv6CfgParam_s
{
	/*default name myrouter*/
	uint8 enabled;
	char routerName[NAMSIZE];
	struct rrResource rr[RR_MAX_NUM];
}dnsv6CfgParam_t;

/*dhcpv6*/
struct dnsOptions
{
	char dns_domain[NAMSIZE];
	uint16 dns_server[8] ;
};

struct prefixDelegation
{
	/*the interface to send IA_PD request*/
	char up_intfName[IFNAMESIZE];
	/*prefix+sla_id = new prefix*/
	int sla_id;
	/*local interface using the new prefix*/
	char local_intfName[IFNAMESIZE];
};

typedef struct dhcpv6sCfgParam_s
{
	uint8 enabled;
	/*now only support DNS options*/
	struct dnsOptions dns;
}dhcpv6sCfgParam_t;

typedef struct dhcpv6cCfgParam_s
{
	uint8 enabled;
	/*now only support prefix delagation on eth0 interface*/
	struct prefixDelegation delegation;
}dhcpv6cCfgParam_t;

typedef struct dnsCfgParam_s
{
	uint32          enable;  /* 1:used/enabled, 0:disable */
	uint8		    primary[4];  /* primary dns server ip */
	uint8		    secondary[4]; /* secondary dns server ip */
} dnsCfgParam_t;

typedef struct mgmtCfgParam_s 
{
	uint8	name[16];
	uint8	password[16];
	uint8	mailfrom[32];
	uint8	rcptto[32];
	uint8	smtpserver[32];
	uint8	logserver[32];		
	uint32	valid; 		/* valid */
	uint32	remoteMgmtIp;
	uint32	remoteMgmtMask; /* reserved */	
	uint16	remoteMgmtPort; /* remote management port */
	uint8	remoteIcmpEnable; /* remote icmp enable */
	uint8	rsv;	
} mgmtCfgParam_t;

enum aclGlobalCfgParam_e {
	ACLGLOBALCFGPARAM_ALLOWALLBUTACL = 0,
	ACLGLOBALCFGPARAM_DENYALLBUTACL,
	ACLGLOBALCFGPARAM_L34_DENYALLBUTACL,
	ACLGLOBALCFGPARAM_ALLOWALLBUTACL_LOG,	
};

typedef struct aclGlobalCfgParam_s 
{
	uint8		policy;				// 0: allow all except ACL , 1: deny all except ACL
	uint8		lanPermit;			// for sercomm spec	
	uint8		layer2Permit;            // for Permit Layer2 ACL rule
	uint8		rsv;				        // for 4 bytes alignment 
} aclGlobalCfgParam_t;

enum aclCfgParam_type_e {
	ACLCFGPARAM_PROTOCOL_TCP = 0,
	ACLCFGPARAM_PROTOCOL_UDP,
	ACLCFGPARAM_PROTOCOL_IP
};

enum aclCfgParam_direction_e {
	ACLCFGPARAM_DIRECTION_EGRESS_DEST = 0,
	ACLCFGPARAM_DIRECTION_INGRESS_DEST,
	ACLCFGPARAM_DIRECTION_EGRESS_SRC,
	ACLCFGPARAM_DIRECTION_INGRESS_SRC
};

typedef struct aclCfgParam_s 
{
	uint8		ip[4];		// IP address
	
	uint16		port;		// IP port	
	uint8		enable;		// 0: disable,  1:enable
	uint8		direction;	// 0: egress dest, 1: ingress dest, 2: egress src, 3: ingress src
	
	uint8		type;		// 0: TCP, 1: UDP
	uint8		day;		// bit0:Sunday, bit1:Monday .... bit6:Saturday
	uint8		start_hour;	
	uint8		start_minute;	
	
	uint8		end_hour;	
	uint8		end_minute;	
	uint8		rsv[2];
	
} aclCfgParam_t;



enum serverpCfgParam_e {
	SERVERPCFGPARAM_PROTOCOL_TCP = 0,
	SERVERPCFGPARAM_PROTOCOL_UDP,
};

typedef struct serverpCfgParam_s 
{
	uint8		ip[4];		// Server IP address
	uint16		wanPortStart;	// WAN Port
	uint16		wanPortFinish;	// WAN Port	
	uint16		portStart;		// Server Port
	uint8       protocol;	// SERVERPCFGPARAM_PROTOCOL_TCP or SERVERPCFGPARAM_PROTOCOL_UDP
	uint8		enable;		// enable
	
} serverpCfgParam_t;

enum specialapCfgParam_e {
	SPECIALACPCFGPARAM_PROTOCOL_TCP = 0,
	SPECIALACPCFGPARAM_PROTOCOL_UDP,
	SPECIALACPCFGPARAM_PROTOCOL_BOTH
};

#define SPECIAL_AP_MAX_IN_RANGE 80
typedef struct specialapCfgParam_s 
{
	uint8		name[16];		// name
	uint32		inType;			// incoming type
	uint8		inRange[SPECIAL_AP_MAX_IN_RANGE];	// incoming port range
	uint32		outType;		// outgoing type
	uint16		outStart;		// outgoing start port
	uint16		outFinish;		// outgoing finish port		
	uint8		enable;			// enable
	uint8		rsv[3];			// for 4 bytes alignment
} specialapCfgParam_t;


#define GAMING_MAX_RANGE 80
typedef struct gamingCfgParam_s 
{
	uint8		name[32];		// name
	uint32		ip;
	uint8		tcpRange[GAMING_MAX_RANGE];
	uint8		udpRange[GAMING_MAX_RANGE];
	uint8		enable;			// enable
	uint8		rsv[3];			// for 4 bytes alignment
} gamingCfgParam_t;

typedef struct tbldrvCfgParam_s 
{
	uint8		naptAutoAdd;		// napt auto add
	uint8       naptAutoDelete;     // napt auto delete
	uint16		naptIcmpTimeout;	// napt icmp timeout
	uint16      naptUdpTimeout;
	uint16		naptTcpLongTimeout;
	uint16      naptTcpMediumTimeout;
	uint16		naptTcpFastTimeout;
} tbldrvCfgParam_t;

typedef struct dmzCfgParam_s	/* DMZ Configuration */
{
	uint32		enable; 	/* 0:off 1:on */
	uint32		enable2;
	uint32		enable3;
	uint32		dmzIp;
	uint32		dmzIp2;
	uint32		dmzIp3;
	uint8		l4fwd;	/* forward various L4 protocol to DMZ */
	uint8		icmpfwd;	/* forward icmp packet to DMZ */
} dmzCfgParam_t;

typedef struct logCfgParam_s	/* Event Log Configuration */
{
	uint32		module;  /* each bit mapping to each module */
} logCfgParam_t;

typedef struct upnpCfgParam_s	/* UPnP Configuration */
{
	uint32		enable; 	/* 0:off 1:on */
	
} upnpCfgParam_t;

typedef struct snmpCfgParam_s	/* SNMP Daemon Configuration */
{
	uint32		enable; 	/* 0:off 1:on */
	
} snmpCfgParam_t;


typedef struct ddnsCfgParam_s	/* DDNS Configuration */
{
	uint32		enable; 	/* 0:off 1:on */
	uint8		username[32];
	uint8		password[32];
	uint8		service[16];		
	uint8		username2[32];
	uint8		password2[32];
	uint8		hostname[32];	
} ddnsCfgParam_t;

typedef struct urlfilterCfgParam_s	/* URL FILTER Configuration */
{
	uint32		enable; 	/* 0:off 1:on */
	uint8		string[32];
	uint32		ip_start;
	uint32		ip_end;
} urlfilterCfgParam_t;


typedef struct urlfilterTrustedUserCfgParam_s	/* URL FILTER Configuration */
{
	uint32		trustedIP;
	uint32		enable; 	
} urlfilterTrustedUserCfgParam_t;
#define URL_FILTER_MAX_TRUSTED_USERS 5


typedef struct timeCfgParam_s	/* Time Configuration */
{
	uint32		timeZoneIndex;
	uint8		timeZone[16];
	uint8		ntpServer1[32];
	uint8		ntpServer2[32];
	uint8		ntpServer3[32];
} timeCfgParam_t;

typedef struct dosCfgParam_s	/* DoS Configuration */
{
	uint32		enable;
	uint32		enableItem;
	uint8		ignoreLanSideCheck;
	/* connection count system */
	/* <---Whole System counter---> */
	uint16		tcpc;
	uint16		udpc;
	uint16		tcpudpc;
	/* <---Per source IP tracking---> */
	uint16		perSrc_tcpc;
	uint16		perSrc_udpc;
	uint16		perSrc_tcpudpc;
	/* flood check system */
	/* <---Whole System counter---> */
	uint16		syn;
	uint16		fin;
	uint16		udp;
	uint16		icmp;
	/* <---Per source IP tracking---> */
	uint16		perSrc_syn;
	uint16		perSrc_fin;		
	uint16		perSrc_udp;
	uint16		perSrc_icmp;
	/* <----Source IP blocking----> */
	uint16		sipblock_enable;
	uint16		sipblock_PrisonTime;
	/* <----Port Scan Level ----> */
	uint16		portScan_level;
} dosCfgParam_t;

typedef struct naptCfgParam_s	/* NAPT Option Configuration */
{
	uint8		weakNaptEnable;						/* Weak TCP mode */
	uint8 		looseUdpEnable;	/*Loose UDP mode*/
	uint8		tcpStaticPortMappingEnable;				/* TCP static port mapping mode */
	uint8		udpStaticPortMappingEnable;				/* UDP static port mapping mode */
} naptCfgParam_t;

typedef struct algCfgParam_s	/* ALG Configuration */
{
	uint32		algList;
	ipaddr_t		localIp[32];
} algCfgParam_t;


extern int totalWlanCards;
extern int is85B;


 typedef enum{
	CRYPT_NONE = 0, 
	CRYPT_WEP,
	CRYPT_WPA,
	CRYPT_WPA2,
	CRYPT_WPAMIX,
	CRYPT_MAX
}WlanCryptType;

typedef enum {
	CRYPT_SUB_NONE = 0,
	CRYPT_SUB_WEP_64,
	CRYPT_SUB_WEP_128,
	CRYPT_SUB_WPA_TKIP,
	CRYPT_SUB_WPA_AES,
	CRYPT_SUB_WPA_TKIPAES,
	CRYPT_SUB_MAX
} WlanCryptSubType;

 typedef enum{
 	AUTH_NONE=0,
	AUTH_KEY,
	AUTH_AUTO,
	AUTH_8021x, 
	AUTH_MAX
}WlanAuthType;


enum wlanDataRateModeCfgParam_e {
	WLAN_DATA_RATE_B=0,
	WLAN_DATA_RATE_G=1,
	WLAN_DATA_RATE_BG=2,
	WLAN_DATA_RATE_A=3
};


enum wlanDataRateCfgParam_e {
	WLAN_DATA_RATE_AUTO = 0,
	WLAN_DATA_RATE_54M,
	WLAN_DATA_RATE_48M,
	WLAN_DATA_RATE_36M,
	WLAN_DATA_RATE_24M,
	WLAN_DATA_RATE_18M,
	WLAN_DATA_RATE_12M,
	WLAN_DATA_RATE_9M,
	WLAN_DATA_RATE_6M,
	WLAN_DATA_RATE_11M,
	WLAN_DATA_RATE_5_5M,
	WLAN_DATA_RATE_2M,
	WLAN_DATA_RATE_1M
};

enum wlanPreambleTypeCfgParam_e {
	WLAN_PREAMBLE_TYPE_LONG = 0,
	WLAN_PREAMBLE_TYPE_SHORT = 1,
};


typedef struct wlanCfgParam_s	/* Wireless LAN Configuration */
{
	uint8	aliasName[32];
	uint8	ssid[16];

	uint8	enable;	
	uint8	channel; //0:auto
	uint8	dataRateMode; //enum  //
	uint8	highPowerChk;

	uint8 	protection;
	uint8 	rxChargePump;
	uint8 	txChargePump;
	uint8 	regDomain;
	
	uint16	dataRate; //enum	//
	uint16	frag;
	uint16	rts;
	uint16	beacon;

	uint8	preambleType; //enum
	uint8	broadcastSSID;
	uint8	range;
	uint8 	chipVer;

	uint8	authRadiusPasswd[16];

	uint8	cryptType; //enum
	uint8	cryptSubType[MAX_WLAN_CRYPT_MIX_TYPE];		/* array: for WPA mix-mode */
	uint8	authType;
	
	uint8	acEnable;
	uint8	wdsEnable; //for 4 bytes alignment

	uint8	wdsCount; //for 4 bytes alignment
	uint8	xxxReserved; //for 4 bytes alignment
	
	uint16	authRadiusPort;
	uint8	mode;
	uint8 	adhoc;

	uint32	authRadiusIp;
	
	uint8	acMacList[MAX_WLAN_AC][6];
	uint8	acComment[MAX_WLAN_AC][8];
	
	uint8	wdsMacList[MAX_WLAN_WDS][6];
	
	uint8	key[68]; //10 hex for wep64,26 hex for wep128, 64 hex for WPA-PSK

	uint8	repeaterssid[16];
 
#ifdef CONFIG_RTL8185B	
	uint8	vap_ssid[MAX_WLAN_VAP][16];	/* for Multiple BSSID */
	uint8	vap_cryptType[MAX_WLAN_VAP]; //enum
	uint8	vap_cryptSubType[MAX_WLAN_VAP][MAX_WLAN_CRYPT_MIX_TYPE];
	uint8	vap_authType[MAX_WLAN_VAP];	/* for Multiple BSSID */

	uint8	vap_authRadiusPasswd[MAX_WLAN_VAP][16];
	uint32	vap_authRadiusIp[MAX_WLAN_VAP];
	
	uint8	vap_key[MAX_WLAN_VAP][68];
	uint8	enable_vap[MAX_WLAN_VAP];

	uint16	vap_authRadiusPort[MAX_WLAN_VAP];
	
	uint8	vap_acEnable[MAX_WLAN_VAP];
	uint8	vap_acMacList[MAX_WLAN_VAP][MAX_WLAN_AC][6];
	uint8	vap_acComment[MAX_WLAN_VAP][MAX_WLAN_AC][8];
 #endif
 	//uint8 	reserved[29];
} wlanCfgParam_t;

typedef struct udpblockCfgParam_s	/* UDP Blocking Configuration */
{
	uint32		enable;
	uint32		size;
} udpblockCfgParam_t;

enum routingInterfaceCfgParam_e {
	ROUTING_INTERFACE_NONE = 0,
	ROUTING_INTERFACE_LAN,
	ROUTING_INTERFACE_WAN	
};

typedef struct routingCfgParam_s	/* Routing Table Configuration */
{
	uint32		route;
	uint32		mask;
	uint32		gateway;
	uint8		interface;
} routingCfgParam_t;

typedef struct qosCfgParam_s /* QoS Configuration */
{
	uint32		qosType;
	uint8		RateRange;
	union {
		struct {
			uint16	port;
			uint8	isHigh;
			uint8	enableFlowCtrl;
			uint32	inRL;
			uint32	outRL;
		} port_based;
		struct {
			uint16	portNumber;
			uint16	isHigh;
		} policy_based;
	} un;
} qosCfgParam_t;


typedef struct ratioAlgQosEntry_s	/* Ratio based ALG QoS entry */
{
	uint32	queueID;	/* 1 ~ max no. of queue */	
	uint8	isHigh;		/* 0: Low 1: High */
	uint8	ratio;		/* percentage */
	uint8	enable;		/* 0: disable, 1: enable */
	uint8 	rsv;			/* reserve for 32bits-alignment */
} ratioAlgQosEntry_t;

typedef struct ratioQosUpstreamCfgParam_s /* Ratio based QoS: Upstream total bandwidth Configuration */
{
	uint32	maxRate;
	uint8	maxRateUnit;		/* 0: Mbps, 1: Kbps */
	uint8	remainingRatio_h;	/* remaining ratio in high queue */
	uint8	remainingRatio_l;	/* remaining ratio in low queue */
	uint8	rsv;				/* reserve for 32bits-alignment */
	ratioAlgQosEntry_t algQosEntry[3];
	
} ratioQosUpstreamCfgParam_t;

typedef struct ratioQosEntry_s	/* Ratio based QoS entry */
{
	uint32	queueID;	/* 1 ~ max no. of queue */	
	uint32	ip;
	uint32	ipMask;
	uint16	s_port;
	uint16	e_port;
	uint8	enable;		/* 0: disable, 1: enable */
	uint8	isSrc;		/* 0: dst, 1: src */
	uint8	protoType;	/* 0: TCP, 1: UDP, 2: IP */
	uint8	isHigh;		/* 0: Low 1: High */
	uint8	ratio;		/* percentage */
	uint8	mark;		/* 0: DSCP marking disabled,  1: enabled */
	uint16	dscp;		/* DSCP value */
} ratioQosEntry_t;

typedef struct ratioQosCfgParam_s	/* Ratio based QoS Configuration */
{
	uint8 enable;							/* 0: disable, 1: enable */
	ratioQosUpstreamCfgParam_t upInfo;
	ratioQosEntry_t entry[MAX_RATIO_QOS];
} ratioQosCfgParam_t;

typedef struct pbnatCfgParam_s /* Protocol-Based NAT Configuration */
{
	uint8 protocol;
	uint8 enabled;
	ipaddr_t IntIp;
} pbnatCfgParam_t;

#define NULL_QOS						0x00
#define PORT_BASED_QOS				0x01
#define POLICY_BASED_QOS				0x02
#define ENABLE_QOS					0x04


typedef struct rateLimitEntry_s
{
	uint8		enable;		/* 0: disable, 1: enable */
	uint32		ip;	
	uint32		ipMask;
	uint16		s_port, e_port;		
	uint8		protoType;	/* 0: TCP, 1: UDP, 2: IP */
	uint8		isByteCount;	/* 0: Packet Count, 1: Byte Count */
	uint32		rate;	
	uint8		rateUnit;		/* 0: Kbps, 1: Mbps */
	uint32		maxRate;		/* Tolerance */
	uint8		maxRateUnit;	/* 0: Kbps, 1: Mbps */
	uint8		isDropLog;	/* 1: Drop & Log, 0: Drop */
	uint8		isSrcIp;
} rateLimitEntry_t;


typedef struct rateLimitCfgParam_s
{
	uint32		enable;		/* 0: disable, 1: enable */
	rateLimitEntry_t entry[16];
} rateLimitCfgParam_t;

typedef struct modeCfgParam_s
{
	uint32		Mode;
} modeCfgParam_t;


typedef struct ripCfgParam_s
{

	uint8		send_eth0;
	uint8		rcv_eth0;
	uint8		auth_eth0;
	char			pass_eth0[16];

	char			pass_eth1[16];
	uint8		send_eth1;
	uint8		rcv_eth1;
	uint8		auth_eth1;
	uint8		update_tr;
	uint8		timeout_tr;
	uint8		garbage_tr;
	uint16		resv;
} ripCfgParam_t;


typedef struct pseudoVlanCfgParam_s
{
	uint8				enable;
	uint16				portVid[9];
	uint8 				rsv[1];
} pseudoVlanCfgParam_t;

typedef struct passthruCfgParam_s	/* Passthru Configuration */
{
	uint8	enabledPppoe;
	uint8	enabledDropUnknownPppoePADT;
	uint8	enabledIpv6;
	uint8	enabledIpx;
	uint8	enabledNetbios;
} passthruCfgParam_t;

typedef struct ipMulticastCfgParam_s	/* IP Multicast Configuration */
{
	uint8	enabledIpMulticast;
} ipMulticastCfgParam_t;

typedef struct ipsecGlobalCfgParam_s	/* IP Multicast Configuration */
{
	uint8	enableNatTraversal;
} ipsecGlobalCfgParam_t;



typedef struct ipsecConfigCfgParam_s	/* Ratio based QoS Configuration */
{
	uint8 enable;							/* 0: disable, 1: enable */
	uint8 protocol;							/* 0: ESP, 1: AH*/
	uint8 encryption;
	uint8 authentication;
	uint8 keymanagement;					/* 0:automatic,  1:manual */
	uint8 ipcompress;						/* 0: no,  1: yes */
	uint8 pfs;
	uint8 dpd;								/* 0: no,  1: yes */

	uint32 localnet;
	uint32 remotegw;
	uint32 remotenet;
	uint32 ipsec_keylife;
	uint32 isakmp_keylife;
	uint8  secrets[30];
	uint8 ipsec_enckey[80];     
	uint8 ipsec_authkey[60];   
	
} ipsecConfigCfgParam_t;


typedef struct settingMgmtCfgParam_s
{
   int16   upload;
   uint8 curversion[29];
   uint8 newversion[29];
}settingMgmtCfgParam_t;

typedef struct domainBlockCfgParam_s
{
	char domain[MAX_ALLOW_DOMAINNAME_LEN];
	ipaddr_t sip;
	ipaddr_t smask;
	uint8 enable;
} domainBlockCfgParam_t;

typedef struct romeCfgParam_s
{
	vlanCfgParam_t      vlanCfgParam[VLAN_LOAD_COUNT];
	ifCfgParam_t        ifCfgParam[MAX_IP_INTERFACE];
	routeCfgParam_t     routeCfgParam[MAX_ROUTE];
	arpCfgParam_t       arpCfgParam[MAX_STATIC_ARP];
	natCfgParam_t       natCfgParam;
	pppoeCfgParam_t     pppoeCfgParam[MAX_PPPOE_SESSION];
	dhcpsCfgParam_t     dhcpsCfgParam;
	dhcpcCfgParam_t     dhcpcCfgParam;
	dnsCfgParam_t       dnsCfgParam;
	mgmtCfgParam_t      mgmtCfgParam[MAX_MGMT_USER];
	aclGlobalCfgParam_t	aclGlobalCfgParam;
	aclCfgParam_t       aclCfgParam[MAX_PPPOE_SESSION][MAX_ACL];
	serverpCfgParam_t   serverpCfgParam[MAX_PPPOE_SESSION][MAX_SERVER_PORT];
	specialapCfgParam_t specialapCfgParam[MAX_PPPOE_SESSION][MAX_SPECIAL_AP];
	gamingCfgParam_t gamingCfgParam[MAX_PPPOE_SESSION][MAX_GAMING];
    dmzCfgParam_t       dmzCfgParam[MAX_PPPOE_SESSION];
	logCfgParam_t       logCfgParam[MAX_PPPOE_SESSION];
    upnpCfgParam_t      upnpCfgParam;
    snmpCfgParam_t      snmpCfgParam;
    ddnsCfgParam_t      ddnsCfgParam;    
	tbldrvCfgParam_t    tbldrvCfgParam;
	urlfilterCfgParam_t	urlfilterCfgParam[MAX_PPPOE_SESSION][MAX_URL_FILTER];
	dosCfgParam_t		dosCfgParam[MAX_PPPOE_SESSION];
	naptCfgParam_t		naptCfgParam;
	timeCfgParam_t		timeCfgParam;	
	algCfgParam_t		algCfgParam[MAX_PPPOE_SESSION];
	wlanCfgParam_t		wlanCfgParam[MAX_WLAN_CARDS];
	udpblockCfgParam_t	udpblockCfgParam[MAX_PPPOE_SESSION];	
	routingCfgParam_t	routingCfgParam[MAX_ROUTING];
	pptpCfgParam_t		pptpCfgParam;
	l2tpCfgParam_t		l2tpCfgParam;
	qosCfgParam_t		qosCfgParam[MAX_QOS];
	rateLimitCfgParam_t	rateLimitCfgParam;
	ratioQosCfgParam_t	ratioQosCfgParam;
	pbnatCfgParam_t		pbnatCfgParam[MAX_PPPOE_SESSION][MAX_PBNAT];
	pseudoVlanCfgParam_t pseudoVlanCfgParam;
	passthruCfgParam_t	passthruCfgParam;
	ipMulticastCfgParam_t	ipMulticastCfgParam;
	modeCfgParam_t		modeCfgParam;
	ripCfgParam_t			ripCfgParam;
	uint8				semaphore[MAX_USER_SEMAPHORE];	/* used by user level */
	ipsecConfigCfgParam_t   ipsecConfigCfgParam[3];
	ipsecGlobalCfgParam_t   ipsecGlobalCfgParam;
	domainBlockCfgParam_t	domainBlockCfgParam[MAX_DOMAIN_BLOCKING];
	pptpServerCfgParam_t   pptpServerCfgParam;
	urlfilterTrustedUserCfgParam_t urlfilterTrustedUserCfgParam[URL_FILTER_MAX_TRUSTED_USERS];
	natMappingCfgParam_t   natMappingCfgParam[MAX_NAT_MAPPING];      
	addrIPv6_t		  addrIPv6Param;
	tunnel6in4_t		  tunnel6in4Param;
	tunnel6in4array_t		tunnel6in4arrayParam;
	tunnel6to4_t			tunnel6to4Param;	
	routeIPv6_t		routeIPv6Param;
	routeTableIPv6_t 	routeTableIPv6Param;
	radvdCfgParam_t   radvdCfgParam;
	dnsv6CfgParam_t   dnsv6CfgParam;
	dhcpv6sCfgParam_t  dhcpv6sCfgParam;
	dhcpv6cCfgParam_t  dhcpv6cCfgParam;
	settingMgmtCfgParam_t  settingMgmtCfgParam;
} romeCfgParam_t;


/* ================================================================================
		Global used variables
   ================================================================================ */
extern romeCfgParam_t*		pRomeCfgParam;
extern serverpCfgParam_t	ramServerpCfgParam[MAX_PPPOE_SESSION][MAX_SERVER_PORT];
extern int					totalWlanCards;
extern int					is85B;
extern uint32				mgmt_udp_500;


#define CONFIG_CHANGE_FW_VER	1
#define CONFIG_CHANGE_LD_VER	(1<<1)
#define CONFIG_CHANGE_NAT		(1<<2)
#define CONFIG_CHANGE_HW_ACCEL	(1<<3)
#define CONFIG_CHANGE_LAN_IP	(1<<4)
#define CONFIG_CHANGE_LAN_MASK	(1<<5)
#define CONFIG_CHANGE_LAN_MAC	(1<<6)
#define CONFIG_CHANGE_DHCPS		(1<<7)
#define CONFIG_CHANGE_CONN_TYPE	(1<<8)
#define CONFIG_CHANGE_WAN_IP	(1<<9)
#define CONFIG_CHANGE_WAN_MASK	(1<<10)
#define CONFIG_CHANGE_WAN_GW	(1<<11)
#define CONFIG_CHANGE_WAN_DNS	(1<<12)
#define CONFIG_CHANGE_WAN_MAC	(1<<13)
#define CONFIG_CHANGE_RTL8185		(1<<14)

#endif /* RTL_BOARD_H */
