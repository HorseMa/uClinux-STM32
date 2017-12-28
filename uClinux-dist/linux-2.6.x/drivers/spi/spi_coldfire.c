/****************************************************************************/

/*
 *	coldfire.c - Master QSPI controller for the ColdFire processors
 *
 *	(C) Copyright 2005, Intec Automation,
 *			    Mike Lavender (mike@steroidmicros)
 *

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */


/****************************************************************************/

/*
 * Includes
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <asm/delay.h>
#include <asm/mcfsim.h>
#include <asm/mcfqspi.h>
#include <asm/coldfire.h>

MODULE_AUTHOR("Mike Lavender");
MODULE_DESCRIPTION("ColdFire QSPI Contoller");
MODULE_LICENSE("GPL");

/****************************************************************************/

/*
 * Local constants and macros
 */

#define QSPI_RAM_SIZE		0x10    /* 16 word table */

#define QSPI_TRANSMIT_RAM 	0x00
#define QSPI_RECEIVE_RAM  	0x10
#define QSPI_COMMAND_RAM  	0x20

#define QSPI_COMMAND		0x7000  /* 15:   X = Continuous CS
					 * 14:   1 = Get BITSE from QMR[BITS]
					 * 13:   1 = Get DT    from QDLYR[DTL]
					 * 12:   1 = Get DSK   from QDLYR[QCD]
					 * 8-11: XXXX = next 4 bytes for CS
					 * 0-7:  0000 0000 Reserved
					 */

#define QIR_WCEF                0x0008  /* write collison */
#define QIR_ABRT                0x0004  /* abort */
#define QIR_SPIF                0x0001  /* finished */

#define QIR_WCEFE              	0x0800
#define QIR_ABRTE              	0x0400
#define QIR_SPIFE              	0x0100

#define QIR_WCEFB              	0x8000
#define QIR_ABRTB              	0x4000
#define QIR_ABRTL              	0x1000

#define QMR_BITS             	0x3C00
#define QMR_BITS_8              0x2000

#define QCR_CONT              	0x8000

#define QDLYR_SPE		0x8000

#define QWR_ENDQP_MASK		0x0F00
#define QWR_CSIV		0x1000  /* 1 = active low chip selects */


#define START_STATE ((void*)0)
#define RUNNING_STATE ((void*)1)
#define DONE_STATE ((void*)2)
#define ERROR_STATE ((void*)-1)

#define QUEUE_RUNNING 0
#define QUEUE_STOPPED 1

/****************************************************************************/

/*
 * Local Data Structures
 */

struct transfer_state {
	u32 index;
	u32 len;
	void *tx;
	void *tx_end;
	void *rx;
	void *rx_end;
	char flags;
#define TRAN_STATE_RX_VOID	   0x01
#define TRAN_STATE_TX_VOID 	   0x02
#define TRAN_STATE_WORD_ODD_NUM	   0x04
	u8 cs;
	u16 void_write_data;
	unsigned cs_change:1;
};

typedef struct {
	unsigned master:1;
	unsigned dohie:1;
	unsigned bits:4;
	unsigned cpol:1;
	unsigned cpha:1;
	unsigned baud:8;
} QMR;

typedef struct {
	unsigned spe:1;
	unsigned qcd:7;
	unsigned dtl:8;
} QDLYR;

typedef struct {
	unsigned halt:1;
	unsigned wren:1;
	unsigned wrto:1;
	unsigned csiv:1;
	unsigned endqp:4;
	unsigned cptqp:4;
	unsigned newqp:4;
} QWR;


struct chip_data {
	union {
		u16 qmr_val;
		QMR qmr;
	};
	union {
		u16 qdlyr_val;
		QDLYR qdlyr;
	};
	union {
		u16 qwr_val;
		QWR qwr;
	};
	u16 void_write_data;
};


struct driver_data {
	/* Driver model hookup */
	struct platform_device *pdev;
	
	/* SPI framework hookup */
	struct spi_master *master;

	/* Driver message queue */
	struct workqueue_struct	*workqueue;
	struct work_struct pump_messages;
	spinlock_t lock;
	struct list_head queue;
	int busy;
	int run;

	/* Message Transfer pump */
	struct tasklet_struct pump_transfers;

