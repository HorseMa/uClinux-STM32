/*
 * include/asm-armnommu/arch-isl3893/intctl.h
 */
/*  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __ASM_ARCH_INTCTL_H
#define __ASM_ARCH_INTCTL_H

/* Class values */
#define	IntCtlINTCTL_TICKINT		(0x1)
#define	IntCtlINTCTL_SOFINT		(0x2)
#define	IntCtlINTCTL_SRTCEV		(0x4)
#define	IntCtlINTCTL_PCIRESETINT	(0x8)
#define	IntCtlINTCTL_GP1FIQINT		(0x10)
#define	IntCtlINTCTL_GP2FIQINT		(0x20)
#define	IntCtlINTCTL_PCIINT		(0x40)
#define	IntCtlINTCTL_BPINT		(0x80)
#define	IntCtlINTCTL_UARTINT		(0x100)
#define	IntCtlINTCTL_HOSTTXDMA		(0x200)
#define	IntCtlINTCTL_HOSTRXDMA		(0x400)
#define	IntCtlINTCTL_GP1IRQINT		(0x800)
#define	IntCtlINTCTL_GP2IRQINT		(0x1000)
#define	IntCtlINTCTL_BPTXDMA		(0x2000)
#define	IntCtlINTCTL_BPRXDMA		(0x4000)
#define	IntCtlINTCTL_M2MTXDMA		(0x8000)
#define	IntCtlINTCTL_M2MRXDMA		(0x10000)
#define	IntCtlINTCTL_TIM0INT		(0x20000)
#define	IntCtlINTCTL_TIM1INT		(0x40000)
#define	IntCtlINTCTL_TIM2INT		(0x80000)
#define	IntCtlINTCTL_WEPINT		(0x100000)
#define	IntCtlINTCTL_COMMRX		(0x200000)
#define	IntCtlINTCTL_COMMTX		(0x400000)
#define	IntCtlINTCTL_ETH1		(0x800000)
#define	IntCtlINTCTL_ETH2		(0x1000000)
#define	IntCtlINTCTL_GP3FIQINT		(0x2000000)
#define	IntCtlINTCTL_GP3IRQINT		(0x4000000)

/* Register values */

/* - Status */
#define	IntCtlStatus			(0x0)
#define	IntCtlStatusMask		(0x7ffffff)
#define	IntCtlStatusAccessType		(RO)
#define	IntCtlStatusInitialValue	(0x0)
#define	IntCtlStatusTestMask		(0x7ffffff)

/* - RawStatus */
#define	IntCtlRawStatus			(0x4)
#define	IntCtlRawStatusMask		(0x1ffffff)
#define	IntCtlRawStatusTestMask		(0x79fffff)
#define	IntCtlRawStatusInitialValue	(0x8)
#define	IntCtlRawStatusAccessType	(WO)

/* - EnableSet */
#define	IntCtlEnableSet			(0x8)
#define	IntCtlEnableSetMask		(0x7ffffff)
#define	IntCtlEnableSetTestMask		(0x7ffffff)
#define	IntCtlEnableSetAccessType	(NO_TEST)
#define	IntCtlEnableSetInitialValue	(0x0)

/* - EnableClear */
#define	IntCtlEnableClear		(0xc)
#define	IntCtlEnableClearMask		(0x7ffffff)
#define	IntCtlEnableClearAccessType	(WO)
#define	IntCtlEnableClearInitialValue	(0x0)
#define	IntCtlEnableClearTestMask	(0x7ffffff)

/* - Soft */
#define	IntCtlSoft			(0x10)
#define	IntCtlSoftGenerate		(0x2)
#define	IntCtlSoftMask			(0x2)
#define	IntCtlSoftAccessType		(WO)
#define	IntCtlSoftInitialValue		(0x0)
#define	IntCtlSoftTestMask		(0x2)

/* - TestInput */
#define	IntCtlTestInput			(0x14)
#define	IntCtlTestInputMask		(0x7fffffd)
#define	IntCtlTestInputInitialValue	(0x0)
#define	IntCtlTestInputTestMask		(0x7fffffd)
#define	IntCtlTestInputAccessType	(RW)

/* - TestSel */
#define	IntCtlTestSel			(0x18)
#define	IntCtlTestSelMask		(0x1)
#define	IntCtlTestSelInitialValue	(0x0)
#define	IntCtlTestSelTestMask		(0x1)
#define	IntCtlTestSelAccessType		(RW)

/* Instance values */
#define aIRQStatus			 0
#define aIRQRawStatus		 0x4
#define aIRQEnableSet		 0x8
#define aIRQEnableClear		 0xc
#define aIRQSoft			 0x10
#define aIRQTestInput		 0x14
#define aIRQTestSel			 0x18
#define aFIQStatus			 0x100
#define aFIQRawStatus		 0x104
#define aFIQEnableSet		 0x108
#define aFIQEnableClear		 0x10c
#define aFIQSoft			 0x110
#define aFIQTestInput		 0x114
#define aFIQTestSel			 0x118

/* C struct view */

#ifndef __ASSEMBLER__

typedef struct s_IntCtl {
 unsigned Status; /* @0 */
 unsigned RawStatus; /* @4 */
 unsigned EnableSet; /* @8 */
 unsigned EnableClear; /* @12 */
 unsigned Soft; /* @16 */
 unsigned TestInput; /* @20 */
 unsigned TestSel; /* @24 */
} s_IntCtl;

#endif /* !__ASSEMBLER__ */

#define uIRQ ((volatile struct s_IntCtl *) 0xc0000000)
#define uFIQ ((volatile struct s_IntCtl *) 0xc0000100)

#endif /* __ASM_ARCH_INTCTL_H */
