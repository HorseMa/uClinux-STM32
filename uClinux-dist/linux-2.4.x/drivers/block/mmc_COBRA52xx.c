/*
 * linux-2.4.x/drivers/block/mmc_COBRA52xx.c 
 *
 * MMC/SD-Card driver
 *
 * Copyright (C) 2004 senTec Elektronik (wolfgang.elsner@sentec-elektronik.de)
 * 
 * It is a adapted version of the mmc.c at http://www.blumba.de that is
 * Copyright (C) 2003 Thomas Fehrenbacher <thomas@blumba.de>
 */

#include <asm/uaccess.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>

#include <linux/delay.h>
#include <linux/timer.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkpg.h>
#include <linux/hdreg.h>
#include <linux/major.h>

#define DEVICE_NAME "mmc"
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define MAJOR_NR 121

#include <linux/blk.h>

#if defined(CONFIG_COBRA5282)
#include <linux/mmc-COBRA5282.h>
#elif defined(CONFIG_COBRA5272)
#include <linux/mmc-COBRA5272.h>
#endif


MODULE_AUTHOR("Wolfgang Elsner <wolfgang.elsner@sentec-elektronik.de>");
MODULE_DESCRIPTION("Driver for MMC/SD-Cards");
#if defined(CONFIG_COBRA5282)
   MODULE_SUPPORTED_DEVICE("Motorola MCF5282");
#elif defined(CONFIG_COBRA5272)
   MODULE_SUPPORTED_DEVICE("Motorola MCF5272");
#endif
MODULE_LICENSE("GPL");


/* we have only one device */
static int mmc_blk_sizes[1] = {0};
static int mmc_blksize_sizes[1] = {0};
static int mmc_hardsect_sizes[1] = {0};
static int mmc_sectors = 0;

static struct timer_list mmc_timer;
static int mmc_media_detect = 0;
static int mmc_media_changed = 1;


unsigned char mmc_spi_io(unsigned char data_out)
{
	unsigned char data_in;

   QAR = COMMAND_RAM_START;
   QDR = QCR_CONT | QCR_CS; //QSPI_CS

	QAR = TX_RAM_START;
	QDR = data_out;
	QIR = 0x0001;
	QDLYR = QDLYR_SPE;
	while (!((QIR) & QIR_SPIF) ) ;
	QAR = RX_RAM_START;
	data_in = QDR;

	return(data_in);
}

int mmc_write_block(unsigned int dest_addr, unsigned int src_addr)
{
	char *data;
	unsigned int address;
	unsigned char r = 0;
	unsigned char ab0, ab1, ab2, ab3;
	int i;

	data = (char *)src_addr;
	address = dest_addr;

	ab3 = 0xff & (address >> 24);
	ab2 = 0xff & (address >> 16);
	ab1 = 0xff & (address >> 8);
	ab0 = 0xff & address;
	
   for (i = 0; i < 4; i++) mmc_spi_io(0xff);
	mmc_spi_io(0x58);
	mmc_spi_io(ab3); /* msb */
	mmc_spi_io(ab2);
	mmc_spi_io(ab1);
	mmc_spi_io(ab0); /* lsb */
	mmc_spi_io(0xff);
	for (i = 0; i < 8; i++)
	{
		r = mmc_spi_io(0xff);
		if (r == 0x00) break;
	}
	if (r != 0x00)
	{
		mmc_spi_io(0xff);
		return(1);
	}
	mmc_spi_io(0xff);
	mmc_spi_io(0xfe);
	for (i = 0; i < 512; i++) mmc_spi_io(data[i]);
	for (i = 0; i < 2; i++) mmc_spi_io(0xff);
	r = mmc_spi_io(0xff);
	if ((r & 0x05) != 0x05)
	{
		mmc_spi_io(0xff);
		return(2);
	}
	for (i = 0; i < 1000000; i++)
	{
		r = mmc_spi_io(0xff);
		if (r != 0x00) break;
	}
	if (r == 0x00)
	{
		mmc_spi_io(0xff);
		return(3);
	}
	mmc_spi_io(0xff);
	return(0);
}

