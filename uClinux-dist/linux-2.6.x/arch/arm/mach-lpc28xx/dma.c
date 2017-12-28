/*
 *  linux/arch/arm/mach-lpc28xx/dma.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/dma-mapping.h>
#include <asm/io.h>
#include <asm/mach/dma.h>

#define DMA_CHANNEL_NUM 8

static struct lpc28xx_gpdma_registers {

	struct _REGGROUP1{
		volatile unsigned int DMASource;
		volatile unsigned int DMADest;
		volatile unsigned int DMALength;
		volatile unsigned int DMAConfig;
		volatile unsigned int DMAEnab;
		volatile unsigned int RES[2];
		volatile unsigned int DMACount ;
	}REGGROUP1[DMA_CHANNEL_NUM];

	volatile unsigned int RES1[0x100];
	
	struct _REGGROUP2{
		volatile unsigned int DMAAltSource;
		volatile unsigned int DMAAltDest;
		volatile unsigned int DMAAltLength;
		volatile unsigned int DMAAltConfig;
	}REGGROUP2[DMA_CHANNEL_NUM];

	volatile unsigned int RES2[0x180];
	
	volatile unsigned int DMAEnable;
	volatile unsigned int DMAStat;
	volatile unsigned int DMAIRQMask;
	volatile unsigned int DMASoftInt;
	
}*lpc28xx_gpdma_registers = (struct lpc28xx_gpdma_registers *)DMAREGBASE;

static struct dma_channel{
	void (*irq_handler) (int, void *);
	int is_used;
	void* data;
	int list_channel;
	int block_channel;
}lpc28xx_dma_channels[DMA_CHANNEL_NUM];

static spinlock_t dma_lock = SPIN_LOCK_UNLOCKED;

static unsigned int dma_enable_status;

static irqreturn_t dma_irq_handler(int irq, void *dev_id)
{	
	int i;
	unsigned status;
	u32 chan_status;

	status = lpc28xx_gpdma_registers->DMAStat;
	lpc28xx_gpdma_registers->DMAStat = status;
	for(i=0; i<DMA_CHANNEL_NUM; ++i){
		if(status & 0x3){
			if(lpc28xx_dma_channels[i].irq_handler){
				lpc28xx_dma_channels[i].irq_handler((status&0x3), lpc28xx_dma_channels[i].data);
			}
		}
		status = status >> 2;
	}

	chan_status = lpc28xx_gpdma_registers->DMAEnable;
	//check list end interrupt
	if(status & (1<<14)){
		for(i=0; i<DMA_CHANNEL_NUM; ++i){
			if((!(chan_status & (1<<i))) && (dma_enable_status & (1<<i))){
				if(lpc28xx_gpdma_registers->REGGROUP1[i].DMADest == 0x80103C10){
					lpc28xx_dma_channels[i].irq_handler(0, lpc28xx_dma_channels[i].data);
				}
			}
		}
	}
	dma_enable_status = chan_status;
	return IRQ_HANDLED;
}

static void lpc28xx_dma_lock(void)
{
	spin_lock_irq(&dma_lock);
}

static void lpc28xx_dma_unlock(void)
{
	spin_unlock_irq(&dma_lock);
}

int lpc28xx_start_dma(int channelid, char* src_addr, char* dest_addr, int src_id, int dest_id, int len)
{
	if(channelid >= DMA_CHANNEL_NUM){
		return -ENODEV;
	}

	lpc28xx_gpdma_registers->REGGROUP1[channelid].DMASource = (unsigned int)src_addr;
	lpc28xx_gpdma_registers->REGGROUP1[channelid].DMADest = (unsigned int)dest_addr;
	lpc28xx_gpdma_registers->REGGROUP1[channelid].DMALength = len & 0x7ff;
	lpc28xx_gpdma_registers->REGGROUP1[channelid].DMAConfig = dest_id | (src_id << 5);
	lpc28xx_gpdma_registers->DMAIRQMask &= ~(0x3 << (channelid*2));
	lpc28xx_gpdma_registers->REGGROUP1[channelid].DMAEnab = 0x1;

	
	return 0;
}
EXPORT_SYMBOL_GPL(lpc28xx_start_dma);

int lpc28xx_start_list_dma(int channelid, struct lpc28xx_dma_entry* entry)
{
	int list_channel;
	u32 mask;
	
	if(channelid >= DMA_CHANNEL_NUM){
		return -ENODEV;
	}

	list_channel = lpc28xx_dma_channels[channelid].list_channel;

	mask = lpc28xx_gpdma_registers->DMAIRQMask;
	mask |= ((0x3 << (channelid*2)) | (0x3 << (list_channel*2)));
	mask &= ~(1<<30);
	lpc28xx_gpdma_registers->DMAIRQMask = mask;
	
	lpc28xx_gpdma_registers->REGGROUP1[list_channel].DMASource = (unsigned int)entry;
	lpc28xx_gpdma_registers->REGGROUP1[list_channel].DMADest = (unsigned int)(&(lpc28xx_gpdma_registers->REGGROUP2[channelid].DMAAltSource));
	lpc28xx_gpdma_registers->REGGROUP1[list_channel].DMALength = 0x4;
	lpc28xx_gpdma_registers->REGGROUP1[list_channel].DMAConfig = (channelid<<13) | (0x1 << 17);
	lpc28xx_gpdma_registers->REGGROUP1[list_channel].DMAEnab = 0x1;

	return 0;
}
EXPORT_SYMBOL_GPL(lpc28xx_start_list_dma);

void lpc28xx_stop_dma(int channelid)
{
	if(channelid >= DMA_CHANNEL_NUM){
		return;
	}
	
	lpc28xx_gpdma_registers->DMAIRQMask |= (0x3 << (channelid*2));
	lpc28xx_gpdma_registers->REGGROUP1[channelid].DMAEnab = 0x0;
}
EXPORT_SYMBOL_GPL(lpc28xx_stop_dma);

void lpc28xx_stop_list_dma(int channelid)
{
	/*if(channelid >= DMA_CHANNEL_NUM){
		return;
	}
	
	lpc28xx_gpdma_registers->DMAIRQMask |= (0x3 << (channelid*2));
	lpc28xx_gpdma_registers->REGGROUP1[channelid].DMAEnab = 0x0;*/
}
EXPORT_SYMBOL_GPL(lpc28xx_stop_list_dma);

