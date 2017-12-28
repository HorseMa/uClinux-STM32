/*
 *  linux/drivers/mmc/nios_mmc.c - FPS-Tech NIOS_MMC Driver
 *
 *  Copyright (C) 2008 Jai Dhar / FPS-Tech, All Rights Reserved.
 *  Credits: This driver is partially derived from pxamci.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/pagemap.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/scatterlist.h>
#include <linux/scatterlist.h>

#include "nios_mmc.h"
#define DRIVER_NAME	"nios_mmc"
#define debug_level 1
#define NR_SG 1

#if defined(CONFIG_MMC_DEBUG)
#define MMC_DEBUG(l,x...) {\
	if (l <= debug_level)\
	{\
       		printk("%s(): ",__func__); printk(x);\
	}\
}
#else
#define MMC_DEBUG(l,x...) {}
#endif /* CONFIG_MMC_DEBUG */

#define MMC_CRIT_ERR(x...) {\
	printk("Crit. error in %s(): %s. Halting\n",__func__,x);\
	while(1);\
}

static void nios_mmc_start_cmd(NIOS_MMC_HOST *host, struct mmc_command *cmd);
static void nios_mmc_end_request(struct mmc_host *mmc, struct mmc_request *mrq);
/***************************** Start of main functions ********************************/

