/*****************************************************************************
 *  
 * linux/include/asm-arm/arch-ep93xx/regs_dma.h
 *
 *  Register definitions for the ep93xx dma channel registers.
 *
 *  Copyright (C) 2003 Cirrus Logic
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
 ****************************************************************************/
#ifndef _REGS_DMA_H_
#define _REGS_DMA_H_

/*****************************************************************************
 * 0x8000.0000 -> 0x8000.003C M2P Channel 0 Registers (Tx) 
 * 0x8000.0040 -> 0x8000.007C M2P Channel 1 Registers (Rx) 
 * 0x8000.0080 -> 0x8000.00BC M2P Channel 2 Registers (Tx)  
 * 0x8000.00C0 -> 0x8000.00FC M2P Channel 3 Registers (Rx) 
 * 0x8000.0100 -> 0x8000.013C M2M Channel 0 Registers      
 * 0x8000.0140 -> 0x8000.017C M2M Channel 1 Registers      
 * 0x8000.0180 -> 0x8000.01BC Not Used                     
 * 0x8000.01C0 -> 0x8000.01FC Not Used                     
 * 0x8000.0200 -> 0x8000.023C M2P Channel 5 Registers (Rx) 
 * 0x8000.0240 -> 0x8000.027C M2P Channel 4 Registers (Tx) 
 * 0x8000.0280 -> 0x8000.02BC M2P Channel 7 Registers (Rx) 
 * 0x8000.02C0 -> 0x8000.02FC M2P Channel 6 Registers (Tx) 
 * 0x8000.0300 -> 0x8000.033C M2P Channel 9 Registers (Rx) 
 * 0x8000.0340 -> 0x8000.037C M2P Channel 8 Registers (Tx) 
 * 0x8000.0380 DMA Channel Arbitration register            
 * 0x8000.03C0 DMA Global Interrupt register               
 * 0x8000.03C4 -> 0x8000.03FC Not Used                     
 *
 *
 * Internal M2P/P2M Channel Register Map                   
 *
 * Offset Name      Access  Bits Reset Value               
 * 0x00   CONTROL   R/W     6    0                         
 * 0x04   INTERRUPT R/W TC* 3    0                         
 * 0x08   PPALLOC   R/W     4    channel dependant         
 *                               (see reg description)     
 * 0x0C   STATUS    RO      8    0                         
 * 0x10   reserved                                         
 * 0x14   REMAIN    RO      16   0                         
 * 0X18   Reserved                                         
 * 0X1C   Reserved                                         
 * 0x20   MAXCNT0   R/W     16   0                         
 * 0x24   BASE0     R/W     32   0                         
 * 0x28   CURRENT0  RO      32   0                         
 * 0x2C   Reserved                                         
 * 0x30   MAXCNT1   R/W     16   0                         
 * 0x34   BASE1     R/W     32   0                         
 * 0X38   CURRENT1  RO      32   0                         
 * 0X3C   Reserved                                         
 *                                                         
 * M2M Channel Register Map                                
 * Offset Name         Access   Bits Reset Value           
 *                                                         
 * 0x00   CONTROL      R/W      22   0                     
 * 0x04   INTERRUPT    R/W TC*  3    0                     
 * 0x08   Reserved                                         
 * 0x0C   STATUS       R/W TC*  14   0                     
 * 0x10   BCR0         R/W      16   0                     
 * 0x14   BCR1         R/W      16   0                     
 * 0x18   SAR_BASE0    R/W      32   0                     
 * 0x1C   SAR_BASE1    R/W      32   0                     
 * 0x20   Reserved                                         
 * 0x24   SAR_CURRENT0 RO       32   0                     
 * 0x28   SAR_CURRENT1 RO       32   0                     
 * 0x2C   DAR_BASE0    R/W      32   0                     
 * 0x30   DAR_BASE1    R/W      32   0                     
 * 0x34   DAR_CURRENT0 RO       32   0                     
 * 0X38   Reserved                                         
 * 0X3C   DAR_CURRENT1 RO       32   0                          
 * * Write this location once to clear the bit (see        
 * Interrupt/Status register description for which bits    
 * this rule applies to).
 *                                  
 ****************************************************************************/

#ifndef __ASSEMBLY__ 
/*
 * DMA Register Base addresses
 */
static unsigned int const DMAM2PChannelBase[10] =
{
    DMA_M2P_TX_0_BASE,
    DMA_M2P_RX_1_BASE,
    DMA_M2P_TX_2_BASE,
    DMA_M2P_RX_3_BASE,
    DMA_M2P_TX_4_BASE,
    DMA_M2P_RX_5_BASE,
    DMA_M2P_TX_6_BASE,
    DMA_M2P_RX_7_BASE,
    DMA_M2P_TX_8_BASE,
    DMA_M2P_RX_9_BASE
};

static unsigned int const DMAM2MChannelBase[2] = 
{
    DMA_M2M_0_BASE,
    DMA_M2M_1_BASE
};