int mmc_read_block(unsigned int dest_addr, unsigned int src_addr)
{
	char *data;
	unsigned int address;
	unsigned char r = 0;
	unsigned char ab0, ab1, ab2, ab3;
	int i;

	data = (char *)dest_addr;
	address = src_addr;

	ab3 = 0xff & (address >> 24);
	ab2 = 0xff & (address >> 16);
	ab1 = 0xff & (address >> 8);
	ab0 = 0xff & address;

	for (i = 0; i < 4; i++) mmc_spi_io(0xff);
	mmc_spi_io(0x51);
	mmc_spi_io(ab3); /* msb */
	mmc_spi_io(ab2);
	mmc_spi_io(ab1);
	mmc_spi_io(ab0); /* lsb */

	mmc_spi_io(0xff);
	for (i = 0; i < 8; i++)
	{
		r = mmc_spi_io(0xff);
		if (r == 0x00) break;
	}
	if (r != 0x00)
	{
		mmc_spi_io(0xff);
		return(1);
	}
	for (i = 0; i < 100000; i++)
	{
		r = mmc_spi_io(0xff);
		if (r == 0xfe) break;
	}
	if (r != 0xfe)
	{
		mmc_spi_io(0xff);
		return(2);
	}
	for (i = 0; i < 512; i++)
	{
		r = mmc_spi_io(0xff);
		data[i] = r;
	}
	for (i = 0; i < 2; i++)
	{
		r = mmc_spi_io(0xff);
	}
	mmc_spi_io(0xff);

	return(0);
}

void mmc_request(request_queue_t *q)
{
	unsigned int mmc_address;
	unsigned int buffer_address;
	int nr_sectors;
	int i;
	int cmd;
	int rc;

	//printk("mmc_request\n"); 
	while(1)
	{
		INIT_REQUEST;
		mmc_address = CURRENT->sector * mmc_hardsect_sizes[0];
		buffer_address = (unsigned int)CURRENT->buffer;
		nr_sectors = CURRENT->current_nr_sectors;
		cmd = CURRENT->cmd;
		if (((CURRENT->sector + CURRENT->current_nr_sectors) >= mmc_sectors) || (mmc_media_detect == 0))
		{
			end_request(0);
			continue;
		}
		else if (cmd == READ)
		{
			for (i = 0; i < nr_sectors; i++)
			{
				rc = mmc_read_block(buffer_address, mmc_address);
				if (rc != 0)
				{
					printk("mmc: error in mmc_read_block (%d)\n", rc);
					end_request(0);
					continue;
				}
				else
				{
					mmc_address += mmc_hardsect_sizes[0];
					buffer_address += mmc_hardsect_sizes[0];
				}
			}
			end_request(1);

		}
		else if (cmd == WRITE)
		{
			for (i = 0; i < nr_sectors; i++)
			{
				rc = mmc_write_block(mmc_address, buffer_address);
				if (rc != 0)
				{
					printk("mmc: error in mmc_read_block (%d)\n", rc);
					end_request(0);
					continue;
				}
				else
				{
					mmc_address += mmc_hardsect_sizes[0];
					buffer_address += mmc_hardsect_sizes[0];
				}
			}
			end_request(1);
		}
		else
		{
			end_request(0);
			continue;
		}
	}
}


int mmc_open(struct inode *inode, struct file *filp)
{
	int device;
	
	//printk("mmc_open\n"); 

	if (mmc_media_detect == 0) return -ENODEV;
	device = DEVICE_NR(inode->i_rdev);
	if (MINOR(device) != 0)
	{
		printk("mmc_open of %d failed\n", MINOR(device));
		return -ENODEV;
	}
#if defined(MODULE)
	MOD_INC_USE_COUNT;
#endif
	return 0;
}

int mmc_release(struct inode *inode, struct file *filp)
{
	//printk("mmc_release\n"); 

	fsync_dev(inode->i_rdev);
        invalidate_buffers(inode->i_rdev);

#if defined(MODULE)
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}

int mmc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct hd_geometry geometry;
	
	//printk("mmc_ioctl\n"); 

	if (cmd == BLKGETSIZE)
	{
		//printk("   BLKGETSIZE\n");
		if (!arg) return -EINVAL;
		if (copy_to_user((long *) arg, (long *) &mmc_sectors, sizeof(long))) return -EFAULT;
		return 0;
	}
	else if (cmd == HDIO_GETGEO)
	{
		//printk("   HDIO_GETGEO\n");
		if (!arg) return -EINVAL;
		geometry.sectors = mmc_sectors;
		geometry.heads = 0;
		geometry.cylinders = 0;
		geometry.start = 0;
		if (copy_to_user((void *) arg, &geometry, sizeof(geometry))) return -EFAULT;
		return 0;
	}
	else return blk_ioctl(inode->i_rdev, cmd, arg);
}