	/* Current message transfer state info */
	struct spi_message* cur_msg;
	struct spi_transfer* cur_transfer;
	struct chip_data *cur_chip;
	size_t len;
	void *tx;
	void *tx_end;
	void *rx;
	void *rx_end;
	char flags;
#define TRAN_STATE_RX_VOID	   0x01
#define TRAN_STATE_TX_VOID 	   0x02
#define TRAN_STATE_WORD_ODD_NUM	   0x04
	u8 cs;
	u16 void_write_data;
	unsigned cs_change:1;
	
	u32 trans_cnt;
	u32 wce_cnt;
	u32 abrt_cnt;
 	u16 *qmr;          /* QSPI mode register      */
 	u16 *qdlyr;        /* QSPI delay register     */
 	u16 *qwr;	   /* QSPI wrap register      */
 	u16 *qir;          /* QSPI interrupt register */
 	u16 *qar;          /* QSPI address register   */
 	u16 *qdr;          /* QSPI data register      */
 	u16 *qcr;	   /* QSPI command register   */
 	u8  *par;	   /* Pin assignment register */
 	u8  *int_icr;	   /* Interrupt level and priority register */
 	u32 *int_mr;       /* Interrupt mask register */
 	void (*cs_control)(u8 cs, u8 command);
};



/****************************************************************************/

/*
 * SPI local functions
 */

//#define SPI_COLDFIRE_DEBUG

static void *next_transfer(struct driver_data *drv_data)
{
	struct spi_message *msg = drv_data->cur_msg;
	struct spi_transfer *trans = drv_data->cur_transfer;

	/* Move to next transfer */
	if (trans->transfer_list.next != &msg->transfers) {
		drv_data->cur_transfer =
			list_entry(trans->transfer_list.next,
					struct spi_transfer,
					transfer_list);
		return RUNNING_STATE;
	} else
		return DONE_STATE;
}

static int write(struct driver_data *drv_data)
{
 	int tx_count = 0;
 	int cmd_count = 0;
 	int tx_word = ((*drv_data->qmr & QMR_BITS) == QMR_BITS_8) ? 0 : 1;

	// If we are in word mode, but only have a single byte to transfer
	// then switch to byte mode temporarily.  Will switch back at the
	// end of the transfer.
 	if (tx_word && ((drv_data->tx_end - drv_data->tx) == 1)) {
 		drv_data->flags |= TRAN_STATE_WORD_ODD_NUM;
 		*drv_data->qmr |= (*drv_data->qmr & ~QMR_BITS) | QMR_BITS_8;
 		tx_word = 0;
	}

 	*drv_data->qar = QSPI_TRANSMIT_RAM;
 	while ((drv_data->tx < drv_data->tx_end) && (tx_count < QSPI_RAM_SIZE)) {
 		if (tx_word) {
			if ((drv_data->tx_end - drv_data->tx) == 1)
				break;

 			if (!(drv_data->flags & TRAN_STATE_TX_VOID))
 				*drv_data->qdr = *(u16 *)drv_data->tx;
 			else
 				*drv_data->qdr = drv_data->void_write_data;
 			drv_data->tx += 2;
 		} else {
 			if (!(drv_data->flags & TRAN_STATE_TX_VOID))
 				*drv_data->qdr = *(u8 *)drv_data->tx;
 			else
 				*drv_data->qdr = *(u8 *)&drv_data->void_write_data;
 			drv_data->tx++;
 		}
 		tx_count++;
 	}


 	*drv_data->qar = QSPI_COMMAND_RAM;
 	while (cmd_count < tx_count) {
		u16 qcr =   QSPI_COMMAND
			  | QCR_CONT
			  | (~((0x01 << drv_data->cs) << 8) & 0x0F00);

		if ( 	   (cmd_count == tx_count - 1)
			&& (drv_data->tx == drv_data->tx_end)
			&& (drv_data->cs_change) ) {
			qcr &= ~QCR_CONT;
		}
		*drv_data->qcr = qcr;
		cmd_count++;
	}

	*drv_data->qwr = (*drv_data->qwr & ~QWR_ENDQP_MASK) | ((cmd_count - 1) << 8);

 	/* Fire it up! */
 	*drv_data->qdlyr |= QDLYR_SPE;

 	return tx_count;
}


