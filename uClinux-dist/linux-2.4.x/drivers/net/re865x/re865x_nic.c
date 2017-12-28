/*
 * Copyright c                Realtek Semiconductor Corporation, 2003
 * All rights reserved.                                                    
 * 
 * $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/Attic/re865x_nic.c,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $
 *
 * $Author: davidm $
 *
 * Abstract: Pure L2 NIC driver, without RTL865X's advanced L3/4 features.
 *
 *   re865x_nic.c: NIC driver for the RealTek 865* 
 *
 */

#define DRV_RELDATE		"Mar 25, 2004"
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/rtl865x/interrupt.h>
#include <asm/rtl865x/re865x.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include "version.h"

#include "rtl865x/rtl_types.h"
#include "rtl865x/rtl_queue.h"
#include "rtl865x/mbuf.h"
#include "rtl865x/asicRegs.h"
#include "rtl865x/rtl8651_tblAsicDrv.h"
#include "rtl865x/rtl8651_tblDrvPatch.h"
#include "rtl865x/rtl8651_tblDrv.h"
#include "rtl865x/rtl8651_tblDrvProto.h"
#include "swNic2.h"
#ifdef CONFIG_RTL8305S
#include "rtl865x/rtl8305s.h"
#endif
/* These identify the driver base version and may not be removed. */
MODULE_DESCRIPTION("RealTek RTL-8650 series 10/100 Ethernet driver");
MODULE_LICENSE("GPL");

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC.  */
MODULE_PARM (multicast_filter_limit, "i");
MODULE_PARM_DESC (multicast_filter_limit, "maximum number of filtered multicast addresses");

#define DRV_NAME		"re865x_nic"
#define PFX			DRV_NAME ": "
#define DRV_VERSION		"0.1"
#define TX_TIMEOUT		(10*HZ)
#define BDINFO_ADDR 0xbfc04000
#define assert(expr) \
        if((!expr)) {					\
        rtlglue_printf( "Return Failed %d ,%s,%s,line=%d\n",	\
        (u32)expr,__FILE__,__FUNCTION__,__LINE__);		\
        }

spinlock_t *rtl865xSpinlock;
struct tasklet_struct	*rtl8651RxTasklet;

 struct re865x_priv
{
	u16			ready;
	u16			addIF;
	u16			devnum;
	u32			sec_count;
	u32			sec;
	struct	net_device	*dev[RTL8651_VLAN_NUMBER];
	spinlock_t		lock;
	void			*regs;
	struct tasklet_struct	rx_tasklet;
	struct timer_list timer;	/* Media monitoring timer. */
	unsigned long		linkchg;	
};

__DRAM_FWD static  struct re865x_priv _rtl86xx_dev; 


struct dev_priv {
	u32			id;            /* VLAN id, not vlan index */
	u32			portmask;     /* member port mask */
	u32			portnum;     	/* number of member ports */
	u32			netinit;
	struct net_device	*dev;
	//struct re865x_priv	*priv;
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
//	struct mii_if_info	mii_if;
};


#define PKT_BUF_SZ		2048 /* Size of each temporary Rx buffer.*/
#define FREQ	 	10
static int re865x_ioctl (struct net_device *dev, struct ifreq *rq, int cmd);
static void re865x_interrupt (int irq, void *dev_instance, struct pt_regs *regs);

static inline void skb_cpy(u16 *dst, void *src,int pktlen)
{
	u32 i=0,k;
	for (i=0;i<=pktlen;i+=4)
	{
		k=*(u32*)src;
		dst[0]=(u16)(k>>16)&0xffff;
		dst[1]=(u16)(k)&0xffff;
		src+=4;
		dst+=2;
	}
}

/* Set or clear the multicast filter for this adaptor.
 * This routine is not state sensitive and need not be SMP locked.
 */
static void re865x_set_rx_mode (struct net_device *dev){
	unsigned long flags;
	spin_lock_irqsave (&_rtl86xx_dev.lock, flags);
	//Not yet implemented.
	spin_unlock_irqrestore (&_rtl86xx_dev.lock, flags);
}

static struct net_device_stats *re865x_get_stats(struct net_device *dev){
	struct dev_priv  *dp = dev->priv;
	return &dp->net_stats;
}

