/*
 * linux/deriver/net/w740mac.c
 * Ethernet driver for winbond w90n740
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>

#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <asm/semaphore.h>
#include <asm/irq.h>
#include <asm/io.h>
#include "w740mac.h"
#include <asm/arch/flash.h>
#include <asm/arch/bootheader.h>

#define RX_INTERRUPT   15
#define TX_INTERRUPT   13


//xx #define HAVE_PHY
#define IC_PLUS
#undef DEBUG
//xxx #define DEBUG
#define TRACE_ERROR printk
#ifdef	DEBUG
	#define TRACE(str, args...)	printk("W90N740 eth: " str , ##args)
	#define MAC_ASSERT(x) do { if (!(x)) printk("ASSERT: %s:%i(%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)
#else
	#define MAC_ASSERT(x) do { } while (0)
	#define TRACE(str, args...) do { } while (0)
#endif

/* Global variables  used for MAC driver */
static unsigned long  	gMCMDR = MCMDR_SPCRC | MCMDR_EnMDC | MCMDR_ACP;//|MCMDR_LBK;

static unsigned long  	gMIEN = EnTXINTR | EnRXINTR | EnRXGD | EnTXCP |
                               EnTxBErr | EnRxBErr | EnTXABT;//| EnTXEMP;//EnDEN
/*
volatile unsigned long  gMIEN = EnTXINTR | EnRXINTR | EnRXGD | EnTXCP |
                        EnTxBErr | EnRxBErr | EnRDU | EnTDU |
                        EnLC | EnTXABT | EnNCS | EnEXDEF | EnTXEMP |
                      EnCRCE | EnRP | EnPTLE | EnALIE | EnCFR | EnRXOV |
                        EnNATErr | EnNATOK;
*/

#define RX_DESC_SIZE (3*10)
#define TX_DESC_SIZE (10)
#define CHECK_SIZE
#define PACKET_BUFFER_SIZE 1600
#define PACKET_SIZE   1560
#define TX_TIMEOUT  (50)

struct  w740_priv
{
        struct net_device_stats stats;
        unsigned long which;
        unsigned long rx_mode;
        volatile unsigned long cur_tx_entry;
        volatile unsigned long cur_rx_entry;
        volatile unsigned long is_rx_all;
        dma_addr_t dma_handle;
        //Test
        unsigned long bInit;
        unsigned long rx_packets;
        unsigned long rx_bytes;
        unsigned long start_time;

        volatile unsigned long tx_ptr;
        unsigned long tx_finish_ptr;
        volatile unsigned long rx_ptr;

        unsigned long start_tx_ptr;
        unsigned long start_tx_buf;

        //char aa[100*100];
        unsigned long mcmdr;
        volatile unsigned long start_rx_ptr;
        volatile unsigned long start_rx_buf;
        char 		  mac_address[ETH_ALEN];
        volatile  	  RXBD   rx_desc[RX_DESC_SIZE]  __attribute__ ((aligned (16)));
        volatile      TXBD   tx_desc[TX_DESC_SIZE]	__attribute__ ((aligned (16)));
        volatile char rx_buf[RX_DESC_SIZE][PACKET_BUFFER_SIZE]	__attribute__ ((aligned (16)));
        volatile char tx_buf[TX_DESC_SIZE][PACKET_BUFFER_SIZE]	__attribute__ ((aligned (16)));
};

static tbl_info info;	// bootloader info

char w740_mac_address[2][ETH_ALEN]={
                                           {0x00,0x02,0xac,0x55,0x88,0xa1},
                                           {0x00,0x02,0x0a,0x0b,0x0c,0x01}
                                   };

static void init_rxtx_rings(struct net_device *dev);
void notify_hit(struct net_device *dev ,RXBD *rxbd);
int send_frame(struct net_device * ,unsigned char *,int);
void ResetMACRx(struct net_device * dev);
void output_register_context(int );
static int  w740_init(struct net_device *dev);
static void   netdev_rx(struct net_device *dev);
static void rx_interrupt(int irq, void *dev_id, struct pt_regs * regs);
static void tx_interrupt(int irq, void *dev_id, struct pt_regs * regs);
int prossess_nata(struct net_device *dev,RXBD * rxbd );
void ResetTxRing(struct w740_priv * w740_priv);
int ResetMAC0(struct net_device * dev);
int ResetMAC1(struct net_device * dev);
void ResetMAC(struct net_device * dev);
void ResetRxRing(struct w740_priv * w740_priv);
int  MiiStationWrite(int num,unsigned long PhyInAddr,unsigned long PhyAddr,unsigned long PhyWrData);
unsigned long MiiStationRead(int num, unsigned long PhyInAddr, unsigned long PhyAddr);


volatile struct net_device w740_netdevice[2]=
        {
                {
        init:
                        w740_init
                },
        {init:
                 w740_init}
        };

void w740_WriteCam(int which,int x, unsigned char *pval)
{

        unsigned int msw,lsw;

        msw =  	(pval[0] << 24) |
                (pval[1] << 16) |
                (pval[2] << 8) |
                pval[3];

        lsw = (pval[4] << 24) |
              (pval[5] << 16);

        w740_WriteCam1(which,0,lsw,msw);


}

void ResetP(int num)
{
        MiiStationWrite(num,PHY_CNTL_REG,0x0100,RESET_PHY);
}

