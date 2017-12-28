/*****************************************************************************
 *  linux/include/asm-arm/arch-ep93xx/ide.h
 *
 *  IDE definitions for the EP93XX architecture
 *
 *  Copyright (c) 2003 Cirrus Logic, Inc., All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 ****************************************************************************/
#ifndef ASM_ARCH_IDE_H
#define ASM_ARCH_IDE_H

/*****************************************************************************
 *
 * Set up a hw structure for a specified data port, control port and IRQ.
 * This should follow whatever the default interface uses.
 *
 ****************************************************************************/
static __inline__ void
ide_init_hwif_ports(hw_regs_t *hw, int data_port, int ctrl_port, int *irq)
{
	ide_ioreg_t reg;
	int i;

	memset(hw, 0, sizeof(*hw));

	reg = (ide_ioreg_t)data_port;
	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += 1;
	}

	hw->io_ports[IDE_CONTROL_OFFSET] = (ide_ioreg_t)ctrl_port;

	if(irq)
		*irq = 0;
}

/*****************************************************************************
 *
 * This registers the standard ports for this architecture with the IDE
 * driver.
 *
 ****************************************************************************/
#ifdef CONFIG_BLK_DEV_IDE_EP93XX
extern void ep93xx_ide_init(void);
#define ide_init_default_hwifs ep93xx_ide_init
#else
#define ide_init_default_hwifs()
#endif

#endif /* ASM_ARCH_IDE_H */