#if 0
static void re865x_timer(unsigned long data){
	spin_lock_irq(&_rtl86xx_dev.lock);
	if (jiffies-_rtl86xx_dev.sec_count >=HZ)	{
		//Nothing to do now...
		_rtl86xx_dev.sec_count = jiffies;
	}

	spin_unlock_irq(&_rtl86xx_dev.lock);
	_rtl86xx_dev.timer.expires = jiffies + FREQ;
	add_timer(&_rtl86xx_dev.timer);
}
#endif

static inline void re865x_rx_skb (struct dev_priv *dp , struct sk_buff *skb)
{
	//memDump(skb->data, 64,"");
	skb->protocol = eth_type_trans (skb, dp->dev);
	dp->net_stats.rx_packets++;
	dp->net_stats.rx_bytes += skb->len;
	dp->dev->last_rx = jiffies;

#if CP_VLAN_TAG_USED
	if (dp->vlgrp && (desc->opts2 & RxVlanTagged)) {
		vlan_hwaccel_rx(skb, dp->vlgrp, desc->opts2 & 0xffff);
	} else
#endif
	netif_rx(skb);
}



static int32 rtl865x_toProtocolStack(struct rtl_pktHdr *pPkt,struct net_device *dev){
	struct dev_priv *dp= (struct dev_priv *)dev->priv;	
	struct sk_buff *new_skb;
	struct sk_buff *old_skb;
	uint16 pktLen=pPkt->ph_len;		
	struct rtl_mBuf *m=pPkt->ph_mbuf;
	unsigned char *dataPtr=m->m_data;

	new_skb = dev_alloc_skb (PKT_BUF_SZ);
	if(!new_skb)
		return FAILED;

#ifdef SWNIC_RX_ALIGNED_IPHDR
#define IS_865XB() ( ( REG32( CRMR ) & 0xffff ) == 0x5788 )
#define IS_865XA() ( !IS_865XB() )
	if(IS_865XB())
	{
		/*For 8650B, 8651B, chip can do Rx at any position, no copy required*/
		old_skb=(struct sk_buff *)m->m_extClusterId;
		old_skb->dev = dev;
		old_skb->data = old_skb->tail = old_skb->head ;
		skb_reserve(old_skb, 16+mBuf_leadingSpace(m));
		old_skb->len=0;
		skb_put(old_skb, pktLen);
		//refill Rx ring use new buffer
		mBuf_attachCluster(m, (void *)UNCACHE(new_skb->data), (uint32)new_skb, 2048, 0, 0);
		mBuf_freeMbufChain(m);
		//send to protocol stack...
		re865x_rx_skb((struct dev_priv*)dp, old_skb);
	}else
#endif
	{
		/*For 8650,8651, chip can only do 4 byte aligned Rx
		 which makes IP hdr starts at 2 byte aligned position. 
		 However, Linux protocol stack requires IP hdr aligned 
		 to 4 byte position so we do a copy here*/
		new_skb->dev = dev;
		skb_reserve(new_skb, 2);	// for alignment issue
		skb_cpy((u16*)new_skb->data, dataPtr, pktLen);
		skb_put(new_skb, pktLen);
		mBuf_freeMbufChain(pPkt->ph_mbuf);
		re865x_rx_skb((struct dev_priv*)dp, new_skb);
	}
	return SUCCESS;
}


static int  rtl865x_rx(struct rtl_pktHdr* pPkt){
	struct rtl_mBuf *m=pPkt->ph_mbuf;
	struct dev_priv *dp;
	struct net_device *dev;

	dev = _rtl86xx_dev.dev[pPkt->ph_vlanIdx];
	assert(dev);
	dp=(struct dev_priv *)dev->priv;
	//send original packet to protocol stack and allocate a new
	//one to refill NIC Rx ring.
	if(FAILED==rtl865x_toProtocolStack(pPkt, dev)){
		assert(dp);
		dp->net_stats.rx_dropped++;
		rtlglue_printf("dropped %d.\n",pPkt->ph_rxdesc);
		mBuf_freeMbufChain(m);//drop the packet....reclaim pkthdr and mbuf only.
	}
	return 0;
}


