/*
 * kbd_ktp178.c
 *
 * This driver is not compatible with the generic linux mouse protocol.
 * There is a microwindows driver available that supports this keyboard
 * driver.
 *
 * Copyright (c) 2004 by Matthias Panten <matthias.panten@sympat.de>
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

#include <asm/arch/s3c44b0x.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/arch/irq.h>
#include <asm/arch/irqs.h>

#include <linux/delay.h>

/* decomment next line to switch off debug-outputs */
/*#define DEBUG_OUTPUTS*/
/* for debugging */
#define KBD_KTP178_DEBUG_ERR    (1 << 0)
#define KBD_KTP178_DEBUG_OPEN   (1 << 1)
#define KBD_KTP178_DEBUG_CLOSE  (1 << 2)
#define KBD_KTP178_DEBUG_INTER  (1 << 3)
#define KBD_KTP178_DEBUG_IOCTL  (1 << 4)
#define KBD_KTP178_DEBUG_READ   (1 << 5)
#define KBD_KTP178_DEBUG_KEYGET (1 << 6)
#define KBD_KTP178_DEBUG_KEYSTO (1 << 7)
#define KBD_KTP178_DEBUG_MASK   (KBD_KTP178_DEBUG_KEYSTO)

#ifdef DEBUG_OUTPUTS
#define KBD_KTP178_DEBUG(x,args...) \
	if (x & KBD_KTP178_DEBUG_MASK) printk("kbdKTP177: " args);
#else
#define KBD_KTP178_DEBUG(x,args...)
#endif

/* function declarations */
static ssize_t kbd_ktp178_read(char *buffer, size_t count);
static int kbd_ktp178_init(void);
static int kbd_ktp178_open(struct inode *inode, struct file *file);
static int kbd_ktp178_close(struct inode * inode,
			    struct file * file);
static void kbd_ktp178_exit(void);
static void kbd_ktp178_keymask_to_buffer(unsigned long irq);
static void kbd_ktp178_interrupt(int irq, void *dev_id,
				 struct pt_regs *regs);
static unsigned int kbd_ktp178_poll(struct file *file, poll_table *wait);
static inline void _kbd_select_line(unsigned int scan_line);
static unsigned long kbd_ktp178_decode_keypad ( void );
static void kbd_ktp178_str_code(unsigned char keycode);
static ssize_t kbd_read_ktp178(struct file *file, char *buffer,
			       size_t len, loff_t *pos);

/** for select-functionality */
DECLARE_WAIT_QUEUE_HEAD(kbd_ktp178_wait);
static spinlock_t kbd_ktp178_lock = SPIN_LOCK_UNLOCKED;

/** time to wait after keyboard-line-switch (in us) */
#define DEBOUNCE_TIME 1000

/** scancode-buffersize */
#define KBD_KTP178_SCNBUFSZ 100

/** hardware interrupt to recognize keypress */
#define KBD_KTP178_IRQ S3C44B0X_INTERRUPT_EINT4

/** the devicename and major-number */
#define KBD_KTP178_DEVICENAME "kbd"
#define KBD_KTP178_MAJOR_NUMBER 241

static int kbd_ktp178_major = KBD_KTP178_MAJOR_NUMBER;

/** A kernel timer that is used for polling the keymask */
static struct timer_list kbd_ktp178_timer;
/** Number of connected users; only one is allowed */
static int kbd_ktp178_users = 0;

/** last keycode */
static unsigned char kbd_ktp178_last_keycode = 0;
/** buffer for scancodes - hardcoded */
static unsigned char kbd_ktp178_scnbuf[KBD_KTP178_SCNBUFSZ];
/** pointer, next keycode to send */
static unsigned int kbd_ktp178_next_send;
/** pointer, next keycode to store */
static unsigned int kbd_ktp178_next_store;
/** counter, actual buffercount */
static unsigned int kbd_ktp178_scncnt;


/*
 * the low-level-part generates the keymask
 */

/** address of the keyboard input port (latch) */
#define KBD_IN_PORT 0x06000000

/** SP lines for output */
#define SCAN_SEL_NUMBER    1

/** scanning order (linenumbers)
 * starting with first in list
 * 0xff selects all lines (for irq)
 */
