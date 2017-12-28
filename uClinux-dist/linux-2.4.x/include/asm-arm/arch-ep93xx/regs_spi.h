/*=======================================================================
 *
 *  FILE:       regs_spi.h
 *
 *  DESCRIPTION:    SSP Register Definition
 *
 *  Copyright Cirrus Logic, 2001-2003
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
 *=======================================================================
 */
 
#ifndef _REGS_SSP_H_
#define _REGS_SSP_H_

//-----------------------------------------------------------------------------
// Bits in SSPCR0
//-----------------------------------------------------------------------------
#define SSPCR0_DSS_MASK             0x0000000f
#define SSPCR0_DSS_4BIT             0x00000003
#define SSPCR0_DSS_5BIT             0x00000004
#define SSPCR0_DSS_6BIT             0x00000005
#define SSPCR0_DSS_7BIT             0x00000006
#define SSPCR0_DSS_8BIT             0x00000007
#define SSPCR0_DSS_9BIT             0x00000008
#define SSPCR0_DSS_10BIT            0x00000009
#define SSPCR0_DSS_11BIT            0x0000000a
#define SSPCR0_DSS_12BIT            0x0000000b
#define SSPCR0_DSS_13BIT            0x0000000c
#define SSPCR0_DSS_14BIT            0x0000000d
#define SSPCR0_DSS_15BIT            0x0000000e
#define SSPCR0_DSS_16BIT            0x0000000f

//-----------------------------------------------------------------------------
// Bits in SSPCR1
//-----------------------------------------------------------------------------
#define SSPC1_RIE                   0x00000001
#define SSPC1_TIE                   0x00000002
#define SSPC1_RORIE		            0x00000004
#define SSPC1_LBM                   0x00000008
#define SSPC1_SSE                   0x00000010
#define SSPC1_MS                    0x00000020
#define SSPC1_SOD                   0x00000040

#define SSPCR0_DSS_SHIFT            0
#define SSPCR0_FRF_MASK             0x00000030
#define SSPCR0_FRF_SHIFT            4
#define SSPCR0_FRF_MOTOROLA         (0 << SSPCR0_FRF_SHIFT)
#define SSPCR0_FRF_TI               (1 << SSPCR0_FRF_SHIFT)
#define SSPCR0_FRF_NI               (2 << SSPCR0_FRF_SHIFT)
#define SSPCR0_SPO                  0x00000040
#define SSPCR0_SPH                  0x00000080
#define SSPCR0_SCR_MASK             0x0000ff00
#define SSPCR0_SCR_SHIFT            8

//-----------------------------------------------------------------------------
// Bits in SSPSR
//-----------------------------------------------------------------------------
#define SSPSR_TFE		    0x00000001      // TX FIFO is empty
#define SSPSR_TNF    		0x00000002      // TX FIFO is not full
#define SSPSR_RNE        	0x00000004      // RX FIFO is not empty
#define SSPSR_RFF   		0x00000008      // RX FIFO is full
#define SSPSR_BSY			0x00000010      // SSP is busy

//-----------------------------------------------------------------------------
// Bits in SSPIIR
//-----------------------------------------------------------------------------
#define SSPIIR_RIS			0x00000001      // RX FIFO IRQ status
#define SSPIIR_TIS			0x00000002      // TX FIFO is not full
#define SSPIIR_RORIS		0x00000004      // RX FIFO is full

//=============================================================================
//=============================================================================

#endif /* _REGS_SSP_H_ */