int  ResetPhyChip(int num)
{
#ifdef HAVE_PHY
        unsigned long RdValue;
        int which=num;
        volatile int loop=1000*100;

        //MiiStationWrite(which, PHY_ANA_REG, PHYAD, DR10_TX_HALF|IEEE_802_3_CSMA_CD);

        if(MiiStationWrite(which, PHY_CNTL_REG, PHYAD, ENABLE_AN | RESTART_AN)==1) {

                return 1;
        }

        TRACE_ERROR("\nWait for auto-negotiation complete...");
        while (1) 	/* wait for auto-negotiation complete */
        {
                RdValue = MiiStationRead(which, PHY_STATUS_REG, PHYAD) ;
                if(RdValue==(unsigned long)1)
                {
                        printk("ResetPhyChip failed 1\n");
                        return 1;
                }
                if ((RdValue & AN_COMPLETE) != 0)
                {
                        break;
                }
                loop--;
                if(loop==0)
                {
                        return 1;
                }
        }
        TRACE_ERROR("OK\n");

        /* read the result of auto-negotiation */
        RdValue = MiiStationRead(which, PHY_CNTL_REG, PHYAD) ;
        if(RdValue==(unsigned long)1)
                return 1;
        if ((RdValue & DR_100MB)!=0) 	/* 100MB */
        {
                TRACE_ERROR("100MB - ");

                w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,which)|MCMDR_OPMOD,which);
        } else {
                w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,which)&~MCMDR_OPMOD,which);
                TRACE_ERROR("10MB - ");
        }
        if ((RdValue & PHY_FULLDUPLEX) != 0) 	/* Full Duplex */
        {
                TRACE_ERROR("Full Duplex\n");
                w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,which)|MCMDR_FDUP,which);
        } else {
                TRACE_ERROR("Half Duplex\n");

                w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,which)&~MCMDR_FDUP,which);
        }
        return 0;
#endif
#ifdef IC_PLUS
#warning __FILE__ __LINE__ Using IC+ PHY
        {
                unsigned long RdValue,j,i;

                //static int reset_phy=0;
                MiiStationWrite(num, PHY_ANA_REG, PHYAD, DR100_TX_FULL|DR100_TX_HALF| DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);

                MiiStationWrite(num, PHY_CNTL_REG, PHYAD, ENABLE_AN | RESET_PHY|RESTART_AN);


                //cbhuang num
                MiiStationWrite(num, 0x16, PHYAD, 0x8420);
                RdValue = MiiStationRead(num, 0x12, PHYAD);

                MiiStationWrite(num, 0x12, PHYAD, RdValue | 0x80); // enable MII registers

                if(num == 1)
                {
                        //TEST .. read phy id!
                        // IP175A: 0243 0c50
                        // IP175C: 0243 0d80
                        RdValue = MiiStationRead(0, PHY_ID1_REG, PHYAD);
                        j = MiiStationRead(0, PHY_ID2_REG, PHYAD);
                        printk("%s %s: PHY Id= 0x%x 0x%x\n",__FILE__,__FUNCTION__,RdValue,j);
                        // End TEST
                        //
                        for(i=0;i<3;i++) {
                                RdValue = MiiStationRead(num, PHY_STATUS_REG, PHYAD) ;
                                if ((RdValue & AN_COMPLETE) != 0) {
                                        printk("come  cbhuang %s   %s  %d  \n",__FILE__,__FUNCTION__,__LINE__);
                                        break;
                                }
                        }
                        if(i==3) {
                                printk("come  cbhuang %s   %s  %d  \n",__FILE__,__FUNCTION__,__LINE__);
                                w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,1)| MCMDR_OPMOD,1);
                                w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,1)| MCMDR_FDUP,1);
                                return 0;
                        }
                }

                {
                        w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,num)|MCMDR_OPMOD,num);
                        w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,num)|MCMDR_FDUP,num);
                }
                return 0;

        }
#endif
}

void ResetMAC(struct net_device * dev)
{
        struct w740_priv * priv=(struct w740_priv *)dev->priv;
        int    which=priv->which ;
        unsigned long val=w740_ReadReg(MCMDR,which);
        unsigned long flags;

        save_flags(flags);
        cli();
        w740_WriteReg(FIFOTHD,0x10000,which); //0x10100
        w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,which)&~(MCMDR_TXON|MCMDR_RXON),which);
        w740_WriteReg(FIFOTHD,0x100300,which); //0x10100
        printk("Reset MAC:%x\n",(unsigned int)&flags);

        if(!netif_queue_stopped(dev))
        {
                netif_stop_queue(dev);
                //printk("Reset MAC stop queue\n");
        }

        init_rxtx_rings(dev);
        dev->trans_start=jiffies;
        priv->cur_tx_entry=0;
        priv->cur_rx_entry=0;
        priv->rx_ptr=priv->start_rx_ptr ;
        priv->tx_ptr=priv->start_tx_ptr ;

        //11-21

        priv->tx_finish_ptr=priv->tx_ptr;

        w740_WriteReg(RXDLSA,priv->start_rx_ptr,which);
        w740_WriteReg(TXDLSA,priv->start_tx_ptr,which);
        w740_WriteReg(DMARFC,PACKET_SIZE,which);

        w740_WriteCam(priv->which,0,dev->dev_addr);

        w740_WriteReg(CAMEN,w740_ReadReg(CAMEN,priv->which) | 1,priv->which);

        w740_WriteReg(CAMCMR,CAMCMR_ECMP|CAMCMR_ABP|CAMCMR_AMP,which);

        /* Configure the MAC control registers. */
        w740_WriteReg(MIEN,gMIEN,which);
        //w740_WriteReg(MCMDR,priv->mcmdr,priv->which);
        if(which==0)
        {
                Enable_Int(EMCTXINT0);
                Enable_Int(EMCRXINT0);
        } else if(which==1)
        {
                Enable_Int(EMCTXINT1);
                Enable_Int(EMCRXINT1);
        }
        /* set MAC0 as LAN port */
        //MCMDR_0 |= MCMDR_LAN ;

        {

                w740_WriteReg(MCMDR,MCMDR_TXON|MCMDR_RXON|val,which);
                w740_WriteReg(TSDR ,0,which);
                w740_WriteReg(RSDR ,0,which);
        }

        w740_WriteReg(MISTA,w740_ReadReg(MISTA,which),which); //clear interrupt

        //printk("reset\n");
        restore_flags(flags);
        //up(&priv->locksend);

        dev->trans_start = jiffies;
        if(netif_queue_stopped(dev))
        {
                netif_wake_queue(dev);
        }
}
/************************************************************************
* FUNCTION                                                             
*        MiiStationWrite
*                                                                      
* DESCRIPTION                                                          
*        Write to the Mii Station Control register.                             
*                                                                      
* INPUTS                                                               
*        int      num         which MAC of w90n740
*        unsigned long   PhyInAddr   PHY register address
*        unsigned long   PhyAddr     Address to write to.
*        unsigned long   PhyWrData   Data to write.
*                                                                      
* OUTPUTS                                                              
*        None.                                    
*************************************************************************/

