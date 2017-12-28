/* ===========================================================================

	GLUE-device module.

	This module would create one ( or several ) pseudo device(s).
	Each of them would
		1. Register to Rome Driver as the specific extension port / vlan.
		2. Rx packet from Rome Driver if Rome Driver has packet sent to it.
		3. Tx packet to Rome Driver if system TX via this device.

	All of them are as the "entities" in Linux for other devices to
	co-work with RTL865X Rome Driver.

   ========================================================================== */


/*	@doc RTL865X_EXTGLUEDEV_API
 *
 *	@module rtl865x_glueDev.c - RTL8651 Home gateway controller Extension GLUE device API documentation       |
 *	This document explains the API interface of the LINUX glue device module for RTL8651 Rome Driver
 *	Extension device mechanism support.
 *	@normal Yi-Lun Chen (chenyl@realtek.com.tw) <date>
 *
 *	Copyright <cp>2006 Realtek<tm> Semiconductor Cooperation, All Rights Reserved.
 *
 *	@head3 List of Symbols |
 *	Here is a list of all External APIs in this module.
 *
 *	@index | RTL865X_EXTGLUEDEV_API
 */

#include "rtl865x/rtl_types.h"
#include "rtl865x/asicRegs.h"
#include <linux/config.h>

#ifndef CONFIG_RTL865X
#error "Glue device is unnecessary if Realtek RTL865X is disabled."
#endif

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/compiler.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/pci.h>

#include "rtl865x_glueDev.h"
#include "rtl865x/rtl8651_layer2fwd.h"


/* =================================================
	OS dependent declaration
   ================================================= */

/* =================================================
	Internal used Variables/Functions' declaration
   ================================================= */
#define RTL865X_GLUEDEV_DECALARATIONs

/* ================= Internal data structure declaration ================= */

#define RTL865X_GLUEDEV_STATUS_UNREGISTERED		0x00
#define RTL865X_GLUEDEV_STATUS_REGISTERED		0x01

typedef struct _rtl865x_glueDev_info_s
{
	uint32	status;
	uint32	glueDevId;									/* Control ID for GLUE device */

	/* Rome Driver GLUE information */
	uint32	defaultVid;
	uint32	extPortNumber;

	/* Network Device GLUE information */
	struct net_device *	dev;
	char				devName[RTL865XB_GLUEDEV_MAXDEVNAMELEN];	/* device name of this glue device */
	ether_addr_t		macAddr;
} _rtl865x_glueDev_info_t;

typedef struct _rtl865x_glueDev_priv_s
{
	struct net_device *dev;
	struct net_device_stats devStats;

	/* Rome Driver GLUE information */
	_rtl865x_glueDev_info_t *devInfo;
	uint32 linkId;
	uint32 defaultVid;
	uint32 extPortNumber;
	uint32 extPortMask;
} _rtl865x_glueDev_priv_t;


/* ================= Internal MACRO declaration ================= */

/* ================= Internal variable declaration ================= */
_rtl865x_glueDev_info_t *rtl865x_glueDev_List = NULL;
static int32 rtl865x_glueDev_Cnt = 0;

/* ================= Internal function declaration ================= */
static _rtl865x_glueDev_info_t *_rtl865x_glueDev_findInfoByGlueDevId(uint32 glueDevId);