static const int _actual_SCAN_SP[] = {
	-1, -1
};

/**
 * configure and set ports to scan a specific line
 * else 'scan' all lines (for interupt)
 *
 */
static inline void _kbd_select_line(unsigned int scan_line)
{
	/* in this 1 x 6 matrix: nothing to do here */

	/* to avoid unused var */
	(void)scan_line;
	return ;
}

/**
 * shift actual key-code and get the bits for pressed key(s)
 * this is possible for 32 keys
 *
 */
static inline void _kbd_get_lines(u_int32_t *key_code) 
{
	*key_code = (inl(KBD_IN_PORT) & 0x3f);
}

/** keynames */
enum KEYNAMES {
	KEY_F1 = (1 << 0),
	KEY_F2 = (1 << 1),
	KEY_F3 = (1 << 2),
	KEY_F4 = (1 << 3),
	KEY_F5 = (1 << 4),
	KEY_F6 = (1 << 5),
};

/** keymask for possible keys */
#define POSSIBLE_KEYS ((KEY_F1) | (KEY_F2) |\
		       (KEY_F3) | (KEY_F4) |\
		       (KEY_F5) | (KEY_F6))

/**
 * This function searches for the pressed keys on the
 * keypad and sets the correspondig bit in a KeyMask.
 *
 *  @return the KeyMask
 */
static unsigned long kbd_ktp178_decode_keypad (void) 
{
	unsigned int i;
	u_int32_t actual_key = 0;

	/* get actual pressed key(s) */
	for (i = 0; i < SCAN_SEL_NUMBER; i++) 	{
		/* port and direction select lines */
		_kbd_select_line(_actual_SCAN_SP[i]);

		_kbd_get_lines(&actual_key);
	}

	/* 'scan' all lines (for interrupt)*/
	_kbd_select_line(_actual_SCAN_SP[SCAN_SEL_NUMBER]);

	/* allow only possible keys */
	actual_key &= POSSIBLE_KEYS;

	return actual_key;
}

/*---------------------------------------------------------------------------
 * end of low-level-part
 *
 *-------------------------------------------------------------------------*/

typedef struct {
	unsigned long keymask;
	unsigned char scancode1;
	unsigned char scancode2;
	unsigned char modifier;
}
keymask_to_scancode_t;

/** the table of allowed keys */
keymask_to_scancode_t keys[] = {
	/* keyname, pre-scancode, scancode, modifier-scancode */
	/*@todo verify all keycodes */
	{KEY_F1 , 0x00, 0x3b, 0x00},
	{KEY_F2 , 0x00, 0x3c, 0x00},
	{KEY_F3 , 0x00, 0x3d, 0x00},
	{KEY_F4 , 0x00, 0x3e, 0x00},
	{KEY_F5 , 0x00, 0x3f, 0x00},
	{KEY_F6 , 0x00, 0x40, 0x00},
	{0, 0, 0, 0}
};

/**
 * This array is stored in the flash and contains the mapping from
 * scancodes to keycodes for single codes (no 0xe0,...  prior).
 * It could be left out because for now keycode = scancode in case of
 * single codes.
 *
 */
const unsigned char keycodes[128] = {
	0, 1, 2, 3, 4,              /*   0 */
	5, 6, 7, 8, 9,              /*   5 */
	10, 11, 12, 13, 14,         /*  10 */
	15, 16, 17, 18, 19,         /*  15 */
	20, 21, 22, 23, 24,         /*  20 */
	25, 26, 27, 28, 29,         /*  25 */
	30, 31, 32, 33, 34,         /*  30 */
	35, 36, 37, 38, 39,         /*  35 */
	40, 41, 42, 43, 44,         /*  40 */
	45, 46, 47, 48, 49,         /*  45 */
	50, 51, 52, 53, 54,         /*  50 */
	55, 56, 57, 58, 59,         /*  55 */
	60, 61, 62, 63, 64,         /*  60 */
	65, 66, 67, 68, 69,         /*  65 */
	70, 71, 72, 73, 74,         /*  70 */
	75, 76, 77, 78, 79,         /*  75 */
	80, 81, 82, 83, 84,         /*  80 */
	85, 86, 87, 88, 89,         /*  85 */
	90, 91, 92, 93, 94,         /*  90 */
	95, 96, 97, 98, 99,         /*  95 */
	100, 101, 102, 103, 104,    /* 100 */
	105, 106, 107, 108, 109,    /* 105 */
	110, 111, 112, 113, 114,    /* 110 */
	115, 116, 117, 118, 119,    /* 115 */
	120, 121, 122, 123, 124,    /* 120 */
	125, 126, 127               /* 125 */
};

