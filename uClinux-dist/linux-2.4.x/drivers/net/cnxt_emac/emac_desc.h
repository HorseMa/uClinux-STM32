/****************************************************************************
*
*	Name:			emac_desc.h
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

#ifndef _EMAC_DESC_H
#define _EMAC_DESC_H



/* 
 * receive descriptor 
 */


/* 
 * Recieve status (RSTAT) - QWORD 
 */

#define RSTAT_FL	((ULONG)(0xffff0000)) /* Frame len including CRC.
													 [31-16]			*/
#define RSTAT_ES	((ULONG)(0x00008000)) /* Error Summary.			 
													 [15]				*/
/* The following Receive status defines are unused */
#define RSTAT_OM	((ULONG)(0x00003000)) /* Operating mode.		 
													 [13-12]			*/
#define RSTAT_TS	((ULONG)(0x00000800)) /* Too short=1, Pkt<64 bytes.
													 [11]				*/
#define RSTAT_MF	((ULONG)(0x00000400)) /* Multicast frame=1.		 
													 [10]				*/
#define RSTAT_TL	((ULONG)(0x00000080)) /* Too long=1, Packet>1518.
													 [7]				*/
#define RSTAT_LC	((ULONG)(0x00000040)) /* Late collision=1.		 
													 [6]				*/
#define RSTAT_OFT	((ULONG)(0x00000020)) /* Frame type.			 
													 [5]				*/
#define RSTAT_RW	((ULONG)(0x00000010)) /* Receive watchdog=1.	 
													 [4]				*/
#define RSTAT_DB	((ULONG)(0x00000004)) /* Dribble bit=1.			 
													 [2]				*/
#define RSTAT_CE	((ULONG)(0x00000002)) /* CRC error.				 
													 [1]				*/
#define RSTAT_OF	((ULONG)(0x00000001)) /* RMAC Fifo overflow=1.	 
													 [0]				*/
/* 
 * receive descriptor describing control information
 */

/* Ownership bit of the descriptor indicates:
 * if reset: the packet received not yet processed.
 * if set  : The pkt processed and now the desc is available.
 *
 * Note: The "processing" here is done by a function runing in tNetTask
 * context. The netJobAdd adds the addr of this func in the tNetTask-queue.
 * After processing a packet, the func sets it.
 */
#define RCTRL_OWN		0x80000000	
#define RCTRL_RER		0x02000000	/* recv end of ring */


/*
 * Length of Received frame related defines.
 */

#define RX_FRM_LEN_MSK		RSTAT_FL	/* Frame length mask */
#define RX_FRM_LEN_SHF		16			/* bit-shift required to obtain
										   the Frame length  */
#define RX_FRM_LEN_GET(x)	(((x) & RX_FRM_LEN_MSK) >> RX_FRM_LEN_SHF)
#define RX_FRM_LEN_SET(x)	(((x) << RX_FRM_LEN_SHF) & RX_FRM_LEN_MSK)

/*
 * transmit descriptor 
 */

/*
 * Transmit Descriptor describes control information.
 */

#define TCTRL_RDY		((ULONG)(0x00010000)) /* Frame ready to be transmitted
												 when this bit is 1.
												 [16]		*/
#define TCTRL_TLEN		((ULONG)(0x0000fff0)) /* Transmit frame length in bytes.
												 [15:4]		*/
#define TCTRL_SET		((ULONG)(0x00000004)) /* Current frame is a setup frame
												 when this bit is 1.
												 [2]		*/
#define TCTRL_DPD		((ULONG)(0x00000002)) /* Disable Tx padding	
												 [1]		*/
#define TCTRL_AC		((ULONG)(0x00000001)) /* Disable CRC append 
												 [0]		*/

#define TX_FRM_LEN_MSK		TCTRL_TLEN	/* Frame length mask */
#define TX_FRM_LEN_SHF		4			/* bit-shift required to obtain
										   the Frame length  */
#define TX_FRM_LEN_SET(x)	(((x) << TX_FRM_LEN_SHF) & TX_FRM_LEN_MSK)

/*
 * Transmit Status (TSTAT) - QWORD 
 */

#define TSTAT_TDN		((ULONG)(0x80000000)) /* When set indicates transmit
												 completed successfully
												 (from the buffer manager)
														[31]		*/
#define TSTAT_TU		((ULONG)(0x40000000)) /* Transmit stopped 
												 (descriptor not ready)
														[30]		*/
#define TSTAT_TOF		((ULONG)(0x00010000)) /* Transmit buffer manager
												 FIFO Overflow when 1.
														[16]		*/
#define TSTAT_TUF		((ULONG)(0x00008000)) /* Transmit buffer manager
												 FIFO underflow when 1.(MIB11)
														[15]		*/
#define TSTAT_ED		((ULONG)(0x00004000)) /* Excessive Tx deferral
												 (Tx deffered longer then
												  8192 x 400 ns.
														[14]		*/
#define TSTAT_DF		((ULONG)(0x00002000)) /* Frame has been defered
												 atlease once (MIB8)
														[13]		*/
#define TSTAT_CD		((ULONG)(0x00001000)) /* Frame transmit completed
												 successfully (from MII).
														[12]		*/
#define TSTAT_ES		((ULONG)(0x00000800)) /* Transmit Error Summary. 
												 TF|C16|LC|NCRS|LCRS|TJT
														[11]			*/
#define TSTAT_RLD		((ULONG)(0x00000400)) /* Tx FIFO reload/abort during
												 frame (includes collision)
														[10]			*/
#define TSTAT_TF		((ULONG)(0x00000200)) /* Tx fault (or unexpected 
												 transmit data request during
												 frame (from MII).
														[9]			*/
#define TSTAT_TJT		((ULONG)(0x00000100)) /* Transmit jabber timeout: set
												 when the jabber timer expires.
												 E_NA_HUJ must be configured 
												 for this bit to function.
														[8]			*/
#define TSTAT_NCRS		((ULONG)(0x00000080)) /* No carrier (ECRS pin never 
												 gone high) during the transmit
												 of the frame.
														[7]			*/
#define TSTAT_LCRS		((ULONG)(0x00000040)) /* Carrier was lost (ECRS pin 
												 gone low) at least once during
												 the transmit of the frame. 
												 (MIB18).
														[6]			*/
#define TSTAT_C16		((ULONG)(0x00000020)) /* 16 or more collisions have 
												 occured during the frame 
												 transmit.
														[5]			*/
#define TSTAT_LC		((ULONG)(0x00000010)) /* Late collision (after 64th 
												 byte) has occured during the
												 frame transmit (MIB16)
														[4]			*/
#define TSTAT_CC		((ULONG)(0x0000000F)) /* Transmit collision count of
												 the frame. Resets after the
												 frame is transmitted 
												 successfully (MIB9). Increments
												 with every collision of the 
												 current frame.
														[3:0]			*/


#define TD_STAT_CC_MSK		TSTAT_CC
#define	TD_STAT_CC_VAL(X)	(((X) & TD_STAT_CC_MSK) >> 3)    


/* 
 * This macro assumes that the status was 0 before Tx started for this TxD.
 * This macro returns true if Tx was done successfully or unsuccessfully.
 * Note in the current version, EMAC HW doesn't clear this bit.
 */
#ifdef HW_CLRS_TXD_RDY
#define IS_TXD_OWNED_BY_HW(pTxD) ((pTxD)->control & (TCTRL_RDY))
#else
#define IS_TXD_OWNED_BY_HW(pTxD) ( ((pTxD)->status == 0) && \
                                   ((pTxD)->control & (TCTRL_RDY)))
#endif


#endif  /* _EMAC_DESC_H */