int mmc_card_init(void)
{
	unsigned char r = 0;
	short i, j;
	unsigned long flags;

	//printk("mmc_card_init\n"); 

	save_flags(flags);
	cli();

	for (i = 0; i < 10; i++) mmc_spi_io(0xff);

	for (i = 0; i < 4; i++) mmc_spi_io(0xff);
	mmc_spi_io(0x40);
	for (i = 0; i < 4; i++) mmc_spi_io(0x00);
	mmc_spi_io(0x95);
	for (i = 0; i < 8; i++)
	{
		r = mmc_spi_io(0xff);
		if (r == 0x01) break;
	}
	mmc_spi_io(0xff);
	if (r != 0x01)
	{
		restore_flags(flags);
		return(1);
	}

	for (j = 0; j < 10000; j++)
	{
		for (i = 0; i < 4; i++) mmc_spi_io(0xff);
		mmc_spi_io(0x41);
		for (i = 0; i < 4; i++) mmc_spi_io(0x00);
		mmc_spi_io(0xff);
		for (i = 0; i < 8; i++)
		{
			r = mmc_spi_io(0xff);
			if (r == 0x00) break;
		}
		mmc_spi_io(0xff);
		if (r == 0x00)
		{
			restore_flags(flags);
			return(0);
		}
	}
	restore_flags(flags);

	return(2);
}

int mmc_card_config(void)
{
	unsigned char r = 0;
	short i;
	unsigned char csd[32];
	unsigned int c_size;
	unsigned int c_size_mult;
	unsigned int mult;
	unsigned int read_bl_len;
	unsigned int blocknr = 0;
	unsigned int block_len = 0;
	unsigned int size = 0;

	//printk("mmc_card_config\n"); 

	for (i = 0; i < 4; i++) mmc_spi_io(0xff);
	mmc_spi_io(0x49);
	for (i = 0; i < 4; i++) mmc_spi_io(0x00);
	mmc_spi_io(0xff);
	for (i = 0; i < 8; i++)
	{
		r = mmc_spi_io(0xff);
		if (r == 0x00) break;
	}
	if (r != 0x00)
	{
		mmc_spi_io(0xff);
		return(1);
	}
	for (i = 0; i < 8; i++)
	{
		r = mmc_spi_io(0xff);
		if (r == 0xfe) break;
	}
	if (r != 0xfe)
	{
		mmc_spi_io(0xff);
		return(2);
	}
	for (i = 0; i < 16; i++)
	{
		r = mmc_spi_io(0xff);
		csd[i] = r;
	}
	for (i = 0; i < 2; i++)
	{
		r = mmc_spi_io(0xff);
	}
	mmc_spi_io(0xff);
	if (r == 0x00) return(3);

	c_size = csd[8] + csd[7] * 256 + (csd[6] & 0x03) * 256 * 256;
	c_size >>= 6;
	c_size_mult = csd[10] + (csd[9] & 0x03) * 256;
	c_size_mult >>= 7;
	read_bl_len = csd[5] & 0x0f;
	mult = 1;
	mult <<= c_size_mult + 2;
	blocknr = (c_size + 1) * mult;
	block_len = 1;
	block_len <<= read_bl_len;
	size = block_len * blocknr;
	size >>= 10;

	mmc_blk_sizes[0] = size;
	mmc_blksize_sizes[0] = 1024;
	mmc_hardsect_sizes[0] = block_len;
	mmc_sectors = blocknr;

	return 0;
}

int mmc_hardware_init(void)
{
	//printk("mmc_hardware_init\n"); 
 
#if defined(CONFIG_COBRA5282)
   *(unsigned char  *)(MCF_IPSBAR + 0x100059) |= 0x7f; //QSPI
   *(unsigned char  *)(MCF_IPSBAR + MCF5282_GPTSCR1) = 0x00; 
   *(unsigned char  *)(MCF_IPSBAR + MCF5282_GPTDDR) = 0x00;
#elif defined(CONFIG_COBRA5272)
   // QSPI_CS[3..1] Output
   *(unsigned int  *)(MCF_MBAR + MCFSIM_PACNT) |= 0x00804000;
   *(unsigned int  *)(MCF_MBAR + MCFSIM_PDCNT) |= 0x00000030;
   // PA10 Output
   *(unsigned long  *)(MCF_MBAR + MCFSIM_PACNT) &= 0xffcfffff;
   *(unsigned short *)(MCF_MBAR + MCFSIM_PADDR) &= 0xfbff;
#endif

   
	QMR = 0xa302; /* 8 bits, baud, 16MHz clk */
	QWR   = 0x1000;
	QIR = 0x0001;
	QDLYR = QDLYR_SPE;
	QAR = COMMAND_RAM_START;
	QDR = 0x0000;

	return 0;
}

