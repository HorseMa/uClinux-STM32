/*
 *  linux/drivers/char/x1226-rtc.c
 *
 *  I2C Real Time Clock Client Driver for Xicor X1226 RTC/Calendar
 *
 * Author: Steve Longerbeam <stevel@mvista.com, or source@mvista.com>
 *
 * 2002-2003 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/rtc.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/time.h>

#undef DEBUG_X1226

#ifdef DEBUG_X1226
#define	dbg(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __func__, ## args)
#else
#define	dbg(fmt, args...)
#endif

#define X1226_MODULE_NAME "X1226"
#define PFX X1226_MODULE_NAME

#define err(format, arg...) printk(KERN_ERR PFX ": " format , ## arg)
#define info(format, arg...) printk(KERN_INFO PFX ": " format , ## arg)
#define warn(format, arg...) printk(KERN_WARNING PFX ": " format , ## arg)
#define emerg(format, arg...) printk(KERN_EMERG PFX ": " format , ## arg)

#define EPOCH 2000
#define SYS_EPOCH 1900

#undef BCD_TO_BIN
#define BCD_TO_BIN(val) (((val)&15) + ((val)>>4)*10)

#undef BIN_TO_BCD
#define BIN_TO_BCD(val) ((((val)/10)<<4) + (val)%10)

#define X1226_RTC_SR      0x3f
#define   RTC_SR_WEL  (1<<1)
#define   RTC_SR_RWEL (1<<2)

#define X1226_RTC_BASE    0x30

/* This is an image of the RTC registers starting at offset 0x30 */
struct rtc_registers {
	unsigned char secs;	// 30
	unsigned char mins;	// 31
	unsigned char hours;	// 32
	unsigned char day;	// 33
	unsigned char mon;	// 34
	unsigned char year;	// 35
	unsigned char dayofweek;	// 36
	unsigned char epoch;	// 37
};

#define X1226_CONTROL_DTR  0x13
#define X1226_CONTROL_ATR  0x12
#define X1226_CONTROL_INT  0x11
#define X1226_CONTROL_BL   0x10

#define DEVID_RTC          0x6F
#define DEVID_NVRAM        0x57

#define ABITS           9
#define EESIZE          (1 << ABITS)	/* size in bytes */

#define NVSIZE          512	/* we use 512 bytes */
#define NVOFFSET        (EESIZE-NVSIZE)	/* at end of EEROM */

static struct i2c_driver x1226_driver;

static int x1226_use_count = 0;

static struct i2c_client *this_client = NULL;

static int rtc_read_proc(char *page, char **start, off_t off,
			 int count, int *eof, void *data);

static int
x1226_read(struct i2c_client *client, u8 reg_offset, u8 * buf, int len)
{
	int ret;
	u8 regbuf[2] = { 0, reg_offset };
	struct i2c_msg random_addr_read[2] = {
		{
		 /* "Set Current Address" */
		 client->addr,
		 client->flags,
		 sizeof (regbuf),
		 regbuf}
		,
		{
		 /* "Sequential Read" if len>1,
		    "Current Address Read" if len=1 */
		 client->addr,
		 client->flags | I2C_M_RD,
		 len,
		 buf}
	};

	if ((ret = i2c_transfer(client->adapter, random_addr_read, 2)) != 2) {
		err("i2c_transfer failed, ret=%d\n", ret);
		ret = -ENXIO;
	}

	return ret;
}

static int
x1226_write(struct i2c_client *client, u8 reg_offset, u8 * buf, int len)
{
	int ret;
	u8 *local_buf;
	u8 regbuf[2] = { 0, reg_offset };
	struct i2c_msg page_write = {
		client->addr,
		client->flags,
		len + sizeof (regbuf),
		NULL
	};

	if ((local_buf = (u8 *) kmalloc(len + sizeof (regbuf),
					GFP_KERNEL)) == NULL) {
		err("buffer alloc failed\n");
		return -ENOMEM;
	}

	memcpy(local_buf, regbuf, sizeof (regbuf));
	memcpy(local_buf + sizeof (regbuf), buf, len);
	page_write.buf = local_buf;

	if ((ret = i2c_transfer(client->adapter, &page_write, 1)) != 1) {
		err("i2c_transfer failed, ret=%d\n", ret);
		ret = -ENXIO;
	}

	kfree(local_buf);
	return ret;
}

static int
ccr_write_enable(struct i2c_client *client)
{
	u8 sr = RTC_SR_WEL;
	int ret;

	if ((ret = x1226_write(client, X1226_RTC_SR, &sr, 1)) < 0)
		return ret;
	sr |= RTC_SR_RWEL;
	if ((ret = x1226_write(client, X1226_RTC_SR, &sr, 1)) < 0)
		return ret;
	sr = 0;
	if ((ret = x1226_read(client, X1226_RTC_SR, &sr, 1)) < 0)
		return ret;

	sr &= (RTC_SR_RWEL | RTC_SR_RWEL);
	if (sr != (RTC_SR_RWEL | RTC_SR_RWEL)) {
		dbg("verify SR failed\n");
		return -ENXIO;
	}

	return 0;
}