static int rtl865x_glueDev_open(struct net_device *dev);
static int rtl865x_glueDev_close(struct net_device *dev);
static void rtl865x_glueDev_set_rx_mode(struct net_device *dev);
static int rtl865x_glueDev_start_xmit(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *rtl865x_glueDev_get_status(struct net_device *dev);
static int rtl865x_glueDev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

/* =================================================
	OS dependent functions implemetation
   ================================================= */
#define RTL865X_GLUEDEV_OSDEP_FUNCs
/*
 * @func int32	| rtl865x_glueDev_init	| Initiate Glue Device module and register device information into it.
 * @parm rtl865x_gluedev_para_t*	| para	| Array of device informations to register into Glue Device module.
 * @rvalue	SUCCESS			| System initiation and device registration are success.
 * @rvalue	FAILED			| Error occurred when system initiating or device registration. Turning debugging message for GLUEDEV_ERR/GLUEDEV_INFO is suggested.
 * @comm
 * The parameter <p para> is necessary for <p rtl865x_glueDev_init()>.
 * And it is an ARRAY to depict the information of each glue device which system want to use.
 * Note that this ARRAY is ended by <p invalid> entry.
 *
 * For example, if user want to register 2 different glue devices to connect LINUX protocol stack and RTL865x Rome Driver.
 * And the registration information is as below:
 *
 *
 * DeviceID  VlanIdInDriver  PortNoInDriver  DevNameInLinux MAC
 * =========================================================================
 * 1         9               6               glue-1-        00:00:00:00:00:01
 * 2         8               8               glue-2-        00:00:00:00:00:02
 *
 * Then we could use the following <p para> to depict it:
 *
 *
 * rtl865x_gluedev_para_t glueDevPara[] =
 * {
 *	 {TRUE, 1, 9, 6, "glue-1-", {octet:{0x00,0x00,0x00,0x00,0x00,0x01}} },
 *	 {TRUE, 2, 8, 8, "glue-2-", {octet:{0x00,0x00,0x00,0x00,0x00,0x02}} },
 *	 {FALSE,0, 0, 0, ""       , {octet:{0x00,0x00,0x00,0x00,0x00,0x00}} }
 * };
 *
 * Some issues would be awared:
 * 1. The field <p glueDevId> is the unique number assigned by user and would be used in further device access.
 * 2. Third entry with <p valid> being FALSE is invalid entry to indicate the entry is the last entry of ARRAY.
 * 3. The VLAN ID, Port NO would related to Rome Driver's setting.
 * 4. The device Name can NOT exceed 8 chars due to LINUX's constraint.
 *
 */ 
int32 rtl865x_glueDev_init(rtl865x_gluedev_para_t *para)
{
	int32 retval;
	rtl865x_gluedev_para_t *paraPtr;
	int32 glueDevCnt;
	int32 idx;

	retval = FAILED;
	glueDevCnt = 0;
	paraPtr = NULL;

	GLUEDEV_INFO("Start glue device initiation.");

	/* Init Global variables */
	rtl865x_glueDev_List = NULL;
	rtl865x_glueDev_Cnt = 0;

	/* Parameter checking */
	if (para == NULL)
	{
		GLUEDEV_INFO("Parameter is [%p]. Ignore setting.", para);
		goto okout;
	}

	/* calculate how many Glue device would be created and check the correctness of each entry */
	paraPtr = para;
	while (paraPtr->valid == TRUE)
	{
		/* check entry */
		if (strlen(paraPtr->devName) == 0)
		{
			GLUEDEV_ERR("NULL device name.");
			goto out;
		}
		/* Add count */
		glueDevCnt ++;
		paraPtr ++;
	}

	GLUEDEV_INFO("Total Glue DEVICE count [%d]", glueDevCnt);
	if (glueDevCnt == 0)
	{
		GLUEDEV_INFO("No");
		goto okout;
	}

	/*
		Check the redundancy of GLUE device. We use the "ineffective" algorithm (O(n^2)) to
		do the checking because we assume there is no too many devices be created.
	*/
	paraPtr = para;
	while (paraPtr->valid == TRUE)
	{
		rtl865x_gluedev_para_t *paraCmpPtr;

		/* trace list to find the redundancy device ID for current ""paraPtr */
		paraCmpPtr = para;
		while (paraCmpPtr->valid == TRUE)
		{
			/* Same entry : ignore */
			if (paraCmpPtr == paraPtr)
			{
				goto nextCmp;
			}

			if (paraCmpPtr->glueDevId == paraPtr->glueDevId)
			{
				GLUEDEV_ERR("Redundant GLUE-Device ID [%d]. Exit!", paraCmpPtr->glueDevId);
				goto out;
			}
nextCmp:
			paraCmpPtr ++;
		}
		/* to next */
		paraPtr ++;
	}

	/* allocate memory to store the configuration */
	rtl865x_glueDev_List = rtlglue_malloc(sizeof(_rtl865x_glueDev_info_t) * glueDevCnt);
	if (rtl865x_glueDev_List == NULL)
	{
		GLUEDEV_ERR("Memory allcation failed. Exit.");
		goto out;
	}

	/* Init Global variables */
	memset(rtl865x_glueDev_List, 0, sizeof(_rtl865x_glueDev_info_t) * glueDevCnt);
	rtl865x_glueDev_Cnt = glueDevCnt;

	/* set parameter */
	for ( idx = 0 ; idx < rtl865x_glueDev_Cnt ; idx ++ )
	{
		rtl865x_glueDev_List[idx].defaultVid		= para[idx].defaultVid;
		rtl865x_glueDev_List[idx].extPortNumber		= para[idx].extPortNumber;
		rtl865x_glueDev_List[idx].glueDevId			= para[idx].glueDevId;
		rtl865x_glueDev_List[idx].dev				= NULL;
		rtl865x_glueDev_List[idx].status			= RTL865X_GLUEDEV_STATUS_UNREGISTERED;
		memcpy(	rtl865x_glueDev_List[idx].devName,
				para[idx].devName,
				sizeof(rtl865x_glueDev_List[idx].devName));
		memcpy(	&(rtl865x_glueDev_List[idx].macAddr),
				&(para[idx].macAddr),
				sizeof(rtl865x_glueDev_List[idx].macAddr));
	}

okout:
	retval = SUCCESS;
out:
	GLUEDEV_INFO("Glue device initiation done: return value [%d].", retval);

	return retval;
}

#define DEVNAME_BUFFSIZE		8
/*
 * @func int32	| rtl865x_glueDev_enable	| Enable specific Glue Device in LINUX.
 * @parm uint32	| glueDevId			| Glue Device ID of device to enable. This ID is assigned by user in <rtl865x_glueDev_init()>
 * @rvalue	SUCCESS			| Glue Device enabling is ok.
 * @rvalue	FAILED			| Error occurred when Glue Device enabling.
 * @comm
 * This API is used to enable specific Glue Device referred by <p glueDevId>.
 * Glue Device module would register this glue device into LINUX kernel when this API is called.
 */
int32 rtl865x_glueDev_enable(uint32 glueDevId)
{
	_rtl865x_glueDev_info_t *devInfo;
	struct net_device		*dev;
	_rtl865x_glueDev_priv_t	*devp;
	char					devName[RTL865XB_GLUEDEV_MAXDEVNAMELEN + DEVNAME_BUFFSIZE];
	int32 retval;

	GLUEDEV_INFO("Eanble Glue device [%d].", glueDevId);

	retval = FAILED;
	devInfo = _rtl865x_glueDev_findInfoByGlueDevId(glueDevId);
	memset(devName, 0, sizeof(devName));

	if (devInfo == NULL)
	{
		GLUEDEV_ERR("Unknown GLUE device ID [%d].", glueDevId);
		goto out;
	}

	if (devInfo->status == RTL865X_GLUEDEV_STATUS_REGISTERED)
	{
		GLUEDEV_INFO("The Device [%d] has been registered. Stop registration.", glueDevId);
		goto okout;
	}

	if (sizeof(devName) - strlen(devInfo->devName) <= DEVNAME_BUFFSIZE)
	{
		GLUEDEV_ERR("Device NAME buffer (%d) can not store device NAME <%s>.", sizeof(devName), devInfo->devName);
		goto out;
	}

	strncpy(devName, devInfo->devName, strlen(devInfo->devName));
	strcat(devName, "%d");

	/* register GLUE device */
	dev = alloc_netdev(sizeof(_rtl865x_glueDev_priv_t), devName, ether_setup);
	if (dev == NULL)
	{
		GLUEDEV_ERR("Glue Device [%d] Allocation ERROR", glueDevId);
		goto out;
	}

	SET_MODULE_OWNER(dev);

	/* Set information in GLUE device */
	if (sizeof(devInfo->macAddr) > sizeof(dev->dev_addr))
	{
		GLUEDEV_ERR(	"Glue Device [%d]'s MAC address size (%d) > system configured (%d)",
						glueDevId,
						sizeof(devInfo->macAddr),
						sizeof(dev->dev_addr));
		goto out;
	}

	memset(dev->dev_addr, 0, sizeof(dev->dev_addr));
	memcpy((void*)(dev->dev_addr), &(devInfo->macAddr), sizeof(devInfo->macAddr));

	/* Set GLUE device CALL back function / features */
	dev->open = rtl865x_glueDev_open;
	dev->stop = rtl865x_glueDev_close;
	dev->set_multicast_list = rtl865x_glueDev_set_rx_mode;
	dev->hard_start_xmit = rtl865x_glueDev_start_xmit;
	dev->get_stats = rtl865x_glueDev_get_status;
	dev->do_ioctl = rtl865x_glueDev_ioctl;
	dev->hard_header_cache = NULL;

	retval = register_netdev(dev);
	if (retval != 0)
	{
		GLUEDEV_ERR("Glue Device [%d] Registration ERROR", glueDevId);
		goto freedev_out;
	}

	/* registration success - update status and store information in private field */
	devp = dev->priv;

	devp->dev = dev;
	memset(&(devp->devStats), 0, sizeof(devp->devStats));

	devp->devInfo = devInfo;
	devp->defaultVid = devInfo->defaultVid;
	devp->extPortNumber = devInfo->extPortNumber;
	devp->extPortMask = (1 << devInfo->extPortNumber);
	devp->linkId = 0;

	devInfo->dev = dev;
	devInfo->status = RTL865X_GLUEDEV_STATUS_REGISTERED;

okout:
	retval = SUCCESS;
	goto out;

freedev_out:
	kfree(dev);

out:
	GLUEDEV_INFO("Glue device [%d] enable process done: return value [%d].", glueDevId, retval);

	return retval;
}
/*
 * @func int32	| rtl865x_glueDev_disable	| Disable specific Glue Device from LINUX.
 * @parm uint32	| glueDevId			| Glue Device ID of device to disable. This ID is assigned by user in <rtl865x_glueDev_init()>
 * @rvalue	SUCCESS			| Glue Device disabling is ok.
 * @rvalue	FAILED			| Error occurred when Glue Device disabling.
 * @comm
 * This API is used to disable specific Glue Device referred by <p glueDevId>.
 * Glue Device module would unregister this glue device into LINUX kernel when this API is called.
*/
int32 rtl865x_glueDev_disable(uint32 glueDevId)
{
	_rtl865x_glueDev_info_t *devInfo;
	struct net_device		*dev;
	int32 retval;

	GLUEDEV_INFO("Disable Glue device [%d].", glueDevId);

	retval = FAILED;
	devInfo = _rtl865x_glueDev_findInfoByGlueDevId(glueDevId);

	if (devInfo == NULL)
	{
		GLUEDEV_ERR("Unknown GLUE device ID [%d].", glueDevId);
		goto out;
	}

	if (devInfo->status == RTL865X_GLUEDEV_STATUS_UNREGISTERED)
	{
		GLUEDEV_INFO("The Device [%d] does not be registered. Stop de-registration.", glueDevId);
		goto okout;
	}

	/* disable dev and de-register it */
	dev = devInfo->dev;

	unregister_netdev(dev);
	rtlglue_free(dev);

	devInfo->dev = NULL;
	devInfo->status = RTL865X_GLUEDEV_STATUS_UNREGISTERED;
	

okout:
	retval = SUCCESS;
out:
	GLUEDEV_INFO("Glue device [%d] disable process done: return value [%d].", glueDevId, retval);

	return retval;
}

/*
 * @func void	| rtl865x_glueDev_exit	| Destructor of Glue Device Module.
 * @comm
 * This API is used to destruct Glue Device module.
 * All registered Glue devices would be unregistrred, and all related data structure would be freeed.
 */
void rtl865x_glueDev_exit(void)
{
	int idx;

	if (rtl865x_glueDev_List)
	{
		for ( idx = 0 ; idx < rtl865x_glueDev_Cnt ; idx ++ )
		{
			if (rtl865x_glueDev_List[idx].status == RTL865X_GLUEDEV_STATUS_REGISTERED)
			{
				if (rtl865x_glueDev_disable(rtl865x_glueDev_List[idx].glueDevId) != SUCCESS)
				{
					GLUEDEV_ERR("FATAL ! Unexpected situation! - registered entry but can not be disabled!");
				}
			}
		}

		/* Free allocated memory */
		rtlglue_free(rtl865x_glueDev_List);
	}
	rtl865x_glueDev_Cnt = 0;

}

static _rtl865x_glueDev_info_t *_rtl865x_glueDev_findInfoByGlueDevId(uint32 glueDevId)
{
	int32 idx;

	if (	(rtl865x_glueDev_Cnt == 0) ||
			(rtl865x_glueDev_List == NULL))
	{
		return NULL;
	}

	for ( idx = 0 ; idx < rtl865x_glueDev_Cnt ; idx ++ )
	{
		if (rtl865x_glueDev_List[idx].glueDevId == glueDevId)
		{
			return &(rtl865x_glueDev_List[idx]);
		}
	}
	return NULL;
}

/* =================================================
	Glue Device processing functions implemetation
   ================================================= */
#define RTL865X_GLUEDEV_PROC_FUNCs

static int rtl865x_glueDev_open(struct net_device *dev)
{
	int retval;
	_rtl865x_glueDev_priv_t *devp;
	_rtl865x_glueDev_info_t *devInfo;

	retval = -1;
	devp = dev->priv;
	devInfo = devp->devInfo;

	GLUEDEV_INFO("Activate Glue device [%s].", dev->name);

	GLUEDEV_INFO(	"Register: Glue device [%s] < Glue ID %d VID %d ExtPortNum %d >",
					dev->name,
					devInfo->glueDevId,
					devp->defaultVid,
					devp->extPortNumber
					);

	/* register extension device */
	if ((retval = devglue_regExtDevice(	dev->name,
										devp->defaultVid,
										devp->extPortNumber,
										&(devp->linkId))) != SUCCESS)
	{
		GLUEDEV_ERR(	"Register Glue device [%s] < Glue ID %d VID %d ExtPortNum %d > return %d.",
						dev->name,
						devInfo->glueDevId,
						devp->defaultVid,
						devp->extPortNumber,
						retval
						);
		goto out;
	}

	GLUEDEV_INFO(	"Register: OK - Link ID: %d", devp->linkId);

	/* Start Network interface */
	netif_start_queue(dev);

	retval = 0;
out:
	GLUEDEV_INFO("Glue device [%s] activation done: return value [%d].", dev->name, retval)

	return retval;
}

static int rtl865x_glueDev_close(struct net_device *dev)
{
	int retval;
	_rtl865x_glueDev_priv_t *devp;
	_rtl865x_glueDev_info_t *devInfo;
	uint32 linkId; 

	retval = -1;
	devp = dev->priv;
	devInfo = devp->devInfo;
	linkId = devp->linkId;
	devp->linkId = 0;

	GLUEDEV_INFO("Deactivate Glue device [%s].", dev->name);

	/* Stop Network interface */
	netif_stop_queue(dev);

	if (linkId > 0)
	{
		GLUEDEV_INFO(	"De-register: Glue device [%s] < Glue ID %d VID %d ExtPortNum %d LinkID %d>",
						dev->name,
						devInfo->glueDevId,
						devp->defaultVid,
						devp->extPortNumber,
						linkId
						);

		if ((retval = devglue_unregExtDevice(linkId)) != SUCCESS)
		{
			GLUEDEV_ERR(	"De-register Glue device [%s] < Glue ID %d VID %d ExtPortNum %d LinkID %d > return %d.",
							dev->name,
							devInfo->glueDevId,
							devp->defaultVid,
							devp->extPortNumber,
							linkId,
							retval
							);
			goto out;
		}
		GLUEDEV_INFO(	"De-register: OK");
	}

	retval = 0;
out:
	GLUEDEV_INFO("Glue device [%s] deactivation done: return value [%d].", dev->name, retval)

	return retval;

}

static void rtl865x_glueDev_set_rx_mode(struct net_device *dev)
{
	return;
}

static int rtl865x_glueDev_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	_rtl865x_glueDev_priv_t *devp;

	devp = dev->priv;

	/* Always kick Watchdog before packet transmission */
	REG32(WDTCNR) |= WDTCLR;

	/*
		For packet TX-ed from protocol stack -- we relay it to Rome Driver.
		For packet TX-ed from Rome Driver -- we relay it to protocol stack.			
	*/
	if (strcmp(skb->cb, RTL865XB_GLUEDEV_ROMEDRVSKBSTR) == 0)
	{
		/* Packet is from Rome Driver */
		memset(skb->cb, 0, strlen(RTL865XB_GLUEDEV_ROMEDRVSKBSTR));


		skb->protocol = eth_type_trans(skb, dev);
		skb->dev = dev;
		netif_rx(skb);
	} else
	{
		/* Packet is from protocol stack */
		if (devp->linkId == 0)
		{
			GLUEDEV_ERR(	"LinkID (%d) of Glue device [%s] is illegal. Drop TX packet",
							devp->linkId,
							dev->name
							);

			dev_kfree_skb(skb);
		} else
		{
			int32 retval;
			int32 skbLen;

			skbLen = skb->len;

			if ((retval = rtlglue_extPortRecv(	skb,
												skb->data,
												skbLen,
												devp->defaultVid,
												devp->extPortMask,
												devp->linkId)) == SUCCESS)
			{
#ifdef RTL865XB_GLUEDEV_SYS_STATISTIC
				devp->devStats.tx_packets ++;
				devp->devStats.tx_bytes += skbLen;
#endif
			} else
			{
				GLUEDEV_ERR(	"Glue device [%s] relay packet from protocol stack to Rome Driver <vid %d extPortMask 0x%x linkID %d> FAILED (return [%d]), Drop packet.",
								dev->name,
								devp->defaultVid,
								devp->extPortMask,
								devp->linkId,
								retval
								);

#ifdef RTL865XB_GLUEDEV_SYS_STATISTIC
				devp->devStats.tx_errors ++;
#endif

				dev_kfree_skb(skb);
			}
		}
	}

	/* We always return 0 to indicate the process is success */
	return 0;
}

static struct net_device_stats *rtl865x_glueDev_get_status(struct net_device *dev)
{
	_rtl865x_glueDev_priv_t *devp;

	devp = dev->priv;

	return &devp->devStats;
}

static int rtl865x_glueDev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	_rtl865x_glueDev_priv_t *devp;

	devp = dev->priv;

	return 0;
}