void re865x_interrupt(int32 irq, void *dev_instance, struct pt_regs *regs){
	if(swNic_intHandler(NULL))
		tasklet_schedule(rtl8651RxTasklet);
}

static int re865x_open (struct net_device *dev)
{
	struct dev_priv *dp = dev->priv;
	if (netif_msg_ifup(dp))
		rtlglue_printf(KERN_DEBUG "%s: enabling interface\n", dev->name);
	if (_rtl86xx_dev.ready)
		netif_start_queue(dev);
	return 0;
}

static int re865x_close (struct net_device *dev)
{
	struct dev_priv *dp = dev->priv;
	if (netif_msg_ifdown(dp))
		rtlglue_printf(KERN_DEBUG "%s: disabling interface\n", dev->name);
	netif_stop_queue(dev);
	return 0;
}

static int re865x_start_xmit (struct sk_buff *skb, struct net_device *dev){
	struct dev_priv		*dp = dev->priv;
	struct rtl_pktHdr 	*pktHdr;	
	struct rtl_mBuf		*Mblk;
	uint32	len = skb->len;//orlando

	if(!dp->portnum){
		rtlglue_printf("Tx: No port num!\n");
		return -1;
	}
	Mblk=mBuf_attachHeader((void*)UNCACHE(skb->data),(uint32)(skb),2048, len,0);
	if(!Mblk){
		rtlglue_printf("Tx: Can't attach mbuf!\n");
		return -1;
	}
	pktHdr=Mblk->m_pkthdr;

	pktHdr->ph_portlist = dp->portmask & rtl8651_asicL2DAlookup(skb->data);
	pktHdr->ph_len = len;
	pktHdr->ph_flags = 0x8800;
	pktHdr->ph_vlanIdx= rtl8651_vlanTableIndex(dp->id);
#ifdef CONFIG_RTL865XC
	pktHdr->ph_svlanId = dp->id;
#endif
	pktHdr->ph_portlist&= ~0x40; //clear CPU port.
	//if portmask contains MII port. MII port must be configured before we can send pkt to it.
	if(pktHdr->ph_portlist & RTL8651_MII_PORTMASK && miiPhyAddress<0)
		pktHdr->ph_portlist&=~RTL8651_MII_PORTMASK;

	rtlglue_drvSend((void*)pktHdr);
	dev->trans_start = jiffies;
	return 0;				
}


static void re865x_tx_timeout (struct net_device *dev)
{
	rtlglue_printf("Tx Timeout!!! Can't send packet\n");
}





static int re865x_ethtool_ioctl (struct dev_priv *dp, void *useraddr)
{
	u32 ethcmd;

	/* dev_ioctl() in ../../net/core/dev.c has already checked
	   capable(CAP_NET_ADMIN), so don't bother with that here.  */

	if (get_user(ethcmd, (u32 *)useraddr))
		return -EFAULT;

	switch (ethcmd) {

	case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };
		strcpy (info.driver, DRV_NAME);
		strcpy (info.version, DRV_VERSION);
		if (copy_to_user (useraddr, &info, sizeof (info)))
			return -EFAULT;
		return 0;
	}

	/* get settings */
	case ETHTOOL_GSET: {
		struct ethtool_cmd ecmd = { ETHTOOL_GSET };
		spin_lock_irq(&_rtl86xx_dev.lock);
		spin_unlock_irq(&_rtl86xx_dev.lock);
		if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
			return -EFAULT;
		return 0;
	}
	/* set settings */
	case ETHTOOL_SSET: {
		struct ethtool_cmd ecmd;
		if (copy_from_user(&ecmd, useraddr, sizeof(ecmd)))
			return -EFAULT;
		spin_lock_irq(&_rtl86xx_dev.lock);
		spin_unlock_irq(&_rtl86xx_dev.lock);
		return 0;
	}
	/* restart autonegotiation */
	/* get link status */
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = {ETHTOOL_GLINK};
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}

	/* get message-level */
	case ETHTOOL_GMSGLVL: {
		struct ethtool_value edata = {ETHTOOL_GMSGLVL};
		edata.data = dp->msg_enable;
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}
	/* set message-level */
	case ETHTOOL_SMSGLVL: {
		struct ethtool_value edata;
		if (copy_from_user(&edata, useraddr, sizeof(edata)))
			return -EFAULT;
		dp->msg_enable = edata.data;
		return 0;
	}

	default:
		break;
	} /* end switch */

	return -EOPNOTSUPP;
}

 int re865x_ioctl (struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct dev_priv *dp = dev->priv;
	int32 rc = 0;
	if (!netif_running(dev))
		return -EINVAL;
	switch (cmd)
        {
	    case SIOCETHTOOL:
		return re865x_ethtool_ioctl(dp, (void *) rq->ifr_data);
	    default:
		rc = -EOPNOTSUPP;
		break;
	}
	return rc;
}



