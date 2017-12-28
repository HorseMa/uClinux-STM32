/*
 * linux/deriver/net/skyeye_ne2k.c
 * Ethernet driver for SkyEye NE2k
 * Copyright (C) 2003 Yu Chen &Souyi Ying 
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>       // printk()
#include <linux/slab.h>		// kmalloc()
#include <linux/errno.h>	// error codes
#include <linux/types.h>	// size_t
#include <linux/interrupt.h>	// mark_bh

#include <linux/in.h>
#include <linux/netdevice.h>    // net_device
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>

#include <asm/irq.h>
#include "skyeyene2k.h"

//chy 2003-02-17 the read* write* define is in /arch/armnommu/mach-atmel/io.[ch]
/*
#define __arch_getb(a)		(*(volatile unsigned char *)(a))
#define __arch_getl(a)		(*(volatile unsigned int  *)(a))
#define __arch_putb(v,a)	(*(volatile unsigned char *)(a) = (v))
#define __arch_putl(v,a)	(*(volatile unsigned int  *)(a) = (v))
*/

#define inb(a) (*(volatile unsigned char *)(a))
#define inw(a) (*(volatile unsigned int  *)(a))
#define inl(a) (*(volatile unsigned int  *)(a))
#define outb(v,a) (*(volatile unsigned char *)(a) = (v))
#define outw(v,a) (*(volatile unsigned int  *)(a) = (v))
#define outl(v,a) (*(volatile unsigned int  *)(a) = (v))

void insb(volatile unsigned char * p,volatile unsigned char *d,unsigned int l) 
{
   while(l--) { 
   	*(volatile unsigned char *)d++ = inb((p)); 
   }
}

void outsb(volatile unsigned char * p,volatile unsigned char *d,unsigned int l) 
{
   while(l--) { 
         outb(*(volatile unsigned char *)d++,p);
   }
}



#undef DEBUG
#ifdef	DEBUG
#define TRACE(str, args...)	printk("skyeye ne2k eth: " str, ## args)
#else
#define TRACE(str, args...)
#endif

void sene2k_interrupt(int irq, void *dev_id, struct pt_regs *regs);

static int timeout = 100;	// tx watchdog ticks 100 = 1s
static char *version = "SkyEye NE2k Ethernet driver version 0.2 (2003-04-27) \n";

/*
 * This structure is private to each device. It is used to pass
 * packets in and out, so there is place for a packet
 */
struct sene2k_priv {
	struct net_device_stats stats;
	
	spinlock_t lock;

	struct sk_buff *skb;
};



/*
 * Pull the specified number of bytes from the device DMA port,
 * and throw them away.
 */
static void sene2k_discard(u16 count)
{
  u8 tmp;
	while(count--) {
		tmp = inb(NE_DMA);
	}
}
					

/*
 * rx 
 */