static int read(struct driver_data *drv_data)
{
	int rx_count = 0;
	int rx_word = ((*drv_data->qmr & QMR_BITS) == QMR_BITS_8) ? 0 : 1;

	*drv_data->qar = QSPI_RECEIVE_RAM;
	while ((drv_data->rx < drv_data->rx_end) && (rx_count < QSPI_RAM_SIZE)) {
		if (rx_word) {
			if ((drv_data->rx_end - drv_data->rx) == 1)
				break;

			if (!(drv_data->flags & TRAN_STATE_RX_VOID))
				*(u16 *)drv_data->rx = *drv_data->qdr;
			drv_data->rx += 2;
		} else {
			if (!(drv_data->flags & TRAN_STATE_RX_VOID))
				*(u8 *)drv_data->rx = *drv_data->qdr;
			drv_data->rx++;
		}
		rx_count++;
	}

	return rx_count;
}


static inline void qspi_setup_chip(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;

	*drv_data->qmr = chip->qmr_val;
	*drv_data->qdlyr = chip->qdlyr_val;
	*drv_data->qwr = chip->qwr_val;

	/*
	 * Enable all the interrupts and clear all the flags
	 */
	*drv_data->qir =  (QIR_SPIFE | QIR_ABRTE | QIR_WCEFE)
			| (QIR_WCEFB | QIR_ABRTB | QIR_ABRTL)
			| (QIR_SPIF  | QIR_ABRT  | QIR_WCEF);
}


static irqreturn_t qspi_interrupt(int irq, void *dev_id)
{
	struct driver_data *drv_data = (struct driver_data *)dev_id;
	struct spi_message *msg = drv_data->cur_msg;
	u16 irq_status = *drv_data->qir;

	/* Clear all flags immediately */
	*drv_data->qir |= (QIR_SPIF | QIR_ABRT | QIR_WCEF);

	if (!drv_data->cur_msg || !drv_data->cur_msg->state) {
		printk(KERN_ERR "coldfire-qspi: bad message or transfer "
		"state in interrupt handler\n");
		return IRQ_NONE;
	}

	if (irq_status & QIR_SPIF) {
		/*
		 * Read the data into the buffer and reload and start
		 * queue with new data if not finished.  If finished
		 * then setup the next transfer
		 */
		 read(drv_data);

		 if (drv_data->rx == drv_data->rx_end) {
			/*
			 * Finished now - fall through and schedule next
			 * transfer tasklet
			 */
			if (drv_data->flags & TRAN_STATE_WORD_ODD_NUM)
				*drv_data->qmr &= ~QMR_BITS;

			msg->state = next_transfer(drv_data);
			msg->actual_length += drv_data->len;
		 } else {
			/* not finished yet - keep going */
		 	write(drv_data);
		 	return IRQ_HANDLED;
		}
	} else {
		if (irq_status & QIR_WCEF)
			drv_data->wce_cnt++;

		if (irq_status & QIR_ABRT)
			drv_data->abrt_cnt++;

		msg->state = ERROR_STATE;
	}

	tasklet_schedule(&drv_data->pump_transfers);

	return IRQ_HANDLED;
}

/* caller already set message->status; dma and pio irqs are blocked */
static void giveback(struct driver_data *drv_data)
{
	struct spi_transfer* last_transfer;
	unsigned long flags;
	struct spi_message *msg;

	spin_lock_irqsave(&drv_data->lock, flags);
	msg = drv_data->cur_msg;
	drv_data->cur_msg = NULL;
	drv_data->cur_transfer = NULL;
	drv_data->cur_chip = NULL;
	queue_work(drv_data->workqueue, &drv_data->pump_messages);
	spin_unlock_irqrestore(&drv_data->lock, flags);

	last_transfer = list_entry(msg->transfers.prev,
					struct spi_transfer,
					transfer_list);

	if (!last_transfer->cs_change)
		drv_data->cs_control(drv_data->cs, QSPI_CS_DROP);

	msg->state = NULL;
	if (msg->complete)
		msg->complete(msg->context);
}


