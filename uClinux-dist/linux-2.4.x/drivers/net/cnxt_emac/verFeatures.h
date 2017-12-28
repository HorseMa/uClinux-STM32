/****************************************************************************
*
*	Name:			verFeatures.h
*
*	Description:	
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
*					Personal Computing Division
*					All Rights Reserved
*
****************************************************************************
*  $Author:   davidsdj  $
*  $Revision:   1.0  $
*  $Modtime:   Mar 19 2003 12:25:22  $
****************************************************************************/

#ifndef _VER_H
#define _VER_H

/*
 * Version numbers
 */

/* EMAC HW versions */
#define REV_A   1
#define REV_B	2
#define REV_C   3
#define REV_D   4

/* EMAC SW versions */
#define VER_01   1
#define VER_02   2
#define VER_03   3
#define VER_04   4
#define VER_05   5
#define VER_06   6
#define VER_07   7
#define VER_08   8
#define VER_09   9
#define VER_10   10
#define VER_11   11
#define VER_12   12
#define VER_13   13
#define VER_14   14
#define VER_15   15
#define VER_16   16
#define VER_17   17
#define VER_18   18
#define VER_19   19
#define VER_20   20

/* EMAC versions */
#define MAC_HW_VER      REV_C		
#define MAC_SW_VER      VER_06     

/*
 * Version Features.
 */

/* Defines to control code generation depending on features available. */

#define INC_SNMP  0 /* AD Aug18 SNMP API support: include SNMP. */

#define NETPOOL_USE_DEF_FUNCTBL
#define REMEMBER_MClBlks_CL_STARTADDR
#define UCAST_IN_IOCTL		/* Unicast addr manipulation using IOCTL */
/*#define USE_BSP_ENET_RTN*/	/* Use the BSP fn to get the mac addr 1st time */
#define USE_BSP_SRAM_RTN
/*#define USE_BSP_DMA_M2M_OBJ*/
/*#define SUPPORT_MACADDR_FILTER*/
#define INC_PHY


#define USE_DMA_M2M_COPY_OP1
#define BUG_MUX_PATCH


/* Do rxHandling - netJobAdd - as late as possible. */
#define RXHANDLING_AS_SOON_AS_POSSIBLE				1
#define RXHANDLING_AS_LATE_AS_POSSIBLE				2
#define RXHANDLING_NO_RXHANDLING					3
#define RXHANDLING RXHANDLING_AS_LATE_AS_POSSIBLE

#if     (MAC_HW_VER == REV_A)

#define EMAC_TX_DMA_LMODE DMA_LMODE_TAIL
#define BUG_HW_EMAC_RX_1   /* This bug is in REV_A & will not be in REV_B/C...*/
#define BUG_HW_EMAC_RX_2   /* This bug is in REV_A & will not be in REV_B/C...*/
#define ZERO_MEM_USING_DMA /* zero memory using DMA */

#else   /* not MAC_HW_VER_A*/

#undef  BUG_HW_EMAC_RX_1 
#define EMAC_TX_DMA_LMODE DMA_LMODE_TAIL

#endif  /*MAC_HW_VER_A*/

#define TEST_PLATFORM_MODEL     1
#define TEST_PLATFORM_HW        2
#define TEST_PLATFORM           TEST_PLATFORM_HW


/*
 * Version strings.
 */

/* EMAC HW version strings */
#if     (MAC_HW_VER == REV_A)
#define MAC_HW_VER_STR  "A"
#elif   (MAC_HW_VER == REV_B)
#define MAC_HW_VER_STR  "B"
#elif   (MAC_HW_VER == REV_C)
#define MAC_HW_VER_STR  "C"
#elif   (MAC_HW_VER == REV_D)
#define MAC_HW_VER_STR  "D"
#else
#error Invalid EMAC HW version.
#endif

/* EMAC SW version strings */
#if     (MAC_SW_VER == VER_01)
#define MAC_SW_VER_STR  "0.1"
#elif   (MAC_SW_VER == VER_02)
#define MAC_SW_VER_STR  "0.2"
#elif   (MAC_SW_VER == VER_03)
#define MAC_SW_VER_STR  "0.3"
#elif   (MAC_SW_VER == VER_04)
#define MAC_SW_VER_STR  "0.4"
#elif   (MAC_SW_VER == VER_05)
#define MAC_SW_VER_STR  "0.5"
#elif   (MAC_SW_VER == VER_06)
#define MAC_SW_VER_STR  "0.6"
#elif   (MAC_SW_VER == VER_07)
#define MAC_SW_VER_STR  "0.7"
#elif   (MAC_SW_VER == VER_08)
#define MAC_SW_VER_STR  "0.8"
#elif   (MAC_SW_VER == VER_09)
#define MAC_SW_VER_STR  "0.9"
#elif   (MAC_SW_VER == VER_10)
#define MAC_SW_VER_STR  "1.0"
#elif   (MAC_SW_VER == VER_11)
#define MAC_SW_VER_STR  "1.1"
#elif   (MAC_SW_VER == VER_12)
#define MAC_SW_VER_STR  "1.2"
#elif   (MAC_SW_VER == VER_13)
#define MAC_SW_VER_STR  "1.3"
#elif   (MAC_SW_VER == VER_14)
#define MAC_SW_VER_STR  "1.4"
#elif   (MAC_SW_VER == VER_15)
#define MAC_SW_VER_STR  "1.5"
#elif   (MAC_SW_VER == VER_16)
#define MAC_SW_VER_STR  "1.6"
#elif   (MAC_SW_VER == VER_17)
#define MAC_SW_VER_STR  "1.7"
#elif   (MAC_SW_VER == VER_18)
#define MAC_SW_VER_STR  "1.8"
#elif   (MAC_SW_VER == VER_19)
#define MAC_SW_VER_STR  "1.9"
#elif   (MAC_SW_VER == VER_20)
#define MAC_SW_VER_STR  "2.0"
#else
#error Invalid EMAC SW version.
#endif

/* EMAC versions */
#define MAC_VER_STR "v:" MAC_HW_VER_STR "," MAC_SW_VER_STR

#endif _VER_H
