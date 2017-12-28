/*********************************************************************
 *                
 * Filename:      ep93xx_irda.c
 * Version:       0.2
 * Description:   Driver for the EP93xx SOC IrDA.
 * Status:        Experimental.
 *
 * Copyright 2003 Cirrus Logic, Inc.
 *
 * Based loosely on the ali-ircc.c implementation:
 *
 *      Author:        Benjamin Kong <benjamin_kong@ali.com.tw>
 *      Created at:    2000/10/16 03:46PM
 *      Modified at:   2001/1/3 02:55PM
 *      Modified by:   Benjamin Kong <benjamin_kong@ali.com.tw>
 * 
 *      Copyright (c) 2000 Benjamin Kong <benjamin_kong@ali.com.tw>
 *      All Rights Reserved
 *      
 *      This program is free software; you can redistribute it and/or 
 *      modify it under the terms of the GNU General Public License as 
 *      published by the Free Software Foundation; either version 2 of 
 *      the License, or (at your option) any later version.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 *      Free Software Foundation, Inc.
 *      59 Temple Place, Suite 330 
 *      Boston, MA  02111-1307  USA
 ********************************************************************/

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>
#include <linux/serial_reg.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/byteorder.h>

#include <linux/pm.h>

#include <net/irda/wrapper.h>
#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irlap_frame.h>
#include <net/irda/irda_device.h>

#include <asm/arch/dma.h>
#include <net/irda/ep93xx_irda.h>
#include <asm/arch/irqs.h>

static char *driver_name = "ep93xx-irda";
static char *SIR_str = "SIR only";
static char *SIRMIR_str = "SIR/MIR only";
static char *SIRFIR_str = "SIR/MIR only";
static char *SIRMFIR_str = "SIR/MIR/FIR";


/* 
 * Turnaround time:  
 * Min. time the HW can respond.
 * 
 * Interestingly, most drivers set this value to
 * 0x1.  That would mean the index in bits_to_value
 * would end up with a value of 0 and the min_turn_times
 * array's [0] value is '10000'(us). 
 *
 * 0-7 are the only valid index positions for the 
 * min_turn_times array.  With the bitshift discovery 
 * method used (and the IrDA spec wanting contiguous bits), 
 * only values of 0xFF, 0x7f, 0x3F, 0x1F, 0xF, 0x7, 0x3, 
 * & 0x1 are valid assignments for this variable.
 * 
 * (look at qos.c's min_turn_times array, bits_to_value and
 * msb_index functions to validate)
 */
static int qos_mtt_bits = 0x7;


/*
 * Modes the clock settings will support.
 */
static int g_iClkSupport = 0;


/*
 * Initialization prototypes.
 */
static int ep93xx_irda_init_9312(ep93xx_chip_t *chip);

/*
 * Currently known EP93xx SOC packages.
 */
static ep93xx_chip_t chips[] =
{
    	/* name|irqs|sir dmatx|sir dmarx|mfir dmatx|mfir dmarx|init func */
	{ "EP9312", IRQ_UART2, IRQ_IRDA, DMATx_UART2, DMATx_IRDA, DMARx_IRDA, ep93xx_irda_init_9312 },
	{ NULL }
};

/* 
 * Max 4 instances (as is "standard" for an IrDA driver).
 */
static struct ep93xx_irda_cb *dev_self[] = { NULL, NULL, NULL, NULL };

/* 
 * Prototypes. 
 */

 /*
  * General ep93xx family.
  */
 static int  ep93xx_irda_open(int i, ep93xx_chip_t *chip);
 #ifdef MODULE
 static int  ep93xx_irda_close(struct ep93xx_irda_cb *self);
 #endif /* MODULE */
 static int  ep93xx_irda_setup(ep93xx_chip_t *chip);
 static int  ep93xx_irda_is_receiving(struct ep93xx_irda_cb *self);
 static int  ep93xx_irda_net_init(struct net_device *dev);
 static int  ep93xx_irda_net_open(struct net_device *dev);
 static int  ep93xx_irda_net_close(struct net_device *dev);
 static int  ep93xx_irda_net_ioctl(struct net_device *dev, struct ifreq *rq, 
     int cmd);
 static void ep93xx_irda_change_speed(struct ep93xx_irda_cb *self, 
     __u32 baud);
 static void ep93xx_irda_interrupt(int irq, void *dev_id, 
     struct pt_regs *regs);
#ifdef POWER_SAVING
 static int ep93xx_irda_pmproc(struct pm_dev *dev, pm_request_t rqst, 
		 void *data);
 static void ep93xx_irda_suspend(struct ep93xx_irda_cb *self);
 static void ep93xx _irda_wakeup(struct ep93xx_irda_cb *self);
#endif 
 static struct net_device_stats *ep93xx_irda_net_get_stats(struct net_device *dev);

/* 
 * Driver SIR functions 
 */
static void ep93xx_irda_sir_change_speed(struct ep93xx_irda_cb *priv, 
                                         __u32 speed);
static void ep93xx_irda_sir_interrupt(int irq, struct ep93xx_irda_cb *self, 
                                      struct pt_regs *regs);
static void ep93xx_irda_sir_receive(struct ep93xx_irda_cb *self);
static void ep93xx_irda_sir_write_wakeup(struct ep93xx_irda_cb *self);
static int ep93xx_irda_sir_write(int iFifo_size, __u8 *pBuf, int iLen);


/* 
 * Driver MIR/FIR functions 
 */
static void ep93xx_irda_mfir_change_speed(struct ep93xx_irda_cb *priv, 
                                          __u32 speed);
static void ep93xx_irda_mfir_interrupt(int irq, struct ep93xx_irda_cb *self, 
                                       struct pt_regs *regs);



/*
 * Driver DMA functions
 */
static int  ep93xx_irda_dma_receive(struct ep93xx_irda_cb *self); 
static int  ep93xx_irda_dma_receive_complete(struct ep93xx_irda_cb *self, unsigned int frameSize);
static int  ep93xx_irda_hard_xmit(struct sk_buff *skb, struct net_device *dev);
static void ep93xx_irda_dma_xmit(struct ep93xx_irda_cb *self);
static int  ep93xx_irda_dma_xmit_complete(struct ep93xx_irda_cb *self, int irqID);

/* 
 * State change functions 
 */
static void SIR2MFIR(struct ep93xx_irda_cb *self, __u32 TargetSpeed);
static void MFIR2SIR(struct ep93xx_irda_cb *self, __u32 TargetSpeed);
static void SetInterrupts(struct ep93xx_irda_cb *self, unsigned char enable);
static void SetIR_Transmit(unsigned char ucEnable);
static void SetIR_Receive(unsigned char ucEnable);
static void SetDMA(struct ep93xx_irda_cb *self, unsigned char direction, 
                   unsigned char enable);




/*
 * Local DEFINES 
 */

/*
 * Define to signal usage of PIO mode only for transmits.
 */
#define PIO_TX


/*
 * Define to signal console printing of debug statements.
 */
//#define DEBUG
#ifdef DEBUG

	/*
 	 * DEBUG_LEVEL determines which DEBUG statements to print.
	 */
	#define DEBUG_LEVEL 1
	#define EP93XX_DEBUG(x,y...) (x <= DEBUG_LEVEL) ? printk( ##y ) : 0
#else
	#define EP93XX_DEBUG(x,y...)
#endif


/*
 * Function ep93xx_irda_init ()
 *
 *    Initialize block.  Verify some basic knowns before proceding setup/load.
 *    
 */
int __init 
ep93xx_irda_init(void)
{
    ep93xx_chip_t *chip;
    unsigned long ulRegValue;
    int ret = -ENODEV;
    int i = 0;
	
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);

    /* 
     * Check for all the EP93xx SOC packages we know about .
     */
    for(chip = chips; chip->name; chip++, i++) 
    {
	EP93XX_DEBUG(2, "%s(), Probing for %s ...\n", __FUNCTION__, chip->name);
				
 	/* 
         * Prepare to test some knowns for the SOC. 
         */
        ulRegValue = 0;

        /*
         *   Read the CHIP_ID register to make sure the family is correct.
         */ 
        ulRegValue = inl(SYSCON_CHIPID);
        EP93XX_DEBUG(1, "SYSCON_CHIPID = 0x%x\n", ulRegValue);

        if(chip->name)
	{
	    EP93XX_DEBUG(2, "%s(), Found %s SOC IrDA block, at 0x%03x\n",
				__FUNCTION__, chip->name, IRDA_BASE);					
            /*
             * Resource usage is fixed.  Initialize and open.
             */
            chip->init(chip);
			
	    if(ep93xx_irda_open(i, chip) == 0)
            {
				ret = 0;
            }

	    i++;				
	}
	else
	{
	    EP93XX_DEBUG(2, "%s(), No %s SOC IrDA block found.\n", 
                __FUNCTION__, chip->name);
	}
    }		
		
	EP93XX_DEBUG(2, "%s(), ----------------- End -----------------\n", 
        __FUNCTION__);
	return(ret);
}


/*
 * Function ep93xx_irda_cleanup ()
 *
 *    Close all configured chips
 *
 */
#ifdef MODULE
static void 
ep93xx_irda_cleanup(void)
{
	int i;

	EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);	
	
	pm_unregister_all(ep93xx_irda_pmproc);

	for(i = 0; i < 4; i++) 
        {
		if(dev_self[i])
        	{
			ep93xx_irda_close(dev_self[i]);
         	}
	}
	
	EP93XX_DEBUG(2, "%s(), ----------------- End -----------------\n", 
        __FUNCTION__);
}
#endif /* MODULE */


/*
 * Function ep93xx_irda_open (int i, ep93xx_chip_t *info)
 *
 *    Open driver instance.
 *
 */
