/*
 * tp177touch.c
 *
 * This driver is not compatible with the generic linux mouse protocol.
 * There is a microwindows driver available that supports this touchscreen
 * driver.
 *
 * The program povides a more accurate calibration based on the provisions
 * of Carlos E. Vidales found at www.embedded.com
 *
 * Copyright (c) 2003 by Stefan Macher <stefan.macher@sympat.de>
 * Copyright (c) 2003 by Alexander Assel <alexander.assel@sympat.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>

#include <linux/config.h>
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

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/arch/irq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/s3c44b0x.h>

#include <linux/delay.h>

#include <linux/tp177touch.h>

/* debugging level
   -----------------*/
#ifdef TP177_TS_DEBUG_LEVEL
static int tp177_ts_debug_mask = TP177_TS_DEBUG_LEVEL;
#else
static int tp177_ts_debug_mask = TP177_TS_DEBUG_ERR;
// static int tp177_ts_debug_mask = TP177_TS_DEBUG_POS;
#endif

/* delay between interrupt and first read out of position
   --------------------------------------------------------*/
#ifdef TP177_TS_FIRST_POLL_DELAY
static int FirstPollDelay = TP177_TS_FIRST_POLL_DELAY;
#else
static int FirstPollDelay = 5; /* default value */
#endif

/* delay between two consecutive position read outs
   --------------------------------------------------------*/
#ifdef TP177_TS_POLL_DELAY
static int PollDelay = TP177_TS_POLL_DELAY;
#else
static int PollDelay = 10; /* default value is 100 ms */
#endif

/* screen informations
   Can also be configured via I/O control
   ---------------------*/
#ifdef TP177_TS_X_RES
static int xres = TP177_TS_X_RES;
#else
static int xres = 320;
#endif

#ifdef TP177_TS_Y_RES
static int yres = TP177_TS_Y_RES;
#else
static int yres = 240;
#endif

/* the default calibration */
/* will be updated through command-line-parameters */
struct tp177_ts_cal_set cal_def = {
	cal_x_off: 75,
	cal_x_scale: 955,
	cal_y_off: 75,
	cal_y_scale: 955,
};

/* the maximum values possible in hardware */
struct tp177_ts_cal_set cal_max = {
	cal_x_off: 0,
	cal_x_scale: 1024,
	cal_y_off: 0,
	cal_y_scale: 1024,
};

/* calibration settings for simple but faster calibration
   -------------------------------------------------------*/
struct tp177_ts_cal_set cal_set;

/* function declarations
   --------------------------------*/
static void           tp177_ts_config_pressed(void);
static void           tp177_ts_config_read_x(void);
static void           tp177_ts_config_read_y(void);
static unsigned int   tp177_ts_read_adc(int channel);
static void           tp177_ts_interv_poll(unsigned long irq);
static void           tp177_ts_interrupt(int irq, void *dev_id,
					 struct pt_regs *regs);
static int            tp177_ts_open(struct inode *inode, struct file *file);
static int            tp177_ts_close(struct inode * inode, struct file * file);
static unsigned int   tp177_ts_poll(struct file *file, poll_table *wait);
static int            tp177_ts_ioctl(struct inode *inode, struct file * file,
					unsigned int cmd, unsigned long arg);
static ssize_t        tp177_ts_read(struct file *file, char *buffer,
					size_t count, loff_t *pos);
static ssize_t        tp177_ts_write(struct file *file, const char *buffer,
					size_t count, loff_t *ppos);
static int __init     tp177_ts_init(void);
static void __exit    tp177_ts_exit(void);
static void           tp177_ts_map_to_coord(int x, int y, int *newx, int *newy);
/*--------------------------------*/

/** Table of the supported touchscreen operations
 */
struct file_operations tp177_ts_fops = {
	owner: THIS_MODULE,
	read: tp177_ts_read,
	write: tp177_ts_write,
	poll: tp177_ts_poll,
	ioctl: tp177_ts_ioctl,
	open: tp177_ts_open,
	release: tp177_ts_close,
	fasync: NULL,
};

/** The misc device entry
 */
static struct miscdevice tp177_ts_touchscreen = {
	TP177_TS_MINOR, "tp177_touchscreen", &tp177_ts_fops
};

/** A kernel timer that is used for polling the touchscreen position */
static struct timer_list tp177_ts_timer;

/** Number of connected users; only one is allowed */
static int tp177_ts_users = 0;

/** Indicates the last measured x position */
static int pos_x=0;

