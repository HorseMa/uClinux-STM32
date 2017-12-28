/*
 * Filename: cx25871.h
 *                                                                     
 * Description: Regisister Definitions and function prototypes for
 *             CX25871 NTSC/PAL encoder.
 *
 * Copyright(c) Cirrus Logic Corporation 2003, All Rights Reserved                       
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

#ifndef _H_CX25871
#define _H_CX25871

//
// CS25871 Device Address.
//
#define CX25871_DEV_ADDRESS                     0x88

//
// Register 0x32
//
#define CX25871_REGx32_AUTO_CHK                 0x80
#define CX25871_REGx32_DRVS_MASK                0x60
#define CX25871_REGx32_DRVS_SHIFT               5
#define CX25871_REGx32_SETUP_HOLD               0x10
#define CX25871_REGx32_INMODE_                  0x08
#define CX25871_REGx32_DATDLY_RE                0x04
#define CX25871_REGx32_OFFSET_RGB               0x02
#define CX25871_REGx32_CSC_SEL                  0x01

//
// Register 0xBA
//
#define CX25871_REGxBA_SRESET                   0x80
#define CX25871_REGxBA_CHECK_STAT               0x40
#define CX25871_REGxBA_SLAVER                   0x20
#define CX25871_REGxBA_DACOFF                   0x10
#define CX25871_REGxBA_DACDISD                  0x08
#define CX25871_REGxBA_DACDISC                  0x04
#define CX25871_REGxBA_DACDISB                  0x02
#define CX25871_REGxBA_DACDISA                  0x01

//
// Register 0xC4
//
#define CX25871_REGxC4_ESTATUS_MASK             0xC0
#define CX25871_REGxC4_ESTATUS_SHIFT            6
#define CX25871_REGxC4_ECCF2                    0x20
#define CX25871_REGxC4_ECCF1                    0x10
#define CX25871_REGxC4_ECCGATE                  0x08
#define CX25871_REGxC4_ECBAR                    0x04
#define CX25871_REGxC4_DCHROMA                  0x02
#define CX25871_REGxC4_EN_OUT                   0x01
                                                

//
// Register 0xC6
//
#define CX25871_REGxC6_EN_BLANKO                0x80
#define CX25871_REGxC6_EN_DOT                   0x40
#define CX25871_REGxC6_FIELDI                   0x20
#define CX25871_REGxC6_VSYNCI                   0x10
#define CX25871_REGxC6_HSYNCI                   0x08
#define CX25871_REGxC6_INMODE_MASK              0x07
#define CX25871_REGxC6_INMODE_SHIFT             0

#endif // _H_CX25871
