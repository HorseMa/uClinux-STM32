/*****************************************************************************
 *
 *  linux/include/asm-arm/arch-ep93xx/regs_ide.h
 *
 *  Register definitions for the ep93xx ide registers.
 *
 *  Copyright (C) 2003 Cirrus Logic
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
#ifndef _REGS_IDE_H_
#define _REGS_IDE_H_

/*****************************************************************************
 *
 *  Bit definitions for use with assembly code for the ide control register.
 *
 ****************************************************************************/
#define IDECtrl_CS0n			0x00000001
#define IDECtrl_CS1n			0x00000002
#define IDECtrl_DA_MASK			0x0000001c
#define IDECtrl_DA_SHIFT		2
#define IDECtrl_DIORn			0x00000020
#define IDECtrl_DIOWn			0x00000040
#define IDECtrl_DASPn			0x00000080
#define IDECtrl_DMARQ			0x00000100
#define IDECtrl_INTRQ			0x00000200
#define IDECtrl_IORDY			0x00000400

#define IDECfg_IDEEN			0x00000001
#define IDECfg_PIO			0x00000002
#define IDECfg_MDMA			0x00000004
#define IDECfg_UDMA			0x00000008
#define IDECfg_MODE_MASK		0x000000f0
#define IDECfg_MODE_SHIFT		4
#define IDECfg_WST_MASK			0x00000300
#define IDECfg_WST_SHIFT		8

#define IDEMDMAOp_MEN			0x00000001
#define IDEMDMAOp_RWOP			0x00000002

#define IDEUDMAOp_UEN			0x00000001
#define IDEUDMAOp_RWOP			0x00000002

#define IDEUDMASts_CS0n			0x00000001
#define IDEUDMASts_CS1n			0x00000002
#define IDEUDMASts_DA_MASK		0x0000001c
#define IDEUDMASts_DA_SHIFT		2
#define IDEUDMASts_HSHD			0x00000020
#define IDEUDMASts_STOP			0x00000040
#define IDEUDMASts_DM			0x00000080
#define IDEUDMASts_DDOE			0x00000100
#define IDEUDMASts_DMARQ		0x00000200
#define IDEUDMASts_DSDD			0x00000400
#define IDEUDMASts_DMAide		0x00010000
#define IDEUDMASts_INTide		0x00020000
#define IDEUDMASts_SBUSY		0x00040000
#define IDEUDMASts_NDO			0x01000000
#define IDEUDMASts_NDI			0x02000000
#define IDEUDMASts_N4X			0x04000000

#define IDEUDMADebug_RWOE		0x00000001
#define IDEUDMADebug_RWPTR		0x00000002
#define IDEUDMADebug_RWDR		0x00000004
#define IDEUDMADebug_RROE		0x00000008
#define IDEUDMADebug_RRPTR		0x00000010
#define IDEUDMADebug_RRDR		0x00000020

#define IDEUDMAWrBufSts_HPTR_MASK	0x0000000f
#define IDEUDMAWrBufSts_HPTR_SHIFT	0
#define IDEUDMAWrBufSts_TPTR_MASK	0x000000f0
#define IDEUDMAWrBufSts_TPTR_SHIFT	4
#define IDEUDMAWrBufSts_EMPTY		0x00000100
#define IDEUDMAWrBufSts_HOM		0x00000200
#define IDEUDMAWrBufSts_NFULL		0x00000400
#define IDEUDMAWrBufSts_FULL		0x00000800
#define IDEUDMAWrBufSts_CRC_MASK	0xffff0000
#define IDEUDMAWrBufSts_CRC_SHIFT	16

#define IDEUDMARdBufSts_HPTR_MASK	0x0000000f
#define IDEUDMARdBufSts_HPTR_SHIFT	0
#define IDEUDMARdBufSts_TPTR_MASK	0x000000f0
#define IDEUDMARdBufSts_TPTR_SHIFT	4
#define IDEUDMARdBufSts_EMPTY		0x00000100
#define IDEUDMARdBufSts_HOM		0x00000200
#define IDEUDMARdBufSts_NFULL		0x00000400
#define IDEUDMARdBufSts_FULL		0x00000800
#define IDEUDMARdBufSts_CRC_MASK	0xffff0000
#define IDEUDMARdBufSts_CRC_SHIFT	16

#endif /* _REGS_IDE_H_ */
