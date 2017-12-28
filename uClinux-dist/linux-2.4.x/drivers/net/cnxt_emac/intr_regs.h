/****************************************************************************
*
*	Name:			intr_regs.h
*
*	Description:	
*
*	Copyright:		(c) 1999-2002 Conexant Systems Inc.
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

#ifndef __INTR_REGS_H
#define __INTR_REGS_H

/*******************************************************************************
 * CX821xx: Reg base addr & Reg offset.
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

/*******************************************************************************
 *
 * Define Interrupt REGs and macros to access them.
 * To optimize REG accesses, redefine INTR_REG_READ and
 * INTR_REG_WRITE macros in a wrapper file.
 */

/*
 * ARM 1 interrupts ( indicated by INT1).
 */

#define INTR1_BASE_ADDR	    ((BYTE*)0x00350040)
#define INTR1_REG_OFFSET	REG_OFFSET	

/* Reg numbering w.r.t base */

#define INT1_MSK	    0
#define INT1_STAT	    1
#define INT1_MSTAT	    2 
#define INT1_SETSTAT	3 

/* Reg access macros */

#define INTR1_REG_ADDR(regNum)   (INTR1_BASE_ADDR + \
                                 ((regNum) * INTR1_REG_OFFSET))

#ifndef INTR1_REG_READ
#define INTR1_REG_READ(regNum)       (*((volatile ULONG*)INTR1_REG_ADDR((regNum))))
#endif /* INTR1_REG_READ */

#ifndef INTR1_REG_WRITE
#define INTR1_REG_WRITE(regNum,val)  (*((volatile ULONG*)INTR1_REG_ADDR((regNum))) = (val))
#endif /* INTR1_REG_WRITE */

#ifndef INTR1_REG_UPDATE
#define INTR1_REG_UPDATE(regNum,val) INTR1_REG_WRITE((regNum), \
                                     INTR1_REG_READ((regNum)) | (val))
#endif /* INTR1_REG_UPDATE */
    
#ifndef INTR1_REG_RESET /* writing 1 resets, 0 no effect */
#define INTR1_REG_RESET(regNum,val)  INTR1_REG_WRITE((regNum), \
                                     INTR1_REG_READ((regNum)) | (val))
#endif /* INTR1_REG_RESET */

/* INT1_STAT Bit masks */

#ifdef INT_ER_ERR
#undef  INT_ER_ERR
#endif
#ifdef INT_ET_ERR
#undef  INT_ET_ERR
#endif
#ifdef INT_DMA_ERR
#undef  INT_DMA_ERR
#endif
#ifdef INT_DMAn_MASK
#undef  INT_DMAn_MASK
#endif
#ifdef INT_DMA3
#undef  INT_DMA3
#endif
#ifdef INT_DMA4
#undef  INT_DMA4
#endif

/*#define INT_ER_ERR    ((ULONG)0x00040000)
#define INT_ET_ERR    ((ULONG)0x00020000)
#define INT_DMA_ERR   ((ULONG)0x00010000)
#define INT_DMAn_MASK ((ULONG)0x000ff000)  Int_DMA{n} 
#define INT_DMA3 ((ULONG)0x00002000)  Ethernet Tx 
#define INT_DMA4 ((ULONG)0x00001000)  Ethernet Rx */

#define INT_EMAC_TX     (INT_ET_ERR|INT_DMA3)
#define INT_EMAC_RX     (INT_ER_ERR|INT_DMA4)
#define INT_EMAC        (INT_EMAC_TX|INT_EMAC_RX)
#define INT_DMA_M2M 	((ULONG)0x00000100)      

/*#define MAX(x,y) ((x)>(y)?(x):(y))*/

#define INT_EMAC_LVL MAX(\
        MAX(INT_LVL_ET_ERR,INT_LVL_DMA3),\
        MAX(INT_LVL_ER_ERR,INT_LVL_DMA4))
/*
 * ARM 2 interrupts ( indicated by INT2).
 * Not listed here as they are not used by the driver.
 */

#define INTR2_BASE_ADDR	    ((BYTE*)0x00350080)


#endif /* __INTR_REGS_H */