void nios_mmc_end_cmd(NIOS_MMC_HOST *host)
{
	unsigned int tmp,ret;	
	struct mmc_command *cmd = host->cmd;
	struct mmc_data *data = cmd->data;
	struct mmc_command *stop = data->stop;

	/* assign stop only if data is assigned */
	if (data) stop = data->stop;
	else stop = NULL;
	/* Check MRQ first */
	if (cmd == NULL)
	{
		MMC_CRIT_ERR("CMD is null when it shouldn't be!");
		while(1);
	}
	tmp = readl(host->base + NIOS_MMC_REG_CTLSTAT);
	/* Interrupt flags will be cleared in ISR routine, so we don't have to touch them */
	if (tmp & NIOS_MMC_CTLSTAT_TIMEOUTERR_IF)
	{
		MMC_DEBUG(1,"Timeout error\n");
		ret = -ETIMEDOUT;
	}
	else if (tmp & NIOS_MMC_CTLSTAT_FRMERR_IF)
	{
		MMC_DEBUG(1,"Framing error\n");
		ret = -EILSEQ;
	}
	else if (tmp & NIOS_MMC_CTLSTAT_CRCERR_IF)
	{
		MMC_DEBUG(1,"CRC Error\n");
		ret = -EILSEQ;
	}
	else if (tmp & NIOS_MMC_CTLSTAT_FIFO_UNDERRUN_IF)
	{
		MMC_DEBUG(1,"FIFO Underrun error\n");
		ret = -EINVAL;
	}
	else if (tmp & NIOS_MMC_CTLSTAT_FIFO_OVERRUN_IF)
	{
		MMC_DEBUG(1,"FIFO Overrun error\n");	
		ret = -EINVAL;
	}
	else
	{
		/* Response is good! */
		ret = 0;
	}
	if (ret)
	{
		MMC_DEBUG(1,"Error executing CMD%d\n",cmd->opcode);
		MMC_DEBUG(2,"Response argument: 0x%X\n",readl(host->base+NIOS_MMC_REG_CMD_ARG0));
	}
	/* Load response into command structure */
	cmd->error = ret;	
	if (mmc_resp_type(cmd) == MMC_RSP_R2)
  	{
    		cmd->resp[0] = readl (host->base + NIOS_MMC_REG_CMD_ARG3);
    		cmd->resp[1] = readl (host->base + NIOS_MMC_REG_CMD_ARG2);
    		cmd->resp[2] = readl (host->base + NIOS_MMC_REG_CMD_ARG1);
    		cmd->resp[3] = readl (host->base + NIOS_MMC_REG_CMD_ARG0);
  	}
	else cmd->resp[0] = readl (host->base + NIOS_MMC_REG_CMD_ARG0);
	/* Check if this was a data transaction */
	if (data)
	{
		if (cmd->error == 0)
			data->bytes_xfered = data->blksz*data->blocks;
		else data->bytes_xfered = 0;
	}
	if (stop)
	{
		/* Schedule the stop command */
		/* We will need to reassign the pointer in the structure since we are 
		 * switching commands now */
		host->cmd = stop;
		nios_mmc_start_cmd(host, stop);
	}
	else
	{
		/* No other commands needed, finish off transaction */
		nios_mmc_end_request(host->mmc, cmd->mrq);
	}
}
/* nios_mmc_execute_cmd(): Key function that interacts with MMC Host to carry out command */
void nios_mmc_execute_cmd(NIOS_MMC_HOST *host, unsigned char cmd, unsigned int arg_in, 
		unsigned char resp_type, unsigned char nocrc, 
		unsigned int bytes, unsigned int blocks, unsigned char rwn, unsigned int buf)
{
	unsigned int xfer_ctl = 0;
	u_char cmdidx;

	/* Do a sanity check that the core isn't busy... why should it be since we haven't started a cmd?? */
	if (readl(host->base + NIOS_MMC_REG_CTLSTAT) & NIOS_MMC_CTLSTAT_BUSY)
	{
		MMC_CRIT_ERR("Core is busy when it shouldn't be!");
	}
	xfer_ctl = (cmd & 0x3F) << NIOS_MMC_XFER_CTL_CMD_IDX_SHIFT;
	writel(arg_in, host->base + NIOS_MMC_REG_CMD_ARG0);
	xfer_ctl |= (resp_type & 0x3) << NIOS_MMC_XFER_CTL_RESP_CODE_SHIFT;
	if (nocrc) xfer_ctl |= NIOS_MMC_XFER_CTL_RESP_NOCRC;
	if (rwn) xfer_ctl |= NIOS_MMC_XFER_CTL_DAT_RWn;
	xfer_ctl |= (bytes & 0x3FF) << NIOS_MMC_XFER_CTL_BYTE_COUNT_SHIFT;
	xfer_ctl |= (blocks & 0x3FF) << NIOS_MMC_XFER_CTL_BLOCK_COUNT_SHIFT;
	xfer_ctl |= NIOS_MMC_XFER_CTL_XFER_START;
	if (host->dat_width) xfer_ctl |= NIOS_MMC_XFER_CTL_DAT_WIDTH;
	cmdidx = (xfer_ctl >> NIOS_MMC_XFER_CTL_CMD_IDX_SHIFT)&0x3F;
	/* Setup DMA base */
	writel(buf | (1<<31), host->base + NIOS_MMC_REG_DMA_BASE);
	MMC_DEBUG(1,"XFER_CTL: 0x%X (CMD%d), DMA_BASE(%c): 0x%X, ARG_IN: 0x%X\n",
			xfer_ctl,cmdidx,
		       	(xfer_ctl&NIOS_MMC_XFER_CTL_DAT_RWn)?'R':'W',buf, arg_in);
	/* Execute command */
	writel(xfer_ctl, host->base + NIOS_MMC_REG_XFER_CTL);
}
static irqreturn_t nios_mmc_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	NIOS_MMC_HOST *host = dev_id;
	unsigned int ret;

	ret = readl(host->base + NIOS_MMC_REG_CTLSTAT);	
	MMC_DEBUG(2,"IRQ, ctlstat: 0x%X\n",ret);

	if (ret & NIOS_MMC_CTLSTAT_CD_IF)
	{
		/* Card-detect interrupt */
		if (ret & NIOS_MMC_CTLSTAT_CD)
		{
			MMC_DEBUG(1,"HOT-PLUG: Card inserted\n");
		}
		else
		{
			MMC_DEBUG(1,"HOT-PLUG: Card removed\n");
		}
		mmc_detect_change(host->mmc, 100);
	}
	if (ret & NIOS_MMC_CTLSTAT_XFER_IF)
	{
		MMC_DEBUG(3,"Detected XFER Interrupt\n");
		/* Transfer has completed */
		nios_mmc_end_cmd(host);	
	}

	/* Clear the interrupt after everyone has had a chance to look at it! */
	writel(ret, host->base + NIOS_MMC_REG_CTLSTAT);
	return IRQ_HANDLED;
}
/* Function to start the CMD process */
static void nios_mmc_start_cmd(NIOS_MMC_HOST *host, struct mmc_command *cmd)
{
	unsigned char resp_type = 0, nocrc = 0, rwn = 0;
	struct mmc_data *data = cmd->data;
	struct scatterlist *sg;
	unsigned int current_address,bytes=0,blocks=0;

	/* Do a sanity check that we are being passed the same CMD that is in host struct */
	if (host->cmd != cmd)
	{
		MMC_CRIT_ERR("Cmd doesn't match what is in structure");
	}

	MMC_DEBUG(2,"Opcode: %d Arg: 0x%X ",cmd->opcode,cmd->arg);	
	switch (mmc_resp_type(cmd))
	{
		case MMC_RSP_R3:
			nocrc = 1;
		case MMC_RSP_R1:
		case MMC_RSP_R1B:
			resp_type = 1;
			break;
		case MMC_RSP_R2:
			resp_type = 2;
			nocrc = 1;
			break;
		case MMC_RSP_NONE:
			resp_type = 0;
			break;
		default:
			MMC_CRIT_ERR("Unhandled MMC Response type!");
			break;
	}

	sg = data->sg;
	current_address = (unsigned int) sg_virt(sg);
	cmd->error = 0;
	if (data)
	{
		if (data->sg_len > 1)
		{
			MMC_CRIT_ERR("sg_len is > 1!");
		}
		MMC_DEBUG(3,"Block size: %d Blocks: %d sg_len: %d\n",
				data->blksz,data->blocks,data->sg_len);
		if (data->stop) MMC_DEBUG(2,"Stop command present\n");
		/* Setup byte count */
		bytes = data->blksz;
		blocks = data->blocks-1;

		if (data->flags & MMC_DATA_READ) rwn = 1;
		else rwn = 0;

	}
	nios_mmc_execute_cmd(host, cmd->opcode, cmd->arg, 
			resp_type, nocrc, bytes, blocks, rwn, current_address);

}
/****************** Driver-level interface *****************/

