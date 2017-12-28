/**
 * kbd_op77a.c
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

#include <asm/arch/s3c3410.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>

#include <linux/delay.h>

/* decomment next line to switch off debug-outputs */
/* #define DEBUG_OUTPUTS */
/* for debugging */
#define KBD_OP77A_DEBUG_ERR    (1 << 0)
#define KBD_OP77A_DEBUG_OPEN   (1 << 1)
#define KBD_OP77A_DEBUG_CLOSE  (1 << 2)
#define KBD_OP77A_DEBUG_INTER  (1 << 3)
#define KBD_OP77A_DEBUG_IOCTL  (1 << 4)
#define KBD_OP77A_DEBUG_READ   (1 << 5)
#define KBD_OP77A_DEBUG_KEYGET (1 << 6)
#define KBD_OP77A_DEBUG_KEYSTO (1 << 7)
#define KBD_OP77A_DEBUG_MASK   (KBD_OP77A_DEBUG_KEYSTO)

#ifdef DEBUG_OUTPUTS
#define KBD_OP77A_DEBUG(x,args...) \
	if (x & KBD_OP77A_DEBUG_MASK) printk("kbdop77A: " args);
#else
#define KBD_OP77A_DEBUG(x,args...)
#endif

/* function declarations
   --------------------------------*/
static ssize_t kbd_op77a_read(char *buffer, size_t count);
static int kbd_op77a_init(void);
static int kbd_op77a_open(struct inode *inode, struct file *file);
static int kbd_op77a_close(struct inode * inode, struct file * file);
static void kbd_op77a_exit(void);
static void kbd_op77a_keymask_to_buffer(unsigned long irq);
static void kbd_op77a_interrupt(int irq, void *dev_id,
				struct pt_regs *regs);
static unsigned int kbd_op77a_poll(struct file *file, poll_table *wait);
static inline void _kbd_select_line(unsigned int scan_line);
static unsigned long kbd_op77a_decode_keypad (void);
static void kbd_op77a_str_code(unsigned char keycode);
static ssize_t kbd_read_op77a(struct file *file, char *buffer,
			      size_t len, loff_t *pos);
static inline int validate_keys(unsigned long key_mask);      
/*--------------------------------*/

/** for select-functionality */
DECLARE_WAIT_QUEUE_HEAD(kbd_op77a_wait);
static spinlock_t kbd_op77a_lock = SPIN_LOCK_UNLOCKED;

/** prevent occuring interrups for scanning keylines */
static spinlock_t kbd_op77a_scan_lock = SPIN_LOCK_UNLOCKED;

/** time to wait after keyboard-line-switch (in us) */
#define DEBOUNCE_TIME 40

/**
 * Time (in us) to wait after changing the return line
 * configuration to reload all capacities
 */
#define RELOAD_TIME 40

/** scancode-buffersize */
#define KBD_OP77A_SCANBUFSZ 100

/** hardware interrupt to recognize keypress */
#define KBD_OP77A_IRQ S3C3410X_INTERRUPT_EINT11

/** delay for entering Bottom Half ISR first time x*10ms */
#if !defined(KBD_OP77A_BHISR_DELAY)
#warning "using default bhisr"
#define KBD_OP77A_BHISR_DELAY 2
#endif

/** repeat-scan-delay in x*10ms */
#if !defined(KBD_OP77A_REPEATSCAN_DELAY)
#define KBD_OP77A_REPEATSCAN_DELAY 30
#endif

/** delay first repeat in multiples of KBD_OP77A_REPEATSCAN_DELAY */
#if !defined(KBD_OP77A_FIRST_REPEAT_DELAY)
#define KBD_OP77A_FIRST_REPEAT_DELAY 5
#endif

/** the devicename and major-number */
#define KBD_OP77A_DEVICENAME "kbd"
#define KBD_OP77A_MAJOR_NUMBER 241

static int kbd_op77a_major = KBD_OP77A_MAJOR_NUMBER;

/*! \brief A kernel timer that is used for polling the keymask */
static struct timer_list kbd_op77a_timer;
/*! \brief Number of connected users; only one is allowed */
static int kbd_op77a_users = 0;

/*! \brief buffer for scancodes - hardcoded */
static unsigned char kbd_op77a_scnbuf[KBD_OP77A_SCANBUFSZ];
/*! \brief pointer, next keycode to send */
static unsigned int kbd_op77a_next_send;
/*! \brief pointer, next keycode to store */
static unsigned int kbd_op77a_next_store;
/*! \brief counter, actual buffercount */
static unsigned int kbd_op77a_scncnt;

/*---------------------------------------------------------------------------
 * the low-level-part generates the keymask
 */

/** Mask for SP-lines at PCON0 (SP0 .. SP3) OP77A */
#define PCON0_SP_MASK 0xFFFFFE07

/** Mask for SP-lines at PDAT0 (SP0 .. SP3) OP77A */
#define PDAT0_SP_MASK 0x87