/** keycodes for keys with leading e0-scancode */
#define E0_KPENTER 0x60
#define E0_UP      0x67
#define E0_LEFT    0x69
#define E0_RIGHT   0x6A
#define E0_DOWN    0x6C
#define E0_INSERT  0x6D

#define E0_DELETE  0x53
#define E0_HOME    0x47
#define E0_END     0x4F
#define E0_PGUP    0x49
#define E0_PGDOWN  0x51

/**
 * This array is stored in the flash and contains the mapping from
 *  scancodes to keycodes for codes with a prior 0xe0.
 *  This means if we receive the scancode 0xe0 0x1C we will generate
 *  a keycode of 0x60 (E0_KPENTER).
 *
 */
const unsigned char e0_keycodes[128] = {
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x00-0x07 */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x08-0x0f */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x10-0x17 */
	0, 0, 0, 0, E0_KPENTER, 0, 0, 0,                     /* 0x18-0x1f */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x20-0x27 */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x28-0x2f */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x30-0x37 */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x38-0x3f */
	0, 0, 0, 0, 0, 0, 0, E0_HOME,                        /* 0x40-0x47 */
	E0_UP, E0_PGUP, 0, E0_LEFT, 0, E0_RIGHT, 0, E0_END,  /* 0x48-0x4f */
	E0_DOWN, E0_PGDOWN, 0, E0_DELETE, 0, 0, 0, 0,        /* 0x50-0x57 */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x58-0x5f */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x60-0x67 */
	0, 0, 0, 0, 0, 0, 0, 0,                              /* 0x68-0x6f */
	0, E0_INSERT, 0, 0, 0, 0, 0, 0,                      /* 0x70-0x77 */
	0, 0, 0, 0, 0, 0, 0, 0                               /* 0x78-0x7f */
};

/**
 * used from the "Device read" function
 *
 */
static ssize_t kbd_ktp178_read(char *buffer, size_t count) 
{
	unsigned char keycode;
	unsigned long flags;

	keycode = kbd_ktp178_scnbuf[kbd_ktp178_next_send++];
	kbd_ktp178_next_send %= KBD_KTP178_SCNBUFSZ;

	spin_lock_irqsave(&kbd_ktp178_lock, flags);
	kbd_ktp178_scncnt--;
	spin_unlock_irqrestore(&kbd_ktp178_lock, flags);

	/* copy this key to user-space */
	if (copy_to_user(buffer, &keycode, 1))
		return -EFAULT;

	KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_READ, "rcd a key:%x\n", keycode);

	return count;
}

/**
 * store a keycode in the scanbuffer
 */
static void kbd_ktp178_str_code(unsigned char keycode) 
{
	/* buffer filled: nothing to do */
	if (kbd_ktp178_scncnt == KBD_KTP178_SCNBUFSZ)
		return ;

	kbd_ktp178_scnbuf[kbd_ktp178_next_store++] = kbd_ktp178_last_keycode
			= keycode;
	KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_KEYSTO, "store code:%x\n", keycode);
	kbd_ktp178_next_store %= KBD_KTP178_SCNBUFSZ;

	spin_lock(&kbd_ktp178_lock);
	kbd_ktp178_scncnt++;
	spin_unlock(&kbd_ktp178_lock);
}

/**
 * get actual keymask and transfer the scancode to the buffer
 *
 * called once from interrupt, repeats itself til no key pressed
 */
