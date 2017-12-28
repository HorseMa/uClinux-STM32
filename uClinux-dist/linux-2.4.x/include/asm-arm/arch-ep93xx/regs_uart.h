/*
 *  File:   linux/include/asm-arm/arch-ep93xx/regs_uart.h
 *
 *  Copyright (C) 2003 Cirrus Logic, Inc
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
 */

#ifndef _REGS_UART_H_
#define _REGS_UART_H_

//-----------------------------------------------------------------------------
// Bits in UARTRSR
//-----------------------------------------------------------------------------
#define UARTRSR_FE              0x00000001      // Framing error
#define UARTRSR_PE              0x00000002      // Parity error
#define UARTRSR_BE              0x00000004      // Break error
#define UARTRSR_OE              0x00000008      // Overrun error

//-----------------------------------------------------------------------------
// Bits in UARTCR - UART1CR, UART2CR, or UART3CR
//-----------------------------------------------------------------------------
#define UARTLCR_H_BRK           0x00000001
#define UARTLCR_H_PEN           0x00000002
#define UARTLCR_H_EPS           0x00000004
#define UARTLCR_H_STP2          0x00000008
#define UARTLCR_H_FEN           0x00000010
#define UARTLCR_H_WLEN          0x00000060
#define UARTLCR_H_WLEN_8_DATA   0x00000060
#define UARTLCR_H_WLEN_7_DATA   0x00000040
#define UARTLCR_H_WLEN_6_DATA   0x00000020
#define UARTLCR_H_WLEN_5_DATA   0x00000000

//-----------------------------------------------------------------------------
// Bits in UARTFR - UART1FR, UART2FR, or UART3FR
//-----------------------------------------------------------------------------
#define UARTFR_RSR_ERRORS       0x0000000F
#define UARTFR_CTS              0x00000001
#define UARTFR_DSR              0x00000002
#define UARTFR_DCD              0x00000004
#define UARTFR_BUSY             0x00000008
#define UARTFR_RXFE             0x00000010
#define UARTFR_TXFF             0x00000020
#define UARTFR_RXFF             0x00000040
#define UARTFR_TXFE             0x00000080

//-----------------------------------------------------------------------------
// Bits in UARTIIR
//-----------------------------------------------------------------------------
#define UARTIIR_MIS             0x00000001
#define UARTIIR_RIS             0x00000002
#define UARTIIR_TIS             0x00000004
#define UARTIIR_RTIS            0x00000008

//-----------------------------------------------------------------------------
// Bits in UARTCR
//-----------------------------------------------------------------------------
#define UARTCR_UARTE            0x00000001
#define UARTCR_MSIE             0x00000008
#define UARTCR_RIE              0x00000010
#define UARTCR_TIE              0x00000020
#define UARTCR_RTIE             0x00000040
#define UARTCR_LBE              0x00000080

//=============================================================================
//=============================================================================
#endif /* _REGS_UART_H_ */
