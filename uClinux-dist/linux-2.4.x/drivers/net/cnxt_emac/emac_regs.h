/****************************************************************************
*
*	Name:			emac_regs.h
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

#ifndef __EMAC_REGS_H
#define __EMAC_REGS_H


/*******************************************************************************
 * Reg base addr & Reg offset.
 * The reg offset is same for all the sub-devices (like DMAC, EMAC & 
 * Interrupt controller).
 */


#ifndef REG_BASE_ADDR
#define REG_BASE_ADDR	 ((BYTE*)(0x00300000)) /* double word aligned */
#endif

#ifndef BE_OFST
#define BE_OFST		 0   /* 0 for little endian, 7 for big  */
#endif

#ifndef REG_OFFSET
#define REG_OFFSET	 (BE_OFST + 0x04) /* double word aligned */
#endif

/*******************************************************************************
 *
 * Define EMAC REGs and macros to access them.
 * To optimize REG accesses, redefine EMAC_REG_READ and
 * EMAC_REG_WRITE macros in a wrapper file.
 */

#define EMAC_BASE_ADDR      ((UCHAR *)0x00310000)

#define EMAC_REG_OFFSET		(REG_OFFSET)	/* double word aligned */

/* Reg numbering w.r.t base */

#define E_DMA	0		/* EMAC TxD / Rx DMA port [63:0]. NOT USED BY DRIVER */
#define E_NA	1		/* network access [32:0]*/
#define E_STAT	2		/* status */
#define E_IE	3		/* enable interrupts */
#define E_LP	4		/* Receiver Last Packet. */
#define E_MII	6		/* MII Management Interface */
#define E_TDMA	8		/* TxD status DMA port. NOT USED BY DRIVER */

/* Reg access macros */

#define EMAC_REG_ADDR(regNum,unit)  (EMAC_BASE_ADDR + (unit * 0x00010000) \
									+ ((regNum) * EMAC_REG_OFFSET))

#define EMAC_INTR_BASE_ADDR      ((volatile ULONG*)0x0031000c)

#define EMAC_INTR_REG_WRITE(val,unit)  (*(EMAC_INTR_BASE_ADDR + (unit * 0x00010000)) \
											 = (val))

#define E_INTR	0	
#define EMAC_INTR_REG_ADDR(regNum)   ((ULONG *)(EMAC_INTR_BASE_ADDR + (unit * 0x00010000)\
                                 + ((regNum) * EMAC_REG_OFFSET)))

#ifndef EMAC_INTR_REG_READ
#define EMAC_INTR_REG_READ(unit)       (*(EMAC_INTR_BASE_ADDR + (unit * 0x00010000)))
#endif /* EMAC_REG_READ */

#ifndef EMAC_REG_READ
#define EMAC_REG_READ(regNum,unit)       (*((volatile ULONG*)EMAC_REG_ADDR((regNum),unit)))
#endif /* EMAC_REG_READ */

#ifndef EMAC_REG_WRITE
#define EMAC_REG_WRITE(regNum,val,unit)  (*((volatile ULONG*)EMAC_REG_ADDR((regNum),unit)) = (val))
#endif /* EMAC_REG_WRITE */

#ifndef EMAC_REG_UPDATE
#define EMAC_REG_UPDATE(regNum,val,unit)  EMAC_REG_WRITE((regNum), \
                                     EMAC_REG_READ((regNum),unit) | (val), unit)
#endif /* EMAC_REG_UPDATE */
    
#ifndef EMAC_REG_RESET
#define EMAC_REG_RESET(regNum,val,unit)   EMAC_REG_WRITE((regNum), \
                                     EMAC_REG_READ((regNum),unit) & ~(val),unit)
#endif /* EMAC_REG_RESET */
    
#ifndef EMAC_REG_RESET_SET
#define EMAC_REG_RESET_SET(regNum,resetVal, setVal,unit)\
            EMAC_REG_WRITE((regNum), \
            (EMAC_REG_READ((regNum),unit) & ~(resetVal))|(setVal),unit)