/** Mask for SP-lines at PCON6 (SP4 .. SP7) OP77A */
#define PCON6_SP_MASK 0xFFFFFC00

/** Mask for SP-lines at PDAT6 (SP4 .. SP7) OP77A */
#define PDAT6_SP_MASK 0xF0

/** port 0.3 = SP0 OP77A*/
#define SCAN_SP0  (~(1 << 3) & ~PDAT0_SP_MASK)
/** port 0.4 = SP1 OP77A*/
#define SCAN_SP1  (~(1 << 4) & ~PDAT0_SP_MASK)
/** port 0.5 = SP2 OP77A*/
#define SCAN_SP2  (~(1 << 5) & ~PDAT0_SP_MASK)
/** port 0.6 = SP3 OP77A*/
#define SCAN_SP3  (~(1 << 6) & ~PDAT0_SP_MASK)
/** port 6.0 = SP4 OP77A = SP0 OP73*/
#define SCAN_SP4  (~(1 << 0) & ~PDAT6_SP_MASK) /* 0000 1110 */
/** port 6.1 = SP5 OP77A = SP1 OP73*/
#define SCAN_SP5  (~(1 << 1) & ~PDAT6_SP_MASK)
/** port 6.2 = SP6 OP77A = SP2 OP73*/
#define SCAN_SP6  (~(1 << 2) & ~PDAT6_SP_MASK)
/** port 6.3 = SP7 OP77A = SP3 OP73*/
#define SCAN_SP7  (~(1 << 3) & ~PDAT6_SP_MASK)
/** to scan all for interrupt */
#define SCAN_ALL  0x00
/** to reload the capacitors */
#define RELOAD    0xff 

/**  SP0 .. SP3 output*/
#define PCON0_SCAN_ALL ((1<<3) | (1<<4) | (1<<5) | (1<<7))
/**  SP0 .. SP3 input*/
#define PCON0_SCAN_NONE 0
/**  SP0 output, others input*/
#define PCON0_SCAN_SP0 (1<<3)
/**  SP1 output, others input*/
#define PCON0_SCAN_SP1 (1<<4)
/**  SP2 output, others input*/
#define PCON0_SCAN_SP2 (1<<5)
/**  SP3 output, others input*/
#define PCON0_SCAN_SP3 (1<<7)

/**  SP4 .. SP7 output*/
#define PCON6_SCAN_ALL ((1<<0) | (1<<2) | (1<<5) | (1<<8))
/**  SP4 .. SP7 input*/
#define PCON6_SCAN_NONE 0
/**  SP4 output, others input*/
#define PCON6_SCAN_SP4 (1<<0)
/**  SP5 output, others input*/
#define PCON6_SCAN_SP5 (1<<2)
/**  SP6 output, others input*/
#define PCON6_SCAN_SP6 (1<<5)
/**  SP7 output, others input*/
#define PCON6_SCAN_SP7 (1<<8)

/** SP lines for output */
#define SCAN_SEL_NUMBER    8

/** scanning order (linenumbers)
 * starting with first in list
 * 0xff selects all lines (for irq)
 */
static const int _actual_SCAN_SP[] = {
	7, 6, 5, 4, 3, 2, 1, 0, -1, -2
};

/**
 * configure and set ports to scan a specific line
 * else 'scan' all lines (for interupt)
 */
