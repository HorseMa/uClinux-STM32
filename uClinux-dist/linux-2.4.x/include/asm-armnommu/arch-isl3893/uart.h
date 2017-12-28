/*
 * include/asm-armnommu/arch-isl3893/uart.h
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

#ifndef __ASM_ARCH_UART_H
#define __ASM_ARCH_UART_H

/* Class values */

/* Register values */

/* - Register0 */
#define	UARTRegister0			(0x0)
#define	UARTRegister0AccessType		(NO_TEST)
#define	UARTRegister0InitialValue	(0x0)
#define	UARTRegister0Mask		(0xffffffff)
#define	UARTRegister0TestMask		(0xffffffff)

/* - InterruptEnable */
#define	UARTInterruptEnable		(0x4)
#define	UARTInterruptEnableMask		(0xf)
#define	UARTInterruptEnableDSSI		(0x8)
#define	UARTInterruptEnableLSI		(0x4)
#define	UARTInterruptEnableTBEI		(0x2)
#define	UARTInterruptEnableRBFI		(0x1)
#define	UARTInterruptEnableMask		(0xf)
#define	UARTInterruptEnableAccessType	(RW)
#define	UARTInterruptEnableInitialValue	(0x0)
#define	UARTInterruptEnableTestMask	(0xf)

/* - InterrupID */
#define	UARTInterrupID			(0x8)
#define	UARTInterrupIDMask		(0x7)
#define	UARTInterrupIDIIR		(0x7)
#define	UARTInterrupIDIIRShift		(0x0)
#define	UARTInterrupIDMask		(0x7)
#define	UARTInterrupIDNoInt		(0x1)
#define	UARTInterrupIDDSSI		(0x0)
#define	UARTInterrupIDTBEI		(0x2)
#define	UARTInterrupIDRBFI		(0x4)
#define	UARTInterrupIDLSI		(0x6)
#define	UARTInterrupIDInitialValue	(0x1)
#define	UARTInterrupIDAccessType	(RO)
#define	UARTInterrupIDTestMask		(0x7)

/* - LineControl */
#define	UARTLineControl			(0xc)
#define	UARTLineControlMask		(0xff)
#define	UARTLineControlDLAB		(0x80)
#define	UARTLineControlBREAK		(0x40)
#define	UARTLineControlSTICK		(0x20)
#define	UARTLineControlEPS		(0x10)
#define	UARTLineControlPEN		(0x8)
#define	UARTLineControlSTOP		(0x4)
#define	UARTLineControlWLS		(0x3)
#define	UARTLineControlWLSShift		(0x0)
#define	UARTLineControlMask		(0xff)
#define	UARTLineControlAccessType	(RW)
#define	UARTLineControlInitialValue	(0x0)
#define	UARTLineControlTestMask		(0xff)

/* - ModemControl */
#define	UARTModemControl		(0x10)
#define	UARTModemControlMask		(0x1b)
#define	UARTModemControlLB		(0x10)
#define	UARTModemControlOUT2		(0x8)
#define	UARTModemControlRTS		(0x2)
#define	UARTModemControlDTR		(0x1)
#define	UARTModemControlMask		(0x1b)
#define	UARTModemControlAccessType	(RW)
#define	UARTModemControlInitialValue	(0x0)
#define	UARTModemControlTestMask	(0x1b)

/* - LineStatus */
#define	UARTLineStatus			(0x14)
#define	UARTLineStatusMask		(0x7f)
#define	UARTLineStatusTEMT		(0x40)
#define	UARTLineStatusTHRE		(0x20)
#define	UARTLineStatusBI		(0x10)
#define	UARTLineStatusFE		(0x8)
#define	UARTLineStatusPE		(0x4)
#define	UARTLineStatusOE		(0x2)
#define	UARTLineStatusDR		(0x1)
#define	UARTLineStatusMask		(0x7f)
#define	UARTLineStatusInitialValue	(0x60)
#define	UARTLineStatusAccessType	(RO)
#define	UARTLineStatusTestMask		(0x7f)

/* - ModemStatus */
#define	UARTModemStatus			(0x18)
#define	UARTModemStatusMask		(0xbb)
#define	UARTModemStatusTestMask		(0xb0)
#define	UARTModemStatusInitialValue	(0x0)
#define	UARTModemStatusDCD		(0x80)
#define	UARTModemStatusDSR		(0x20)
#define	UARTModemStatusCTS		(0x10)
#define	UARTModemStatusDDCD		(0x8)
#define	UARTModemStatusDDSR		(0x2)
#define	UARTModemStatusDCTS		(0x1)
#define	UARTModemStatusMask		(0xbb)
#define	UARTModemStatusAccessType	(NO_TEST)

/* - ScratchPad */
#define	UARTScratchPad			(0x1c)
#define	UARTScratchPadMask		(0xff)
#define	UARTScratchPadSRCPAD		(0xff)
#define	UARTScratchPadSRCPADShift	(0x0)
#define	UARTScratchPadMask		(0xff)
#define	UARTScratchPadAccessType	(RW)
#define	UARTScratchPadInitialValue	(0x0)
#define	UARTScratchPadTestMask		(0xff)

/* Instance values */
#define aUARTRegister0		 0x500
#define aUARTInterruptEnable		 0x504
#define aUARTInterrupID		 0x508
#define aUARTLineControl		 0x50c
#define aUARTModemControl		 0x510
#define aUARTLineStatus		 0x514
#define aUARTModemStatus		 0x518
#define aUARTScratchPad		 0x51c

/* C struct view */

#ifndef __ASSEMBLER__

typedef struct s_UART {
 unsigned Register0; /* @0 */
 unsigned InterruptEnable; /* @4 */
 unsigned InterrupID; /* @8 */
 unsigned LineControl; /* @12 */
 unsigned ModemControl; /* @16 */
 unsigned LineStatus; /* @20 */
 unsigned ModemStatus; /* @24 */
 unsigned ScratchPad; /* @28 */
} s_UART;

#endif /* !__ASSEMBLER__ */

#define uUART ((volatile struct s_UART *) 0xc0000500)

#endif /* __ASM_ARCH_UART_H */
