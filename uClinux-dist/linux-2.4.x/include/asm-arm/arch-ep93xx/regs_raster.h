/*=============================================================================
 *
 *  FILE:       	regs_raster.h
 *
 *  DESCRIPTION:    ep93xx Raster Engine Register Definition
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
#ifndef _REGS_RASTER_H_
#define _REGS_RASTER_H_

//-----------------------------------------------------------------------------
// VLINESTOTAL Register Definitions
//-----------------------------------------------------------------------------
#define VLINESTOTAL_MASK            0x000007ff

//-----------------------------------------------------------------------------
// VSYNCSTRTSTOP Register Definitions
//-----------------------------------------------------------------------------
#define VSYNCSTRTSTOP_STRT_MASK     0x07ff0000 
#define VSYNCSTRTSTOP_STRT_SHIFT    0 
#define VSYNCSTRTSTOP_STOP_MASK     0x000007ff
#define VSYNCSTRTSTOP_STOP_SHIFT    16 

//-----------------------------------------------------------------------------
// VACTIVESTRTSTOP Register Definitions
//-----------------------------------------------------------------------------
#define VACTIVESTRTSTOP_STRT_MASK   0x07ff0000
#define VACTIVESTRTSTOP_STRT_SHIFT  0 
#define VACTIVESTRTSTOP_STOP_MASK   0x000007ff
#define VACTIVESTRTSTOP_STOP_SHIFT  16 

//-----------------------------------------------------------------------------
// VCLKSTRTSTOP Register Definitions
//-----------------------------------------------------------------------------
#define VCLKSTRTSTOP_STRT_MASK      0x07ff0000
#define VCLKSTRTSTOP_STRT_SHIFT     0 
#define VCLKSTRTSTOP_STOP_MASK      0x000007ff
#define VCLKSTRTSTOP_STOP_SHIFT     16 

//-----------------------------------------------------------------------------
// VBLANKSTRTSTOP Register Definitions
//-----------------------------------------------------------------------------
#define VBLANKSTRTSTOP_STRT_MASK  0x07ff0000
#define VBLANKSTRTSTOP_STRT_SHIFT 0 
#define VBLANKSTRTSTOP_STOP_MASK  0x000007ff
#define VBLANKSTRTSTOP_STOP_SHIFT 16 

//-----------------------------------------------------------------------------
// HSYNCSTRTSTOP Register Definitions
//-----------------------------------------------------------------------------
#define HSYNCSTRTSTOP_STRT_MASK      0x07ff0000
#define HSYNCSTRTSTOP_STRT_SHIFT     0 
#define HSYNCSTRTSTOP_STOP_MASK      0x000007ff
#define HSYNCSTRTSTOP_STOP_SHIFT     16 

//-----------------------------------------------------------------------------
// HACTIVESTRTSTOP Register Definitions
//-----------------------------------------------------------------------------
#define HACTIVESTRTSTOP_STRT_MASK    0x07ff0000
#define HACTIVESTRTSTOP_STRT_SHIFT   0 
#define HACTIVESTRTSTOP_STOP_MASK    0x000007ff
#define HACTIVESTRTSTOP_STOP_SHIFT   16 

//-----------------------------------------------------------------------------
// HCLKSTRTSTOP Register Definitions
//-----------------------------------------------------------------------------
#define HCLKSTRTSTOP_STRT_MASK      0x07ff0000
#define HCLKSTRTSTOP_STRT_SHIFT     0 
#define HCLKSTRTSTOP_STOP_MASK      0x000007ff
#define HCLKSTRTSTOP_STOP_SHIFT     16 

//-----------------------------------------------------------------------------
// BRIGHTNESS Register Definitions
//-----------------------------------------------------------------------------
#define BRIGHTNESS_MASK             0x0000ffff
#define BRIGHTNESS_CNT_MASK         0x000000ff
#define BRIGHTNESS_CNT_SHIFT        0
#define BRIGHTNESS_CMP_MASK         0x0000ff00
#define BRIGHTNESS_CMP_SHIFT        8

//-----------------------------------------------------------------------------
// VIDEOATTRIBS Register Definitions
//-----------------------------------------------------------------------------
#define VIDEOATTRIBS_MASK           0x001fffff
#define VIDEOATTRIBS_EN             0x00000001
#define VIDEOATTRIBS_PCLKEN         0x00000002
#define VIDEOATTRIBS_SYNCEN         0x00000004
#define VIDEOATTRIBS_DATAEN         0x00000008
#define VIDEOATTRIBS_CSYNC          0x00000010
#define VIDEOATTRIBS_VCPOL          0x00000020
#define VIDEOATTRIBS_HSPOL          0x00000040
#define VIDEOATTRIBS_BLKPOL         0x00000080
#define VIDEOATTRIBS_INVCLK         0x00000100
#define VIDEOATTRIBS_ACEN           0x00000200
#define VIDEOATTRIBS_LCDEN          0x00000400
#define VIDEOATTRIBS_RGBEN          0x00000800
#define VIDEOATTRIBS_CCIREN         0x00001000
#define VIDEOATTRIBS_PIFEN          0x00002000
#define VIDEOATTRIBS_INTEN          0x00004000
#define VIDEOATTRIBS_INT            0x00008000
#define VIDEOATTRIBS_INTRLC         0x00010000
#define VIDEOATTRIBS_EQUSER         0x00020000
#define VIDEOATTRIBS_DHORZ          0x00040000
#define VIDEOATTRIBS_DVERT          0x00080000
#define VIDEOATTRIBS_BKPXD          0x00100000

#define VIDEOATTRIBS_SDSEL_MASK     0x00600000
#define VIDEOATTRIBS_SDSEL_SHIFT    21

//-----------------------------------------------------------------------------
// HBLANKSTRTSTOP Register Definitions
//-----------------------------------------------------------------------------
#define HBLANKSTRTSTOP_STRT_MASK    0x07ff0000
#define HBLANKSTRTSTOP_STRT_SHIFT   0 
#define HBLANKSTRTSTOP_STOP_MASK    0x000007ff
#define HBLANKSTRTSTOP_STOP_SHIFT   16 

//-----------------------------------------------------------------------------
// LINECARRY Register Definitions
//-----------------------------------------------------------------------------
#define LINECARRY_LCARY_MASK        0x000007ff
#define LINECARRY_LCARY_SHIFT       0

//-----------------------------------------------------------------------------
// BLINKRATE Register Definitons
//-----------------------------------------------------------------------------
#define BLINKRATE_MASK              0x000000ff

//-----------------------------------------------------------------------------
// BLINKMASK Register Definitons
//-----------------------------------------------------------------------------
#define BLINKMASK_MASK              0x00ffffff

//-----------------------------------------------------------------------------
// VIDSCRNPAGE Register Definitons
//-----------------------------------------------------------------------------
#define VIDSCRNPAGE_PAGE_MASK       0x0ffffffc

//-----------------------------------------------------------------------------
// VIDSCRNHPG Register Definitons
//-----------------------------------------------------------------------------
#define VIDSCRNHPG_MASK             0x0ffffffc

//-----------------------------------------------------------------------------
// SCRNLINES Register Definitons
//-----------------------------------------------------------------------------
#define SCRNLINES_MASK              0x000007ff

//-----------------------------------------------------------------------------
// LINELENGTH Register Definitons
//-----------------------------------------------------------------------------
#define LINELENGTH_MASK             0x000007ff

//-----------------------------------------------------------------------------
// VLINESTEP Register Definitons
//-----------------------------------------------------------------------------
#define VLINESTEP_MASK              0x00000fff

//-----------------------------------------------------------------------------
// REALITI_SWLOCK Register Definitons
//-----------------------------------------------------------------------------
#define REALITI_SWLOCK_MASK_WR      0xff
#define REALITI_SWLOCK_MASK_R       0x1
#define REALITI_SWLOCK_VALUE        0xaa

//-----------------------------------------------------------------------------
// LUTCONT Register Definitions
//-----------------------------------------------------------------------------
#define LUTCONT_MASK                0x00000003
#define LUTCONT_SWTCH               0x00000001
#define LUTCONT_STAT                0x00000002
#define LUTCONT_RAM0                0
#define LUTCONT_RAM1                1

//-----------------------------------------------------------------------------
// CURSORBLINK1 Register Definitions
//-----------------------------------------------------------------------------
#define CURSORBLINK1_MASK           0x00ffffff
//-----------------------------------------------------------------------------
// CURSORBLINK2 Register Definitions
//-----------------------------------------------------------------------------
#define CURSORBLINK2_MASK           0x00ffffff

//-----------------------------------------------------------------------------
// CURSORBLINK Register Definitions
//-----------------------------------------------------------------------------
#define CURSORBLINK_MASK            0x000001ff
#define CURSORBLINK_RATE_MASK       0x000000ff
#define CURSORBLINK_RATE_SHIFT      0
#define CURSORBLINK_EN              0x00000100    

//-----------------------------------------------------------------------------
// BLINKPATRN Register Definitions
//-----------------------------------------------------------------------------
#define BLINKPATRN_MASK             0x00ffffff

//-----------------------------------------------------------------------------
// PATRNMASK Register Definitions
//-----------------------------------------------------------------------------
#define PATRNMASK_MASK              0x00ffffff

//-----------------------------------------------------------------------------
// BG_OFFSET Register Definitions
//-----------------------------------------------------------------------------
#define BG_OFFSET_MASK              0x00ffffff

//-----------------------------------------------------------------------------
// PIXELMODE Register Definitions
//-----------------------------------------------------------------------------
#define PIXELMODE_P_MASK            0x00000007
#define PIXELMODE_P_MUX_DISABLE     0x00000000            
#define PIXELMODE_P_4BPP            0x00000001            
#define PIXELMODE_P_8BPP            0x00000002            
#define PIXELMODE_P_16BPP           0x00000004            
#define PIXELMODE_P_24BPP           0x00000006            
#define PIXELMODE_P_32BPP           0x00000007            
#define PIXELMODE_P_SHIFT           0

#define PIXELMODE_S_MASK            0x00000038
#define PIXELMODE_S_SHIFT           3
#define PIXELMODE_S_1PPC            0x0
#define PIXELMODE_S_1PPCMAPPED      0x1
#define PIXELMODE_S_2PPC            0x2
#define PIXELMODE_S_4PPC            0x3
#define PIXELMODE_S_8PPC            0x4
#define PIXELMODE_S_223PPC          0x5
#define PIXELMODE_S_DS223PPC        0x6
#define PIXELMODE_S_UNDEF           0x7

#define PIXELMODE_M_MASK            0x000003c0
#define PIXELMODE_M_SHIFT           6
#define PIXELMODE_M_NOBLINK         0
#define PIXELMODE_M_ANDBLINK        1
#define PIXELMODE_M_ORBLINK         2
#define PIXELMODE_M_XORBLINK        3
#define PIXELMODE_M_BGBLINK         4
#define PIXELMODE_M_OFFSINGBLINK    5
#define PIXELMODE_M_OFF888BLINK     6
#define PIXELMODE_M_DIMBLINK        0xc
#define PIXELMODE_M_BRTBLINK        0xd
#define PIXELMODE_M_DIM888BLINK     0xe
#define PIXELMODE_M_BRT888BLINK     0xf

#define PIXELMODE_C_MASK            0x00003c00
#define PIXELMODE_C_SHIFT           10
#define PIXELMODE_C_LUT             0
#define PIXELMODE_C_888             4
#define PIXELMODE_C_565             5
#define PIXELMODE_C_555             6
#define PIXELMODE_C_GSLUT           8

#define PIXELMODE_DSCAN             0x00004000
#define PIXELMODE_TRBSW             0x00008000
#define PIXELMODE_P13951            0x00010000

//-----------------------------------------------------------------------------
//PARLLIFOUT Register Defintions
//-----------------------------------------------------------------------------
#define PARLLIFOUT_DAT_MASK         0x0000000f
#define PARLLIFOUT_DAT_SHIFT        0
#define PARLLIFOUT_RD               0x00000010

//-----------------------------------------------------------------------------
//PARLLIFIN Register Defintions
//-----------------------------------------------------------------------------
#define PARLLIFIN_DAT_MASK          0x0000000f
#define PARLLIFIN_DAT_SHIFT         0
#define PARLLIFIN_CNT_MASK          0x000f0000
#define PARLLIFIN_CNT_SHIFT         16
#define PARLLIFIN_ESTRT_MASK        0x00f00000
#define PARLLIFIN_ESTRT_SHIFT       20

//-----------------------------------------------------------------------------
// CURSORADRSTART Register Defintions
//-----------------------------------------------------------------------------
#define CURSOR_ADR_START_MASK         0xfffffffc

//-----------------------------------------------------------------------------
// CURSORADRSTART Register Defintions
//-----------------------------------------------------------------------------
#define CURSOR_ADR_RESET_MASK         0xfffffffc

//-----------------------------------------------------------------------------
// CURSORCOLOR1 Register Definitions
//-----------------------------------------------------------------------------
#define CURSORCOLOR1_MASK               0x00ffffff
//-----------------------------------------------------------------------------
// CURSORCOLOR2 Register Definitions
//-----------------------------------------------------------------------------
#define CURSORCOLOR2_MASK               0x00ffffff

//-----------------------------------------------------------------------------
// CURSORXYLOC Register Definitions
//-----------------------------------------------------------------------------
#define CURSORXYLOC_MASK                0x07ff87ff
#define CURSORXYLOC_XLOC_MASK           0x000007ff
#define CURSORXYLOC_XLOC_SHIFT          0
#define CURSORXYLOC_CEN                 0x00008000
#define CURSORXYLOC_YLOC_MASK           0x07ff0000
#define CURSORXYLOC_YLOC_SHIFT          16

//-----------------------------------------------------------------------------
// CURSOR_DSCAN_LH_YLOC Register Definitions
//-----------------------------------------------------------------------------
#define CURSOR_DSCAN_LH_YLOC_MASK       0x000087ff

#define CURSOR_DSCAN_LH_YLOC_YLOC_MASK  0x000007ff
#define CURSOR_DSCAN_LH_YLOC_YLOC_SHIFT 0
#define CURSOR_DSCAN_LH_YLOC_CLHEN      0x00008000

//-----------------------------------------------------------------------------
// CURSORSIZE Register Definitions
//-----------------------------------------------------------------------------
#define CURSORSIZE_MASK                 0x0000ffff

#define CURSORSIZE_CWID_MASK            0x00000003
#define CURSORSIZE_CWID_SHIFT           0
#define CURSORSIZE_CWID_1_WORD          0
#define CURSORSIZE_CWID_2_WORD          1
#define CURSORSIZE_CWID_3_WORD          2
#define CURSORSIZE_CWID_4_WORD          3

#define CURSORSIZE_CLINS_MASK           0x000000fc
#define CURSORSIZE_CLINS_SHIFT          2

#define CURSORSIZE_CSTEP_MASK           0x00000300
#define CURSORSIZE_CSTEP_SHIFT          8
#define CURSORSIZE_CSTEP_1_WORD         0
#define CURSORSIZE_CSTEP_2_WORD         1
#define CURSORSIZE_CSTEP_3_WORD         2
#define CURSORSIZE_CSTEP_4_WORD         3

#define CURSORSIZE_DLNS_MASK            0x0000fc00
#define CURSORSIZE_DLNS_SHIFT           10

#endif /* _REGS_RASTER_H_ */