#define DRV_INIT_CHECK(expr) do {\
	if((expr)!=SUCCESS){\
		rtlglue_printf("Error >>> initialize failed at line %d!!!\n", __LINE__);\
			return FAILED;\
	}\
}while(0)


struct  init_vlan_setting {
	uint16 vid;
	int32 portmask;
} vlanSetting[]={
	{8,  0x1 },
	{9,  0x1e }
};

#define MAX_PORT_NUM 9

static int re86xx_add_vlan(struct net_device *dev,int id,int port,char * mac_addr)
{
	struct dev_priv *dp = dev->priv;
	u16 fid=0,i;
	int rc;
	char *mac=(char*)mac_addr;

	dp->id = id;
	if(port==0){
		rtlglue_printf("No member port on vlan %d\n", id);
		return -1;
	}		
	dp->portmask =  port;
	dp->portnum  =0;
	for(i=0;i<RTL8651_AGGREGATOR_NUMBER;i++){
		if(port & (1<<i))
			dp->portnum++;
	}		
	_rtl86xx_dev.dev[id&0x7]=dev;
	if ((rc = rtl8651_addVlan(dp->id))!=SUCCESS){
		rtlglue_printf("add vlan %d fail. err=%d\n",id, rc);
		return -1;
	}
	rc=rtl8651_assignVlanMacAddress(dp->id,(ether_addr_t*)mac,1);
	if(rc!=SUCCESS){
		rtlglue_printf("assign MAC for vlan %d fail. err=%d\n",id, rc);
		return -1;
	}
	memcpy((char*)dev->dev_addr,(char*)mac,6);

	rc=rtl8651_specifyVlanFilterDatabase(dp->id, fid);
	if(rc!=SUCCESS){
		rtlglue_printf("assign FDB for vlan %d fail.  err=%d\n",id, rc);
		return -1;
	}

	for(i=0; i<MAX_PORT_NUM; i++)
	{
		if(dp->portmask& (1<<i))	{
			rc=rtl8651_addVlanPortMember(dp->id, i);
			if(rc!=SUCCESS){
				rtlglue_printf("assign port member %d for vlan %d fail. err=%d\n",i, id, rc);
				return -1;
			}			
			rc=rtl8651_setPvid(i, dp->id);
			if(rc!=SUCCESS){
				rtlglue_printf("assign PVID for vlan %d fail. err=%d\n",id, rc);
				return -1;
			}			
			rc=rtl8651_setEthernetPortAutoNegotiation(i,1,RTL8651_ETHER_AUTO_100FULL);
			if(rc!=SUCCESS){
				rtlglue_printf("assign autoneg for port %d fail. err=%d\n",i, rc);
				return -1;
			}
			rc=rtl8651_setEthernetPortDuplexMode(i,1);			
			if(rc!=SUCCESS){
				rtlglue_printf("assign duplex mode for port %d fail. err=%d\n",i, rc);
				return -1;
			}			
		}
	}
	return 0;
}