#endif /* EMAC_REG_RESET_SET */

/*******************************************************************************
 *
 * Define EMAC bit masks.
 */


/*
 * EMAC Register masks. 
 */

#define E_NA_MSK	((ULONG)(0xcffffff9)) /* [31:0]RW  */
#define E_STAT_MSK	((ULONG)(0xffffffff)) /* [31:0]RW* */
#define E_IE_MSK	((ULONG)(0x0001ffff)) /* [16:0]RW  */
#define E_LP_MSK	((ULONG)(0x000003ff)) /* [9:0] RW  */
#define E_MII_MSK	((ULONG)(0x0000001f)) /* [4:0]     */


/* 
 * EMAC Register bit masks.
 */


/* 
 * Network Access 
 */

#define E_NA_RTX	((ULONG)((ULONG)1<<31)) /* Tx Software Reset	        [31]        */
#define E_NA_STOP	((ULONG)((ULONG)1<<30)) /* Stop transmit		        [30]        */
#define E_NA_HP		((ULONG)((ULONG)1<<27)) /* Hash/perfect addr filter     [27]        */
#define E_NA_HO		((ULONG)((ULONG)1<<26)) /* Hash only			        [26]        */
#define E_NA_IF		((ULONG)((ULONG)1<<25)) /* Inverse Filter		        [25]        */
#define E_NA_PR		((ULONG)((ULONG)1<<24)) /* Promiscuous mode	            [24]        */
#define E_NA_PM     ((ULONG)((ULONG)1<<23)) /* Pass all Multicast           [23]        */
#define E_NA_PB		((ULONG)((ULONG)1<<22)) /* Pass bad packet	            [22]        */
#define E_NA_RRX	((ULONG)((ULONG)1<<21)) /* Rx s/w reset		            [21]        */
#define E_NA_THU	((ULONG)((ULONG)1<<20)) /* Tx test HUJ counter          [20]        */
#define E_NA_DIS	((ULONG)((ULONG)1<<19)) /* Tx Disable backoff counter   [19]        */
#define E_NA_RUT	((ULONG)((ULONG)1<<18)) /* Tx reset unit timer          [18]        */
#define E_NA_IFG	((ULONG)((ULONG)3<<16)) /* Inter-frame gap	            [17:16]		*/
#define E_NA_JBD	((ULONG)((ULONG)1<<15)) /* Jabber disable		        [15]        */
#define E_NA_HUJ	((ULONG)((ULONG)1<<14)) /* Host Un-Jabber		        [14]        */
#define E_NA_JCLK	((ULONG)((ULONG)1<<13)) /* Jabber clock		            [13]        */
#define E_NA_SB		((ULONG)((ULONG)1<<12)) /* Start/Stop Backoff Counter   [12]        */
#define E_NA_FD		((ULONG)((ULONG)1<<11)) /* Full duplex		            [11]        */
#define E_NA_OM 	((ULONG)((ULONG)3<<9 )) /* Operating mode               [10:9]	   	*/
#define E_NA_OM_R	((ULONG)((ULONG)3<<9 )) /* Operating mode: Reserved	    [10:9]	-GL	*/
#define E_NA_OM_E   ((ULONG)((ULONG)2<<9 )) /* Operating mode: External LB	[10:9]  -GL	*/ 
#define E_NA_OM_I   ((ULONG)((ULONG)1<<9 )) /* Operating mode: Internal LB	[10:9]	-GL	*/
#define E_NA_FC		((ULONG)((ULONG)1<<8 )) /* Force Collisions	            [8]         */
#define E_NA_HLAN	((ULONG)((ULONG)1<<7 )) /* Home LAN Enablee	            [7] 	    */
#define E_NA_SR		((ULONG)((ULONG)1<<6 )) /* Start/Stop receive	        [6] 	    */
#define E_NA_NS		((ULONG)((ULONG)1<<5 )) /* Network speed		        [5] 	    */
#define E_NA_RWR	((ULONG)((ULONG)1<<4 )) /* Receive WD release	        [4]			*/
#define E_NA_RWD	((ULONG)((ULONG)1<<3 )) /* Receive WD disable	        [3] 	    */
#define E_NA_STRT	((ULONG)((ULONG)1    )) /* Start transmit		        [0] 	    */