int lpc28xx_request_list_channel(void (*irq_handler) (int, void *), void *data, int* list_channel)
{
	int found = 0;	/* basic sanity checks */
	int block_chan = -1;
	int list_chan;
	int i;

	lpc28xx_dma_lock();	/* try grabbing a DMA channel with the requested priority */	
	for (i = 0; i<DMA_CHANNEL_NUM; ++i) {		
		if (!lpc28xx_dma_channels[i].is_used) {
			block_chan = i;
			lpc28xx_dma_channels[i].is_used = 1;
			break;		
		}
	}	

	for (; i < DMA_CHANNEL_NUM; ++i) {		
		if (!lpc28xx_dma_channels[i].is_used) {
			list_chan = i;
			found = 1;
			break;		
		}
	}

	if (found) {
		lpc28xx_dma_channels[block_chan].irq_handler = irq_handler;	
		lpc28xx_dma_channels[block_chan].data = data;
		lpc28xx_dma_channels[block_chan].is_used = 1;
		lpc28xx_dma_channels[block_chan].list_channel = list_chan;
		lpc28xx_dma_channels[block_chan].block_channel = -1;

		lpc28xx_dma_channels[list_chan].irq_handler = NULL;	
		lpc28xx_dma_channels[list_chan].data = NULL;
		lpc28xx_dma_channels[list_chan].is_used = 1;
		lpc28xx_dma_channels[list_chan].list_channel = -1;
		lpc28xx_dma_channels[list_chan].block_channel = block_chan;

		*list_channel = list_chan;
	} else {		
		printk(KERN_WARNING "No more available DMA channels\n");		
		if(block_chan >= 0){
			lpc28xx_dma_channels[i].is_used = 0;
		}
		block_chan = -ENODEV;
	}	
	lpc28xx_dma_unlock();
	
	return block_chan;
}
EXPORT_SYMBOL_GPL(lpc28xx_request_list_channel);

