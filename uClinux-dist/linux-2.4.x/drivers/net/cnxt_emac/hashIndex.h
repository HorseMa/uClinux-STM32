/****************************************************************************
*
*	Name:			hashIndex.h
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

#ifndef __HASH_INDEX_H
#define __HASH_INDEX_H
/*
 * 
 *
 *
 */

#define EADDR_LEN 6 /* Lenth of ethernet address, in bytes. */

/*
 * Hash Index (HI) methods & types.
 */

/* List of available HI methods. */
#define HI_METHOD_CRC32        0  /* CRC32 method. */
#define HI_METHOD_RSSCM        1  /* RSS Cable Modem (CM) HI method. */
#define HI_METHOD_NEW          2  /* Example. */

/* The default HI method. */
#define HI_METHOD_DEF HI_METHOD_RSSCM

/* No HI method specified? Choose the default one. */
#ifndef HI_METHOD
#define HI_METHOD HI_METHOD_DEF
#endif



#define NUM_HI_SUPPORTED 256
typedef unsigned char HI;
#define HT_Y(hi) ((hi)>>3)            /* byte# in the Hash Table  */
#define HT_X(hi) (1 << ((hi) & 0x07)) /* bit position in the byte */


/*
 * In next two macros the interpretation of the location is as follows:
 * L(x,y) means: nth bit in yth byte of HT. Where n is the bit # of the
 * only bit set in x. Depending upon various HT organisation the y co-ordinate
 * may change from plain HT_Y(hi), but x remains unchanged.
 *
 * HT options for Y co-ord modifications: These options decide which bytes
 * (pattern) to be used for y co-ordinate. Commonly used options are listed
 * below.
 *
 * HT_OPTY_i_j: First i bytes [0...i-1] are used for y. Next j bytes [i...i+j-1]
 *              are not used. Then again bytes [i+j...i+j+i-1] are used & so on.
 *
 * HT_OPTY_1_0: All the bytes of HT are used for y co-ordinate.
 *              Here y = HT_Y(hi);
 *
 * HT_OPTY_1_1: Byte# 0,2,4,6,8,...are used & 1,3,5,7... not used.
 *              Here y = HT_OPTY_1_1(HT_Y(hi));
 * 
 * HT_OPTY_2_2: Byte# 0,1,4,5,8,9,...are used & 2,3,6,7,...not used.
 *              Here y = HT_OPTY_1_1(HT_Y(hi));
 *              This is the commonly used option when the HT is to be formed
 *              in the Setup frame.
 */

/* Set the location (x,y), in the HT. */
#define HT_SET_HI(ht, x, y)    (ht[(y)] |= (x))

/* Check the location (x,y), in the HT.(i.e. xth bit in yth byte of HT) */
#define HT_PASS_HI(ht, x, y)   ((ht[(y)] & (x)) ? 1:0)

#define HT_OPTY_1_0(y) (y)
#define HT_OPTY_1_1(y) TBD /* To Be Defined */
#define HT_OPTY_2_2(y) ((((y) & ~0x1) * 2) + ((y) & 0x1))

#define HT1_0_SET_HI(ht,hi) HT_SET_HI(ht,HT_X(hi),HT_OPTY_1_0(HT_Y(hi)))
#define HT1_1_SET_HI(ht,hi) HT_SET_HI(ht,HT_X(hi),HT_OPTY_1_1(HT_Y(hi)))
#define HT2_2_SET_HI(ht,hi) HT_SET_HI(ht,HT_X(hi),HT_OPTY_2_2(HT_Y(hi)))

#define HT1_0_PASS_HI(ht,hi) HT_PASS_HI(ht,HT_X(hi),HT_OPTY_1_0(HT_Y(hi)))
#define HT1_1_PASS_HI(ht,hi) HT_PASS_HI(ht,HT_X(hi),HT_OPTY_1_1(HT_Y(hi)))
#define HT2_2_PASS_HI(ht,hi) HT_PASS_HI(ht,HT_X(hi),HT_OPTY_2_2(HT_Y(hi)))

/* 
 * Hash Table size: Depending on the HT_OPTY_*_*, table size required to hold
 * HIs varies. This size is #ofbytes required to hold Hash Values (HV) for all 
 * of the Hash Indices. Each HV is a bit: HV = value of HIth bit.
 */
#define HT1_0_SIZE   (NUM_HI_SUPPORTED/8) 
#define HT1_1_SIZE   (2*HT1_0_SIZE) 
#define HT2_2_SIZE   HT1_1_SIZE 

/* Externals. */
extern HI HashIndex(unsigned char* eAddr);

/* Macros for a user of this module. */
#define HASH_INDEX(eAddr) HashIndex((eAddr))

#endif __HASH_INDEX_H
