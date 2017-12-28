/*
 * An I2C driver for the Philips PCF8563 RTC
 * Copyright 2005-06 Tower Technologies
 *
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 * Maintainers: http://www.nslu2-linux.org/
 *
 * based on the other drivers in this same directory.
 *
 * http://www.semiconductors.philips.com/acrobat/datasheets/PCF8563-04.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>

#define DRV_VERSION "0.4.2"

/* Addresses to scan: none
 * This chip cannot be reliably autodetected. An empty eeprom
 * located at 0x51 will pass the validation routine due to
 * the way the registers are implemented.
 */
static const unsigned short normal_i2c[] = { I2C_CLIENT_END };

/* Module parameters */
I2C_CLIENT_INSMOD;

#define PCF8563_REG_ST1		0x00 /* status */
#define PCF8563_REG_ST2		0x01

#define PCF8563_REG_SC		0x02 /* datetime */
#define PCF8563_REG_MN		0x03
#define PCF8563_REG_HR		0x04
#define PCF8563_REG_DM		0x05
#define PCF8563_REG_DW		0x06
#define PCF8563_REG_MO		0x07
#define PCF8563_REG_YR		0x08

#define PCF8563_REG_AMN		0x09 /* alarm */
#define PCF8563_REG_AHR		0x0A
#define PCF8563_REG_ADM		0x0B
#define PCF8563_REG_ADW		0x0C

#define PCF8563_REG_CLKO	0x0D /* clock out */
#define PCF8563_REG_TMRC	0x0E /* timer control */
#define PCF8563_REG_TMR		0x0F /* timer */

#define PCF8563_SC_LV		0x80 /* low voltage */
#define PCF8563_MO_C		0x80 /* century */

struct pcf8563 {
	struct i2c_client client;
	/*
	 * The meaning of MO_C bit varies by the chip type.
	 * From PCF8563 datasheet: this bit is toggled when the years
	 * register overflows from 99 to 00
	 *   0 indicates the century is 20xx
	 *   1 indicates the century is 19xx
	 * From RTC8564 datasheet: this bit indicates change of
	 * century. When the year digit data overflows from 99 to 00,
	 * this bit is set. By presetting it to 0 while still in the
	 * 20th century, it will be set in year 2000, ...
	 * There seems no reliable way to know how the system use this
	 * bit.  So let's do it heuristically, assuming we are live in
	 * 1970...2069.
	 */
	int c_polarity;	/* 0: MO_C=1 means 19xx, otherwise MO_C=1 means 20xx */
};

/* Define various control and status bits in the registers. */
#define	PCF8563_ST1_TEST1		0x80	// 1 = test mode.
#define	PCF8563_ST1_STOP		0x20	// 1 = stop RTC.
#define	PCF8563_ST1_TESTC		0x8	// 1 = override power-on reset.

#define	PCF8563_ST2_TI			0x10	// 1 = output clock on /int.
#define	PCF8563_ST2_AF			0x8	// Alarm flag.
#define	PCF8563_ST2_TF			0x4	// Timer flag.
#define	PCF8563_ST2_AIE			0x2	// Alarm interrupt enable.
#define	PCF8563_ST2_TIE			0x1	// Timer interrupt enable.

#define	PCF8563_ALARM_AE		0x80	// 1 = enable alarm output.

static int pcf8563_probe(struct i2c_adapter *adapter, int address, int kind);
static int pcf8563_detach(struct i2c_client *client);

#define ALARM_BCD2BIN(B) (((B) & PCF8563_ALARM_AE) ? 0xFF : BCD2BIN(B))
#define ALARM_BIN(B) (((B) & PCF8563_ALARM_AE) ? 0xFF : (B))

/*
 * In the routines that deal directly with the pcf8563 hardware, we use
 * rtc_time -- month 0-11, hour 0-23, yr = calendar year-epoch.
 *
 * If 'alrm' is not NULL, the alarm state is retrieved into *tm, alrm->pending and alrm->enabled.
 */