static int 
ep93xx_irda_open(int i, ep93xx_chip_t *info)
{
	struct net_device *dev;
	struct ep93xx_irda_cb *self;
#ifdef POWER_SAVING	
	struct pm_dev *pmdev;
#endif	
	int err;
	dma_addr_t rxdmaphys, txdmaphys;
			
	EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);	
	
    /* 
     * Get setup for the attack run... 
     */
    if((ep93xx_irda_setup(info)) == -1)
    {
	return(-1);
    }
		
    /* 
     * Allocate a new instance of the driver 
     */
    self = kmalloc(sizeof(struct ep93xx_irda_cb), GFP_KERNEL);
    if(self == NULL) 
    {
 	ERROR("%s(), can't allocate memory for control block!\n", 
            __FUNCTION__);
	return(-ENOMEM);
    }

    memset(self, 0, sizeof(struct ep93xx_irda_cb));
    spin_lock_init(&self->lock);
   
    /* 
     * Track 'self' pointer 
     */
    dev_self[i] = self;
    self->index = i;

    /* 
     * Save important SOC data, starting with IRQs used 
     */
    self->iSIR_irq = info->iSIR_IRQ;
    self->iMFIR_irq = info->iMFIR_IRQ;

    /* 
     * ep93xx DMA port ids 
     */
    self->ePorts[DMA_SIR_TX] = info->eSIR_DMATx;
    self->ePorts[DMA_MFIR_TX] = info->eMFIR_DMATx;
    self->ePorts[DMA_MFIR_RX] = info->eMFIR_DMARx;
    self->fifo_size = 16;

    /* 
     * Initialize QoS for this device 
     */
    irda_init_max_qos_capabilies(&self->qos);
	
    /* 
     * Override some values returned by max_qos 
     */
    self->qos.baud_rate.bits = 0;
    
    if(g_iClkSupport & CLK_SIR)
    {
       self->qos.baud_rate.bits |= IR_9600|IR_19200|IR_38400|IR_57600|
	       IR_115200;
    }

    if(g_iClkSupport & CLK_MIR)
    {
	self->qos.baud_rate.bits |= IR_576000|IR_1152000;
    }

    if(g_iClkSupport & CLK_FIR)
    {
	self->qos.baud_rate.bits |= (IR_4000000 << 8);
    }
    
    /* 
     * Turnaround time 
     */
    self->qos.min_turn_time.bits = qos_mtt_bits;
			
    irda_qos_bits_to_value(&self->qos);
	
    self->flags = IFF_SIR|IFF_MIR|IFF_FIR|IFF_DMA|IFF_PIO; 	

    /* 
     * Max DMA buffer size needed = (data_size + 6) * (window_size) + 6;
     */
    self->rx_buff.truesize = 14384; 
    self->tx_buff.truesize = 14384;
    
    /* 
     * Allocate memory 
     */
    self->rx_buff.head = (__u8 *)consistent_alloc(GFP_KERNEL|GFP_DMA, 
		    self->rx_buff.truesize, &rxdmaphys);
    
    if(self->rx_buff.head == NULL) 
    {
 	kfree(self);
	return(-ENOMEM);
    }

    memcpy(&self->rx_dmaphysh, &rxdmaphys, sizeof(dma_addr_t));
    memset(self->rx_buff.head, 0, self->rx_buff.truesize);
	
    self->tx_buff.head = (__u8 *) consistent_alloc(GFP_KERNEL|GFP_DMA,
		    self->tx_buff.truesize, &txdmaphys);
    
    if(self->tx_buff.head == NULL) 
    {
	consistent_free(self->rx_buff.head, self->rx_buff.truesize, rxdmaphys);
	kfree(self);
	return(-ENOMEM);
    }

    memcpy(&self->tx_dmaphysh, &txdmaphys, sizeof(dma_addr_t));
    memset(self->tx_buff.head, 0, self->tx_buff.truesize);

    
    /*
     * Conversion buffer.  Temp holding space for DMA buffers.
     * Needed for SIR RX.
     */
    self->conv_buf = (__u8 *) kmalloc(self->rx_buff.truesize,
                        GFP_KERNEL);
    memset(self->conv_buf, 0, self->rx_buff.truesize);

    self->rx_buff.in_frame = FALSE;
    self->rx_buff.state = OUTSIDE_FRAME;
    self->tx_buff.data = self->tx_buff.head;
    self->rx_buff.data = self->rx_buff.head;
	
    /* 
     * Reset Tx queue info 
     */
    self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
    self->tx_fifo.tail = self->tx_buff.head;

    if(!(dev = dev_alloc("irda%d", &err))) 
    {
 	ERROR("%s(), dev_alloc() failed!\n", __FUNCTION__);
	return(-ENOMEM);
    }

    dev->priv = (void *) self;
    self->netdev = dev;
	
    /*
     * Override the network functions we need to use 
     */
    dev->init            = ep93xx_irda_net_init;
    dev->hard_start_xmit = ep93xx_irda_hard_xmit;
    dev->open            = ep93xx_irda_net_open;
    dev->stop            = ep93xx_irda_net_close;
    dev->do_ioctl        = ep93xx_irda_net_ioctl;
    dev->get_stats	     = ep93xx_irda_net_get_stats;

    rtnl_lock();
    err = register_netdevice(dev);
    rtnl_unlock();
	
    if(err) 
    {
	ERROR("%s(), register_netdev() failed!\n", __FUNCTION__);
	return(-1);
    }
   
    MESSAGE("IrDA: Registered device %s\n", dev->name);

#ifdef POWER_SAVING
	pmdev = pm_register(PM_SYS_DEV, PM_SYS_IRDA, ep93xx_irda_pmproc);

    if(pmdev)
    {
            pmdev->data = self;
    }
#endif

    EP93XX_DEBUG(2, "%s(), ----------------- End -----------------\n", 
        __FUNCTION__);
	
    return(0);
}


#ifdef MODULE
/*
 * Function ep93xx_irda_close (self)
 *
 *    Close driver instance.
 *
 */
static int 
ep93xx_irda_close(struct ep93xx_irda_cb *self)
{

    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);

    ASSERT(self != NULL, return -1;);

    /*
     * Remove netdevice 
     */
    if(self->netdev) 
    {
	rtnl_lock();
	unregister_netdevice(self->netdev);
	rtnl_unlock();
    }

    if(self->tx_buff.head)
    {
	consistent_free(self->tx_buff.head, self->tx_buff.truesize, 
			self->tx_dmaphysh);
    }
	
    if(self->rx_buff.head)
    {
	consistent_free(self->rx_buff.head, self->rx_buff.truesize, 
			self->rx_dmaphysh);
    }

    dev_self[self->index] = NULL;
    kfree(self);
	
    EP93XX_DEBUG(2, "%s(), ----------------- End -----------------\n", 
        __FUNCTION__);
	
    return(0);
}
#endif /* MODULE */

/*
 * ep93xx_get_PLL_frequency
 *
 * Given a value for ClkSet1 or ClkSet2, calculate the PLL1 or PLL2 frequency.
 */
static unsigned long ep93xx_get_PLL_frequency( unsigned long ulCLKSET )
{
	ulong ulX1FBD, ulX2FBD, ulX2IPD, ulPS, ulPLL_Freq;

	ulPS = (ulCLKSET & SYSCON_CLKSET1_PLL1_PS_MASK) >> SYSCON_CLKSET1_PLL1_PS_SHIFT; 	
	ulX1FBD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X1FBD1_MASK) >> SYSCON_CLKSET1_PLL1_X1FBD1_SHIFT;
	ulX2FBD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X2FBD2_MASK) >> SYSCON_CLKSET1_PLL1_X2FBD2_SHIFT;
	ulX2IPD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X2IPD_MASK) >> SYSCON_CLKSET1_PLL1_X2IPD_SHIFT;

	/*
	 * This may be off by a very small fraction of a percent.
	 */
	ulPLL_Freq = (((0x00e10000 * (ulX1FBD+1)) / (ulX2IPD+1)) * (ulX2FBD+1)) >> ulPS;

	return ulPLL_Freq;
}

/*
 * Function ep93xx_irda_init_9312 (chip)
 *
 *    Identify the EP9312 SOC's IrDA capabilities
 *    as it is currently configured. 
 */
static int 
ep93xx_irda_init_9312(ep93xx_chip_t *chip) 
{
    unsigned int uiMIRDiv = 0, uiUSBDiv = 0;
    unsigned long ulPLL1, ulPLL2;
       
#define MIRCLK 18432000 
    /*
     * Identify the what modes the clocks will support.
     * 		- SIR will always be supported.
     * 		- MIR depends on processor speed. 
     * 		- FIR should be supported.
     */
    
    /*
     * Dedicated clocks for UARTs, which is what SIR uses.
     */
    g_iClkSupport = CLK_SIR;

	/*
	 * Get the frequency of the two PLLs.
	 */
	ulPLL1 = ep93xx_get_PLL_frequency( inl(SYSCON_CLKSET1) );
	ulPLL2 = ep93xx_get_PLL_frequency( inl(SYSCON_CLKSET2) );

    /*
     * MIR: Check PLL1 clock for one of our supported
     * settings and  setup MIR clock, if the processor speed is
     * MIR friendly.
     * 
     * 0xC000 sets MIR_CLK div on, internal clock
     * source and PLL1 as the source.
     *
     * Need to find pre-clk divisor(2,2.5,3) and 7bit divisor.
     */
    if(!((ulPLL1 / 3) % MIRCLK) && 
		    (((ulPLL1 / 3) / MIRCLK) < 0x7f))
    {
	    uiMIRDiv = (0x3 << 9) | (unsigned int)((ulPLL1 / 3) / MIRCLK);
    } 
    else if(!((unsigned long)((ulPLL1 * 5) / 2) % MIRCLK) &&
		    ((((ulPLL1 * 5) / 2) / MIRCLK) < 0x7f))
    {    
	    uiMIRDiv = (0x2 << 9) | (unsigned int)(((ulPLL1 * 5) / 2) / MIRCLK);
    }
    else if(!((ulPLL1 / 2) % MIRCLK) && 
		    (((ulPLL1 / 2) / MIRCLK) < 0x7f))
    {
	    uiMIRDiv = (0x1 << 9) | (unsigned int)((ulPLL1 / 2) / MIRCLK);
    }

    if(uiMIRDiv > 0)
    {
	    /*
	     * Setup the MIR clock register and flag the 
	     * driver to report MIR support.
	     */
	    uiMIRDiv |=  0xC000;
	    outl(uiMIRDiv, SYSCON_MIRDIV);
	    g_iClkSupport = CLK_MIR; 
    }

    /*
     * FIR: Check PLL2 and make sure it's giving
     * a CLK that the USBDiv can convert to 48mhz
     */
    uiUSBDiv = (inl(SYSCON_CLKSET2) >> 28) + 1;
    if((uiUSBDiv == 0) || (uiUSBDiv == 16))
    {
	uiUSBDiv = 1;
    }
    
    if((ulPLL2 / uiUSBDiv) == 48000000)
    {
	g_iClkSupport |= CLK_FIR;
    }    
	
    return(0);
}


/*
 * Function ep93xx_irda_setup (info)
 *
 *    Attempts to take ownership of UART2, configures for SIR
 *    usage (lowest speed IrDA).
 *
 *	  Returns non-negative on success.
 */
