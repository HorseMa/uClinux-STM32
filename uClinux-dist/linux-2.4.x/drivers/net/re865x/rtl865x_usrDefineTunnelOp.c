/*
* Copyright c                  Realtek Semiconductor Corporation, 2006
* All rights reserved.
* 
* Program : 	rtl865x_usrDefineTunnelOp.c
* Abstract : 
		user-defined tunnel: mainly for modifying the pkt's content by user defined callback function.
* Creator : hyking
* Author : hyking
*/


#include "rtl865x/rtl_types.h"
#include "rtl865x/asicRegs.h"

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/compiler.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/pci.h>

#include "rtl865x_glueDev.h"
#include "rtl865x/rtl8651_layer2fwd.h"
#include "rtl865x/rtl8651_layer2.h"

#include "rtl865x_usrDefineTunnelOp.h"
#include <asm/rtl865x/re865x.h>
#include <asm/uaccess.h>


rtl865x_usrTunnelOp usrTunnelOperation[RTL865X_USRTUNNELOPERATION_DEFAULTNUM]={{0}};
struct net_device		*usrTunnelDev = NULL;
uint32 usrTunnelLinkId = 0;
char usrTunnelDevName[]= "ut0";
uint8 wanPort = 1;


struct dev_priv {
	u32			id;            /* VLAN id, not vlan index */
	u32			portmask;     /* member port mask */
	u32			portnum;     	/* number of member ports */
	u32			netinit;
	struct net_device	*dev;
#if CP_VLAN_TAG_USED
	struct vlan_group	*vlgrp;
#endif
	u32			msg_enable;
	struct net_device_stats net_stats;
	int			ip;
	int			mask;
	int			dns;
	int			pppoe_status;
	int			gateway;
	int			setup;
};


