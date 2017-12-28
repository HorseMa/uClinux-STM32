/****************************************************************************
*
*	Name:			filters.h
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

#ifndef __FILTERS_H
#define __FILTERS_H


/* 
 * Hashing & Address filtering.
 */

#include "hashIndex.h"

/*
 * Ethernet Address Filters supported by MAC HW...
 */

/* Basic filters */
/* 16 */
#define HW_FLT_16PER    0x0001/* 16 uni or multi perfect  */
#define HW_FLT_16INV    0x0002/* 16 uni or multi inverse  */
#define HW_FLT_16PER_PM 0x0004/* Pass all multi & 16 uni perfect */
#define HW_FLT_16RES    0x0008/* reserved */

/* hash */
#define HW_FLT_1PER_HASH      0x0010/* 1 uni perfect & unlimited multi hash */
#define HW_FLT_HASH           0x0020/* unlimited uni and/or multi hash */
#define HW_FLT_1PER_PM        0x0040/* reserved */
#define HW_FLT_HASH_RES2      0x0080/* reserved */

/* others */
#define HW_FLT_PM       0x0100/* Pass all multicast addrs */
#define HW_FLT_PR       0x0200/* Promiscuous: all good pkts pass */
#define HW_FLT_PB       0x0400/* Pass all bad pkts */

/* Derived filters */
#define HW_FLT_16ANY          (HW_FLT_16PER|HW_FLT_16INV|HW_FLT_16PER_PM|\
                              HW_FLT_16RES)
#define HW_FLT_HASHANY        (HW_FLT_1PER_HASH|HW_FLT_HASH|HW_FLT_1PER_PM|\
                              HW_FLT_HASH_RES2)
#define HW_FLT_OTHERANY       (HW_FLT_1PER_PM|HW_FLT_PM|HW_FLT_PR|\
                              HW_FLT_PB)

/* Default filter */
#define HW_FLT_DEF            HW_FLT_16ANY/* our default */

/* Default filter */
#define HW_FLT                HW_FLT_DEF

/* 
 * Setup Frame Filtering Modes: 
 *
 * There are two ways of storing an ethernet address in the setup frame.
 * 1) Directly: copying the addr, byte by byte.
 * 2) hashing : set 1bit -per addr- of the hash table contained within setup frame.
 *
 * Hence, eventhough there are multiple modes available for address filtering, 
 * as far as preparation of the setup frame goes there are only two. 
 * 1) "Perfect": 16 addrs stored "directly".
 * 2) "1Perfect-Hash": 1 addr stored directly & unlimited addrs stored using hashing.
 */

#define SF_FM_NONE       0x0/* No setup frame filter needed */
#define SF_FM_PER        0x1/* 16 addrs perfect & no hash table*/
#define SF_FM_1PER_HASH  0x2/* 1 addr perfect & a hash table */

#ifdef SF_FM
#undef SF_FM
#endif /* SF_FM */

#if (HW_FLT & HW_FLT_16ANY)
#define SF_FM   SF_FM_PER
#endif /* HW_FLT & HW_FLT_16ANY */

#if (HW_FLT & HW_FLT_HASHANY)
#ifdef SF_FM
#error Setup frame filter can not support mixing of hash & 16* filters.
#else
#define SF_FM   SF_FM_1PER_HASH
#endif /* SF_FM */
#endif /* HW_FLT & HW_FLT_HASHANY */

#if (HW_FLT & HW_FLT_OTHERANY)
#ifndef SF_FM
#define SF_FM       SF_FM_NONE
#endif /* SF_FM */
#endif /* HW_FLT & HW_FLT_OTHERANY */

/*
 *
 */

#if 0
#define E_NA_HP		((ULONG)(0x08000000)) /* Hash/perfect addr filter [27]	*/
#define E_NA_HO		((ULONG)(0x04000000)) /* Hash only			[26]		*/
#define E_NA_IF		((ULONG)(0x02000000)) /* Inverse Filter		[25]		*/
#define E_NA_PR		((ULONG)(0x01000000)) /* Promiscuous mode	[24]		*/
#define E_NA_PM		((ULONG)(0x00800000)) /* Pass all Multicast [23]		*/
#define E_NA_PB		((ULONG)(0x00400000)) /* Pass bad packet	[22]		*/

#define E_NA_FM     (E_NA_IF|E_NA_PM)
#endif

#if 0
typedef struct _FLT_FRM_EADDR_BYTE_INDEX
{
#if  (SF_FM==SF_FM_PER)
int index;
int indexOfst;

#elif(SF_FM==SF_FM_1PER_HASH)
HI hi;

#elif(SF_FM==SF_FM_NONE)

#else
#error Wrong HW filter mode set.

#endif /* SF_FM */
}FLT_FRM_EADDR_BYTE_INDEX;
#endif

#define HASH_TBL_SIZ HT1_0_SIZE 
#define IS_FILTERED_HI(pDrvCtrl, hi) HT1_0_PASS_HI((pDrvCtrl)->hashTable,hi)
#define ADD_TO_HASHTBL(pDrvCtrl, hi) HT1_0_SET_HI((pDrvCtrl)->hashTable,hi)

#define FLT_INDEX               HT_OPTY_2_2
#define FLTR_FRM_PHY_ADRS_OFF	156   /* = FILT_INDEX(13 * 6):14th addr */
#define FLTR_FRM_SIZE		    0xC0  /* filter frm size 192 bytes	*/
#define FLTR_FRM_NUM_ADRS       16

/* 
 * If the LSB bit - 0th - of the first byte - byte #0 - of the
 * ethernet address, is set - 1- then the address is a multicast one
 * else it is unicast.
 */
//#define IS_MULTICAST(addrPtr) ( (*((UCHAR *)(addrPtr))) & (UCHAR *)0x01 )


#endif /* __FILTERS_H */