/** Indicates the last measured y position */
static int pos_y=0;

/** The x position that was read the last time */
static int last_pos_x=0;

/** The x position that was read the last time */
static int last_pos_y=0;

/** Indicates if the touch is currently pressed */
static unsigned char touch_pressed=0;

/** Indicates if there is an event pending */
static int touch_event=0;

static int x_reverse=0, y_reverse=0;


DECLARE_WAIT_QUEUE_HEAD(tp177_ts_wait);
static spinlock_t tp177_ts_lock = SPIN_LOCK_UNLOCKED;

/* #define DBG_OUT(x) outw(x, 0x04100010) */

#define TOUCH_ENXP_N (1 << 0)  /**< ENXP_N = port F.0 */
#define TOUCH_ENYM   (1 << 4)  /**< ENYM   = port F.4 */
#define TOUCH_ENYP_N (1 << 1)  /**< ENYP_N = port F.1 */
#define TOUCH_ENXM   (1 << 3)  /**< ENXM   = port F.3 */

/**
 * bitmask for detection of keypresses
 * X layer -> output, low-level
 *     ENXM   = 1
 *     ENXP_N = 1
 * Y layer -> input, low==pressed
 *     ENYM   = 0
 *     ENYP_N = 1
 */
#define TOUCH_BITMASK_DETECT (TOUCH_ENXM | TOUCH_ENXP_N | TOUCH_ENYP_N)

/**
 * bitmask for reading the X-axis value
 *
 * ENXM   = 1 -> TOU_XL = 0
 * ENXP_N = 0 -> TOU_XR = 1
 * ENYM   = 0 -> TOU_YD = input
 * ENYP_N = 1 -> TOU_YU = input
 */
#define TOUCH_BITMASK_READ_X (TOUCH_ENXM | TOUCH_ENYP_N)

/**
 * bitmask for reading the Y-axis value
 *
 * ENXM   = 0 -> TOU_XL = input
 * ENXP_N = 1 -> TOU_XR = input
 * ENYM   = 1 -> TOU_YD = 0
 * ENYP_N = 0 -> TOU_YU = 1
 */
#define TOUCH_BITMASK_READ_Y (TOUCH_ENXP_N | TOUCH_ENYM)

/**
 * Maps the measured touch position to pixel values according to the
 *         calibration settings.
 *
 *  ATTENTION: The numerator is given in pows of 2 (e.g. 21 means 2^21). This
 *  just a faster approach. Be careful not to get into overrun conditions by
 *  choosing the numerators too big.
 */
static void tp177_ts_map_to_coord(int x, int y, int *newx, int *newy) 
{
	int current_x, current_y;

	if (x < cal_set.cal_x_off)
		x=cal_set.cal_x_off;
	if (y < cal_set.cal_y_off)
		y= cal_set.cal_y_off;

	current_x = ((x - cal_set.cal_x_off) * xres) / cal_set.cal_x_scale;
	current_y = ((y - cal_set.cal_y_off) * yres) / cal_set.cal_y_scale;

	/* limit values */
	if (current_x >= xres)
		current_x = xres;
	if (current_y >= yres)
		current_y = yres;

	/* handle x/y reverse mode */
	if (x_reverse)
		current_x = xres - current_x;
	if (y_reverse)
		current_y = yres - current_y;

	if (newx)
		*newx = current_x;
	if (newy)
		*newy = current_y;
}

/**
 * Configures the touchscreen ports for testing if
 *         the touchscreen is pressed or not.
 *
 *  It configures
 *  - The Y plane as input (will be pulled up)
 *  - The X plane as output low level
 *
 *  If the touch is pressed the low level of the X plane is connected to
 *  the Y plane and therefore the voltage at AIN2 (AIT_YU) should also be low.
 *
 *  This setting is also used to enable the interrupt.
 *
 *  Note: The function just configures for the test but does not perform it!
 */
static void tp177_ts_config_pressed(void) 
{
	TP177_TS_DEBUG(TP177_TS_DEBUG_CONFIG, "switch to config pressed\n");

	/* set to detect: ENXM(1), ENXP_N(1), ENYM(1), ENYP_N(1) */
	outl(TOUCH_BITMASK_DETECT, S3C44B0X_PDATF);
}

/**
 * Configures the touchscreen ports for reading out the
 *         the x position
 *
 *  It configures
 *  - TOUCH_XH as output high
 *  - TOUCH_XL as output low
 *  - disable pull-ups for TOUCH_*
 *
 *  The voltage level at AIN1 then defines the X position
 */