u8 ethpkt[1600];
//void sene2k_rx(struct net_device *dev_id
void sene2k_rx(void *dev_id)
{
//	int i;
	
	int len, packetLength;

	//unsigned char *data;
	struct sk_buff *skb;
	u8 PDHeader[4];



	struct net_device *dev = (struct net_device *) dev_id;
	struct sene2k_priv *priv = (struct sene2k_priv *) dev->priv;
	TRACE("rx\n");

	spin_lock(&priv->lock);

	outb(ISR_RDC, NE_ISR);
	outb(0x0f, NE_RBCR1); 	/* See controller manual , use send packet command */
	outb(CMD_PAGE0 | CMD_SEND | CMD_RUN, NE_CR);
	
	insb(NE_DMA, PDHeader, 4);

	packetLength = ((unsigned) PDHeader[2] | (PDHeader[3] << 8 )); // real length of ethernet packet
	len = packetLength + 4;  // length of 8019as frame,size of e8390_pkt_hdr is 4
	
	//chy 2003-05-25
        //verify if the packet is an IP packet or ARP packet
	if(PDHeader[3]>0x06)
	{
	     sene2k_discard(packetLength);
	     priv->stats.rx_dropped++;
	     spin_unlock(&priv->lock);
	     return ;
	}  
	//---------------------------------------
	// the status octet handler should be added in the future
	//chy 2003-02-18
	//memcpy(ethpkt, PDHeader + 4, 14);
	//insb(NE_DMA, ethpkt+14, packetLength-14);  // ethpkt is the 8019as frame
	
	skb = dev_alloc_skb(packetLength + 2);
	if (skb != NULL) 
	{
		skb->dev = dev;
		skb_reserve(skb, 2);     /* Align IP on 16 byte */
	        skb_put(skb,packetLength);
		insb(NE_DMA, skb->data,packetLength);
		//eth_copy_and_sum(skb, ethpkt, packetLength, 0);
		
		skb->protocol = eth_type_trans(skb,dev);
		priv->stats.rx_packets++;
		priv->stats.rx_bytes += packetLength;
		netif_rx(skb);
		
	}else{  //chy 2003-02-18
		//sene2k_discard(packetLength-14);
		//chy 2003-05-25
		sene2k_discard(packetLength);
	        priv->stats.rx_dropped++;
	}
	
	spin_unlock(&priv->lock);
	return;
}

/*
 * tx *** not completed, it is not necessary 
 */

//void sene2k_tx(struct net_device *dev)
void sene2k_tx(void *dev_id)
{
	//struct FrameDesc *FD_ptr;
	//unsigned long CFD_ptr;
	//unsigned long *FB_ptr;
	//unsigned long status;
	struct net_device *dev = (struct net_device *) dev_id;
	struct sene2k_priv *priv = (struct sene2k_priv *) dev->priv;
	TRACE("tx\n");

	spin_lock(&priv->lock);

	
	spin_unlock(&priv->lock);
	return;
}

/*
 * Open and Close
 */
int sene2k_open(struct net_device *dev)
{
	int i;
	unsigned char tmpb;
	//unsigned long status;
	MOD_INC_USE_COUNT;

	TRACE("open\n");

	// Disable net irq
	disable_irq(AT91_NET_IRQNUM);
	

	// register net isr
	if(request_irq(AT91_NET_IRQNUM, &sene2k_interrupt, SA_INTERRUPT, "sene2k isr", dev)) {
		printk(KERN_ERR "sene2k: Can't get irq %d\n", AT91_NET_IRQNUM);
		return -EAGAIN;
	}

	//setup the hardware
	
	outb(0x22,NE_CR);
	/*
	 * Initialize physical device
	 */
	//write and read 0x1f to reset the nic
	outb(0, NE_RESET);
	tmpb=inb(NE_RESET);            
	for(i=0; i < 20; i++);		//delay at least 10ms


	//in PAGE0
	outb(CMD_PAGE0 | CMD_NODMA | CMD_STOP, NE_CR);
	/* FIFO threshold = 8 bytes, normal operation, 8-bit byte write DMA, BOS=0 */
	outb(DCR_LS | DCR_FIFO8, NE_DCR);
	
	outb(0, NE_RBCR0);
	outb(0, NE_RBCR1);

/*
 * Allow broadcast packets, in addition to packets unicast to us.
 * Multicast packets that match MAR filter bits will also be
 * allowed in.
 */
	outb(RCR_AB, NE_RCR);

	//Place the SNIC in LOOPBACK mode 1 or 2 (Transmit Configuration Register e 02H or 04H)
	outb(TCR_LOOP_INT, NE_TCR);

	outb(XMIT_START >> 8, NE_TPSR);
	outb(RECV_START >> 8, NE_PSTART);
	outb(RECV_START >> 8, NE_BNRY);
	outb(RECV_STOP >> 8, NE_PSTOP);
	//in PAGE1
	outb(CMD_PAGE1 | CMD_NODMA | CMD_STOP, NE_CR);
	outb(RECV_START >> 8, NE_CURR);

      
/*
 * Set physical address here.(not use 93c46)
 */
        /*
	outb(dev->dev_addr[0], NE_PAR0);
	outb(dev->dev_addr[1], NE_PAR1);
	outb(dev->dev_addr[2], NE_PAR2);
	outb(dev->dev_addr[3], NE_PAR3);
	outb(dev->dev_addr[4], NE_PAR4);
	outb(dev->dev_addr[5], NE_PAR5);
        */
	
	//chy 2003-04-27: get physical address from skyeye
	outb(CMD_PAGE1 | CMD_NODMA | CMD_STOP, NE_CR);
	dev->dev_addr[0]=inb(NE_PAR0);
	dev->dev_addr[1]=inb(NE_PAR1);
	dev->dev_addr[2]=inb(NE_PAR2);
	dev->dev_addr[3]=inb(NE_PAR3);
	dev->dev_addr[4]=inb(NE_PAR4);
	dev->dev_addr[5]=inb(NE_PAR5);
        /* for test
        printk("sene2k open: ");
	for(i = 0; i < 6; i++)
		printk("%2.2x%c", dev->dev_addr[i], (i==5) ? ' ' : ':');
	printk("\n");
	*/
	
	//chy 2003-02-18: 
        //select PAGE0 and start the nic
		outb(CMD_PAGE0 | CMD_NODMA | CMD_RUN, NE_CR);
	//set Interrupt mask reg
	outb(ISR_OVW | ISR_TXE | ISR_PTX | ISR_PRX, NE_IMR);
	
	outb(TCR_LOOP_NONE, NE_TCR);
	
    // Enable net irq
	enable_irq(AT91_NET_IRQNUM);
	// Start the transmit queue
	netif_start_queue(dev);
	
	return 0;
}