int  MiiStationWrite(int num,unsigned long PhyInAddr,unsigned long PhyAddr,unsigned long PhyWrData)
{
        volatile int i = 1000;
        int which=num;
        volatile int loop=1000*100;
#ifdef IC_PLUS1

        num = 0;
#endif

        which=num;
        w740_WriteReg(MIID,PhyWrData,which);
        w740_WriteReg(MIIDA,PhyInAddr|PhyAddr|PHYBUSY|PHYWR|MDCCR1,which);
        while(i--)
                ;
        while((w740_ReadReg(MIIDA,which) &PHYBUSY)) {
                loop--;
                if(loop==0) {
                        TRACE_ERROR ("MiiStationWrite: %u still busy on write of %x, %x, %x\n", num, PhyInAddr, PhyAddr, PhyWrData);
                        return 1;
                }
        }
        //   printk("MiiStationWrite 1\n");
        return 0;
}


/************************************************************************
* FUNCTION                                                             
*        MiiStationRead
*                                                                      
* DESCRIPTION                                                          
*        Read from the Mii Station control register.                             
*                                                                      
* INPUTS                                                               
*        int      num         which MAC of w90n740
*        unsigned long   PhyInAddr   PHY register address.
*        unsigned long   PhyAddr     Address to read from.
*                                                                      
* OUTPUTS                                                              
*        unsigned long   Data read.
*************************************************************************/
unsigned long MiiStationRead(int num, unsigned long PhyInAddr, unsigned long PhyAddr)
{
        unsigned long PhyRdData ;
        int which=num;
        volatile int loop=1000*100;

#ifdef  IC_PLUS1

        num = 0;
#endif

        which=num;
#define MDCCR1   0x00b00000  // MDC clock rating

        w740_WriteReg(MIIDA, PhyInAddr | PhyAddr | PHYBUSY | MDCCR1,which);
        while( (w740_ReadReg(MIIDA,which)& PHYBUSY) ) {
                loop--;
                if(loop==0) {
                        TRACE_ERROR ("MiiStationRead: %d still busy on write of %x, %x\n", num, PhyInAddr, PhyAddr);
                        return (unsigned long)1;
                }
        }

        PhyRdData = w740_ReadReg(MIID,which) ;
        return PhyRdData ;
}

/************************************************************************
* FUNCTION                                                             
*        w740_set_mac_address
*                                                                      
* DESCRIPTION                                                          
*        Set MAC Address For Device By Writing CAM Entry 0,  
*                                                                      
* INPUTS                                                               
*       dev :The MAC which address require to modified
*		addr:New Address 
*                                                                      
* OUTPUTS                                                              
*		Always sucess    
*************************************************************************/
static int w740_set_mac_address(struct net_device *dev, void *addr)
{
        struct w740_priv * priv=(struct w740_priv *)dev->priv;

        if(netif_running(dev))
                return -EBUSY;

        memcpy(&priv->mac_address[0],addr+2,ETH_ALEN);
        memcpy(dev->dev_addr,priv->mac_address,ETH_ALEN);
        memcpy(w740_mac_address[priv->which],dev->dev_addr,ETH_ALEN);

        TRACE_ERROR("\nSet MaC Address %u:%u:%u:%u:%u:%u\n",
                    dev->dev_addr[0],
                    dev->dev_addr[1],
                    dev->dev_addr[2],
                    dev->dev_addr[3],
                    dev->dev_addr[4],
                    dev->dev_addr[5]);

        //w740_WriteReg(CAMEN,w740_ReadReg(CAMEN,priv->which) & ~1,priv->which);

        return 0;
}

/************************************************************************
* FUNCTION                                                             
*        init_rxtx_rings
*                                                                      
* DESCRIPTION                                                          
*		Initialize the Tx ring and Rx ring.
*                                                                      
* INPUTS                                                               
*       dev :Which Ring is initialized including Tx and Rx Ring.
*                                                                      
* OUTPUTS                                                              
*		None
*************************************************************************/
static void init_rxtx_rings(struct net_device *dev)
{
        int i;
        struct w740_priv * w740_priv=dev->priv;

        w740_priv->start_tx_ptr =(unsigned long)&w740_priv->tx_desc[0];
        w740_priv->start_tx_buf =(unsigned long)&w740_priv->tx_buf[0];

        w740_priv->start_rx_ptr =(unsigned long)&w740_priv->rx_desc[0];
        w740_priv->start_rx_buf =(unsigned long)&w740_priv->rx_buf[0];


        //Tx Ring
        MAC_ASSERT(w740_priv->start_tx_ptr );
        MAC_ASSERT(w740_priv->start_tx_buf );
        TRACE(" tx which %d start_tx_ptr %x\n",w740_priv->which,w740_priv->start_tx_ptr);

        for ( i = 0 ; i < TX_DESC_SIZE ; i++ )
        {
                w740_priv->tx_desc[i].SL=0;
                w740_priv->tx_desc[i].mode=0;
                w740_priv->tx_desc[i].buffer=(unsigned long)&w740_priv->tx_buf[i];
                w740_priv->tx_desc[i].next=(unsigned long)&w740_priv->tx_desc[i+1];
                TRACE(" *tx cur %d desc %x buffer  %x", i,  &w740_priv->tx_desc[i],w740_priv->tx_desc[i].buffer);
                TRACE("  next %x\n",w740_priv->tx_desc[i].next);
        }
        w740_priv->tx_desc[i-1].next=(unsigned long)&w740_priv->tx_desc[0];
        TRACE(" * cur %d desc %x buffer  %x", i-1,  &w740_priv->tx_desc[i-1],w740_priv->tx_desc[i-1].buffer);
        TRACE("  next %x\n",w740_priv->tx_desc[i-1].next);

        //Rx Ring
        MAC_ASSERT(w740_priv->start_rx_ptr );
        MAC_ASSERT(w740_priv->start_rx_buf );
        TRACE(" tx which %d start_rx_ptr %x\n",w740_priv->which,w740_priv->start_rx_ptr);

        for( i =0 ; i < RX_DESC_SIZE ; i++)
        {
                w740_priv->rx_desc[i].SL=RXfOwnership_DMA;

                w740_priv->rx_desc[i].buffer=(unsigned long)&w740_priv->rx_buf[i];
                w740_priv->rx_desc[i].next=(unsigned long)&w740_priv->rx_desc[i+1];

                TRACE(" # rx which %d,desc %d desc-addr  %x", w740_priv->which,i, &w740_priv->rx_desc[i]);
                TRACE("  next %x\n",w740_priv->rx_desc[i].next);
        }
        w740_priv->rx_desc[i-1].next=(unsigned long)&w740_priv->rx_desc[0];

}

