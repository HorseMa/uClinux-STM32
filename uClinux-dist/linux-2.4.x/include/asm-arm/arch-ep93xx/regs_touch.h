/*=============================================================================
 *
 *  FILE:       regs_touch.h
 *
 *  DESCRIPTION:    Analog Touchscreen Register Definition
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
 *=============================================================================
 */
#ifndef _REGS_TOUCH_H_
#define _REGS_TOUCH_H_

/*
 *-----------------------------------------------------------------------------
 * Individual bit #defines
 *-----------------------------------------------------------------------------
 */
#define TSSETUP_SDLY_MASK           0x000003FF 
#define TSSETUP_SDLY_SHIFT          0
#define TSSETUP_NSMP_4              0x00000000
#define TSSETUP_NSMP_8              0x00000400
#define TSSETUP_NSMP_16             0x00000800
#define TSSETUP_NSMP_32             0x00000C00
#define TSSETUP_NSMP_MASK           0x00000C00
#define TSSETUP_DEV_4               0x00000000
#define TSSETUP_DEV_8               0x00001000
#define TSSETUP_DEV_12              0x00002000
#define TSSETUP_DEV_16              0x00003000
#define TSSETUP_DEV_24              0x00004000
#define TSSETUP_DEV_32              0x00005000
#define TSSETUP_DEV_64              0x00006000
#define TSSETUP_DEV_128             0x00007000
#define TSSETUP_ENABLE              0x00008000
#define TSSETUP_DLY_MASK            0x03FF0000
#define TSSETUP_DLY_SHIFT           16
#define TSSETUP_TDTCT               0x80000000

#define TSMAXMIN_XMIN_MASK          0x000000FF
#define TSMAXMIN_XMIN_SHIFT         0
#define TSMAXMIN_YMIN_MASK          0x0000FF00
#define TSMAXMIN_YMIN_SHIFT         8
#define TSMAXMIN_XMAX_MASK          0x00FF0000
#define TSMAXMIN_XMAX_SHIFT         16
#define TSMAXMIN_YMAX_MASK          0xFF000000
#define TSMAXMIN_YMAX_SHIFT         24

#define TSXYRESULT_X_MASK           0x00000FFF
#define TSXYRESULT_X_SHIFT          0
#define TSXYRESULT_AD_MASK          0x0000FFFF
#define TSXYRESULT_AD_SHIFT         0
#define TSXYRESULT_Y_MASK           0x0FFF0000
#define TSXYRESULT_Y_SHIFT          16
#define TSXYRESULT_SDR              0x80000000

#define TSX_SAMPLE_MASK             0x00003FFF
#define TSX_SAMPLE_SHIFT            0x00
#define TSY_SAMPLE_MASK             0x3FFF0000
#define TSY_SAMPLE_SHIFT            0x10

#define TSSETUP2_TINT               0x00000001
#define TSSETUP2_NICOR              0x00000002
#define TSSETUP2_PINT               0x00000004
#define TSSETUP2_PENSTS             0x00000008
#define TSSETUP2_PINTEN             0x00000010
#define TSSETUP2_DEVINT             0x00000020
#define TSSETUP2_DINTEN             0x00000040
#define TSSETUP2_DTMEN              0x00000080
#define TSSETUP2_DISDEV             0x00000100
#define TSSETUP2_NSIGND             0x00000200
#define TSSETUP2_S28EN              0x00000400
#define TSSETUP2_RINTEN             0x00000800

#define TSXYRESULT_SDR     			0x80000000

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */


#endif /* _REGS_TOUCH_H_ */