static inline void _kbd_select_line(unsigned int scan_line)
{
	u_int32_t act_con6;
	u_int16_t act_con0;
	u_int8_t act_dat6;
	u_int8_t act_dat0;

	/* for irq-locking and restoring */
	unsigned long flags;

	/* PCON configs for each scanline at specific port*/
	const u_int32_t select_SP[] = {
		PCON0_SCAN_SP0, PCON0_SCAN_SP1, PCON0_SCAN_SP2, PCON0_SCAN_SP3,
		PCON6_SCAN_SP4, PCON6_SCAN_SP5, PCON6_SCAN_SP6, PCON6_SCAN_SP7
	};

	/* scanmask for each scanline at specific port*/
	const u_int32_t state_SP[] = {
		SCAN_SP0, SCAN_SP1, SCAN_SP2, SCAN_SP3,
		SCAN_SP4, SCAN_SP5, SCAN_SP6, SCAN_SP7
	};

	switch (scan_line) {
		case 0:
		case 1:
		case 2:
		case 3:
			/* to prevent interaction with SPC2 setting, disable interrupts */
			spin_lock_irqsave(kbd_op77a_scan_lock, flags);

			/* scanning with port0, none scanning with port6 */
			outl(((inl(S3C3410X_PCON6) & PCON6_SP_MASK) | PCON6_SCAN_NONE),
			     S3C3410X_PCON6);

			/* get actual state port0*/
			act_con0 = (inw(S3C3410X_PCON0) & PCON0_SP_MASK) |
				   select_SP[scan_line];

			/* get actual PDAT0 */
			act_dat0 = (inb(S3C3410X_PDAT0) & PDAT0_SP_MASK) |
				   state_SP[scan_line];

			/* set PDAT0 */
			outb(act_dat0, S3C3410X_PDAT0);

			/* set new config port0*/
			outw(act_con0, S3C3410X_PCON0);

			/* now we are safe again - enable interrupts */
			spin_unlock_irqrestore(kbd_op77a_scan_lock, flags);
			break;

		case 4:
		case 5:
		case 6:
		case 7:
			/* scanning with port6, none scanning with port0 */
			outw(((inw(S3C3410X_PCON0) & PCON0_SP_MASK) |
			     PCON0_SCAN_NONE) , S3C3410X_PCON0);

			/* SP0 output, others tristate (input)*/
			act_con6 = (inl(S3C3410X_PCON6) & PCON6_SP_MASK) |
				   select_SP[scan_line];

			/* get actual PDAT6 */
			act_dat6 = (inb(S3C3410X_PDAT6) & PDAT6_SP_MASK) |
				   state_SP[scan_line];

			/* set PDAT6 */
			outb(act_dat6, S3C3410X_PDAT6);

			/* set new config port6 */
			outl(act_con6, S3C3410X_PCON6);

			break;

		case - 2:
			/* to prevent interaction with SPC2 setting,
			   disable interrupts */
			spin_lock_irqsave(kbd_op77a_scan_lock, flags);

			/* reload all line-capacitors */
			act_con0 = (inw(S3C3410X_PCON0) & PCON0_SP_MASK) |
				   PCON0_SCAN_ALL;

			/* get actual PDAT0 */
			act_dat0 = (inb(S3C3410X_PDAT0) & PDAT0_SP_MASK) |
				   (~PDAT0_SP_MASK & RELOAD);

			/* set PDAT0 */
			outb(act_dat0, S3C3410X_PDAT0);
			/* set new config */
			outw(act_con0, S3C3410X_PCON0);

			/* now we are safe again - enable interrupts */
			spin_unlock_irqrestore(kbd_op77a_scan_lock, flags);
#if (RELOAD_TIME > 0)
			/* wait until the capacities are reloaded */
			udelay(RELOAD_TIME);
#endif
			/* compute new config to 'scan' all (for interrupt)*/
			act_con6 = (inl(S3C3410X_PCON6) & PCON6_SP_MASK) |
				   PCON6_SCAN_ALL;

			/* get actual PDAT6 */
			act_dat6 = (inb(S3C3410X_PDAT6) & PDAT6_SP_MASK) |
				   (~PDAT6_SP_MASK & RELOAD);
			/* set PDAT6 */
			outb(act_dat6, S3C3410X_PDAT6);

			/* set new config */
			outl(act_con6, S3C3410X_PCON6);
			break;

		default:
			/* to prevent interaction with SPC2 setting,
			   disable interrupts */
			spin_lock_irqsave(kbd_op77a_scan_lock, flags);

			/* compute new config to 'scan' all (for interrupt)*/
			act_con0 = (inw(S3C3410X_PCON0) & PCON0_SP_MASK) |
				   PCON0_SCAN_ALL;

			/* get actual PDAT0 */
			act_dat0 = (inb(S3C3410X_PDAT0) & PDAT0_SP_MASK) |
				   (~PDAT0_SP_MASK & SCAN_ALL);

			/* set PDAT0 */
			outb(act_dat0, S3C3410X_PDAT0);

			/* set new config */
			outw(act_con0, S3C3410X_PCON0);

			/* now we are safe again - enable interrupts */
			spin_unlock_irqrestore(kbd_op77a_scan_lock, flags);

			/* compute new config to 'scan' all (for interrupt)*/
			act_con6 = (inl(S3C3410X_PCON6) & PCON6_SP_MASK) |
				   PCON6_SCAN_ALL;

			/* get actual PDAT6 */
			act_dat6 = (inb(S3C3410X_PDAT6) & PDAT6_SP_MASK) |
				   (~PDAT6_SP_MASK & SCAN_ALL);

			/* set PDAT6 */
			outb(act_dat6, S3C3410X_PDAT6);

			/* set new config */
			outl(act_con6, S3C3410X_PCON6);
			break;
	}
}

/**
 * shift actual key-code and get the bits for pressed key(s)
 * this is possible for 32 keys
 */
static inline void _kbd_get_lines(u_int32_t *key_code)
{
	*key_code = (*key_code << 4) | (~inb(S3C3410X_PDAT5) & 0x0f);
}