static void kbd_ktp178_keymask_to_buffer(unsigned long irq) 
{
	unsigned long KeyMask_hard;
	int cnt = 0;

	/* for max 64 keys 2x32 bit */
	unsigned long KeyMask[] = { 0, 0};
	unsigned long NewMask[] = { 0, 0};
	unsigned long RepKeyMask[] = { 0, 0};
	unsigned long ReleaseMask[] = { 0, 0};
	static unsigned long KeyMaskOld[] = { 0, 0};
	static unsigned long KeyMaskBlkd[] = { 0, 0};
	static unsigned int delay_counter = 0;

	/* buffer filled: nothing to do */
	if (kbd_ktp178_scncnt == KBD_KTP178_SCNBUFSZ) {
		enable_irq(irq);
		return ;
	}

	/* get actual keymask from hardware */
	KeyMask_hard = kbd_ktp178_decode_keypad();

	/* set the (soft)keymask according the keymask_hard */
	for (cnt = 0; (keys[cnt].keymask != 0) ; cnt++) {
		/* check if "key" (in fact key-combination) is pressed */
		if ((KeyMask_hard & keys[cnt].keymask) == keys[cnt].keymask) {
			/* SHIFT is modifier-key, SHIFT-state must be equal */
			/* NOT WITH KTP178    if ((KeyMask_hard & KEY_SHIFT)
						    == (keys[cnt].keymask &
						     KEY_SHIFT)) */
			{
				/* generate keymask and increase found keys */
				KeyMask[(cnt >> 5)] |= (1 << (cnt & 0x1f));
			}
		}
	}

	KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_KEYGET,
			 "\ngot_hard:%x got_soft:%x\n", KeyMask_hard, KeyMask);

	/* get all new pressed keys */
	NewMask[0] = ~KeyMaskOld[0] & KeyMask[0];
	NewMask[1] = ~KeyMaskOld[1] & KeyMask[1];

	/* no ghostkeys possible! nothing to validate */
	
	/* check for a new pressed key */
	if ((NewMask[0] == 0) && (NewMask[1] == 0)) {
		/* repeat if none found */
		RepKeyMask[0] = KeyMaskOld[0] & KeyMask[0] & (~KeyMaskBlkd[0]);
		RepKeyMask[1] = KeyMaskOld[1] & KeyMask[1] & (~KeyMaskBlkd[1]);
	}
	else {
		/* block repeating the old ones */
		KeyMaskBlkd[0] = KeyMaskOld[0] & KeyMask[0];
		KeyMaskBlkd[1] = KeyMaskOld[1] & KeyMask[1];
		
		/* delay the first repeat after newpress */
		delay_counter = 0;
	}

	/* to delay the first repeat increase counter */
	delay_counter++;

	/* get all released keys */
	ReleaseMask[0] = (KeyMaskOld[0] & (~KeyMask[0]));
	ReleaseMask[1] = (KeyMaskOld[1] & (~KeyMask[1]));

	/* set blocked keys */
	KeyMaskBlkd[0] &= (~ReleaseMask[0]);
	KeyMaskBlkd[1] &= (~ReleaseMask[1]);
	/* end building all needed key masks */

	/* check each key and send a code if neccessary */
	for (cnt = 0; (keys[cnt].keymask != 0) ; cnt++) {
		/* newpressed key? */
		if (NewMask[cnt >> 5] & (1 << (cnt & 0x1f))) {
			if (keys[cnt].modifier)
				kbd_ktp178_str_code(keys[cnt].modifier);
			if (keys[cnt].scancode1 == 0xe0)
				kbd_ktp178_str_code(keys[cnt].scancode1);
			kbd_ktp178_str_code(keys[cnt].scancode2);
		}
		/* key to repeat? */
		if ((RepKeyMask[cnt >> 5] & (1 << (cnt & 0x1f))) && 
		    (delay_counter >= KBD_KTP177_FIRST_REPEAT_DELAY)) {
			if (keys[cnt].scancode1 == 0xe0)
				kbd_ktp178_str_code(keys[cnt].scancode1);
			kbd_ktp178_str_code(keys[cnt].scancode2);
		}
		/* key to release? */
		if (ReleaseMask[cnt >> 5] & (1 << (cnt & 0x1f))) {
			if (keys[cnt].scancode1 == 0xe0)
				kbd_ktp178_str_code(keys[cnt].scancode1);
			kbd_ktp178_str_code(keys[cnt].scancode2 + 0x80);
			if (keys[cnt].modifier)
				kbd_ktp178_str_code(keys[cnt].modifier
						    + 0x80);
		}
	}

	/* save actual keymask */
	KeyMaskOld[0] = KeyMask[0];
	KeyMaskOld[1] = KeyMask[1];

	/* no key pressed, reenable irq to wait for next key */
	if (KeyMask_hard == 0) {
		/* clear S3C44B0X_EXTINPND0 (for EINT4) */
		s3c44b0x_clear_pb(irq);
		enable_irq(irq);
		return ;
	}

	/* wake up sleeping DEVICE-READ-function */
	wake_up_interruptible(&kbd_ktp178_wait);

	/* a key was pressed or released - repeat scan */
	init_timer(&kbd_ktp178_timer);
	kbd_ktp178_timer.function = kbd_ktp178_keymask_to_buffer;
	kbd_ktp178_timer.data = irq;
	kbd_ktp178_timer.expires = jiffies + KBD_KTP178_REPEATSCAN_DELAY;
	add_timer(&kbd_ktp178_timer);

	return ;
}