static int 
ep93xx_irda_setup(ep93xx_chip_t *info)
{
    unsigned int uiRegVal;
    char *cptrSupport = NULL;

    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);

    /*
     * Set UART2 to be an IrDA interface.
     */
    uiRegVal = inl(SYSCON_DEVCFG);
    SysconSetLocked(SYSCON_DEVCFG, (uiRegVal | 
			    (SYSCON_DEVCFG_IonU2 | SYSCON_DEVCFG_U2EN)));
	
    uiRegVal = inl(SYSCON_DEVCFG);

    /*
     * Set the pulse width.
     */
    outl(3, UART2ILPR);

    /*
     * Set SIREN bit in UART2 - this enables the SIR encoder/decoder.
     */
    uiRegVal = inl(UART2CR);
    outl((uiRegVal | (U2CR_SIR|U2CR_UART)), UART2CR);
    uiRegVal = 0;

    /*
     * Enable Ir in SIR mode.
     */
    outl(IrENABLE_SIR, IrEnable);
 
    uiRegVal = inl(IrEnable);

    switch(g_iClkSupport & (CLK_SIR|CLK_MIR|CLK_FIR))
    {
	    case 3:cptrSupport = SIRMIR_str; 
		    break;
	    case 5:cptrSupport = SIRFIR_str;
		    break;
	    case 7: cptrSupport = SIRMFIR_str;
		    break;
	
	    default:cptrSupport = SIR_str;
		    break;
    } 

    
    MESSAGE("%s, driver loaded supporting %s (Cirrus Logic, Inc.).\n", 
		    driver_name, cptrSupport);
	
    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
	
	return(0);
}


/*
 * Function ep93xx_irda_interrupt (irq, dev_id, regs)
 *
 *    An interrupt from the chip has arrived. Time to do some work.
 *
 */
static void 
ep93xx_irda_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct ep93xx_irda_cb *self;
		
	EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
		
 	if(!dev) 
    	{
		WARNING("%s: irq %d for unknown device.\n", driver_name, irq);
		return;
	}	

	EP93XX_DEBUG(2, "%s(), for IRQ %d\n", __FUNCTION__, irq);

	self = (struct ep93xx_irda_cb *) dev->priv;
	
	spin_lock(&self->lock);
	
	/* 
	 * Dispatch interrupt handler for the IRQ signaled.
	 */
	if(irq == IRQ_IRDA)
    	{
		ep93xx_irda_mfir_interrupt(irq, self, regs);
	}
	else if(irq == IRQ_UART2)
    	{
		ep93xx_irda_sir_interrupt(irq, self, regs);	
    	} 
	else
	{
		WARNING("%s:  IRQ%d signaled but not recognized\n", 
				driver_name, irq);
	}

	spin_unlock(&self->lock);
	
	EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
}


/*
 * Function ep93xx_irda_mfir_interrupt(irq, struct ep93xx_irda_cb *self, regs)
 *
 *    Handle MIR/FIR interrupt.
 *
 */
static void 
ep93xx_irda_mfir_interrupt(int irq, struct ep93xx_irda_cb *self, 
                          struct pt_regs *regs)
{
    __u8 irqID;
    int iLen, iVal, iWriteBack, iStatus;
	
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);

    /*
     *  Identify the IRQ signal for MIR/FIR.  Branch on speed to
     *  determine which register to check.
     */
    if(self->speed < 4000000)
    {
        /*
         * Should be a MIR interrupt.
         */
        irqID = inl(MIIR);		
    }
    else
    {
        /*
         * Should be a FIR interrupt.
         */
        irqID = inl(FIIR);		
    }

    /*
     * Disable Interrupts
     */
    SetInterrupts(self, FALSE);
        
    if(!irqID)
    {
	EP93XX_DEBUG(1,"MFIR IRQ signalled, but no flag set? (speed = %d)\n", 
			self->speed);
    }

    /*
     * Determine why the interrupt (TX/RX?).  
     */
    if(irqID & (MFIIR_RXFL|MFIIR_RXIL|MFIIR_RXFC|MFIIR_RXFS))
    {
        /*
         * Service the Rx related interrupt.
         */

	/*
	 * Clear our write back value variable.
	 */
	iWriteBack = 0;
	    
        if(irqID & MFIIR_RXFL)
        {
          	EP93XX_DEBUG(1, 
                "%s(), ******* Frame lost (RX) *******\n", __FUNCTION__);

            /*
             * Receive Frame Lost. A ROR occurred at the start of a new frame,
             * before any data for the frame could be placed into the rx FIFO.
             * The last entry in the FIFO already contains a valid EOF bit from
	     * the previous frame and a FIFO overrun has occurred.  The frame is
	     * lost.
             */
	    iWriteBack |= MFISR_RXFL;
        }

        if(irqID & MFIIR_RXIL)
        {
          	EP93XX_DEBUG(1, 
                "%s(), ******* Info buffer lost (RX) *******\n", __FUNCTION__);

            /*
             * Receive Information Buffer Lost.  The last data for the frame
             * has been read from the RX FIFO and the RFC bit is still set
             * from the previous EOF.  The IrRIB register data for the previous
             * frame has therefore been lost.
             *
             */
	    iWriteBack |= MFISR_RXIL;

        }

        if(irqID & MFIIR_RXFC)
        {
          	EP93XX_DEBUG(1, 
                "%s(), ******* Frame complete (RX) *******\n", __FUNCTION__);

            /*
             * Received Frame Complete.  The last data for frame has been
             * read from the FIFO.  IrRIB now has IrFlag and byte count.
             *
             */
            iVal = inw(IrRIB);
            iLen = (iVal&IrRIB_ByteCt) >> 4;
            iStatus = (iVal & (IrRIB_BUFFE | IrRIB_BUFOR | 
				    IrRIB_BUFCRCERR | IrRIB_BUFRXABORT));
	
            EP93XX_DEBUG(1, "%s(), RX Length = 0x%.2x\n", __FUNCTION__, iLen);	
            EP93XX_DEBUG(1, "%s(), RX Status = 0x%.2x (IrRIB = 0x%x)\n", 
			    __FUNCTION__, iStatus, iVal);
		  
            
            /* 
             * Check for errors - the lower 4 bits signal errors.  However,
	     * we ignore Buffered Framing errors, so only check the lower
	     * 3 bits.
             */
            if((iStatus & 0x7) ||(iLen==0))
            {
 	        EP93XX_DEBUG(1, "%s(), ************ RX Errors *********** \n",
			       	__FUNCTION__);
		        
	        /* 
                 * If there was errors, skip the frame.
                 */
	        self->stats.rx_errors++;
		        
	        self->rx_buff.data += iLen;
		        
	        if(iStatus & IrRIB_BUFRXABORT) 
	        {
		        self->stats.rx_frame_errors++;
		        EP93XX_DEBUG(1, 
                        "%s(), ************* ABORT Condition ************ \n", 
                           __FUNCTION__);
	        }
        
	        if(iStatus & IrRIB_BUFFE)
	        {
		        self->stats.rx_frame_errors++;
		        EP93XX_DEBUG(1, 
                         "%s(), ************* FRAME Errors ************ \n", 
                           __FUNCTION__);
	        }
						        
	        if(iStatus & IrRIB_BUFCRCERR) 
	        {
		        self->stats.rx_crc_errors++;
		        EP93XX_DEBUG(1, 
                         "%s(), ************* CRC Errors ************ \n", 
                            __FUNCTION__);
	        }
		        
	        if(iStatus & IrRIB_BUFOR)
	        {
		        self->stats.rx_frame_errors++;
		        EP93XX_DEBUG(1, 
                         "%s(), *********** Overran DMA buffer ********** \n", 
                            __FUNCTION__);
	        }

	        if(iLen == 0)
	        {
		        self->stats.rx_frame_errors++;
		        EP93XX_DEBUG(1, 
                         "%s(), ********* Receive Frame Size = 0 ******** \n", 
                            __FUNCTION__);
	        }
            }
            else
            {
                /*
                 * Looks okay, call handler.
                 */
                ep93xx_irda_dma_receive_complete(self, iLen);
            }
        }

        if(irqID & MFIIR_RXFS)
        {
          	EP93XX_DEBUG(1, 
                "%s(), ******* Buffer SR (RX) *******\n", __FUNCTION__);

            /*
             * Receive Buffer Service Request (RO).  Buffer is not empty and 
             * the receiver is enabled, DMA service request signaled.
             *
             */
        }

	if(self->speed < 4000000)
	{
		outb(iWriteBack, MISR);
	}
	else
	{
		outb(iWriteBack, FISR);
	}
    }

    if(irqID & (MFIIR_TXABORT|MFIIR_TXFC|MFIIR_TXFS))
    {
        /*
         * Service the Tx related interrupt.
         */

        if(irqID & MFIIR_TXABORT)
        {
          	EP93XX_DEBUG(1, 
                "%s(), ******* Frame abort (TX) *******\n", __FUNCTION__);

            /*
             * Transmit Frame Abort.  Transmitted frame has been terminated
             * with an abort.
             *
             */
        }

        if(irqID & MFIIR_TXFC)
        {
          	EP93XX_DEBUG(1, 
                "%s(), ******* Frame complete (TX) *******\n", __FUNCTION__);
            
            /*
             * Transmitted Frame Complete.  The frame has been transmitted and
             * terminated (possibly with an error).
             */
   	    if(ep93xx_irda_dma_xmit_complete(self, irqID))
	    {
		/*
                 * Prepare for receive 
                 */
		ep93xx_irda_dma_receive(self);					
		
	    }
        }

        if(irqID & MFIIR_TXFS)
        {
          	EP93XX_DEBUG(1, 
                "%s(), ******* Buffer SR (TX) *******\n", __FUNCTION__);

            /*
             * Transmit Buffer Service Request (RO).  TX buffer is not full and
             * transmit is enabled, DMA service is signaled.
             */
         }
    }

    /*
     * Restore Interupt
     */
    SetInterrupts(self, TRUE);
    

    EP93XX_DEBUG(1,
	"ISR(), IrEnable = 0x%x, IrCtrl = 0x%x, IrFlag = 0x%x, FIMR = 0x%x\n",
		inb(IrEnable), inb(IrCtrl), inw(IrFlag), inb(FIMR));

    EP93XX_DEBUG(2, "%s(), ----------------- End ---------------\n", 
		    __FUNCTION__);
}

/*
 * Function ep93xx_irda_sir_interrupt(irq, self, eir)
 *
 *    Handle SIR interrupt.
 *
 */
static void 
ep93xx_irda_sir_interrupt(int irq, struct ep93xx_irda_cb *self, 
                          struct pt_regs *regs)
{
    int iIrq, iLSR, iVal;
	
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
	

    iIrq = (char)inl(UART2IIR);