/************************************************************************
* FUNCTION                                                             
*       w740_open
*                                                                      
* DESCRIPTION                                                          
*		Set Register ,Register ISR ,The MAC began to Receive Package.
*                                                                      
* INPUTS                                                               
*       dev :Pointer to MAC That is Opened.
*                                                                      
* OUTPUTS                                                              
*		Sucess if Return 0		
*************************************************************************/
static int   w740_open(struct net_device *dev)
{
        struct w740_priv * priv;
        int    which ;

        priv=(struct w740_priv *)dev->priv;
        which= priv->which;

        init_rxtx_rings(dev);
        priv->rx_ptr=priv->start_rx_ptr ;
        priv->tx_ptr=priv->start_tx_ptr ;

        w740_WriteReg(FIFOTHD,0x10000,which); //0x10100
        w740_WriteReg(FIFOTHD,0x100300,which); //0x10100
        w740_WriteReg(RXDLSA,priv->start_rx_ptr,which);
        w740_WriteReg(TXDLSA,priv->start_tx_ptr,which);
        w740_WriteReg(DMARFC,2000,which);

        w740_WriteCam(priv->which,0,dev->dev_addr);
        w740_WriteReg(CAMEN,w740_ReadReg(CAMEN,priv->which) | 1,priv->which);

        w740_WriteReg(CAMCMR,CAMCMR_ECMP|CAMCMR_ABP|CAMCMR_AMP,which);
        //w740_WriteReg(CAMCMR,CAMCMR_ECMP|CAMCMR_ABP|CAMCMR_AMP|CAMCMR_AUP,which);

        w740_WriteReg(MCMDR,1<<19,which);
        ResetP(which);
        if(ResetPhyChip(which)==1)
        {
                TRACE_ERROR("ResetPhyChip Failed\n");
                return -1;
        }

        //number interrupt  number
        TRACE("**** which %d \n", which);

        /* Configure the MAC control registers. */
        w740_WriteReg(MIEN,gMIEN,which);
        w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,which)|gMCMDR,which);
        w740_WriteReg(MCMDR,w740_ReadReg(MCMDR,which)|MCMDR_RXON,which);

        priv->mcmdr=w740_ReadReg(MCMDR,which);
        priv->bInit=1;
        priv->rx_packets=0;
        priv->rx_bytes=0;
        priv->start_time=jiffies;

        if(which==0)
        {
                /* Tx interrupt vector setup. */
                AIC_SCR_EMCTX0 = 0x41;
                /* Rx interrupt vector setup. */
                AIC_SCR_EMCRX0 = 0x41;
                /* Enable MAC Tx and Rx interrupt. */
                //	Enable_Int(EMCTXINT0);
                //	Enable_Int(EMCRXINT0);
                /* set MAC0 as LAN port */
                //MCMDR_0 |= MCMDR_LAN ;
        } else
        {
                /* Tx interrupt vector setup. */
                AIC_SCR_EMCTX1 = 0x41;
                /* Rx interrupt vector setup. */
                AIC_SCR_EMCRX1 = 0x41;
                /* Enable MAC Tx and Rx interrupt. */
                //	Enable_Int(EMCTXINT1);
                //	Enable_Int(EMCRXINT1);
                /* set MAC0 as LAN port */
                //MCMDR_1 |= MCMDR_LAN ;
        }

        if(request_irq(TX_INTERRUPT+which,tx_interrupt,SA_INTERRUPT,"w740_tx_irq",dev))
        {
                TRACE_ERROR(KERN_ERR "W90N740 : register irq tx failed\n");
                return -EAGAIN;
        }

        //compute    interrupt number
        if(request_irq(RX_INTERRUPT+which,rx_interrupt,SA_INTERRUPT,"w740_rx_irq",dev))
        {
                TRACE_ERROR(KERN_ERR "w90N740 : register irq rx failed\n");
                return -EAGAIN;
        }
        netif_start_queue(dev);
        w740_WriteReg(RSDR ,0,which);

        TRACE("%s is OPENED\n",dev->name);
        return 0;
}


static int   w740_close(struct net_device *dev)
{
        struct w740_priv *priv=(struct w740_priv *)dev->priv;
        int which=priv->which;

        priv->bInit=0;

        netif_stop_queue(dev);
        w740_WriteReg(MCMDR,0,which);
        free_irq(RX_INTERRUPT+which,dev);
        free_irq(TX_INTERRUPT+which,dev);

        return 0;
}

/* Get the current statistics.	This may be called with the card open or
   closed. */

static struct net_device_stats * w740_get_stats(struct net_device *dev)
{
        struct w740_priv *priv = (struct w740_priv *)dev->priv;

        return &priv->stats;
}

