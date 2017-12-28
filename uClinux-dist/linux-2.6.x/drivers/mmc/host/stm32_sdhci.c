#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/proc_fs.h>

#include <asm/dma.h>
#include <asm/arch/stm_sd.h>
#include <asm/arch/stm32f10x_conf.h>

//#include "sdcard.h"

#define UseInterrupt	1

#define DRIVER_NAME "stm3210e-sdci"

#define SD_DATATIMEOUT                  ((u32)0xFFFFFFFF)
#define SDIO_STATIC_FLAGS               ((u32)0x000005FF)

#define TO_DEVICE			0x1
#define FROM_DEVICE			0x2
#define SDIO_INIT_CLK_DIV		((u8)0xB2)
#define SDIO_TRANSFER_CLK_DIV		((u8)0x1) 
#define SDIO_FIFO_Address		((u32)0x40018080)

#define sdh_suspend			NULL
#define sdh_resume			NULL

struct sdh_host {
	struct mmc_host		*mmc;
	spinlock_t		lock; 
	struct resource		*res;
	void __iomem		*base;
	int			irq;
	int			stat_irq;
	int			data_dir;
	unsigned int		imask;
	unsigned int		power_mode;
	unsigned int		clk_div;
	
	u32*			DBuff;
	int			Dlen;
	
	unsigned		xfert;
	
#define XFERT_NONE		(0)
#define XFERT_READING		(1)
#define XFERT_WRTITING		(1<<1)
#define XFERT_STOPED		(1<<2)

	struct mmc_request	*mrq;
	struct mmc_command	*cmd;
	struct mmc_data		*data;
};


static stm32_sd_host *get_sdh_data(struct platform_device *pdev)
{
	return pdev->dev.platform_data;
}

static void sdh_stop_clock(struct sdh_host *host)
{
	SDIO_ClockCmd(DISABLE);
}

