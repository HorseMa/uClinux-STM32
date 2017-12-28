/**
*******************************************************************************
*
* \brief	m41t94.c - Device driver for ST M41T94 RTC/WDT
*
* (C) Copyright 2004, Josef Baumgartner (josef.baumgartner@telex.de)
*
* This module implements a watchdog timer (WDT) and real time clock (RTC)
* driver for the ST M41T94 device. The driver uses the SPI driver to access
* the hardware.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 675 Mass Ave, Cambridge, MA 02139, USA.
*
******************************************************************************/

/*****************************************************************************/
/* includes */
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fcntl.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/rtc.h>
#include <linux/watchdog.h>
#include <linux/miscdevice.h>
#include <linux/version.h>
#include <asm/semaphore.h>
#include <asm/ioctl.h>
#include <linux/spi.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>


#include "spi_core.h"

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)) || \
     (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,99)))
#error Driver only supports kernel 2.4.x
#endif

#define M41T94_VERSION		"0.2"

#define RTC_BAUDRATE                    64              // 2kBaud????
#define RTC_WRITE                       0x80
#define RTC_READ                        0x00

/* macros for offest of time values in RTC */
#define SEC10_OFF                       0
#define SEC_OFF                         1
#define MIN_OFF                         2
#define HOUR_OFF                        3
#define DOW_OFF                         4
#define DAY_OFF                         5
#define MON_OFF                         6
#define YEAR_OFF                        7

/* Conversion between binary and BCD */
#define BCD_TO_BIN(val) 	(((val)&15) + ((val)>>4)*10)
#define BIN_TO_BCD(val) 	((((val)/10)<<4) + (val)%10)

#define	M41T94_WDT_REG			0x9

#define WDT_DEFAULT_TIME	5	/* 5 seconds */
#define WDT_MAX_TIME		31	/* 31 seconds */

/* CS pin to use for driver */
#ifndef CONFIG_SPI_M41T94_CS
#define	CONFIG_SPI_M41T94_CS	1
#endif

char	*dev;					/* pointer to spi device structure
						   used by spi driver */
uint	num = CONFIG_SPI_M41T94_CS;		/* config CS/minor device number to use */

uint	wdt_timeout = WDT_DEFAULT_TIME;		/* watchdog timeout value */
uint	wdt_enable = 0;				/* watchdog enable flag */
uint	wdt_trig_time = 0;
uint	wdt_last_trig = 0;