/**
 * Standard device poll function
 */
static unsigned int kbd_ktp178_poll(struct file *file, poll_table *wait) 
{
	poll_wait(file, &kbd_ktp178_wait, wait);
	if (kbd_ktp178_scncnt)
		return (POLLIN | POLLRDNORM);

	return 0;
}

/**
 * Device close function
 *
 * Reduces the user count and disables the keyboard interrupt
 */
static int kbd_ktp178_close(struct inode * inode, struct file * file) 
{
	KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_CLOSE, "close device\n");

	kbd_ktp178_users--;
	/* must be always true since we allow only one user */
	if (!kbd_ktp178_users)
		disable_irq(KBD_KTP178_IRQ);
	return 0;
}

/**
 * This function represents the device read function
 *
 * The function reads numOfKeyCodesRead times the scancodes via the
 * kbd_op77a_read function. If the read scancode is identical 0xEO a second
 * scancode byte has to be read and the keycode has to be extracted from
 * the e0_keycodes table. The calculated keycode is copied into the buffer
 * and the number of read scancodes is written into the parsed len value.
 *
 * @note 0xEO 0xXY (is one scancode)
 */
static ssize_t kbd_read_ktp178(struct file *file, char *buffer,
			       size_t len, loff_t *pos) 
{
	static char SpecCodeReached = 0;
	unsigned char *bp = (unsigned char *)buffer;
	unsigned char ReadBuffer = 0;

	u_int32_t ReadScanCodes = 0;
	unsigned long numOfKeyCodesRead = len;
	unsigned long ReadBytes = len;

	KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_READ, "want to read key\n");

	while (!kbd_ktp178_scncnt) 	{
		KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_READ, "wait for key!\n");
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_READ, "sleeping\n");
		if (wait_event_interruptible(kbd_ktp178_wait, kbd_ktp178_scncnt))
			return -ERESTARTSYS;
	}

	while (numOfKeyCodesRead > 0) {
		ReadScanCodes = kbd_ktp178_read(&ReadBuffer, 1);
		if ((ReadBuffer != 0xE0) && (ReadScanCodes == 1)) {
			/* the read scancode before was 0xe0 */
			if (SpecCodeReached == 1) {
				/* go into second table */
				*bp = e0_keycodes[(ReadBuffer & 0x7F)] |
				                  (ReadBuffer & 0x80);
				bp++;
				ReadBytes--;
				SpecCodeReached = 0;
			}
			else {
				/* go into first table */
				*bp = keycodes[(ReadBuffer & 0x7F)] |
				               (ReadBuffer & 0x80);
				bp++;
				ReadBytes--;
			}
		}
		else if (ReadBuffer == 0xE0) {
			/* read again */
			ReadScanCodes = kbd_ktp178_read(&ReadBuffer, 1);
			if (ReadScanCodes == 1) {
				/* go into second table */
				*bp = e0_keycodes[(ReadBuffer & 0x7F)] |
				                  (ReadBuffer & 0x80);
				bp++;
				ReadBytes--;
			}
			else {
				SpecCodeReached = 1;
			}
		}
		numOfKeyCodesRead--;
	}

	len -= ReadBytes;
	return len;
}