static void tp177_ts_config_read_x(void) 
{
	TP177_TS_DEBUG(TP177_TS_DEBUG_CONFIG, "switch to config read x\n");

	/* set to read x: ENXM(1), ENXP_N(0), ENYM(0), ENYP_N(1) */
	outl(TOUCH_BITMASK_READ_X, S3C44B0X_PDATF);
}

/**
 * Configures the touchscreen ports for reading out the
 *         the y position
 *
 *  It configures
 *  - TOUCH_YH as output high
 *  - TOUCH_YL as output low
 *  - disable pull-ups for TOUCH_*
 *
 *  The voltage level at AIN0 then defines the Y position
 */
static void tp177_ts_config_read_y(void) 
{
	TP177_TS_DEBUG(TP177_TS_DEBUG_CONFIG, "switch to config read y\n");

	/* set to read y: ENXM(0), ENXP_N(1), ENYM(1), ENYP_N(0) */
	outl(TOUCH_BITMASK_READ_Y, S3C44B0X_PDATF);
}


/**
 * Reads out the ADC value
 *
 *  According to the S3C44B0X manual the procedure is as follows:
 *  - Activate conversion by setting bit 0 in ADCCON
 *  - Wait until the start bit is cleared (after one ADC clock) because
 *    the ready flag if 1 for one ADC clock after conversion started
 *  - Wait until the ready bit is 1
 *  - Wait on further ADC clock because the ready bit is set on ADC clock
 *    before conversion is finished
 *
 *  \return The ADC value
 */
static unsigned int tp177_ts_read_adc(int channel) 
{
	u_int32_t msrmnt;
	u_int32_t adcdat = 0;
	int i;

	/* select channel */
	outl((channel << 2), S3C44B0X_ADCCON);
	udelay(15); /* 15us switch time */

	/* do 1 dummy measurement and make 1 real one */
	for (i=0; i < TOUCH_MEASURE_DUMMY + TOUCH_MEASURE_REAL; i++) {
		s3c44b0x_mask_ack_irq(S3C44B0X_INTERRUPT_ADC);

		/* select channel + start */
		outl((channel << 2) | 0x00000001, S3C44B0X_ADCCON);

		/* wait for the ADC */
		for (msrmnt = 0;
		     (!(inl(S3C44B0X_INTPND) & (1<<S3C44B0X_INTERRUPT_ADC)));
		     msrmnt++) {
			/* a (normal) conversion takes about 0x18 steps */
			if (msrmnt > 0x123) {
				TP177_TS_DEBUG(TP177_TS_DEBUG_ERR,
				"ADC-ready FAILED\n");
				break;
			}
		}

		msrmnt = inl(S3C44B0X_ADCDAT);

		if (i >= TOUCH_MEASURE_DUMMY) adcdat += msrmnt;
	}
	adcdat /= TOUCH_MEASURE_REAL;

	return adcdat;
}

/**
 * This function is called by the kernel timer and checks the actual
 *         position.
 *
 *  At first we check if the touch is actually pressed.
 *  If not, we will reenable the interrupt and just return.
 *  If it is pressed, we will check the actually position and
 *  if it changed compared to the previous position we will store it
 */
static void tp177_ts_interv_poll(unsigned long irq) 
{
	unsigned int x, y, map_x, map_y;

	TP177_TS_DEBUG(TP177_TS_DEBUG_INFO, "polling\n");

	/* config the touchscreen ports for measuring if any key is pressed */
	tp177_ts_config_pressed();

	/* check if touch is no longer pressed */
	/* if port G2 high it can not be pressed otherwise it may be pressed */
	/* if ADC value is above threshold it is definitly not pressed */
	if ((inl(S3C44B0X_PDATG) & (1 << 2)) ||
		(tp177_ts_read_adc(2) > TP177_TS_PRESS_THRESHOLD)) {
		/* it is not pressed */
		if (touch_pressed)
			touch_event = 1; /* release event */
		touch_pressed = 0;
		/* ack interrupt */
		s3c44b0x_clear_pb(irq);
		/* reenable the touchscreen interrupt */
		enable_irq(irq);
		wake_up_interruptible(&tp177_ts_wait);
		return;
	}
	
	/* it is pressed so calculate the new position*/
	/* we start with reading the x position */
	tp177_ts_config_read_x();
	x = tp177_ts_read_adc(2);
	/* then we read the y position */
	tp177_ts_config_read_y();
	y = tp177_ts_read_adc(1);

	tp177_ts_map_to_coord(x, y, &map_x, &map_y);
	spin_lock(&tp177_ts_lock);
	pos_x = map_x;
	pos_y = map_y;
	/* an event occured if the position changed or */
	/* if the touch is newly pressed */
	if ((pos_x != last_pos_x) ||
		(pos_y != last_pos_y) ||
		(!touch_pressed))
		touch_event=1;

	touch_pressed = 1;
	spin_unlock(&tp177_ts_lock);

	init_timer(&tp177_ts_timer);
	tp177_ts_timer.function = tp177_ts_interv_poll;
	tp177_ts_timer.data = irq;
	tp177_ts_timer.expires = jiffies + PollDelay;
	add_timer(&tp177_ts_timer);

 	TP177_TS_DEBUG(TP177_TS_DEBUG_POS, "pressed:%d event:%d\n",
		 touch_pressed, touch_event);

	wake_up_interruptible(&tp177_ts_wait);
}

