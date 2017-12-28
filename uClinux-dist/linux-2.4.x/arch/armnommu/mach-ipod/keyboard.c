/*
 * keyboard.c - keyboard driver for iPod
 *
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/mm.h>
#include <linux/kbd_kern.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/arch/irqs.h>
#include <asm/keyboard.h>
#include <asm/hardware.h>

/* undefine these to produce keycodes from left/right/up/down */
#undef DO_SCROLLBACK
#undef DO_CONTRAST
#undef USE_ARROW_KEYS

/* we use the keycodes and translation is 1 to 1 */
#define R_SC		19	/* 'r' */
#define L_SC		38	/* 'l' */

#if defined(USE_ARROW_KEYS)
#define UP_SC		103
#define LEFT_SC		105
#define RIGHT_SC	106
#define DOWN_SC		108
#else
#define UP_SC		50	/* 'm' */
#define LEFT_SC		17	/* 'w' */
#define RIGHT_SC	33	/* 'f' */
#define DOWN_SC		32	/* 'd' */
#endif

#define ACTION_SC	28	/* '\n' */

/* send ^S and ^Q for the hold switch */
#define LEFT_CTRL_SC	29
#define Q_SC		16
#define S_SC		31

/* need to pass something becuase we use a shared irq */
#define KEYBOARD_DEV_ID	0x4b455942

static unsigned ipod_hw_ver;

static void keyboard_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	static int prev_scroll = -1;
	unsigned char source, state;
	static int was_hold = 0;

	/* we need some delay for g3, cause hold generates several interrupts,
	 * some of them delayed
	 */
	if (ipod_hw_ver == 0x3) {
		udelay(250);
	}

	/* get source of interupts */
	source = inb(0xcf000040);
	if ( source == 0 ) {
		return; 	/* not for us */
	}

	/* get current keypad status */
	state = inb(0xcf000030);
	outb(~state, 0xcf000060);

	if (ipod_hw_ver == 0x3) {
		if (was_hold && source == 0x40 && state == 0xbf) {
			/* ack any active interrupts */
			outb(source, 0xcf000070);
			return;
		}
		was_hold = 0;
	}

#ifdef CONFIG_VT
	kbd_pt_regs = regs;

	if ( source & 0x20 ) {
		if ((ipod_hw_ver == 0x3 && (state & 0x20) == 0 ) || 
				(state & 0x20)) {
			/* CTRL-S down */
			handle_scancode(LEFT_CTRL_SC, 1);
			handle_scancode(S_SC, 1);

			/* CTRL-S up */
			handle_scancode(S_SC, 0);
			handle_scancode(LEFT_CTRL_SC, 0);
		}
		else {
			/* CTRL-Q down */
			handle_scancode(LEFT_CTRL_SC, 1);
			handle_scancode(Q_SC, 1);

			/* CTRL-Q up */
			handle_scancode(Q_SC, 0);
			handle_scancode(LEFT_CTRL_SC, 0);
			if (ipod_hw_ver == 0x3) {
				was_hold = 1;
			}
		}
		/* hold switch on 3g causes all outputs to go low */
		/* we shouldn't interpret these as key presses */
		if (ipod_hw_ver == 0x3) {
			goto done;
		}
	}

	if ( source & 0x1 ) {
		if ( state & 0x1 ) {
#if defined(DO_SCROLLBACK)
			scrollfront(0);
#else
			handle_scancode(RIGHT_SC, 0);
#endif
		}
#if !defined(DO_SCROLLBACK)
		else {
			handle_scancode(RIGHT_SC, 1);
		}
#endif
	}
	if ( source & 0x2 ) {
		if ( state & 0x2 ) {
			handle_scancode(ACTION_SC, 0);
		}
		else {
			handle_scancode(ACTION_SC, 1);
		}
	}

	if ( source & 0x4 ) {
		if ( state & 0x4 ) {
#if defined(DO_CONTRAST)
			contrast_down();
#else
			handle_scancode(DOWN_SC, 0);
#endif
		}
#if !defined(DO_CONTRAST)
		else {
			handle_scancode(DOWN_SC, 1);
		}
#endif
	}
	if ( source & 0x8 ) {
		if ( state & 0x8 ) {
#if defined(DO_SCROLLBACK)
			scrollback(0);
#else
			handle_scancode(LEFT_SC, 0);
#endif
		}
#if !defined(DO_SCROLLBACK)
		else {
			handle_scancode(LEFT_SC, 1);
		}
#endif
	}
	if ( source & 0x10 ) {
		if ( state & 0x10 ) {
#if defined(DO_CONTRAST)
			contrast_up();
#else
			handle_scancode(UP_SC, 0);
#endif
		}
#if !defined(DO_CONTRAST)
		else {
			handle_scancode(UP_SC, 1);
		}
#endif
	}

	if ( source & 0xc0 ) {
		static int scroll_state[4][4] = {
			{0, 1, -1, 0},
			{-1, 0, 0, 1},
			{1, 0, 0, -1},
			{0, -1, 1, 0}
		};
		unsigned now_scroll = (state & 0xc0) >> 6;
						
		if ( prev_scroll == -1 ) {
			prev_scroll = now_scroll;
		}
		else {
			switch (scroll_state[prev_scroll][now_scroll]) {
			case 1:
				/* 'l' keypress */
				handle_scancode(L_SC, 1);
				handle_scancode(L_SC, 0);
				break;
			case -1:
				/* 'r' keypress */
				handle_scancode(R_SC, 1);
				handle_scancode(R_SC, 0);
				break;
			default:
				/* only happens if we get out of sync */
			}
		}

		prev_scroll = now_scroll;
	}
done:

	tasklet_schedule(&keyboard_tasklet);
#endif /* CONFIG_VT */

	/* ack any active interrupts */
	outb(source, 0xcf000070);
}

void __init ipodkb_init_hw(void)
{
	outb(~inb(0xcf000030), 0xcf000060);
	outb(inb(0xcf000040), 0xcf000070);

	outb(inb(0xcf000004) | 0x1, 0xcf000004);
	outb(inb(0xcf000014) | 0x1, 0xcf000014);
	outb(inb(0xcf000024) | 0x1, 0xcf000024);

	if ( request_irq(GPIO_IRQ, keyboard_interrupt, SA_SHIRQ, "keyboard", KEYBOARD_DEV_ID) ) {
		printk("ipodkb: IRQ %d failed\n", GPIO_IRQ);
	}

	outb(0xff, 0xcf000050);

	/* get our hardware type */
	ipod_hw_ver = ipod_get_hw_version() >> 16;
}

