/*----------------------------------------------------------------------------
 * ep93xx_eth.c
 *  Ethernet Device Driver for Cirrus Logic EP93xx.
 *
 * Copyright (C) 2003 by Cirrus Logic www.cirrus.com 
 * This software may be used and distributed according to the terms
 * of the GNU Public License.
 *
 *   This driver was written based on skeleton.c by Donald Becker and
 * smc9194.c by Erik Stahlman.  
 *
 * Theory of Operation
 * Driver Configuration
 *  - Getting MAC address from system
 *     To setup identical MAC address for each target board, driver need
 *      to get a MAC address from system.  Normally, system has a Serial 
 *      EEPROM or other media to store individual MAC address when 
 *      manufacturing.
 *      The macro GET_MAC_ADDR is prepared to get the MAC address from 
 *      system and one should supply a routine for this purpose.
 * Driver Initialization
 * DMA Operation
 * Cache Coherence
 *
 * History:
 * 07/19/01 0.1  Sungwook Kim  initial release
 * 10/16/01 0.2  Sungwook Kim  add workaround for ignorance of Tx request while sending frame
 *                             add some error stuations handling
 *
 * 03/25/03 Melody Lee Modified for EP93xx
 *--------------------------------------------------------------------------*/

static const char  *version=
    "ep93xx_eth.c: V1.0 09/04/2003 Cirrus Logic\n";


#include <linux/module.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/arch/hardware.h>
#include <asm/arch/ssp.h>

#include "ep93xx_eth.h"


/*----------------------------------------------------------------------------
 * The name of the card.
 * It is used for messages and in the requests for io regions, irqs and ...
 * This device is not in a card but I used same name in skeleton.c file.
 *--------------------------------------------------------------------------*/
#define  cardname  "ep93xx_ethernet"


/*----------------------------------------------------------------------------
 * Macro to get MAC address from system
 *
 * Parameters:
 *  - pD  : a pointer to the net device information, struct net_device.
 *          one can get base address of device from pD->base_addr,
 *          device instance ID (0 for 1st and so on) from *((int*)pD->priv).
 *  - pMac: a pointer to a 6 bytes length buffer to return device MAC address.
 *          this buffer will be initialized with default_mac[] value before 
 *          calling.
 * Return: none
 *--------------------------------------------------------------------------*/
#define GET_MAC_ADDR(pD, pMac)						\
	{								\
		unsigned int uiTemp;					\
		int SSP_Handle;						\
									\
		SSP_Handle = SSPDriver->Open( SERIAL_FLASH, 0 );	\
		if (SSP_Handle != -1) {					\
			SSPDriver->Read( SSP_Handle, 0x1000, &uiTemp );	\
			if (uiTemp == 0x43414d45) {			\
				SSPDriver->Read( SSP_Handle, 0x1004,	\
						 &uiTemp );		\
				pMac[0] = uiTemp & 255;			\
				pMac[1] = (uiTemp >> 8) & 255;		\
				pMac[2] = (uiTemp >> 16) & 255;		\
				pMac[3] = uiTemp >> 24;			\
				SSPDriver->Read( SSP_Handle, 0x1008,	\
						 &uiTemp );		\
				pMac[4] = uiTemp & 255;			\
				pMac[5] = (uiTemp >> 8) & 255;		\
			}						\
			SSPDriver->Close( SSP_Handle );			\
		}							\
	}



/****  default MAC address if GET_MAC_ADDR does not override  *************/
static const U8  default_mac[6] = {0x00, 0xba, 0xd0, 0x0b, 0xad, 0x00};


/*----------------------------------------------------------------------------
 * A List of default device port configuration for auto probing.
 * At this time, the CPU has only one Ethernet device,
 * but better to support multiple device configuration.
 * Keep in mind that the array must end in zero.
 *--------------------------------------------------------------------------*/

/* We get the MAC_BASE from include/asm-arm/arch-ep93xx/regmap.h */
static struct {
    unsigned int  baseAddr;   /*base address, (0:end mark)*/
    int           irq;        /*IRQ number, (0:auto detect)*/
}  portList[] __initdata = { 
    {MAC_BASE, 39},
    {0/*end mark*/, 0}
};



/*----------------------------------------------------------------------------
 * Some definitions belong to the operation of this driver.
 * You should understand how it affect to driver before any modification.
 *--------------------------------------------------------------------------*/

/****  Interrupt Sources in Use  *******************************************/
/*#define  Default_IntSrc  (IntEn_RxMIE|IntEn_RxSQIE|IntEn_TxLEIE|IntEn_TIE|IntEn_TxSQIE|IntEn_RxEOFIE|IntEn_RxEOBIE|IntEn_RxHDRIE)
*/
#define  Default_IntSrc  (IntEn_TxSQIE|IntEn_RxEOFIE|IntEn_RxEOBIE|IntEn_RxHDRIE)

/****  Length of Device Queue in number of entries
       (must be less than or equal to 255)  ********************************/
#define  LEN_QueRxDesc  64             /*length of Rx Descriptor Queue (4 or bigger) Must be power of 2.*/
#define  LEN_QueRxSts   LEN_QueRxDesc  /*length of Rx Status Queue*/
#define  LEN_QueTxDesc  8              /*length of Tx Descriptor Queue (4 or bigger) Must be power of 2.*/
#define  LEN_QueTxSts   LEN_QueTxDesc  /*length of Tx Status Queue*/

/****  Tx Queue fill-up level control  *************************************/
#define  LVL_TxStop    LEN_QueTxDesc - 2    /*level to ask the stack to stop Tx*/
#define  LVL_TxResume  2   /*level to ask the stack to resume Tx*/

/****  Rx Buffer length in bytes  ******************************************/
#define  LEN_RxBuf  (1518+2+16)  /*length of Rx buffer, must be 4-byte aligned*/
#define  LEN_TxBuf   LEN_RxBuf


/*----------------------------------------------------------------------------
 * MACRO for ease
 *--------------------------------------------------------------------------*/

#define  Align32(a)  (((unsigned int)(a)+3)&~0x03)  /*32bit address alignment*/
#define  IdxNext(idxCur,len)  (((idxCur)+1)%(len))  /*calc next array index number*/



/****  malloc/free routine for DMA buffer  **********************************/
 /*use non-cached DMA buffer*/
#define  MALLOC_DMA(size, pPhyAddr)  consistent_alloc(GFP_KERNEL|GFP_DMA, (size), (dma_addr_t*)(pPhyAddr))
#define  FREE_DMA(vaddr)            consistent_free(vaddr)
extern void  cpu_arm920_dcache_clean_range(U32 va_bgn,U32 va_end);
#define  dma_cache_wback(vaddr,size)  cpu_arm920_dcache_clean_range((U32)(vaddr),(U32)(vaddr)+(size)-1)


/*----------------------------------------------------------------------------
 * DEBUGGING LEVELS
 *
 * 0 for normal operation
 * 1 for slightly more details
 * >2 for various levels of increasingly useless information
 *    2 for interrupt tracking, status flags
 *    3 for packet dumps, etc.
 *--------------------------------------------------------------------------*/
//#define  _DBG  3

#if (_DBG > 2 )
#define PRINTK3(x) printk x
#else
#define PRINTK3(x)
#endif

#if _DBG > 1
#define PRINTK2(x) printk x
#else
#define PRINTK2(x)
#endif

#if _DBG > 0
#define PRINTK1(x) printk x
#else
#define PRINTK1(x)
#endif

#ifdef _DBG
#define PRINTK(x) printk x
#else
#define PRINTK(x)
#endif

#define  _PRTK_ENTRY      PRINTK2   /*to trace function entries*/
#define  _PRTK_SWERR      PRINTK    /*logical S/W error*/
#define  _PRTK_SYSFAIL    PRINTK    /*system service failure*/
#define  _PRTK_HWFAIL     PRINTK    /*H/W operation failure message*/
#define  _PRTK_WARN       PRINTK1   /*warning information*/
#define  _PRTK_INFO       PRINTK2   /*general information*/
#define  _PRTK_ENTRY_ISR  PRINTK3   /*to trace function entries belong to ISR*/
#define  _PRTK_WARN_ISR   PRINTK1   /*warning informations from ISR*/
#define  _PRTK_INFO_ISR   PRINTK3   /*general informations from ISR*/
#define  _PRTK_           PRINTK    /*for temporary print out*/

#if 0
# define  _PRTK_DUMP       PRINTK1   /*to dump large amount of debug info*/
#endif



/*----------------------------------------------------------------------------
 * Custom Data Structures
 *--------------------------------------------------------------------------*/

/****  the information about the buffer passed to device.
       there are matching bufferDescriptor informations 
       for each Tx/Rx Descriptor Queue entry to trace 
       the buffer within those queues.  ************************************/
typedef  struct bufferDescriptor  {
    void  *vaddr;                /*virtual address representing the buffer passed to device*/
    int(*pFreeRtn)(void *pBuf);  /*free routine*/
}  bufferDescriptor;


/****  device privite informations
       pointed by struct net_device::priv  *********************************/
typedef  struct ep9213Eth_info  {
    /****  static device informations  **********************************/
    struct {
        int                 id;            /*device instance ID (0 for 1st and so on)
                                             must be first element of this structure*/
        receiveDescriptor   *pQueRxDesc;   /*pointer to Rx Descriptor Queue*/
        receiveStatus       *pQueRxSts;    /*pointer to Rx Status Queue*/
        transmitDescriptor  *pQueTxDesc;   /*pointer to Tx Descriptor Queue*/
        transmitStatus      *pQueTxSts;    /*pointer to Tx Status Queue*/
        unsigned char       *pRxBuf;       /*base of Rx Buffer pool*/
        unsigned char       *pTxBuf;       /*base of Tx Buffer pool*/
        unsigned long       phyQueueBase;  /*physical address of device queues*/
        unsigned long       phyQueRxDesc,  /*physical address of Rx Descriptor Queue*/
                            phyQueRxSts,   /*physical address of Rx Status Queue*/
                            phyQueTxDesc,  /*physical address of Tx Descriptor Queue*/
                            phyQueTxSts,   /*physical address of Tx Status Queue*/
                            phyRxBuf,      /*physical address of Rx Buffer pool*/
                            phyTxBuf;      /*physical address of Tx Buffer pool*/
        bufferDescriptor    *pRxBufDesc,   /*info of Rx Buffers*/
                            *pTxBufDesc;   /*info of Tx Buffers*/
        int                 miiIdPhy;      /*MII Bus ID of Ethernet PHY*/
    }  s;
    /****  dynamic information, subject to clear when device open  ******/
    struct {
        struct net_device_stats  stats;  /*statistic data*/
        int  idxQueRxDesc,       /*next processing index of device queues*/
             idxQueRxSts,
             idxQueTxDescHead,
             idxQueTxDescTail,
             idxQueTxSts;
        int  txStopped;          /*flag for Tx condition*/
    }  d;
}  ep9213Eth_info;