static int pcf8563_get_datetime(struct i2c_client *client, struct rtc_time *tm, struct rtc_wkalrm *alrm)
{
	struct pcf8563 *pcf8563 = container_of(client, struct pcf8563, client);
	unsigned char buf[13] = { PCF8563_REG_ST1 };

	struct i2c_msg msgs[] = {
		{ client->addr, 0, 1, buf },	/* setup read ptr */
		{ client->addr, I2C_M_RD, 13, buf },	/* read status + date */
	};

	/* read registers */
	if ((i2c_transfer(client->adapter, msgs, 2)) != 2) {
		dev_err(&client->dev, "%s: read error\n", __FUNCTION__);
		return -EIO;
	}

	if (buf[PCF8563_REG_SC] & PCF8563_SC_LV)
		dev_info(&client->dev,
			"low voltage detected, date/time is not reliable.\n");

	dev_dbg(&client->dev,
		"%s: raw data is st1=%02x, st2=%02x, sec=%02x, min=%02x, hr=%02x, "
		"mday=%02x, wday=%02x, mon=%02x, year=%02x\n",
		__FUNCTION__,
		buf[0], buf[1], buf[2], buf[3],
		buf[4], buf[5], buf[6], buf[7],
		buf[8]);


	if (alrm) {
		dev_dbg(&client->dev,
			"%s: alarm raw data is min=%02x, hr=%02x, "
			"mday=%02x, wday=%02x\n",
			__FUNCTION__,
			buf[9], buf[10], buf[11], buf[12]);
	}

	/* Alarm has no month and year, so these are the same for both */
	tm->tm_mon = BCD2BIN(buf[PCF8563_REG_MO] & 0x1F) - 1; /* rtc mn 1-12 */
	tm->tm_year = BCD2BIN(buf[PCF8563_REG_YR]);
	if (tm->tm_year < 70)
		tm->tm_year += 100;	/* assume we are in 1970...2069 */
	/* detect the polarity heuristically. see note above. */
	pcf8563->c_polarity = (buf[PCF8563_REG_MO] & PCF8563_MO_C) ?
		(tm->tm_year >= 100) : (tm->tm_year < 100);

	if (alrm) {
		tm->tm_sec = 0;
		tm->tm_min = ALARM_BCD2BIN(buf[PCF8563_REG_AMN]);
		tm->tm_hour = ALARM_BCD2BIN(buf[PCF8563_REG_AHR] & 0xBF);
		tm->tm_mday = ALARM_BCD2BIN(buf[PCF8563_REG_ADM] & 0xBF);
		tm->tm_wday = ALARM_BIN(buf[PCF8563_REG_ADW] & 0x87);
		/* tm_mon and tm_year are set below */
		alrm->enabled = buf[PCF8563_REG_ST2] & PCF8563_ST2_AIE;
		alrm->pending = buf[PCF8563_REG_ST2] & PCF8563_ST2_AF;
	}
	else {
		tm->tm_sec = BCD2BIN(buf[PCF8563_REG_SC] & 0x7F);
		tm->tm_min = BCD2BIN(buf[PCF8563_REG_MN] & 0x7F);
		tm->tm_hour = BCD2BIN(buf[PCF8563_REG_HR] & 0x3F); /* rtc hr 0-23 */
		tm->tm_mday = BCD2BIN(buf[PCF8563_REG_DM] & 0x3F);
		tm->tm_wday = buf[PCF8563_REG_DW] & 0x07;

		/* the clock can give out invalid datetime, but we cannot return
		 * -EINVAL otherwise hwclock will refuse to set the time on bootup.
		 */
		if (rtc_valid_tm(tm) < 0) {
			dev_err(&client->dev, "retrieved date/time is not valid.\n");
		}
	}

	dev_dbg(&client->dev, "%s: %s tm is secs=%d, mins=%d, hours=%d, "
		"mday=%d, mon=%d, year=%d, wday=%d\n",
		__FUNCTION__,
		alrm ? "alarm" : "time",
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday);

	return 0;
}