/** the keynames and their bitmask */
enum KEYNAMES {
	/** KEY_NAME = (1<< (Z + 4*SP)*/
	KEY_F1     = (1 << (0 +  0)), KEY_1     = (1 << (1 +  0)),
	KEY_4      = (1 << (2 +  0)), KEY_7     = (1 << (3 +  0)),
	KEY_F2     = (1 << (0 +  4)), KEY_2     = (1 << (1 +  4)),
	KEY_5      = (1 << (2 +  4)), KEY_8     = (1 << (3 +  4)),
	KEY_F3     = (1 << (0 +  8)), KEY_3     = (1 << (1 +  8)),
	KEY_6      = (1 << (2 +  8)), KEY_9     = (1 << (3 +  8)),
	KEY_F4     = (1 << (0 + 12)), KEY_Cul   = (1 << (1 + 12)),
	KEY_INSDEL = (1 << (2 + 12)), KEY_DOT   = (1 << (3 + 12)),
	KEY_K1     = (1 << (0 + 16)), KEY_Cuu   = (1 << (1 + 16)),
	KEY_HELP   = (1 << (2 + 16)), KEY_0     = (1 << (3 + 16)),
	KEY_K2     = (1 << (0 + 20)), KEY_Cur   = (1 << (1 + 20)),
	KEY_TAB    = (1 << (2 + 20)), KEY_KOMMA = (1 << (3 + 20)),
	KEY_NONE   = (1 << (0 + 24)), KEY_ESC   = (1 << (1 + 24)),
	KEY_ACK    = (1 << (2 + 24)), KEY_SHIFT = (1 << (3 + 24)),
	KEY_K4     = (1 << (0 + 28)), KEY_Cud   = (1 << (1 + 28)),
	KEY_K3     = (1 << (2 + 28)), KEY_ENTER = (1 << (3 + 28))
};

/** keymask for possible keys (Z0 SP6 unused)*/
#define POSSIBLE_KEYS ~(KEY_NONE)

/**
 * This function searches for the pressed keys on the
 * keyboard and sets the correspondig bit in a KeyMask.
 *
 *  @return the KeyMask
 *
 */