/*----------------------------------------------------------------------------
 * Global Variables
 *--------------------------------------------------------------------------*/
static int  numOfInstance = 0;  /*total number of device instance, 0 means the 1st instance.*/

//static struct sk_buff gTxSkb;
//static char gTxBuff[LEN_TxBuf];

static char gTxDataBuff[LEN_QueTxDesc][LEN_TxBuf];

// To know if PHY auto-negotiation has done?
static int gPhyAutoNegoDone=0;

/*----------------------------------------------------------------------------
 * Function Proto-types
 *--------------------------------------------------------------------------*/
int __init  ep93xxEth_probe(struct net_device *pD);
static int  ep9213Eth_open(struct net_device *pD);
static int  ep9213Eth_close(struct net_device *pD);
static int  ep9213Eth_hardStartXmit(struct sk_buff *pSkb, struct net_device *pD);
static void  ep9213Eth_txTimeout(struct net_device *pD);
static void  ep9213Eth_setMulticastList(struct net_device *pD);
static struct net_device_stats*  ep9213Eth_getStats(struct net_device *pD);

static void  ep9213Eth_isr(int irq, void *pDev, struct pt_regs *pRegs);



/*============================================================================
 *
 * Internal Routines
 *
 *==========================================================================*/

/*****************************************************************************
* free_skb()
*****************************************************************************/
static int  free_skb(void *pSkb)
{
    dev_kfree_skb_irq((struct sk_buff*)pSkb);
    return 0;
}/*free_skb()*/



/*****************************************************************************
* waitOnReg32()
*****************************************************************************/
static int  waitOnReg32(struct net_device *pD, int reg,
                        unsigned long mask, unsigned long expect, int tout)
{
    int  i;
    int  dt;

/*    _PRTK_ENTRY(("waitOnReg32(pD:0x%x,reg:0x%x,mask:0x%x,expect:0x%x,tout:%d)\n",
                 pD,reg,mask,expect,tout));
*/

    for(i=0;i<10000;) {
        dt=RegRd32(reg);
        dt=(dt^expect)&mask;
        if(0==dt)  break;
        if(tout)  i++;
    }/*for*/
    return dt;
}/*waitOnReg32()*/



#define  phy_wr(reg,dt)  _phy_write(pD, ((ep9213Eth_info*)pD->priv)->s.miiIdPhy, (reg), (dt))
#define  phy_rd(reg)     _phy_read(pD, ((ep9213Eth_info*)pD->priv)->s.miiIdPhy, (reg))
#define  phy_waitRdy()  waitOnReg32(pD, REG_MIISts,MIISts_Busy, ~MIISts_Busy, 1)

/*****************************************************************************
* _phy_write()
*****************************************************************************/
static void  _phy_write(struct net_device *pD,int idPhy,int reg,U16 dt) {

/*    _PRTK_ENTRY(("_phy_write(pD:0x%p,idPhy:%d,reg:0x%x,dt:0x%x)\n",pD,idPhy,reg,dt));*/

    phy_waitRdy();
    RegWr32(REG_MIIData,dt);
    RegWr32(REG_MIICmd,MIICmd_OP_WR|((idPhy&0x1f)<<5)|((reg&0x1f)<<0));
}/*_phy_write()*/



/*****************************************************************************
* _phy_read()
*****************************************************************************/
static U16  _phy_read(struct net_device *pD,int idPhy,int reg) {
    U16  dt;

/*    _PRTK_ENTRY(("_phy_read(pD:0x%p,idPhy:%d,reg:0x%x)\n",pD,idPhy,reg)); */

    phy_waitRdy();
    RegWr32(REG_MIICmd,MIICmd_OP_RD|((idPhy&0x1f)<<5)|((reg&0x1f)<<0));
    phy_waitRdy();
    dt=(unsigned short)RegRd32(REG_MIIData);

/*    _PRTK_INFO(("_phy_read(): read 0x%x\n",dt));*/

    return dt;
}/*_phy_read()*/



#ifndef _PRTK_DUMP
#define _dbg_phy_dumpReg(pD)
#else
/*****************************************************************************
* _dbg_phy_dumpReg()
*****************************************************************************/
static void  _dbg_phy_dumpReg(struct net_device *pD) {
    _PRTK_DUMP(("Dumping registers of Ethernet PHY\n"));
    _PRTK_DUMP((" pD:0x%p, Eth Base Address:0x%x\n", pD, (unsigned int)pD->base_addr));

    _PRTK_DUMP((" 0-3:0x%04x 0x%04x  0x%04x 0x%04x\n",
                phy_rd(0),phy_rd(1),phy_rd(2),phy_rd(3)));
    _PRTK_DUMP((" 4-6:0x%04x 0x%04x  0x%04x\n",
                phy_rd(4),phy_rd(5),phy_rd(6)));
    _PRTK_DUMP((" 16-19:0x%04x 0x%04x  0x%04x  0x%04x\n",
                phy_rd(16),phy_rd(17),phy_rd(18),phy_rd(19)));
    _PRTK_DUMP((" 20:0x%04x\n",phy_rd(20)));
}/*_dbg_phy_dumpReg()*/
#endif/*ifndef _PRTK_DUMP else*/


/*****************************************************************************
* phy_autoNegotiation()
*****************************************************************************/
static int  phy_autoNegotiation(struct net_device *pD) {
   U16 val;
   U16 oldVal;
   U16 count=0;

   phy_wr(4, 0x01e1);  /*Set 802.3, 100M/10M Full/Half ability*/
   phy_wr(0,(1<<12)|(1<<9)); /* enable auto negotiation*/
   while (1) {
     val = phy_rd(1); /* read BM status Reg*/
     if ( val&0x0020) /* if Auto_Neg_complete?*/
     {
       break;
     }
     else {
       if (count >= 3)
       {
         return -1;
       }
       mdelay(1000);//delay 1 second.
       count++;
     }
   }

   //CS8952 PHY needs the delay.  Otherwise it won't send the 1st frame.
   mdelay(1000);//delay 1 second.

   val = phy_rd(5); /* read ANLPAR Reg*/
   if ( val&0x0140) /* if 100M_FDX or 10M_FDX?*/
   {
     oldVal=RegRd16(REG_TestCTL);
     /*Enable MAC's Full Duplex mode.*/
     RegWr16(REG_TestCTL, oldVal|TestCTL_MFDX); 
     //printk("ep93xx_eth(): set MAC to Full Duplex mode\n");
   } else {
     //printk("ep93xx_eth(): set MAC to Half Duplex mode\n");
   }
   // TODO PJJ-added to get rid of _dbg_phy_dumpReg be sure returning zero is ok.
    gPhyAutoNegoDone=1;

   return(0);
}

/*****************************************************************************
* phy_init()
*****************************************************************************/
static int  phy_init(struct net_device *pD) {
    U32 oldVal;
    int status = -1;
    U16 val;

    _PRTK_ENTRY(("phy_init(pD:0x%x)\n", (unsigned int)pD));

    oldVal = RegRd32(REG_SelfCTL);
    RegWr32(REG_SelfCTL,0x0e00);/*Set MDC clock to be divided by 8
                               and disable PreambleSuppress bit*/

    val = phy_rd(1); /* read BM status Reg; Link Status Bit remains cleared
                        until the Reg is read. */
    val = phy_rd(1); /* read BMStaReg again to get the current link status */
    if ( val&0x0004) /* if Link Status == established? */
    {
      status = phy_autoNegotiation(pD);
    }

    RegWr32(REG_SelfCTL, oldVal);/*restore the old value */

    return status;
}/*phy_init()*/

/*****************************************************************************
* phy_reset()
*****************************************************************************/
#if 0
static int  phy_reset(struct net_device *pD) {
    int  i;

    _PRTK_ENTRY(("phy_reset(pD:0x%p)\n",pD));

    phy_wr(0,1<<15);  /*reset PHY*/
    for(i=0;i<1000;i++)  if(0==((1<<15)&phy_rd(0)))  break;
    if(0!=((1<<15)&phy_rd(0))) {
        _PRTK_HWFAIL(("phy_reset(): PHY reset does not self-clear\n"));
        return -1;
    }/*if*/

    phy_wr(19,0x00);  /*@ override default value from H/W pin that is not correct*/
    phy_wr(4,(1<<8)|(1<<7)|(1<<6)|(1<<5)|(0x01<<0));  /*advertise 100/10M full/half*/
    phy_wr(0,(1<<12)|(1<<9));  /*enable auto negotiation*/

    return 0;
}/*phy_reset()*/
#endif