static void pump_transfers(unsigned long data)
{
	struct driver_data *drv_data = (struct driver_data *)data;
	struct spi_message *message = NULL;
	struct spi_transfer *transfer = NULL;
	struct spi_transfer *previous = NULL;
	struct chip_data *chip = NULL;
	unsigned long flags;

	/* Get current state information */
	message = drv_data->cur_msg;
	transfer = drv_data->cur_transfer;
	chip = drv_data->cur_chip;

	/* Handle for abort */
	if (message->state == ERROR_STATE) {
		message->status = -EIO;
		giveback(drv_data);
		return;
	}

	/* Handle end of message */
	if (message->state == DONE_STATE) {
		message->status = 0;
		giveback(drv_data);
		return;
	}
	
	if (message->state == START_STATE) {
		qspi_setup_chip(drv_data);
		
		if (drv_data->cs_control) {
			//printk( "m s\n" );
        		drv_data->cs_control(message->spi->chip_select, QSPI_CS_ASSERT);
		}
	}

	/* Delay if requested at end of transfer*/
	if (message->state == RUNNING_STATE) {
		previous = list_entry(transfer->transfer_list.prev,
					struct spi_transfer,
					transfer_list);
					
		if (drv_data->cs_control && transfer->cs_change)
			drv_data->cs_control(message->spi->chip_select, QSPI_CS_DROP);
					
		if (previous->delay_usecs)
			udelay(previous->delay_usecs);
			
		if (drv_data->cs_control && transfer->cs_change)
			drv_data->cs_control(message->spi->chip_select, QSPI_CS_ASSERT);
	}

	drv_data->flags = 0;
	drv_data->tx = (void *)transfer->tx_buf;
	drv_data->tx_end = drv_data->tx + transfer->len;
	drv_data->rx = transfer->rx_buf;
	drv_data->rx_end = drv_data->rx + transfer->len;
	drv_data->len = transfer->len;
	if (!drv_data->rx)
		drv_data->flags |= TRAN_STATE_RX_VOID;
	if (!drv_data->tx)
		drv_data->flags |= TRAN_STATE_TX_VOID;
	drv_data->cs = message->spi->chip_select;
	drv_data->cs_change = transfer->cs_change;
	drv_data->void_write_data = chip->void_write_data;
	
	message->state = RUNNING_STATE;
	
	/* Go baby, go */
	local_irq_save(flags);
	write(drv_data);
	local_irq_restore(flags);
}


static void pump_messages(void * data)
{
	struct driver_data *drv_data = data;
	unsigned long flags;

	/* Lock queue and check for queue work */
	spin_lock_irqsave(&drv_data->lock, flags);
	if (list_empty(&drv_data->queue) || drv_data->run == QUEUE_STOPPED) {
		drv_data->busy = 0;
		spin_unlock_irqrestore(&drv_data->lock, flags);
		return;
	}

	/* Make sure we are not already running a message */
	if (drv_data->cur_msg) {
		spin_unlock_irqrestore(&drv_data->lock, flags);
		return;
	}

	/* Extract head of queue */
	drv_data->cur_msg = list_entry(drv_data->queue.next,
					struct spi_message, queue);
	list_del_init(&drv_data->cur_msg->queue);

	/* Initial message state*/
	drv_data->cur_msg->state = START_STATE;
	drv_data->cur_transfer = list_entry(drv_data->cur_msg->transfers.next,
						struct spi_transfer,
						transfer_list);
						
	/* Setup the SPI Registers using the per chip configuration */
	drv_data->cur_chip = spi_get_ctldata(drv_data->cur_msg->spi);

	/* Mark as busy and launch transfers */
	tasklet_schedule(&drv_data->pump_transfers);

	drv_data->busy = 1;
	spin_unlock_irqrestore(&drv_data->lock, flags);
}

/****************************************************************************/

/*
 * SPI master implementation
 */

static int transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct driver_data *drv_data = spi_master_get_devdata(spi->master);
	unsigned long flags;

	spin_lock_irqsave(&drv_data->lock, flags);

	if (drv_data->run == QUEUE_STOPPED) {
		spin_unlock_irqrestore(&drv_data->lock, flags);
		return -ESHUTDOWN;
	}

	msg->actual_length = 0;
	msg->status = -EINPROGRESS;
	msg->state = START_STATE;

	list_add_tail(&msg->queue, &drv_data->queue);

	if (drv_data->run == QUEUE_RUNNING && !drv_data->busy)
		queue_work(drv_data->workqueue, &drv_data->pump_messages);

	spin_unlock_irqrestore(&drv_data->lock, flags);

	return 0;
}