int sene2k_stop(struct net_device *dev)
{
	TRACE("stop\n");
	
	free_irq(AT91_NET_IRQNUM, dev);
	
	netif_stop_queue(dev);
	MOD_DEC_USE_COUNT;
	
	return 0;
}


// skyeye net interrupt handler

void sene2k_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct sene2k_priv *priv = (struct sene2k_priv *) dev->priv;
	u8 isr, curr, bnry;

	if (dev == NULL) {
		printk ("sene2k_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}
	
	//chy 2003-02-16
	/*
	if (dev->interrupt)
		printk("%s: Re-entering the interrupt handler.\n", dev->name);
	dev->interrupt = 1;
	*/

	//close nic
	outb(CMD_PAGE0 | CMD_NODMA | CMD_STOP,NE_CR);
	
	//in PAGE0
	isr = inb(NE_ISR);

	// ram overflow interrupt
	if (isr & ISR_OVW) {
		outb(ISR_OVW,NE_ISR);		// clear interrupt
	//	ne2k_overflowProcess();              //yangye :no overflow now 
	// chy 2003-05-27: // set BNRY=CURR
		outb(CMD_PAGE1 | CMD_NODMA | CMD_STOP,NE_CR);
		curr = inb(NE_CURR);
		outb(CMD_PAGE0 | CMD_NODMA | CMD_STOP,NE_CR);
		outb(curr, NE_BNRY);
		priv->stats.rx_over_errors++;
		priv->stats.rx_errors++;
		//printk(KERN_ERR "sene2k overflow !\n");
	}
	
	// error transfer interrupt ,NIC abort tx due to excessive collisions	
	if (isr & ISR_TXE) {
		outb(ISR_TXE,NE_ISR);		// clear interrupt
		priv->stats.tx_errors++;
	 	//temporarily do nothing
	}

	// Rx error , reset BNRY pointer to CURR (use SEND PACKET mode)
	if (isr & ISR_RXE) {
		outb(ISR_RXE,NE_ISR);		// clear interrupt
		
		outb(CMD_PAGE1 | CMD_NODMA | CMD_STOP,NE_CR);
		curr = inb(NE_CURR);
		outb(CMD_PAGE0 | CMD_NODMA | CMD_STOP,NE_CR);
		outb(curr, NE_BNRY);
		priv->stats.rx_errors++;
	}	

	//got packet with no errors
	if (isr & ISR_PRX) {
		outb(ISR_PRX, NE_ISR);		// clear interrupt

		outb(CMD_PAGE1 | CMD_NODMA | CMD_STOP, NE_CR);
		curr  =  inb(NE_CURR);
		outb(CMD_PAGE0 | CMD_NODMA | CMD_STOP, NE_CR);
		bnry = inb(NE_BNRY);
		//yangye 2003-1-21
		//get more than one packet until receive buffer is empty
		while(curr != bnry){
			sene2k_rx(dev);
			outb(CMD_PAGE1 | CMD_NODMA | CMD_STOP, NE_CR);
			curr =  inb(NE_CURR);
			outb(CMD_PAGE0 | CMD_NODMA | CMD_STOP, NE_CR);
			bnry = 	inb(NE_BNRY);			
			}
	}
		
	//Transfer complelte, do nothing here
	if( isr & ISR_PTX){
		//printk("ne2k_isr: is ISR_PTX\n");
		outb(ISR_PTX, NE_ISR);          // clear interrupt
	}
		
	outb(CMD_PAGE0 | CMD_NODMA | CMD_STOP, NE_CR);
	outb(0xff, NE_ISR);			// clear ISR	
	
	//open nic for next packet
	outb(CMD_PAGE0 | CMD_NODMA | CMD_RUN, NE_CR);

	//chy 2003-2-17
	//dev->interrupt = 0;
	return;
}


// hard_start_xmit 

int sene2k_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int i;
	int len;
	char *data;
	struct sene2k_priv *priv = (struct sene2k_priv *) dev->priv;
	u8 isr;

	TRACE("start_xmit\n");
	//chy 2003-02-21: debug the net data
	/*
	printk("sk_buff->len: %d\n", skb->len);
	printk("sk_buff->data_len: %d\n", skb->data_len);
	for(i = 0; i < skb->len; i++) {
		printk("%4x", skb->data[i]);
		if((i+1)%16 ==0)
			printk("\n");
	}
	printk("\n");
	*/
	
	len = skb->len < ETH_ZLEN ? ETH_ZLEN : skb->len;
	data = skb->data;
	dev->trans_start = jiffies;
	
	
	//don't close nic,just close receive interrupt	
	outb(CMD_PAGE2 | CMD_NODMA | CMD_RUN, NE_CR);
	isr = inb(NE_IMR);
	isr &= ~ISR_PRX;
      	outb(CMD_PAGE0 | CMD_NODMA | CMD_RUN, NE_CR);
	outb(isr, NE_IMR);
	
	outb(ISR_RDC, NE_ISR);	
	
	// Amount to send
	outb(len & 0xff, NE_RBCR0);
	outb(len >> 8, NE_RBCR1);

    // Address on NIC to store
	outb(XMIT_START & 0xff, NE_RSAR0);
	outb(XMIT_START >> 8, NE_RSAR1);
	// Write command to start
	outb(CMD_PAGE0 | CMD_WRITE | CMD_RUN, NE_CR);    

	//write data to remote dma
	outsb(NE_DMA, skb->data, len);

	//  Wait for remote dma to complete - ISR Bit 6 clear if busy
	while((u8)(inb(NE_ISR) & ISR_RDC) == 0 );
	
	outb(ISR_RDC, NE_ISR);     //clear RDC

	/*
	 * Issue the transmit command.(start local dma)
	 */
 	outb(XMIT_START >> 8, NE_TPSR);
	outb(len & 0xff, NE_TBCR0);
	outb(len >> 8, NE_TBCR1);	
	//  Start transmission (and shut off remote dma)
	//  and reopen nic(CMD_RUN)
	outb(CMD_PAGE0 | CMD_NODMA | CMD_XMIT | CMD_RUN, NE_CR);
		
	
	//reopen receive interrupt
	outb(CMD_PAGE2 | CMD_NODMA | CMD_RUN, NE_CR);
	isr = inb(NE_IMR);
	isr |= ISR_PRX;
        outb(CMD_PAGE0 | CMD_NODMA | CMD_RUN, NE_CR);
	outb(isr, NE_IMR);


	dev_kfree_skb(skb);

	//chy: stats tx info
	priv->stats.tx_packets++;
	priv->stats.tx_bytes+=len;
	
	return 0;
}