static void sdh_enable_stat_irq(struct sdh_host *host, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->imask |= mask;
	SDIO_ITConfig(mask,ENABLE);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void sdh_disable_stat_irq(struct sdh_host *host, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->imask &= ~mask;
	SDIO_ITConfig(mask,DISABLE);
	SDIO_ClearFlag(mask);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void sdh_finish_request(struct sdh_host *host, struct mmc_request *mrq)
{
	pr_debug("%s enter\n", __FUNCTION__);
	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;
	mmc_request_done(host->mmc, mrq);
}

static void sdh_start_cmd(struct sdh_host *host,struct mmc_command * cmd)
{
	/* configure host following the data in mmc_command struct */
	unsigned int stat_mask;
	SDIO_CmdInitTypeDef SDIO_CmdInit;

	pr_debug("%s enter cmd:0x%p\n", __FUNCTION__, cmd);
	pr_debug("arg: 0x%08x opcode: 0x%08x flags: 0x%08x\n", cmd->arg, cmd->opcode, cmd->flags);
	WARN_ON(host->cmd != NULL);

	host->cmd = cmd;

	stat_mask = 0;
	
	SDIO_CmdStructInit(&SDIO_CmdInit);


	if (cmd->flags & MMC_RSP_PRESENT) {
		SDIO_CmdInit.SDIO_Response = SDIO_Response_Short;
		stat_mask |= SDIO_FLAG_CMDREND;
	} else
	{
		SDIO_CmdInit.SDIO_Response = SDIO_Response_No;
		stat_mask |= SDIO_FLAG_CMDSENT;
	}

	if (cmd->flags & MMC_RSP_136)
		SDIO_CmdInit.SDIO_Response = SDIO_Response_Long;

	stat_mask |= SDIO_IT_CCRCFAIL | SDIO_IT_CTIMEOUT;

	sdh_enable_stat_irq(host, stat_mask);

	SDIO_CmdInit.SDIO_Argument = cmd->arg;
	SDIO_CmdInit.SDIO_CmdIndex = cmd->opcode;
	SDIO_CmdInit.SDIO_CPSM = SDIO_CPSM_Enable;
	
	SDIO_SendCommand(&SDIO_CmdInit);
	SDIO_ClockCmd(ENABLE);
}

static void sdh_setup_data(struct sdh_host *host,struct mmc_data * data)
{
	/* configure host and DMA following the data in mmc_data struct */
	unsigned int length;
	SDIO_DataInitTypeDef DataSetup;
	
	host->data = data;

	length = data->blksz * data->blocks;
	DataSetup.SDIO_DataLength = length;
	
	if (data->flags & MMC_DATA_STREAM)
		DataSetup.SDIO_TransferMode = SDIO_TransferMode_Stream;
	else if(data->flags & (MMC_DATA_WRITE|MMC_DATA_READ))
		DataSetup.SDIO_TransferMode = SDIO_TransferMode_Block;

	if (data->flags & MMC_DATA_READ)
	{
		host->data_dir = FROM_DEVICE;
		DataSetup.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	} else if (data->flags & MMC_DATA_WRITE)
	{
		host->data_dir = TO_DEVICE;
		DataSetup.SDIO_TransferDir = SDIO_TransferDir_ToCard;
	}
		
	WARN_ON(data->blksz & (data->blksz -1));
	
	DataSetup.SDIO_DataBlockSize = ((ffs(data->blksz) -1) << 4);

	DataSetup.SDIO_DataTimeOut = SD_DATATIMEOUT;
	DataSetup.SDIO_DPSM = SDIO_DPSM_Enable;	

	sdh_enable_stat_irq(host, (SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND));
	
	host->xfert = XFERT_NONE;
	
	/* GET Source Buffer Addr from mmc_data struct */
	/* the buffer address can be retrived from the struct scatterlist sg->dma_address */
	host->DBuff = page_address(sg_page(&data->sg[0])) + data->sg[0].offset;
	host->Dlen = data->sg[0].length;
	
	if(host->data_dir == FROM_DEVICE)
	{
		/*read data from device */
		sdh_enable_stat_irq(host,SDIO_IT_RXFIFOHF);
		pr_debug("%s: DBuffer: 0x%p Dlen: 0x%x\n", __FUNCTION__, host->DBuff, host->Dlen);
	}
	else if(host->data_dir == TO_DEVICE)
		{
			/* write data to device */
			sdh_enable_stat_irq(host,SDIO_IT_TXFIFOE);
		}
		
	SDIO_DataConfig(&DataSetup);
	pr_debug("%s exit\n", __FUNCTION__);
}

static int sdh_cmd_done(struct sdh_host *host, unsigned int stat)
{
	struct mmc_command *cmd = host->cmd;

	pr_debug("%s enter stat:0x%x 0x%p\n", __FUNCTION__, stat, cmd);
	if (!cmd)
		return 0;

	host->cmd = NULL;

	if (cmd->flags & MMC_RSP_PRESENT) {
		cmd->resp[0] = SDIO_GetResponse(SDIO_RESP1);
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[1] = SDIO_GetResponse(SDIO_RESP2);
			cmd->resp[2] = SDIO_GetResponse(SDIO_RESP3);
			cmd->resp[3] = SDIO_GetResponse(SDIO_RESP4);
		}
	}
	if (stat & SDIO_IT_CTIMEOUT)
		cmd->error = -ETIMEDOUT;
	else if (stat & SDIO_IT_CCRCFAIL && cmd->flags & MMC_RSP_CRC)
		cmd->error = -EILSEQ;

	sdh_disable_stat_irq(host, (SDIO_IT_CMDSENT | SDIO_IT_CMDREND | SDIO_IT_CTIMEOUT | SDIO_IT_CCRCFAIL));

	if (host->data && !cmd->error) {
		if (host->data->flags & MMC_DATA_WRITE)
			sdh_setup_data(host, host->data);

		sdh_enable_stat_irq(host, SDIO_IT_DATAEND | SDIO_IT_RXOVERR | SDIO_IT_TXUNDERR | SDIO_IT_DTIMEOUT);
	} else
	{
		if(!host->mrq)
			return 0;
		sdh_finish_request(host, host->mrq);
	}

	return 1;
}

	/*FIXME: sdh_data_done should be reviewed.*/
static int sdh_data_done(struct sdh_host *host, unsigned int stat)
{
	struct mmc_data *data = host->data;

	pr_debug("%s enter stat:0x%x\n", __FUNCTION__, stat);
	if (!data)
		return 0;

	if (stat & SDIO_FLAG_DTIMEOUT)
		data->error = -ETIMEDOUT;
	else if (stat & SDIO_FLAG_DCRCFAIL)
		data->error = -EILSEQ;
	else if (stat & (SDIO_FLAG_RXOVERR | SDIO_FLAG_TXUNDERR))
		data->error = -EIO;

	if (!data->error)
		/* */
		data->bytes_xfered = data->blocks * data->blksz;
	else
		/* */
		data->bytes_xfered = data->blocks * data->blksz - SDIO_GetDataCounter();

	sdh_disable_stat_irq(host, SDIO_IT_DATAEND | SDIO_IT_DTIMEOUT | SDIO_IT_DCRCFAIL | SDIO_IT_RXOVERR | SDIO_IT_TXUNDERR);
#ifdef UseInterrupt	
	sdh_disable_stat_irq(host,SDIO_IT_RXFIFOHF|SDIO_IT_TXFIFOHE);
#endif
	SDIO_ClearFlag(SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | \
		SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DBCKEND | SDIO_FLAG_RXOVERR | SDIO_FLAG_TXUNDERR);
#ifdef UseInterrupt		
	SDIO_ClearFlag(SDIO_FLAG_RXFIFOHF|SDIO_FLAG_TXFIFOHE);
#endif

	/* Clear Data Control register */
	SDIO->DCTRL = 0;

	host->data = NULL;
	
	/* Handle Stop request*/
	if (host->mrq->stop) {
		sdh_stop_clock(host);
		sdh_start_cmd(host, host->mrq->stop);
	} else 
		sdh_finish_request(host, host->mrq);

	return 1;
}

static void sdh_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct sdh_host *host = mmc_priv(mmc);

	pr_debug("%s enter, mrp:%p, cmd:%p\n", __FUNCTION__, mrq, mrq->cmd);
	WARN_ON(host->mrq != NULL);

	host->mrq = mrq;
	host->data = mrq->data;

	if (mrq->data && mrq->data->flags & MMC_DATA_READ)
		sdh_setup_data(host, mrq->data);//setup DATA path.

	sdh_start_cmd(host, mrq->cmd);//setup CMD
}