static int
ccr_write_disable(struct i2c_client *client)
{
	int ret;
	u8 sr = 0;

	if ((ret = x1226_write(client, X1226_RTC_SR, &sr, 1)) < 0)
		return ret;
	if ((ret = x1226_read(client, X1226_RTC_SR, &sr, 1)) < 0)
		return ret;
	if (sr != 0) {
		dbg("verify SR failed\n");
		return -ENXIO;
	}

	return 0;
}

static int
x1226_get_time(struct i2c_client *client, struct rtc_time *tm)
{
	struct rtc_registers rtc;
	u32 epoch;
	int ret;

	/* read RTC registers */
	if ((ret = x1226_read(client, X1226_RTC_BASE, (u8 *) & rtc,
			      sizeof (struct rtc_registers))) < 0) {
		dbg("couldn't read RTC\n");
		return ret;
	}

	dbg("IN: epoch=%02x, year=%02x, mon=%02x, day=%02x, hour=%02x, "
	    "min=%02x, sec=%02x\n",
	    rtc.epoch, rtc.year, rtc.mon, rtc.day, rtc.hours,
	    rtc.mins, rtc.secs);

	epoch = 100 * BCD_TO_BIN(rtc.epoch);	// 19 / 20
	tm->tm_year = BCD_TO_BIN(rtc.year);	// 0 - 99
	tm->tm_year += (epoch - SYS_EPOCH);
	tm->tm_mon = BCD_TO_BIN(rtc.mon);	// 1 - 12
	if (tm->tm_mon)
		tm->tm_mon--;	/* tm_mon is 0 to 11 */
	tm->tm_mday = BCD_TO_BIN(rtc.day);	// 1 - 31
	if (!tm->tm_mday)
		tm->tm_mday = 1;
	tm->tm_hour = BCD_TO_BIN(rtc.hours & ~0x80);
	if (rtc.hours && !(rtc.hours & 0x80)) {
		// AM/PM 1-12 format, convert to MIL
		tm->tm_hour--;	// 0 - 11
		if (rtc.hours & (1 << 5))
			tm->tm_hour += 12;	// PM
	}

	tm->tm_min = BCD_TO_BIN(rtc.mins);
	tm->tm_sec = BCD_TO_BIN(rtc.secs);

	dbg("OUT: year=%d, mon=%d, day=%d, hour=%d, min=%d, sec=%d\n",
	    tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour,
	    tm->tm_min, tm->tm_sec);

	return 0;
}

static int
x1226_set_time(struct i2c_client *client, const struct rtc_time *tm)
{
	struct rtc_registers rtc;
	int ret;

	dbg("IN: year=%d, mon=%d, day=%d, hour=%d, min=%d, sec=%d\n",
	    tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour,
	    tm->tm_min, tm->tm_sec);

	rtc.epoch = BIN_TO_BCD(EPOCH / 100);
	rtc.year = BIN_TO_BCD(tm->tm_year + SYS_EPOCH - EPOCH);
	rtc.mon = BIN_TO_BCD(tm->tm_mon + 1);	/* tm_mon is 0 to 11 */
	rtc.day = BIN_TO_BCD(tm->tm_mday);
	rtc.dayofweek = 0;	// ignore day of week
	rtc.hours = BIN_TO_BCD(tm->tm_hour) | 0x80;	/* 24 hour format */
	rtc.mins = BIN_TO_BCD(tm->tm_min);
	rtc.secs = BIN_TO_BCD(tm->tm_sec);

	dbg("OUT: epoch=%02x, year=%02x, mon=%02x, day=%02x, hour=%02x, "
	    "min=%02x, sec=%02x\n",
	    rtc.epoch, rtc.year, rtc.mon, rtc.day, rtc.hours,
	    rtc.mins, rtc.secs);

	/* write RTC registers */
	if ((ret = ccr_write_enable(client)) < 0)
		return ret;
	if ((ret = x1226_write(client, X1226_RTC_BASE, (u8 *) & rtc,
			       sizeof (struct rtc_registers))) < 0) {
		dbg("couldn't write RTC\n");
		return ret;
	}
	ccr_write_disable(client);

	return 0;
}

static int
x1226_probe(struct i2c_adapter *adap)
{
	int ret;
	struct rtc_time dummy_tm;

	if (this_client != NULL)
		return -EBUSY;

	this_client = kmalloc(sizeof (*this_client), GFP_KERNEL);
	if (this_client == NULL) {
		return -ENOMEM;
	}

	strcpy(this_client->name, X1226_MODULE_NAME);
	this_client->id = x1226_driver.id;
	this_client->flags = 0;
	this_client->addr = DEVID_RTC;
	this_client->adapter = adap;
	this_client->driver = &x1226_driver;
	this_client->data = NULL;

	/*
	 * use x1226_get_time() to probe for an X1226 on this bus.
	 */
	if ((ret = x1226_get_time(this_client, &dummy_tm)) < 0) {
		kfree(this_client);
		this_client = NULL;
		return ret;
	}

	info("found X1226 on %s\n", adap->name);

	/* attach it. */
	return i2c_attach_client(this_client);
}