static int setup(struct spi_device *spi)
{
	struct coldfire_spi_chip *chip_info;
	struct chip_data *chip;
	u32 baud_divisor = 255;

	chip_info = (struct coldfire_spi_chip *)spi->controller_data;

	/* Only alloc on first setup */
	chip = spi_get_ctldata(spi);
	if (chip == NULL) {
 		chip = kcalloc(1, sizeof(struct chip_data), GFP_KERNEL);
		if (!chip)
			return -ENOMEM;
		spi->mode = chip_info->mode;
		spi->bits_per_word = chip_info->bits_per_word;
	}

	chip->qwr.csiv = 1;    // Chip selects are active low
	chip->qmr.master = 1;  // Must set to master mode
	chip->qmr.dohie = 1;   // Data output high impediance enabled
	chip->void_write_data = chip_info->void_write_data;

	chip->qdlyr.qcd = chip_info->del_cs_to_clk;
	chip->qdlyr.dtl = chip_info->del_after_trans;

	if (spi->max_speed_hz != 0)
		baud_divisor = (MCF_CLK/(2*spi->max_speed_hz));

	if (baud_divisor < 2)
		baud_divisor = 2;

	if (baud_divisor > 255)
		baud_divisor = 255;

	chip->qmr.baud = baud_divisor;

	//printk( "QSPI: spi->max_speed_hz %d\n", spi->max_speed_hz );
	//printk( "QSPI: Baud set to %d\n", chip->qmr.baud );

	if (spi->mode & SPI_CPHA)
		chip->qmr.cpha = 1;

	if (spi->mode & SPI_CPOL)
		chip->qmr.cpol = 1;

	if (spi->bits_per_word == 16) {
		chip->qmr.bits = 0;
	} else if ((spi->bits_per_word >= 8) && (spi->bits_per_word <= 15)) {
		chip->qmr.bits = spi->bits_per_word;
	} else {
		printk(KERN_ERR "coldfire-qspi: invalid wordsize\n");
		kfree(chip);
		return -ENODEV;
	}

 	spi_set_ctldata(spi, chip);

 	return 0;
}

static int init_queue(struct driver_data *drv_data)
{
	INIT_LIST_HEAD(&drv_data->queue);
	spin_lock_init(&drv_data->lock);

	drv_data->run = QUEUE_STOPPED;
	drv_data->busy = 0;

	tasklet_init(&drv_data->pump_transfers,
			pump_transfers,	(unsigned long)drv_data);

	INIT_WORK(&drv_data->pump_messages, pump_messages, drv_data);
	drv_data->workqueue = create_singlethread_workqueue(
					drv_data->master->cdev.dev->bus_id);
	if (drv_data->workqueue == NULL)
		return -EBUSY;

	return 0;
}

static int start_queue(struct driver_data *drv_data)
{
	unsigned long flags;

	spin_lock_irqsave(&drv_data->lock, flags);

	if (drv_data->run == QUEUE_RUNNING || drv_data->busy) {
		spin_unlock_irqrestore(&drv_data->lock, flags);
		return -EBUSY;
	}

	drv_data->run = QUEUE_RUNNING;
	drv_data->cur_msg = NULL;
	drv_data->cur_transfer = NULL;
	drv_data->cur_chip = NULL;
	spin_unlock_irqrestore(&drv_data->lock, flags);

	queue_work(drv_data->workqueue, &drv_data->pump_messages);

	return 0;
}

static int stop_queue(struct driver_data *drv_data)
{
	unsigned long flags;
	unsigned limit = 500;
	int status = 0;

	spin_lock_irqsave(&drv_data->lock, flags);

	/* This is a bit lame, but is optimized for the common execution path.
	 * A wait_queue on the drv_data->busy could be used, but then the common
	 * execution path (pump_messages) would be required to call wake_up or
	 * friends on every SPI message. Do this instead */
	drv_data->run = QUEUE_STOPPED;
	while (!list_empty(&drv_data->queue) && drv_data->busy && limit--) {
		spin_unlock_irqrestore(&drv_data->lock, flags);
		msleep(10);
		spin_lock_irqsave(&drv_data->lock, flags);
	}

	if (!list_empty(&drv_data->queue) || drv_data->busy)
		status = -EBUSY;

	spin_unlock_irqrestore(&drv_data->lock, flags);

	return status;
}