unsigned char days_in_mo[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static DECLARE_MUTEX(sem);


/**
*******************************************************************************
* \brief        read m41t94 register
*
******************************************************************************/
int m41t94_read_reg(u8 adrs, size_t size, char *p_buf)
{
        qspi_read_data      rd_data;
        char                tbuf[33];
        int                 ssize;

        if ((adrs > 127) || (size > 31))
                return -1;

        rd_data.buf[0] = adrs | RTC_READ;
        rd_data.length = 1;
        rd_data.loop = 0;

        down(&sem);

        if (spi_dev_ioctl (dev, QSPIIOCS_READKDATA, (unsigned long)&rd_data) < 0)
            return 0;

        ssize = spi_dev_read (dev, tbuf, size + 1, MEM_TYPE_KERNEL);    /* size bytes + address */

        up(&sem);

        if (ssize > 0) {
                /* the first byte is not needed, copy the rest into the buffer */
                memcpy (p_buf, tbuf + 1, (ssize > size) ? size : ssize - 1);
        }

    return ssize;
}

/**
*******************************************************************************
* \brief        write m41t94 register
*
******************************************************************************/
int m41t94_write_reg(u8 adrs, size_t size, char *p_buf)
{
        char                tbuf[33];

        if ((adrs > 127) || (size > 31))
            return -1;

        tbuf[0] = adrs | RTC_WRITE;

        memcpy (tbuf + 1, p_buf, size);

        return (spi_dev_write (dev, tbuf, size + 1, MEM_TYPE_KERNEL));
}

/**
*******************************************************************************
* \brief        set m41t94 time
*
******************************************************************************/
int m41t94_set_time(struct rtc_time *rtc_tm)
{
	u8		wday, mon, day, hrs, min, sec, leap_yr;
	uint		yrs;
        char		buf[8];

	yrs = rtc_tm->tm_year + 1900;
	mon = rtc_tm->tm_mon + 1;   /* tm_mon starts at zero */
	day = rtc_tm->tm_mday;
        wday = rtc_tm->tm_wday;
	hrs = rtc_tm->tm_hour;
	min = rtc_tm->tm_min;
	sec = rtc_tm->tm_sec;

	if ((yrs < 1970) || (yrs > 2069))
		return -EINVAL;

        leap_yr = ((!(yrs % 4) && (yrs % 100)) || !(yrs % 400));
        yrs -= 1900;
        if (yrs >= 100)
		yrs -= 100;

	if ((mon > 12) || (day == 0))
		return -EINVAL;

	if (day > (days_in_mo[mon] + ((mon == 2) && leap_yr)))
		return -EINVAL;

	if ((hrs >= 24) || (min >= 60) || (sec >= 60))
		return -EINVAL;
                
        //tm_year is # of years since 1900, if it is greater than 99 we have to subtract 100
        buf[YEAR_OFF] = BIN_TO_BCD(yrs);
        buf[MON_OFF] = BIN_TO_BCD(mon);
        buf[DAY_OFF] = BIN_TO_BCD(day);
        // day of week is stored from 1 - 7 therefor we have to add 1
        buf[DOW_OFF] = BIN_TO_BCD(wday + 1);
        buf[HOUR_OFF] = BIN_TO_BCD(hrs);
        buf[MIN_OFF] = BIN_TO_BCD(min);
        buf[SEC_OFF] = BIN_TO_BCD(sec);
        buf[SEC_OFF] &= 0x7f;   // clear stop bit to start oscillator
        buf[SEC10_OFF] = 0;
                                       
        if (m41t94_write_reg (0, 8, buf) < 0)
                return -1;

        return 0;
}

/**
*******************************************************************************
* \brief        get m41t94 time
*
******************************************************************************/
int m41t94_get_time(struct rtc_time *rtc_tm)
{
        char buf[8];
        char temp;

        /* clear halt bit to get current time */
        m41t94_read_reg (0x0c, 1, buf);
        buf[0] &= 0xbf;
        m41t94_write_reg (0x0c, 1, buf);

        /* read all date/time info in a burst read */
        if (m41t94_read_reg (0, 8, buf) < 0) {
                rtc_tm->tm_year = 99;
                rtc_tm->tm_mon = 0;
                rtc_tm->tm_mday = 1;
                rtc_tm->tm_wday = 4;
                rtc_tm->tm_hour = 12;
                rtc_tm->tm_min = 0;
                rtc_tm->tm_sec = 0;
                return -1;
        }

        //tm_year is # of years since 1900, so assume 00-99 is 2000-2098
        temp = BCD_TO_BIN(buf[YEAR_OFF]);
        rtc_tm->tm_year = (temp < 70) ? temp + 100 : temp;
        // month is stored from 0 - 11 therefor we have to subtract 1
        rtc_tm->tm_mon = ((buf[MON_OFF] >> 4) * 10) + (buf[MON_OFF] & 0xf) - 1;
        rtc_tm->tm_mday = ((buf[DAY_OFF] >> 4) * 10) + (buf[DAY_OFF] & 0xf);
        // day of week is stored from 0 - 6 therefor we have to subtract 1
        rtc_tm->tm_wday = ((buf[DOW_OFF] >> 4) * 10) + (buf[DOW_OFF] & 0xf) - 1;
        rtc_tm->tm_hour =(((buf[HOUR_OFF] & 0x30) >> 4) * 10) + (buf[HOUR_OFF] & 0xf);
        rtc_tm->tm_min = ((buf[MIN_OFF] >> 4) * 10) + (buf[MIN_OFF] & 0xf);

        /* mask ST bit before copiing!*/
        rtc_tm->tm_sec = (((buf[SEC_OFF] & 0x7f) >> 4) * 10) + (buf[SEC_OFF] & 0xf);

        return 0;
}

/**
*******************************************************************************
* \brief        enable m41t94 watchdog
*
******************************************************************************/
void m41t94_enable_wdt(int timeout)
{
	u8 wdtreg;

        wdt_enable = 1;
        wdt_timeout = timeout;

        wdtreg = ((wdt_timeout & 0x1f) << 2) | 0x80 | 0x2;
        m41t94_write_reg (M41T94_WDT_REG, 1, &wdtreg);
}

/**
*******************************************************************************
* \brief        force m41t94 watchdog reset
*
******************************************************************************/
void m41t94_force_wdt_reset(int timeout)
{
	u8 wdtreg;
        wdt_enable = 0;
        wdtreg = 0x80 | 0x2;	/* set timeout to 0 */
        m41t94_write_reg (M41T94_WDT_REG, 1, &wdtreg);
}


/**
*******************************************************************************
* \brief        retrigger m41t94 watchdog
*
******************************************************************************/
void m41t94_trigger_wdt(void)
{
	u8 wdtreg;
        uint cur_trig;

        if (wdt_enable){
        	cur_trig = jiffies;
        	wdt_trig_time = cur_trig - wdt_last_trig;
                wdt_last_trig = cur_trig;
        
        	wdtreg = ((wdt_timeout & 0x1f) << 2) | 0x80 | 0x2;
        	m41t94_write_reg (M41T94_WDT_REG, 1, &wdtreg);
        }
}

/******************************************************************************/
/*----------------------- M41T94 RTC driver functions ------------------------*/

/**
*******************************************************************************
* \brief        m41t94 RTC driver ioctl function
*
******************************************************************************/
static int m41t94_rtc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
                        unsigned long arg)
{
	struct rtc_time rtc_tm;

	switch (cmd) {
        case RTC_RD_TIME:	/* Read the time/date from RTC	*/
		memset(&rtc_tm, 0, sizeof(struct rtc_time));
		if (m41t94_get_time(&rtc_tm) < 0)
                    return -EFAULT;
		break;
	case RTC_SET_TIME:	/* Set the RTC */
		if (!capable(CAP_SYS_TIME))
			return -EACCES;

		if (copy_from_user(&rtc_tm, (struct rtc_time*)arg,
				   sizeof(struct rtc_time)))
			return -EFAULT;

		return m41t94_set_time(&rtc_tm);
	default:
		return -EINVAL;
	}
	return copy_to_user((void *)arg, &rtc_tm, sizeof(rtc_tm)) ? -EFAULT : 0;
}