static int pcf8563_set_datetime(struct i2c_client *client, struct rtc_time *tm, struct rtc_wkalrm *alrm)
{
	struct pcf8563 *pcf8563 = container_of(client, struct pcf8563, client);
	int i, err;
	unsigned char buf[13];

	dev_dbg(&client->dev, "%s: %s secs=%d, mins=%d, hours=%d, ",
		__FUNCTION__,
		alrm ? "alarm" : "time",
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday);

	/* hours, minutes and seconds */
	buf[PCF8563_REG_SC] = BIN2BCD(tm->tm_sec);
	buf[PCF8563_REG_MN] = BIN2BCD(tm->tm_min);
	buf[PCF8563_REG_HR] = BIN2BCD(tm->tm_hour);

	buf[PCF8563_REG_DM] = BIN2BCD(tm->tm_mday);

	/* month, 1 - 12 */
	buf[PCF8563_REG_MO] = BIN2BCD(tm->tm_mon + 1);

	/* year and century */
	buf[PCF8563_REG_YR] = BIN2BCD(tm->tm_year % 100);
	if (pcf8563->c_polarity ? (tm->tm_year >= 100) : (tm->tm_year < 100))
		buf[PCF8563_REG_MO] |= PCF8563_MO_C;

	buf[PCF8563_REG_DW] = tm->tm_wday & 0x07;

	/* write register's data */
	for (i = 0; i < 7; i++) {
		unsigned char data[2] = { PCF8563_REG_SC + i,
						buf[PCF8563_REG_SC + i] };

		err = i2c_master_send(client, data, sizeof(data));
		if (err != sizeof(data)) {
			dev_err(&client->dev,
				"%s: err=%d addr=%02x, data=%02x\n",
				__FUNCTION__, err, data[0], data[1]);
			return -EIO;
		}
	};

	return 0;
}

static int pcf8563_set_alarm(struct i2c_client *client, struct rtc_wkalrm *alrm)
{
	int err;
	unsigned char buf[5];

	dev_dbg(&client->dev, "%s: alarm mins=%d, hours=%d, "
		"mday=%d, wday=%d, enabled=%d\n",
		__FUNCTION__,
		alrm->time.tm_min, alrm->time.tm_hour,
		alrm->time.tm_mday, alrm->time.tm_wday, alrm->enabled);

	/* First set the alarm time/date */
	/* Wild card (0xff) value in field means that alarm is not set for that field. */
	buf[0] = PCF8563_REG_AMN;	/* Address to write data */
	buf[1] = (alrm->time.tm_min == 0xff) ? 0x80 : BIN_TO_BCD(alrm->time.tm_min);
	buf[2] = (alrm->time.tm_hour == 0xff) ? 0x80 : BIN_TO_BCD(alrm->time.tm_hour);
	buf[3] = (alrm->time.tm_mday == 0xff) ? 0x81 : BIN_TO_BCD(alrm->time.tm_mday);
	buf[4] = (alrm->time.tm_wday == 0xff) ? 0x80 : alrm->time.tm_wday;

	/* write alarm registers */
	err = i2c_master_send(client, buf, sizeof(buf));
	if (err != sizeof(buf)) {
		dev_err(&client->dev,
			"%s: err=%d addr=%02x\n",
			__FUNCTION__, err, buf[0]);
		return -EIO;
	}

	/* Now write the status/control register 2 */
	buf[0] = PCF8563_REG_ST2;	/* Address to write data */
	buf[1] = alrm->enabled ? PCF8563_ST2_AIE : 0;
	err = i2c_master_send(client, buf, 2);
	if (err != 2) {
		dev_err(&client->dev,
			"%s: err=%d addr=%02x\n",
			__FUNCTION__, err, buf[0]);
		return -EIO;
	}

	return 0;
}

struct pcf8563_limit
{
	unsigned char reg;
	unsigned char mask;
	unsigned char min;
	unsigned char max;
};

static int pcf8563_validate_client(struct i2c_client *client)
{
	int i;

	static const struct pcf8563_limit pattern[] = {
		/* register, mask, min, max */
		{ PCF8563_REG_SC,	0x7F,	0,	59	},
		{ PCF8563_REG_MN,	0x7F,	0,	59	},
		{ PCF8563_REG_HR,	0x3F,	0,	23	},
		{ PCF8563_REG_DM,	0x3F,	0,	31	},
		{ PCF8563_REG_MO,	0x1F,	0,	12	},
	};

	/* check limits (only registers with bcd values) */
	for (i = 0; i < ARRAY_SIZE(pattern); i++) {
		int xfer;
		unsigned char value;
		unsigned char buf = pattern[i].reg;

		struct i2c_msg msgs[] = {
			{ client->addr, 0, 1, &buf },
			{ client->addr, I2C_M_RD, 1, &buf },
		};

		xfer = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));

		if (xfer != ARRAY_SIZE(msgs)) {
			dev_err(&client->dev,
				"%s: could not read register 0x%02X\n",
				__FUNCTION__, pattern[i].reg);

			return -EIO;
		}

		value = BCD2BIN(buf & pattern[i].mask);

		if (value > pattern[i].max ||
			value < pattern[i].min) {
			dev_dbg(&client->dev,
				"%s: pattern=%d, reg=%x, mask=0x%02x, min=%d, "
				"max=%d, value=%d, raw=0x%02X\n",
				__FUNCTION__, i, pattern[i].reg, pattern[i].mask,
				pattern[i].min, pattern[i].max,
				value, buf);

			return -ENODEV;
		}
	}

	return 0;
}