struct net_device_stats *sene2k_get_stats(struct net_device *dev)
{
	struct sene2k_priv *priv = (struct sene2k_priv *) dev->priv;
	TRACE("get_stats\n");
	return &priv->stats;
}

/*
 * The init function, invoked by register_netdev()
 */
int sene2k_init(struct net_device *dev)
{
	int i;
	//unsigned char tmpb;
	TRACE("init\n");
	//ether_setup in linux-2.4.x/driver/net
	ether_setup(dev);	// Assign some of the fields

	// set net_device methods
	dev->open = sene2k_open;
	dev->stop = sene2k_stop;
//	dev->ioctl = sene2k_ioctl;
	dev->get_stats = sene2k_get_stats;
//	dev->tx_timeout = sene2k_tx_timeout;
	dev->hard_start_xmit = sene2k_start_xmit;
	// set net_device data members
	dev->watchdog_timeo = timeout;
	dev->irq =AT91_NET_IRQNUM ;  //the ext int number is 16
	dev->dma = 0;
	dev->base_addr=AT91_NET_BASE; //the net registers base addr (0xfffa0000)

	//2003-02-25: walimi find a bug, if addr[0]&1==1 then, this addr is a multicast addr,
	//so the mac addr[0]&1 should be 0. 
	//when tcp_v4_rcv function do command: if (skb->pkt_type!=PACKET_HOST), 
	//if  addr[0]&1==1 then it will discard this packet

	// set MAC address manually
	//chy 2003-04-27: the mac addr is got from skyeye, which get from skyeye.conf
	
	/*
	dev->dev_addr[0] = 0x0;
	dev->dev_addr[1] = 0x1;
	dev->dev_addr[2] = 0x2;
	dev->dev_addr[3] = 0x3;
	dev->dev_addr[4] = 0x4;
	dev->dev_addr[5] = 0x5;
        */

        
	
	printk(KERN_INFO "sene2k dev name: %s: ", dev->name);
	
	/*
	for(i = 0; i < 6; i++)
		printk("%2.2x%c", dev->dev_addr[i], (i==5) ? ' ' : ':');
	printk("\n");
        */
	
	SET_MODULE_OWNER(dev);

	dev->priv = kmalloc(sizeof(struct sene2k_priv), GFP_KERNEL);
	if(dev->priv == NULL){
	    printk(KERN_ERR "sene2k: nomem for sene2k_priv struct\n"); 	
     	return -ENOMEM;
	}
	memset(dev->priv, 0, sizeof(struct sene2k_priv));
	spin_lock_init(&((struct sene2k_priv *) dev->priv)->lock);
	

	
	return 0;
}

struct net_device sene2k_netdevs = {
	init: sene2k_init,
};

/*
 * Finally, the module stuff
 */
int sene2k_init_module(void)
{
	int result;
	TRACE("init_module\n");

	//Print version information
	printk(KERN_INFO "%s", version);

	//register_netdev will call sene2k_init()
	if((result = register_netdev(&sene2k_netdevs)))
		printk("sene2k eth: Error %i registering device \"%s\"\n", result, sene2k_netdevs.name);
	return result ? 0 : -ENODEV;
}

void sene2k_cleanup(void)
{
	TRACE("cleanup\n");
	kfree(sene2k_netdevs.priv);
	unregister_netdev(&sene2k_netdevs);
	return;
}

module_init(sene2k_init_module);
module_exit(sene2k_cleanup);

MODULE_DESCRIPTION("SkyEye NE2k ethernet driver");
MODULE_AUTHOR("Yu Chen & Souyi Ying");
MODULE_LICENSE("GPL");
