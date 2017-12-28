/****************************************************************************/

/*
 *	mcfqspi.c - Master QSPI controller for the ColdFire processors
 *
 *	(C) Copyright 2005, Intec Automation,
 *			    Mike Lavender (mike@steroidmicros)
 *

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */

#ifndef MCFQSPI_H_
#define MCFQSPI_H_

#define QSPI_CS_INIT     0x01
#define QSPI_CS_ASSERT	 0x02
#define QSPI_CS_DROP	 0x04

struct coldfire_spi_master {
	u16 bus_num;
	u16 num_chipselect;
	u8  irq_source;
	u32 irq_vector;
	u32 irq_mask;
	u8  irq_lp;
	u8  par_val;
	void (*cs_control)(u8 cs, u8 command);
};


struct coldfire_spi_chip {
	u8 mode;
	u8 bits_per_word;
	u8 del_cs_to_clk;
	u8 del_after_trans;
	u16 void_write_data;
};

#endif /*MCFQSPI_H_*/
