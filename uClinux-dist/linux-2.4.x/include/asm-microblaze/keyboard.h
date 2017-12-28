/*
 *  linux/include/asm-microblaze/keyboard.h
 *  Created 04 Dec 2001 by Khaled Hassounah <khassounah@mediumware.net>
 *  This file contains the Dragonball architecture specific keyboard definitions
 */

#ifndef _MICROBLAZE_KEYBOARD_H
#define _MICROBLAZE_KEYBOARD_H

#include <linux/config.h>

#ifdef CONFIG_XILINX_PS2_DUAL_REF_0_BASEADDR

unsigned char x8042_read_input(void);
unsigned char x8042_read_status(void);
void x8042_write_output(unsigned char val);
void x8042_write_command(unsigned char val);

#define kbd_setkeycode(x...)        (-ENOSYS)
#define kbd_getkeycode(x...)        (-ENOSYS)
#define kbd_translate(sc_,kc_,rm_)	((*(kc_)=(sc_)),1)
#define kbd_unexpected_up(x...)     (1)
#define kbd_leds(x...)		        do {;} while (0)
#define kbd_init_hw                 pckbd_init_hw
#define kbd_enable_irq(x...)	    do {;} while (0)
#define kbd_disable_irq(x...)	    do {;} while (0)
#define kbd_request_region(x...)    do {;} while (0)
#define kbd_read_input()            x8042_read_input()
#define kbd_read_status()           x8042_read_status()
#define kbd_write_output(val)       x8042_write_output(val)
#define kbd_write_command(val)      x8042_write_command(val)
#define kbd_request_irq(handler) \
    request_irq(CONFIG_XILINX_PS2_DUAL_REF_0_SYS_INTR2_IRQ, \
    handler, 0, "keyboard", NULL)
#define aux_request_irq(hand, dev_id) \
    request_irq(CONFIG_XILINX_PS2_DUAL_REF_0_SYS_INTR1_IRQ, \
    hand, SA_SHIRQ, "PS/2 Mouse", dev_id)
#define aux_free_irq(dev_id) \
    free_irq(CONFIG_XILINX_PS2_DUAL_REF_0_SYS_INTR1_IRQ, dev_id)

#else

/* dummy i.e. no real keyboard */
#define kbd_setkeycode(x...)	(-ENOSYS)
#define kbd_getkeycode(x...)	(-ENOSYS)
#define kbd_translate(x...)	(0)
#define kbd_unexpected_up(x...)	(1)
#define kbd_leds(x...)		do {;} while (0)
#define kbd_init_hw(x...)	do {;} while (0)
#define kbd_enable_irq(x...)	do {;} while (0)
#define kbd_disable_irq(x...)	do {;} while (0)
#define kbd_request_irq(x...)   do {;} while (0)
#define kbd_request_region(x...)   do {;} while (0)
#define kbd_read_status(x...)   (0)
#define kbd_write_output(x...)   do {;} while (0)
#define kbd_write_command(x...)   do {;} while (0)

#endif


/* needed if MAGIC_SYSRQ is enabled for serial console */
#ifndef SYSRQ_KEY
#define SYSRQ_KEY		((unsigned char)(-1))
#define kbd_sysrq_xlate         ((unsigned char *)NULL)
#endif


#endif  /* _MICROBLAZE_KEYBOARD_H */