static void re865x_timer(unsigned long data)
{
	/* reset watchdog timer. Reboot when 2^18 watchdog cycles reached */
	REG32(WDTCNR) |= WDTCLR;

	spin_lock_irq(&_rtl86xx_dev.lock);

	#if defined(CONFIG_RTL865X_INIT_BUTTON)	
	if ((init_button_event==1) && (init_button_count>0)){
		init_button_count--;
		//rtlglue_printf("\n%d",init_button_count);
		if (init_button_count==0){
			sys_kill(rtl8651_user_pid,50);
 		}
	}
	#endif /* CONFIG_RTL865X_INIT_BUTTON */
	#if defined(CONFIG_RTL865X_DIAG_LED)	
	if (ledCtrlFreq){
		ledCtrlTick++;/* kick the heartbeat */
		if ((ledCtrlTick%5)==0){
			if (ledCtrlPattern[ledCtrlTick2]){
				DIAG_LED_ON();
			}else{
				DIAG_LED_OFF();
			}
			ledCtrlTick2++;/* kick the hearbeat */
			ledCtrlTick2 = ledCtrlTick2 % (ledCtrlPatternIndexLimit[ledCtrlFreq]);			
		}
	}
	#endif /* CONFIG_RTL865X_DIAG_LED */
	spin_unlock_irq(&_rtl86xx_dev.lock);
	_rtl86xx_dev.timer.expires = jiffies + FREQ;
	add_timer(&_rtl86xx_dev.timer); //schedule next timer event
}