static int rtl865x_usrDefineTunnel_open(struct net_device *dev);
static int rtl865x_usrDefineTunnel_close(struct net_device *dev);
static void rtl865x_usrDefineTunnel_set_rx_mode(struct net_device *dev);
static int rtl865x_usrDefineTunnel_start_xmit(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *rtl865x_usrDefineTunnel_get_status(struct net_device *dev);
static int rtl865x_usrDefineTunnel_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

static int32 rtl8651_test_usrTunnel_op(struct sk_buff *skb)
{	
	/*rtlglue_printf("usrTunnel: outbound...\n");*/
	return SUCCESS;
}
static int32 rtl8651_test_usrTunnel_inbound(struct sk_buff *skb)
{
	/*rtlglue_printf("usrTunnel: inbound...\n");*/
	return SUCCESS;
}


int32 rtl865x_register_Op(rtl865x_usrTunnelOp *usrDefineTunnelOp)
{
	int32 index;
	
	rtl865x_usrTunnelOp *regUsrOperation;

	regUsrOperation = NULL;
	
	for(index = 0; index < RTL865X_USRTUNNELOPERATION_DEFAULTNUM;index++)
	{
		if(usrTunnelOperation[index].valid == 1)
		{
			if(strcmp(usrTunnelOperation[index].funName, usrDefineTunnelOp->funName) == 0)
				return FAILED;
			else
				continue;
		}
		else
			regUsrOperation = &usrTunnelOperation[index];
	}

	if(regUsrOperation != NULL)
		memcpy(regUsrOperation,usrDefineTunnelOp,sizeof(rtl865x_usrTunnelOp));

	return SUCCESS;
}
int32 rtl865x_remove_Op(rtl865x_usrTunnelOp *usrDefineTunnelOp)
{
	int32 index;
	for(index = 0; index < RTL865X_USRTUNNELOPERATION_DEFAULTNUM;index++)
	{
		if(strcmp(usrTunnelOperation[index].funName,usrDefineTunnelOp->funName) == 0)
		{
			/*remove it*/
			memset(&usrTunnelOperation[index],0,sizeof(rtl865x_usrTunnelOp));
			return SUCCESS;
		}
	}
	return FAILED;
}
int32 rtl865x_usrDefineTunnel_init(void)
{
	int32 retval;
	struct dev_priv *dp;		

	retval = FAILED;

	/* register usr define tunnel device */
	usrTunnelDev = alloc_netdev(sizeof(struct dev_priv), usrTunnelDevName, ether_setup);
	if (usrTunnelDev == NULL)
	{
		rtlglue_printf("usrDefineTunnel Device: %s Allocation ERROR \n", usrTunnelDevName);
		goto out;
	}

	SET_MODULE_OWNER(usrTunnelDev);

	dp = usrTunnelDev->priv;
	memset(dp,0,sizeof(struct dev_priv));

	dp->dev = usrTunnelDev;	

	/* Set  device CALL back function / features */
	usrTunnelDev->open = rtl865x_usrDefineTunnel_open;
	usrTunnelDev->stop = rtl865x_usrDefineTunnel_close;
	usrTunnelDev->set_multicast_list = rtl865x_usrDefineTunnel_set_rx_mode;
	usrTunnelDev->hard_start_xmit = rtl865x_usrDefineTunnel_start_xmit;
	usrTunnelDev->get_stats = rtl865x_usrDefineTunnel_get_status;
	usrTunnelDev->do_ioctl = rtl865x_usrDefineTunnel_ioctl;
	usrTunnelDev->hard_header_cache = NULL;

	retval = register_netdev(usrTunnelDev);
	if (retval != 0)
	{
		rtlglue_printf("usrDefineTunnel Device:%s Registration ERROR \n", usrTunnelDevName);
		goto freedev_out;
	}

	retval = SUCCESS;
	goto out;

freedev_out:
	kfree(usrTunnelDev);

out:
	rtlglue_printf("usrDefienTunnel device: %s alloc done: return value [%d]. \n", usrTunnelDevName, retval);

	return retval;
	
}

void rtl865x_usrDefineTunnel_exit(void)
{
	rtlglue_printf("Exit usrDefineTunnel device: %s. \n", usrTunnelDevName);

	unregister_netdev(usrTunnelDev);
	rtlglue_free(usrTunnelDev);

	return;
}

static int rtl865x_usrDefineTunnel_open(struct net_device *dev)
{
	int retval;	
	retval = -1;
	rtl865x_usrTunnelOp *callBack, utFunction;
	
	
	/* register extension device */
	if ((retval = devglue_regExtDevice(	dev->name,
										RTL865X_USRTUNNEL_DEFAULT_VLAN,
										RTL865X_USRTUNNEL_DEFAULT_EXTPORT,
										&usrTunnelLinkId)) != SUCCESS)
	{
		rtlglue_printf("Register usrDefineTunnel device [%s]  VID %d ExtPortNum %d  return %d. \n",
						dev->name,
						RTL865X_USRTUNNEL_DEFAULT_VLAN,
						RTL865X_USRTUNNEL_DEFAULT_EXTPORT,
						retval
						);
		goto out;
	}	

	/*==================================================
	* regist the callback function at here!
	* the following code is an example...
	*===================================================*/

	/*example code begin...*/
	callBack = &utFunction;
	memset(callBack,0,sizeof(rtl865x_usrTunnelOp));

	callBack->direction = RTL865X_USRTUNNELOPERATION_DIR_OUTBOUND;
	strncpy(callBack->funName,"test",4);
	callBack->valid = 1;
	callBack->callBackFun = rtl8651_test_usrTunnel_op;
	retval = rtl865x_register_Op(callBack);

	memset(callBack, 0, sizeof(rtl865x_usrTunnelOp));

	callBack->direction = RTL865X_USRTUNNELOPERATION_DIR_INBOUND;
	strncpy(callBack->funName, "test2",5);
	callBack->valid = 1;
	callBack->callBackFun = rtl8651_test_usrTunnel_inbound;
	retval = rtl865x_register_Op(callBack);

	/*==================================================
	* example code end....
	*===================================================*/
	

	
	/* Start Network interface */
	netif_start_queue(dev);

	retval = 0;
out:
	rtlglue_printf("usrDefineTunnel device [%s] activation done: return value [%d].\n", dev->name, retval);

	return retval;
}

static int rtl865x_usrDefineTunnel_close(struct net_device *dev)
{
	int retval;	
	retval = -1;	
	
	/* Stop Network interface */
	netif_stop_queue(dev);

	rtl8651_delVlan(RTL865X_USRTUNNEL_ACTUAL_WAN_VLAN);

	if (usrTunnelLinkId > 0)
	{
		rtlglue_printf("Unregister: usrDefineTunnel device:%s,  LinkID %d\n",
						dev->name,						 
						usrTunnelLinkId
						);

		if ((retval = devglue_unregExtDevice(usrTunnelLinkId)) != SUCCESS)
		{
			rtlglue_printf(	"Unregister usrDefineTunnel device:%s  FAILEd, the retVale is %d.\n",
							dev->name,
							retval
							);
			goto out;
		}
		rtlglue_printf(	"Unregister: OK!\n");
	}

	retval = 0;
out:
	rtlglue_printf("Glue device [%s] deactivation done: return value [%d].", dev->name, retval);

	return retval;

}

static void rtl865x_usrDefineTunnel_set_rx_mode(struct net_device *dev)
{
	return;
}

static int rtl865x_usrDefineTunnel_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int32 retval = 0;
	int32 index;
	

	/* Always kick Watchdog before packet transmission */
	REG32(WDTCNR) |= WDTCLR;

	/*
		according to the direction, the pkt should be deal in following ways:
		outbound: 
			1) call the callback function firstly.
			2) sent the pkt to the wan port.

		inbound:
			1) call the callback function firstly.
			2) sent the pkt to rome driver.			
		
	*/	
	if (strcmp(skb->cb, RTL865XB_GLUEDEV_ROMEDRVWAN) == 0)
	{
		/*Inbound: from wan*/	
		for(index = 0; index < RTL865X_USRTUNNELOPERATION_DEFAULTNUM;index++)
		{
			if((usrTunnelOperation[index].direction & RTL865X_USRTUNNELOPERATION_DIR_INBOUND)
				&&(usrTunnelOperation[index].valid == 1)
				)
			{
				retval = usrTunnelOperation[index].callBackFun(skb);
				if(retval != SUCCESS)
				{
					rtlglue_printf("run usrDefineTunnel callbackfunction error!\n");
					dev_kfree_skb(skb);
					return FAILED;
				}
			}
		}
		/*send this pkt to fwd*/
		if (usrTunnelLinkId == 0)
		{
			rtlglue_printf("usrDefienTunnel's linkId is error!\n");
			dev_kfree_skb(skb);
		} else 
		{			
			int32 skbLen;

			skbLen = skb->len;
			if ((retval = rtlglue_extPortRecv(	skb,
												skb->data,
												skbLen,
												RTL865X_USRTUNNEL_DEFAULT_VLAN,
												(1<< RTL865X_USRTUNNEL_DEFAULT_EXTPORT),
												usrTunnelLinkId)) == SUCCESS)
			{
				/*do nothing*/				
			}
			else
			{
				rtlglue_printf(	"usrDefienTunnel device:%s relay packet to Rome Driver <vid %d extPortMask 0x%x linkID %d> FAILED (return [%d]), Drop packet.",
								dev->name,
								RTL865X_USRTUNNEL_DEFAULT_VLAN,
								(1<<RTL865X_USRTUNNEL_DEFAULT_EXTPORT),
								usrTunnelLinkId,
								retval
								);
				/*ext port receive failed, drop this pkt!*/
				dev_kfree_skb(skb);				
			}
		
		}
	}
	else if(strcmp(skb->cb, RTL865XB_GLUEDEV_ROMEDRVSKBSTR) == 0)
	{
		/*Outbound: from ps or romedriver */
		struct rtl_pktHdr 	*pktHdr;	
		struct rtl_mBuf	*Mblk;
		uint32	len = skb->len;
#ifdef CONFIG_RTL865XC	
		struct dev_priv *dp = dev->priv;
#endif
		
		/*callback function...*/
		for(index = 0; index < RTL865X_USRTUNNELOPERATION_DEFAULTNUM;index++)
		{
			if((usrTunnelOperation[index].direction & RTL865X_USRTUNNELOPERATION_DIR_OUTBOUND)
				&&(usrTunnelOperation[index].valid == 1)
				)
			{
				retval = usrTunnelOperation[index].callBackFun(skb);
				if(retval != SUCCESS)
				{
					rtlglue_printf("retvalue of usrDefineTunnel callbackfunction is not success!\n");
					/*drop this pkt*/
					dev_kfree_skb(skb);
					return FAILED;
				}
			}
		}
		Mblk=mBuf_attachHeader((void*)UNCACHE(skb->data),(uint32)(skb),2048, len,0);
		if(Mblk == NULL){
			rtlglue_printf("Tx: Can't attach mbuf!\n");
			dev_kfree_skb(skb);
			return FAILED;
		}
		
		pktHdr=Mblk->m_pkthdr;
		pktHdr->ph_portlist = wanPort;
		pktHdr->ph_len = len;
		
		/*l34 checksum has calculated, switchcore don't do it!*/
		pktHdr->ph_flags = 0x8800;		
		/*dvid*/
		pktHdr->ph_vlanIdx= rtl8651_vlanTableIndex(RTL865X_USRTUNNEL_DEFAULT_VLAN);
#ifdef CONFIG_RTL865XC
		pktHdr->ph_svlanId = dp->id;
#endif
		pktHdr->ph_portlist&= ~0x40; /*clear CPU port.*/
		
		/*send this pkt to wan port*/		
		swNic_write((void*)pktHdr);
	}
	else
	{
		dev_kfree_skb(skb);
	}

	/* We always return 0 to indicate the process is success */
	return SUCCESS;
}