#ifndef _PRTK_DUMP
# define _dbg_ep9312Eth_dumpQueues(pD)
#else
/*****************************************************************************
* _dbg_ep9312Eth_dumpQueues()
*****************************************************************************/
static void  _dbg_ep9312Eth_dumpQueues(struct net_device *pD) {
//    struct ep9213Eth_info  *pP=pD->priv;
    int  i;

    _PRTK_DUMP(("Dumping Descriptor/Status Queues\n"));
    _PRTK_DUMP((" pD:0x%p, Base Address:0x%x\n",pD,(unsigned int)pD->base_addr));

    _PRTK_DUMP((" o Rx Status Queue: at 0x%p, %d entries\n",pP->s.pQueRxSts,LEN_QueRxSts));
    for(i=0;i<LEN_QueRxSts;i++)
        _PRTK_DUMP(("  - %2d: 0x%08x 0x%08x \n",
                    i,(unsigned int)pP->s.pQueRxSts[i].w.e0, (unsigned int)pP->s.pQueRxSts[i].w.e1));

    _PRTK_DUMP((" o Rx Descriptor Queue: at 0x%p, %d entries\n",pP->s.pQueRxDesc,LEN_QueRxDesc));
    for(i=0;i<LEN_QueRxDesc;i++)
        _PRTK_DUMP(("  - %2d: 0x%08x 0x%08x \n",
                    i, (unsigned int)pP->s.pQueRxDesc[i].w.e0, (unsigned int)pP->s.pQueRxDesc[i].w.e1));

    _PRTK_DUMP((" o Tx Status Queue: at 0x%p, %d entries\n",pP->s.pQueTxSts,LEN_QueTxSts));
    for(i=0;i<LEN_QueTxSts;i++)
        _PRTK_DUMP(("  - %2d: 0x%08x \n",i,(unsigned int)pP->s.pQueTxSts[i].w.e0));

    _PRTK_DUMP((" o Tx Descriptor Queue: at 0x%p, %d entries\n",pP->s.pQueTxDesc,LEN_QueTxDesc));
    for(i=0;i<LEN_QueTxDesc;i++)
        _PRTK_DUMP(("  - %2d: 0x%08x 0x%08x \n",
                    i, (unsigned int)pP->s.pQueTxDesc[i].w.e0, (unsigned int)pP->s.pQueTxDesc[i].w.e1));
}/*_dbg_ep9312Eth_dumpQueues()*/
#endif/*ifndef _PRTK_DUMP else*/


/*****************************************************************************
* devQue_start()
*
  make descriptor queues active
  allocate queue entries if needed
  and set device registers up to make it operational
  assume device has been initialized
* 
*****************************************************************************/
static int  devQue_start(struct net_device *pD) {
    int  err;
    struct ep9213Eth_info  *pP=pD->priv;
    int  i;
    void  *pBuf;
    U32  phyA;

    _PRTK_ENTRY(("devQue_start(pD:0x%p)\n",pD));

    /*turn off device bus mastering*/
    RegWr32(REG_BMCtl,BMCtl_RxDis|BMCtl_TxDis|RegRd32(REG_BMCtl));
    err=waitOnReg32(pD,REG_BMSts,BMSts_TxAct,~BMSts_TxAct,1);
    err|=waitOnReg32(pD,REG_BMSts,BMSts_RxAct,~BMSts_RxAct,1);
    if(err) {
        _PRTK_HWFAIL(("devQue_start(): BM does not stop\n"));
    }/*if*/

    /*Tx Status Queue*/
    memset(pP->s.pQueTxSts, 0, sizeof(pP->s.pQueTxSts[0])*LEN_QueTxSts);  /*clear status*/
    pP->d.idxQueTxSts=0;  /*init index*/
    RegWr32(REG_TxSBA,pP->s.phyQueTxSts);  /*base addr*/
    RegWr32(REG_TxSCA,pP->s.phyQueTxSts);  /*current addr*/
    RegWr16(REG_TxSBL,sizeof(pP->s.pQueTxSts[0])*LEN_QueTxSts);  /*base len*/
    RegWr16(REG_TxSCL,sizeof(pP->s.pQueTxSts[0])*LEN_QueTxSts);  /*current len*/

    /*Tx Descriptor Queue*/
    memset(pP->s.pQueTxDesc, 0, sizeof(pP->s.pQueTxDesc[0])*LEN_QueTxDesc); /*clear descriptor*/
    pP->d.idxQueTxDescHead=pP->d.idxQueTxDescTail=0;  /*init index*/
    RegWr32(REG_TxDBA, pP->s.phyQueTxDesc);  /*base addr*/
    RegWr32(REG_TxDCA, pP->s.phyQueTxDesc);  /*current addr*/
    RegWr16(REG_TxDBL, sizeof(pP->s.pQueTxDesc[0])*LEN_QueTxDesc);  /*base len*/
    RegWr16(REG_TxDCL, sizeof(pP->s.pQueTxDesc[0])*LEN_QueTxDesc);  /*current len*/

    /*Rx Status Queue*/
    memset(pP->s.pQueRxSts, 0, sizeof(pP->s.pQueRxSts[0])*LEN_QueRxSts);  /*clear status*/
    pP->d.idxQueRxSts=0;  /*init index*/
    RegWr32(REG_RxSBA, pP->s.phyQueRxSts);  /*base addr*/
    RegWr32(REG_RxSCA, pP->s.phyQueRxSts);  /*current addr*/
    RegWr16(REG_RxSBL, sizeof(pP->s.pQueRxSts[0])*LEN_QueRxSts);  /*base len*/
    RegWr16(REG_RxSCL, sizeof(pP->s.pQueRxSts[0])*LEN_QueRxSts);  /*current len*/

    /*Rx Descriptor Queue*/
    memset(pP->s.pQueRxDesc, 0, sizeof(pP->s.pQueRxDesc[0])*LEN_QueRxDesc);  /*clear descriptor*/
    phyA=pP->s.phyRxBuf;
    for(i=0;i<LEN_QueRxDesc;i++) {
        pP->s.pQueRxDesc[i].f.bi=i;  /*Rx Buffer Index*/
        pP->s.pQueRxDesc[i].f.ba=phyA;  /*physical address of Rx Buf*/
        pP->s.pQueRxDesc[i].f.bl=LEN_RxBuf;  /*Rx Buffer Length*/
        phyA+=(LEN_RxBuf+3)&~0x03;
    }/*for*/
    pP->d.idxQueRxDesc=0;  /*init index*/
    RegWr32(REG_RxDBA, pP->s.phyQueRxDesc);  /*base addr*/
    RegWr32(REG_RxDCA, pP->s.phyQueRxDesc);  /*current addr*/
    RegWr16(REG_RxDBL, sizeof(pP->s.pQueRxDesc[0])*LEN_QueRxDesc);  /*base len*/
    RegWr16(REG_RxDCL, sizeof(pP->s.pQueRxDesc[0])*LEN_QueRxDesc);  /*current len*/

    /*init Rx Buffer Descriptors*/
    pBuf=pP->s.pRxBuf;
    for(i=0;i<LEN_QueRxDesc;i++) {
        pP->s.pRxBufDesc[i].vaddr=pBuf;  /*system address of buffer*/
        pP->s.pRxBufDesc[i].pFreeRtn=0;  /*no freeing after use*/
        pBuf+=(LEN_RxBuf+3)&~0x03;
    }/*for*/

    /*init Tx Buffer Descriptors*/
    memset(pP->s.pTxBufDesc, 0x0, sizeof(*pP->s.pTxBufDesc)*LEN_QueTxDesc);
	pP->s.pTxBuf = &gTxDataBuff[0][0];
    for(i=0;i<LEN_QueTxDesc;i++) {
        pP->s.pTxBufDesc[i].vaddr=&gTxDataBuff[i][0];  /*system address of buffer*/
        pP->s.pTxBufDesc[i].pFreeRtn=0;  /*no freeing after use*/
    }/*for*/

    /*turn on device bus mastering*/
    RegWr32(REG_BMCtl, BMCtl_TxEn|BMCtl_RxEn|RegRd32(REG_BMCtl));
    err=waitOnReg32(pD, REG_BMSts,BMSts_TxAct|BMSts_TxAct,BMSts_TxAct|BMSts_TxAct,1);
    if(err) {
        _PRTK_HWFAIL(("devQue_start(): BM does not start\n"));
    }/*if*/

    /*Enqueue whole entries; this must be done after BM activation*/
    RegWr32(REG_RxSEQ, LEN_QueRxSts);  /*Rx Status Queue*/
    RegWr32(REG_RxDEQ, LEN_QueRxDesc);  /*Rx Descriptor queue*/

    return 0;
}/*devQue_start()*/



