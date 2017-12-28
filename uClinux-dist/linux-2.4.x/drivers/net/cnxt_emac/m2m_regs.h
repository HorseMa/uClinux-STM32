/****************************************************************************
*
*	Name:			m2m_regs.h
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

#ifndef __M2M_REGS_H
#define __M2M_REGS_H


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

/**************************************************************
 * M2M regs, bit masks & reg-access macros
 */

/* 
 * Reg addresses
 */

/* M2M DMA port reg. It's 64 bit reg. Lower 32 are software writable */
#define M2M_DMA_64BITS	  ((volatile BYTE*)0x00350000) 

#if (BE_OFST==0) /* Little endian */
#define M2M_DMA	           M2M_DMA_64BITS /* lower 32bits start here */
#else /* big endian */
#define M2M_DMA	          (M2M_DMA_64BITS + sizeof(DWORD)) /* Next ULONG */
#endif /* BE_OFST == 0 */

/* M2M CNT reg*/
#define M2M_CNT	      ((volatile BYTE*)0x00350004)

/* 
 * M2M_CNT masks 
 */

#define M2M_BS_MASK       ((ULONG)(0x00600000)) /* [22:21] RW */

#define M2M_BS0_MASK      ((ULONG)(0x00000000)) /* bs=0 */
#define M2M_BS1_MASK      ((ULONG)(0x00200000)) /* bs=1 */
#define M2M_BS2_MASK      ((ULONG)(0x00400000)) /* bs=2 */
#define M2M_BS3_MASK      ((ULONG)(0x00600000)) /* bs=3 */

#define M2M_DO_MASK       ((ULONG)(0x00100000)) /* Dst only. [20] RW */
#define M2M_SD_MASK       ((ULONG)(0x00000000)) /* src & dst */

#define M2M_COUNT_MASK    ((ULONG)(0x000fffff)) /* [19:0] RW */

/* 
 * M2M macros that form the val that may be written to M2M_CNT reg. 
 */

#define M2M_BS0_DO_CNT(num_of_qwords) \
        (M2M_BS0_MASK|M2M_DO_MASK|(M2M_COUNT_MASK & (ULONG)(num_of_qwords)))

#define M2M_BS0_SD_CNT(num_of_qwords) \
        (M2M_BS0_MASK|M2M_SD_MASK|(M2M_COUNT_MASK & (ULONG)(num_of_qwords)))


#endif /*__M2M_REGS_H */
