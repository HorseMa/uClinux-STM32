/*
 * beep_ktp178.c
 *
 * Copyright (c) 2005 by Matthias Panten <matthias.panten@sympat.de>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/beep_ktp178.h>

#include <asm/arch/s3c44b0x.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>

#include <linux/delay.h>

/* decomment next line to switch off debug-outputs */
/*#define BEEP_KTP178_DEBUG_OUTPUTS*/
/*#define BEEP_KTP178_DELAY*/
/* for debugging */
#define BEEP_KTP178_DEBUG_ERR    (1 << 0)
#define BEEP_KTP178_DEBUG_OPEN   (1 << 1)
#define BEEP_KTP178_DEBUG_CLOSE  (1 << 2)
#define BEEP_KTP178_DEBUG_INTER  (1 << 3)
#define BEEP_KTP178_DEBUG_IOCTL  (1 << 4)
#define BEEP_KTP178_DEBUG_MASK   (BEEP_KTP178_DEBUG_IOCTL)

#ifdef BEEP_KTP178_DEBUG_OUTPUTS
#define BEEP_KTP178_DEBUG(x,args...) if (x & BEEP_KTP178_DEBUG_MASK) printk("\nktp178: " args);
#else
#define BEEP_KTP178_DEBUG(x,args...)
#endif

/** default frequency is f3d = 1kHz for old prescaler value */
/** default frequency is 14 = 900Hz */
#define _BEEP_FREQ_STD 0x14
/** default duty is freq/2 */
#define _BEEP_DUTY_STD (_BEEP_FREQ_STD >> 1)
/** default duration is 20 x10ms */
#define _BEEP_DURAT_STD 20

/** maximum frequency value is f3d = 1kHz */
#define _BEEP_FREQ_MAX 0xF3D
/** maximum duration is 200 x10ms */
#define _BEEP_DURAT_MAX 200

/* defines for timer-regiser */
#define _START_TIMER_1 0x900
#define _STOP_TIMER_1  0xA00
#define _TIMER_1_MASK  0xFFFFF0FF
#define _FREQUENCY_1_MASK 0xFFFF0000
#define _DUTY_1_MASK 0xFFFF0000
#define BEEP_DUTYMASK 0x0000FFFF

/** A kernel timer that is used for touch-beep */
static struct timer_list tp178_beep_timer;

#ifdef BEEP_KTP178_DELAY
/** pause between two beeps (x 10ms) */
static int ktp178_beeppause = 5;
#endif

/** flag for using the piezo (0 = actually no beep) */
static unsigned int ktp178_beep_inuse = 0;

/* function declarations
   --------------------------------*/
void _beep_start(void);
void _beep_stop(void);
int _beep_set_freq_duty(unsigned int freq, unsigned int duty);
int _beep_set_freq_vol(unsigned int freq, unsigned int vol);
static int beep_ktp178_ioctl(struct inode *inode, struct file * file,
				unsigned int cmd, unsigned long arg);
static int beep_ktp178_close(struct inode * inode, struct file * file);
static int beep_ktp178_open(struct inode *inode, struct file *file);
static int __init beep_ktp178_init(void);
static void __exit beep_ktp178_exit(void);
/*--------------------------------*/

static int beep_ktp178_major = BEEP_KTP178_MAJOR_NUMBER;

static beep_ktp178_t ktp178_beep_def;

static beep_ktp178_t ktp178_beep_now;

/** user-counter */
static int beep_ktp178_users = 0;

/**
 * inline-function to start the pwm-timer
 */
inline void _beep_start(void) 
{
	/* set the regisers */
	outl(((inl(S3C44B0X_TCON) & _TIMER_1_MASK) | _START_TIMER_1),
		S3C44B0X_TCON);
	/* set our use-var (1 = beep is activated) */
	ktp178_beep_inuse = 1;
}

/**
 * inline-function to stop the pwm-timer
 */