static int sdh_get_ro(struct mmc_host *mmc)
{
	/*System does not support write protection detection let MMC Core decide.*/
	return -ENOSYS;
}

static void sdh_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sdh_host *host;
	SDIO_InitTypeDef SDIO_InitStruct;
	unsigned long flags;
	
	host = mmc_priv(mmc);
	
	SDIO_StructInit(&SDIO_InitStruct);
	
	spin_lock_irqsave(&host->lock, flags);
	if (ios->clock) {
		/* SETUP CLOCK */
		SDIO_InitStruct.SDIO_ClockDiv = \
		/* TODO Compute Clock Divider */
		/* (HCLK / ios->clock) - 2 */
#define HCLK 72000000
		(HCLK / (ios->clock) ) - 2;
		
	} else
		sdh_stop_clock(host);

	if (ios->bus_mode == MMC_BUSMODE_OPENDRAIN); /*FIXME SETUP Bus Mode */

	if (ios->bus_width == MMC_BUS_WIDTH_4) {
		/* Bus Width 4-bits */
		SDIO_InitStruct.SDIO_BusWide = SDIO_BusWide_4b;
	} else {
		/* Bus Width 1-bit */
		SDIO_InitStruct.SDIO_BusWide = SDIO_BusWide_1b;
	}
		
	SDIO_Init(&SDIO_InitStruct);
	
	host->power_mode = ios->power_mode;
	if (ios->power_mode == MMC_POWER_ON)/*Power on */
		SDIO_SetPowerState(SDIO_PowerState_ON);
	else if(ios->power_mode == MMC_POWER_OFF)
		SDIO_SetPowerState(SDIO_PowerState_OFF);

	spin_unlock_irqrestore(&host->lock, flags);

	pr_debug("Setup HOST clk_div: 0x%x bus Mode: 0x%x bus width: 0x%x\n",SDIO_InitStruct.SDIO_ClockDiv, ios->bus_mode, ios->bus_width);
}