static void w740_timeout(struct net_device *dev)
{
        struct w740_priv * priv=(struct w740_priv *)dev->priv;
        int which=priv->which;

#ifdef DEBUG

        int i=0;
        unsigned long cur_ptr;
        TXBD  *txbd;

        cur_ptr=w740_ReadReg(CTXDSA,which);
        printk("&(priv->tx_desc[%d]):%x,&(priv->tx_desc[%d]:%x\n"
               ,priv->cur_tx_entry,&(priv->tx_desc[priv->cur_tx_entry])
               ,priv->cur_tx_entry+1,&(priv->tx_desc[priv->cur_tx_entry+1]));
        printk(",cur_ptr:%x,mode:%x,SL:%x\n",
               cur_ptr,((TXBD  *)cur_ptr)->mode,((TXBD  *)cur_ptr)->SL);
        printk("priv->tx_ptr:%x,SL:%x,mode:%x\n",
               priv->tx_ptr,((TXBD *)(priv->tx_ptr))->SL,((TXBD *)(priv->tx_ptr))->mode);
        printk("0xfff82114:%x,MIEN:%x,MISTA:%x\n",CSR_READ(0xfff82114),
               w740_ReadReg(MIEN,which),w740_ReadReg(MISTA,which));
        printk("MAC %d timeout,pid:%d\n",
               priv->which,current->pid);
#if 0

        printk("MAC %d timeout,pid:%d,mode:%d\n",
               priv->which,current->pid,mode);
#endif

        for ( i = 0 ; i < TX_DESC_SIZE ; i++ )
        {
                printk("*tx cur %d desc %x buffer %x",i,&priv->tx_desc[i],priv->tx_desc[i].buffer);
                printk(" next %x\n",priv->tx_desc[i].next);
        }
#endif
        {
                printk("RXFSM:%x\n",w740_ReadReg(RXFSM,which));
                printk("TXFSM:%x\n",w740_ReadReg(TXFSM,which));
                printk("FSM0:%x\n",w740_ReadReg(FSM0,which));
                printk("FSM1:%x\n",w740_ReadReg(FSM1,which));
                //if((w740_ReadReg(TXFSM,which)&0x0FFF0000)==0x8200000)
                ResetMAC(dev);
        }
        dev->trans_start = jiffies;

}

static int w740_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
#ifdef DEBUG
        char *data;
        int i=0;
        int len=skb->len;
        for(i=0;i<len;i+=10)
                printk("%2x %2x %2x %2x %2x %2x %2x %2x %2x %2x\n",*(data+i),*(data+i+1),
                       *(data+i+2),*(data+i+3),*(data+i+4),*(data+i+5),*(data+i+6),
                       *(data+i+7),*(data+i+8),*(data+i+9));
        printk("\n");
#endif
        //printk("w740_start_xmit:dev:%x\n",dev);
        if(!(send_frame(dev,skb->data,skb->len)) )
        {
                dev_kfree_skb(skb);
                TRACE("w740_start_xmit ok\n");
                return 0;
        }
        //printk("send failed\n");
        return -1;
}

/* The typical workload of the driver:
   Handle the network interface interrupts. */

static void tx_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
        struct net_device *dev = (struct net_device *)dev_id;
        struct w740_priv *priv = (struct w740_priv *)dev->priv;
        unsigned long status;
        unsigned long cur_ptr;
        int which=priv->which;
        TXBD  *txbd;
        static  unsigned  reset_rx_loop=0;

        unsigned long flags;
        save_flags(flags);
        cli();
        status=w740_ReadReg(MISTA,which);   //get  interrupt status;
        //w740_WriteReg(MISTA,status&0xFFFF0000,which);  //clear interrupt
        w740_WriteReg(MISTA,status,which);  //clear interrupt
        restore_flags(flags);

        cur_ptr=w740_ReadReg(CTXDSA,which);
#if 0

        if(which==1)
                printk("tx_ptr:%x,cur_ptr:%x,tx_entry:%d,s:%x\n",priv->tx_ptr,cur_ptr,priv->cur_tx_entry,status);
#endif

        while((&(priv->tx_desc[priv->cur_tx_entry]) != cur_ptr))
        {
                txbd =(TXBD *)&(priv->tx_desc[priv->cur_tx_entry]);
                priv->cur_tx_entry = (priv->cur_tx_entry+1)%(TX_DESC_SIZE);

                TRACE("*txbd->SL %x\n",txbd->SL);
                TRACE("priv->tx_ptr %x  cru_ptr =%x\n",priv->tx_ptr,cur_ptr);
                if(txbd->SL &TXDS_TXCP) {
                        priv->stats.tx_packets++;
                } else {
                        priv->stats.tx_errors++;
                }

                txbd->SL=0;
                txbd->mode=0;

                if (netif_queue_stopped(dev)) {
                        netif_wake_queue(dev);
                }
        }

        if(status&MISTA_EXDEF)
        {
                printk("MISTA_EXDEF\n");
        }
        if((status & MISTA_RDU)&& ++reset_rx_loop==5)
        {
                TRACE_ERROR("W90N740 MAC In Tx %d RX   I Have Not Any Descript Needed\n",priv->which);
                //ResetMAC(dev);
                //reset_rx_loop=0;
        }
        if(status&MISTA_TxBErr)
                printk("MISTA_TxBErr\n");
        if(status&MISTA_TDU)
        {
                //printk("MISTA_TDU\n");
                if (netif_queue_stopped(dev)) {
                        netif_wake_queue(dev);
                        TRACE_ERROR("queue restart TDU\n");
                }
        }
        TRACE("After %d tx_interrupt status %x  \n",which,status);
}
volatile unsigned long rx_jiffies0=0;
volatile unsigned long rx_jiffies1=0;
extern volatile unsigned long jiffies;

static void rx_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
        struct net_device *dev = (struct net_device *)dev_id;
        struct w740_priv  *priv = (struct w740_priv *) dev->priv;
        unsigned long status;
        int which=priv->which;
        unsigned long flags;
        if(which==0)
                rx_jiffies0 = jiffies;
        else if(which==1)
                rx_jiffies1 = jiffies;
        status=w740_ReadReg(MISTA,which);   //get  interrupt status;
        save_flags(flags);
        cli();
        //w740_WriteReg(MISTA,status&0xFFFF,which); //clear interrupt
        w740_WriteReg(MISTA,status,which); //clear interrupt
        restore_flags(flags);

        priv->cur_rx_entry++;