/* This function is called from the driver level above */
/* nios_mmc_request() initiates the MMC request as setup in the mrq structure */
static void nios_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	NIOS_MMC_HOST *host = mmc_priv(mmc);

	if (host->cmd != NULL)
	{
		MMC_DEBUG(1,"HOST_CMD Not null!\n");
	}
	host->cmd = mrq->cmd;
	MMC_DEBUG(3,"Start req\n");
	nios_mmc_start_cmd(host, host->cmd);
	return;
}
/* Function to cleanup previous call */
static void nios_mmc_end_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	NIOS_MMC_HOST *host = mmc_priv(mmc);
	host->cmd = NULL;
	mmc_request_done(host->mmc, mrq);
	return;
}
static int nios_mmc_get_ro(struct mmc_host *mmc)
{
	int ctlstat;
	NIOS_MMC_HOST *host = mmc_priv(mmc);
	MMC_DEBUG(3,"Get RO\n");
	ctlstat = readl(host->base + NIOS_MMC_REG_CTLSTAT);
	if (ctlstat & NIOS_MMC_CTLSTAT_WP) return 1;
	return 0;
}
static void nios_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	NIOS_MMC_HOST *host = mmc_priv(mmc);
	int div;

	if (ios->clock)
	{
		/* FIXME: Look at divider calculation! */
		MMC_DEBUG(3,"Requesting clock: %d\n",ios->clock);
		div = (nasys_clock_freq / (2*ios->clock))-1;
		writel((div & 0xFFFF) | NIOS_MMC_CLK_CTL_CLK_EN,
				host->base + NIOS_MMC_REG_CLK_CTL);
	}
	else
	{
		/* Stop the clock */
		MMC_DEBUG(3,"Request stop clock\n");
		writel(0, host->base + NIOS_MMC_REG_CLK_CTL);
	}

	if (ios->bus_width)
		host->dat_width = 1;
	else 
		host->dat_width = 0;

	return;
}

static struct mmc_host_ops nios_mmc_ops = {
	.request	= nios_mmc_request,
	.get_ro		= nios_mmc_get_ro,
	.set_ios	= nios_mmc_set_ios,
};