static struct net_device_stats *rtl865x_usrDefineTunnel_get_status(struct net_device *dev)
{
	struct dev_priv *devp;

	devp = dev->priv;

	return &devp->net_stats;
}

static int rtl865x_usrDefineTunnel_addVlan(int vid,
							int port,
							int mac_addr)
{
	uint16 fid=0,i;		

	wanPort = port;
	rtl8651_addVlan(vid);

	rtl8651_assignVlanMacAddress(vid,(ether_addr_t *)mac_addr,1);

	rtl8651_specifyVlanFilterDatabase(vid,fid);
	for (i = 0; i < 9; i++)
	{
		if(port & (1 << i))
		{
			rtl8651_addVlanPortMember(vid, i);
			rtl8651_setPvid(i, vid);
		}
	}
	return SUCCESS;
	
}

static int rtl865x_usrDefineTunnel_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	unsigned long args[4];
	unsigned long *data;

	if (cmd != SIOCDEVPRIVATE)
	{
		rtlglue_printf("unknow msg!\n");
	}

	data = (unsigned long *)rq->ifr_data;

	if (copy_from_user(args, data, 4*sizeof(unsigned long)))
	{
		return -EFAULT;
	}

	switch (args[0])
	{
			/*hyking: set vlan promiscuous*/
		case RTL8651_IOCTL_SETVLANPROMISCUOUS:
			rtl8651_setVlanPromiscuous((uint16)args[1] , (int8)args[2]);			
			break;
		case ADD_VLAN:
			rtlglue_printf("vid = %d, port = %d,\n", (int)args[1],(int)args[2]);
			rtl865x_usrDefineTunnel_addVlan((int)args[1],(int)args[2],(int)args[3]);			
			break;			
	}
	return 0;
}