/**
*******************************************************************************
* \brief        m41t94 RTC driver open function
*
******************************************************************************/
static int m41t94_rtc_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;
        return(0);
}

/**
*******************************************************************************
* \brief        m41t94 RTC driver release function
*
******************************************************************************/
static int m41t94_rtc_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
        return(0);
}

static struct file_operations m41t94_rtc_fops = {
	owner:		THIS_MODULE,
	ioctl:		m41t94_rtc_ioctl,
	open:		m41t94_rtc_open,
	release:	m41t94_rtc_release,
};

static struct miscdevice m41t94_rtc_dev=
{
	RTC_MINOR,
	"rtc",
	&m41t94_rtc_fops,
};


/******************************************************************************/
/*--------------------- M41T94 watchdog driver functions ---------------------*/

/**
*******************************************************************************
* \brief        m41t94 WDT driver procmem read function
*
******************************************************************************/
int m41t94_wdt_read_procmem (char *buf, char **start, off_t offset,
                      int count, int *eof, void *data)
{
        unsigned int len = 0;
        
        len += sprintf (buf + len, "----- M41T94 WDT -----\n");    
        len += sprintf (buf + len, "Trigger Time: %d ms\n", wdt_trig_time * 1000 / HZ);                                       
        *eof = 1;
        return len;
}

