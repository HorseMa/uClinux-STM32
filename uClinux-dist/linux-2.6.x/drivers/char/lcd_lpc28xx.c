/* Copyright (C) 2007 NXP. 
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>		
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <asm/system.h>	
#include <asm/uaccess.h>
#include <asm/arch/lpc28xx.h>

#include "lcd_lpc28xx.h"

int lcd_major = LCD_MAJOR;
static dev_t dev;
struct cdev lcd_cdev;
__inline void write_display(unsigned char command, unsigned char data)
{
	unsigned int status;

	status = LCDSTAT;
	status = (status >> 5) & 0x1F;
	while (status >= 0x0E)
	{
		status = LCDSTAT;
		status = (status >> 5) & 0x1F;
	}

	if ( command == CMD )
	{
		LCDIBYTE = data;
	}
	else
	{
		LCDDBYTE = data;
	}
}

ssize_t lpc28xx_lcd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	return 0;
}

__inline void set_line(int line)
{
	write_display(CMD, START_LINE_SET);
	write_display(CMD, PAGE_ADDRESS_SET + line);
}

__inline void set_col(int col)
{
	write_display(CMD, COLUMN_ADDRESS_HIGH+((col&0xf0)>>4));
	write_display(CMD, COLUMN_ADDRESS_LOW+(col&0xf));
}

ssize_t lpc28xx_lcd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	unsigned char tmpbuf[1024];
	size_t bytewrite;
	size_t oldsize;
	size_t loop;
	size_t bytewrited;

	if((count+filp->f_pos)>LCD_SIZE)
	{
		count = LCD_SIZE - filp->f_pos;
	}
	oldsize = count;
	bytewrite = 1024;
	bytewrited = 0;
	while(count>0)
	{
		if(count<1024)
		{
			bytewrite = count;
		}
		if (copy_from_user(tmpbuf, buf+bytewrited, bytewrite))
		{		
			return -EFAULT;	
		}
		for(loop=0; loop<bytewrite; loop++)
		{
			write_display(DATA, (char)tmpbuf[loop]);
			++(filp->f_pos);
			if(filp->f_pos%128 == 0)
			{
				set_line(filp->f_pos/128);
				set_col(0);
			}
		}
		count -= bytewrite;	
		bytewrited += bytewrite;
	}
	return oldsize;
}

__inline void set_pos(struct file *filp)
{
	set_line(filp->f_pos/(LCD_WIDTH*8));
	set_col((filp->f_pos%(LCD_WIDTH*8))/8);
}

loff_t lpc28xx_lcd_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;

	switch(whence) {
	  case 0: /* SEEK_SET */
		if(off>=0 && off<LCD_SIZE)
		{
			filp->f_pos = off;
			set_pos(filp);
			return filp->f_pos;
		}
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		if(newpos>=0 && newpos<LCD_SIZE)
		{
			filp->f_pos = newpos;
			set_pos(filp);
			return filp->f_pos;
		}
		break;

	  case 2: /* SEEK_END */
	        newpos = LCD_SIZE + off;
                if(newpos>=0 && newpos<LCD_SIZE)
                {
                        filp->f_pos = newpos;
			set_pos(filp);
                        return filp->f_pos;
                }
		break;

	  default: /* can't happen */
		return -EINVAL;
	}
	return -EINVAL;
}

int lpc28xx_lcd_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
	return 0;
}

int lpc28xx_lcd_open(struct inode *inode, struct file *filp)
{
	filp->f_pos = 0;
	set_pos(filp->f_pos);
	return 0;          /* success */
}

int lpc28xx_lcd_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations lcd_fops = {
	.owner =    THIS_MODULE,
	.llseek =   lpc28xx_lcd_llseek,
	.read =     lpc28xx_lcd_read,
	.write =    lpc28xx_lcd_write,
	.ioctl =    lpc28xx_lcd_ioctl,
	.open =     lpc28xx_lcd_open,
	.release =  lpc28xx_lcd_release,
};

void lpc28xx_lcd_hwinit()
{
	LCDCTRL = 0x4000;

	write_display(CMD, RESET_DISPLAY);
	write_display(CMD, LCD_BIAS_1_9);
	write_display(CMD, ADC_SELECT_REVERSE);
	write_display(CMD, COMMON_OUTPUT_NORMAL);
	write_display(CMD, V5_RESISTOR_RATIO);
	write_display(CMD, ELECTRONIC_VOLUME_SET);
	write_display(CMD, ELECTRONIC_VOLUME_INIT);
	write_display(CMD, (POWER_CONTROL_SET | VOLTAGE_REGULATOR | VOLTAGE_FOLLOWER | BOOSTER_CIRCUIT));
//	write_display(CMD, DISPLAY_NORMAL);
	write_display(CMD, DISPLAY_REVERSE);
	write_display(CMD, DISPLAY_ON);
}

int lpc28xx_lcd_init_module(void)
{
	int result;

	printk(KERN_INFO "lpc28xx lcd: ");

	dev = MKDEV(lcd_major, 0);
	result = register_chrdev_region(dev, 1, "lcd");
	if (result < 0) 
	{
		printk(KERN_WARNING "lcd: can't get major %d\n", lcd_major);
		return result;
	}

	cdev_init(&lcd_cdev, &lcd_fops);
	lcd_cdev.owner = THIS_MODULE;
	lcd_cdev.ops = &lcd_fops;
	result = cdev_add (&lcd_cdev, dev, 1);
	/* Fail gracefully if need be */
	if (result)
		printk(KERN_NOTICE "Error %d ", result);
	
	lpc28xx_lcd_hwinit();
	printk("driver loaded\n");

	return 0;
}

void lpc28xx_lcd_cleanup_module(void)
{

	/* cleanup_module is never called if registering failed */
	cdev_del(&lcd_cdev);
	unregister_chrdev_region(dev, 2);

}

MODULE_AUTHOR("yang");
MODULE_LICENSE("GPL");
module_init(lpc28xx_lcd_init_module);
module_exit(lpc28xx_lcd_cleanup_module);