/*****************************************************************************
* devQue_init()
  init device descriptor queues at system level
  device access is not recommended at this point
* 
*****************************************************************************/
static int  devQue_init(struct net_device *pD) {
    struct ep9213Eth_info  *pP=pD->priv;
    void  *pBuf;
    int   size;

    _PRTK_ENTRY(("devQue_init(pD:0x%x)\n", (unsigned int)pD));

    /*verify device Tx/Rx Descriptor/Status Queue data size*/
    if(8 != sizeof(receiveDescriptor)) {
        _PRTK_SWERR(("devQue_init(): size of receiveDescriptor is not 8 bytes!!!\n"));
        return -1;
    }else if(8 != sizeof(receiveStatus)) {
        _PRTK_SWERR(("devQue_init(): size of receiveStatus is not 8 bytes!!!\n"));
        return -1;
    }else if(8 != sizeof(transmitDescriptor)) {
        _PRTK_SWERR(("devQue_init(): size of transmitDescriptor is not 8 bytes!!!\n"));
        return -1;
    }else if(4 != sizeof(transmitStatus)) {
        _PRTK_SWERR(("devQue_init(): size of transmitStatus is not 4 bytes!!!\n"));
        return -1;
    }/*else*/

    /*
      allocate kernel memory for whole queues
      best if non-cached memory block due to DMA access by the device
      if CPU doesn't have bus snooping
    */
    size=sizeof(receiveDescriptor)*(LEN_QueRxDesc+1)+
         sizeof(receiveStatus)*(LEN_QueRxSts+1)+
         sizeof(transmitDescriptor)*(LEN_QueTxDesc+1)+
         sizeof(transmitStatus)*(LEN_QueTxSts+1)+
         sizeof(unsigned long)*4;

    pBuf=MALLOC_DMA(size, &pP->s.phyQueueBase);  /*request memory allocation*/
    if(!pBuf) {
        _PRTK_SYSFAIL(("devQue_initAll(): fail to allocate memory for queues\n"));
        return -1;
    }/*if*/

    /*
      assign memory to each queue
    */
    pP->s.pQueRxDesc = (void*)Align32(pBuf);
    pBuf = (char*)pBuf + sizeof(receiveDescriptor)*(LEN_QueRxDesc+1);
    pP->s.pQueRxSts = (void*)Align32(pBuf);
    pBuf = (char*)pBuf + sizeof(receiveStatus)*(LEN_QueRxSts+1);
    pP->s.pQueTxDesc = (void*)Align32(pBuf);
    pBuf = (char*)pBuf + sizeof(transmitDescriptor)*(LEN_QueTxDesc+1);
    pP->s.pQueTxSts = (void*)Align32(pBuf);
    pBuf = (char*)pBuf + sizeof(transmitStatus)*(LEN_QueTxSts+1);

    /*
      store physical address of each queue
    */
    pP->s.phyQueRxDesc = Align32(pP->s.phyQueueBase);
    pP->s.phyQueRxSts = pP->s.phyQueRxDesc + ((U32)pP->s.pQueRxSts-(U32)pP->s.pQueRxDesc);
    pP->s.phyQueTxDesc = pP->s.phyQueRxDesc + ((U32)pP->s.pQueTxDesc-(U32)pP->s.pQueRxDesc);
    pP->s.phyQueTxSts = pP->s.phyQueRxDesc + ((U32)pP->s.pQueTxSts-(U32)pP->s.pQueRxDesc);

    _PRTK_INFO(("devQue_init(): Rx Descriptor Queue at 0x%p(Phy0x%x), %d entries\n",
                pP->s.pQueRxDesc, (unsigned int)pP->s.phyQueRxDesc,LEN_QueRxDesc));
    _PRTK_INFO(("devQue_init(): Rx Status Queue at 0x%p(Phy0x%x), %d entries\n",
                pP->s.pQueRxSts, (unsigned int)pP->s.phyQueRxSts,LEN_QueRxSts));
    _PRTK_INFO(("devQue_init(): Tx Descriptor Queue at 0x%p(Phy0x%x), %d entries\n",
                pP->s.pQueTxDesc, (unsigned int)pP->s.phyQueTxDesc,LEN_QueTxDesc));
    _PRTK_INFO(("devQue_init(): Tx Status Queue at 0x%p(Phy0x%x), %d entries\n",
                pP->s.pQueTxSts, (unsigned int)pP->s.phyQueTxSts,LEN_QueTxSts));

    /*
      init queue entries
    */
    memset(pP->s.pQueRxDesc, 0, sizeof(receiveDescriptor)*LEN_QueRxDesc);
    memset(pP->s.pQueRxSts, 0, sizeof(receiveStatus)*LEN_QueRxSts);
    memset(pP->s.pQueTxDesc, 0, sizeof(transmitDescriptor)*LEN_QueTxDesc);
    memset(pP->s.pQueTxSts, 0, sizeof(transmitStatus)*LEN_QueTxSts);

    /*Allocate Rx Buffer
      (We might need to copy from Rx buf to skbuff in whatever case,
       because device bus master requires 32bit aligned Rx buffer address 
       but Linux network stack requires odd 16bit aligned Rx buf address)*/
    pP->s.pRxBuf = MALLOC_DMA(((LEN_RxBuf+3)&~0x03)*LEN_QueRxDesc, &pP->s.phyRxBuf);
    if(!pP->s.pRxBuf) { 
        /*@*/
        pP->s.pRxBuf=0;
        _PRTK_SYSFAIL(("devQue_init(): fail to allocate memory for RxBuf\n"));
        return -1;
    }/*if*/


    /*
      allocate kernel memory for buffer descriptors
    */
    size = sizeof(bufferDescriptor)*(LEN_QueRxDesc+LEN_QueTxDesc);
    pBuf = kmalloc(size, GFP_KERNEL);
    if(!pBuf) {
        _PRTK_SYSFAIL(("devQue_initAll(): fail to allocate memory for buf desc\n"));
        return -1;
    }/*if*/
    memset(pBuf,0x0,size);  /*clear with 0*/
    pP->s.pRxBufDesc=pBuf;
    pP->s.pTxBufDesc=pBuf+sizeof(bufferDescriptor)*LEN_QueRxDesc;

    return 0;
}/*devQue_init()*/



#ifndef _PRTK_DUMP
# define _dbg_ep9312eth_dumpReg(pD)
#else
/*****************************************************************************
* _dbg_ep9312eth_dumpReg()
*****************************************************************************/
static void  _dbg_ep9312eth_dumpReg(struct net_device *pD) {
//    struct ep9213Eth_info  *pP = pD->priv;

    _PRTK_DUMP(("Dumping registers of Ethernet Module Embedded within EP9312\n"));
    _PRTK_DUMP((" pD:0x%p, Base Address:0x%x\n",pD,(unsigned int)pD->base_addr));

    _PRTK_DUMP((" RxCTL:0x%08x  TxCTL:0x%08x  TestCTL:0x%08x\n",
                (unsigned int)RegRd32(REG_RxCTL), (unsigned int)RegRd32(REG_TxCTL), (unsigned int)RegRd32(REG_TestCTL)));
    _PRTK_DUMP((" SelfCTL:0x%08x  IntEn:0x%08x  IntStsP:0x%08x\n",
                (unsigned int)RegRd32(REG_SelfCTL), (unsigned int)RegRd32(REG_IntEn), (unsigned int)RegRd32(REG_IntStsP)));
    _PRTK_DUMP((" GT:0x%08x  FCT:0x%08x  FCF:0x%08x\n",
                (unsigned int)RegRd32(REG_GT), (unsigned int)RegRd32(REG_FCT), (unsigned int)RegRd32(REG_FCF)));
    _PRTK_DUMP((" AFP:0x%08x\n", (unsigned int)RegRd32(REG_AFP)));
    _PRTK_DUMP((" TxCollCnt:0x%08x  RxMissCnt:0x%08x  RxRntCnt:0x%08x\n",
                (unsigned int)RegRd32(REG_TxCollCnt), (unsigned int)RegRd32(REG_RxMissCnt), (unsigned int)RegRd32(REG_RxRntCnt)));
    _PRTK_DUMP((" BMCtl:0x%08x  BMSts:0x%08x\n",
                (unsigned int)RegRd32(REG_BMCtl), (unsigned int)RegRd32(REG_BMSts)));
    _PRTK_DUMP((" RBCA:0x%08x  TBCA:0x%08x\n",
                (unsigned int)RegRd32(REG_RBCA), (unsigned int)RegRd32(REG_TBCA)));
    _PRTK_DUMP((" RxDBA:0x%08x  RxDBL/CL:0x%08x  RxDCA:0x%08x\n",
                (unsigned int)RegRd32(REG_RxDBA), (unsigned int)RegRd32(REG_RxDBL), (unsigned int)RegRd32(REG_RxDCA)));
    _PRTK_DUMP((" RxSBA:0x%08x  RxSBL/CL:0x%08x  RxSCA:0x%08x\n",
                (unsigned int)RegRd32(REG_RxSBA), (unsigned int)RegRd32(REG_RxSBL), (unsigned int)RegRd32(REG_RxSCA)));
    _PRTK_DUMP((" RxDEQ:0x%08x  RxSEQ:0x%08x\n",
                (unsigned int)RegRd32(REG_RxDEQ), (unsigned int)RegRd32(REG_RxSEQ)));
    _PRTK_DUMP((" TxDBA:0x%08x  TxDBL/CL:0x%08x  TxDCA:0x%08x\n",
                (unsigned int)RegRd32(REG_TxDBA), (unsigned int)RegRd32(REG_TxDBL), (unsigned int)RegRd32(REG_TxDCA)));
    _PRTK_DUMP((" TxSBA:0x%08x  TxSBL/CL:0x%08x  TxSCA:0x%08x\n",
                (unsigned int)RegRd32(REG_TxSBA), (unsigned int)RegRd32(REG_TxSBL), (unsigned int)RegRd32(REG_TxSCA)));
    _PRTK_DUMP((" TxDEQ:0x%08x\n",(unsigned int)RegRd32(REG_TxDEQ)));
    _PRTK_DUMP((" RxBTH:0x%08x  TxBTH:0x%08x  RxSTH:0x%08x\n",
                (unsigned int)RegRd32(REG_RxBTH), (unsigned int)RegRd32(REG_TxBTH), (unsigned int)RegRd32(REG_RxSTH)));
    _PRTK_DUMP((" TxSTH:0x%08x  RxDTH:0x%08x  TxDTH:0x%08x\n",
                (unsigned int)RegRd32(REG_TxSTH), (unsigned int)RegRd32(REG_RxDTH), (unsigned int)RegRd32(REG_TxDTH)));
    _PRTK_DUMP((" MaxFL:0x%08x  RxHL:0x%08x\n",
                (unsigned int)RegRd32(REG_MaxFL), (unsigned int)RegRd32(REG_RxHL)));
    _PRTK_DUMP((" MACCFG0-3:0x%08x 0x%08x 0x%08x 0x%08x\n",
                (unsigned int)RegRd32(REG_MACCFG0), (unsigned int)RegRd32(REG_MACCFG1), (unsigned int)RegRd32(REG_MACCFG2), (unsigned int)RegRd32(REG_MACCFG3)));
/*
    _PRTK_DUMP((" ---INT Controller Reg---\n"));
    _PRTK_DUMP((" RawIrqSts :0x%08x 0x%08x\n",_RegRd(U32,0x80800004),_RegRd(U32,0x80800018)));
    _PRTK_DUMP((" IrqMask   :0x%08x 0x%08x\n",_RegRd(U32,0x80800008),_RegRd(U32,0x8080001c)));
    _PRTK_DUMP((" MaskIrqSts:0x%08x 0x%08x\n",_RegRd(U32,0x80800000),_RegRd(U32,0x80800014)));
*/

    _PRTK_DUMP(("Dumping private data:\n"));
    _PRTK_DUMP((" d.txStopped:%d  d.idxQueTxSts:%d  d.idxQueTxDescHead:%d  d.idxQueTxDescTail:%d\n",
                pP->d.txStopped,pP->d.idxQueTxSts,pP->d.idxQueTxDescHead,pP->d.idxQueTxDescTail));
    _PRTK_DUMP((" d.idxQueRxDesc:%d  d.idxQueRxSts:%d\n",pP->d.idxQueRxDesc,pP->d.idxQueRxSts));
}/*_dbg_ep9312eth_dumpReg()*/
#endif/*ifndef _PRTK_DUMP else*/



