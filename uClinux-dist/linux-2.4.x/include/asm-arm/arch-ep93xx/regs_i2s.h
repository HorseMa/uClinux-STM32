/*=============================================================================
 *
 *  FILE:       	reg_i2s.h
 *
 *  DESCRIPTION:    ep93xx I2S Register Definition
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
 *
 *=============================================================================
 */
#ifndef _REG_I2S_H_
#define _REG_I2S_H_

//
// I2STXClkCfg bits
//
#define i2s_txcc_trls           0x00000001
#define i2s_txcc_tckp           0x00000002
#define i2s_txcc_trel           0x00000004
#define i2s_txcc_mstr           0x00000008
#define i2s_txcc_nbcg           0x00000010

#define i2s_txcc_bcr_32x        0x00000020
#define i2s_txcc_bcr_64x        0x00000040
#define i2s_txcc_bcr_128x       0x00000060

//
// I2SRxClkCfg bits
//
#define i2s_rxcc_rrls           0x00000001
#define i2s_rxcc_rckp           0x00000002
#define i2s_rxcc_rrel           0x00000004
#define i2s_rxcc_mstr           0x00000008
#define i2s_rxcc_nbcg           0x00000010

#define i2s_rxcc_bcr_32x        0x00000020
#define i2s_rxcc_bcr_64x        0x00000040
#define i2s_rxcc_bcr_128x       0x00000060

//
// I2SGlSts bits
//
#define TX0_UNDERFLOW           0x00000001
#define TX1_UNDERFLOW           0x00000002
#define TX2_UNDERFLOW           0x00000004

#define RX0_OVERFLOW            0x00000008
#define RX1_OVERFLOW            0x00000010
#define RX2_OVERFLOW            0x00000020

#define TX0_OVERFLOW            0x00000040
#define TX1_OVERFLOW            0x00000080
#define TX2_OVERFLOW            0x00000100

#define RX0_UNDERFLOW           0x00000200
#define RX1_UNDERFLOW           0x00000400
#define RX2_UNDERFLOW           0x00000800

#define TX0_FIFO_FULL           0x00001000
#define TX0_FIFO_EMPTY          0x00002000
#define TX0_FIFO_HALF_EMPTY     0x00004000

#define RX0_FIFO_FULL           0x00008000
#define RX0_FIFO_EMPTY          0x00010000
#define RX0_FIFO_HALF_FULL      0x00020000

#define TX1_FIFO_FULL           0x00040000
#define TX1_FIFO_EMPTY          0x00080000
#define TX1_FIFO_HALF_EMPTY     0x00100000

#define RX1_FIFO_FULL           0x00200000
#define RX1_FIFO_EMPTY          0x00400000
#define RX1_FIFO_HALF_FULL      0x00800000

#define TX2_FIFO_FULL           0x01000000
#define TX2_FIFO_EMPTY          0x02000000
#define TX2_FIFO_HALF_EMPTY     0x04000000

#define RX2_FIFO_FULL           0x08000000
#define RX2_FIFO_EMPTY          0x10000000
#define RX2_FIFO_HALF_FULL      0x20000000

#endif // _REG_I2S_H_