static int destroy_queue(struct driver_data *drv_data)
{
	int status;

	status = stop_queue(drv_data);
	if (status != 0)
		return status;

	destroy_workqueue(drv_data->workqueue);

	return 0;
}


static void cleanup(const struct spi_device *spi)
{
	struct chip_data *chip = spi_get_ctldata((struct spi_device *)spi);

 	dev_dbg(&spi->dev, "spi_device %u.%u cleanup\n",
 		spi->master->bus_num, spi->chip_select);

	kfree(chip);
}


/****************************************************************************/

/*
 * Generic Device driver routines and interface implementation
 */

static int coldfire_spi_probe(struct platform_device *pdev)
{
 	struct device *dev = &pdev->dev;
 	struct coldfire_spi_master *platform_info;
 	struct spi_master *master;
 	struct driver_data *drv_data = 0;
 	struct resource *memory_resource;
	int irq;
	int status = 0;
	int i;

 	platform_info = (struct coldfire_spi_master *)pdev->dev.platform_data;

  	master = spi_alloc_master(dev, sizeof(struct driver_data));
  	if (!master)
 		return -ENOMEM;

 	drv_data = class_get_devdata(&master->cdev);
 	drv_data->master = master;

	INIT_LIST_HEAD(&drv_data->queue);
	spin_lock_init(&drv_data->lock);

	master->bus_num = platform_info->bus_num;
	master->num_chipselect = platform_info->num_chipselect;
	master->cleanup = cleanup;
	master->setup = setup;
	master->transfer = transfer;


 	drv_data->cs_control = platform_info->cs_control;
 	if (drv_data->cs_control)
 		for(i = 0; i < master->num_chipselect; i++)
 			drv_data->cs_control(i, QSPI_CS_INIT | QSPI_CS_DROP);

	/* Setup register addresses */
 	memory_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qspi-module");
 	if (!memory_resource) {
 		dev_dbg(dev, "can not find platform module memory\n");
 		goto out_error_master_alloc;
 	}

 	drv_data->qmr   = (void *)(memory_resource->start + 0x00000000);
 	drv_data->qdlyr = (void *)(memory_resource->start + 0x00000004);
 	drv_data->qwr   = (void *)(memory_resource->start + 0x00000008);
 	drv_data->qir   = (void *)(memory_resource->start + 0x0000000c);
 	drv_data->qar   = (void *)(memory_resource->start + 0x00000010);
 	drv_data->qdr   = (void *)(memory_resource->start + 0x00000014);
 	drv_data->qcr   = (void *)(memory_resource->start + 0x00000014);

	/* Setup register addresses */
 	memory_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qspi-par");
 	if (!memory_resource) {
 		dev_dbg(dev, "can not find platform par memory\n");
 		goto out_error_master_alloc;
 	}

 	drv_data->par = (void *)memory_resource->start;

	/* Setup register addresses */
 	memory_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qspi-int-level");
 	if (!memory_resource) {
 		dev_dbg(dev, "can not find platform par memory\n");
 		goto out_error_master_alloc;
 	}

 	drv_data->int_icr = (void *)memory_resource->start;

	/* Setup register addresses */
 	memory_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qspi-int-mask");
 	if (!memory_resource) {
 		dev_dbg(dev, "can not find platform par memory\n");
 		goto out_error_master_alloc;
 	}

 	drv_data->int_mr = (void *)memory_resource->start;

	status = request_irq(platform_info->irq_vector, qspi_interrupt, SA_INTERRUPT, dev->bus_id, drv_data);
	if (status < 0) {
		dev_err(&pdev->dev, "unable to attach ColdFire QSPI interrupt\n");
		goto out_error_master_alloc;
	}

        /* Now that we have all the addresses etc.  Let's set it up */
        *drv_data->par = platform_info->par_val;
        *drv_data->int_icr = platform_info->irq_lp;
        *drv_data->int_mr &= ~platform_info->irq_mask;
	
	/* Initial and start queue */
	status = init_queue(drv_data);
	if (status != 0) {
		dev_err(&pdev->dev, "problem initializing queue\n");
		goto out_error_irq_alloc;
	}
	status = start_queue(drv_data);
	if (status != 0) {
		dev_err(&pdev->dev, "problem starting queue\n");
		goto out_error_irq_alloc;
	}

	/* Register with the SPI framework */
	platform_set_drvdata(pdev, drv_data);
	status = spi_register_master(master);
	if (status != 0) {
		dev_err(&pdev->dev, "problem registering spi master\n");
		status = -EINVAL;
                goto out_error_queue_alloc;
	}
	
	printk( "SPI: Coldfire master initialized\n" );
	//dev_info(&pdev->dev, "driver initialized\n");
	return status;

out_error_queue_alloc:
	destroy_queue(drv_data);
	
out_error_irq_alloc:
	free_irq(irq, drv_data);
	
out_error_master_alloc:
	spi_master_put(master);
	return status;

}