#define CRC_PRIME 0xFFFFFFFF
#define CRC_POLYNOMIAL 0x04C11DB6
/*****************************************************************************
* calculate_hash_index()
*****************************************************************************/
static unsigned char  calculate_hash_index(char *pMulticastAddr) {
   unsigned long CRC;
   unsigned char  HashIndex;
   unsigned char  AddrByte;
   unsigned char *pC;
   unsigned long HighBit;
   int   Byte;
   int   Bit;

   /* Prime the CRC */
   CRC = CRC_PRIME;
   pC=pMulticastAddr;

   /* For each of the six bytes of the multicast address */
   for ( Byte=0; Byte<6; Byte++ )
   {
      AddrByte = *pC;
      pC++;
      /* For each bit of the byte */
      for ( Bit=8; Bit>0; Bit-- )
      {
         HighBit = CRC >> 31;
         CRC <<= 1;

         if ( HighBit ^ (AddrByte & 1) )
         {
            CRC ^= CRC_POLYNOMIAL;
            CRC |= 1;
         }

         AddrByte >>= 1;
      }
   }

   /* Take the least significant six bits of the CRC and copy them */
   /* to the HashIndex in reverse order.                           */
   for( Bit=0,HashIndex=0; Bit<6; Bit++ )
   {
      HashIndex <<= 1;
      HashIndex |= (unsigned char)(CRC & 1);
      CRC >>= 1;
   }

   return HashIndex;
}/*calculate_hash_index()*/

/*****************************************************************************
* eth_setMulticastTbl()
*****************************************************************************/
static void  eth_setMulticastTbl(struct net_device *pD,U8 *pBuf) {
    int  i;
    unsigned char position;
    struct dev_mc_list  *cur_addr;

    memset(pBuf,0x00,8);

    cur_addr = pD->mc_list;
    for(i=0;i<pD->mc_count;i++,cur_addr=cur_addr->next) {

        if(!cur_addr)  break;
        if(!(*cur_addr->dmi_addr&1))  continue;  /*make sure multicast addr*/
        position=calculate_hash_index(cur_addr->dmi_addr);
        pBuf[position>>3]|=1<<(position&0x07);

    }/*for*/
}/*eth_setMulticastTbl()*/


/*****************************************************************************
* eth_indAddrWr()
*****************************************************************************/
static int  eth_indAddrWr(struct net_device *pD,int afp,char *pBuf) {
    U32  rxctl;
    int  i,  len;

    _PRTK_ENTRY(("eth_indAddrWr(pD:0x%x,afp:0x%x,pBuf:0x%x)\n",(unsigned int)pD,afp, (unsigned int)pBuf));

    afp&=0x07;
    if(4==afp || 5==afp) {
        _PRTK_SWERR(("eth_indAddrWr(): invalid afp value\n"));
        return -1;
    }/*if*/
    len=(AFP_AFP_HASH==afp)?8:6;

    _PRTK_INFO(("eth_indAddrWr(): %02x:%02x:%02x:%02x:%02x:%02x%s",
                pBuf[0],pBuf[1],pBuf[2],pBuf[3],pBuf[4],pBuf[5],(6==len)?"\n":":"));
    if(6!=len) {
        _PRTK_INFO(("%02x:%02x\n",pBuf[6],pBuf[7]));
    }/*if*/

    rxctl=RegRd32(REG_RxCTL);    /*turn Rx off*/
    RegWr32(REG_RxCTL,~RxCTL_SRxON&rxctl);
    RegWr32(REG_AFP,afp);  /*load new address pattern*/
    for(i=0;i<len;i++)  RegWr8(REG_IndAD+i,pBuf[i]);
    RegWr32(REG_RxCTL,rxctl);  /*turn Rx back*/

    return 0;
}/*eth_indAddrWr()*/



/*****************************************************************************
* eth_indAddrRd()
*****************************************************************************/
#if 0
static int  eth_indAddrRd(struct net_device *pD,int afp,char *pBuf) {
    int  i,  len;

    _PRTK_ENTRY(("eth_indAddrRd(pD:0x%x,afp:0x%x,pBuf:0x%x)\n", (unsigned int)pD, afp, (unsigned int)pBuf));

    afp&=0x07;
    if(4==afp||5==afp) {
        _PRTK_SWERR(("eth_indAddrRd(): invalid afp value\n"));
        return -1;
    }/*if*/

    RegWr32(REG_AFP,afp);
    len=(AFP_AFP_HASH==afp)?8:6;
    for(i=0;i<len;i++)  pBuf[i]=RegRd8(REG_IndAD+i);

    return 0;
}/*eth_indAddrRd()*/
#endif // 0

/*****************************************************************************
* eth_rxCtl()
*****************************************************************************/
static int  eth_rxCtl(struct net_device *pD,int sw) {

    _PRTK_ENTRY(("eth_rxCtl(pD:0x%p,sw:%d)\n",pD,sw));

/* Workaround for MAC lost 60-byte-long frames:  must enable Runt_CRC_Accept bit */
    RegWr32(REG_RxCTL, sw?RegRd32(REG_RxCTL)| RxCTL_SRxON | RxCTL_RCRCA:
                         RegRd32(REG_RxCTL)&~RxCTL_SRxON);

    return 0;
}/*eth_rxCtl()*/



/*****************************************************************************
* eth_chkTxLvl()
*****************************************************************************/
static void  eth_chkTxLvl(struct net_device *pD) {
    struct ep9213Eth_info  *pP = pD->priv;
    int  idxQTxDescHd;
    int  filled;

    _PRTK_ENTRY_ISR(("eth_chkTxLvl(pD:0x%x)\n", (unsigned int) pD));

    idxQTxDescHd=pP->d.idxQueTxDescHead;

    /*check Tx Descriptor Queue fill-up level*/
    filled=idxQTxDescHd-pP->d.idxQueTxDescTail;
    if(filled<0)  filled += LEN_QueTxDesc;

    if(pP->d.txStopped && filled <= (LVL_TxResume+1)) {
        pP->d.txStopped=0;
        pD->trans_start = jiffies;
        netif_wake_queue(pD);
        _PRTK_INFO_ISR(("eth_chkTxLvl(): Tx Resumed, filled:%d\n",filled));
    }/*if*/
}/*eth_chkTxLvl()*/



/*****************************************************************************
* eth_cleanUpTx()
*****************************************************************************/
static int  eth_cleanUpTx(struct net_device *pD) {
    struct ep9213Eth_info  *pP = pD->priv;
    transmitStatus  *pQTxSts;
    int  idxSts,  bi;

    _PRTK_ENTRY(("eth_cleanUpTx(pD:0x%x)\n", (unsigned int)pD));

    /*process Tx Status Queue (no need to limit processing of TxStatus Queue because
      each queue entry consist of 1 dword)*/
    while(pP->s.pQueTxSts[pP->d.idxQueTxSts].f.txfp) {

        idxSts = pP->d.idxQueTxSts;

        /*ahead Tx Status Queue index*/
        pP->d.idxQueTxSts = IdxNext(pP->d.idxQueTxSts,LEN_QueTxSts);
        pQTxSts = &pP->s.pQueTxSts[idxSts];
        _PRTK_INFO(("eth_cleanUpTx(): QTxSts[%d]:0x%x\n",idxSts,(unsigned int)pQTxSts->w.e0));
        if(!pQTxSts->f.txfp) {  /*empty?*/
            _PRTK_HWFAIL(("eth_cleanUpTx(): QueTxSts[%d]:x%08x is empty\n",idxSts,(int)pQTxSts->w.e0));
            /*@ do recover*/
            return -1;
        }/*if*/
        pQTxSts->f.txfp=0;  /*mark processed*/

        bi=pQTxSts->f.bi;  /*buffer index*/
#if 0
        if(pP->d.idxQueTxDescTail!=bi) {  /*verify BI*/
            _PRTK_HWFAIL(("eth_cleanUpTx(): unmatching QTxSts[%d].BI:%d idxQTxDTail:%d\n",
                          idxSts,bi,pP->d.idxQueTxDescTail));
            /*@ recover*/
        }/*if*/
#endif

        /*free Tx buffer*/
        if(pP->s.pTxBufDesc[bi].pFreeRtn) {
            (*pP->s.pTxBufDesc[bi].pFreeRtn)(pP->s.pTxBufDesc[bi].vaddr);
            pP->s.pTxBufDesc[bi].pFreeRtn=0;
        }/*if*/

        /*statistics collection*/
        if(pQTxSts->f.txwe) {  /*Sent without error*/
            pP->d.stats.tx_packets++;
        }else {  /*Tx failed due to error*/
            _PRTK_INFO(("eth_cleanUpTx(): Tx failed due to error QTxSts[%d]:0x%x\n",
                            idxSts, (unsigned int)pQTxSts->w.e0));
            pP->d.stats.tx_errors++;
            if(pQTxSts->f.lcrs)   pP->d.stats.tx_carrier_errors++;  /*loss of CRS*/
            if(pQTxSts->f.txu) {
                pP->d.stats.tx_fifo_errors++;     /*underrun*/
            }/*if*/
            if(pQTxSts->f.ecoll)  pP->d.stats.collisions++;  /*excessive collision*/
        }/*else*/

        /*ahead Tx Descriptor Queue tail index*/
        pP->d.idxQueTxDescTail=IdxNext(pP->d.idxQueTxDescTail,LEN_QueTxDesc);
    }/*while*/

    return 0;
}/*eth_cleanUpTx()*/