static int nios_mmc_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct resource *r;
	NIOS_MMC_HOST *host = NULL;
	int ret, irq;

	MMC_DEBUG(3,"Starting NIOS_MMC Probe\n");	
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!r || irq < 0)
		return -ENXIO;
	r = request_mem_region(r->start, 16*4, DRIVER_NAME);
	if (!r)
	{
		MMC_DEBUG(3,"Error allocating mem. region\n");		
		return -EBUSY;
	}
	mmc = mmc_alloc_host(sizeof(NIOS_MMC_HOST), &pdev->dev);
	if (!mmc)
	{
		MMC_DEBUG(3,"Error allocating MMC Host\n");
		ret = -ENOMEM;
		goto out;
	}
	mmc->ops = &nios_mmc_ops;
	MMC_DEBUG(3,"Done initial probe\n");
	mmc->f_max = nasys_clock_freq/4;
	mmc->f_min = nasys_clock_freq/(1<<16);
	mmc->max_phys_segs = NR_SG;
	mmc->max_seg_size = 256;

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->dma = -1;
	host->dat_width = 0;
	host->cmd = NULL;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	spin_lock_init(&host->lock);
	host->res = r;
	host->irq = irq;
	host->base = ioremap(r->start, 16*4);
	if (!host->base)
	{
		ret = -ENOMEM;
		MMC_DEBUG(3,"Error in IO Remap\n");
		goto out;
	}
	MMC_DEBUG(3,"Setup host with Base: 0x%X IRQ: %d\n",(unsigned int) host->base, host->irq);

	/* Check that SD/MMC Core is present */
	ret = 0;
	ret = readl(host->base + NIOS_MMC_REG_VERSION_INFO);
	if ((ret & 0xFFFF) != 0xBEEF)
	{
		MMC_DEBUG(3,"Core not present\n");
		ret = -ENXIO;
		goto out;
	}
	printk("NIOS_MMC: FPS-Tech SD/SDIO/MMC Host Core, version %d.%d\n",ret >> 24, (ret >> 16) & 0xff);
	printk("NIOS_MMC: F_MAX: %d Hz, F_MIN: %d Hz\n",mmc->f_max,mmc->f_min);
	ret = readl(host->base + NIOS_MMC_REG_CTLSTAT);
	printk("NIOS_MMC: Host built with %s DAT driver\n",(ret & NIOS_MMC_CTLSTAT_HOST_4BIT)?"4-bit":"1-bit");
	mmc->caps = (ret&NIOS_MMC_CTLSTAT_HOST_4BIT)?MMC_CAP_4_BIT_DATA:0;

	/* Execute soft-reset on core */
	writel(NIOS_MMC_CTLSTAT_SOFT_RST,host->base + NIOS_MMC_REG_CTLSTAT);

	/* Enable interrupts on CD and XFER_IF only */
	/* Use BLK_PREFETCH for linux unless disabled */
	/* This section sets up CTLSTAT for the rest of the driver.
	 * Make sure all further writes to CTLSTAT are using bitwise OR!!! */
	ret = NIOS_MMC_CTLSTAT_CD_IE | NIOS_MMC_CTLSTAT_XFER_IE;
#if !defined(CONFIG_NIOS_MMC_NOBLK_PREFETCH)
	ret |= NIOS_MMC_CTLSTAT_BLK_PREFETCH;
#endif
	/* Execute write to CTLSTAT here */
	writel(ret, host->base + NIOS_MMC_REG_CTLSTAT);
	if (ret & NIOS_MMC_CTLSTAT_BLK_PREFETCH) {
		printk("NIOS_MMC: Using block-prefetching\n");
	}
	else {
		printk("NIOS_MMC: Block-prefetching disabled!\n");
	}


	ret = request_irq(host->irq, nios_mmc_irq, 0, DRIVER_NAME, (void *) host);
	if (ret)
	{
		MMC_DEBUG(3,"Error allocating interrupt\n");
		goto out;
	}
	platform_set_drvdata(pdev, mmc);
	mmc_add_host(mmc);
	MMC_DEBUG(3,"Completed full probe successfully\n");
	return 0;

out:
	if (host)
	{
	}
	if (mmc) mmc_free_host(mmc);
	release_resource(r);
	return ret;
}

static int nios_mmc_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	if (mmc) {
		NIOS_MMC_HOST *host = mmc_priv(mmc);

		mmc_remove_host(mmc);
		free_irq(host->irq, (void *) host);
		iounmap(host->base);
		release_resource(host->res);
		mmc_free_host(mmc);
	}
	return 0;
}
static int nios_mmc_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}
static int nios_mmc_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver nios_mmc_driver = {
	.probe		= nios_mmc_probe,
	.remove		= nios_mmc_remove,
	.suspend	= nios_mmc_suspend,
	.resume		= nios_mmc_resume,
	.driver		= {
		.name	= DRIVER_NAME,
	},
};

static int __init nios_mmc_init(void)
{
	int ret;
	ret =  platform_driver_register(&nios_mmc_driver);
	return ret;
}

static void __exit nios_mmc_exit(void)
{
	platform_driver_unregister(&nios_mmc_driver);
}

module_init(nios_mmc_init);
module_exit(nios_mmc_exit);

MODULE_DESCRIPTION("NIOS MMC Host Driver");
MODULE_LICENSE("GPL");