/**
 * The interrupt function
 *
 * It instantiates and activates a timer that will handle the
 * determination of the touch position. This approach was choosen
 * to keep the interrupt load low.
 */
static void tp177_ts_interrupt(int irq, void *dev_id, struct pt_regs *regs) 
{
	/* disable irq */
	disable_irq(irq);

	TP177_TS_DEBUG(TP177_TS_DEBUG_IRQ, "irq\n");

	/* setup kernel timer */
	init_timer(&tp177_ts_timer);
	tp177_ts_timer.function = tp177_ts_interv_poll;
	tp177_ts_timer.data = irq;
	tp177_ts_timer.expires = jiffies + FirstPollDelay;
	add_timer(&tp177_ts_timer);
	/* return from interrupt */
	
	/* the irq will be acked in interv_poll */
	return;
}

/**
 * Device open function
 *
 * It checks if the driver is in use (i.e. the user count is > 0)
 * and if so rejects the open. If the driver is not yet in use it
 * sets the user count to one and enables the touch screen interrupt.
 */
static int tp177_ts_open(struct inode *inode, struct file *file) 
{
	TP177_TS_DEBUG(TP177_TS_DEBUG_INFO, "open (#%d)\n", tp177_ts_users);

	if (!tp177_ts_users)
		enable_irq(TP177_TS_IRQ);
	tp177_ts_users++;

	if (tp177_ts_users > 2) /* only two users allowed!! for now */
		return -EBUSY;

	return 0;
}

/**
 * Device close function
 *
 * Reduces the user count and disables the touch screen interrupt
 */
static int tp177_ts_close(struct inode * inode, struct file * file) 
{
	tp177_ts_users--;
	TP177_TS_DEBUG(TP177_TS_DEBUG_INFO, "close (#%d)\n", tp177_ts_users);
	if (!tp177_ts_users)
		disable_irq(TP177_TS_IRQ);
	return 0;
}

/**
 * Standard device poll function
 */
static unsigned int tp177_ts_poll(struct file *file, poll_table *wait) 
{
	poll_wait(file, &tp177_ts_wait, wait);
	if (touch_event)
		return (POLLIN | POLLRDNORM);

	return 0;
}