    if(iIrq) 
    {	
		/* 
        	 * Clear interrupt flags 
        	 */
		outl(0, UART2ICR);

		EP93XX_DEBUG(2, "%s(), SIR IRQ (flag=0x%x)\n", 
				__FUNCTION__, iIrq);


        	if(iIrq & U2IICR_RXTOIRQ)
        	{
            		EP93XX_DEBUG(1, "%s(), Receive Timeout IRQ\n", 
					__FUNCTION__);

            		/*
            		 * Check RSR for OE, BE & FE error conditions 
			 * (and clear flags).
            		 */
            		iLSR = inl(UART2RSR);
            		outl(0, UART2ECR);

            		if(iLSR & U2RSR_OvnErr)
            		{
                		EP93XX_DEBUG(1, "%s(), Overrun Error\n", 
						__FUNCTION__);
                
                		/*
                 		 * Make certain we service the RX FIFO.
                 		 */
                		iIrq |= U2IICR_RXIRQ;
            		}
           		else if(iLSR & U2RSR_BrkErr)
            		{
                		EP93XX_DEBUG(1, "%s(), Break Error\n", 
						__FUNCTION__);
                		/*
                		 * Clear the bad byte.
                		 */
                		inb(UART2DR);
            		}
            		else if(iLSR & U2RSR_FrmErr)
            		{
                		EP93XX_DEBUG(1, "%s(), Frame Error\n", 
						__FUNCTION__);
                		/*
                		 * Clear the bad byte.
                		 */
                		inb(UART2DR);
            		}

        	}


	if(iIrq & U2IICR_TXIRQ)
        {
            EP93XX_DEBUG(2, "%s(), Transmit IRQ\n", __FUNCTION__);
            /*
             * Tx'd a char, check Tx FIFO status.
             */
            iVal = inl(UART2FR);
            if(iVal & U2FR_TXFE)
            {
#if U2_DMA_MODE
                /*
                 * TxFIFO is empty, check DMA position vs frame length.
                 */
                ep93xx_irda_dma_xmit_complete(self, iIrq);
#else
                /*
                 * Check if more data to send.
                 */
                ep93xx_irda_sir_write_wakeup(self);
#endif
            }
        }


	if(iIrq & U2IICR_RXIRQ)
        {
	    EP93XX_DEBUG(2, "%s(), Receive IRQ\n", __FUNCTION__);
            ep93xx_irda_sir_receive(self);
        }


        if(iIrq & U2IICR_MDMIRQ)
        {
            EP93XX_DEBUG(1, "%s(), Modem IRQ\n", __FUNCTION__);
            /*
             * Modem status, ignore but notify developer if in debug mode.
             */
        }
    }
    else
    {
        EP93XX_DEBUG(1, "%s(), An interrupt was signaled but not found.\n", 
            __FUNCTION__);
    }

     EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
}


/*
 * Function ep93xx_irda_sir_receive (self)
 *
 *    Pull data from the RX FIFO and parse for
 *    frames.
 *
 */
static void 
ep93xx_irda_sir_receive(struct ep93xx_irda_cb *self) 
{
	int iLoopCt = 0, iRxPkts = 0;
	char cData;
	
	EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
			__FUNCTION__);
	ASSERT(self != NULL, return;);

	iRxPkts = self->stats.rx_packets;
	/*  
	 * Receive all characters in Rx FIFO, unwrap and unstuff them. 
         * async_unwrap_char will deliver all found frames  
	 */
	do {
		cData = inb(UART2DR);
		async_unwrap_char(self->netdev, &self->stats, &self->rx_buff, 
				  cData);

		EP93XX_DEBUG(2,"Byte = 0x%x\n", cData);

 		/* 
		 * Avoid taking too much time in here.
		 */
		if (iLoopCt++ > 32) 
        	{
			EP93XX_DEBUG(1, "%s(), breaking!\n", __FUNCTION__);
			break;
		}
		
		
	} while (!(inl(UART2FR) & U2FR_RXFE));	
	
	if(iRxPkts < self->stats.rx_packets)
	{
		EP93XX_DEBUG(2, "Received %d packets so far...\n", 
				self->stats.rx_packets);
		//mdelay(5);
	}
	
	EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
			__FUNCTION__);	
}



/*
 * Function ep93xx_irda_sir_write_wakeup (tty)
 *
 *    Called by the driver when there's room for more data.  If we have
 *    more packets to send, we send them here.
 *
 */
static void 
ep93xx_irda_sir_write_wakeup(struct ep93xx_irda_cb *self)
{
	int iBytes = 0;
	char cUART = 0;

	ASSERT(self != NULL, return;);

	EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
			__FUNCTION__);

       SetInterrupts(self, FALSE);	
       /* 
        * Finished with frame?  
        */
	if (self->tx_buff.len > 0)  
	{
		/* 
        	 * Write data left in transmit buffer 
        	 */
		iBytes = ep93xx_irda_sir_write(self->fifo_size, 
				      self->tx_buff.data, self->tx_buff.len);
		self->tx_buff.data += iBytes;
		self->tx_buff.len  -= iBytes;
	} 
	else //if(!(inl(UART2FR) & U2FR_BUSY))
	{
		
		if ((self->new_speed > 0)) 
		{
			/* 
             		 * Wait until the TX FIFO is empty. 
            		 */
			do
			{
			    cUART=inl(UART2FR);
  			    EP93XX_DEBUG(2, "Waiting on TX to complete...\n");
			} while (cUART & U2FR_BUSY);
			
			EP93XX_DEBUG(1, 
				"%s(), Changing speed! self->new_speed = %d\n", 
        	        		__FUNCTION__, self->new_speed);
			
			ep93xx_irda_change_speed(self, self->new_speed);
		}
		else
		{
			/*
			 * Notify the stack we're ready for more data.
			 */
			netif_wake_queue(self->netdev);
		}
		
		self->stats.tx_packets++;
		self->direction = DIR_RX;
	}

	SetInterrupts(self,TRUE);
		
	EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
			__FUNCTION__);	
}


/*
 * Function ep93xx_irda_sir_write ()
 *
 *    Fill Tx FIFO with transmit data
 *
 */
static int 
ep93xx_irda_sir_write(int iFifo_size, __u8 *pBuf, int iLen)
{
	int iByteCt = 0;
	
	EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
			__FUNCTION__);
       
	/* 
    	 * Fill TX FIFO with current frame.
    	 */
	while ((iFifo_size-- > 0) && (iByteCt < iLen)) 
    	{
		/*
        	 * Transmit byte 
        	 */
		outl(pBuf[iByteCt], UART2DR);
		EP93XX_DEBUG(2,"Tx Byte = 0x%x\n", pBuf[iByteCt]);
		iByteCt++;
	}
	
        EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
			__FUNCTION__);	
	return(iByteCt);
}


/*
 * Function ep93xx_irda_change_speed
 *
 *    Exposed interface function.  Check speed
 *    and call worker function.
 *
 */
static void 
ep93xx_irda_change_speed(struct ep93xx_irda_cb *self, __u32 baud)
{
	unsigned long ulFlags;
	
	EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
	
    if(self->speed == baud)
    {
        /*
         * Desired speed is configured speed.
         */
        return;
    }

    EP93XX_DEBUG(1, "%s(), setting speed = %d \n", __FUNCTION__, baud);
	
    /*
     * Turn off all interrupts.
     */
    save_flags(ulFlags);
    cli();
		
    if(baud > 115200)
    {
    	/* 
         * Change to MIR/FIR speed 
         */
  	 ep93xx_irda_mfir_change_speed(self, baud);

 	/*
 	 * Ready DMA for receive...
 	 */
 	ep93xx_irda_dma_receive(self);
    }	
    else
    {
        /* 
         * Change to SIR speed 
         */
	ep93xx_irda_sir_change_speed(self, baud);
	
	/*
	 * Ready for receive...
	 */
	self->direction = DIR_RX;
	SetInterrupts(self, TRUE);
    }



    EP93XX_DEBUG(1, 
	    "IrCtrl = 0x%x, IrEnable = 0x%x, FIMR = 0x%x, DMA = 0x%x\n", 
		    inb(IrCtrl), inb(IrEnable), inb(FIMR), inb(IrDMACR));

    /*
     * Enable IRQs.
     */
    restore_flags(ulFlags);


    /*
     * Debug stuff - print packets tx/rx here.
     */
    EP93XX_DEBUG(2,"Packets:  RX = %d; TX = %d\n", self->stats.rx_packets, 
		    self->stats.tx_packets);
    
    /*
     * Notify the upper layers the driver is read to receive packets
     * and to schedule the driver for service.
     */
    netif_wake_queue(self->netdev);	
     	

    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
}


/*
 * Function ep93xx_irda_mfir_change_speed
 *
 *    Adjusts speed for MFIR (possibly needs to handle 
 *    transition from SIR).
 *
 */
static void 
ep93xx_irda_mfir_change_speed(struct ep93xx_irda_cb *self, __u32 speed)
{
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
		
    ASSERT(self != NULL, return;);

    EP93XX_DEBUG(2, "%s(), self->speed = %d, change to speed = %d\n", 
        __FUNCTION__, self->speed, speed);
	
    /* 
     * Call regardless, no point duplicating code.
     */
    SIR2MFIR(self, speed);
		
    /* 
     * Update accounting for new speed 
     */
    self->speed = speed;
    self->new_speed = 0;
		
		
    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
}


/*
 * Function ep93xx_irda_sir_change_speed (self, speed)
 *
 *    Adjusts speed for SIR (possibly needs to handle
 *    transition from MFIR).
 *
 */
static void 
ep93xx_irda_sir_change_speed(struct ep93xx_irda_cb *priv, __u32 speed)
{
    struct ep93xx_irda_cb *self = (struct ep93xx_irda_cb *) priv;
    unsigned int uiRegVal, uiCR_H, uiCR;

    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);

    ASSERT(self != NULL, return;);

    EP93XX_DEBUG(2, "%s(), self->speed = %d, change to speed = %d\n", 
        __FUNCTION__, self->speed, speed);

    /*
     * May be transitioning from MFIR to SIR.
     */
    if(self->speed > 115200)
    {
        MFIR2SIR(self, speed);
    }

    /* 
     * Determine the clock and divisor 
     */
    uiRegVal = inl(SYSCON_PWRCNT);
    if(uiRegVal & SYSCON_PWRCNT_UARTBAUD)
    {
        /* 
         * UART CLK is 14.7456 
         */
	uiRegVal = 14.7456 * 1000000;
    }
    else
    {
        /* 
         * UART CLK is 7.3728 
         */
	uiRegVal = 7.3728 * 1000000;
    }
	
    uiRegVal /= (16*speed);
    uiRegVal -= 1;    
    
    /* 
     * IrDA ports use 8N1 
     */
    uiCR_H = U2CRH_WLen | U2CRH_FEn;
	
    /*
     * Clear all related registers before we update.  Save
     * CR first.
     */
    uiCR = inl(UART2CR);
    
    outb(0, UART2CR);
    outb(0, UART2CR_M);
    outb(0, UART2CR_L);
    outb(0, UART2CR_H);
    
    /*
     * Three write options for these registers:
     *
     *  - UART2CR_L, UART2CR_M, UARTCR_H
     *  - UART2CR_M, UART2CR_L, UARTCR_H
     *  - UART2CR_L || UART2CR_M, UART2CR_H
     */
    outb(((uiRegVal & 0xf00) >> 8), UART2CR_M);
    outb((uiRegVal & 0xff), UART2CR_L);
    outb(uiCR_H, UART2CR_H);
    outb(uiCR, UART2CR);
		
		
    /* 
     * Update accounting for new speed 
     */
    self->speed = speed;
    self->new_speed = 0;

    /*
     * The following is recommended by other implementations to maintain 
     * connection from MIR/FIR speed transition:
     * 
     * 	                set DSR|CTS.
     */
    //uiRegVal = inl(UART2FR) & ~(U2FR_DSR | U2FR_CTS);
    //outl(uiRegVal, UART2FR);
	
    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
}


