/*
 *  linux/include/asm-arm/keyboard.h
 *
 *  Copyright (C) 1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Keyboard driver definitions for ARM
 */
#ifndef __ASM_ARM_KEYBOARD_H
#define __ASM_ARM_KEYBOARD_H

#include <linux/kd.h>
#include <linux/pm.h>

/*
 * We provide a unified keyboard interface when in VC_MEDIUMRAW
 * mode.  This means that all keycodes must be common between
 * all supported keyboards.  This unfortunately puts us at odds
 * with the PC keyboard interface chip... but we can't do anything
 * about that now.
 */
#ifdef __KERNEL__

#ifdef	CONFIG_ARCH_EDB7312

#include <asm/arch/irqs.h>

#undef	mdelay
#define	mdelay(x)	udelay(2000)

#define kbd_read_input() __raw_readb(EDB7312_VIRT_8042 + 0)
#define kbd_read_status() __raw_readb(EDB7312_VIRT_8042 + 1)
#define kbd_write_output(val) __raw_writeb(val, EDB7312_VIRT_8042 + 0)
#define kbd_write_command(val) __raw_writeb(val, EDB7312_VIRT_8042 + 1)

#define	kbd_request_region()
#define	kbd_request_irq(handler) request_irq(IRQ_EINT1, handler, SA_SHIRQ, \
                                             "keyboard", "kbd"); \
				 edb7312_enable_eint1()
#define	kbd_disable_irq()
#define	kbd_enable_irq()
#define	aux_request_irq(a, b)	0000
#define	aux_free_irq(a)

extern unsigned char pckbd_sysrq_xlate[];

#define kbd_setkeycode          pckbd_setkeycode
#define kbd_getkeycode          pckbd_getkeycode
#define kbd_translate           pckbd_translate
#define kbd_unexpected_up       pckbd_unexpected_up
#define kbd_leds                pckbd_leds
#define kbd_init_hw             pckbd_init_hw
#define SYSRQ_KEY		0x63
#define kbd_sysrq_xlate         pckbd_sysrq_xlate

#else

extern int  (*k_setkeycode)(unsigned int, unsigned int);
extern int  (*k_getkeycode)(unsigned int);
extern int  (*k_translate)(unsigned char, unsigned char *, char);
extern char (*k_unexpected_up)(unsigned char);
extern void (*k_leds)(unsigned char);


static inline int kbd_setkeycode(unsigned int sc, unsigned int kc)
{
	int ret = -EINVAL;

	if (k_setkeycode)
		ret = k_setkeycode(sc, kc);

	return ret;
}

static inline int kbd_getkeycode(unsigned int sc)
{
	int ret = -EINVAL;

	if (k_getkeycode)
		ret = k_getkeycode(sc);

	return ret;
}

static inline void kbd_leds(unsigned char leds)
{
	if (k_leds)
		k_leds(leds);
}

extern int k_sysrq_key;
extern unsigned char *k_sysrq_xlate;

#define SYSRQ_KEY		k_sysrq_key
#define kbd_sysrq_xlate		k_sysrq_xlate
#define kbd_translate		k_translate
#define kbd_unexpected_up	k_unexpected_up

#include <asm/arch/keyboard.h>

#endif	/* CONFIG_ARCH_EDB7312 */

#endif /* __KERNEL__ */

#endif /* __ASM_ARM_KEYBOARD_H */