/**
 * Touch screen ioctl function
 *
 * Following IOCTLs are supported:
   - TP177_TS_SET_CAL_PARAMS: \b
     The argument is a pointer to a cal_set / cal_set2 structure that
     contains the new calibration settings. \b
     ATTENTION: The numerators are given in pows of 2 (e.g. 21 means 2^21)
   - TP177_TS_GET_CAL_PARAMS: \b
     The argument is apointer to a cal_set / cal_set2 structure that
     will be filled with the actual calibration settings. \b
     ATTENTION: The numerators are given in pows of 2 (e.g. 21 means 2^21)
   - TP177_TS_SET_X_RES:
     The argument is a pointer to an integer containing the horizontal
     resolution of the screen
   - TP177_TS_SET_Y_RES:
     The argument is a pointer to an integer containing the vertical
     resolution of the screen
   - TP177_TS_GET_CAL_DEF:
     The argument is a pointer to a cal_set structure that will be filled
     with the default driver-calibration
   - TP177_TS_GET_CAL_MAX:
     The argument is a pointer to a cal_set structure that will be filled
     maximum touch-resolution
   - TP177_TS_GET_X_RES:
     The argument is a pointer to an integer that will be filled
     with the actual x-resolution of the screen
   - TP177_TS_GET_Y_RES:
     The argument is a pointer to an integer that will be filled
     with the actual y-resolution of the screen
   - TP177_TS_GET_X_REVERSE:
     The argument is a pointer to an integer that will be filled
     with the actual x-reverse mode
   - TP177_TS_GET_Y_REVERSE:
     The argument is a pointer to an integer that will be filled
     with the actual y-reverse mode
*/
static int tp177_ts_ioctl(struct inode *inode, struct file * file,
		unsigned int cmd, unsigned long arg) 
{
	
	switch(cmd) {
		case TP177_TS_SET_CAL_PARAMS:
			if (!arg) {
				TP177_TS_DEBUG(TP177_TS_DEBUG_ERR,
				"Set calibration params with zero pointer\n");
				return -EINVAL;
			}
			copy_from_user(&cal_set, (void*)arg, sizeof(cal_set));
			break;

		case TP177_TS_GET_CAL_PARAMS:
			if (!arg) {
				TP177_TS_DEBUG(TP177_TS_DEBUG_ERR,
				"Get calibration params with zero pointer\n");
				return -EINVAL;
			}
			copy_to_user((void*)arg, &cal_set, sizeof(cal_set));
			break;

		case TP177_TS_SET_X_RES:
			copy_from_user(&xres, (void*)arg, sizeof(xres));
			break;

		case TP177_TS_SET_Y_RES:
			copy_from_user(&yres, (void*)arg, sizeof(yres));
			break;

		case TP177_TS_GET_CAL_DEF:
			copy_to_user((void*)arg, &cal_def, sizeof(cal_def));
			break;

		case TP177_TS_GET_CAL_MAX:
			copy_to_user((void*)arg, &cal_max, sizeof(cal_max));
			break;

		case TP177_TS_GET_X_RES:
			copy_to_user((void*)arg, &xres, sizeof(xres));
			break;

		case TP177_TS_GET_Y_RES:
			copy_to_user((void*)arg, &yres, sizeof(yres));
			break;

		case TP177_TS_GET_X_REVERSE:
			copy_to_user((void*)arg, &x_reverse, 
				sizeof(x_reverse));
			break;

		case TP177_TS_GET_Y_REVERSE:
			copy_to_user((void*)arg, &y_reverse,
				 sizeof(y_reverse));
			break;

		default:
			TP177_TS_DEBUG(TP177_TS_DEBUG_ERR,
				 "Received unknown ioctl command\n");
			return -EINVAL;
	}

	return 0;
}

/** Device read function
 *
 * Writes the actual position and the pressed state into the
 * buffer as soon as a new event arrives. In case of a
 * non-blocking call and no pending event it returns with
 * -EWOULDBLOCK. The buffer has to be a pointer to a ts_event
 * structure that is used as interface. The size has therefore
 * to be sizeof(struct ts_event).
 */
static ssize_t tp177_ts_read(struct file *file, char *buffer,
				size_t count, loff_t *pos) 
{
	struct ts_event event;
	unsigned long flags;

	if (count != sizeof(struct ts_event))
		return -EINVAL;

	while (!touch_event) {
		if (file->f_flags&O_NDELAY) {
			return -EWOULDBLOCK;
		}
		
		if (signal_pending(current)) {
			return -ERESTARTSYS;
                }
		
		if (wait_event_interruptible(tp177_ts_wait, touch_event))
			return -ERESTARTSYS;
	}

	/* Grab the event */
	spin_lock_irqsave(&tp177_ts_lock, flags);

	event.x = pos_x;
	event.y = pos_y;
	event.pressure = touch_pressed;
	touch_event = 0;
	last_pos_x = pos_x;
	last_pos_y = pos_y;

	spin_unlock_irqrestore(&tp177_ts_lock, flags);

	event.pad = 0;

	if (copy_to_user(buffer, &event, sizeof(struct ts_event)))
		return -EFAULT;
	return count;
}

/**
 * Device write function is invalid for touch screen so it return -EINVAL
 */
static ssize_t tp177_ts_write(struct file *file, const char *buffer, size_t count,
				loff_t *ppos) 
{
	return -EINVAL;
}

/** 
 * Init function
 *
 * Configures the touch screen ports to enable interrupts.
 * Sets the IRQ settings to normal IRQ and falling edge active.
 * Configures Port G pin 3 to be used as INT2.
 * Request the interrupt from the kernel, disables it and
 * registers itself as a misc device with minor number
 * TP177_TS_MINOR.
 *
 */