static int coldfire_spi_remove(struct platform_device *pdev)
{
	struct driver_data *drv_data = platform_get_drvdata(pdev);
	int irq;
	int status = 0;

	if (!drv_data)
		return 0;

	/* Remove the queue */
	status = destroy_queue(drv_data);
	if (status != 0)
		return status;

	/* Disable the SSP at the peripheral and SOC level */
	/*write_SSCR0(0, drv_data->ioaddr);
	pxa_set_cken(drv_data->master_info->clock_enable, 0);*/

	/* Release DMA */
	/*if (drv_data->master_info->enable_dma) {
		if (drv_data->ioaddr == SSP1_VIRT) {
			DRCMRRXSSDR = 0;
			DRCMRTXSSDR = 0;
		} else if (drv_data->ioaddr == SSP2_VIRT) {
			DRCMRRXSS2DR = 0;
			DRCMRTXSS2DR = 0;
		} else if (drv_data->ioaddr == SSP3_VIRT) {
			DRCMRRXSS3DR = 0;
			DRCMRTXSS3DR = 0;
		}
		pxa_free_dma(drv_data->tx_channel);
		pxa_free_dma(drv_data->rx_channel);
	}*/

	/* Release IRQ */
	irq = platform_get_irq(pdev, 0);
	if (irq >= 0)
		free_irq(irq, drv_data);

	/* Disconnect from the SPI framework */
	spi_unregister_master(drv_data->master);

	/* Prevent double remove */
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void coldfire_spi_shutdown(struct platform_device *pdev)
{
	int status = 0;

	if ((status = coldfire_spi_remove(pdev)) != 0)
		dev_err(&pdev->dev, "shutdown failed with %d\n", status);
}


#ifdef CONFIG_PM
static int suspend_devices(struct device *dev, void *pm_message)
{
	pm_message_t *state = pm_message;

	if (dev->power.power_state.event != state->event) {
		dev_warn(dev, "pm state does not match request\n");
		return -1;
	}

	return 0;
}

static int coldfire_spi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct driver_data *drv_data = platform_get_drvdata(pdev);
	int status = 0;

	/* Check all childern for current power state */
	if (device_for_each_child(&pdev->dev, &state, suspend_devices) != 0) {
		dev_warn(&pdev->dev, "suspend aborted\n");
		return -1;
	}

	status = stop_queue(drv_data);
	if (status != 0)
		return status;
	/*write_SSCR0(0, drv_data->ioaddr);
	pxa_set_cken(drv_data->master_info->clock_enable, 0);*/

	return 0;
}

static int coldfire_spi_resume(struct platform_device *pdev)
{
	struct driver_data *drv_data = platform_get_drvdata(pdev);
	int status = 0;

	/* Enable the SSP clock */
	/*pxa_set_cken(drv_data->master_info->clock_enable, 1);*/

	/* Start the queue running */
	status = start_queue(drv_data);
	if (status != 0) {
		dev_err(&pdev->dev, "problem starting queue (%d)\n", status);
		return status;
	}

	return 0;
}
#else
#define coldfire_spi_suspend NULL
#define coldfire_spi_resume NULL
#endif /* CONFIG_PM */

static struct platform_driver driver = {
	.driver = {
		.name = "spi_coldfire",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = coldfire_spi_probe,
	.remove = __devexit_p(coldfire_spi_remove),
	.shutdown = coldfire_spi_shutdown,
	.suspend = coldfire_spi_suspend,
	.resume = coldfire_spi_resume,
};

static int __init coldfire_spi_init(void)
{
	platform_driver_register(&driver);

	return 0;
}
module_init(coldfire_spi_init);

static void __exit coldfire_spi_exit(void)
{
	platform_driver_unregister(&driver);
}
module_exit(coldfire_spi_exit);