static unsigned long kbd_op77a_decode_keypad (void)
{
	unsigned int i;
	u_int32_t actual_key = 0;

	/* get actual pressed key(s) */
	for (i = 0; i < SCAN_SEL_NUMBER; i++) {
		/* reload capacitors */
		_kbd_select_line(_actual_SCAN_SP[SCAN_SEL_NUMBER + 1]);

		/* port and direction select lines */
		_kbd_select_line(_actual_SCAN_SP[i]);

#if (DEBOUNCE_TIME > 0)
		udelay(DEBOUNCE_TIME);
#endif
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

/** typedef for the keylist */
typedef struct {
	unsigned long keymask;
	unsigned char scancode1;
	unsigned char scancode2;
	unsigned char modifier;
}
keymask_to_scancode_t;

/** this holds ALL allowed key-combinations */
keymask_to_scancode_t keys[] = {
	/* keyname, pre-scancode, scancode, modifier-scancode */
	{(KEY_SHIFT | KEY_INSDEL), 0xe0, 0x71, 0x00},  /* INS */
	{(KEY_SHIFT | KEY_KOMMA) , 0x00, 0x6f, 0x00},  /* +/- */
	{(KEY_SHIFT | KEY_DOT) ,   0xe0, 0x47, 0x00},  /* HOME */
	{(KEY_SHIFT | KEY_0) ,     0xe0, 0x4f, 0x00},  /* END */
	{(KEY_SHIFT | KEY_1) ,     0x00, 0x02, 0x2a},  /* 'A' */
	{(KEY_SHIFT | KEY_2) ,     0x00, 0x03, 0x2a},  /* 'B' */
	{(KEY_SHIFT | KEY_3) ,     0xe0, 0x51, 0x00},  /* PGDN */
	{(KEY_SHIFT | KEY_4) ,     0x00, 0x05, 0x2a},  /* 'C' */
	{(KEY_SHIFT | KEY_5) ,     0x00, 0x06, 0x2a},  /* 'D' */
	{(KEY_SHIFT | KEY_7) ,     0x00, 0x08, 0x2a},  /* 'E' */
	{(KEY_SHIFT | KEY_8) ,     0x00, 0x09, 0x2a},  /* 'F' */
	{(KEY_SHIFT | KEY_9) ,     0xe0, 0x49, 0x00},  /* PGUP */
	{(KEY_SHIFT | KEY_TAB) ,   0x00, 0x0f, 0x2a},  /* 'SHIFT-TAB' */
	{(KEY_SHIFT | KEY_Cul) ,   0xe0, 0x4b, 0x2a},  /* 'SHIFT-Cul' */
	{(KEY_SHIFT | KEY_Cur) ,   0xe0, 0x4d, 0x2a},  /* 'SHIFT-Cur' */
	{(KEY_SHIFT | KEY_Cuu) ,   0xe0, 0x48, 0x2a},  /* 'SHIFT-Cuu' */
	{(KEY_SHIFT | KEY_Cud) ,   0xe0, 0x50, 0x2a},  /* 'SHIFT-Cud' */
	{(KEY_SHIFT | KEY_ENTER),  0xe0, 0x1c, 0x2a},
	{(KEY_SHIFT | KEY_HELP) ,  0x00, 0x5e, 0x2a},
	{(KEY_SHIFT | KEY_ESC) ,   0x00, 0x01, 0x2a},
	{(KEY_SHIFT | KEY_ACK) ,   0x00, 0x58, 0x2a},
	{(KEY_SHIFT | KEY_F1) ,    0x00, 0x3b, 0x2a},
	{(KEY_SHIFT | KEY_F2) ,    0x00, 0x3c, 0x2a},
	{(KEY_SHIFT | KEY_F3) ,    0x00, 0x3d, 0x2a},
	{(KEY_SHIFT | KEY_F4) ,    0x00, 0x3e, 0x2a},
	{(KEY_SHIFT | KEY_K1) ,    0x00, 0x5a, 0x2a},
	{(KEY_SHIFT | KEY_K2) ,    0x00, 0x5b, 0x2a},
	{(KEY_SHIFT | KEY_K3) ,    0x00, 0x5c, 0x2a},
	{(KEY_SHIFT | KEY_K4) ,    0x00, 0x5d, 0x2a},
	{(KEY_SHIFT | KEY_6) ,     0x00, 0x07, 0x2a},
	{KEY_Cul ,                 0xe0, 0x4b, 0x00},
	{KEY_Cur ,                 0xe0, 0x4d, 0x00},
	{KEY_Cuu ,                 0xe0, 0x48, 0x00},
	{KEY_Cud ,                 0xe0, 0x50, 0x00},
	{KEY_ENTER ,               0xe0, 0x1c, 0x00},
	{KEY_ESC ,                 0x00, 0x01, 0x00},
	{KEY_ACK ,                 0x00, 0x58, 0x00},
	{KEY_F1 ,                  0x00, 0x3b, 0x00},
	{KEY_F2 ,                  0x00, 0x3c, 0x00},
	{KEY_F3 ,                  0x00, 0x3d, 0x00},
	{KEY_F4 ,                  0x00, 0x3e, 0x00},
	{KEY_INSDEL ,              0xe0, 0x53, 0x00},  /* DEL */
	{KEY_K1 ,                  0x00, 0x5a, 0x00},
	{KEY_K2 ,                  0x00, 0x5b, 0x00},
	{KEY_K3 ,                  0x00, 0x5c, 0x00},
	{KEY_K4 ,                  0x00, 0x5d, 0x00},
	{KEY_DOT ,                 0x00, 0x34, 0x00},
	{KEY_KOMMA ,               0x00, 0x41, 0x00},
	{KEY_0 ,                   0x00, 0x0b, 0x00},
	{KEY_1 ,                   0x00, 0x02, 0x00},
	{KEY_2 ,                   0x00, 0x03, 0x00},
	{KEY_3 ,                   0x00, 0x04, 0x00},
	{KEY_4 ,                   0x00, 0x05, 0x00},
	{KEY_5 ,                   0x00, 0x06, 0x00},
	{KEY_6 ,                   0x00, 0x07, 0x00},
	{KEY_7 ,                   0x00, 0x08, 0x00},
	{KEY_8 ,                   0x00, 0x09, 0x00},
	{KEY_9 ,                   0x00, 0x0a, 0x00},
	{KEY_TAB ,                 0x00, 0x0f, 0x00},
	{KEY_HELP ,                0x00, 0x5e, 0x00},
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
	0, 1, 2, 3, 4,    /* 0*/
	5, 6, 7, 8, 9,    /* 5*/
	10, 11, 12, 13, 14,    /* 10*/
	15, 16, 17, 18, 19,    /* 15*/
	20, 21, 22, 23, 24,    /* 20*/
	25, 26, 27, 28, 29,    /* 25*/
	30, 31, 32, 33, 34,    /* 30*/
	35, 36, 37, 38, 39,    /* 35*/
	40, 41, 42, 43, 44,    /* 40*/
	45, 46, 47, 48, 49,    /* 45*/
	50, 51, 52, 53, 54,    /* 50*/
	55, 56, 57, 58, 59,    /* 55*/
	60, 61, 62, 63, 64,    /* 60*/
	65, 66, 67, 68, 69,    /* 65*/
	70, 71, 72, 73, 74,    /* 70*/
	75, 76, 77, 78, 79,    /* 75*/
	80, 81, 82, 83, 84,    /* 80*/
	85, 86, 87, 88, 89,    /* 85*/
	90, 91, 92, 93, 94,    /* 90*/
	95, 96, 97, 98, 99,    /* 95*/
	100, 101, 102, 103, 104,    /* 100*/
	105, 106, 107, 108, 109,    /* 105*/
	110, 111, 112, 113, 114,    /* 110*/
	115, 116, 117, 118, 119,    /* 115*/
	120, 121, 122, 123, 124,    /* 120*/
	125, 126, 127              /* 125*/
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
 */
static ssize_t kbd_op77a_read(char *buffer, size_t count)
{
	unsigned char keycode;
	unsigned long flags;

	/* is there a buffer? */
	if (buffer == NULL)
		return 0;
	
	keycode = kbd_op77a_scnbuf[kbd_op77a_next_send++];
	kbd_op77a_next_send %= KBD_OP77A_SCANBUFSZ;

	spin_lock_irqsave(&kbd_op77a_lock, flags);
	kbd_op77a_scncnt--;
	spin_unlock_irqrestore(&kbd_op77a_lock, flags);

	/* copy this key to user-space */
	if (copy_to_user(buffer, &keycode, 1))
		return -EFAULT;

	KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_READ, "received a key:%x\n", keycode);

	return count;
}


/**
 * store a keycode in the scanbuffer
 */
static void kbd_op77a_str_code(unsigned char keycode)
{
	/* buffer filled: nothing to do */
	if (kbd_op77a_scncnt == KBD_OP77A_SCANBUFSZ)
		return ;

	kbd_op77a_scnbuf[kbd_op77a_next_store++] = keycode;
	KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_KEYSTO, "store keycode:%x\n", keycode);
	kbd_op77a_next_store %= KBD_OP77A_SCANBUFSZ;

	spin_lock(&kbd_op77a_lock);
	kbd_op77a_scncnt++;
	spin_unlock(&kbd_op77a_lock);
}

/**
 * get actual keymask and transfer the scancode to the buffer
 *
 * called once from interrupt, repeats itself til no key pressed
 */
static void kbd_op77a_keymask_to_buffer(unsigned long irq)
{
	unsigned long KeyMask_hard;
	int cnt = 0;

	/* for max 64 keys 2x32 bit */
	unsigned long KeyMask[] = { 0, 0};
	unsigned long NewMask[] = { 0, 0};
	unsigned long RepKeyMask[] = { 0, 0};
	unsigned long ReleaseMask[] = { 0, 0};
	static unsigned long GhostMask[] = { 0, 0};
	static unsigned long KeyMaskOld[] = { 0, 0};
	static unsigned long KeyMaskBlkd[] = { 0, 0};
	static unsigned int delay_counter = 0;
#ifdef OP77A_SPECIAL_KEY_HANDLING
	static unsigned int special_keys = 0;
	static unsigned long KeyMask_hard_old = 0;
#endif OP77A_SPECIAL_KEY_HANDLING

	/* buffer filled: nothing to do */
	if (kbd_op77a_scncnt == KBD_OP77A_SCANBUFSZ) {
		enable_irq(irq);
		return ;
	}

	/* get actual keymask from hardware */
	KeyMask_hard = kbd_op77a_decode_keypad();

#ifdef OP77A_SPECIAL_KEY_HANDLING
	/* handle the SHIFT-key with special care... */
	if (KeyMask_hard_old == 0) {
		/* remember single pressed SHIFT key */
		if (KeyMask_hard == KEY_SHIFT) {
			special_keys |= KEY_SHIFT;
		}
	}
#endif OP77A_SPECIAL_KEY_HANDLING

	/* set the (soft)keymask according the keymask_hard */
	for (cnt = 0; (keys[cnt].keymask != 0) ; cnt++) {
		/* check if "key" (in fact key-combination) is pressed */
		if ((KeyMask_hard & keys[cnt].keymask) == keys[cnt].keymask) {
			/* SHIFT is a modifier, so SHIFT-state must be equal */
			if ((KeyMask_hard & KEY_SHIFT) ==
			    (keys[cnt].keymask & KEY_SHIFT)) {
				/* generate keymask and increase found keys */
				KeyMask[(cnt >> 5)] |= (1 << (cnt & 0x1f));
#ifdef OP77A_SPECIAL_KEY_HANDLING
				/* cancel special key handling */
				special_keys = 0;
#endif OP77A_SPECIAL_KEY_HANDLING
			}
		}
	}

	KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_KEYGET,
			"\ngot_hard:%x got_soft:%x\n", KeyMask_hard, KeyMask);

	/* get all new pressed keys */
	NewMask[0] = ~KeyMaskOld[0] & KeyMask[0];
	NewMask[1] = ~KeyMaskOld[1] & KeyMask[1];

	/* mark ghost-keys */
	if (!validate_keys(KeyMask_hard)) {
		GhostMask[0] |= NewMask[0];
		GhostMask[1] |= NewMask[1];
	}

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
			/* dont put a ghostkey to the eventbuffer */
			if (!(GhostMask[cnt >> 5] & (1<< (cnt & 0x1f)))) {
				if (keys[cnt].modifier)
				       kbd_op77a_str_code(keys[cnt].modifier);
				if (keys[cnt].scancode1 == 0xe0)
				       kbd_op77a_str_code(keys[cnt].scancode1);
				kbd_op77a_str_code(keys[cnt].scancode2);
			}
		}
		/* key to repeat? but delay first repeat */
		if ((RepKeyMask[cnt >> 5] & (1 << (cnt & 0x1f))) && 
		    (delay_counter >= KBD_OP77A_FIRST_REPEAT_DELAY)) {
			/* dont put a ghostkey to the eventbuffer */
			if (!(GhostMask[cnt >> 5] & (1<< (cnt & 0x1f)))) {
				if (keys[cnt].scancode1 == 0xe0)
				       kbd_op77a_str_code(keys[cnt].scancode1);
				kbd_op77a_str_code(keys[cnt].scancode2);
			}
		}
		/* key to release? */
		if (ReleaseMask[cnt >> 5] & (1 << (cnt & 0x1f))) {
			/* dont put a ghostkey to the eventbuffer */
			if (!(GhostMask[cnt >> 5] & (1<< (cnt & 0x1f)))) {
				if (keys[cnt].scancode1 == 0xe0)
					kbd_op77a_str_code(keys[cnt].scancode1);
				kbd_op77a_str_code(keys[cnt].scancode2 + 0x80);
				if (keys[cnt].modifier)
					kbd_op77a_str_code(keys[cnt].modifier +
					0x80);
			}
			else {
				/* this ghostkey was released */
				GhostMask[cnt >> 5] &= ~(1 << (cnt & 0x1f));
			}
			
		}
	}

	/* save actual keymask */
	KeyMaskOld[0] = KeyMask[0];
	KeyMaskOld[1] = KeyMask[1];
