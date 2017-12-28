/*******************************************************************************
 *
 * Copyright (c) 2003 Cypress Semiconductor
 *
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
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef CY7C67200_300_COMMON_H
#define CY7C67200_300_COMMON_H

#define TRUE 		1
#define FALSE 		0
#define ERROR 		-1
#define SUCCESS		0

#define SIE1		0
#define SIE2		1

#define PORT0		0
#define PORT1		1
#define PORT2		2
#define PORT3		3

#define DE1			1
#define DE2			2
#define DE3			4
#define DE4			8

#define HOST_ROLE    	    0
#define PERIPHERAL_ROLE     1
#define HOST_INACTIVE_ROLE  2


typedef struct cy_priv {
	int cy_addr;		/* cy7c67200_300 Base addr */
	int cy_irq;			/* cy7c67200_300 IRQ number */
	int system_mode[2];    /* system mode on for each sie: host or peripheral */
	unsigned short cy_buf_addr;	/* buffer address within CY7c67200 */
	unsigned short cy_buf_size;	/* buffer size in the cy7c67200 */ 
	void * otg;		/* OTG data structure */
	void * hci; 		/* Host controller driver private data */
	void * lcd_priv;	/* LCD private data */
	void * pcdi;		/* Peripheral controller driver private data */
} cy_priv_t;

#endif