#if 0

        if(which==1)
                printk("rx_interrupt status %x\n",which,status);
#endif

        if(status & (MISTA_RDU|MISTA_RxBErr))
        {
                printk("No Descript available\n");
                priv->is_rx_all=RX_DESC_SIZE; //receive all
                netdev_rx(dev);    //start doing
                priv->is_rx_all=0;
                if(status&MISTA_RxBErr) {
                        printk("MISTA_RxBErr\n");
                        ResetMAC(dev);
                }
                w740_WriteReg(RSDR ,0,which);
                TRACE("* %d rx_interrupt MISTA %x \n",irq,status);

                return ;
        }
        save_flags(flags);
        cli();
        w740_WriteReg(MISTA,status,which); //clear interrupt
        restore_flags(flags);
        netdev_rx(dev);
}

void  ResetMACRx(struct net_device * dev)
{
        struct w740_priv * priv=(struct w740_priv *)dev->priv;
        unsigned long val=w740_ReadReg(MCMDR,priv->which);
        printk("In ResetMAC Rx \n");
        ResetRxRing(dev->priv);
        w740_WriteReg(MCMDR,(MCMDR_RXON|val),priv->which);
}

void ResetMACTx(struct net_device * dev)
{
        struct w740_priv * priv=(struct w740_priv *)dev->priv;
        unsigned long val=w740_ReadReg(MCMDR,priv->which);
        printk("In ResetMAC Tx \n");
        //ResetTxRing(dev->priv);
        w740_WriteReg(MCMDR,(MCMDR_TXON|val),priv->which);
}

void ResetRxRing(struct w740_priv * w740_priv)
{

        int i;
        for( i =0 ; i < RX_DESC_SIZE ; i++)
        {
                w740_priv->rx_desc[i].SL=0;
                w740_priv->rx_desc[i].SL|=RXfOwnership_DMA;
        }

}
#if 0
void DescriptorLock(struct w740_priv * priv)
{
        unsigned long flags;
        save_flags(flags);
        cli();
        if(priv->lock==1)

                while(priv->lock==1)
                        ;
        priv->lock=1;
        restore_flags(flags)
        ;

}
void DescriptorUnLock(struct w740_priv * priv)
{
        unsigned long flags;
        save_flags(flags);
        cli();
        priv->lock=0;
        restore_flags(flags)
        ;
}
/*0:failed,1:success*/
int DescriptorTryLock(struct w740_priv * priv)
{
        unsigned long flags;
        save_flags(flags);
        cli();
        if(priv->lock)
                return 0;
        priv->lock=1;
        restore_flags(flags)
        ;
        return 1;
}
#endif

/* We have a good packet(s), get it/them out of the buffers. */
static void   netdev_rx(struct net_device *dev)
{
        struct w740_priv * priv = (struct w740_priv *)dev->priv;
        RXBD *rxbd;
        unsigned long length;
        unsigned long status;
        int flag=0;

        rxbd=(RXBD *)priv->rx_ptr ;

        do
        {
#if 0
                if(!priv->is_rx_all&&(w740_ReadReg(CRXDSA,priv->which)==(unsigned long)rxbd)) {
                        break;
                }
                priv->is_rx_all = 0;
#endif

                if(priv->is_rx_all>0) {
                        flag=1;
                        --priv->is_rx_all;
                } else if(flag==1) {
                        flag=0;
                        break;
                } else if(((w740_ReadReg(CRXDSA,priv->which)==(unsigned long)rxbd)||(rxbd->SL & RXfOwnership_NAT))) {
                        break;
                }

                /*	if(!(rxbd->SL & RXfOwnership_CPU))
                	{	
                		if(priv->is_rx_all)
                			rxbd->SL |=RXfOwnership_DMA;
                		
                		priv->rx_ptr=( RXBD *)rxbd->next;  
                		rxbd=priv->rx_ptr;
                		continue;
                	}
                */

                length = rxbd->SL & 0xFFFF;
                status = (rxbd->SL & 0xFFFF0000)&((unsigned long)~0 >>2);

                if(status & RXDS_RXGD) {
                        unsigned char  * data;
                        struct sk_buff * skb;
                        //When NATA Hit ,Transmit it derictly
#ifdef WINBOND_NATA

                        if(status & (RXDS_NATFSH|RXDS_Hit)) {
                                TRACE("NATA Hit");
                                //if(prossess_nata(dev,(unsigned char *) rxbd->buffer))
                                continue;
                        }
#endif	 //end WINBOND_NATA
#if 0
                        if(which==1)
                                printk("****Good****\n");
#endif

                        data = (unsigned char *) rxbd->buffer;

                        skb = dev_alloc_skb(length+2);  //Get Skb Buffer;
                        if(!skb) {
                                TRACE_ERROR("W90N740: I Have Not Got Memory In Fun %s\n",__FUNCTION__);
                                priv->stats.rx_dropped++;
                                return;
                        }

                        skb->dev = dev;
                        skb_reserve(skb, 2);   //For IP Align 4-byte
                        skb_put(skb, length);
                        eth_copy_and_sum(skb, data, length, 0);  //copy
                        skb->protocol = eth_type_trans(skb, dev);
                        priv->stats.rx_packets++;
                        priv->stats.rx_bytes += length;
                        netif_rx(skb);    // Enqueue for Up Layer

                } else {

                        if(priv->is_rx_all==RX_DESC_SIZE)
                                TRACE_ERROR("Rx error:%x,rxbd:%x,priv->is_rx_all:%d\n",status,rxbd,priv->is_rx_all);
                        priv->stats.rx_errors++;
                        if(status & RXDS_RP ) {
                                TRACE_ERROR("W90N740 MAC: Receive Runt Packet Drop it!\n");
                                priv->stats.rx_length_errors++;
                        }
                        if(status & RXDS_CRCE ) {
                                TRACE_ERROR("W90N740 MAC Receive CRC  Packet Drop It! \n");
                                priv->stats.rx_crc_errors ++;
                        }
                        if(status & RXDS_ALIE ) {
                                TRACE_ERROR("W90N740 MAC Receive Aligment Packet Dropt It!\n");
                                priv->stats.rx_frame_errors++;
                        }

                        if(status &  RXDS_PTLE) {
                                TRACE_ERROR("W90N740 MAC Receive Too Long  Packet Dropt It!\n");
                                priv->stats.rx_over_errors++;
                        }
                }

                //rxbd->SL= RX_OWNERSHIP_DMA; //clear status and set dma flag
                rxbd->SL =RXfOwnership_DMA;
                rxbd->reserved = 0;
                priv->rx_ptr=(unsigned long)rxbd->next;
                rxbd=(RXBD *)priv->rx_ptr;
                dev->last_rx = jiffies;
        } while(1);
        priv->is_rx_all = 0;
}