#endif /* __ASSEMBLY__ */

/*----------------------------------------------------------------------------------*/
/* M2P Registers                                                                    */
/*----------------------------------------------------------------------------------*/
/*
 * M2P CONTROL register bit defines 
 */
#define CONTROL_M2P_STALLINTEN      0x00000001	    /* Enables the STALL interrupt  */
#define CONTROL_M2P_NFBINTEN        0x00000002	    /* Enables the NFB interrupt    */
#define CONTROL_M2P_CHERRORINTEN    0x00000008      /* Enables the ChError interrupt*/
#define CONTROL_M2P_ENABLE		    0x00000010      /* Enables the channel          */
#define CONTROL_M2P_ABRT		    0x00000020      /* Determines how DMA behaves in*/ 
			                                        /* NEXT state with peripheral   */
                                                    /* error                        */
			                                        /* 0: NEXT -> ON, ignore error  */
			                                        /* 1: NEXT -> STALL, disable ch.*/ 
#define CONTROL_M2P_ICE			    0x00000040      /* Ignore Channel Error         */

/*
 * M2P INTERRUPT register bit defines
 */
#define INTERRUPT_M2P_STALLINT      0x00000001	    /* Indicates channel stalled.   */
#define INTERRUPT_M2P_NFBINT        0x00000002		/* Indicates channel is hungry. */
#define INTERRUPT_M2P_CHERRORINT    0x00000008	    /* Peripheral detects error     */


/*
 * STATUS register bit defines
 */
#define STATUS_M2P_STALL            0x00000001		/* A '1' indicates channel is       */
                                                    /* stalled                          */
#define STATUS_M2P_NFB			    0x00000002      /* A '1' indicates channel has moved*/
			                                        /* from NEXT state to ON state, but */
			                                        /* waiting for next buffer to be    */
                                                    /* programmed.                      */
#define STATUS_M2P_CHERROR		    0x00000008      /* Enables the ChError interrupt    */
#define STATUS_M2P_CURRENT_MASK     0x00000030      /* Current state of the FSM         */
#define STATUS_M2P_CURRENT_SHIFT    4
#define STATUS_M2P_NEXTBUFFER	    0x00000040      /* Informs the int handler after an */
			                                        /* NFB int which pair of maxcnt and */
                                                    /* base regs to update.             */
#define STATUS_M2P_BYTES_MASK       0x0000f800 		/* number of valid DMA data         */
#define STATUS_M2P_BYTES_SHIFT      7               /* currently in                     */
								        		    /* packer/unpacker                  */

#define STATUS_M2P_DMA_NO_BUF		0x00000000
#define STATUS_M2P_DMA_BUF_ON		0x00000010
#define STATUS_M2P_DMA_BUF_NEXT		0x00000020

/*
 * Register masks to mask off reserved bits after reading register.
 */
#define M2P_MASK_PPALLOC            0x0000000f
#define M2P_MASK_REMAIN             0x0000ffff
#define M2P_MASK_MAXCNT0            0x0000ffff
#define M2P_MASK_BASE0              0xffffffff
#define M2P_MASK_CURRENT0           0xffffffff
#define M2P_MASK_MAXCNT1            0x0000ffff
#define M2P_MASK_BASE1              0xffffffff
#define M2P_MASK_CURRENT1           0xffffffff


/*----------------------------------------------------------------------------------*/
/* M2M Registers                                                                    */
/*----------------------------------------------------------------------------------*/

#define CONTROL_M2M_STALLINTEN	0x00000001  /* Enables the STALL interrupt                     */
#define CONTROL_M2M_SCT			0x00000002  /* Source Copy Transfer. Setup a                   */
										    /* block transfer from 1 memory source             */
										    /* location.                                       */
#define CONTROL_M2M_DONEINTEN	0x00000004  /* Enables the DONE interrupt which                */
										    /* indicates if the xfer completed                 */
										    /* successfully                                    */
#define CONTROL_M2M_ENABLE		0x00000008  /* Enables the channel                             */
#define CONTROL_M2M_START		0x00000010  /* Initiates the xfer. 'software trigger'          */
#define CONTROL_M2M_BWC_MASK	0x000001e0  /* Bandwidth control. Indicate number of           */
#define CONTROL_M2M_BWC_SHIFT   5			/* bytes in a transfer.                            */
#define CONTROL_M2M_PW_MASK		0x00000600  /* Peripheral width. Used for xfers                */
#define CONTROL_M2M_PW_SHIFT    9			/* between memory and external peripheral.         */
										    /* 00: byte, 01: halfword, 10: word.               */