inline void _beep_stop(void) 
{
	/* set the regisers */
	outl(((inl(S3C44B0X_TCON) & _TIMER_1_MASK) | _STOP_TIMER_1),
		S3C44B0X_TCON);
	/* set our use-var (0 = beep is deactivated) */
	ktp178_beep_inuse = 0;
}

/**
 * beep_set_freq_duty
 *
 * @param unsigned int freq: timervalue for specific frequence
 * @param unsigned int duty: timervalue for specific duty
 * @return int: 0 = ok
 */
int _beep_set_freq_duty(unsigned int freq, unsigned int duty) 
{
	/* maximum duty is the frequence itself */
	if (duty >= freq)
		return -EINVAL;

	/* set the counter-register to get the frequency */
	outl(((inl(S3C44B0X_TCNTB1) & _FREQUENCY_1_MASK) | freq),
	     S3C44B0X_TCNTB1);

	/* set the compare-register to set the duty-time */
	outl(((inl(S3C44B0X_TCMPB1) & _DUTY_1_MASK) | duty),
	     S3C44B0X_TCMPB1);

	return 0;
}

/**
 * The Timerfunction to switch off the beep
 */
static void beep_beep_stop(unsigned long reserved) 
{
	/* reserved is yet unused */
	(void)reserved;
	
	/* stop the beep */
	_beep_stop();

	return;
}

/**
 * beep_new_one
 *
 * @param unsigned long beep_time: timervalue for beep-duration 
 */
static void beep_new_one(unsigned long reserved) 
{
#ifdef BEEP_KTP178_DELAY
	/* check if piezo oscillates */
	if (ktp178_beep_inuse) {
		/* first switch off the beep */ 
		_beep_stop();
		
		/* delete our (yet running) timer (to stop) */
		del_timer(&tp178_beep_timer);
		
		/* and after ktp178_beeppause x ms enter this function gain */
		init_timer(&tp178_beep_timer);
		tp178_beep_timer.function = beep_new_one;
		tp178_beep_timer.data = reserved;
		tp178_beep_timer.expires = jiffies + ktp178_beeppause;
		add_timer(&tp178_beep_timer);
		return;
	}
#endif
	
	/* set the registers to wanted configuration */
	_beep_set_freq_duty(ktp178_beep_now.freq, ktp178_beep_now.duty);
	
	/* now let the piezo oscillate (for a certian time) */
	_beep_start();
	
	/* delete the timer */
	del_timer(&tp178_beep_timer);
	
	/* and after x ms stop the piezo */
	init_timer(&tp178_beep_timer);
	tp178_beep_timer.function = beep_beep_stop;
	tp178_beep_timer.data = reserved;
	tp178_beep_timer.expires = jiffies + ktp178_beep_now.durat;
	add_timer(&tp178_beep_timer);
	
	return;
}

/**
 * The device-ioctl funcition
 *
 * Following IOCTLs are supported:
 */
