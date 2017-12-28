/****************************************************************************
*
*	Name:			cnxtEmac_hw.h
*
*	Description:	
*
*	Copyright:		(c) 2001,2002 Conexant Systems Inc.
*					Personal Computing Division
*****************************************************************************

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA
*
****************************************************************************
*  $Author:   davidsdj  $
*  $Revision:   1.0  $
*  $Modtime:   Mar 19 2003 12:25:20  $
****************************************************************************/

#ifndef __CNXT_EMAC_HW_H
#define __CNXT_EMAC_HW_H

/*******************************************************************************
 *
 * This file simply INCLUDES files. 
 * Each included file:
 * 1) corresponds to a sub-device of the CX821xx device. 
 * 2) It defines registers, bit masks & reg-access macros for that sub-device.
 *
 */

/*******************************************************************************
 * Emac: Reg base addr & Reg offset.
 * The reg offset is same for all the sub-devices (like DMAC, EMAC & 
 * Interrupt controller).
 */


#ifndef REG_BASE_ADDR
#define REG_BASE_ADDR	 ((BYTE*)(0x00300000)) /* double word aligned */
#endif

#ifndef BE_OFST
#define BE_OFST		 0   /* 0 for little endian, 7 for big  */
#endif

#ifndef REG_OFFSET
#define REG_OFFSET	 (BE_OFST + 0x04) /* double word aligned */
#endif


/*
 * Reg Access Macros, using Register-ADDRESS ( and not the reg number)
 */

#ifndef REG_WRITE
#define REG_WRITE(regAddr,val)  ( (*(volatile ULONG*)(regAddr)) = (val) )
#endif /* REG_WRITE */

/*******************************************************************************
 * Emac sub-devices: registers, bitmasks & reg access macros.
 */


/* M2M registers & access-macros*/
#include "m2m_regs.h"

/* Interrupt controller registers (eg int msk, intr stat) & access-macros */
#include "intr_regs.h"

/* EMAC registers & access-macros */
#include "emac_regs.h"

/* EMAC descriptors, status & access-macros */
#include "emac_desc.h"


#endif /* __CNXT_EMAC_HW_H */