/* 
 * Status 
 */

#define E_STAT_TU	((ULONG)(0x80000000)) /* Transmit descriptor not ready	
                                             [31] */
#define E_STAT_RS	((ULONG)(0x78000000)) /* Receive state					
                                             [30:27] */
#define E_STAT_RO	((ULONG)(0x04000000)) /* Receive FIFO overflow			
                                             [26] */
#define E_STAT_RWT	((ULONG)(0x02000000)) /* Receive WD timeout				
                                             [25] */
#define E_STAT_TDS	((ULONG)(0x01e00000)) /* Transmit buffer manager state	
                                             [24:21] */
#define E_STAT_TS	((ULONG)(0x001e0000)) /* Transmit state					
                                             [20:17] */
#define E_STAT_TOF	((ULONG)(0x00010000)) /* Transmit FIFO overflow			
                                             [16] */
#define E_STAT_TUF	((ULONG)(0x00008000)) /* Transmit FIFO underflow		
                                             [15] */
#define E_STAT_ED	((ULONG)(0x00004000)) /* Excessive Transmit deferrals	
                                             [14] */
#define E_STAT_DF	((ULONG)(0x00002000)) /* Tx frame differed				
                                             [13] */
#define E_STAT_CD	((ULONG)(0x00001000)) /* Frame transmit done from MII	
                                             [12] */
#define E_STAT_ES	((ULONG)(0x00000800)) /* Transmitter error summary		
                                             [11] */
#define E_STAT_RLD	((ULONG)(0x00000400)) /* Transmit FIFO reload/abort		
                                             [10] */
#define E_STAT_TF	((ULONG)(0x00000200)) /* Transmit fault					
                                             [9] */
#define E_STAT_TJT	((ULONG)(0x00000100)) /* Transmit Jabber Time-Out		
                                             [8] */
#define E_STAT_NCRS	((ULONG)(0x00000080)) /* No Carrier						
                                             [7] */
#define E_STAT_LCRS	((ULONG)(0x00000040)) /* Carrier was lost				
                                             [6] */
#define E_STAT_16	((ULONG)(0x00000020)) /* 16 or more collisions			
                                             [5] */
#define E_STAT_LC	((ULONG)(0x00000010)) /* Late collision					
                                             [4] */
#define E_STAT_CC	((ULONG)(0x0000000f)) /* Transmit collision count		
                                             [3:0] */
/* 
 * Interrupt enable 
 */

#define E_IE_AU		((ULONG)(0x00020000)) /* Transmit FIFO almost underflow interrupt enable
                                             [17] */
#define E_IE_NI		((ULONG)(0x00010000)) /* Normal interrupt summary enable
                                             [16] */
#define E_IE_RW		((ULONG)(0x00008000)) /* Receive WD Timer				
                                             [15] */
#define E_IE_RI		((ULONG)(0x00004000)) /* Receive OK interrupt enable	
                                             [14] */
#define E_IE_AI		((ULONG)(0x00002000)) /* Abnormal interrupt summary 	
                                             [13] */
#define E_IE_LC		((ULONG)(0x00001000)) /* Late collision enable			
                                             [12] */
#define E_IE_16		((ULONG)(0x00000800)) /* 16 collisions interrupt enable	
                                             [11] */
#define E_IE_LCRS	((ULONG)(0x00000400)) /* Lost carrier interrupt enable	
                                             [10] */
#define E_IE_NCRS	((ULONG)(0x00000200)) /* No carrier interrupt enable	
                                             [9] */
#define E_IE_TF		((ULONG)(0x00000100)) /* Transmit fault enable			
                                             [8] */
#define E_IE_RLD	((ULONG)(0x00000080)) /* Transmit reload/abort enable	
                                             [7] */
#define E_IE_ED		((ULONG)(0x00000040)) /* Excessive deferral enable		
                                             [6] */