/*****************************************************************************
* eth_restartTx()
*****************************************************************************/
static int  eth_restartTx(struct net_device *pD)
{
    struct ep9213Eth_info  *pP = pD->priv;
    int  i;

    _PRTK_ENTRY(("eth_restartTx(pD:0x%p)\n",pD));

    /*disable int*/
    RegWr32(REG_GIntMsk, RegRd32(REG_GIntMsk)&~GIntMsk_IntEn);  /*turn off master INT control*/

    /*stop Tx and Tx DMA*/
    RegWr32(REG_TxCTL, RegRd32(REG_TxCTL)&~TxCTL_STxON);  /*Tx off*/
    RegWr32(REG_BMCtl, RegRd32(REG_BMCtl)|BMCtl_TxDis);  /*disable Tx DMA*/

    /*reset Tx DMA*/
    RegWr32(REG_BMCtl,BMCtl_TxChR|RegRd32(REG_BMCtl));  /*reset Tx DMA*/

    /*release Tx buffers*/
    for(i=0;i<LEN_QueTxDesc;i++) {
        if(pP->s.pTxBufDesc[i].pFreeRtn) {
            pP->s.pTxBufDesc[i].pFreeRtn(pP->s.pTxBufDesc[i].vaddr);
            pP->s.pTxBufDesc[i].pFreeRtn=0;
        }/*if*/
        pP->d.stats.tx_dropped++;
    }/*for*/

    /*init Tx Queues and flush cache*/
    memset(pP->s.pQueTxSts, 0, sizeof(pP->s.pQueTxSts[0])*LEN_QueTxSts);  /*clear FP flag*/

    /*init variables*/
    pP->d.txStopped=0;
    pP->d.idxQueTxSts=pP->d.idxQueTxDescHead=pP->d.idxQueTxDescTail=0;

    /*init registers*/
    waitOnReg32(pD,REG_BMSts,BMCtl_TxChR,~BMCtl_TxChR,1);  /*wait to finish Tx DMA reset*/
    RegWr32(REG_TxSBA,pP->s.phyQueTxSts);  /*base addr of Tx Status Queue*/
    RegWr32(REG_TxSCA,pP->s.phyQueTxSts);  /*current addr*/
    RegWr16(REG_TxSBL,sizeof(pP->s.pQueTxSts[0])*LEN_QueTxSts);  /*base len*/
    RegWr16(REG_TxSCL,sizeof(pP->s.pQueTxSts[0])*LEN_QueTxSts);  /*current len*/
    RegWr32(REG_TxDBA,pP->s.phyQueTxDesc);  /*base addr of Tx Descriptor Queue*/
    RegWr32(REG_TxDCA,pP->s.phyQueTxDesc);  /*current addr*/
    RegWr16(REG_TxDBL,sizeof(pP->s.pQueTxDesc[0])*LEN_QueTxDesc);  /*base len*/
    RegWr16(REG_TxDCL,sizeof(pP->s.pQueTxDesc[0])*LEN_QueTxDesc);  /*current len*/

    /*start Tx and Tx DMA*/
    RegWr32(REG_TxCTL,RegRd32(REG_TxCTL)|TxCTL_STxON);  /*Tx on*/
    RegWr32(REG_BMCtl,RegRd32(REG_BMCtl)|BMCtl_TxEn);  /*enable Tx DMA*/

    /*enable int again*/
    RegWr32(REG_GIntMsk,RegRd32(REG_GIntMsk)|GIntMsk_IntEn);  /*turn on master INT control*/

    return 0;
}/*eth_restartTx()*/



/*****************************************************************************
* eth_reset()
*****************************************************************************/
static int  eth_reset(struct net_device *pD) {
    int  err;

    _PRTK_ENTRY(("eth_reset(pD:0x%p)\n",pD));

    RegWr8(REG_SelfCTL,SelfCTL_RESET);  /*soft reset command*/
    err=waitOnReg32(pD,REG_SelfCTL,SelfCTL_RESET,~SelfCTL_RESET,1);
    if(err) {
        _PRTK_WARN(("eth_reset(): Soft Reset does not self-clear\n"));
    }/*if*/

    /*reset PHY*/
    //phy_reset(pD);

    return 0;
}/*eth_reset()*/



/*****************************************************************************
 . Function: eth_shutDown()
 . Purpose:  closes down the Ethernet module
 . Make sure to:
 .	1. disable all interrupt mask
 .	2. disable Rx
 .	3. disable Tx
 .
 . TODO:
 .   (1) maybe utilize power down mode.
 .	Why not yet?  Because while the chip will go into power down mode,
 .	the manual says that it will wake up in response to any I/O requests
 .	in the register space.   Empirical results do not show this working.
* 
*****************************************************************************/
static int  eth_shutDown(struct net_device *pD) {

    _PRTK_ENTRY(("eth_shutDown(pD:0x%p)\n",pD));

    eth_reset(pD);
    /*@ power down the Ethernet module*/

    return 0;
}/*eth_shutDown()*/



/*****************************************************************************
*  eth_enable()
 
  Purpose:
        Turn on device interrupt for interrupt driven operation.
        Also turn on Rx but no Tx.
* 
*****************************************************************************/
static int  eth_enable(struct net_device *pD) {

    _PRTK_ENTRY(("eth_enable(pD:0x%p)\n",pD));

    RegWr32(REG_IntEn,Default_IntSrc);  /*setup Interrupt sources*/
    RegWr32(REG_GIntMsk,GIntMsk_IntEn);  /*turn on INT*/
    eth_rxCtl(pD,1);  /*turn on Rx*/

    return 0;
}/*eth_enable()*/



/*****************************************************************************
*  eth_init()
 
  Purpose:
        Reset and initialize the device.
        Device should be initialized enough to function in polling mode.
        Tx and Rx must be disabled and no INT generation.
* 
*****************************************************************************/
static int  eth_init(struct net_device *pD) {
     int status;

    _PRTK_ENTRY(("eth_init(pD:0x%p)\n",pD));

    /*reset device*/
    eth_reset(pD);

    /*init PHY*/
    gPhyAutoNegoDone=0;
    status = phy_init(pD);
    if (status != 0)
    {
      printk(KERN_WARNING "%s: No network cable detected!\n", pD->name);
    }

    /*init MAC*/
    RegWr32(REG_SelfCTL,0x0f00);/*Set MDC clock to be divided by 8
                               and enable PreambleSuppress bit*/
    RegWr32(REG_GIntMsk,0x00);  /*mask Interrupt*/
    RegWr32(REG_RxCTL,RxCTL_BA|RxCTL_IA0);  /*no Rx on at this point*/
    RegWr32(REG_TxCTL,0x00);
    RegWr32(REG_GT,0x00);
    RegWr32(REG_BMCtl,0x00);
    RegWr32(REG_RxBTH,(0x80<<16)|(0x40<<0));  /*Buffer Threshold*/
    RegWr32(REG_TxBTH,(0x80<<16)|(0x40<<0) );
    RegWr32(REG_RxSTH,(4<<16)|(2<<0) );        /*Status Threshold*/
    RegWr32(REG_TxSTH,(4<<16)|(2<<0) );
    RegWr32(REG_RxDTH,(4<<16)|(2<<0) );        /*Descriptor Threshold*/
    RegWr32(REG_TxDTH,(4<<16)|(2<<0) );
    RegWr32(REG_MaxFL,((1518+1)<<16)|(944<<0));  /*Max Frame Length & Tx Start Threshold*/
    
    RegRd32(REG_TxCollCnt);  /*clear Tx Collision Counter*/
    RegRd32(REG_RxMissCnt);  /*clear Rx Miss Counter*/
    RegRd32(REG_RxRntCnt);   /*clear Rx Runt Counter*/

    RegRd32(REG_IntStsC);  /*clear Pending INT*/

    RegWr32(REG_TxCTL,TxCTL_STxON|RegRd32(REG_TxCTL));  /*Tx on*/

    /*Set MAC address*/
    eth_indAddrWr(pD,AFP_AFP_IA0,&pD->dev_addr[0]);

    /*init queue*/
    devQue_start(pD);

    return 0;
}/*eth_init()*/



/*****************************************************************************
*  eth_probe()
 
  Purpose:
        Tests to see if a given base address points to an valid device
        Returns 0 on success
 
  Algorithm:
        No method available now
        Alway returns success
* 
*****************************************************************************/
#if 0
static int  eth_probe(U32 baseA) {

    _PRTK_ENTRY(("eth_probe(baseA:0x%x)\n",(unsigned int)baseA));

    return (portList[0].baseAddr==baseA)?0:-1;  /*no probe method available now except base addr*/
}/*eth_probe()*/
#endif // 0



/*****************************************************************************
*  driver_init()
*
*  Purpose:
*        Logical driver initialization for an individual device
*        Minimum device H/W access at this point
*
*  Task:
*        Initialize the structure if needed
*        print out my vanity message if not done so already
*        print out what type of hardware is detected
*        print out the ethernet address
*        find the IRQ
*        set up my private data
*        configure the dev structure with my subroutines
*        actually GRAB the irq.
*        GRAB the region
* 
*****************************************************************************/
static int __init  driver_init(struct net_device *pD,U32 baseA,int irq) {
    struct ep9213Eth_info  *pP;
    int  err;
    int  i;


    _PRTK_ENTRY(("driver_init(pD:0x%p,baseA:0x%x,irq:%d)\n",pD, (unsigned int)baseA,irq));

    /*print out driver version*/
    if(0==numOfInstance)  printk("ep93xx_eth() version: %s",version);

    /*allocate the private structure*/
    if(0==pD->priv) {
        pD->priv = kmalloc(sizeof(struct ep9213Eth_info),GFP_KERNEL);
        if(0==pD->priv) {
            _PRTK_SYSFAIL((cardname " initDriver(): fail to allocate pD->priv\n"));
            return -ENOMEM;
	}/*if*/
    }/*if*/
    memset(pD->priv,0x00,sizeof(struct ep9213Eth_info));  /*init to 0*/

    /*privite info*/
    pP=pD->priv;
    pP->s.id=numOfInstance;  /*device instance ID*/
    pP->s.miiIdPhy=1;        /*MII Bus ID of PHY device*/

    /*device access info*/
    pD->base_addr  = baseA;     /*base address*/
    pD->irq        = irq;       /*IRQ number*/

    /*Get the Ethernet MAC address from system
      but, do NOT set to the device at this point*/
    for(i=0;i<6;i++)  pD->dev_addr[i]=default_mac[i];
    GET_MAC_ADDR(pD, pD->dev_addr);

    /*Grab the region so that no one else tries to probe this device*/
    err=(int)request_mem_region(baseA,DEV_REG_SPACE,cardname);
    if(!err) {
        _PRTK_SYSFAIL((cardname " initDriver(): unable to get region 0x%x-0x%x (err=%d)\n",
                       (int)baseA,(int)(baseA+DEV_REG_SPACE-1),err));
        return -EAGAIN;
    }/*if*/

    /*Grab the IRQ (support no auto-probing)*/
    _PRTK_INFO(("initDriver(): grabbing IRQ %d\n",pD->irq));
    err = (int)request_irq(pD->irq, &ep9213Eth_isr, 0, cardname, pD);
    if(err) {
        _PRTK_SYSFAIL((cardname " initDriver(): unable to get IRQ %d (err=%d)\n",pD->irq,err));
        return -EAGAIN;
    }/*if*/

    /*init device handler functions*/
    pD->open               = &ep9213Eth_open;
    pD->stop               = &ep9213Eth_close;
    pD->hard_start_xmit    = &ep9213Eth_hardStartXmit;
    pD->tx_timeout         = &ep9213Eth_txTimeout;
    pD->watchdog_timeo     = /*@ HZ/2 */ HZ*5 ;
    pD->get_stats          = &ep9213Eth_getStats;
    pD->set_multicast_list = &ep9213Eth_setMulticastList;

    /*Fill in the fields of the device structure with ethernet values*/
    ether_setup(pD);

    /*init device descriptor/status queues*/
    devQue_init(pD);
    /*reset the device, and put it into a known state*/
    eth_reset(pD);

    /*print out the device info*/
    PRINTK((cardname " #%d at 0x%x IRQ:%d MAC",pP->s.id,(int)pD->base_addr,pD->irq));
    for(i=0;i<6;i++)  PRINTK((":%02x",pD->dev_addr[i]));
    PRINTK(("\n"));

    numOfInstance++;  /*inc Number Of Instance under this device driver*/

    return 0;
}/*driver_init()*/