int mmc_init(void)
{
	int rc;

   //printk("mmc_init\n");
	rc = mmc_hardware_init(); 

	if ( rc != 0)
	{
		printk("mmc: error in mmc_hardware_init (%d)\n", rc);
		return -1;
	}

	rc = mmc_card_init(); 
	if ( rc != 0)
	{
		printk("mmc: error in mmc_card_init (%d)\n", rc);
		return -1;
	}

	rc = mmc_card_config(); 
	if ( rc != 0)
	{
		printk("mmc: error in mmc_card_config (%d)\n", rc);
		return -1;
	}

	blk_size[MAJOR_NR] = mmc_blk_sizes;
	blksize_size[MAJOR_NR] = mmc_blksize_sizes;
	hardsect_size[MAJOR_NR] = mmc_hardsect_sizes;

	return 0;
}

void mmc_exit(void)
{
   //printk("mmc_exit\n"); 

	blk_size[MAJOR_NR] = NULL;
	blksize_size[MAJOR_NR] = NULL;
	hardsect_size[MAJOR_NR] = NULL;
	mmc_sectors = 0;
}

int mmc_check_media_change(kdev_t dev)
{
	//printk("mmc_check_media_change\n"); 
	if (mmc_media_changed == 1)
	{
		mmc_media_changed = 0;
		return 1;
	}
	else return 0;
}

int mmc_revalidate(kdev_t dev)
{
	//printk("mmc_revalidate\n"); 
	if (mmc_media_detect == 0) return -ENODEV;
	return 0;
}

static struct block_device_operations mmc_bdops = 
{
	open: mmc_open,
	release: mmc_release,
	ioctl: mmc_ioctl,
	check_media_change: mmc_check_media_change,
	revalidate: mmc_revalidate,
};

static void mmc_check_media(void)
{
	int old_state;
	int rc;
   volatile unsigned short carddedect;
  
   //printk("mmc_check_media\n");
   
	old_state = mmc_media_detect; 

#if defined(CONFIG_COBRA5282) 
   carddedect = *(volatile unsigned char *)(MCF_IPSBAR + MCF5282_GPTPORT) & 0x04;
#elif defined(CONFIG_COBRA5272)
   carddedect = *(unsigned short  *)(MCF_MBAR + MCFSIM_PADAT) & 0x0400;
#endif
 
   mmc_media_detect = (carddedect == 0) ? 1 : 0;
  
   //printk("carddedect: %x\n", carddedect);
   
	if (old_state != mmc_media_detect) 
	{
		mmc_media_changed = 1;
		if (mmc_media_detect == 1)
		{
			rc = mmc_init();
			if (rc != 0) printk("mmc: error in mmc_init (%d)\n", rc);
		}
		else 
		{
			mmc_exit();
		}
	}
	del_timer(&mmc_timer);
	mmc_timer.expires = jiffies + HZ;
	add_timer(&mmc_timer);
}
int __init mmc_driver_init(void)
{
	int rc;

	printk("mmc_driver_init\n");

	rc = register_blkdev(MAJOR_NR, "mmc", &mmc_bdops);
	if (rc < 0)
	{
		printk(KERN_WARNING "mmc: can't get major %d\n", MAJOR_NR);
		return rc;
	}
	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), &mmc_request);

	init_timer(&mmc_timer);
	mmc_timer.expires = jiffies + HZ;
	mmc_timer.function = (void *)mmc_check_media;
	add_timer(&mmc_timer);

	return 0;
}

void __exit mmc_driver_exit(void)
{
	//printk("mmc_driver_exit\n"); 
	del_timer(&mmc_timer);
	fsync_dev(MKDEV(MAJOR_NR, 0));
	unregister_blkdev(MAJOR_NR, "mmc");
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
	mmc_exit();
}

module_init(mmc_driver_init);
module_exit(mmc_driver_exit);