static void w740_set_multicast_list(struct net_device *dev)
{

        struct w740_priv *priv = (struct w740_priv *)dev->priv;
        unsigned long rx_mode;
        //printk("w740_set_multicast_list\n");
        int which=priv->which;

        if(dev->flags&IFF_PROMISC)
        {
                rx_mode = CAMCMR_AUP|CAMCMR_AMP|CAMCMR_ABP|CAMCMR_ECMP;
                TRACE("W90N740 : Set Prommisc Flag \n");

        } else if((dev->flags&IFF_ALLMULTI)||dev->mc_list)
        {

                rx_mode=CAMCMR_AMP|CAMCMR_ABP|CAMCMR_ECMP;
        } else
        {
                rx_mode = CAMCMR_ECMP|CAMCMR_ABP;
                TRACE("W90N740 :Set Compare Flag\n");
        }

        //rx_mode=CAMCMR_AMP|CAMCMR_ABP|CAMCMR_ECMP;//|CAMCMR_AUP;
        priv->rx_mode=rx_mode;
        w740_WriteReg(CAMCMR,rx_mode,which);

}
#define SIODEVSTARTNATA 0x6677
#define SIODEVSTARTNATA 0x6688

static int w740_do_ioctl(struct net_device *dev,struct ifreq *ifr,int cmd)
{
        //u16 *data = (u16 *)&ifr->ifr_data;
        struct w740_priv *priv=dev->priv;
        int which = priv->which;

        printk("W90N740 IOCTL:\n");

        switch(cmd)
        {
        case  SIOCSIFHWADDR:
                if(dev->flags&IFF_PROMISC)
                        return -1;

                memcpy(dev->dev_addr,ifr->ifr_hwaddr.sa_data,ETH_ALEN);
                memcpy(w740_mac_address[which],dev->dev_addr,ETH_ALEN);

                w740_set_mac_address(dev,dev->dev_addr);

                break;

#define SIOCW740MACDEGUG SIOCDEVPRIVATE+1

        case  SIOCW740MACDEGUG :  //For Debug;
                output_register_context(which);
                break;

        default:
                return -EOPNOTSUPP;
        }
        return 0;
}
void output_register_context(int which)
{
        printk("		** W90N740 EMC Register %d **\n",which);

        printk("CAMCMR:%x ",w740_ReadReg(CAMCMR,which));
        printk("CAMEN:%x ",w740_ReadReg(CAMEN,which));
        printk("MIEN: %x ",w740_ReadReg(MIEN,which));
        printk("MCMDR: %x ",w740_ReadReg(MCMDR,which));
        printk("MISTA: %x ",w740_ReadReg(MISTA,which));
        printk("TXDLSA:%x ", w740_ReadReg(TXDLSA,which));
        printk("RXDLSA:%x \n", w740_ReadReg(RXDLSA,which));
        printk("DMARFC:%x ", w740_ReadReg(DMARFC,which));
        printk("TSDR:%x ", w740_ReadReg(TSDR,which));
        printk("RSDR:%x ", w740_ReadReg(RSDR,which));
        printk("FIFOTHD:%x ", w740_ReadReg(FIFOTHD,which));
        printk("MISTA:%x ", w740_ReadReg(MISTA,which));
        printk("MGSTA:%x ", w740_ReadReg(MGSTA,which));

        printk("CTXDSA:%x \n",w740_ReadReg(CTXDSA,which));
        printk("CTXBSA:%x ",w740_ReadReg(CTXBSA,which));
        printk("CRXDSA:%x ", w740_ReadReg(CRXDSA,which));
        printk("CRXBSA:%x ", w740_ReadReg(CRXBSA,which));
        printk("RXFSM:%x ",w740_ReadReg(RXFSM,which));
        printk("TXFSM:%x ",w740_ReadReg(TXFSM,which));
        printk("FSM0:%x ",w740_ReadReg(FSM0,which));
        printk("FSM1:%x \n",w740_ReadReg(FSM1,which));
#if 0

        printk("EMC01 :\n");

        printk("MCMDR: %x ",w740_ReadReg(MCMDR,1));
        printk("MISTA: %x ",w740_ReadReg(MISTA,1));
        printk("RXDLSA:%x ", w740_ReadReg(RXDLSA,1));
        printk("CTXDSA:%x \n",w740_ReadReg(CTXDSA,1));
#endif
}