#define CONTROL_M2M_DAH			0x00000800  /* Destination Address Hold                        */
#define CONTROL_M2M_SAH			0x00001000  /* Source Address Hold                             */
#define CONTROL_M2M_TM_MASK     0x00006000  /* Transfer Mode. 00: sw triggered,                */
#define CONTROL_M2M_TM_SHIFT    13			/* 01: hw initiated M2P, 01: hw initiated P2M      */
#define CONTROL_M2M_ETDP_MASK	0x00018000  /* End-of-Transfer/Terminal Count pin              */
#define CONTROL_M2M_ETDP_SHIFT  15		    /* direction and polarity.                         */
#define CONTROL_M2M_DACKP		0x00020000  /* DMA acknowledge pin polarity                    */

#define CONTROL_M2M_DREQP_MASK  0x00180000	/* DMA request pin polarity. must be set           */
#define CONTROL_M2M_DREQP_SHIFT 19			/* before enable bit.                              */
#define CONTROL_M2M_NFBINTEN	0x00200000  /* Enables generation of the NFB interrupt.        */
#define CONTROL_M2M_RSS_MASK    0x00c00000	/* Request source selection:                       */
#define CONTROL_M2M_RSS_SHIFT	22			/*		000 - External DReq[0]                     */
										    /*		001 - External DReq[1]                     */
										    /*		01X - Internal SSPRx                       */
										    /*		10X - Internal SSPTx                       */
										    /*		11X - Internal IDE                         */
#define CONTROL_M2M_NO_HDSK		0x01000000  /* No handshake.  When set the peripheral doesn't  */
										    /* require the regular handshake protocal. Must    */
									    	/* be set for SSP and IDE operations, optional     */
										    /* for external peripherals.                       */
#define CONTROL_M2M_PWSC_MASK   0xfe000000	/* Peripheral wait states count. Gives the latency */
#define CONTROL_M2M_PWSC_SHIFT	25			/* (in PCLK cycles) needed by the peripheral to    */
								    		/* deassert its' request once the M2M xfer w/ DMA  */
									    	/* is complete.                                    */

/*
 * M2M INTERRUPT register bit defines
 */
#define INTERRUPT_M2M_STALLINT	0x00000001	/* Stall interrupt indicates channel stalled. */
#define INTERRUPT_M2M_DONEINT	0x00000002	/* Transaction done.                          */
#define INTERRUPT_M2M_NFBINT	0x00000004	/* Next frame buffer interrupt indicates      */
											/* channel requires a new buffer              */



/*
 * M2M STATUS register bit defines
 */
#define STATUS_M2M_STALL		0x00000001  /* A '1' indicates channel is stalled           */
#define STATUS_M2M_CURRENTSTATE_MASK  0x0000003e  /* Indicates state of M2M Channel control       */
#define STATUS_M2M_CURRENTSTATE_SHIFT 1		/* FSM (0-2):                                   */
										    /*	000 - IDLE, 001 - STALL, 010 - MEM_RD,      */
										    /*  011 - MEM_WR, 100 - BWC_WAIT                */
										    /* and M2M buffer FSM (3-2):                    */
										    /* 	00 - NO_BUF, 01 - BUF_ON, 10 - BUF_NEXT     */
#define STATUS_M2M_DONE		    0x00000040  /* Transfer completed successfully if 1.        */
#define STATUS_M2M_TCS_MASK		0x00000180  /* Terminal Count status. Indicates whether or  */
#define STATUS_M2M_TCS_SHIFT    7			/* or not the actual byte count reached         */
								    		/* programmed limit for buffer descriptor       */
#define STATUS_M2M_EOTS_MASK    0x00000600  /* End-of-Transfer status for buffer            */
#define STATUS_M2M_EOTS_SHIFT   9
#define STATUS_M2M_NFB			0x00000800  /* A '1' indicates channel has moved            */
										    /* from NEXT state to ON state, but	the next    */
										    /* byte count reg for next buffer has not been  */
										    /* programmed yet.                              */
#define STATUS_M2M_NB			0x00001000  /* NextBuffer status. Informs NFB service       */
										    /* routine, after NFB int, which pair of buffer */
										    /* descriptor registers is free to update.      */
#define STATUS_M2M_DREQS		0x00002000  /* DREQ status.  Reflects the status of the     */
										    /* synchronized external peripherals DMA        */
										    /* request signal.                              */

/*
 * Register masks to mask off reserved bits after reading register.
 */
#define M2M_MASK_BCR0             0x0000ffff
#define M2M_MASK_BCR1             0x0000ffff
#define M2M_MASK_SAR_BASE0        0xffffffff
#define M2M_MASK_SAR_BASE1        0xffffffff
#define M2M_MASK_SAR_CURRENT0     0xffffffff
#define M2M_MASK_SAR_CURRENT1     0xffffffff
#define M2M_MASK_DAR_BASE0        0xffffffff
#define M2M_MASK_DAR_BASE1        0xffffffff
#define M2M_MASK_DAR_CURRENT0     0xffffffff
#define M2M_MASK_DAR_CURRENT1     0xffffffff


#endif /* _REGS_DMA_H_ */