#ifdef OP77A_SPECIAL_KEY_HANDLING
	KeyMask_hard_old = KeyMask_hard;
#endif OP77A_SPECIAL_KEY_HANDLING

	/* no key pressed, reenable irq to wait for next key */
	if (KeyMask_hard == 0) {
#ifdef OP77A_SPECIAL_KEY_HANDLING
		if (special_keys & KEY_SHIFT) {
			/* post SHIFT press event - yess - hardcoded */
			kbd_op77a_str_code(0x2a);
			/* post SHIFT release event - yess - hardcoded */
			kbd_op77a_str_code(0xaa);
		}
		special_keys = 0;
#endif OP77A_SPECIAL_KEY_HANDLING
		enable_irq(irq);
		return ;
	}

	/* wake up sleeping DEVICE-READ-function */
	wake_up_interruptible(&kbd_op77a_wait);

	/* a key was pressed or released - repeat scan */
	init_timer(&kbd_op77a_timer);
	kbd_op77a_timer.function = kbd_op77a_keymask_to_buffer;
	kbd_op77a_timer.data = irq;
	kbd_op77a_timer.expires = jiffies + KBD_OP77A_REPEATSCAN_DELAY;
	add_timer(&kbd_op77a_timer);

	return ;
}

/**
 * function to validate the pressed keys: check for ghost-keys
 *
 * @param key_mask keymask to check
 *
 * @return 1 for valid, 0 for invalid keymask (keymask with ghosts)
 */