int lpc28xx_request_channel(void (*irq_handler) (int, void *), void *data)
{

	int i, found = 0;	/* basic sanity checks */	

	lpc28xx_dma_lock();	/* try grabbing a DMA channel with the requested priority */	
	for (i = 0; i < DMA_CHANNEL_NUM; ++i) {		
		if (!lpc28xx_dma_channels[i].is_used) {
			found = 1;			
			break;		
		}
	}	

	if (found) {
		lpc28xx_dma_channels[i].irq_handler = irq_handler;	
		lpc28xx_dma_channels[i].data = data;
		lpc28xx_dma_channels[i].is_used = 1;
	} else {		
		printk(KERN_WARNING "No more available DMA channels\n");		
		i = -ENODEV;	
	}	
	lpc28xx_dma_unlock();
	
	return i;
}
EXPORT_SYMBOL_GPL(lpc28xx_request_channel);

void lpc28xx_release_channel(int channelid)
{
	if(channelid >= DMA_CHANNEL_NUM){
		return;
	}

	lpc28xx_stop_dma(channelid);

	lpc28xx_dma_lock();	/* try grabbing a DMA channel with the requested priority */	
	lpc28xx_dma_channels[channelid].irq_handler = NULL;	
	lpc28xx_dma_channels[channelid].data = NULL;
	lpc28xx_dma_channels[channelid].is_used = 0;
	lpc28xx_dma_unlock();
}
EXPORT_SYMBOL_GPL(lpc28xx_release_channel);

void lpc28xx_release_list_channel(int channelid)
{
	int list_channel;
	if(channelid >= DMA_CHANNEL_NUM){
		return;
	}

	lpc28xx_stop_list_dma(channelid);

	lpc28xx_dma_lock();	/* try grabbing a DMA channel with the requested priority */	
	list_channel = lpc28xx_dma_channels[channelid].list_channel;
	lpc28xx_dma_channels[channelid].irq_handler = NULL;	
	lpc28xx_dma_channels[channelid].data = NULL;
	lpc28xx_dma_channels[channelid].is_used = 0;
	lpc28xx_dma_channels[channelid].list_channel = -1;

	lpc28xx_dma_channels[list_channel].irq_handler = NULL;	
	lpc28xx_dma_channels[list_channel].data = NULL;
	lpc28xx_dma_channels[list_channel].is_used = 0;
	lpc28xx_dma_channels[list_channel].list_channel = -1;
	lpc28xx_dma_unlock();
}
EXPORT_SYMBOL_GPL(lpc28xx_release_list_channel);

static int __init lpc28xx_dma_init(void)
{
	int ret, i;	

	ret = request_irq(LPC28XX_IRQ_GPDMA, dma_irq_handler, 0, "DMA", NULL);
	if (ret) {		
		printk(KERN_CRIT "Wow!  Can't register IRQ for DMA\n");
		goto out;
	}
	
	lpc28xx_gpdma_registers->DMAEnable = 0x0;
	lpc28xx_gpdma_registers->DMAIRQMask = 0xC000FFFF; 

	for(i=0; i<DMA_CHANNEL_NUM; ++i){
		lpc28xx_dma_channels[i].irq_handler = NULL;
		lpc28xx_dma_channels[i].is_used = 0;
		lpc28xx_dma_channels[i].list_channel = -1;
		lpc28xx_dma_channels[i].data = NULL;
	}

	dma_enable_status = 0;
	
out:	return ret;

}
arch_initcall(lpc28xx_dma_init);