/*****************************************************************************
* eth_isrRx()
*
*  Interrupt Service Routines
* 
*****************************************************************************/
static int  eth_isrRx(struct net_device *pD)
{
    ep9213Eth_info  *pP = pD->priv;
    receiveStatus   *pQRxSts;
    int  idxQRxStsHead;  /*index of Rx Status Queue Head from device (next put point)*/
    int  idxSts;
    int  cntStsProcessed,  cntDescProcessed;
    char  *pDest;
    struct sk_buff  *pSkb;
    int  len;
    UINT  dt;


    _PRTK_ENTRY_ISR(("eth_isrRx(pD:0x%x)\n", (unsigned int)pD));

    /*get Current Rx Status Queue pointer*/
    dt=RegRd32(REG_RxSCA);
    idxQRxStsHead=(dt-pP->s.phyQueRxSts)/sizeof(pP->s.pQueRxSts[0]);  /*convert to array index*/
    _PRTK_INFO_ISR(("eth_isrRx(): RxSCA:0x%x -> idx:%d\n",dt,idxQRxStsHead));
    if(!(0<=idxQRxStsHead&&idxQRxStsHead<LEN_QueRxSts)) {
        _PRTK_HWFAIL(("eth_isrRx(): invalid REG_RxSCA:0x%x idx:%d (phyQueRxSts:0x%x Len:%x)\n",
                      dt,idxQRxStsHead,(int)pP->s.phyQueRxSts,LEN_QueRxSts));
        /*@ do recover*/
        return -1;
    }/*if*/

    /*process Rx (limit to idxQRxStsHead due to cache)*/
    cntStsProcessed=cntDescProcessed=0;
    while(idxQRxStsHead!=pP->d.idxQueRxSts) {
        idxSts=pP->d.idxQueRxSts;
        pP->d.idxQueRxSts=IdxNext(pP->d.idxQueRxSts,LEN_QueRxSts);
        pQRxSts=&pP->s.pQueRxSts[idxSts];
        _PRTK_INFO_ISR(("eth_isrRx(): QRxSts[%d]:0x%x 0x%x\n",idxSts,(unsigned int)pQRxSts->w.e0, (unsigned int)pQRxSts->w.e1));
        if(!pQRxSts->f.rfp) {  /*empty?*/
            _PRTK_HWFAIL(("eth_isrRx(): QueRxSts[%d] is empty; Hd:%d\n",
                          idxSts,idxQRxStsHead));
            /*@ do recover*/
            return -1;
        }/*if*/
        pQRxSts->f.rfp=0;  /*mark processed*/

        if(pQRxSts->f.eob) {  /*buffer has data*/
            if(pQRxSts->f.bi==pP->d.idxQueRxDesc) {  /*check BI*/
                pP->d.idxQueRxDesc=IdxNext(pP->d.idxQueRxDesc,LEN_QueRxDesc);  /*inc Desc index*/
                cntDescProcessed++;
                if(pQRxSts->f.eof&&pQRxSts->f.rwe) {  /*received a frame without error*/
                    len=pQRxSts->f.fl;
                    _PRTK_INFO_ISR(("eth_isrRx(): Rx Len:%d\n",len));
                    pSkb=dev_alloc_skb(len+5);  /*alloc buffer for protocal stack*/
                    if(NULL!=pSkb) {
                        skb_reserve(pSkb, 2);   /*odd 16 bit alignment to make protocal stack happy*/
                        pSkb->dev=pD;
                        pDest=skb_put(pSkb, len);

                        memcpy( pDest, pP->s.pRxBufDesc[pQRxSts->f.bi].vaddr, len);
                        pSkb->protocol=eth_type_trans(pSkb, pD);
                        netif_rx(pSkb);  /*pass Rx packet to system*/
                        pP->d.stats.rx_packets++;  /*inc Rx stat counter*/
                        if(3==pQRxSts->f.am)  pP->d.stats.multicast++;  /*multicast*/
                    }else {
                        _PRTK_SYSFAIL(("eth_isrRx(): Low Memory, Rx dropped\n"));
                        pP->d.stats.rx_dropped++;
                    }/*else*/
                }else {  /*errored Rx*/
                    _PRTK_INFO_ISR(("eth_isrRx(): errored Rx, QueRxSts[%d]:0x%x/0x%x\n",
                                    idxSts, (unsigned int)pP->s.pQueRxSts[idxSts].w.e0, (unsigned int)pP->s.pQueRxSts[idxSts].w.e1));
                    pP->d.stats.rx_errors++;
                    if(pQRxSts->f.oe)  pP->d.stats.rx_fifo_errors++;  /*overrun*/
                    if(pQRxSts->f.fe)  pP->d.stats.rx_frame_errors++;  /*frame error*/
                    if(pQRxSts->f.runt||pQRxSts->f.edata)  pP->d.stats.rx_length_errors++;  /*inv length*/
                    if(pQRxSts->f.crce)  pP->d.stats.rx_crc_errors++;  /*crc error*/
                }/*else*/
            }else {
                _PRTK_HWFAIL(("eth_isrRx(): unmatching QueRxSts[%d].BI:0x%x; idxQueRxDesc:0x%x\n",
                              idxSts,pQRxSts->f.bi,pP->d.idxQueRxDesc));
            }/*else*/
        }/*if*/

        cntStsProcessed++;
    }/*while*/

    /*enqueue*/
    _PRTK_INFO_ISR(("eth_isrRx(): enqueue QueRxSts:%d QueRxDesc:%d\n",
                    cntStsProcessed,cntDescProcessed));
    RegWr32(REG_RxSEQ, cntStsProcessed);
    RegWr32(REG_RxDEQ, cntDescProcessed);
    
    return 0;    
}/*eth_isrRx()*/



/*****************************************************************************
* eth_isrTx()
*****************************************************************************/
static int  eth_isrTx(struct net_device *pD)
{
    _PRTK_ENTRY_ISR(("eth_isrTx(pD:0x%x)\n",(unsigned int)pD));

    eth_cleanUpTx(pD);
    eth_chkTxLvl(pD);  /*resume Tx if it was stopped*/
    return 0;    
}/*eth_isrTx()*/




/*****************************************************************************
* ep9213Eth_isr()
*****************************************************************************/
static void ep9213Eth_isr(int irq,void *pDev,struct pt_regs *pRegs) {
    struct net_device  *pD = pDev;
    int  lpCnt;
    U32  intS;

    _PRTK_ENTRY_ISR(("ep9213Eth_isr(irq:%d,pDev:0x%x,pRegs:0x%x)\n",
                     irq, (unsigned int)pDev, (unsigned int)pRegs));

    lpCnt=0;
    do {
        intS=RegRd32(REG_IntStsC);  /*get INT status and then clear*/

        /*
        intE=RegRd32(REG_IntEn);  //get INT Source Enable
        intS&=(intE&~0x07)|(IntSts_AHBE|IntSts_OTHER|IntSts_SWI)|
              ((intE&0x07)?IntSts_RxSQ:0x00);
        _PRTK_INFO_ISR(("ep9213Eth_isr(): intS:0x%x intE:0x%x\n",intS,intE));
       */

        if(!intS)  break;  /*no INT*/
        if(IntSts_RxSQ&intS)  eth_isrRx(pD);  /*Rx INT*/
        if(IntSts_TxSQ&intS)  eth_isrTx(pD);  /*Tx INT*/
    }while(lpCnt++ < 64);  /*limit loop to serve other interrupts too*/
}/*ep9213Eth_isr()*/




/*=========================================================
 *  Exposed Driver Routines to the Outside World
 *=======================================================*/

/*****************************************************************************
* ep9213Eth_getStats()
*****************************************************************************/
static struct net_device_stats*  ep9213Eth_getStats(struct net_device *pD)
{
    _PRTK_ENTRY(("ep9213Eth_getStats(pD:0x%x)\n", (unsigned int)pD));

#ifdef _PRTK_DUMP  /*@ for debug*/
    {
        static int  tog=0;
        tog=tog?0:1;
        if(tog) {
            _dbg_ep9312Eth_dumpQueues(pD);
            /*_dbg_phy_dumpReg(pD);*/
            _dbg_ep9312eth_dumpReg(pD);
        }/*if*/
    }
#endif/*ifdef _PRTK_DUMP*/

    return &((struct ep9213Eth_info*)pD->priv)->d.stats;
}/*ep9213Eth_getStats()*/