int  __init re865x_probe (void)
{
	int32 i, rc,totalVlans=sizeof(vlanSetting)/sizeof(struct init_vlan_setting);
	struct	re865x_priv *rp;
	ether_addr_t mac;
	rtlglue_printf("Probing RTL8651 10/100 NIC...\n");
    	REG32(CPUIIMR) = 0x00;
    	REG32(CPUICR) &= ~(TXCMD | RXCMD);
	
	DRV_INIT_CHECK(rtl8651_initDrvParam(NULL));
	//Initial ASIC table
	DRV_INIT_CHECK(rtl8651_initAsic());

	DRV_INIT_CHECK(rtl8651_layer2_alloc());
	DRV_INIT_CHECK(rtl8651_layer2_init());
	DRV_INIT_CHECK(rtl8651_installPortStatusChangeNotifier(NULL));
	DRV_INIT_CHECK(rtl8651_installFDBEntryChangeNotifier(NULL));
	DRV_INIT_CHECK(rtl8651_addSpanningTreeInstance(0));
	DRV_INIT_CHECK(rtl8651_setSpanningTreeInstanceProtocolWorking(0, FALSE));
	DRV_INIT_CHECK(rtl8651_addFilterDatabase(0));
	DRV_INIT_CHECK(rtl8651_specifyFilterDatabaseSpanningTreeInstance(0, 0));
	mac.octet[0] = 0xFF;mac.octet[1] = 0xFF;mac.octet[2] = 0xFF;
	mac.octet[3] = 0xFF;mac.octet[4] = 0xFF;mac.octet[5] = 0xFF;
	rtl8651_addFilterDatabaseEntry(0, &mac,RTL8651_FORWARD_MAC,0xFFFFFFFF);

	//init PHY LED style
	#if defined(CONFIG_RTL865X_BICOLOR_LED)
		#ifdef BICOLOR_LED_VENDOR_BXXX
			REG32(LEDCR) |= (1 << 19); // 5 ledmode set to 1 for bi-color LED
			REG32(PABCNR) &= ~0x001f0000; /* set port port b-4/3/2/1/0 to gpio */
			REG32(PABDIR) |=  0x001f0000; /* set port port b-4/3/2/1/0 gpio direction-output */
		#else
			//8650B demo board default: Bi-color 5 LED
			WRITE_MEM32(LEDCR, READ_MEM32(LEDCR) | 0x01180000 ); // bi-color LED
		#endif
		
		/* config LED mode */
		WRITE_MEM32(SWTAA, PORT5_PHY_CONTROL);
		WRITE_MEM32(TCR0, 0x000002C2); //8651 demo board default: 15 LED boards
		WRITE_MEM32(SWTACR, CMD_FORCE | ACTION_START); // force add
#ifndef RTL865X_TEST
		while ( (READ_MEM32(SWTACR) & ACTION_MASK) != ACTION_DONE ); /* Wait for command done */
#endif /* RTL865X_TEST */

	#else /* CONFIG_RTL865X_BICOLOR_LED */

		/* config LED mode */
		WRITE_MEM32(LEDCR, 0x00000000 ); // 15 LED
		WRITE_MEM32(SWTAA, PORT5_PHY_CONTROL);
		WRITE_MEM32(TCR0, 0x000002C7); //8651 demo board default: 15 LED boards
		WRITE_MEM32(SWTACR, CMD_FORCE | ACTION_START); // force add
#ifndef RTL865X_TEST
		while ( (READ_MEM32(SWTACR) & ACTION_MASK) != ACTION_DONE ); /* Wait for command done */
#endif /* RTL865X_TEST */

	#endif /* CONFIG_RTL865X_BICOLOR_LED */
	

	rp = &_rtl86xx_dev;
	memset(rp,0,sizeof(struct re865x_priv));	
	synchronize_irq();
	udelay(10);
	spin_lock_init (&_rtl86xx_dev.lock);
	rtl865xSpinlock = &_rtl86xx_dev.lock;
	rtlglue_init();
	DRV_INIT_CHECK(mBuf_init(128,  0, 128, PKT_BUF_SZ, 0));
	DRV_INIT_CHECK(swNic_init(64,64,16,PKT_BUF_SZ, rtl865x_rx ,NULL));

	memcpy((void*)&mac,(void*)BDINFO_ADDR,sizeof(macaddr_t));

	//create all default VLANs
	rtlglue_printf("   creating eth0~eth%d...\n",totalVlans-1 );
	for(i=0;i<totalVlans;i++){
		struct net_device *dev;
		struct dev_priv	  *dp;
		int rc;
		rp = &_rtl86xx_dev;
		dev = alloc_etherdev(sizeof(struct dev_priv));
		if (!dev){
			rtlglue_printf("failed to allocate dev %d", i);
			return -1;
		}
		SET_MODULE_OWNER(dev);
		dp = dev->priv;
		memset(dp,0,sizeof(*dp));
		dp->dev = dev;
		mac.octet[5]++;
		DRV_INIT_CHECK(re86xx_add_vlan(dev, vlanSetting[i].vid, vlanSetting[i].portmask,(char *)&mac));
		dev->open = re865x_open;
		dev->stop = re865x_close;
		dev->set_multicast_list = re865x_set_rx_mode;
		dev->hard_start_xmit = re865x_start_xmit;
		dev->get_stats = re865x_get_stats;
		dev->do_ioctl = re865x_ioctl;
		dev->tx_timeout = re865x_tx_timeout;
		dev->watchdog_timeo = TX_TIMEOUT;
#if 0 
		dev->features |= NETIF_F_SG | NETIF_F_IP_CSUM;
#endif
#if CP_VLAN_TAG_USED
		dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
		dev->vlan_rx_register = cp_vlan_rx_register;
		dev->vlan_rx_kill_vid = cp_vlan_rx_kill_vid;
#endif
		dev->irq = ICU_NIC;
		rc = register_netdev(dev);
		if(!rc){
			_rtl86xx_dev.dev[i]=dev;
			rtlglue_printf("eth%d added. vid=%d Member port 0x%x...\n", i,vlanSetting[i].vid ,vlanSetting[i].portmask );			
		}else
			rtlglue_printf("Failed to allocate eth%d\n", i);

	}
#ifdef CONFIG_RTL8305S
	rtl8305s_init();
#endif
	rtl8651RxTasklet=&_rtl86xx_dev.rx_tasklet;
	rc = request_irq(ICU_NIC, re865x_interrupt, SA_INTERRUPT, _rtl86xx_dev.dev[0]->name, &_rtl86xx_dev.dev[0]);
	if(rc)
		rtlglue_printf("Failed to allocate irq %d ...\n", ICU_NIC);
	else
		rtlglue_printf("IRQ %d allocated for 8650 NIC\n", ICU_NIC);
	_rtl86xx_dev.sec_count=jiffies;
	_rtl86xx_dev.ready=1;
	init_timer(&_rtl86xx_dev.timer);
	_rtl86xx_dev.timer.expires = jiffies + 3*HZ;
	_rtl86xx_dev.timer.data = (unsigned long)0;
	_rtl86xx_dev.timer.function = &re865x_timer;/* timer handler */
	add_timer(&_rtl86xx_dev.timer);

	tasklet_init(&_rtl86xx_dev.rx_tasklet, swNic_rxThread, (unsigned long)0);
	return 0;
}

static void __exit re865x_exit (void){
	return;
}


module_init(re865x_probe);
module_exit(re865x_exit);