static inline int validate_keys(unsigned long key_mask)
{
	unsigned long keys_pressed = key_mask;
	int keylines[SCAN_SEL_NUMBER];
	int cnt;
	int cnt_mult = 0;
	int line_act;
	int valid = 1; /* assuming valid keymask */

	/* an empty keymask is valid */
	if (!key_mask) return valid;

	/* check lines for more than one pressed key; store this linemasks */
	for (cnt = 0; cnt < SCAN_SEL_NUMBER; cnt++) {

		/* get last four bits */
		line_act = keys_pressed & 0xf;
		/* check if there is more than one bit set */
		if (line_act) {
			if ((line_act & ((line_act << 1) - 1)) != line_act) {
				/* no single press - store line */
				keylines[cnt_mult++] = line_act;
			}
		}

		/* now the next line */
		keys_pressed = keys_pressed >> 4;

		/* not more lines with bits set: we can bail out */
		if (!keys_pressed) break;
	}

	/* more than one line with more than one bit set */
	if (cnt_mult > 1) {
		/* compare lines with more than one bit set */
		for (line_act = 0; line_act < (cnt_mult - 1); line_act++) {
			for (cnt = line_act + 1; (cnt < cnt_mult); cnt++) {
				/* equal lines: mark keymask as invalid */
				if (keylines[line_act] == keylines[cnt]) {
					valid = 0; /* keymask is invalid */
				}
			}
		}   
	}
        
	return valid;
}

/**
 * Standard device poll function
 */
static unsigned int kbd_op77a_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &kbd_op77a_wait, wait);
	if (kbd_op77a_scncnt)
		return (POLLIN | POLLRDNORM);

	return 0;
}

/**
 * Device close function
 *
 * Reduces the user count and disables the keyboard interrupt
 */