static int beep_ktp178_ioctl(struct inode *inode, struct file * file,
	unsigned int cmd, unsigned long arg) 
{
	beep_ktp178_t data; /* params of the beep */

	BEEP_KTP178_DEBUG(BEEP_KTP178_DEBUG_IOCTL, "-> beep_ioctl:");
	
	switch(cmd) {
		case BEEP_KTP178_SET_SND:
			/* get parameters from userspace */
			copy_from_user(&data, (void*)arg, sizeof(data));
			BEEP_KTP178_DEBUG(BEEP_KTP178_DEBUG_IOCTL, 
				"beep_ioctl: duty=%x", data.duty);
			BEEP_KTP178_DEBUG(BEEP_KTP178_DEBUG_IOCTL,
				"beep_ioctl: freq=%x", data.freq);
			BEEP_KTP178_DEBUG(BEEP_KTP178_DEBUG_IOCTL,
				"beep_ioctl: durat=%x", data.durat);

			/* check duty maximum duty */
			if (data.duty >= data.freq) {
				return -EINVAL;
			}
			
			/* if duty or duration is 0 switch off beep */
			if ((data.duty == 0) || (data.durat == 0)) {
				/* delete the timer */
				del_timer(&tp178_beep_timer);
				
				/* stop the beep */
				_beep_stop();
				break;
			}

			ktp178_beep_now = data;
			
			/* start a new beep with duration */
			beep_new_one(arg);
			break;
		
		case BEEP_KTP178_GET_DEF:
			copy_to_user((void*)arg,
				&ktp178_beep_def, sizeof(ktp178_beep_def));
			break;

		case BEEP_KTP178_GET_MAX:
			data.freq = _BEEP_FREQ_MAX;
			data.durat = _BEEP_DURAT_MAX;
			data.duty = data.freq >> 1;
			copy_to_user((void*)arg,
				&data, sizeof(data));
			break;

		default:
			BEEP_KTP178_DEBUG(BEEP_KTP178_DEBUG_IOCTL,
					 "INVALID! IOCTL\n");
			return -EINVAL;
	}
	return 0;
}

/**
 * The device-close function
 * Decreases the user-count
 */
static int beep_ktp178_close(struct inode * inode, struct file * file) 
{
	BEEP_KTP178_DEBUG(BEEP_KTP178_DEBUG_CLOSE, "close device\n");

	/* decrease the user count inf not 0 */
	if (beep_ktp178_users)
		beep_ktp178_users--;

	return 0;
}

/**
 * The device-open function
 * Increases the user-count and starts the update-timer
 */
static int beep_ktp178_open(struct inode *inode, struct file *file) 
{
	BEEP_KTP178_DEBUG(BEEP_KTP178_DEBUG_OPEN, "open device\n");

	/* increase the user count (for now no user-limit)*/
	beep_ktp178_users++;

	return 0;
}

/**
 * Table of the supported device-operations
 */
struct file_operations beep_ktp178_fops = {
	owner: THIS_MODULE,
	read: NULL,
	write: NULL,
	poll: NULL,
	ioctl: beep_ktp178_ioctl,
	open: beep_ktp178_open,
	release: beep_ktp178_close,
	fasync: NULL,
};

/**
 * The device-exit function
 */
static void __exit beep_ktp178_exit(void) 
{
	unregister_chrdev(beep_ktp178_major, BEEP_KTP178_DEVICENAME);
	printk("beepdriver for KTP178 uninstalled\n");
}

/**
 * The device-init function
 *
 * Register the driver in the system
 * and switch off beep
 *
 */
static int __init beep_ktp178_init(void) 
{
	u_int32_t tcfg0;

	printk("KTP178 BEEPdriver: initalizing...\n");

	/* set prescaler-config for timer 0 (PPi) / 1 (BEEP) */
	tcfg0 = inl(S3C44B0X_TCFG0);
	tcfg0 &= ~0xFF;
        tcfg0 |= 206; /* this is for common xTP17x */
	outl(tcfg0, S3C44B0X_TCFG0);

	/* register char-device */
	if (register_chrdev(beep_ktp178_major, BEEP_KTP178_DEVICENAME,
		&beep_ktp178_fops) < 0) {
		printk("register failed!\n");
		return -ENODEV;
	}
	else
		printk("KTP178 BEEPdriver: init done\n");

	printk(BEEP_KTP178_DEVICENAME);
	printk(": KTP178 BEEPdriver\n");

	/* switch off any beep */
	_beep_stop();
	
	/* init the default-values */
	ktp178_beep_def.freq = _BEEP_FREQ_STD;
	ktp178_beep_def.duty = _BEEP_DUTY_STD;
	ktp178_beep_def.durat = _BEEP_DURAT_STD;

	return 0;
}

module_init(beep_ktp178_init);
module_exit(beep_ktp178_exit);

/*
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