/*
 * Function ep93xx_irda_net_init (dev)
 *
 *    Initialize network device.
 *
 */
static int 
ep93xx_irda_net_init(struct net_device *dev)
{
	EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
	
	/* 
     * Setup to be a normal IrDA network device driver 
     */
	irda_device_setup(dev);

	EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
	
	return(0);
}

/*
 * Function ep93xx_irda_net_open (dev)
 *
 *    Start the device.
 *
 */
static int 
ep93xx_irda_net_open(struct net_device *dev)
{
    struct ep93xx_irda_cb *self;
    char hwname[64];
    unsigned int uiRegVal;
		
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
	
    ASSERT(dev != NULL, return -1;);
	
    self = (struct ep93xx_irda_cb *) dev->priv;
	
    ASSERT(self != NULL, return 0;);
	
    /* 
     * Request IRQs and install Interrupt Handler for them 
     */
    if(request_irq(self->iSIR_irq, ep93xx_irda_interrupt, 0, dev->name, dev)) 
    {
	WARNING("%s, unable to allocate irq=%d\n", driver_name, 
			self->iSIR_irq);
	return(-EAGAIN);
    }

    if(request_irq(self->iMFIR_irq, ep93xx_irda_interrupt, 0, dev->name, dev)) 
    {
	WARNING("%s, unable to allocate irq=%d\n", driver_name, 
			self->iMFIR_irq);
	return(-EAGAIN);
    }
	
    /*
     * Allocate the DMA channels through the ep9xx special DMA API. Clean up on 
     * failure.  As there is no way to report failure in change_speed requests,
     * get everything we need for all cases now.
     */
    if(ep93xx_dma_request(&(self->iDMAh[DMA_SIR_TX]), driver_name, 
        		self->ePorts[DMA_SIR_TX]))
    {
	ERROR("%s, unable to allocate SIR dma tx channel.\n", driver_name);
	free_irq(self->iSIR_irq, self);
        free_irq(self->iMFIR_irq, self);
	return(-EAGAIN);
    }

    if(ep93xx_dma_config(self->iDMAh[DMA_SIR_TX], IGNORE_CHANNEL_ERROR, 0,
			    0, 0))
    {
	ERROR("%s, unable to config SIR dma tx channel.\n", driver_name);
	ep93xx_dma_free(self->iDMAh[DMA_SIR_TX]);
	free_irq(self->iSIR_irq, self);
	free_irq(self->iMFIR_irq, self);
	return(-EAGAIN);
    }

    if(ep93xx_dma_request(&(self->iDMAh[DMA_MFIR_TX]), driver_name, 
			    self->ePorts[DMA_MFIR_TX]))
    {
	ERROR("%s, unable to allocate MFIR dma tx channel.\n", driver_name);
        ep93xx_dma_free(self->iDMAh[DMA_SIR_TX]);
	free_irq(self->iSIR_irq, self);
        free_irq(self->iMFIR_irq, self);
	return(-EAGAIN);
    }

    if(ep93xx_dma_config(self->iDMAh[DMA_MFIR_TX], IGNORE_CHANNEL_ERROR,
			    0, 0, 0))
    {
	ERROR("%s, unable to configure MFIR dma tx channel.\n", driver_name);
	ep93xx_dma_free(self->iDMAh[DMA_SIR_TX]);
	ep93xx_dma_free(self->iDMAh[DMA_MFIR_TX]);
	free_irq(self->iSIR_irq, self);
	free_irq(self->iMFIR_irq, self);
	return(-EAGAIN);
    }

    if(ep93xx_dma_request(&(self->iDMAh[DMA_MFIR_RX]), driver_name, 
			    self->ePorts[DMA_MFIR_RX]))
    {
	ERROR("%s, unable to allocate MFIR dma rx channel.\n", driver_name);
        ep93xx_dma_free(self->iDMAh[DMA_SIR_TX]);
        ep93xx_dma_free(self->iDMAh[DMA_MFIR_TX]);
	free_irq(self->iSIR_irq, self);
        free_irq(self->iMFIR_irq, self);
	return(-EAGAIN);
    }

    if(ep93xx_dma_config(self->iDMAh[DMA_MFIR_RX], IGNORE_CHANNEL_ERROR,
			    0, 0, 0))
    {
	ERROR("%s, unable to configure MFIR dma rx channel.\n", driver_name);
	ep93xx_dma_free(self->iDMAh[DMA_SIR_TX]);
	ep93xx_dma_free(self->iDMAh[DMA_MFIR_TX]);
	ep93xx_dma_free(self->iDMAh[DMA_MFIR_RX]);
	free_irq(self->iSIR_irq, self);
	free_irq(self->iMFIR_irq, self);
	return(-EAGAIN);
    }
        
    /* 
     * Set start speed - 9600 (need to read clk first) 
     */
    ep93xx_irda_sir_change_speed(self, (__u32)9600);
   
    /* 
     * Turn on recieve interrupts for SIR (starting point) 
     */
    uiRegVal = inl(UART2CR);
    outl(uiRegVal|U2CR_RxIrq, UART2CR);
    self->direction = DIR_RX;

    /*
     * Signal device is ready. 
     */
    netif_start_queue(dev); 
	
    /* 
     * Give self a "hardware" name 
     */
    sprintf(hwname, "EP93XX-S/M/FIR @ 0x%03x", IRDA_BASE);

    /* 
     * Open new IrLAP layer instance, now that everything should be
     * initialized properly 
     */
    self->irlap = irlap_open(dev, &self->qos, hwname);
		
    MOD_INC_USE_COUNT;

    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);

    return(0);
}

/*
 * Function ep93xx_irda_net_close (dev)
 *
 *    Stop the device.
 *
 */
static int 
ep93xx_irda_net_close(struct net_device *dev)
{	
    struct ep93xx_irda_cb *self;
    int i;
				
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
		
    ASSERT(dev != NULL, return -1;);

    self = (struct ep93xx_irda_cb *) dev->priv;
    ASSERT(self != NULL, return 0;);

    /* 
     * Stop device 
     */
    netif_stop_queue(dev);
	
    /*
     *  Stop and remove instance of IrLAP 
     */  
    if (self->irlap)
    {
	irlap_close(self->irlap);
    }

    self->irlap = NULL;
		
    /* 
     * Stop DMA channels & TX/RX. 
     */
    SetDMA(self, DIR_BOTH, FALSE);
    SetIR_Transmit(FALSE);
    SetIR_Receive(FALSE);


    /* 
     * Disable and release interrupts 
     */
    SetInterrupts(self, FALSE);
	       
    free_irq(self->iSIR_irq, dev);
    free_irq(self->iMFIR_irq, dev);

    /*
     * release specialized EP93xx DMA channels 
     */
    for(i = 0; i < DMA_COUNT; i++)
    {
        ep93xx_dma_free(self->iDMAh[i]);
    }

    MOD_DEC_USE_COUNT;

    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
	
    return(0);
}

/*
 * Function ep93xx_irda_hard_xmit(skb, dev)
 *
 *    Prep a frame for Tx, and signal Tx (via ep93xx_irda_xmit).
 *
 */
static int 
ep93xx_irda_hard_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct ep93xx_irda_cb *self;
    unsigned long ulFlags;
    int iLength;
    __u32 speed;
    int mtt;
	
    EP93XX_DEBUG(2, "%s(), ---------------- Start -----------------\n", 
        __FUNCTION__);
    self = (struct ep93xx_irda_cb *) dev->priv;

    SetDMA(self, self->direction, FALSE);
    SetInterrupts(self, FALSE);

    /*
     * Getting ready to TX, so turn off RX.
     */
    //SetIR_Receive(FALSE);
  
    /* 
     * Check if we need to change the speed 
     */
   speed = irda_get_next_speed(skb);
   if((speed != self->speed) && (speed != -1)) 
   {
	   EP93XX_DEBUG(1,"Speed change queued (%d)\n", speed);
	   self->new_speed = speed;
   }
	  
    /* 
     * Check for empty frame 
     */
    if((skb->len == 0) && (self->new_speed > 0)) 
    {
	EP93XX_DEBUG(1, "%s(), Empty frame for speed change.\n", __FUNCTION__);
	ep93xx_irda_change_speed(self, speed); 
	dev_kfree_skb(skb);
	EP93XX_DEBUG(2,"%s(), ----------- SpeedOptOut ---------\n", 
			__FUNCTION__);
	return(0);
    } 
  
   spin_lock_irqsave(&self->lock, ulFlags);

   if((unsigned long)self->tx_fifo.tail % 0x10)
   {
	EP93XX_DEBUG(1, "Buffer not aligned?\n");
   }
   
   /* 
    * Register and copy this frame to DMA memory (do some alignment first).
    */
   self->tx_fifo.queue[self->tx_fifo.free].start = self->tx_fifo.tail;

   if(self->speed <= 115200)
   {
        /* 
         * SIR: Copy skb to tx_fifo while wrapping, stuffing and making CRC.
         */
         netif_stop_queue(dev);
	 self->tx_buff.data = self->tx_buff.head;
	 iLength = async_wrap_skb(skb, self->tx_buff.data, 
			 self->tx_buff.truesize);
	 self->tx_buff.len = iLength;
   }
   else
   {
	EP93XX_DEBUG(1,"MFIR XMIT : %d bytes\n", skb->len);
	memcpy(self->tx_fifo.queue[self->tx_fifo.free].start,
			skb->data, skb->len);

	iLength = skb->len;
	self->tx_fifo.queue[self->tx_fifo.free].len = iLength;
	self->tx_fifo.tail += iLength;
	self->tx_fifo.len++;
	self->tx_fifo.free++;
   }
  
   self->stats.tx_bytes += iLength;

   /* 
    * Start transmit (if not currently)
    */
   if(((self->tx_fifo.len == 1) || (iLength > 0))) 
   {

	self->direction = DIR_TX;
        /* 
         * Check if we must wait the min turn time or not 
         */
	mtt = irda_get_mtt(skb);
	if(mtt) 
	{
		EP93XX_DEBUG(1, "%s(), ******* delay = %d ******* \n", 
                    __FUNCTION__, mtt);

		mdelay(mtt);
        }
	
	/*
	 * Enable TX
	 */
	SetIR_Transmit(TRUE);

	
   	/* 
    	 * Enable appropriate interrupts.
    	 */
   	SetInterrupts(self, TRUE);
		
    	/* 
    	 * Transmit frame, if MIR/FIR.
    	 */
	if(self->speed > 115200)
	{
	  	ep93xx_irda_dma_xmit(self);
	}
   } 

   
   /* 
    * Not busy transmitting if window is not full 
    */
   if((self->tx_fifo.free < MAX_TX_WINDOW) && (self->speed > 115200))
   {
	netif_wake_queue(self->netdev);
   }
	
   spin_unlock_irqrestore(&self->lock, ulFlags);
   dev_kfree_skb(skb);

   EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
   return(0);
}