static const struct mmc_host_ops sdh_ops = {
	.request	= sdh_request,
	.get_ro		= sdh_get_ro,
	.set_ios	= sdh_set_ios,
};

#define BPoint() asm("1: b 1b")
static irqreturn_t sdh_stat_irq(int irq, void *devid)
{
	struct sdh_host *host = devid;
	uint32_t status;
	int handled = 0;

	pr_debug("%s enter\n", __FUNCTION__);
	
#ifdef ReadWrite_OK	
	status = (uint32_t)SDIO->STA;
			
	if((status & (SDIO_FLAG_TXFIFOHE))&&(host->data_dir==TO_DEVICE))
	{
		/*TODO WRITE DATA TO THE CARD*/
		/*FIXME: Not Implemented yet.*/
		WARN_ON(1);
		handled |= 1;
	}
	else if((status & (SDIO_FLAG_RXFIFOHF)) && (host->data_dir==FROM_DEVICE))
	{
		/*TODO READ DATA FROM THE CARD*/
		/*FIXME: RXOVERR is set every time we want to read from the card.*/
		printk("FIFOCNT: 0x%x DATACNT 0x%x STAT: 0x%x\n",SDIO->FIFOCNT, SDIO->DCOUNT, SDIO->STA);
		while (SDIO->FIFOCNT != 0 && !((host->Dlen--) < 0))
		{
			*(host->DBuff++) = SDIO->FIFO;
		}
		/*
		host->xfert = XFERT_READING;
		while (!(SDIO->STA &(SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL |\
				 SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND | SDIO_FLAG_STBITERR)))
	    	{
	      		if ((SDIO->STA & SDIO_FLAG_RXFIFOHF))
	      		{
	        		for (count = 0; count < 8; count++)
        			{
	          			*(host->DBuff + count) = SDIO_ReadData();
        			}
	        		
	      		}
	    	}
	    	if(!(SDIO->STA &(SDIO_FLAG_RXOVERR |\
		 	SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND | SDIO_FLAG_STBITERR)))
    			while ((SDIO->STA & SDIO_FLAG_RXDAVL))
			{
				*host->DBuff = SDIO_ReadData();
				host->DBuff++;
			}
	    	
		host->xfert = XFERT_NONE;
		handled |= 1;*/
		handled |= 1;
	}
#endif	
	status = (uint32_t)SDIO->STA;
	
	/* TODO: GET CMD status if cmd error or cmd done call function sdh_cmd_done(); && Clear flags */
	
		
	if (status & (SDIO_IT_CMDSENT | SDIO_IT_CMDREND | SDIO_IT_CTIMEOUT | SDIO_IT_CCRCFAIL)) {
		handled |= sdh_cmd_done(host, status);
		SDIO_ClearFlag((SDIO_FLAG_CMDSENT | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL));
	}
	
	status = (uint32_t)SDIO->STA;
	
	/* TODO: GET DATA Status if data error or DATA done CALL function sdh_data_done(); */
	
	if (status & (SDIO_IT_DATAEND | SDIO_IT_DTIMEOUT | SDIO_IT_DCRCFAIL | SDIO_IT_RXOVERR | SDIO_IT_TXUNDERR))
		handled |= sdh_data_done(host, status);

	pr_debug("%s exit\n\n", __FUNCTION__);
	
	return IRQ_RETVAL(handled);
}