#define E_IE_DF		((ULONG)(0x00000020)) /* Transmit deferred enable		
                                             [5] */
#define E_IE_TOF	((ULONG)(0x00000010)) /* Transmit overflow enable		
                                             [4] */
#define E_IE_TUF	((ULONG)(0x00000008)) /* Transmit underflow enable		
                                             [3] */
#define E_IE_TJT	((ULONG)(0x00000004)) /* Transmit Jabber time-out enable
                                             [2] */
#define E_IE_TU		((ULONG)(0x00000002)) /* Transmit process stopped enable
                                             [1] */
#define E_IE_TI		((ULONG)(0x00000001)) /* Transmit OK interrupt			
                                             [0] */

/* 
 * Reciever Last Packet 
 */

#define E_LP_RDMA	((ULONG)(0xf<<6)) 	/* [9:6] NOT USED by the driver */
#define E_LP_RFIFO	((ULONG)(0xf<<2)) 	/* [5:2] NOT USED by the driver */
#define E_LP_AU		((ULONG)(1<<10))    /* Rx Done OK from Rx buff-mgr[1] */
#define E_LP_RI		((ULONG)(1<<1))     /* Rx Done OK from Rx buff-mgr[1] */
#define E_LP_TI		((ULONG)(1<<0))     /* Tx Done OK from Tx buff-mgr[0] */

/* 
 * Media Independent Interface 
 */

#define E_MII_MDIP	((ULONG)(0x00000010))
#define E_MII_MM	((ULONG)(0x00000008))
#define E_MII_MDO	((ULONG)(0x00000004))
#define E_MII_MDI	((ULONG)(0x00000002))
#define E_MII_MDC	((ULONG)(0x00000001))

/*
 * Derived bit masks
 */

/* 
 * When system error occurs, HW must be reset & driver should
 * be restarted. Now the hw needs to be reset whenever 
 * Tx/Rx under/over flow occurs.So these are also included in
 * the system error.
 *
 * Note: 
 * 1) Rx Overflow is sys err but there is no separate bit
 * in the interrupt enable reg. It is absorbed in the abnormal
 * interrupt summary bit.
 * 2) Rx underflow is not captured in both status and intr-enable regs.
 */
#define E_SYS_ERR    (E_STAT_TOF|E_STAT_TUF|E_STAT_TF|E_STAT_RO)
#define E_IE_SYS_ERR (E_IE_TOF  |E_IE_TUF  |E_IE_TF)

#define IS_RX_STOPED 0x0 /* the Rx process is not stopped*/



/******************************************************************************
 * EMAC's Media Independent Interface (MII) register defines, bit masks &
 * access macros.
 */

/*
 * EMAC MII's Management Interface register & bit-masks.
 */

#define E_MMI_OFFSET       ((ULONG)0x18) /*offset from EMAC base addr. */

/* 
 * Active edge is the edge of the Clk - EMDC -. The sampling of the EMDIO
 * is done, in input mode, using this edge 
 */
#define E_MDIP             ((ULONG)0x10) /* Active edge Mask.Bit[4].*/
#define E_MDIP_RISING      ((ULONG)0x10) /* Active edge = rising edge  */
#define E_MDIP_FALING      ((ULONG)0x00) /* Active edge = falling edge */
#define E_MDIP_DEF         E_MDIP_FALING /* HW default active edge     */

#define E_MM               ((ULONG)0x08) /* MDIO pin dire' mask.Bit[3] */
#define E_MM_INPUT         ((ULONG)0x08) /* direction = input          */
#define E_MM_OUTPUT        ((ULONG)0x00) /* direction = output         */
#define E_MM_DEF           E_MM_INPUT    /* HW default direction       */

#define E_MDO              ((ULONG)0x04) /* "Value to output" mask.Bit[2] */
#define E_MDO_VAL1         ((ULONG)0x04) /* Value=1 to output.            */
#define E_MDO_VAL0         ((ULONG)0x00) /* Value=0 to output.            */
#define E_MDO_DEF          E_MDO_VAL0    /* HW default value to output.   */