/*
 * Function ep93xx_irda_dma_xmit(self)
 *  
 *    Start DMA of TX buffer.
 *  
 */
static void 
ep93xx_irda_dma_xmit(struct ep93xx_irda_cb *self)
{
    int i;
#ifndef PIO_TX
    int iResult;
#endif    
    unsigned long *ulDebug;
    char *cDebug;
		
    EP93XX_DEBUG(1, "%s(), ---------------- Start -----------------\n", 
        __FUNCTION__);	

    //MESSAGE("DMA_TX start\n");
    
    /* 
     * Disable DMA 
     */
    //SetDMA(self, DIR_BOTH, FALSE);
    //self->direction = DIR_TX;
    
#ifdef PIO_TX   
    ulDebug = (unsigned long *)self->tx_fifo.queue[self->tx_fifo.ptr].start;
    cDebug = (char *)self->tx_fifo.queue[self->tx_fifo.ptr].start + 
	    self->tx_fifo.queue[self->tx_fifo.ptr].len;

    // hit the tail register if necessary
    switch(self->tx_fifo.queue[self->tx_fifo.ptr].len % 4)
    {
	    case 3: cDebug -= 3;
		    outl(*((unsigned long *)cDebug), IrDataTail1);
		    break;
	    case 2: cDebug -= 2;
		    outl(*((unsigned long *)cDebug), IrDataTail2);
		    break;
            case 1: cDebug -= 1;
		    outl(*((unsigned long *)cDebug), IrDataTail3);
		    break;

            default: EP93XX_DEBUG(1, "No tail data.\n");
		    break;
    }
    SetDMA(self, self->direction, TRUE);
    for(i = 0; i < (self->tx_fifo.queue[self->tx_fifo.ptr].len / 4); i++)
    {
	/*
	 * Wait for a SR to be flagged.
	 */
 	while(!(inb(FISR) & MFISR_TXSR))
	{
		mdelay(1);
	}	
	
	outl(ulDebug[i], IrData);
    }

    EP93XX_DEBUG(1,"XMIT wrote %d bytes\n", 
	    ((i * 4) + self->tx_fifo.queue[self->tx_fifo.ptr].len % 4));
#else    
    
    if(self->speed <= 115200)
    {
        iResult = ep93xx_dma_add_buffer(self->iDMAh[DMA_SIR_TX], 
            (unsigned int)self->tx_fifo.queue[self->tx_fifo.ptr].start, 0, 
                self->tx_fifo.queue[self->tx_fifo.ptr].len, 0, 
                    self->iDMAh[DMA_SIR_TX]);
    }
    else
    {
	    
        iResult = ep93xx_dma_add_buffer(self->iDMAh[DMA_MFIR_TX], 
            (unsigned int)self->tx_fifo.queue[self->tx_fifo.ptr].start, 0, 
                self->tx_fifo.queue[self->tx_fifo.ptr].len, 1, 
                    self->iDMAh[DMA_MFIR_TX]);
    }
	
    if(iResult < 0)
    {
	EP93XX_DEBUG(1,"%s(), DMA add buffer failed (%d)\n", 
			__FUNCTION__, iResult);
    }

    
    /* 
     * Enable DMA. 
     */
    SetDMA(self, self->direction, TRUE);
#endif
   
    EP93XX_DEBUG(1, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
}


/*
 * Function ep93xx_irda_dma_xmit_complete(self)
 *
 *    Clean up after TX.
 *
 */
static int 
ep93xx_irda_dma_xmit_complete(struct ep93xx_irda_cb *self, int irqID)
{
    int ret = TRUE;
#ifndef PIO_TX    
    int iDMAh, iBufID, iTotalBytes;
#endif    
	
    EP93XX_DEBUG(1, "%s(), ---------------- Start -----------------\n", 
        __FUNCTION__);


    /*
     * Disable DMA.
     */
    SetDMA(self, DIR_TX, FALSE);
    
    /*
     * Don't proceed until the transmitter goes idle.
     */
    do
    {
	mdelay(1); //new
    }
    while((inl(IrFlag) & IrFLAG_TXBUSY));

#ifndef PIO_TX
    /*
     * Check for underrun. Due to SIP requirement and HW behavior, we'll
     * need to check the DMA position to try and verify an underrun
     * occured.
     */
    if(self->speed < 115200)
    {
        iDMAh = self->iDMAh[DMA_SIR_TX];
    }
    else
    {
        iDMAh = self->iDMAh[DMA_MFIR_TX];
    }

    ep93xx_dma_remove_buffer(iDMAh, &iBufID);
    
    ep93xx_dma_get_position(iDMAh, &iBufID, &iTotalBytes, 0);
    
    if((iTotalBytes < self->tx_fifo.queue[self->tx_fifo.ptr].len) && 
		    (!(irqID & MFIIR_TXABORT)))
    {
	EP93XX_DEBUG(1,"TX Complete found a problem.\n");
        self->stats.tx_errors++;
 	self->stats.tx_fifo_errors++;		
    }
    else
    {

	self->stats.tx_packets++;
    }
#else
    if(!(irqID & MFIIR_TXABORT))
    {
	    /*
	     * "Assume" everything went well.
	     */
	    self->stats.tx_packets++;
    } 
    else
    {
	    self->stats.tx_errors++;
	    self->stats.tx_fifo_errors++;
    }
#endif
    
    /* 
     * Check if we need to change the speed 
     */
    if(self->new_speed) 
    {
	EP93XX_DEBUG(1,"%s(), speed change queued - changing\n",
			__FUNCTION__);
        ep93xx_irda_change_speed(self, self->new_speed);
	self->new_speed = 0;
    }

    /* 
     * Finished with this frame, so prepare for next 
     */
    self->tx_fifo.ptr++;
    self->tx_fifo.len--;

    /* 
     * Any frames to be sent back-to-back? 
     */
   if(self->tx_fifo.len) 
   {
	ep93xx_irda_dma_xmit(self);
		
	/*
         * Not finished yet! 
         */
	ret = FALSE;
   } 
   else 
   {	

        /*
	 * Disable TX
	 */
	SetIR_Transmit(FALSE); //new
	   
        /* 
         * Reset Tx FIFO info 
         */
	self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
	self->tx_fifo.tail = self->tx_buff.head;
#ifndef PIO_TX
        /*
         * Flush the TX DMA (clears totalbytes for one thing).  
         */
        ep93xx_dma_flush(iDMAh);
#endif	
    }

    /* 
     * Make sure we have room for more frames 
     */
    if(self->tx_fifo.free < MAX_TX_WINDOW) 
    {
	/* 
         * Not busy transmitting anymore.  Tell the network layer, 
         * that we can accept more frames 
         */
	netif_wake_queue(self->netdev);
	
    }
		

    /*
     * Re-start the DMA
     */
    //SetDMA(self, self->direction, TRUE);
    EP93XX_DEBUG(1, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
    return(ret);
}

/*
 * Function ep93xx_irda_dma_receive (self)
 *
 *    Get ready to receive a frame. The device will initiate a DMA
 *    if it starts to receive a frame.
 *
 */
static int 
ep93xx_irda_dma_receive(struct ep93xx_irda_cb *self) 
{
    int iResult;

    EP93XX_DEBUG(2, "%s(), ---------------- Start -----------------\n", 
        __FUNCTION__);

    /* 
     * Reset the Tx FIFO info 
     */
    self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
    self->tx_fifo.tail = self->tx_buff.head;
		
    /* 
     * Disable DMA until we're ready.
     */
    SetDMA(self, DIR_RX, FALSE);

		
    /* 
     * Reset Rx FIFO info 
     */
    self->direction = DIR_RX;
    self->rx_buff.data = self->rx_buff.head;
		
    /* 
     * Reset Rx FIFO 
     */
    self->st_fifo.len = self->st_fifo.pending_bytes = 0;
    self->st_fifo.tail = self->st_fifo.head = 0;
    
    /*
     * Add the buffer to the DMA queue.
     */
    iResult = ep93xx_dma_add_buffer(self->iDMAh[DMA_MFIR_RX], 
		    (unsigned int)self->rx_buff.data, 0, 
		    	self->rx_buff.truesize, 1, self->iDMAh[DMA_MFIR_RX]);

    if(iResult < 0)
    {
	EP93XX_DEBUG(1,"DMARx failed to add buffer\n");
    }

    /* 
     * Enable RX & DMA
     */
    SetIR_Receive(TRUE);
    SetDMA(self, DIR_RX, TRUE);

    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
    return(0);
}


/*
 * Function ep93xx_irda_dma_receive_complete(self, size)
 *
 *    Check for complete frame.  Massage as needed and
 *    put it on the stack if it's "good".
 *
 */
static int 
ep93xx_irda_dma_receive_complete(struct ep93xx_irda_cb *self, 
		unsigned int frameSize)
{
    struct st_fifo *st_fifo;
    struct sk_buff *skb;
    int iDMAh, iBufID, iTotalBytes;	
    
    EP93XX_DEBUG(2, "%s(), ---------------- Start -----------------\n", 
        __FUNCTION__);
    
    /*
     * Stop DMA.
     */  
    SetDMA(self, self->direction, FALSE);
    
    st_fifo = &self->st_fifo;		

    iDMAh = self->iDMAh[DMA_MFIR_RX];
    if(ep93xx_dma_remove_buffer(iDMAh, &iBufID) == -EINVAL)
    {
	EP93XX_DEBUG(1, "DMARx failed to remove buffer?\n");
    }
   
    if(ep93xx_dma_get_position(iDMAh, &iBufID, &iTotalBytes, 0) == -EINVAL)
    {
        /*
         * This can currently only happen if the handle is bad.
         */
	ERROR("%s(), ********* DMA_GET_POS FAILED *********\n", 
            __FUNCTION__);
    }

    /*
     * Flush the channel.
     */
    ep93xx_dma_flush(iDMAh);
    
    if((frameSize > 2047) && (self->speed > 115200))
    {
        /*
         * By IrDA protocol, this is invalid.
         */
	EP93XX_DEBUG(1, "%s(), *******Rx frame size > 2047!****** \n", 
			__FUNCTION__);
    
        self->stats.rx_errors++;
	self->rx_buff.data += frameSize;
        return(FALSE);
    }

    skb = dev_alloc_skb(frameSize+1);
    if(skb == NULL)  
    {
	WARNING("%s(), memory squeeze, dropping frame.\n", 
               __FUNCTION__);
   	self->stats.rx_dropped++;
  	return(FALSE);
    }
	
    /* 
     * Make sure IP header gets aligned 
     */
    skb_reserve(skb, 1); 

    /* 
     * HW does not stuff CRC32 into RX buffer.
     */
     skb_put(skb, frameSize);
     memcpy(skb->data, self->rx_buff.data, frameSize);

     /* 
      * Move to next frame 
      */
     self->rx_buff.data += frameSize;
     self->stats.rx_bytes += frameSize;
     self->stats.rx_packets++;

     skb->dev = self->netdev;
     skb->mac.raw  = skb->data;
     skb->protocol = htons(ETH_P_IRDA);
     netif_rx(skb);       

     //mdelay(5);

     EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
     return(TRUE);
}

/*
 * Function ep93xx_irda_net_ioctl(dev, rq, cmd)
 *
 *    Process IOCTL commands for this device.
 *
 */
static int 
ep93xx_irda_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{ 
    struct if_irda_req *irq = (struct if_irda_req *) rq;
    struct ep93xx_irda_cb *self;
    unsigned long ulFlags;
    int iRet = 0;
	
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
	
    ASSERT(dev != NULL, return -1;);

    self = dev->priv;

    ASSERT(self != NULL, return -1;);

    EP93XX_DEBUG(2, "%s(), %s, (cmd=0x%X)\n", __FUNCTION__, dev->name, cmd);
	
    switch(cmd) 
    {
	
        case SIOCSBANDWIDTH: 
            {
                /* Set bandwidth */
	        EP93XX_DEBUG(1, "%s(), SIOCSBANDWIDTH\n", __FUNCTION__);
	        /* Root only */
	        if (!capable(CAP_NET_ADMIN))
               	{
		        return(-EPERM);
               	}
	        ep93xx_irda_change_speed(self, irq->ifr_baudrate);		
	        break;
            }

	    case SIOCSMEDIABUSY: 
            {
                /* Set media busy */
	        EP93XX_DEBUG(2, "%s(), SIOCSMEDIABUSY\n", __FUNCTION__);
	        if (!capable(CAP_NET_ADMIN))
                {
		        return(-EPERM);
                }
	        irda_device_set_media_busy(self->netdev, TRUE);
	        break;
            }

	    case SIOCGRECEIVING: 
            {
                /* Check if we are receiving right now */
	        EP93XX_DEBUG(2, "%s(), SIOCGRECEIVING - %d\n", __FUNCTION__, 
					self->speed);
	        /* Is it really needed ? And what about spinlock ? */
	        save_flags(ulFlags);
	        cli();	

	        irq->ifr_receiving = ep93xx_irda_is_receiving(self);
	        restore_flags(ulFlags);
	        break;
            }

	    default:
            {
		        iRet = -EOPNOTSUPP;
            }
    }

    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
	
    return(iRet);
}


/*
 * Function ep93xx_irda_is_receiving(self)
 *
 *    Return TRUE is we are currently receiving a frame
 *
 */
static int 
ep93xx_irda_is_receiving(struct ep93xx_irda_cb *self)
{
    unsigned long ulFlags;
    int iStatus = FALSE;
	
    EP93XX_DEBUG(2, "%s(), ---------------- Start -----------------\n", 
        __FUNCTION__);
	
    ASSERT(self != NULL, return FALSE;);

    spin_lock_irqsave(&self->lock, ulFlags);

    if(self->speed > 115200) 
    {
    	EP93XX_DEBUG(1, "%s(), check MFIR.\n", __FUNCTION__);
 	if((inl(IrFlag) & IrFLAG_RXINFRM) != 0) 		
	{
		/* 
		 * We are receiving something 
		 */
		EP93XX_DEBUG(1, "%s(), We are receiving something\n", 
                		__FUNCTION__);
		iStatus = TRUE;
	}
    } 
    else
    { 
	iStatus = (self->rx_buff.state != OUTSIDE_FRAME);
    }
	
    spin_unlock_irqrestore(&self->lock, ulFlags);

    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
	
    return(iStatus);
}


/*
 * Function ep93xx_irda_net_get_stats(dev)
 *
 *    Return stats structure.
 *
 */
static struct net_device_stats *
ep93xx_irda_net_get_stats(struct net_device *dev)
{
    struct ep93xx_irda_cb *self = (struct ep93xx_irda_cb *) dev->priv;
	
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
		
    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
	
    return(&self->stats);
}

#ifdef POWER_SAVING
/*
 * Function ep93xx_irda_suspend(self)
 *
 *    Called by the OS to indicate the system
 *    is being suspended for PM.  Shut our hardware
 *    down.
 *
 */
static void 
ep93xx_irda_suspend(struct ep93xx_irda_cb *self)
{
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
	
    MESSAGE("%s, Suspending\n", driver_name);

    if(self->suspended)
    {
	return;
    }

    ep93xx_irda_net_close(self->netdev);

    self->suspended = 1;
	
    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
}


/*
 * Function ep93xx_irda_wakeup(self)
 *
 *    Called by the OS to indicate a resume from suspend is taking place.
 *    Awaken the hardware. 
 *
 */
static void 
ep93xx_irda_wakeup(struct ep93xx_irda_cb *self)
{
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
	
    if(!self->suspended)
    {
	return;
    }
	
    ep93xx_irda_net_open(self->netdev);
	
    MESSAGE("%s, Waking up\n", driver_name);

    self->suspended = 0;
	
    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
}

/*
 * Function ep93xx_irda_pmproc(dev, rqst, data)
 *
 * Registered power management callback procedure.
 *
 */
static int 
ep93xx_irda_pmproc(struct pm_dev *dev, pm_request_t rqst, void *data)
{
    struct ep93xx_irda_cb *self = (struct ep93xx_irda_cb*) dev->data;
        
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
            __FUNCTION__);
	
    if(self) 
    {
	    switch(rqst) 
            {
		    case PM_SUSPEND:
                        {
                            ep93xx_irda_suspend(self);
                            break;
                        }
                    case PM_RESUME:
                        {
                            ep93xx_irda_wakeup(self);
                            break;
                        }
            }
    }
        
    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
            __FUNCTION__);	
        
    return(0);
}
#endif