static int __init tp177_ts_init(void) 
{
	/* the bootloader should have configured all */

	printk("tp177_touch: initializing...\n");

	if (request_irq(TP177_TS_IRQ, tp177_ts_interrupt, 0,
			 "tp177_touchscreen",NULL)) {
		printk("tp177_ts: unable to get IRQ\n");
		return -EBUSY;
	}

	disable_irq(TP177_TS_IRQ);

	/* register misc device */
	if (misc_register(&tp177_ts_touchscreen)<0) {
		free_irq(TP177_TS_IRQ, NULL);
		return -ENODEV;
	}

	/* configure interrupt EXTINT2 */
	
	/* normal IRQ for EXTINT2*/
	outl((inl(S3C44B0X_INTMOD) & ~(1<<23)), S3C44B0X_INTMOD);

	/* EXTINT2 is lowlevel sensitiv */
	outl((inl(S3C44B0X_EXTINT) & ~(0x7<<8)), S3C44B0X_EXTINT);
	
	/* use Port G pin 2 as ext int 2 */
	outl(inl(S3C44B0X_PCONG) | (0x3<<4), S3C44B0X_PCONG);

	/*configure touchscreen ports for interrupt*/
	tp177_ts_config_pressed();

	printk("tp177_ts_touch: init ready\n");

	return 0;
}

/**
 * Standard device exit function
 */
static void __exit tp177_ts_exit(void) 
{
	misc_deregister(&tp177_ts_touchscreen);
	free_irq(TP177_TS_IRQ, NULL);
	printk(KERN_INFO "tp177_ts touchscreen uninstalled\n");
}

/**
 * get cofnig form command-line
 */
static int option_parse(char *str) 
{
	int temp[7];

	get_options(str, 7, temp);

	if (temp[0] != 6)
		return 0;

	if (temp[3]/*x_raw_left*/ < temp[4]/*x_raw_right*/) {
                cal_set.cal_x_off = temp[3]; /*x_raw_left*/
                cal_set.cal_x_scale = temp[4]/*x_raw_right*/-
					temp[3];/*x_raw_left;*/
		x_reverse = 0;
	}
	else {
		cal_set.cal_x_off = temp[4];/*x_raw_right;*/
		cal_set.cal_x_scale = temp[3]/*x_raw_left*/-
					temp[4];/*x_raw_right;*/
		x_reverse = 1;
	}

	if (temp[5] < temp[6]) {
		cal_set.cal_y_off = temp[5];
		cal_set.cal_y_scale = temp[6]-temp[5];
		y_reverse = 0;
	}
	else {
		cal_set.cal_y_off = temp[6];
		cal_set.cal_y_scale = temp[5]-temp[6];
		y_reverse = 1;
	}
	
	xres = temp[1];
	yres = temp[2];

	cal_def = cal_set;

	return 1;
}

__setup("touchcal=", option_parse);

MODULE_AUTHOR("Stefan Macher & Alexander Assel; sympat GmbH Nuremberg");
MODULE_DESCRIPTION("TP177 touch screen driver");

MODULE_PARM(FirstPollDelay, "i");
MODULE_PARM_DESC(FirstPollDelay, "Is the delay between the interrupt and the "\
"first read out of the touchscreen position in jiffies (normallay 10 ms) "\
"(default is 2 -> 20 ms)");

MODULE_PARM(PollDelay, "i");
MODULE_PARM_DESC(PollDelay, "Is the delay between two consecutive read out "\
"of the touchscreen position in jiffies (normallay 10 ms)"\
" (default is 10 -> 100 ms)");

/*MODULE_PARM(irq, "i");*/
/*MODULE_PARM_DESC(irq, "IRQ number of TP177 touch screen controller "\
"(default is 22 -> EINT3)");*/

MODULE_PARM(adcpsr, "i");
MODULE_PARM_DESC(adcpsr, "Prescaler configuration for the ADC; valid values "\
"are 1...255 (default is 15)");

MODULE_PARM(xres, "i");
MODULE_PARM_DESC(xres, "X resolution of the display (default is 320 pixels)");

MODULE_PARM(yres, "i");
MODULE_PARM_DESC(yres, "Y resolution of the display (default is 240 pixels)");

MODULE_LICENSE("GPL");

module_init(tp177_ts_init);
module_exit(tp177_ts_exit);

/*
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