/*****************************************************************************
* ep9213Eth_setMulticastList()
*****************************************************************************/
static void ep9213Eth_setMulticastList(struct net_device *pD)
{
    U8  tblMulti[8+1];

    _PRTK_ENTRY(("ep9213Eth_setMulticastList(pD:0x%x)\n", (unsigned int)pD));

    if(IFF_PROMISC&pD->flags) {  /*set promiscuous mode*/
        _PRTK_INFO(("ep9213Eth_setMulticastList(): set Promiscuous mode\n"));
        RegWr32(REG_RxCTL,RxCTL_PA|RegRd32(REG_RxCTL));

    }else if(IFF_ALLMULTI&pD->flags) {  /*set to receive all multicast addr*/
        _PRTK_INFO(("ep9213Eth_setMulticastList(): set All-Multicast mode\n"));
        RegWr32(REG_RxCTL,RxCTL_MA|(~RxCTL_PA&RegRd32(REG_RxCTL)));
        eth_indAddrWr(pD,AFP_AFP_HASH,"\xff\xff\xff\xff\xff\xff\xff\xff");

    }else if(pD->mc_count) {  /*set H/W multicasting filter*/
        _PRTK_INFO(("ep9213Eth_setMulticastList(): set Multicast mode\n"));
        RegWr32(REG_RxCTL,RxCTL_MA|(~RxCTL_PA&RegRd32(REG_RxCTL)));
        eth_setMulticastTbl(pD,&tblMulti[0]);
        eth_indAddrWr(pD,AFP_AFP_HASH,&tblMulti[0]);

    }else {  /*no multicasting*/
        _PRTK_INFO(("ep9213Eth_setMulticastList(): no Promiscuous/Multicast\n"));
        RegWr32(REG_RxCTL,~(RxCTL_PA|RxCTL_MA)&RegRd32(REG_RxCTL));
    }/*else*/
}/*ep9213Eth_setMulticastList()*/



/*****************************************************************************
* ep9213Eth_txTimeout()
*****************************************************************************/
static void  ep9213Eth_txTimeout(struct net_device *pD)
{
    int status;

    _PRTK_ENTRY(("ep9213Eth_txTimeout(pD:0x%p)\n",pD));

    /* If we get here, some higher level has decided we are broken.
       There should really be a "kick me" function call instead. */
    _PRTK_WARN(("ep9213Eth_txTimeout(): transmit timed out\n"));

    // Check if PHY Auto negotiation has done?
    // If not, we need to check the Link status.
    // If no network cable is present, just print a error message
    if (gPhyAutoNegoDone == 0) {
       status = phy_init(pD);
       if (status != 0)
       {
         printk(KERN_WARNING "%s: No network cable detected!\n", pD->name);
         return ;
       }
    }

    /*kick Tx engine*/
    eth_restartTx(pD);

    /*ask the Network Stack to resume Tx*/
    pD->trans_start = jiffies;
    netif_wake_queue(pD);
}/*ep9213Eth_txTimeout()*/



/*****************************************************************************
* ep9213Eth_hardStartXmit()
*****************************************************************************/
static int  ep9213Eth_hardStartXmit(struct sk_buff *pSkb,struct net_device *pD) {

/*@swk  to check H/W defect of Tx Underrun Error caused by certain frame length*/

    struct ep9213Eth_info  *pP = pD->priv;
    transmitDescriptor  *pQTxDesc;
    int  idxQTxDescHd;
    int  filled;
    int status;
//	unsigned int *pTemp;

    _PRTK_ENTRY(("ep9213Eth_hardStartXmit(pSkb:0x%x,pD:0x%x)\n",(unsigned int)pSkb, (unsigned int)pD));

    _PRTK_INFO(("ep9213Eth_hardStartXmit(): pSkb->len:0x%x\n",pSkb->len));

    // Check if PHY Auto negotiation has done?
    // If not, we need to check the Link status
    // and have PHY do auto-negotiation.
    // If no network cable is present, just drop the packet.
    if (gPhyAutoNegoDone == 0) {
       status = phy_init(pD);
       if (status != 0)
       {
         return 1;
       }
    }

    idxQTxDescHd = pP->d.idxQueTxDescHead;
    pQTxDesc = &pP->s.pQueTxDesc[idxQTxDescHd];

    /*check Tx Descriptor Queue fill-up level*/
    filled=idxQTxDescHd-pP->d.idxQueTxDescTail;
    if(filled<0)  filled+=LEN_QueTxDesc;
    filled += 1;

    if(/*!pP->d.txStopped&&*/LVL_TxStop <= filled) {  /*check Queue level*/
        netif_stop_queue(pD);  /*no more Tx allowed*/
        pP->d.txStopped=1;
        _PRTK_INFO(("ep9213Eth_hardStartXmit(): Tx STOP requested, filled:%d\n",filled));
        if(LVL_TxStop<filled) {
            /*this situation can not be happen*/
            _PRTK_SYSFAIL(("ep9213Eth_hardStartXmit(): a Tx Request while stop\n"));
            return 1;
        }/*if*/
    }/*if*/

    /*fill up Tx Descriptor Queue entry*/
    if ( pSkb->len < 60 )
    {
       pQTxDesc->f.bl = 60;
	   memset(pP->s.pTxBufDesc[idxQTxDescHd].vaddr, 0, 60);
    }
    else
    {
       pQTxDesc->f.bl = pSkb->len;
    }
    pQTxDesc->f.ba = virt_to_bus(pP->s.pTxBufDesc[idxQTxDescHd].vaddr); 
    pQTxDesc->f.bi = idxQTxDescHd;
    pQTxDesc->f.af = 0;
    pQTxDesc->f.eof = 1;
    _PRTK_INFO(("ep9213Eth_hardStartXmit(): Tx buf:x%08x len:%d QTxD[%d]:x%08x x%08x\n",
                (unsigned int)pSkb->data, pSkb->len, (unsigned int)idxQTxDescHd,(unsigned int)pQTxDesc->w.e0, (unsigned int)pQTxDesc->w.e1));

	//copy data to Tx buffer
    memcpy(pP->s.pTxBufDesc[idxQTxDescHd].vaddr, pSkb->data, pSkb->len);  
    pP->s.pTxBufDesc[idxQTxDescHd].pFreeRtn = 0;

	//Don't need this. Flush Tx buffer into memory
    //dma_cache_wback(pP->s.pTxBufDesc[idxQTxDescHd].vaddr, pQTxDesc->f.bl);

	//Free the data buffer passed by upper layer
	free_skb(pSkb);

    pP->d.idxQueTxDescHead=IdxNext(pP->d.idxQueTxDescHead,LEN_QueTxDesc);  /*ahead Tx Desc Queue*/
    RegWr32(REG_TxDEQ,1);  /*Enqueue a Tx Descriptor to the device*/

    return 0;
}/*ep9213Eth_hardStartXmit()*/



/*****************************************************************************
 . ep9213Eth_close()
 .
 . this makes the board clean up everything that it can
 . and not talk to the outside world.   Caused by
 . an 'ifconfig ethX down'
 * 
*****************************************************************************/

static int  ep9213Eth_close(struct net_device *pD) {

    _PRTK_ENTRY(("ep9213Eth_close(pD:0x%x)\n", (unsigned int)pD));

    netif_stop_queue(pD);
    eth_shutDown(pD);  /*shut device down*/

    MOD_DEC_USE_COUNT;
    return 0;
}/*ep9213Eth_close()*/



/*******************************************************
 * ep9213Eth_open()
 *
 * Open and Initialize the board
 *
 * Set up everything, reset the card, etc ..
 *
 ******************************************************/
static int  ep9213Eth_open(struct net_device *pD) {
    int status;
    struct ep9213Eth_info  *pP = pD->priv;


    _PRTK_ENTRY(("ep9213Eth_open(pD:0x%x)\n",(unsigned int)pD));

    /*clear dynamic device info*/
    memset(&pP->d, 0, sizeof(pP->d));

    MOD_INC_USE_COUNT;

    /*reset/init device*/
    status = eth_init(pD);
    if (status != 0 )
    {
      return -EAGAIN;
    }

    /*turn on INT, turn on Rx*/
    eth_enable(pD);

#if 0  /*@*/
    _dbg_phy_dumpReg(pD);
    _dbg_ep9312eth_dumpReg(pD);
    _dbg_ep9312Eth_dumpQueues(pD);
#endif

    /*link to upper layer*/
    netif_start_queue(pD);

    return 0;
}/*ep9213Eth_open()*/



/*****************************************************************************
 .
 . ep93xxEth_probe( struct net_device * dev )
 .   This is the first routine called to probe device existance
 .   and initialize the driver if the device found.
 .
 .   Input parameters:
 .	dev->base_addr == 0, try to find all possible locations
 .	dev->base_addr == 1, return failure code
 .	dev->base_addr == 2, always allocate space,  and return success
 .	dev->base_addr == <anything else>   this is the address to check
 .
 .   Output:
 .	0 --> there is a device
 .	anything else, error

*****************************************************************************/

int __init  ep93xxEth_probe(struct net_device *pD) {
//    int  err;
//    UINT  baseA;

    
    /*check input arguments*/
    if(0==pD) {
        _PRTK_SWERR(("ep93xxEth_probe(): pD equal 0\n"));
        return -ENXIO;
    }/*if*/

    pD->base_addr=portList[0].baseAddr;
    pD->irq=portList[0].irq;

    _PRTK_ENTRY(("ep93xxEth_probe(pD:0x%x) base_addr=0x%x irq=%d \n", (unsigned int)pD, (unsigned int)pD->base_addr, pD->irq));

    return driver_init(pD, pD->base_addr, pD->irq);

}/*ep93xxEth_probe()*/




#ifdef MODULE

static struct net_device dev_ep9312Eth = { init: ep93xxEth_probe };

static int io;
static int irq;
static int ifport;

MODULE_PARM(io, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(ifport, "i");

/*****************************************************************************
* init_module()
*****************************************************************************/
int init_module(void)
{
	int result;

	if (io == 0)
		printk( "ep93xx_eth: You shouldn't use auto-probing with insmod!\n" );

	/* copy the parameters from insmod into the device structure */
	dev_ep9312Eth.base_addr = io;
	dev_ep9312Eth.irq       = irq;
	dev_ep9312Eth.if_port	= ifport;
	if ((result = register_netdev(&dev_ep9312Eth)) != 0)
		return result;

	return 0;
}

/*****************************************************************************
* cleanup_module()
*****************************************************************************/
void cleanup_module(void)
{
	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	unregister_netdev(&dev_ep9312Eth);

	free_irq(dev_ep9312Eth.irq, &dev_ep9312Eth);
	release_mem_region(dev_ep9312Eth.base_addr, DEV_REG_SPACE);
        

	if (dev_ep9312Eth.priv)
		kfree(dev_ep9312Eth.priv);
}


#endif /* MODULE */