/*
 * Function SetInterrupts
 *
 *    Sets interrupts on/off based on speed and boolean.
 *    A boolean value of TRUE turns on interrupts for 
 *    the hardware that manages that IR speed.  FALSE turns them off.
 *
 */
static void 
SetInterrupts(struct ep93xx_irda_cb *self, unsigned char enable)
{
	
    unsigned char ucRegVal = 0, ucRegMask = 0;
    unsigned long ulRegAddr, ulRegAddr2;

    EP93XX_DEBUG(2, "%s(), ----------- Start -----------\n", 
        __FUNCTION__);	

    EP93XX_DEBUG(2, "%s(), IRQs enable = %d, speed = %d\n", __FUNCTION__, 
		    enable, self->speed);
    

    if(self->speed <= 115200)
    {
        /* Action on IRQs in SIR mode */
        ulRegAddr = UART2CR;
        ucRegVal = inl(ulRegAddr);

	 
	if(self->direction & DIR_TX)
	{
		ucRegMask |= U2CR_TxIrq;
	} 
	
	if(self->direction & DIR_RX)
	{
		ucRegMask |= U2CR_RxIrq;
	}
	
        if(enable)
        {
	    ucRegVal &= (char)~(U2CR_RxIrq|U2CR_TxIrq);	
            ucRegVal |= (char)(ucRegMask);
        }
        else
        {
            ucRegVal &= (char)~(ucRegMask);
        }
    }
    else
    {
	ucRegMask = 0;
        if(self->speed <= 1152000)
        {
            /*
             * Action on IRQs in MIR mode 
             */
            ulRegAddr = MIMR;
	    ulRegAddr2 = MISR;
        }
        else
        {
            /* 
             * Action on IRQs in FIR mode     
             */     
            ulRegAddr = FIMR;
	    ulRegAddr2 = FISR;
        }

	if(self->direction & DIR_TX)
	{
		ucRegMask |= (MFIMR_TXABORT|MFIMR_TXFC);
	}

	if(self->direction & DIR_RX)
	{
		ucRegMask |= (MFIMR_RXFL|MFIMR_RXFC);
	}
	
        /*
         * Bit mask is the same for MIR/FIR.
         */
        if(enable)
        {
            ucRegVal |= (char)(ucRegMask);

	    /*
	     * Clear sticky bits before turning on IRQs
	     */
	    outl(MFISR_TXFC|MFISR_TXFABORT|MFISR_RXIL|MFISR_RXFL, ulRegAddr2);
	    inl(ulRegAddr2);
	    inl(IrRIB);
        }
        else
        {
            /*
             * M/FIR IRQ registers only have IRQ related bits, safe
             * to blast them therefore.
             */
            ucRegVal = 0;
        }
    }

    outl(ucRegVal, ulRegAddr);
      
    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
}

static void 
SetIR_Transmit(unsigned char ucEnable)
{
	unsigned long ulRegVal;
	
	ulRegVal = inb(IrCtrl);

	if(ucEnable)
	{
		if(ulRegVal & IrCONTROL_TXON)
		{
			return;
		}
		else
		{
			outb(ulRegVal|IrCONTROL_TXON, IrCtrl);
		}
	}
	else
	{
		if(!(ulRegVal & IrCONTROL_TXON))
		{
			return;
		}
		else
		{
			outb(ulRegVal&~(IrCONTROL_TXON), IrCtrl);
		}

	}
}


/*
 * Function SetIR_Receive()
 *
 * 	Enables OR Disables RX & RXRP in IrCtrl based on
 * 	supplied boolean.
 */