void ShowDescriptor(struct net_device *dev)
{
        int i;
        struct w740_priv * w740_priv=dev->priv;
        for(i=0;i<TX_DESC_SIZE;i++)
                printk("%x mode:%lx,2 SL:%lx\n",&w740_priv->tx_desc[i],w740_priv->tx_desc[i].mode,w740_priv->tx_desc[i].SL);
        for(i=0;i<RX_DESC_SIZE;i++)
                printk("%x SL:%x\n",&w740_priv->rx_desc[i],w740_priv->rx_desc[i].SL);
        printk("tx_ptr:%x,tx_entry:%d\n",w740_priv->tx_ptr,w740_priv->cur_tx_entry);
        printk("rx_ptr:%x\n",w740_priv->rx_ptr);

        return;

}
int prossess_nata(struct net_device *dev,RXBD * rxbd )
{
#if 0
        struct net_device which_dev;
        struct w740_priv *priv;
        int lenght=rxbd->SL&0xFFFF;
        int which=(priv->which==0);

        which_dev=&w740_netdevice[which];

        notify_hit(dev,rxbd);
        send_frame(which_dev,(unsigned char * ) rxbd->buffer,length);
#endif

        return 1;
}
int send_frame(struct net_device *dev ,unsigned char *data,int length)
{
        struct w740_priv * priv= dev->priv;
        int which;
        TXBD *txbd;
        unsigned long flags;

        which=priv->which;

        //if (!down_trylock(&priv->locksend)) {
        txbd=( TXBD *)priv->tx_ptr;

        //Have a Descriptor For Transmition?
        /*
           if((txbd->mode&TXfOwnership_DMA))
           {
           	TRACE_ERROR("send_frame failed\n");
           	netif_stop_queue(dev);
           	return -1;
           }
        */
        //txbd->mode=(TX_OWNERSHIP_DMA|TX_MODE_PAD|TX_MODE_CRC|TX_MODE_IE);

        //Check Frame Length
        if(length>1514)
        {
                TRACE(" Send Data %d Bytes ,Please  Recheck Again\n",length);
                length=1514;
        }

        txbd->SL=length&0xFFFF;

        memcpy((void *)txbd->buffer,data,length);

        txbd->mode=(PaddingMode | CRCMode | MACTxIntEn);
        txbd->mode|= TXfOwnership_DMA;

        {
                int val=w740_ReadReg(MCMDR,which);
                if(!(val&	MCMDR_TXON)) {
                        //printk("****w740_WriteReg(MCMDR\n");
                        w740_WriteReg(MCMDR,val|MCMDR_TXON,which);
                }
                w740_WriteReg(TSDR ,0,which);
        }
        txbd=(TXBD *)txbd->next;
        priv->tx_ptr=(unsigned long)txbd;
        dev->trans_start=jiffies;

        save_flags(flags);
        cli();
        if(txbd->mode&TXfOwnership_DMA)
                netif_stop_queue(dev);
        restore_flags(flags);
        return 0;
}
void notify_hit(struct net_device *dev ,RXBD *rxbd)
{
        TRACE("notify_hit not implement\n");
}


static int w740_init(struct net_device *dev)
{
        static int which=1;
        struct w740_priv *priv;
        dma_addr_t dma_handle;

        printk(KERN_INFO __FILE__ ": %s Initialization OK!\n",dev->name);
        if(dev==&w740_netdevice[0])
                which=0;
        else
                which=1;
        //printk("which:%d\n",which);
        ether_setup(dev);
        dev->open=w740_open;
        dev->stop=w740_close;
        dev->do_ioctl=w740_do_ioctl;
        dev->hard_start_xmit=w740_start_xmit;
        dev->tx_timeout=w740_timeout;
        dev->get_stats=w740_get_stats;
        dev->watchdog_timeo =TX_TIMEOUT;
        dev->irq=13+which;
        dev->set_multicast_list=w740_set_multicast_list;
        dev->set_mac_address=w740_set_mac_address;
        dev->priv = consistent_alloc(GFP_KERNEL|GFP_DMA,sizeof(struct w740_priv),&dma_handle);
        if(dev->priv == NULL)
                return -ENOMEM;
        memset(dev->priv, 0, sizeof(struct w740_priv));

        if (info.type == BOOTLOADER_INFO )
                memcpy(w740_mac_address[which],(which)?info.mac1:info.mac0,ETH_ALEN);
        memcpy(dev->dev_addr,w740_mac_address[which],ETH_ALEN);

        priv=(struct w740_priv *)dev->priv;
        priv->dma_handle=dma_handle;
        priv->which=which;
        priv->cur_tx_entry=0;
        priv->cur_rx_entry=0;

        TRACE("%s initial ok!\n",dev->name);
        return 0;

}

#ifdef CONFIG_PROC_FS
static int read_proc (char *buf, char **start, off_t offset,
                      int len, int *eof, void *private)
{
        int written = 0;

        written += sprintf (buf + written, "W740MAC: %s %s\n", __DATE__, __TIME__);
        written += sprintf (buf + written, "AIC: IRSR: %x, IASR: %x, ISR: %x, IPER: %x, ISNR: %x, IMR: %x, OISR: %x\n",
                            CSR_READ (0xfff82100), CSR_READ(0xfff82104), CSR_READ(0xfff82108), CSR_READ(0xfff8210c),
                            CSR_READ (0xfff82110), CSR_READ(0xfff82114), CSR_READ(0xfff82118));
        written += sprintf (buf + written, "EMC0: MISTA: %x, MGSTA: %x\n", CSR_READ(0xfff030b4), CSR_READ(0xfff030b8));

        return written;
}
#endif

int  init_module(void)
{
        int ret;
        winbond_header *part;

        part = find_winbond_part(0);
        if (part)
                info = *((tbl_info*)(part->start));

#ifdef CONFIG_W90N740FLASH

        GetLoadImage();
#endif

#ifdef CONFIG_PROC_FS

        if (!create_proc_read_entry ("w740mac", 0, 0, read_proc, 0))
                printk (KERN_ERR "can't create w740mac proc entry\n");
#endif

        memset((void *)w740_netdevice[0].name ,0 ,IFNAMSIZ);
        ret=register_netdev((struct net_device *)&w740_netdevice[0]);
        if(ret!=0) {
                TRACE_ERROR("Regiter EMC 0 W90N740 FAILED\n");
                return  -ENODEV;
        }


        memset((void*)w740_netdevice[1].name ,0 ,IFNAMSIZ);
        ret=register_netdev((struct net_device *)&w740_netdevice[1]);
        if(ret!=0) {
                TRACE_ERROR("Regiter EMC 1 W90N740 FAILED\n");
                return  -ENODEV;
        }

        return 0;
}

void cleanup_module(void)
{
        unregister_netdev((struct net_device *)&w740_netdevice[0]);
        unregister_netdev((struct net_device *)&w740_netdevice[1]);
}

module_init(init_module);
module_exit(cleanup_module);
