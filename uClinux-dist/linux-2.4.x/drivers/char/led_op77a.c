/*
 * led_op77a.c
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
#include <linux/led_op77a.h>

#include <asm/arch/s3c3410.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>

#include <linux/delay.h>

/* decomment next line to switch off debug-outputs */
/*#define DEBUG_OUTPUTS*/
/* for debugging */
#define LED_OP77A_DEBUG_ERR    (1 << 0)
#define LED_OP77A_DEBUG_OPEN   (1 << 1)
#define LED_OP77A_DEBUG_CLOSE  (1 << 2)
#define LED_OP77A_DEBUG_INTER  (1 << 3)
#define LED_OP77A_DEBUG_IOCTL  (1 << 4)
#define LED_OP77A_DEBUG_MASK   (LED_OP77A_DEBUG_OPEN | LED_OP77A_DEBUG_IOCTL)

#ifdef DEBUG_OUTPUTS
#define LED_OP77A_DEBUG(x,args...) \
if(x & LED_OP77A_DEBUG_MASK) printk("ledop77A: " args);
#else
#define LED_OP77A_DEBUG(x,args...)
#endif

/* function declarations
   --------------------------------*/
static int         led_op77a_init(void);
static int         led_op77a_open(struct inode *inode, struct file *file);
static int         led_op77a_close(struct inode * inode, struct file * file);
static void        led_op77a_exit(void);
static int         led_op77a_ioctl(struct inode *inode, struct file * file,
				   unsigned int cmd, unsigned long arg);
static void        led_op77a_update(unsigned long data);
static inline void _led_op77a_led_on(u_int8_t bitmask);
static inline void _led_op77a_led_off(u_int8_t bitmask);
static inline void _led_op77a_led_toggle(u_int8_t bitmask);
static inline void _set_all_leds(int mode);

/*--------------------------------*/

/** for locking write-access */
static spinlock_t led_op77a_lock = SPIN_LOCK_UNLOCKED;

/** base-timer-delay in x*10ms; setup in kernel-config */
/* #define LED_OP77A_UPDATE 25 */

/** counted base-time for freq A; setup in kernel-config */
/** 0 .. 1 = 0,5 sec (2 Hz = 2 * 250ms)*/
/* #define LED_OP77A_REPEATS_A 2 */

/** counted base-time for freq B; setup in kernel-config */
/** 0 .. 7 = 1,5 sec (0,5 Hz = 8 * 250ms) */
/* #define LED_OP77A_REPEATS_B 8 */

static int led_op77a_major = LED_OP77A_MAJOR_NUMBER;

/** Port connected to LEDs */
#define LED_PORT S3C3410X_EXTPORT

/** LED at which portpin */
enum LED_OP77A_PORTMASKS {
	LED_MASK_HELP = (1<<0),
	LED_MASK_K4   = (1<<1),
	LED_MASK_K3   = (1<<2),
	LED_MASK_K2   = (1<<3),
	LED_MASK_K1   = (1<<4),
	LED_MASK_ACK  = (1<<5)
};

/** LED-to-number definition */
static u_int8_t led_op77a_defs[] = {
	LED_MASK_HELP, /* number 0 */
	LED_MASK_K4,
	LED_MASK_K3,
	LED_MASK_K2,
	LED_MASK_K1,
	LED_MASK_ACK   /* number 5 */
};

#define LED_OP77A_LEDCOUNT sizeof(led_op77a_defs)

#define LED_MODEMASK (unsigned long)0x0000ffff

/** modes for each led */
static unsigned int led_op77a_mode[LED_OP77A_LEDCOUNT];

/** counter for freq A */
static int _led_op77a_cnt_a = (LED_OP77A_REPEATS_A - 1);

/** counter for freq B */
static int _led_op77a_cnt_b = (LED_OP77A_REPEATS_B - 1);

/** kernel timer to cyclic call the update-function */
static struct timer_list led_op77a_timer;

/** user-counter */
static int led_op77a_users = 0;

/*
 * set bitmask at specific port to use leds
 */
static inline void _led_op77a_led_on(u_int8_t bitmask)
{
	/* set bit at port */
	outb((inb(LED_PORT) | bitmask), LED_PORT);
}

/*
 * clear bitmask at specific port to use leds
 */