#define E_MDI              ((ULONG)0x02) /* "Value read" mask.Bit[1] */
#define E_MDI_DEF          ((ULONG)0x02) /* HW default value read.   */


#define E_MDC              ((ULONG)0x01) /* "Val to write to clk" mask.Bit[0]*/
#define E_MDC_VAL1         ((ULONG)0x01) /* Val=1 to write to the clk, MDC   */
#define E_MDC_VAL0         ((ULONG)0x00) /* Val=0 to write to the clk, MDC   */
#define E_MDC_DEF          E_MDC_VAL0    /* HW default-value to write to clk.*/

/*
 * Map the bit masks as per "functional" interpretation.
 * This mapping is used by the Phy driver.
 * 
 * PHY Modes:
 * ----------
 * Read : PHY is in read mode when it's reading data from the network cable.
 * Write: PHY is in write mode when it's writing data to the network cable.
 * 
 * MAC Modes:
 * ----------
 * Output: MAC should output data to PHY after setting PHY in write mode.
 * Input : MAC should input data from PHY after setting PHY in read mode.
 *
 * MAC & PHY Data Exchange:
 * ------------------------
 * 1. Data exchange between MAC & PHY is a bit (value = 0 or 1) at a time.
 * 2. MAC controlls the PHY's mode.
 * 3. MAC's modes are "exclusive" either input or output.
 * 4. PHY's modes are "exclusive" either read or write.
 */


/* First undefined all if defined ... */

#ifdef MAC_ADAPTER_ADDR
#undef MAC_ADAPTER_ADDR
#endif /* MAC_ADAPTER_ADDR */

#ifdef MAC_MDIO_REG
#undef MAC_MDIO_REG
#endif /* MAC_MDIO_REG */

#ifdef MDIO_CLK_MDIP
#undef MDIO_CLK_MDIP
#endif /* MDIO_CLK_MDIP */

#ifdef MDIO_RD_ENABLE
#undef MDIO_RD_ENABLE
#endif /* MDIO_RD_ENABLE */

#ifdef MDIO_WR_ENABLE
#undef MDIO_WR_ENABLE
#endif /* MDIO_WR_ENABLE */

#ifdef MDIO_RD_DATA
#undef MDIO_RD_DATA
#endif /* MDIO_RD_DATA */

#ifdef MDIO_WR_DATA_1
#undef MDIO_WR_DATA_1
#endif /* MDIO_WR_DATA_1 */

#ifdef MDIO_WR_DATA_0
#undef MDIO_WR_DATA_0
#endif /* MDIO_WR_DATA_0 */

#ifdef MDIO_CLK
#undef MDIO_CLK
#endif /* MDIO_CLK */           

/* ... Now define them for Super Pipe */

#define MAC_ADAPTER_ADDR  EMAC_BASE_ADDR
#define MAC_MDIO_REG      E_MMI_OFFSET
#define MDIO_CLK_MDIP     E_MDIP_DEF	
#define MDIO_RD_ENABLE    (E_MM_INPUT | MDIO_CLK_MDIP) /* Set PHY in read mode.*/
#define MDIO_WR_ENABLE    E_MM_OUTPUT /* Set PHY in write mode.*/
#define MDIO_RD_DATA      E_MDI  /* Data read by MAC.*/
#define MDIO_WR_DATA_1    E_MDO_VAL1  /* MAC writes 1. */
#define MDIO_WR_DATA_0    E_MDO_VAL0  /* MAC writes 0. */
#define MDIO_CLK          E_MDC_VAL1  /* MAC sends clock, MDC pulse.*/
                          /* Why not E_MDC_VAL0? 
                             While writting data to MII, the Phy driver
                             either needs to send clk=0 or clk=1 ORed with
                             the other flags - eg RD_ENABLE|CLK|.
                             So clk=0 case is automatically taken care of
                             if driver doesn't "OR" the flags with the CLK.
                             eg. RD_ENABLE   - no "OR"ing with CLK.
                           */



#endif /* __EMAC_REGS_H */