/**
*******************************************************************************
* \brief        m41t94 WDT driver ioctl function
*
******************************************************************************/
static int m41t94_wdt_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
                        unsigned long arg)
{
        uint	new_value;

        switch (cmd) {
        case WDIOC_KEEPALIVE:
                m41t94_trigger_wdt();	/* trigger the watchdog */
                break;;

        case WDIOC_SETTIMEOUT:
                if (get_user(new_value, (int *)arg))
                        return -EFAULT;
                if ((new_value <= 0) || (new_value > WDT_MAX_TIME))
                        return -EINVAL;
                /* Restart watchdog with new time */
                m41t94_enable_wdt(new_value);
		break;

        case WDIOC_GETTIMEOUT:
                return put_user(wdt_timeout, (int *)arg);

        default:
                return -EINVAL;
        }
        return 0;
}

/**
*******************************************************************************
* \brief        m41t94 WDT driver open function
*
******************************************************************************/
static int m41t94_wdt_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;
        m41t94_enable_wdt(WDT_DEFAULT_TIME);
        return(0);
}

/**
*******************************************************************************
* \brief        m41t94 driver release function
*
******************************************************************************/
static int m41t94_wdt_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
        /* watchdog timer is never stopped! */
        return(0);
}

static struct file_operations m41t94_wdt_fops = {
	owner:		THIS_MODULE,
	ioctl:		m41t94_wdt_ioctl,
	open:		m41t94_wdt_open,
	release:	m41t94_wdt_release,
};

static struct miscdevice m41t94_wdt_dev=
{
	WATCHDOG_MINOR,
	"wdt",
	&m41t94_wdt_fops,
};

/**
*******************************************************************************
* \brief        m41t94 driver init function
*
******************************************************************************/
int m41t94_init(void)
{
	int	result;

	if ((dev = spi_dev_open(num)) == NULL)
            return (-ENOMEM);

        spi_dev_ioctl (dev, QSPIIOCS_BAUD, RTC_BAUDRATE);
        spi_dev_ioctl (dev, QSPIIOCS_BITS, 8);
        spi_dev_ioctl (dev, QSPIIOCS_CONT, 1);
        spi_dev_ioctl (dev, QSPIIOCS_CPOL, 0);
        spi_dev_ioctl (dev, QSPIIOCS_CPHA, 0);
        spi_dev_ioctl (dev, QSPIIOCS_QCD,  17);
        spi_dev_ioctl (dev, QSPIIOCS_DTL, 1);

        /* register RTC device */
	result = misc_register(&m41t94_rtc_dev);
	if (result < 0){
        	spi_dev_release(dev);
        	return result;
        }

        /* register WDT device */
	result = misc_register(&m41t94_wdt_dev);
	if (result < 0){
        	misc_deregister(&m41t94_rtc_dev);
        	spi_dev_release(dev);
        	return result;
        }

	create_proc_read_entry ("driver/m41t94_wdt", 0, NULL, m41t94_wdt_read_procmem, NULL);
      
        
        printk(KERN_INFO "M41T94 RTC/WDT Driver %s using SPI device %d\n", M41T94_VERSION, num);

        return 0;
}

/**
*******************************************************************************
* \brief        m41t94 driver exit function
*
******************************************************************************/
void __exit m41t94_exit(void)      /* the __exit added by ron  */
{
	misc_deregister(&m41t94_rtc_dev);
        misc_deregister(&m41t94_wdt_dev);
	spi_dev_release(dev);
	
        remove_proc_entry ("dirver/m41t94_wdt", NULL);                        
}

module_init(m41t94_init);
module_exit(m41t94_exit);

EXPORT_SYMBOL(m41t94_force_wdt_reset);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Josef Baumgartner <josef.baumgartner@telex.de>");
MODULE_DESCRIPTION("M41T94 RTC/WDT driver module");