static void init_periph(void)
{
	/* Configure SDIO interface GPIO */
	GPIO_InitTypeDef  GPIO_InitStructure;

	/* GPIOC and GPIOD Periph clock enable */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);

	/* Configure PC.08, PC.09, PC.10, PC.11, PC.12 pin: D0, D1, D2, D3, CLK pin */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* Configure PD.02 CMD line */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* Enable the SDIO AHB Clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_SDIO, ENABLE);
}


static int __devinit sdh_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct sdh_host *host = NULL;
	stm32_sd_host *drv_data = get_sdh_data(pdev);
	int ret;
	
	pr_debug("%s: %s\n",DRIVER_NAME,__FUNCTION__);
	
	SDIO_DeInit();
	
	init_periph();
		
	if (!drv_data) {
		dev_err(&pdev->dev, "missing platform driver data\n");
		ret = -EINVAL;
		goto out;
	}

	mmc = mmc_alloc_host(sizeof(*mmc), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto out;
	}

	mmc->ops = &sdh_ops;
	
	mmc->max_phys_segs = 1 /*FIXME*/;
	mmc->max_seg_size = 4095 /*FIXME*/;
	mmc->max_blk_size = 4095 /*FIXME*/;
	mmc->max_blk_count = 1 /*FIXME*/;
	
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count /*FIXME*/;
	
	/*Avail OCR*/
	mmc->ocr_avail = MMC_VDD_27_28|MMC_VDD_28_29|MMC_VDD_29_30|\
			 MMC_VDD_30_31|MMC_VDD_31_32|MMC_VDD_32_33|\
			 MMC_VDD_33_34|MMC_VDD_34_35|MMC_VDD_35_36;
			 
	/*min frequency*/
	mmc->f_min = 400000; /*TODO define F_MIN*/
	/*Max frequency*/
	mmc->f_max = 1000000; /*TODO define F_MAX*/
	
	mmc->caps = MMC_CAP_4_BIT_DATA;
	
	host = mmc_priv(mmc);
	host->mmc = mmc;
	
	host->irq = drv_data->irq_num;

	ret = request_irq(host->irq, sdh_stat_irq, 0, "stm32 SD-Host Status IRQ", host);
	
	if (ret) {
		dev_err(&pdev->dev, "unable to request status irq\n");
		goto out1;
	}
	
	platform_set_drvdata(pdev, mmc);
	mmc_add_host(mmc);

	return 0;
out1:
	mmc_free_host(mmc);
 out:
	return ret;
}

static int __devexit sdh_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	if (mmc) {
		struct sdh_host *host = mmc_priv(mmc);
		mmc_remove_host(mmc);
		sdh_stop_clock(host);
		free_irq(host->irq, host);
		mmc_free_host(mmc);
	}
	remove_proc_entry("driver/sdh", NULL);

	return 0;
}

static struct platform_driver sdh_driver = {
	.probe		= sdh_probe,
	.remove		= __devexit_p(sdh_remove),
	.suspend	= sdh_suspend,
	.resume		= sdh_resume,
	.driver		= {
		.name	= DRIVER_NAME,
	},
};

static int __init sdh_init(void)
{
	pr_debug("%s: %s\n",DRIVER_NAME,__FUNCTION__);
	return platform_driver_register(&sdh_driver);
}

static void __exit sdh_exit(void)
{
	platform_driver_unregister(&sdh_driver);
}

module_init(sdh_init);
module_exit(sdh_exit);

MODULE_DESCRIPTION("STM32 Secure Digital Host Driver");
MODULE_AUTHOR("MCD Application Team");
MODULE_LICENSE("GPL");