/**
 * Device open function
 *
 * It checks if the driver is in use (i.e. the user count is > 0)
 * and if so rejects the open. If the driver is not yet in use it
 * sets the user count to one and enables the touch screen interrupt.
 */
static int kbd_ktp178_open(struct inode *inode, struct file *file) 
{
	KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_OPEN, "open device\n");

	if (!kbd_ktp178_users) /* only one user is allowed!! */
		kbd_ktp178_users++;
	else
		return -EBUSY;

	/* 'flush' keyboard buffer */
	kbd_ktp178_scncnt = 0;
	/* reset buffer-pointers */
	kbd_ktp178_next_send = 0;
	kbd_ktp178_next_store = 0;
	return 0;
}

/**
 * Table of the supported keyboard-operations
 */
struct file_operations kbd_ktp178_fops = {
	owner:   THIS_MODULE,
	read:    kbd_read_ktp178,
	write:   NULL,
	poll:    kbd_ktp178_poll,
	ioctl:   NULL,
	open:    kbd_ktp178_open,
	release: kbd_ktp178_close,
	fasync:  NULL,
};

/* The interrupt function
 *
 * It instantiates and activates a timer that will handle the
 * determination of the touch position. This approach was choosen
 * to keep the interrupt load low.
 */
static void kbd_ktp178_interrupt(int irq, void *dev_id, struct pt_regs *regs) 
{
	KBD_KTP178_DEBUG(KBD_KTP178_DEBUG_INTER, "interrupt\n");

	/* disable irq */
	disable_irq(irq);

	/* setup kernel timer */
	init_timer(&kbd_ktp178_timer);
	kbd_ktp178_timer.function = kbd_ktp178_keymask_to_buffer;
	kbd_ktp178_timer.data = irq;
	kbd_ktp178_timer.expires = jiffies + KBD_KTP178_BHISR_DELAY;
	add_timer(&kbd_ktp178_timer);

	/* return from interrupt */
	return ;
}

/**
 * Standard device exit function
 */
static void __exit kbd_ktp178_exit(void) 
{
	unregister_chrdev(kbd_ktp178_major, KBD_KTP178_DEVICENAME);
	free_irq(KBD_KTP178_IRQ, NULL);
	printk("keyboard KTP178 uninstalled\n");
}

/**
 * Init function
 *
 * Configures the touch screen ports to enable interrupts.
 * Sets the IRQ settings to normal IRQ and rising edge active.
 * Request the interrupt from the kernel, disables it and
 * registers itself.
 */
static int __init kbd_ktp178_init(void) 
{
	printk("KTP178 Keyboarddriver: initalizing...\n");

	if (request_irq(KBD_KTP178_IRQ, kbd_ktp178_interrupt, 0,
			KBD_KTP178_DEVICENAME, NULL)) {
		printk("KTP178 Keyboarddriver: unable to get IRQ\n");
		return -EBUSY;
	}

	disable_irq(KBD_KTP178_IRQ);

	/* register char-device */
	if (register_chrdev(kbd_ktp178_major, KBD_KTP178_DEVICENAME,
			     &kbd_ktp178_fops) < 0) {
		printk("register failed!\n");
		free_irq(KBD_KTP178_IRQ, NULL);
		return -ENODEV;
	}
	else
		printk("KTP178 Keyboarddriver: init done\n");

	/* configure interrupt EXTINT4
	 * assuming INTCON set to non-vectored IRQ mode enabled
	 * set EINT4 to IRQ
	 */
	outl(inl(S3C44B0X_INTMOD) & ~(1 << 21), S3C44B0X_INTMOD);

	/* set EINT4 to low-active */
	outl((inl(S3C44B0X_EXTINT) & ~(7 << 16)) | (0 << 16), S3C44B0X_EXTINT);

	printk(KBD_KTP178_DEVICENAME);
	printk(": KTP178 Keyboarddriver\n");

	return 0;
}

module_init(kbd_ktp178_init);
module_exit(kbd_ktp178_exit);

/*
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