static void
SetIR_Receive(unsigned char ucEnable)
{
	unsigned long ulRegVal;

	ulRegVal = inb(IrCtrl);

	if(ucEnable)
	{
		if((ulRegVal & (IrCONTROL_RXON|IrCONTROL_RXRP)) == 
				(IrCONTROL_RXON|IrCONTROL_RXRP))
		{
			return;
		}
		else
		{
			outb(ulRegVal|IrCONTROL_RXON|IrCONTROL_RXRP, IrCtrl);
		}
	}
	else
	{
		if(!(ulRegVal & (IrCONTROL_RXON & IrCONTROL_RXRP)))
		{
			return;
		}
		else
		{
			outb(ulRegVal&~(IrCONTROL_RXON|IrCONTROL_RXRP), 
					IrCtrl);
		}
	}
}


/*
 * Function SetDMA()
 *
 *    Enables or disables DMA  capability for current speed.
 *    'enable' parameter determines action. Direction 
 *    determines tx and/or rx DMA.
 *
 */
static void
SetDMA(struct ep93xx_irda_cb *self, unsigned char direction, 
         unsigned char enable)
{
    int iRegVal, iDMARegVal;
 
    
    if(self->speed <= 115200)
    {
	/*
	 * SIR section.  No DMA for SIR at this time.
	 */
        if(direction == DIR_RX)
	{

	}
	else
	{

	}	
    }
    else
    {    
	 /*
	  * MFIR Section
	  */
   
	 /*
	  * Read in current value.
	  */
	 iRegVal = iDMARegVal = inb(IrDMACR);
      	 
	 if(direction & DIR_RX)
	 {
		if(enable)
		{
			iRegVal |= IrDMACR_DMARXE;
		}
		else
		{
			iRegVal &= ~(IrDMACR_DMARXE);
		}
	 }
	 
	 if(direction & DIR_TX)
	 {
		 if(enable)
		 {
#ifndef PIO_TX			 
			 iRegVal |= IrDMACR_DMATXE;
#endif			 
		 }
		 else
		 {
#ifndef PIO_TX			 
			 iRegVal &= ~(IrDMACR_DMATXE);
#endif			 
		 }
	 }	 

	 /*
	  * Start or stop DMA engine, and update controlling registers
	  * as appropriate.
	  */	 
	 if(enable)
	 {	
		if(direction & DIR_RX)
		{
			ep93xx_dma_start(self->iDMAh[DMA_MFIR_RX],1,0);
			if(iRegVal != iDMARegVal)
			{
				outb(iRegVal, IrDMACR);
			}
		}

		if(direction & DIR_TX)
		{
#ifndef PIO_TX		
			if(iRegVal != iDMARegVal)
			{	
				outb(iRegVal, IrDMACR);
			}
			ep93xx_dma_start(self->iDMAh[DMA_MFIR_TX],1,0);
#endif			
		}
	 }
	 else
	 { 
		if(direction & DIR_RX)
		{
			ep93xx_dma_pause(self->iDMAh[DMA_MFIR_RX],1,0);
			if(iRegVal != iDMARegVal)
			{
				outb(iRegVal, IrDMACR);
			}
		}

		if(direction & DIR_TX)
		{
#ifndef PIO_TX			
			ep93xx_dma_pause(self->iDMAh[DMA_MFIR_TX],1,0);
			if(iRegVal != iDMARegVal)
			{
				outb(iRegVal, IrDMACR);
			}
#endif
		}
	 }

#ifdef DEBUG	
	 if(iRegVal != iDMARegVal)
	 {
		 EP93XX_DEBUG(1, "%s(), IrDMACR = 0x%x\n", 
			 __FUNCTION__, inb(IrDMACR));
	 }
#endif	 
    }
}


/*
 * Function SIR2MFIR(self, TargetSpeed)
 *
 *    Reconfigures hardware for MIR/FIR transactions.
 *
 */
static void 
SIR2MFIR(struct ep93xx_irda_cb *self, __u32 TargetSpeed)
{
    unsigned long ulRegVal, ulReg, ulRegSR;
		
    EP93XX_DEBUG(2, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
    /*
     * Suspend IrDA block
     */
    outb(0, IrCtrl);
    
    /*
     * Now safe to disable, so disable IrDA block.
     */
    outb(0, IrEnable);
    
    /*
     * Disable UART2, and clear CTRL registers.
     */
    outb(0, UART2CR);
          
    /*
     * Disable private DMA for SIR/UART2.
     */
    SetDMA(self, DIR_BOTH, FALSE);

    /*
     * Need a slight delay, when IrENABLE gets set it may
     * generate a SIP signal.  Give the target of our last
     * SIR TX a chance to process before then.
     */
    mdelay(10);

    /*
     * Set IrDA enc/dec to MIR/FIR (based on target speed).
     */
    if(TargetSpeed < 4000000)
    {
        /* 
         * MIR - enable MIR mode
         */
        outl(IrENABLE_MIR, IrEnable);

        /* 
         * set the MIR bit rate 
         */
        if(TargetSpeed == 1152000)
        {
            outb(IrCONTROL_MIRBR, IrCtrl);
        }
            
        ulRegSR = MISR;
    }
    else
    {
	    
        /* 
         * FIR - turn on FIR clk, enable FIR mode 
	 */
	ulRegVal = inl(SYSCON_PWRCNT);
	outl((ulRegVal | SYSCON_PWRCNT_FIREN), SYSCON_PWRCNT);
	    
        outb(IrENABLE_FIR, IrEnable);
        
        ulRegSR = FISR;
    }

    /*
     * Clear sticky bits and clear RFC.
     */
    outl(MFISR_TXFC|MFISR_TXFABORT|MFISR_RXIL|MFISR_RXFL, ulRegSR);
    inl(ulRegSR);
    inl(IrRIB);
 
    /*
     * Enable IRQs.
     */
    if(TargetSpeed < 4000000)
    {
	ulReg = MIMR;
    }
    else
    {
	ulReg = FIMR;
    }
    ulRegVal = MFIMR_RXFL | MFIMR_RXIL| MFIMR_RXFC | MFIMR_RXFS;
    outb(ulRegVal, ulReg);
 
    /*
     * Enable private DMA access.
     */
    //outb(IrDMACR_DMARXE, IrDMACR);
    //self->direction |= DIR_RX;
 
    /*
     * Enable Tx/Rx.  This should result in a SIP broadcast, which should buy
     * us 500ms to get ready.
     */
    //outb(IrCONTROL_RXRP|IrCONTROL_RXON, IrCtrl); // new
   SetIR_Receive(TRUE);
    
    EP93XX_DEBUG(1, "IrEnable = 0x%x, IrCtrl = 0x%x, M/FISR = 0x%x\n", 
    				inb(IrEnable), inb(IrCtrl), inl(ulRegSR));
				
    EP93XX_DEBUG(1,"M/FIMR = 0x%x, PWRCNT = 0x%x, DEVCFG = 0x%x\n", 
    		inb(ulReg), inl(SYSCON_PWRCNT), inl(SYSCON_DEVCFG));
			
    EP93XX_DEBUG(2, "IrAdrMatchVal = 0x%x\n", inb(IrAdrMatchVal));
    EP93XX_DEBUG(2, "IrFlag = 0x%x\n", inw(IrFlag));
    EP93XX_DEBUG(2, "IrData = 0x%x\n", inl(IrData));
    EP93XX_DEBUG(2, "IrRIB = 0x%x\n", inw(IrRIB));
    EP93XX_DEBUG(2, "IrTR0 = 0x%x\n", inw(IrTR0));
    EP93XX_DEBUG(2, "IrDMACR = 0x%x\n", inb(IrDMACR));
    EP93XX_DEBUG(2, "SIRTR0 = 0x%x\n", inb(SIRTR0));
    EP93XX_DEBUG(2, "MISR = 0x%x\n", inb(MISR));
    EP93XX_DEBUG(2, "MIMR = 0x%x\n", inb(MIMR));
    EP93XX_DEBUG(2, "MIIR = 0x%x\n", inb(MIIR));
    EP93XX_DEBUG(2, "FISR = 0x%x\n", inb(FISR));
    EP93XX_DEBUG(2, "FIMR = 0x%x\n", inb(FIMR));
    EP93XX_DEBUG(2, "FIIR = 0x%x\n", inb(FIIR));


    EP93XX_DEBUG(2, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);	
}


/*
 * Function MFIR2SIR(self, TargetSpeed)
 *
 *    Reconfigures hardware for SIR transactions.
 *
 */
static void 
MFIR2SIR(struct ep93xx_irda_cb *self, __u32 TargetSpeed)
{
    unsigned long ulRegVal;
    	
    EP93XX_DEBUG(1, "%s(), ---------------- Start ----------------\n", 
        __FUNCTION__);
     
    /*
     * Disable all interrupts.
     */
    ulRegVal = self->direction;
    self->direction = DIR_BOTH;
    SetInterrupts(self, FALSE);
    self->direction = ulRegVal;
   
    if(self->speed == 4000000)
    {
	    ulRegVal = inl(SYSCON_PWRCNT);
	    ulRegVal &= ~(SYSCON_PWRCNT_FIREN);
	    outl(ulRegVal, SYSCON_PWRCNT);
    }
    
    /*
     * Disable MIR/FIR private DMA and stop IrDA block.
     */
    outl(0, IrCtrl);
    SetDMA(self, DIR_BOTH, FALSE);

    /*
    * Enable SIR in IR block.
    */
    outl(IrENABLE_SIR, IrEnable);
    
    /*
     * Make certain DEVCFG is set as we need it.
     */
    ulRegVal = inl(SYSCON_DEVCFG);
    SysconSetLocked(SYSCON_DEVCFG, (ulRegVal | 
			    (SYSCON_DEVCFG_U2EN | SYSCON_DEVCFG_IonU2)));
   
   /*
    * Enable SIR & UART on UART2 block.
    */ 
   outb(U2CR_SIR|U2CR_UART, UART2CR); 
  
    /*
     * Enable SIR/UART2 private DMA.
     */
    //outl(U2DMACR_TXDMA|U2DMACR_RXDMA, UART2DMACR);
   	
    EP93XX_DEBUG(1, "%s(), ----------------- End ------------------\n", 
        __FUNCTION__);
}

#ifdef MODULE
MODULE_AUTHOR("Cirrus Logic, Inc.");
MODULE_DESCRIPTION("EP93xx IrDA Driver");
MODULE_LICENSE("GPL");


MODULE_PARM(irq, "1-4i");
MODULE_PARM_DESC(irq, "IRQ lines");
MODULE_PARM(dma, "1-4i");
MODULE_PARM_DESC(dma, "DMA channels");

int init_module(void)
{
	return(ep93xx_irda_init());	
}

void cleanup_module(void)
{
	ep93xx_irda_cleanup();
}
#endif /* MODULE */