static inline void _led_op77a_led_off(u_int8_t bitmask)
{
	/* clear bit at port */
	outb((inb(LED_PORT) & ~bitmask), LED_PORT);
}

/*
 * toggle bitmask at specific port to use leds
 */
static inline void _led_op77a_led_toggle(u_int8_t bitmask)
{
	/* toggle bit at port */
	(inb(LED_PORT) & bitmask) ? _led_op77a_led_off(bitmask)
				  : _led_op77a_led_on(bitmask);
}

/*
 * set all LEDs with same mode
 */
static inline void _set_all_leds(int mode)
{
	int count;

	for(count = 0; count < LED_OP77A_LEDCOUNT; count++)
		led_op77a_mode[count] = mode;
}

/**
 * The device-ioctl funcition
 *
 * Following IOCTLs are supported:
 * - LED_OP77A_LED_SET
 *     parameter is one unsigned long as packed format for
 *     led-mode: 16 LSB
 *     led-id: 16 MSB
 *     e.g. mode ON for led 4: 0x00040003
 */
static int led_op77a_ioctl(struct inode *inode, struct file * file,
			   unsigned int cmd, unsigned long arg)
{
	unsigned int led_mode;
	unsigned int led_id;

	LED_OP77A_DEBUG(LED_OP77A_DEBUG_IOCTL, "led_ioctl: ");

	switch(cmd) {
		case LED_OP77A_SET_LED:
			/* get parameters */
			led_mode = (unsigned int)(arg & LED_MODEMASK);
			led_id = (unsigned int)(arg >> 16);

			/* check mode */
			if (led_mode > LED_OP77A_MODE_ON) {
				LED_OP77A_DEBUG(LED_OP77A_DEBUG_IOCTL,
				"invalid ulLedMode:%2u   ", led_mode);
				LED_OP77A_DEBUG(LED_OP77A_DEBUG_IOCTL,
				"ulLedMode min:0 max:%2u\n",
				(unsigned int)LED_OP77A_MODE_ON);
				return -EINVAL;
			}

			/* all leds to set in one mode */
			if (led_id == 0xffff) {
				_set_all_leds(led_mode);
				LED_OP77A_DEBUG(LED_OP77A_DEBUG_IOCTL,
				"all LEDS ulLedMode:%2u\n", led_mode);
				/* now all is done, bail out */
				break;
			}

			/* handle one LED */
			
			/* check ID */
			if (led_id >= LED_OP77A_LEDCOUNT) {
				LED_OP77A_DEBUG(LED_OP77A_DEBUG_IOCTL,
				"invalid ulLedID:%2u   ", led_id);
				LED_OP77A_DEBUG(LED_OP77A_DEBUG_IOCTL,
				"ulLedID min:0 max:%2u\n",
				((unsigned int)LED_OP77A_LEDCOUNT)-1);
				return -EINVAL;
			}

			LED_OP77A_DEBUG(LED_OP77A_DEBUG_IOCTL,
			"ID:%2u  Mode:%2u\n", led_id, led_mode);

			/* set specified LED to wanted mode */
			led_op77a_mode[ led_id] = led_mode;

			/* in mode ON and OFF switch immediately */
			if (led_mode == LED_OP77A_MODE_OFF)
				_led_op77a_led_off( led_op77a_defs[ led_id]);
			else if (led_mode == LED_OP77A_MODE_ON)
				_led_op77a_led_on( led_op77a_defs[ led_id]);

			break;

		default:
			LED_OP77A_DEBUG(LED_OP77A_DEBUG_IOCTL,
			"INVALID! IOCTL\n");
			return -EINVAL;
	}

	return 0;
}

/** update LEDs to wanted state
 * this is triggering itself if there is at least one user */