static int pcf8563_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	return pcf8563_get_datetime(to_i2c_client(dev), &alrm->time, alrm);
}

static int pcf8563_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	return pcf8563_set_alarm(to_i2c_client(dev), alrm);
}

static int pcf8563_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	return pcf8563_get_datetime(to_i2c_client(dev), tm, 0);
}

static int pcf8563_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return pcf8563_set_datetime(to_i2c_client(dev), tm, 0);
}

static const struct rtc_class_ops pcf8563_rtc_ops = {
	.read_time	= pcf8563_rtc_read_time,
	.set_time	= pcf8563_rtc_set_time,
	.read_alarm	= pcf8563_rtc_read_alarm,
	.set_alarm	= pcf8563_rtc_set_alarm,
};

static int pcf8563_attach(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, pcf8563_probe);
}

static struct i2c_driver pcf8563_driver = {
	.driver		= {
		.name	= "pcf8563",
	},
	.id		= I2C_DRIVERID_PCF8563,
	.attach_adapter = &pcf8563_attach,
	.detach_client	= &pcf8563_detach,
};

static int pcf8563_probe(struct i2c_adapter *adapter, int address, int kind)
{
	struct pcf8563 *pcf8563;
	struct i2c_client *client;
	struct rtc_device *rtc;

	int err = 0;

	dev_dbg(&adapter->dev, "%s\n", __FUNCTION__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit;
	}

	if (!(pcf8563 = kzalloc(sizeof(struct pcf8563), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}

	client = &pcf8563->client;
	client->addr = address;
	client->driver = &pcf8563_driver;
	client->adapter	= adapter;

	strlcpy(client->name, pcf8563_driver.driver.name, I2C_NAME_SIZE);

	/* Verify the chip is really an PCF8563 */
	if (kind < 0) {
		if (pcf8563_validate_client(client) < 0) {
			err = -ENODEV;
			goto exit_kfree;
		}
	}

	/* Inform the i2c layer */
	if ((err = i2c_attach_client(client)))
		goto exit_kfree;

	dev_info(&client->dev, "chip found, driver version " DRV_VERSION "\n");

	rtc = rtc_device_register(pcf8563_driver.driver.name, &client->dev,
				&pcf8563_rtc_ops, THIS_MODULE);

	if (IS_ERR(rtc)) {
		err = PTR_ERR(rtc);
		goto exit_detach;
	}

	i2c_set_clientdata(client, rtc);

	return 0;

exit_detach:
	i2c_detach_client(client);

exit_kfree:
	kfree(pcf8563);

exit:
	return err;
}

static int pcf8563_detach(struct i2c_client *client)
{
	struct pcf8563 *pcf8563 = container_of(client, struct pcf8563, client);
	int err;
	struct rtc_device *rtc = i2c_get_clientdata(client);

	if (rtc)
		rtc_device_unregister(rtc);

	if ((err = i2c_detach_client(client)))
		return err;

	kfree(pcf8563);

	return 0;
}

static int __init pcf8563_init(void)
{
	return i2c_add_driver(&pcf8563_driver);
}

static void __exit pcf8563_exit(void)
{
	i2c_del_driver(&pcf8563_driver);
}

MODULE_AUTHOR("Alessandro Zummo <a.zummo@towertech.it>");
MODULE_DESCRIPTION("Philips PCF8563/Epson RTC8564 RTC driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

module_init(pcf8563_init);
module_exit(pcf8563_exit);