static int kbd_op77a_close(struct inode * inode, struct file * file)
{
	KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_CLOSE, "close device\n");

	kbd_op77a_users--;
	/* must be always true since we allow only one user */
	if (!kbd_op77a_users)
		disable_irq(KBD_OP77A_IRQ);
		
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
static ssize_t kbd_read_op77a(struct file *file, char *buffer,
			      size_t len, loff_t *pos)
{
	static char SpecCodeReached = 0;
	unsigned char *bp = (unsigned char *)buffer;
	unsigned char ReadBuffer = 0;

	u_int32_t ReadScanCodes = 0;
	unsigned long numOfKeyCodesRead = len;
	unsigned long ReadBytes = len;
	
	if ((buffer == NULL) || (file == NULL) || (pos == NULL))
		return 0;
		
	KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_READ, "want to read key\n");

	while (!kbd_op77a_scncnt) {
		KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_READ, "wait for key!\n");
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_READ, "sleeping\n");
		if (wait_event_interruptible(kbd_op77a_wait, kbd_op77a_scncnt))
			return -ERESTARTSYS;
	}

	while (numOfKeyCodesRead > 0) {
		ReadScanCodes = kbd_op77a_read(&ReadBuffer, 1);
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
			ReadScanCodes = kbd_op77a_read(&ReadBuffer, 1);
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
static int kbd_op77a_open(struct inode *inode, struct file *file)
{
	KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_OPEN, "open device\n");

	/* only one user is allowed!! */
	if (!kbd_op77a_users)
		kbd_op77a_users++;
	else
		return -EBUSY;

	enable_irq(KBD_OP77A_IRQ);

	/* 'flush' keyboard buffer */
	kbd_op77a_scncnt = 0;
	/* reset buffer-pointers */
	kbd_op77a_next_send = 0;
	kbd_op77a_next_store = 0;
	return 0;
}

/**
 * Table of the supported keyboard-operations
 */
struct file_operations kbd_op77a_fops = {
	owner:   THIS_MODULE,
	read:    kbd_read_op77a,
	write:   NULL,
	poll:    kbd_op77a_poll,
	ioctl:   NULL,
	open:    kbd_op77a_open,
	release: kbd_op77a_close,
	fasync:  NULL,
};

/**
 * The interrupt function
 *
 * It instantiates and activates a timer that will handle the
 * determination of the touch position. This approach was choosen
 * to keep the interrupt load low.
 */
static void kbd_op77a_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	KBD_OP77A_DEBUG(KBD_OP77A_DEBUG_INTER, "interrupt\n");

	/*-------------------------
	  disable irq
	  -------------------------*/
	disable_irq(irq);

	/*-------------------------
	  setup kernel timer
	  -------------------------*/
	init_timer(&kbd_op77a_timer);
	kbd_op77a_timer.function = kbd_op77a_keymask_to_buffer;
	kbd_op77a_timer.data = irq;
	kbd_op77a_timer.expires = jiffies + KBD_OP77A_BHISR_DELAY;
	add_timer(&kbd_op77a_timer);
	/*--------------------------
	 return from interrupt
	 --------------------------*/

	return ;
}

/**
 * Standard device exit function
 */
static void __exit kbd_op77a_exit(void)
{
	unregister_chrdev(kbd_op77a_major, KBD_OP77A_DEVICENAME);
	free_irq(KBD_OP77A_IRQ, NULL);
	printk("keyboard OP77A uninstalled\n");
}

/**
 * Init function
 *
 * Configures the touch screen ports to enable interrupts.
 * Sets the IRQ settings to normal IRQ and rising edge active.
 * Request the interrupt from the kernel, disables it and
 * registers itself.
 */
static int __init kbd_op77a_init(void)
{
	int value;

	printk("OP77A Keyboarddriver: initalizing...\n");

	if (request_irq(KBD_OP77A_IRQ, kbd_op77a_interrupt,
			0, KBD_OP77A_DEVICENAME, NULL)) {
		printk("OP77A Keyboarddriver: unable to get IRQ\n");
		return -EBUSY;
	}

	disable_irq(KBD_OP77A_IRQ);

	/* register char-device */
	value = register_chrdev(kbd_op77a_major,
				KBD_OP77A_DEVICENAME, &kbd_op77a_fops);
	if (value < 0) {
		printk("register failed!\n");
		free_irq(KBD_OP77A_IRQ, NULL);
		return -ENODEV;
	}
	else
		printk("OP77A Keyboarddriver: init done\n");

	/*-------------------------------
	  configure interrupt EXTINT11
	  -------------------------------*/
	/* enable EINT11 */
	outw(inw(S3C3410X_EINTCON) | (1 << 11), S3C3410X_EINTCON);
	/* set EINT11 to rising edge */
	outl((inl(S3C3410X_EINTMOD) & 0x3FFFFFFF) | 0x40000000,
	     S3C3410X_EINTMOD);

	printk(KBD_OP77A_DEVICENAME);
	printk(": OP77A Keyboarddriver\n");

	return 0;
}

module_init(kbd_op77a_init);
module_exit(kbd_op77a_exit);

/*
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