static void led_op77a_update(unsigned long data)
{

	int count;

	LED_OP77A_DEBUG(LED_OP77A_DEBUG_INTER, "led_update\n");

	for(count = 0; count < LED_OP77A_LEDCOUNT; count++) {
		switch( led_op77a_mode[count]) {
			case LED_OP77A_MODE_OFF:
				_led_op77a_led_off( led_op77a_defs[count]);
				break;

			case LED_OP77A_MODE_ON:
				_led_op77a_led_on( led_op77a_defs[count]);
				break;

			case LED_OP77A_MODE_FREQA:
				if (_led_op77a_cnt_a >
				   ((LED_OP77A_REPEATS_A - 1) >> 1))
					_led_op77a_led_on(
						led_op77a_defs[count]);
				else
					_led_op77a_led_off(
						led_op77a_defs[count]);
				break;

			case LED_OP77A_MODE_FREQB:
				if (_led_op77a_cnt_b >
				   ((LED_OP77A_REPEATS_B - 1) >> 1))
					_led_op77a_led_on(
						led_op77a_defs[count]);
				else
					_led_op77a_led_off(
						led_op77a_defs[count]);
				break;

			default:
				break;
		}
	}

	/* decrease counter */
	_led_op77a_cnt_a--;
	_led_op77a_cnt_b--;

	/* counter < 0 -> reload counter */
	if (_led_op77a_cnt_a < 0)
		_led_op77a_cnt_a = (LED_OP77A_REPEATS_A - 1);

	/* counter < 0 -> reload counter */
	if (_led_op77a_cnt_b < 0)
		_led_op77a_cnt_b = (LED_OP77A_REPEATS_B - 1);

	/* update again if in use */
	if (led_op77a_users) {
		/* restart timer */
		init_timer(&led_op77a_timer);
		led_op77a_timer.function = led_op77a_update;
		led_op77a_timer.data = 0;
		led_op77a_timer.expires = jiffies + LED_OP77A_UPDATE;
		add_timer(&led_op77a_timer);
	}
	return;
}

/**
 * The device-close function
 * Decreases the user-count
 */
static int led_op77a_close(struct inode * inode, struct file * file)
{
	LED_OP77A_DEBUG(LED_OP77A_DEBUG_CLOSE, "close device\n");

	/** get lock for write-access */
	spin_lock(&led_op77a_lock);

	/* decrease the user conut inf not 0 */
	if (led_op77a_users)
		led_op77a_users--;

	/** release lock after write-access */
	spin_unlock(&led_op77a_lock);

	return 0;
}

/**
 * The device-open function
 * Increases the user-count and starts the update-timer
 */
static int led_op77a_open(struct inode *inode, struct file *file)
{
	LED_OP77A_DEBUG(LED_OP77A_DEBUG_OPEN, "open device\n");

	/* first open? */
	if (!led_op77a_users) {
		/* init the counters */
		_led_op77a_cnt_b = (LED_OP77A_REPEATS_B - 1);
		_led_op77a_cnt_a = (LED_OP77A_REPEATS_A - 1);

		/* start LED update-function delayed */
		init_timer(&led_op77a_timer);
		led_op77a_timer.function = led_op77a_update;
		led_op77a_timer.data = 0;
		led_op77a_timer.expires = jiffies + LED_OP77A_UPDATE;
		add_timer(&led_op77a_timer);

	}

	/** get lock for write-access */
	spin_lock(&led_op77a_lock);

	/* increase the user count */
	led_op77a_users++;

	/** release lock after write-access */
	spin_unlock(&led_op77a_lock);

	return 0;
}

/**
 * Table of the supported device-operations
 */
struct file_operations led_op77a_fops = {
	owner: THIS_MODULE,
	read: NULL,
	write: NULL,
	poll: NULL,
	ioctl: led_op77a_ioctl,
	open: led_op77a_open,
	release: led_op77a_close,
	fasync: NULL,
};

/**
 * The device-exit function
 */
static void __exit led_op77a_exit(void)
{
	unregister_chrdev(led_op77a_major, LED_OP77A_DEVICENAME);
	printk("leddriver for OP77A uninstalled\n");
}

/**
 * The device-init function
 *
 * Register the driver in the system
 * and set the mode of all LEDs to OFF
 *
 */
static int __init led_op77a_init(void)
{
	int value;

	printk("OP77A LEDdriver: initalizing...\n");

	/* register char-device */
	value = register_chrdev(led_op77a_major, LED_OP77A_DEVICENAME, &led_op77a_fops);
	if(value < 0) {
		printk("register failed!\n");
		return -ENODEV;
	}
	else
		printk("OP77A LEDdriver: init done\n");

	printk(LED_OP77A_DEVICENAME);
	printk(": OP77A LEDdriver\n");

	/* all LEDs off */
	_set_all_leds(LED_OP77A_MODE_OFF);

	return 0;
}

module_init(led_op77a_init);
module_exit(led_op77a_exit);

/*
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