static int
x1226_detach(struct i2c_client *client)
{
	i2c_detach_client(client);

	if (this_client != NULL) {
		kfree(this_client);
		this_client = NULL;
	}

	return 0;
}

int
rtc_open(struct inode *minode, struct file *mfile)
{
	/*if(MOD_IN_USE) */
	if (x1226_use_count > 0) {
		return -EBUSY;
	}
	MOD_INC_USE_COUNT;
	++x1226_use_count;
	return 0;
}

int
rtc_release(struct inode *minode, struct file *mfile)
{
	MOD_DEC_USE_COUNT;
	--x1226_use_count;
	return 0;
}

static loff_t
rtc_llseek(struct file *mfile, loff_t offset, int origint)
{
	return -ESPIPE;
}

static int
x1226_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return -EINVAL;
}

static int
rtc_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg)
{
	struct rtc_time rtc_tm;
	int ret;

	switch (cmd) {
	case RTC_RD_TIME:	/* Read the time/date from RTC  */
		memset(&rtc_tm, 0, sizeof(rtc_tm));
		if ((ret = x1226_get_time(this_client, &rtc_tm)) < 0)
			return ret;
		return copy_to_user((void *) arg, &rtc_tm, sizeof (rtc_tm)) ?
		    -EFAULT : 0;
	case RTC_SET_TIME:	/* Set the RTC */
		if (!capable(CAP_SYS_TIME))
			return -EACCES;

		if (copy_from_user(&rtc_tm,
				   (struct rtc_time *) arg,
				   sizeof (struct rtc_time)))
			return -EFAULT;

		return x1226_set_time(this_client, &rtc_tm);
	default:
		return -EINVAL;
	}
}

static struct i2c_driver x1226_driver = {
	.name = X1226_MODULE_NAME,
	.id = I2C_DRIVERID_EXP3,
	.flags = I2C_DF_NOTIFY,
	.attach_adapter = x1226_probe,
	.detach_client = x1226_detach,
	.command = x1226_command
};

static struct file_operations rtc_fops = {
	.owner = THIS_MODULE,
	.llseek = rtc_llseek,
	.ioctl = rtc_ioctl,
	.open = rtc_open,
	.release = rtc_release,
};

static struct miscdevice x1226rtc_miscdev = {
	RTC_MINOR,
	"rtc",
	&rtc_fops
};

static __init int
x1226_init(void)
{
	int ret;

	info("I2C based RTC driver.\n");
	ret = i2c_add_driver(&x1226_driver);
	if (ret) {
		err("Register I2C driver failed, errno is %d\n", ret);
		return ret;
	}
	ret = misc_register(&x1226rtc_miscdev);
	if (ret) {
		err("Register misc driver failed, errno is %d\n", ret);
		ret = i2c_del_driver(&x1226_driver);
		if (ret) {
			err("Unregister I2C driver failed, errno is %d\n", ret);
		}
		return ret;
	}

	create_proc_read_entry("driver/rtc", 0, 0, rtc_read_proc, NULL);

	return 0;
}

static void __exit
x1226_exit(void)
{
	remove_proc_entry("driver/rtc", NULL);
	misc_deregister(&x1226rtc_miscdev);
	i2c_del_driver(&x1226_driver);
}

module_init(x1226_init);
module_exit(x1226_exit);

/*
 *	Info exported via "/proc/driver/rtc".
 */

static int
rtc_proc_output(char *buf)
{
	char *p;
	struct rtc_time tm;
	int ret;

	if ((ret = x1226_get_time(this_client, &tm)) < 0)
		return ret;

	p = buf;

	/*
	 * There is no way to tell if the luser has the RTC set for local
	 * time or for Universal Standard Time (GMT). Probably local though.
	 */
	p += sprintf(p,
		     "rtc_time\t: %02d:%02d:%02d\n"
		     "rtc_date\t: %04d-%02d-%02d\n"
		     "rtc_epoch\t: %04d\n",
		     tm.tm_hour, tm.tm_min, tm.tm_sec,
		     tm.tm_year + SYS_EPOCH, tm.tm_mon + 1, tm.tm_mday, EPOCH);

	return p - buf;
}

static int
rtc_read_proc(char *page, char **start, off_t off,
	      int count, int *eof, void *data)
{
	int len = rtc_proc_output(page);
	if (len <= off + count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;
}

MODULE_AUTHOR("Steve Longerbeam");
MODULE_LICENSE("GPL");
