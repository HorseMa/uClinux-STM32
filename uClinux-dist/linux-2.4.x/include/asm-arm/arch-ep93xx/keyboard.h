/*
 *  linux/include/asm-arm/arch-integrator/keyboard.h
 *
 *  Copyright (C) 2000-2001 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Keyboard driver definitions for the Integrator architecture
 */
#include <asm/irq.h>

// Scancode mapping is as follows:
//
//	Scancode = keycode if key down event
//	Scancode = keycode|KBUP if key up event
//
//	Valid keycodes are 1..0x7F and are defined by the table
//	in keymap_dave.map.  Keycode 0 is reserved to mean invalid.
//
//	Keycodes are computed from scanned key matrix as follows:

#define	KEYCODE( row, col)	( ((row)<<4) + (col) + 1)
#define KBUP	0x80
#define NR_SCANCODES 128	//TBD used?


#ifdef CONFIG_EP93XX_KBD_SCANNED
void __init ep93xx_scan_kbd_hw_init(void);
#define kbd_init_hw()			ep93xx_scan_kbd_hw_init()			
#elif defined CONFIG_EP93XX_KBD_SPI
void __init EP93XXSpiKbdInit(void);
#define kbd_init_hw()			EP93XXSpiKbdInit()
#elif defined CONFIG_EP93XX_KBD_USB
void __init EP93XXUSBKbdInit(void);
#define kbd_init_hw()			EP93XXUSBKbdInit()
#endif

#define kbd_disable_irq()		disable_irq( IRQ_KEY)
#define kbd_enable_irq()		enable_irq( IRQ_KEY)
