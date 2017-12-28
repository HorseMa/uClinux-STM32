/*
* Copyright c                  Realtek Semiconductor Corporation, 2006
* All rights reserved.
* 
* Program : NIC driver for RTL865x serious.
* Abstract : 
* Creator : Yi-Lun Chen (chenyl@realtek.com.tw)
* Author :  
*
*/

/*	@doc RTL8651_SWNIC_API

	@module rtl865xc_swNic.c - RTL8651 Home gateway controller Switch NIC driver documentation	|
	This document explains the function descriptions of the NIC driver module.
	@normal Yi-Lun Chen (chenyl@realtek.com.tw) <date>

	Copyright <cp>2006 Realtek<tm> Semiconductor Cooperation, All Rights Reserved.

 	@head3 List of Symbols |
 	Here is a list of all functions and variables in this module.

 	@index | RTL8651_SWNIC_API
*/

#ifdef RTL865X_MODEL_USER
	#include <errno.h>
	#include "rtl_glue.h"
	#include "rtl_utils.h"
#else
	#ifdef __linux__
		#include <linux/config.h>
		#include <linux/mm.h>
		#include <linux/interrupt.h>
	#endif
	#include <asm/rtl865x/interrupt.h>
#endif
#include "rtl865x/rtl_types.h"
#include "rtl865x/rtl_glue.h"
#include "rtl865x/mbuf.h"
#include "rtl865xc_swNic.h"
#include "rtl865x/assert.h"
#include "rtl865x/asicRegs.h"
#include "rtl865x/rtl8651_layer2.h"

#include "rtl865x/rtl865xC_tblAsicDrv.h"

#include "rtl865x/rtl_utils.h"

 int8 swNic_Id[] = "$Id: rtl865xc_swNic.c,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $";


/* =========================================================================
		Variable / internal function/macro Declarations
    ========================================================================= */

/* ------------------------------------------------------------------------------------------
	Ring management
    ====================================================================== */

/* RX Ring */
uint32*	rxPkthdrRing[RTL865X_SWNIC_RXRING_MAX_PKTDESC];					/* Point to the starting address of RX pkt Hdr Ring */
uint32	rxPkthdrRingCnt[RTL865X_SWNIC_RXRING_MAX_PKTDESC];				/* Total pkt count for each Rx descriptor Ring */
uint32	rxPkthdrRingMaxIndex[RTL865X_SWNIC_RXRING_MAX_PKTDESC];			/* Max index for each Rx descriptor Ring */
uint32	rxPkthdrRingIndex[RTL865X_SWNIC_RXRING_MAX_PKTDESC];				/* Current Index for each Rx descriptor Ring */
uint32	rxPkthdrRingLastRecvIndex[RTL865X_SWNIC_RXRING_MAX_PKTDESC];		/* Last Received Packet in each RX Descriptor Ring */
uint32	rxPkthdrRingLastReclaimIndex[RTL865X_SWNIC_RXRING_MAX_PKTDESC];	/* Last Reclaimed packet index in each RX descriptor Ring */
static uint32 rxPkthdrRingRxDoneMask;

static int32	rxPkthdrRingRunoutIndex[RTL865X_SWNIC_RXRING_MAX_PKTDESC];	/* Record the index which RUNOUT occurred */
static uint32	rxPkthdrRingRunoutEntry[RTL865X_SWNIC_RXRING_MAX_PKTDESC];	/* Record the entry which RUNOUT occurred */
static uint32	rxPkthdrRingRunoutMask;

/* <-------------------------- RX Ring DRR queue ---------------------------->*/
int32 drrenable;
uint32	rxPkthdrRingRefillByDRR[RTL865X_SWNIC_RXRING_MAX_PKTDESC] = {600,500,400,300,200,100};	/* How many bytes refill to receive of each ring by DRR */	
uint32	rxPkthdrRingReceiveByDRR[RTL865X_SWNIC_RXRING_MAX_PKTDESC] = {0,0,0,0,0,0};	/* How many bytes is able to receive of each ring by DRR */

/*
	TX Ring

	Architecture
	=====================================================================


		|<----------------------------- txPkthdrRingCnt ----------------------------->|

		[ Tx entry ][ Tx entry ][ Tx entry ][ Tx entry ][ Tx entry ][ Tx entry ][ Tx entry ][ Tx entry ]
		     Idx 0         Idx 1         Idx 2       ................................................................. Idx (txPkthdrRingMaxIndex)
		^
		|____ txPkthdrRing
	

	Mechanism
	=====================================================================

	Several pointers manage the TX ring:

		txPkthdrRingDoneIndex : Point to the NEXT entry for last-recycled-TX-ring-entry.
		txPkthdrRingFreeIndex : Point to the NEXT entry for last-TXed-ring-entry

		(1) Empty state (DONE == FREE) / Initial state (DONE == FREE == IDX 0)
		---------------------------------------------------------------------------------

		[   empty  ][   empty  ][   empty  ][   empty  ][   empty  ][   empty  ][   empty  ][   empty  ]
	 	   (DONE)
		   (FREE)

		(2) FULL state (DONE points the next entry of FREE)
			=> So there ALWAYS be least 1 entry is empty.
		---------------------------------------------------------------------------------
		
		[  TxProc  ][  TxProc  ][  TxProc  ][  empty  ][  TxProc  ][  TxProc  ][  TxProc  ][  TxProc  ]
		                                                    (FREE)     (DONE)

		(3) Packet TX (Put TX-packet into FREE and move FREE)
		---------------------------------------------------------------------------------

		Before:
		---------------
		[   TxProc  ][   TxProc  ][   TxProc  ][   empty  ][   empty  ][   empty  ][   empty  ][   empty  ]
		                                      (DONE)
		                                                       (FREE)

		After:
		---------------
		[   TxProc  ][   TxProc  ][   TxProc  ][   TxProc  ][   empty  ][   empty  ][   empty  ][   empty  ]
		                                      (DONE)
		                                                                        (FREE)

		(4) Sent-packet Recycling
			(	Recycle packet point by DONE if it has sent by ASIC and
				move DONE until its FREE [DONE == FREE means TX ring is empty]	)
		---------------------------------------------------------------------------------

		Case 1 - Some TXed packets are processed by ASIC (TxDone) , and some are not yet (TxProc).
			=> Recycling would recycle all packets which is processed.

		Before:
		---------------
		[   empty  ][ TxDone  ][ TxDone  ][ TxDone  ][ TxDone  ][ TxProc  ][   empty  ][   empty  ]
		                    (DONE)
		                                                                                                       (FREE)

		After:
		---------------
		[   empty  ][   empty  ][   empty  ][   empty  ][   empty  ][ TxProc  ][   empty  ][   empty  ]
		                                                                                    (DONE)
		                                                                                                       (FREE)


		Case 2 - All TXed packets are processed by ASIC (TxDone).
			=> Recycling would recycle all packets which is processed until TX ring is empty.

		Before:
		---------------
		[   empty  ][ TxDone  ][ TxDone  ][ TxDone  ][ TxDone  ][ TxDone  ][   empty  ][   empty  ]
		                    (DONE)
		                                                                                                       (FREE)

		After:
		---------------
		[   empty  ][   empty  ][   empty  ][   empty  ][   empty  ][   empty  ][   empty  ][   empty  ]
		                                                                                                       (DONE)
		                                                                                                       (FREE)



*/
uint32*	txPkthdrRing[RTL865X_SWNIC_TXRING_MAX_PKTDESC];				/* Point to the starting address of TX pkt Hdr Ring */
uint32	txPkthdrRingCnt[RTL865X_SWNIC_TXRING_MAX_PKTDESC];			/* Total pkt count for each Tx descriptor Ring */
uint32	txPkthdrRingMaxIndex[RTL865X_SWNIC_TXRING_MAX_PKTDESC];		/* To remember the MAX index of Tx Ring */
uint32	txPkthdrRingDoneIndex[RTL865X_SWNIC_TXRING_MAX_PKTDESC];		/* Point to the NEXT entry for the entry which the packet has be sent and recycled  */
uint32	txPkthdrRingFreeIndex[RTL865X_SWNIC_TXRING_MAX_PKTDESC];		/* Point to the entry can be set to SEND packet */
uint32	txPkthdrRingLastIndex[RTL865X_SWNIC_TXRING_MAX_PKTDESC];

#define txPktHdrRingFull(idx)	(((txPkthdrRingFreeIndex[idx] + 1) & (txPkthdrRingMaxIndex[idx])) == (txPkthdrRingDoneIndex[idx]))

/* Mbuf */
uint32*	rxMbufRing;														/* Point to the starting address of MBUF Ring */
uint32	rxMbufRingCnt;													/* Total MBUF count */
uint32	rxMbufRingIndex;
uint32	rxMbufRingMaxIndex;

/* ------------------------------------------------------------------------------------------
	Packet process generic function
    ====================================================================== */
uint32 _swNic_getNextRxPkthdrProcIndexDRR(uint32 *rxPkthdrRingProcIndex_p);
uint32 _swNic_getNextRxPkthdrProcIndex(uint32 *rxPkthdrRingProcIndex_p);
static inline uint32 _swNic_checkRunout(void);
static int32 _swNic_isrTxRecycle(int32 dummy, int32 txPkthdrRingIndex);
static int32 _swNic_txRxSwitch(int32 tx, int32 rx);
static int32 _swNic_installRxCallBackFunc(	proc_input_pkt_funcptr_t rxPreProcFunc,
												proc_input_pkt_funcptr_t rxProcFunc,
												proc_input_pkt_funcptr_t* orgRxPreProcFunc_p,
												proc_input_pkt_funcptr_t* orgRxProcFunc_p);

/* ------------------------------------------------------------------------------------------
	Top HALF
    ====================================================================== */
uint32	swNicWriteTXFN = FALSE;
uint32	rxEnable = FALSE;
uint32	txEnable = FALSE;

static int32 _swNic_init(	uint32 userNeedRxPkthdrRingCnt[RTL865X_SWNIC_RXRING_MAX_PKTDESC],
						uint32 userNeedRxMbufRingCnt,
						uint32 userNeedTxPkthdrRingCnt[RTL865X_SWNIC_TXRING_MAX_PKTDESC],
						uint32 clusterSize,
						proc_input_pkt_funcptr_t rxCallBackFunc, 
						proc_input_pkt_funcptr_t rxPreProcFunc);
static int32 _swNic_hwSetup(void);
static void _swNic_reset(void);
static int32 _swNic_descRingCleanup(void);
static int32 _swNic_getRingSize(uint32 ringType, uint32 *ringSize, uint32 ringIdx);
static int32 _swNic_enableSwNicWriteTxFN(uint32 enable);

/* ------------------------------------------------------------------------------------------
	Bottom HALF
    ====================================================================== */
static proc_input_pkt_funcptr_t installedPreProcessInputPacket;	/* Call-back proces to pre-process RX packet */
static proc_input_pkt_funcptr_t installedProcessInputPacket;		/* Call-back proces to process RX packet */

static uint32	rxIntNum;
static uint32	linkChanged;
static uint32	txIntNum;
static uint32	rxMbufRunoutNum;
static uint32	rxPktErrorNum;
static uint32	txPktErrorNum;
static uint32	rxPktCounter;
static uint32	txPktCounter;
static uint32	rxPkthdrRunoutNum;
static uint32	rxPkthdrRunoutSolved;
static uint32	maxCountinuousRxCount;

static int32 _swNic_linkChgIntHandler(void);
static inline void _swNic_updateRxPendingMask(void);
static void _swNic_rxRunoutHandler(void);
static int32 _swNic_recvLoop(uint32 rxPkthdrRingProcIndex, uint32 rxPkthdrRingProcBurstSize);
static int32 _swNic_rxRunoutTxPending(struct rtl_pktHdr *pkthdr_p);

/* ------------------------------------------------------------------------------------------
	NIC Tx-Alignment support
	( For RTL865xB Cut A/B only )
    ====================================================================== */
#ifdef SWNIC_TXALIGN
static int32 _swNic_TxAlign_Support = FALSE;
static void _swNic_txAlign(struct rtl_pktHdr *pPkt, struct rtl_mBuf *pMbuf);
#endif

/* ------------------------------------------------------------------------------------------
	NIC Debugging
    ====================================================================== */

#ifdef SWNIC_DEBUG
uint32 nicDbgMesg = NIC_DEBUGMSG_DEFAULT_VALUE;
#endif

/* ------------------------------------------------------------------------------------------
	Rome Real support
    ====================================================================== */

#ifdef CONFIG_RTL865X_ROMEREAL
	//Turn on In-memory "ethereal" like pkt sniffing code.
	#define START_SNIFFING rtl8651_romerealRecord
#else
	#define START_SNIFFING(x,y) do{}while(0)	
#endif


/* ------------------------------------------------------------------------------------------
	NIC Burst size Early stop receiving support
    ====================================================================== */
#ifdef SWNIC_EARLYSTOP
static int32 nicRxPktBurstSize;
static int32 nicRxPktOneshotCnt;
static int32 nicRxEarlyStopTriggered;
static uint32 nicRxConcernedGisrInteruptMask;
#endif

/* ------------------------------------------------------------------------------------------
	CLE Shell support
    ====================================================================== */
#ifdef CONFIG_RTL865X_CLE

/* For Loopback test */
static uint32 loopBackEnable = FALSE;
static uint32 loopBackOrgMSCR = 0;
static int32 loopbackPortMask = 0;
static proc_input_pkt_funcptr_t loopBackOrgRxPreProcFunc = NULL;
static proc_input_pkt_funcptr_t loopBackOrgRxProcFunc = NULL;

/* For HUB mode */
static uint32 hubEnable = FALSE;
static uint32 hubOrgMSCR = 0;
static uint32 hubMbrMask = 0;
static proc_input_pkt_funcptr_t hubOrgRxPreProcFunc = NULL;
static proc_input_pkt_funcptr_t hubOrgRxProcFunc = NULL;

/* For Packet generator mode */
#ifdef NIC_DEBUG_PKTGEN
#define NIC_PKTGEN_DEFAULT_PKTCNT		1
#define NIC_PKTGEN_DEFAULT_TXPORTMASK	0x1f	/* b'000011111 */
#define NIC_PKTGEN_CONTENT_LEN			64

static uint32 pktGenCount = NIC_PKTGEN_DEFAULT_PKTCNT;
static uint32 pktGenTxPortMask = NIC_PKTGEN_DEFAULT_TXPORTMASK;
static uint32 pktGenMinPktLen = NIC_PKTGEN_CONTENT_LEN;
static uint32 pktGenMaxPktLen = NIC_PKTGEN_CONTENT_LEN;
static char pktGenContent[NIC_PKTGEN_CONTENT_LEN] = {0};
static char defaultPktGenContent[NIC_PKTGEN_CONTENT_LEN] = {	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,								/* Broadcast DA */
															0x00, 0x52, 0x54, 0x4b, 0x86, 0x52,								/* SA : RTK8652 */
															0x86, 0xdd,													/* IPv6 !? Gotten from AIR */
															0x49, 0x20, 0x6c, 0x6f, 0x76, 0x65, 0x20, 0x52, 0x54, 0x4b, 0x20,	/* "I love RTK " */
															0x53, 0x44, 0x32, 0x20, 0x47, 0x61, 0x74, 0x65, 0x77, 0x61, 0x79,	/* "SD2 Gateway" */
															0x20, 0x54, 0x65, 0x61, 0x6d									/* " Team" */
															};
#endif

static int32 swNic_cleshell_init(void);
static int32 swNic_cmd(uint32 userId, int32 argc, int8 **saved);

#ifdef NIC_DEBUG_PKTDUMP
static int32 swNic_pktdumpCmd(uint32 userId, int32 argc, int8 **saved);
static void _swNic_pktdump(	uint32 currentCase,
								struct rtl_pktHdr *pktHdr_p,
								char *pktContent_p,
								uint32 pktLen,
								uint32 additionalInfo);

#endif

static int32 swNic_counterCmd(uint32 userId, int32 argc, int8 **saved);
static int32 swNic_getCounters(swNicCounter_t *counter_p);
static int32 swNic_resetCounters(void);

static int32 swNic_ringCmd(uint32 userId, int32 argc, int8 **saved);

static int32 swNic_loopbackCmd(uint32 userId, int32 argc, int8 **saved);
static int32 re865x_testL2loopback(struct rtl_pktHdr *pPkt);

static int32 swNic_hubCmd(uint32 userId, int32 argc, int8 **saved);
static int32 swNic_hubMode(struct rtl_pktHdr *pPkt);

#ifdef NIC_DEBUG_PKTGEN
static int32 _swNic_pktGenDump(void);
#endif
static int32 swNic_pktGenCmd(uint32 userId, int32 argc, int8 **saved);
#endif

/* ------------------------------------------------------------------------------------------
	Fast external device support
    ====================================================================== */
#ifdef RTL865X_SWNIC_FAST_EXTDEV_SUPPORT
#define SWNIC_FAST_EXTDEV_RXRUNOUTTXPENDED_PKT	0x3478
#define SWNIC_FAST_EXTDEV_ID							0x66

swNic_FastExtDevFreeCallBackFuncType_f swNic_fastExtDevFreeCallBackFunc_p = NULL;

static int32 _swNic_registerFastExtDevFreeCallBackFunc(swNic_FastExtDevFreeCallBackFuncType_f callBackFunc);
static inline int32 _swNic_fastExtDevFreePkt(struct rtl_pktHdr *pktHdr_p, struct rtl_mBuf *mbuf_p);
#endif

/* =========================================================================
		Function implementation
    ========================================================================= */

#define RTL865X_SWNIC_FUNCTION_IMPLEMENTATION

/* =========================================================
		NIC general procedure
    ========================================================= */
#define RTL865X_SWNIC_GENERAL_PROC_FUNCs


//#ifdef CONFIG_865xC_NIC_RX_DRR
/*
@func uint32		| _swNic_getNextRxPkthdrProcIndexDRR | Apply DRR algorithm to determine the Rx pack burst size and Rx ring index.
@parm uint32		| *rxPkthdrRingProcIndex_p | Call by reference parameter. Index to the 6 Rx descriptor rings.
@rvalue 0		| There is no Rx pending and burst packet size = 0.
@rvalue 1024		| There is Rx pending and burst packet size = 1024.
@comm
If there is Rx pending it will apply the DRR algorithm to determine the package amount and which ring could currently receive.
*/
__IRAM_FWD uint32 _swNic_getNextRxPkthdrProcIndexDRR(uint32 *rxPkthdrRingProcIndex_p)
{
	int32 index =0;
	uint32 i,RecvPktCnt=0,pktSize=0;
	uint32 *_rxPkthdrRing;
	uint32 _rxPkthdrRingIndex;
	uint32 _rxPkthdrRingMaxIndex;
	uint32 _rxDescContent;	
	struct rtl_pktHdr *rxPkt_p;
	static uint32 rxPkthdrRingEmptyQueueMask = 0x3f;

	if (rxPkthdrRingRxDoneMask == 0)
	{
		goto noNextRxIndex;
	}

	/*
		Originally, RX ring with higher ring "index" would have higher priority.
		=> So we ALWAYS do strict priority to check pending-RX from high ring to low ring.
	*/

	while(1)
	{
		index = (RTL865X_SWNIC_RXRING_MAX_PKTDESC - 1);

		while (1)
		{

			if(rxPkthdrRingRxDoneMask & (1 << index))
			{
			rxPkthdrRingEmptyQueueMask &= (~(1 << index) & 0x3f);

	nextRing:
				if(index<0) break;
				_rxPkthdrRing = rxPkthdrRing[index];
				_rxPkthdrRingIndex = rxPkthdrRingIndex[index];
				_rxPkthdrRingMaxIndex = rxPkthdrRingMaxIndex[index];
				_rxDescContent = _rxPkthdrRing[_rxPkthdrRingIndex];

				/* If the descriptor is SWCORE own, we stop receiving */
				if ((_rxDescContent & DESC_OWNED_BIT) == DESC_SWCORE_OWNED)
				{
					index--;
					goto nextRing;
				}

				rxPkt_p = (struct rtl_pktHdr*)(_rxDescContent & ~(DESC_OWNED_BIT|DESC_WRAP));
				pktSize=rxPkt_p->ph_len;

				while(rxPkthdrRingReceiveByDRR[index]>=pktSize)
				{

					++RecvPktCnt;
					rxPkthdrRingReceiveByDRR[index]-=pktSize;
					/* when runout, all the ring is CPU owned, must break the loop by checking recvice counter */
					if(RecvPktCnt>=_rxPkthdrRingMaxIndex) break;

					
					/* update recv descriptor index */
					_rxPkthdrRingIndex = (_rxPkthdrRingIndex == _rxPkthdrRingMaxIndex)?
											(0):
											(_rxPkthdrRingIndex + 1);
											
					_rxDescContent = _rxPkthdrRing[_rxPkthdrRingIndex];


					/* If the descriptor is SWCORE own, we stop receiving */
					if ((_rxDescContent & DESC_OWNED_BIT) == DESC_SWCORE_OWNED)
					{
						break;
					}						

					rxPkt_p = (struct rtl_pktHdr*)(_rxDescContent & ~(DESC_OWNED_BIT|DESC_WRAP));
					pktSize=rxPkt_p->ph_len;
					
				}

				if(RecvPktCnt!=0)
						goto retRxBurstSizeForRecv;
			}
			
			/* Find next entry */
			index --;
		}

		/* refill the DRR pool */
		for(i=0;i<RTL865X_SWNIC_RXRING_MAX_PKTDESC;i++)
		{
			if(rxPkthdrRingEmptyQueueMask & (1 << i))
			{
				//rxPkthdrRingReceiveByDRR[i] = rxPkthdrRingReceiveByDRR[i];
			}else
			{
				rxPkthdrRingReceiveByDRR[i] += rxPkthdrRingRefillByDRR[i];
				
				rxPkthdrRingEmptyQueueMask |= (1 << i);
			}
		}
		
	}
retRxBurstSizeForRecv:
	*rxPkthdrRingProcIndex_p = index;
	return RecvPktCnt;	/* Burst size is gotten from the AIR */

noNextRxIndex:
	return 0;

}

//#else

/*
@func uint32		| _swNic_getNextRxPkthdrProcIndex | Determine the Rx pack burst size and Rx ring index.
@parm uint32		| *rxPkthdrRingProcIndex_p | Call by reference parameter. Index to the current Rx ring.
@rvalue 0		| There is no Rx pending and burst packet size = 0.
@rvalue 1024		| There is Rx pending and burst packet size = 1024.
@comm
If the indexed Rx ring is pending, it will return the Rx burst packet size 1024. It checks the pending ring 
always start from the highest to the lowest one. It return zero if there is no Rx ring pending.
*/
__IRAM_FWD uint32 _swNic_getNextRxPkthdrProcIndex(uint32 *rxPkthdrRingProcIndex_p)
{
	uint32 index = (RTL865X_SWNIC_RXRING_MAX_PKTDESC - 1);

	if (rxPkthdrRingRxDoneMask == 0)
	{
		goto noNextRxIndex;
	}

	/*
		Originally, RX ring with higher ring "index" would have higher priority.
		=> So we ALWAYS do strict priority to check pending-RX from high ring to low ring.
	*/

	while (1)
	{
		if (rxPkthdrRingRxDoneMask & (1 << index))
		{
			goto retRxBurstSizeForRecv;
		}
		/* Find next entry */
		index --;
	}

retRxBurstSizeForRecv:
	*rxPkthdrRingProcIndex_p = index;
	return 1024;	/* Burst size is gotten from the AIR */

noNextRxIndex:
	return 0;
}

//#endif

/*
@func uint32		| _swNic_checkRunout | Check whether the Rx runout is solved.
@rvalue rxPkthdrRingRunoutMask	| The Rx ring mask indicate which ring is runout.
@comm
Check whether the content of Rx ring index to is equal to the Rx runout entry.
If they are different which means the runout is solved and modify the Rx runout
mask value.
*/
__IRAM_FWD static inline uint32 _swNic_checkRunout(void)
{
	if (rxPkthdrRingRunoutMask)
	{
		int32 idx;
		uint32 idxMask;

		for (	idx = 0, idxMask = 1 ;
				idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ;
				idx ++, idxMask <<= 1 /* left-shift 1 bit */ )
		{
			if (rxPkthdrRingRunoutMask & idxMask)
			{	/* This RX-ring need to resolve RUNOUT */

				if (rxPkthdrRingRunoutIndex[idx] < 0)
				{
					NIC_FATAL("RX ring [%d]: Runout is not be solved but Runout index does not exist!", idx);
					goto out;
				}

				/*
					Check the RX ring index which reported being RUNOUT:
						If the content of that entry is differ from original ( recorded when runout occurred ),
						it means this runout has been solved.
				*/
				if ((rxPkthdrRing[idx][rxPkthdrRingRunoutIndex[idx]]) != rxPkthdrRingRunoutEntry[idx])
				{
					rxPkthdrRunoutSolved ++;
					rxPkthdrRingRunoutIndex[idx] = -1;
					rxPkthdrRingRunoutEntry[idx] = 0;
					rxPkthdrRingRunoutMask &= ~(idxMask);
				}
			}
		}
	}

out:
	return rxPkthdrRingRunoutMask;
}

/*
@func int32		| swNic_isrReclaim 		| Reclaim the mbuf & pkthdr entry before recycle.
@parm uint32		| rxPkthdrRingProcIndex		| Rx descriptor ring index.
@parm uint32		| rxPkthdrDescIndex 		| Rx descriptor ring pkthdr entry index.
@parm uint32		| *pkt_p 				| Current pkthdr entry structure.
@parm uint32		| *firstMbuf_p 			| Current mbuf entry structure.
@rvalue FAILED	| The packet of recycled entry is not from Rx ring.
@rvalue SUCCESS	| Reclaim successful.
@comm
If the packet is NOT from RX ring, return FAILED. Otherwise, the pkthdr content will be clear and
put the pkthdr back to Rx descriptor ring. Then the content of mbuf chian linked to the pkther
will be clear and reset the mbuf's own bit to recycle it.

*/
__IRAM_FWD int32 swNic_isrReclaim(	uint32 rxPkthdrRingProcIndex,
										uint32 rxPkthdrDescIndex,
										struct rtl_pktHdr *pkt_p,
										struct rtl_mBuf *firstMbuf_p)
{
	uint32 rxReclaimDescContent;
	struct rtl_mBuf *mbuf_p;
	struct rtl_mBuf *nextMbuf_p;
	uint32 rxMbufDesc;

	NIC_TRACE_IN("");

	NIC_INFO(	"Reclaim packet [0x%p] - firstMbuf [0x%p] to RX Ring (%d) index (%d)",
				pkt_p,
				firstMbuf_p,
				rxPkthdrRingProcIndex,
				rxPkthdrDescIndex);

	/* This packet is NOT from RX ring */
	if (pkt_p->ph_rxPkthdrDescIdx < PH_RXPKTHDRDESC_MINIDX)
	{
#ifdef RTL865X_SWNIC_FAST_EXTDEV_SUPPORT
		/* This packet ever cause RX-runout TX-pending before */
		if (pkt_p->ph_reserved == SWNIC_FAST_EXTDEV_RXRUNOUTTXPENDED_PKT)
		{
			pkt_p->ph_reserved = 0;
			firstMbuf_p->m_next = 0;
			mBuf_freeOne(firstMbuf_p);
		} else
#endif
		{
			pkt_p->ph_flags |= PKTHDR_DRIVERHOLD;
			pkt_p->ph_rxdesc = PH_RXDESC_INDRV;
			pkt_p->ph_rxPkthdrDescIdx = PH_RXPKTHDRDESC_INDRV;
			pkt_p->ph_rxmbufdesc = rxMbufRingCnt;
		}

		NIC_TRACE_OUT("FAILED");

		return FAILED;
	}

	/* ==========================================================
			Clear Packet Header content
	     ========================================================== */
	pkt_p->ph_len = 0;
	pkt_p->ph_srcExtPortNum = 0;
	pkt_p->ph_extPortList = 0;
	pkt_p->ph_portlist = 0;

	/* ==========================================================
		Recycle Packet Header
	     ========================================================== */
	rxReclaimDescContent = rxPkthdrRing[rxPkthdrRingProcIndex][rxPkthdrDescIndex];
	if ((rxReclaimDescContent & DESC_OWNED_BIT) == DESC_SWCORE_OWNED)
	{
		NIC_FATAL(	"Reclaim one SWCORE-OWNED packet to RX ring [%d] index [%d]",
					rxPkthdrRingProcIndex,
					rxPkthdrDescIndex);
	}
	pkt_p->ph_flags &= ~(PKTHDR_DRIVERHOLD);	/* we put this packet back to RxRing, so we clear the flag to indicate it */

	rxPkthdrRing[rxPkthdrRingProcIndex][rxPkthdrDescIndex] =
		((rxReclaimDescContent & ~DESC_OWNED_BIT) | (DESC_SWCORE_OWNED));

	NIC_INFO(	"Recycle packet header to RX ring (0x%p) content (0x%x)",
				&(rxPkthdrRing[rxPkthdrRingProcIndex][rxPkthdrDescIndex]),
				rxPkthdrRing[rxPkthdrRingProcIndex][rxPkthdrDescIndex]);

	wmb();
	rxPkthdrRingLastReclaimIndex[rxPkthdrRingProcIndex] = rxPkthdrDescIndex;

	NIC_INFO(	"Update index : rxPkthdrRingLastReclaimIndex[%d] = %d",
				rxPkthdrRingProcIndex,
				rxPkthdrDescIndex);

	/* ==========================================================
		Proc MBUF chain
	     ========================================================== */
	mbuf_p = firstMbuf_p;
	while (mbuf_p !=  NULL)
	{
		nextMbuf_p = mbuf_p->m_next;
		/* ==========================================================
			Clear MBUF content
		     ========================================================== */
		mbuf_p->m_len = 0;

		/* ==========================================================
			Recycle MBUF
		     ========================================================== */
		if (mbuf_p->m_rxDesc & MBUF_RXDESC_FROM_RXRING)
		{
			/*
				MBUF is from MBUF Ring
			*/
			rxMbufDesc = mbuf_p->m_rxDesc & MBUF_RXDESC_IDXMASK;

			/*
				Before putting MBUF back to MBUF ring, we check its m_data to make sure headroom alignment
			*/
			{
				uint16 headRoom;

				headRoom = 0;

#if defined(CONFIG_RTL865X_MBUF_HEADROOM)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
				headRoom += CONFIG_RTL865X_MBUF_HEADROOM;
#endif

#ifdef SWNIC_RX_ALIGNED_IPHDR
				headRoom += 2;
#endif
				if (	(headRoom > 0) &&
					(mBuf_leadingSpace(mbuf_p) != headRoom))
				{
					if (mBuf_reserve(mbuf_p, headRoom))
					{
						NIC_ERR("MBUF HeadRoom reserving for entry [%d] failed", rxMbufDesc);
					}
				}
			}

			rxReclaimDescContent = rxMbufRing[rxMbufDesc];
			if ((rxReclaimDescContent & DESC_OWNED_BIT) == DESC_SWCORE_OWNED)
			{
				NIC_FATAL(	"Reclaim one SWCORE-OWNED mbuf to MBUF ring index [%d]",
							rxMbufDesc);
			}

			rxMbufRing[rxMbufDesc] =
				((rxReclaimDescContent & ~DESC_OWNED_BIT) | (DESC_SWCORE_OWNED));

			NIC_INFO(	"Recycle MBUF (%p) to MBUF ring index [%d] - (0x%p) content (0x%x)",
						mbuf_p,
						rxMbufDesc,
						&(rxMbufRing[rxMbufDesc]),
						rxMbufRing[rxMbufDesc]);

			wmb();
		} else
		{
			/*
				MBUF is from MBUF pool
			*/
			NIC_INFO("Recycle MBUF (%p) back to mbuf pool", mbuf_p);
			mBuf_freeOne(mbuf_p);
		}
		/* ==========================================================
			trace for next
		     ========================================================== */
		mbuf_p = nextMbuf_p;
	}

	NIC_TRACE_OUT("");

	return SUCCESS;
}


/*
@func int32		| _swNic_isrTxRecycle | Recycle the Tx descriptor ring entrys.
@parm			| dummy.
@parm			| txPkthdrRingIndex	| Index to Tx descriptor ring.
@rvalue count	| Recycled entrys count.
@comm
Check the CPUIISR Rx run out pending again and clear pending interrupts. And
then recycle the Rx descriptor ring entrys to resolve run out pending. Finally
update run out pending variable state after recycling.
*/
int32 swNic_isrTxRecycle(int32 dummy, int32 txPkthdrRingIndex)
{
	int32 retval;

	rtlglue_drvMutexLock();
	retval = _swNic_isrTxRecycle(dummy, txPkthdrRingIndex);
	rtlglue_drvMutexUnlock();
	return retval;
}

__IRAM_FWD static int32 _swNic_isrTxRecycle(int32 dummy, int32 txPkthdrRingIndex)
{
	uint32 *_txPkthdrRing;
	uint32 _txPkthdrRingDoneIndex;
	uint32 _txPkthdrRingMaxIndex;
	struct rtl_pktHdr* pktHdr_p;
	struct rtl_mBuf* firstMbuf_p;
	uint32 recycledPktCnt;

	NIC_TRACE_IN("");

	_txPkthdrRing = txPkthdrRing[txPkthdrRingIndex];
	_txPkthdrRingMaxIndex = txPkthdrRingMaxIndex[txPkthdrRingIndex];

	/*
		A loop to continuously recycle packet in TX ring
	*/
	_txPkthdrRingDoneIndex = txPkthdrRingDoneIndex[txPkthdrRingIndex];
	recycledPktCnt = 0;
	while (	(_txPkthdrRingDoneIndex != txPkthdrRingFreeIndex[txPkthdrRingIndex]) &&
			((_txPkthdrRing[_txPkthdrRingDoneIndex] & DESC_OWNED_BIT) == DESC_RISC_OWNED))
	{
		/* Packet is sent by ASIC - recycle this entry */
		pktHdr_p = (struct rtl_pktHdr*)(_txPkthdrRing[_txPkthdrRingDoneIndex] & ~(DESC_OWNED_BIT|DESC_WRAP));
		firstMbuf_p = pktHdr_p->ph_mbuf;

		if (pktHdr_p->ph_rxPkthdrDescIdx >= PH_RXPKTHDRDESC_MINIDX)
		{	/* Packet from RX ring : just reclaim it */
			swNic_isrReclaim(	pktHdr_p->ph_rxPkthdrDescIdx,
								pktHdr_p->ph_rxdesc,
								pktHdr_p,
								pktHdr_p->ph_mbuf);
		} else
		{
#ifdef RTL865X_SWNIC_FAST_EXTDEV_SUPPORT
			if (_swNic_fastExtDevFreePkt(pktHdr_p, firstMbuf_p) != SUCCESS)
#endif
			{
				mBuf_freeMbufChain(firstMbuf_p);
			}
		}

		/* update TX ring */
		_txPkthdrRing[_txPkthdrRingDoneIndex] &= (DESC_RISC_OWNED | DESC_WRAP);
		wmb();

		/* update TX counter */
		txPktCounter ++;
		recycledPktCnt ++;

		/* update TX ring pointer */
		_txPkthdrRingDoneIndex = (_txPkthdrRingDoneIndex == _txPkthdrRingMaxIndex)?
								(0):
								(_txPkthdrRingDoneIndex+1);

		txPkthdrRingDoneIndex[txPkthdrRingIndex] = _txPkthdrRingDoneIndex;
	}

	NIC_TRACE_OUT("Recycled Count: %d", recycledPktCnt);

	return recycledPktCnt;
}

/*
@func int32			| swNic_txRxSwitch	| Enable / disabe RX and TX function
@parm int32			|	tx				| Enable / Disable TX function.
@parm int32			|	rx				| Enable / Disable RX function.
@rvalue SUCCESS		|	Process success.
@rvalue NON-SUCCESS	|	Problem occurs when change RX/TX configuration.
@comm
Enable / disable RX and TX function of NIC.
The parameters <p rx> and <p tx> mean the RX/TX function would be enabled or not.
If <p rx>, <p tx>:
TRUE		: Enable
FALSE		: Disable
other		: Don't change current setting, just reset ASIC registers
 */
int32 swNic_txRxSwitch(int32 tx, int32 rx)
{
	int32 retval;
	rtlglue_drvMutexLock();
	retval = _swNic_txRxSwitch(tx, rx);
	rtlglue_drvMutexUnlock();

	return retval;
}

static int32 _swNic_txRxSwitch(int32 tx, int32 rx)
{
	uint32 cpuiicr;
	uint32 cpuiimr;
	int32 spinLock = 0;

	NIC_TRACE_IN("");

	spin_lock_irqsave(rtl865xSpinlock, spinLock);

	if ((tx == TRUE) || (tx == FALSE))
	{
		txEnable = tx;
	}

	if ((rx == TRUE) || (rx == FALSE))
	{
		rxEnable = rx;
	}

	cpuiicr = READ_MEM32(CPUICR);
	cpuiimr = READ_MEM32(CPUIIMR);

	if (txEnable == FALSE)
	{
		cpuiicr &= ~TXCMD;
		cpuiimr &= ~TX_ERR_IE_ALL;
	} else
	{
		cpuiicr |= TXCMD;
		cpuiimr |= TX_ERR_IE_ALL;
	}

	if (rxEnable == FALSE)
	{
		cpuiicr &=~ RXCMD;
		cpuiimr &= ~(RX_ERR_IE_ALL | PKTHDR_DESC_RUNOUT_IE_ALL |  RX_DONE_IE_ALL);
	} else
	{
		cpuiicr |= RXCMD;
		cpuiimr |=(RX_ERR_IE_ALL | PKTHDR_DESC_RUNOUT_IE_ALL |  RX_DONE_IE_ALL);
	}

	WRITE_MEM32(CPUICR, cpuiicr);
	WRITE_MEM32(CPUIIMR, cpuiimr);

	NIC_INFO(	"TX : %s RX : %s",
				(txEnable == TRUE)?"YES":"NO",
				(rxEnable == TRUE)?"YES":"NO");

	NIC_INFO(	"CPUICR (0x%x) == 0x%x (write 0x%x)",
				CPUICR, READ_MEM32(CPUICR), cpuiicr);

	NIC_INFO(	"CPUIIMR (0x%x) == 0x%x (write 0x%x)",
				CPUIIMR, READ_MEM32(CPUIIMR), cpuiimr);


	spin_unlock_irqrestore(rtl865xSpinlock, spinLock);

	NIC_TRACE_OUT("");

	return SUCCESS;
}

int32 swNic_enableSwNicWriteTxFN(uint32 enable)
{
	int32 retval;

	rtlglue_drvMutexLock();
	retval = _swNic_enableSwNicWriteTxFN(enable);
	rtlglue_drvMutexUnlock();

	return retval;
}

static int32 _swNic_enableSwNicWriteTxFN(uint32 enable)
{
	swNicWriteTXFN = (enable == TRUE)?TRUE:FALSE;

	return swNicWriteTXFN;
}

int32 swNic_installRxCallBackFunc(	proc_input_pkt_funcptr_t rxPreProcFunc,
										proc_input_pkt_funcptr_t rxProcFunc,
										proc_input_pkt_funcptr_t* orgRxPreProcFunc_p,
										proc_input_pkt_funcptr_t* orgRxProcFunc_p)
{
	int32 retval;

	rtlglue_drvMutexLock();

	retval = _swNic_installRxCallBackFunc(	rxPreProcFunc,
										rxProcFunc,
										orgRxPreProcFunc_p,
										orgRxProcFunc_p);

	rtlglue_drvMutexUnlock();

	return retval;
}

static int32 _swNic_installRxCallBackFunc(	proc_input_pkt_funcptr_t rxPreProcFunc,
												proc_input_pkt_funcptr_t rxProcFunc,
												proc_input_pkt_funcptr_t* orgRxPreProcFunc_p,
												proc_input_pkt_funcptr_t* orgRxProcFunc_p)
{
	int32 retval = FAILED;

	if (orgRxPreProcFunc_p)
	{
		*orgRxPreProcFunc_p = installedPreProcessInputPacket;
	}

	if (orgRxProcFunc_p)
	{
		*orgRxProcFunc_p = installedProcessInputPacket;
	}

	installedPreProcessInputPacket = rxPreProcFunc;
	installedProcessInputPacket = rxProcFunc;

	retval = SUCCESS;

	return retval;
}

/* =========================================================
		BOTTOM-HALF process
    ========================================================= */
#define RTL865X_SWNIC_BOTTOM_HALF_FUNCs

static int32 _swNic_linkChgIntHandler(void)
{
	NIC_TRACE_IN("");

	/* update counter */
	rtl8651_updateLinkChangePendingCount();

	if (rtl8651_updateLinkStatus() == SUCCESS)
	{
		#ifdef CONFIG_RTL865X_BICOLOR_LED
		#ifdef BICOLOR_LED_VENDOR_BXXX
		int32 idx;
		for ( idx = 0 ; idx < RTL8651_PHY_NUMBER ; idx ++ )
		{  
			if (READ_MEM32((PHY_BASE+(i<<5)+0x4)) & 0x4)
			{
				if (READ_MEM32((PHY_BASE+(i<<5))) & 0x2000)
				{
					/* link up speed 100Mbps */
					/* set gpio port  with high */
					WRITE_MEM32(PABDAT, READ_MEM32(PABDAT) | (1<<(16+i)));
				} else
				{
					/* link up speed 10Mbps */
					/* set gpio port  with high */
					WRITE_MEM32(PABDAT, READ_MEM32(PABDAT) & ~(1<<(16+i)));
				} /* end if  100M */
			}/* end if  link up */
		}/* end for */
		#endif /* BICOLOR_LED_VENDOR_BXXX */
		#endif /* CONFIG_RTL865X_BICOLOR_LED */

		NIC_TRACE_OUT("");

		return SUCCESS;
	}else
	{
		NIC_ERR("Link Change interrupt handling ERROR")
	}

	NIC_TRACE_OUT("");

	return FAILED;
}

/*
@func int32		| swNic_intHandler | RTL865xc NIC driver RX interrupt handler top half.
@parm int32		| param | dummy.
@rvalue FALSE	| Do nothing
@rvalue TRUE 	| Info Linux kernel to schedule the interrupt handler bottom half.
@comm
After ASIC ready packets for NIC RX, interrupt will trigger off this function.
This function will turn off CPUIIMR Rx pending mask to prevent continuously 
trigger by other following interrupts. Within alternative these 6 descriptor 
rings while ASIC ready packets for RX(RX done) or RX descriptor ring run 
out this function will return TRUE value to call rx thread function for receiving 
packets to CPU, or it will return FALSE to do nothing. Processed interrupts will 
be clear then for next interrupt.
*/
__IRAM_FWD int32 swNic_intHandler(int32 *param)
{
	uint32 cpuiisr;
	uint32 cpuiimr;
	uint32 intPending;
	int32 procDsr;

	NIC_TRACE_IN("");

	spin_lock(rtl865xSpinlock);

	procDsr = FALSE;

	{
		/* Read interrupt MASK/STATUS register */
		cpuiimr = READ_MEM32(CPUIIMR);
		cpuiisr = READ_MEM32(CPUIISR);

		intPending = cpuiisr & cpuiimr;

		/* ========================================================
				Process NIC related interrupts
					- RX done
					- Runout
					- RX/TX error
		    ======================================================== */
		if (intPending & INTPENDING_NIC_MASK)
		{
			/* Process RX done / Runout */
			if (intPending & (RX_DONE_IP_ALL|PKTHDR_DESC_RUNOUT_IP_ALL))
			{
				int32 idx;
				uint32 procIdx;

				/* Turning OFF IMR for these interrupts to prevent continuously trigger */
				WRITE_MEM32(CPUIIMR, cpuiimr & ~(RX_DONE_IP_ALL|PKTHDR_DESC_RUNOUT_IP_ALL));

				for ( idx = 0 ; idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; idx ++ )
				{
					procIdx = ((uint32*)(READ_MEM32(CPURPDCR(idx))) - rxPkthdrRing[idx]);
					if (intPending & RX_DONE_IP(idx))
					{
						NIC_INFO("RX done for index [%d]", idx);
						rxIntNum ++;
						rxPkthdrRingRxDoneMask |= (1 << idx);
						rxPkthdrRingLastRecvIndex[idx] = (procIdx == 0)?(rxPkthdrRingMaxIndex[idx]):(procIdx - 1);

						NIC_INFO(	"Update mask : RX-done MASK : 0x%x",
									rxPkthdrRingRxDoneMask);

						NIC_INFO(	"Update index : rxPkthdrRingLastRecvIndex[%d] : %d",
									idx,
									rxPkthdrRingLastRecvIndex[idx]);
					}
					if (intPending & PKTHDR_DESC_RUNOUT_IP(idx))
					{
						NIC_INFO("RX runout for index [%d]", idx);
						rxPkthdrRunoutNum ++;
						rxPkthdrRingRunoutMask |= (1 << idx);
						rxPkthdrRingRunoutIndex[idx] = procIdx;
						rxPkthdrRingRunoutEntry[idx] = rxPkthdrRing[idx][procIdx];

						NIC_INFO(	"Update mask : RX-runout MASK : 0x%x",
									rxPkthdrRingRunoutMask);

						NIC_INFO(	"Update index : rxPkthdrRingRunoutIndex[%d] : %d",
									idx,
									procIdx);
					}
				}

				procDsr = TRUE;
			}

			/* RX/TX error interrupt */
			if (intPending & (RX_ERR_IP_ALL | TX_ERR_IP_ALL))
			{
				int32 idx;

				/* RX ERROR */
				for ( idx = 0 ; idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; idx ++ )
				{
					if (intPending & RX_ERR_IP(idx))
					{
						rxPktErrorNum ++;
					}
				}

				/* TX ERROR */
				for ( idx = 0 ; idx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; idx ++ )
				{
					if (intPending & TX_ERR_IP(idx))
					{
						txPktErrorNum ++;
					}
				}
			}
		}

		/* ========================================================
				Process Link change interrupt
		    ======================================================== */
		if (intPending & LINK_CHANGE_IP)
		{
			linkChanged ++;
			_swNic_linkChgIntHandler();
		}

		/* ========================================================
				Clear all processed interrupts
					- Write 1 to clear
		    ======================================================== */
		intPending &= INTPENDING_NIC_MASK|LINK_CHANGE_IP;
		WRITE_MEM32(CPUIISR, intPending);
	}

	spin_unlock(rtl865xSpinlock);

	NIC_TRACE_OUT("Do DSR : %s", procDsr?"YES":"NO");

	return procDsr;
}

/*
@func void		| _swNic_updateRxPendingMask | Check CPUIISR RX done status and update RX done mask variable.
@comm
Check the RX done status of the 6 descriptor rings, mask the RX done mask variable
if there is RX done pending.
*/
__IRAM_FWD static inline void _swNic_updateRxPendingMask(void)
{
	int32 idx;
	int32 idxMask;
	uint32 cpuiisr;
	uint32 procIdx;

	NIC_TRACE_IN("");

	cpuiisr = READ_MEM32(CPUIISR);	/* get IISR */

	for (	idx = 0, idxMask = 1 ;
			idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ;
			idx ++, idxMask <<= 1 /* left-shift for 1 bit */ )
	{
		if (cpuiisr & RX_DONE_IP(idx))
		{
			NIC_INFO("RX ring (%d) - RX done interrupt", idx);

			procIdx = ((uint32*)(READ_MEM32(CPURPDCR(idx))) - rxPkthdrRing[idx]);

			/* Update Last Recv Index & RxDoneMask */
			rxPkthdrRingLastRecvIndex[idx] = (procIdx == 0)?(rxPkthdrRingMaxIndex[idx]):(procIdx - 1);
			rxPkthdrRingRxDoneMask |= idxMask;

			NIC_INFO(	"Update mask : RX-done MASK : 0x%x",
						rxPkthdrRingRxDoneMask);

			NIC_INFO(	"Update index : rxPkthdrRingLastRecvIndex[%d] : %d",
						idx,
						rxPkthdrRingLastRecvIndex[idx]);

		} else
		{
			/* No RX Done interrupt for this Ring, so we check if we would to turn OFF its Rx Done Mask */
			if (	(rxPkthdrRingRxDoneMask & idxMask) &&
				(((rxPkthdrRing[idx][rxPkthdrRingIndex[idx]]) & DESC_OWNED_BIT) == DESC_SWCORE_OWNED))
			{
				NIC_INFO("RX ring (%d) - no more pending RX packet", idx);

				rxPkthdrRingRxDoneMask &= ~idxMask;
			}
		}
	}

	/* Clear CPUIISR for RxDone interrupt */
	cpuiisr &= RX_DONE_IP_ALL;
	WRITE_MEM32(CPUIISR, cpuiisr);

	NIC_TRACE_OUT("");
}

/*
@func void		| _swNic_rxRunoutHandler | Handle the Rx run out pending.
@comm
Check the CPUIISR Rx run out pending again and clear pending interrupts. And
then recycle the Tx descriptor ring entrys to resolve run out pending. Finally
update run out pending variable state after recycling.
*/
static inline void _swNic_rxRunoutHandler(void)
{
	int32 idx;
	int32 idxMask;
	uint32 cpuiisr;
	uint32 procIdx;

	/* ========================================================
		We always check if there is any additional runout event before recycling
	     ======================================================== */

	cpuiisr = READ_MEM32(CPUIISR);	/* get IISR */

	for (	idx = 0, idxMask = 1 ;
			idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ;
			idx ++, idxMask <<= 1 /* left-shift for 1 bit */ )
	{
		if (cpuiisr & PKTHDR_DESC_RUNOUT_IP(idx))
		{
			procIdx = ((uint32*)(READ_MEM32(CPURPDCR(idx))) - rxPkthdrRing[idx]);

			/* Update Runout Index & RxRunoutMask */
			rxPkthdrRunoutNum ++;
			rxPkthdrRingRunoutMask |= (1 << idx);
			rxPkthdrRingRunoutIndex[idx] = procIdx;
			rxPkthdrRingRunoutEntry[idx] = rxPkthdrRing[idx][procIdx];
		}
	}
	/* Clear CPUIISR for RxDone interrupt */
	cpuiisr &= PKTHDR_DESC_RUNOUT_IP_ALL;
	WRITE_MEM32(CPUIISR, cpuiisr);

	/* ========================================================
		Recycle TX ring to resolve runout
	     ======================================================== */
	for ( idx = 0 ; idx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; idx ++ )
	{
		_swNic_isrTxRecycle(0, idx);
	}

	/* update runout state after recycling */
	_swNic_checkRunout();
}

/*
@func void		| swNic_rxThread | RTL865xc NIC driver RX interrupt bottom half.
@parm int32		| param | dummy.
@comm
This function will check RX done pending again to prevent Rx done pending mask does
not match the CPUIISR status.According to the interrupt handler top half return value, the 
kernel will schedule this function to receive packets to CPU while there is RX done 
pending. Or it will check Rx run out pending and handle it, and then it turns on the Rx
pending mask which is turned off by top half to handle following interrupts.
*/
__IRAM_FWD void swNic_rxThread(unsigned long param)
{
	uint32 _rxPkthdrRingProcIndex;
	uint32 _rxPkthdrRingProcBurstSize;
	
	int32 spinLock = 0;

#ifdef SWNIC_EARLYSTOP
	nicRxEarlyStopTriggered = FALSE;
	nicRxPktOneshotCnt = 0;
#endif

	spin_lock_irqsave(rtl865xSpinlock, spinLock);

	/* At the first time, we would double check RX pending issue if rxPkthdrRingRxDoneMask is NULL */
	if (rxPkthdrRingRxDoneMask == 0)
	{
		_swNic_updateRxPendingMask();
	}

	/* While loop to continuously polling RX ring and */
	do
	{
		NIC_INFO("Current rxPkthdrRingRxDoneMask: 0x%x", rxPkthdrRingRxDoneMask);

#ifdef CONFIG_865XC_NIC_RX_DRR
	if(drrenable)
	{
		if ((_rxPkthdrRingProcBurstSize = _swNic_getNextRxPkthdrProcIndexDRR(&_rxPkthdrRingProcIndex)) == 0)
		{
			goto rxDone;
		}
	}
	else
	{
#endif 
		if ((_rxPkthdrRingProcBurstSize = _swNic_getNextRxPkthdrProcIndex(&_rxPkthdrRingProcIndex)) == 0)
		{
			goto rxDone;
		}
#ifdef CONFIG_865XC_NIC_RX_DRR		
	}	
#endif


		/* receiving packet */
		rxPktCounter += _swNic_recvLoop(_rxPkthdrRingProcIndex, _rxPkthdrRingProcBurstSize);

		/* update Rx Pending List */
		_swNic_updateRxPendingMask();

#ifdef SWNIC_EARLYSTOP
		if (nicRxEarlyStopTriggered == TRUE)
		{
			goto rxDone;
		}
#endif

	} while (rxPkthdrRingRxDoneMask);

rxDone:

	/* ================================================================
			RX runout interrupt process
	    ================================================================ */
	if (	(rxPkthdrRingRunoutMask != 0) ||
		(READ_MEM32(CPUIISR) & PKTHDR_DESC_RUNOUT_IP_ALL))
	{
		/* Runout occurred between NIC ISR and here */
		_swNic_rxRunoutHandler();
	}

	/* ================================================================
			Turn ON RxDone / Runout interrupt back before DSR finish ( We turned it OFF in ISR)
	    ================================================================ */
	WRITE_MEM32(CPUIIMR, READ_MEM32(CPUIIMR)|(RX_DONE_IE_ALL|PKTHDR_DESC_RUNOUT_IE_ALL));

	spin_unlock_irqrestore(rtl865xSpinlock, spinLock);

}

/*
@func int32		| _swNic_recvLoop 		| Receive packet from Rx ring to CPU.
@parm int32		| rxPkthdrRingProcIndex		| dummy.
@parm int32		| rxPkthdrRingProcBurstSize	| dummy.
@rvalue count	| Total number of receiving packets.
@comm
Loop to receive packets until receiving packets number is large than Rx ring bust size
or the packet is held by driver or is held by Tx pending. Actually it invokes an installed 
function pointer to handle packets.
*/
__IRAM_FWD static int32 _swNic_recvLoop(uint32 rxPkthdrRingProcIndex, uint32 rxPkthdrRingProcBurstSize)
{
	uint32 *_rxPkthdrRing;
	uint32 *_rxPkthdrRingIndex_p;
	uint32 _rxPkthdrRingMaxIndex;
	uint32 _rxDescContent;

	struct rtl_pktHdr *rxPkt_p;
	uint32 recvCnt;

	NIC_TRACE_IN("");

	NIC_INFO("Receive packet in RX ring (%d) MAX count (%d)", rxPkthdrRingProcIndex, rxPkthdrRingProcBurstSize);

	/* =================================================================
		Loop to continuously receiving packet
	    ================================================================= */
	recvCnt = 0;
	_rxPkthdrRing = rxPkthdrRing[rxPkthdrRingProcIndex];
	_rxPkthdrRingIndex_p = &(rxPkthdrRingIndex[rxPkthdrRingProcIndex]);
	_rxPkthdrRingMaxIndex = rxPkthdrRingMaxIndex[rxPkthdrRingProcIndex];

	while (recvCnt < rxPkthdrRingProcBurstSize)
	{
		PROFILING_START(ROMEPERF_INDEX_RECVLOOP);

#ifdef SWNIC_EARLYSTOP
		if (	(nicRxPktBurstSize) &&
			(nicRxConcernedGisrInteruptMask) &&
			(nicRxPktOneshotCnt >= nicRxPktBurstSize))
		{
			uint32 gisr = READ_MEM32(GISR);

			if (gisr & nicRxConcernedGisrInteruptMask)
			{
				nicRxEarlyStopTriggered = TRUE;

				PROFILING_END(ROMEPERF_INDEX_RECVLOOP);
				goto rxDone;
			}
		}
#endif

		/* ===============================================
			Procedure to receive packet
				=> Get packet pointed by rxPkthdrRingIndex and process it.
		    ================================================ */
		{
			_rxDescContent = _rxPkthdrRing[*_rxPkthdrRingIndex_p];

			/* If the descriptor is SWCORE own, we stop receiving */
			if ((_rxDescContent & DESC_OWNED_BIT) == DESC_SWCORE_OWNED)
			{
				goto rxDone;
			}

			/* Get Packet header from descriptor ring */
			rxPkt_p = (struct rtl_pktHdr*)(_rxDescContent & ~(DESC_OWNED_BIT|DESC_WRAP));

			NIC_INFO(	"Receive packet (0x%p) for RX ring (%d) index (%d)",
						rxPkt_p,
						rxPkthdrRingProcIndex,
						*_rxPkthdrRingIndex_p);

			/*
				(pPkthdr->ph_flags&PKTHDR_DRIVERHOLD) != 0 means :
				
					This packet is held by ROME Driver (ex. it's queued in Driver)
					or held by Tx Ring (ex. Can not be sent immediately due to CPU port were flow controlled.) instead of RxRing.
					And Rx is blocked by this pending packet.

					So we allocate another packet and put it into Rx Ring to solve this problem (done in swNic_rxRunoutTxPending()).
			*/

			if((rxPkt_p->ph_flags & PKTHDR_DRIVERHOLD) != 0)
			{
				/*
					RX packet is found that it's processed by Rome Driver yet.
					=> Handle it and STOP receiving.
				*/
				NIC_INFO(	"RX runout TX pending occurs for packet (0x%p) for RX ring (%d) index (%d) ",
							rxPkt_p,
							rxPkthdrRingProcIndex,
							*_rxPkthdrRingIndex_p);

				_swNic_rxRunoutTxPending((struct rtl_pktHdr *)rxPkt_p);

				PROFILING_END(ROMEPERF_INDEX_RECVLOOP);

				goto rxDone;
			}

			rxPkt_p->ph_flags |= PKTHDR_DRIVERHOLD;	/* this packet is received/processed by Rome Driver or TxRing */

			/* Set MBUF flags */
			SETBITS(rxPkt_p->ph_mbuf->m_flags, MBUF_PKTHDR);

			assert(rxPkt_p->ph_rxdesc == (*_rxPkthdrRingIndex_p));

#ifdef CONFIG_RTL865X_MULTILAYER_BSP
			if (	(installedPreProcessInputPacket) &&
				((*installedPreProcessInputPacket)(rxPkt_p) != SUCCESS))
			{
				NIC_WARN(	"Drop Packet -- RX ring index [%d] - rx desc [%d]",
							rxPkthdrRingProcIndex,
							*_rxPkthdrRingIndex_p);

				swNic_isrReclaim(	rxPkthdrRingProcIndex,
									*_rxPkthdrRingIndex_p,
									rxPkt_p,
									rxPkt_p->ph_mbuf);
			} else
#endif
			{

#ifdef NIC_DEBUG_PKTDUMP
				swNic_pktdump(	NIC_PHY_RX_PKTDUMP,
								rxPkt_p,
								rxPkt_p->ph_mbuf->m_data,
								rxPkt_p->ph_len,
								0 /* reserved */ );
#endif
				NIC_INFO(	"RUN RX-Callback (0x%p) for packet (0x%p)",
							installedProcessInputPacket,
							rxPkt_p);

				/* Invoked installed function pointer to handle packet */
				if (installedProcessInputPacket)
				{
					(*installedProcessInputPacket)(rxPkt_p);
				} else
				{
					/* No CallBack function : drop this packet */
					swNic_isrReclaim(	rxPkthdrRingProcIndex,
										*_rxPkthdrRingIndex_p,
										rxPkt_p,
										rxPkt_p->ph_mbuf);
				}
			}
		}


	/* =================================================================
		Post-process of Rx-loop
	    ================================================================= */

		/* update counter */
#ifdef SWNIC_EARLYSTOP
		nicRxPktOneshotCnt ++;
#endif
		recvCnt ++;

		/* update recv descriptor index */
		*_rxPkthdrRingIndex_p = (*_rxPkthdrRingIndex_p == _rxPkthdrRingMaxIndex)?
								(0):
								((*_rxPkthdrRingIndex_p) + 1);

#ifdef CONFIG_RTL865X_MULTILAYER_BSP
		/* Louis: We shall check per packet to guarantee VoIP realtime process. */
		rtl8651_realtimeSchedule();
#endif


		PROFILING_END(ROMEPERF_INDEX_RECVLOOP);
	}

rxDone:

	if (recvCnt > maxCountinuousRxCount)
	{
		maxCountinuousRxCount = recvCnt;
	}

	NIC_TRACE_OUT("Receive count: %d", recvCnt);

	return recvCnt;

}

/*
@func int32		| _swNic_rxRunoutTxPending	| Resolve the Rx runout caused by Tx pending.
@parm rtl_pktHdr	| *pkthdr_p					| Pending packet.
@rvalue FAILED	| Fail to resolve the Tx pending problem.
@rvalue SUCCESS	| Resolve the Rx runout problem successfully.
@comm
This function will first allocate a new pkthdr and configure it according to the
Tx pending one, and put it to Rx ring for Rx receiving. Then we caculate the
amount of the number of mbuf linked to the pending Rx ring. So we can allocate
a new mbuf chain with the same amount and replace the pending one.
*/
static int32 _swNic_rxRunoutTxPending(struct rtl_pktHdr *pkthdr_p)
{
	struct rtl_pktHdr *newPkthdr_p;

	int32 rxRunoutRingIndex;
	int32 rxRunoutDescIndex;
	uint32 rxRunoutRingDescControlMask;
	struct rtl_mBuf *orgMbuf_p;
	struct rtl_mBuf *orgMbuf_trace_p;
	struct rtl_mBuf *mbuf_p;
	struct rtl_mBuf *newMbuf_head_p;
	struct rtl_mBuf *newMbuf_tail_p;
	int32 mbufCnt;
	int32 idx;

	NIC_INFO("Packet 0x%p : RX runout by pending TX", pkthdr_p);

	orgMbuf_p = pkthdr_p->ph_mbuf;
	rxRunoutRingIndex = pkthdr_p->ph_rxPkthdrDescIdx;
	rxRunoutDescIndex = pkthdr_p->ph_rxdesc;

	NIC_INFO("Packet ( first mbuf 0x%p) is in RX ring [%d] index (%d)", orgMbuf_p, rxRunoutRingIndex, rxRunoutDescIndex);

	/* ========================================================
		Allocate and init a new packet to put into Rx Ring and replace the pending one.
	    ======================================================== */
	if (mBuf_driverGetPkthdr(1, &newPkthdr_p, &newPkthdr_p) != 1)
	{
		NIC_FATAL("Not any free packet header! Runout can NOT be solved!");
		return FAILED;
	}

	NIC_INFO("Use new packet header [0x%p] to replace original one", newPkthdr_p);

	/* Config new packet header before put it into RX ring */
	newPkthdr_p->ph_flags &= ~PKTHDR_DRIVERHOLD;	/* pkthdr put to RxRing, we remove this flag */		
	newPkthdr_p->ph_rxdesc = rxRunoutDescIndex;
	newPkthdr_p->ph_rxPkthdrDescIdx = rxRunoutRingIndex;
	newPkthdr_p->ph_rxmbufdesc = rxRunoutDescIndex;

	/*
		Update runout packet header:
			Original pending packet: we modify its rxdesc to indicate
			this packet is no longer being the Rx Ring packet.
	*/
	pkthdr_p->ph_rxdesc = PH_RXDESC_INDRV;
	pkthdr_p->ph_rxPkthdrDescIdx = PH_RXPKTHDRDESC_INDRV;

	/* Put new packet header into RX ring */
	NIC_INFO("Put new packet header into RX ring");
	rxRunoutRingDescControlMask = (rxPkthdrRing[rxRunoutRingIndex][rxRunoutDescIndex]) & DESC_WRAP;
	rxRunoutRingDescControlMask |= DESC_SWCORE_OWNED;
	rxPkthdrRing[rxRunoutRingIndex][rxRunoutDescIndex] =
		(	(((uint32)newPkthdr_p) & ~(DESC_OWNED_BIT|DESC_WRAP)) |
			rxRunoutRingDescControlMask	);

	wmb();

	/* ========================================================
		Allocate and init a new MBUF to put into Rx Ring and replace the pending one.
	    ======================================================== */
	/* check total MBUF count */
	mbufCnt = 0;
	mbuf_p = orgMbuf_p;
	while (mbuf_p != NULL)
	{
		NIC_INFO("Check MBUF : 0x%p", mbuf_p);

		if (mbuf_p->m_rxDesc & MBUF_RXDESC_FROM_RXRING)
		{
			NIC_INFO("\t=> In RX ring (%d)", (uint32)(mbuf_p->m_rxDesc & MBUF_RXDESC_FROM_RXRING));
			mbufCnt ++;
		} else
		{
			NIC_INFO("\t=> From MBUF pool");
		}
		mbuf_p = mbuf_p->m_next;
	}

	NIC_INFO("Total MBUFs need replacement : %d", mbufCnt);

	if (mBuf_driverGet(	mbufCnt,
						&newMbuf_head_p,
						&newMbuf_tail_p) != mbufCnt)
	{
		NIC_FATAL("No enough mbuf (%d) to solve runout", mbufCnt);
		mBuf_driverFreePkthdr(newPkthdr_p, 1, NULL);
		if (newMbuf_head_p)
		{
			mBuf_driverFreeMbufChain(newMbuf_head_p);
		}
		return FAILED;
	}

	for (	idx = 0, mbuf_p = newMbuf_head_p, orgMbuf_trace_p = orgMbuf_p ;
			idx < mbufCnt ;
			idx ++, mbuf_p = mbuf_p->m_next )
	{
		mbuf_p->m_extbuf = (uint8*)(UNCACHE(mbuf_p->m_extbuf));
		mbuf_p->m_data = (uint8*)(UNCACHE(mbuf_p->m_data));

#if defined(CONFIG_RTL865X_MBUF_HEADROOM)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
		if (mBuf_reserve(mbuf_p, CONFIG_RTL865X_MBUF_HEADROOM) != SUCCESS)
		{
			NIC_FATAL("MBUF(0x%p) reserve headroom (%d) FAILED", mbuf_p, CONFIG_RTL865X_MBUF_HEADROOM);
		}
#endif

		/* Trace original mbuf chain to find mbufs from RX-RING */
		while ((orgMbuf_trace_p->m_rxDesc & MBUF_RXDESC_FROM_RXRING) == 0)
		{
			orgMbuf_trace_p = orgMbuf_trace_p->m_next;

			if (orgMbuf_trace_p == NULL)
			{
				NIC_FATAL("The count of MBUF from RX ring is not match with previous result (%d) !", mbufCnt);
				return FAILED;
			}
		}

		/* use new MBUF to replace the original one */
		NIC_INFO(	"Put new MBUF (0x%p) into RX ring index [%d] to replace original (0x%p)",
					mbuf_p,
					(mbuf_p->m_rxDesc & MBUF_RXDESC_IDXMASK),
					orgMbuf_trace_p);

		mbuf_p->m_rxDesc =  orgMbuf_trace_p->m_rxDesc;
		rxRunoutDescIndex = mbuf_p->m_rxDesc & MBUF_RXDESC_IDXMASK;
		orgMbuf_trace_p->m_rxDesc = 0;

		rxRunoutRingDescControlMask = rxMbufRing[rxRunoutDescIndex] & DESC_WRAP;
		rxRunoutRingDescControlMask |= DESC_SWCORE_OWNED;
		rxMbufRing[rxRunoutDescIndex] =
			(	((uint32)(mbuf_p) & ~(DESC_OWNED_BIT|DESC_WRAP)) |
				rxRunoutRingDescControlMask	);
		wmb();
	}

#ifdef RTL865X_SWNIC_FAST_EXTDEV_SUPPORT
	/*
		For Fast extension device support,
		we would set some information to indicate the original packet is replaced RX ring packet.
	*/
	{
		pkthdr_p->ph_reserved = SWNIC_FAST_EXTDEV_RXRUNOUTTXPENDED_PKT;
	}
#endif
	
	return SUCCESS;
}


/* =========================================================
		TOP-HALF process
    ========================================================= */
#define RTL865X_SWNIC_TOP_HALF_FUNCs

__IRAM_FWD int32 swNic_write(void *outPkt, int32 txPkthdrRingIndex)
{
    	struct rtl_pktHdr *txPkt_p;
	struct rtl_mBuf *txFirstMbuf_p;
	uint32 txPktLen;
	uint32 txFreeIndex, txNextFreeIndex;
	uint32 txDescRingContent;
	int32 spinLock;

	NIC_TRACE_IN("");

	txPkt_p = (struct rtl_pktHdr*)outPkt;
	txFirstMbuf_p = txPkt_p->ph_mbuf;
	txPktLen = txPkt_p->ph_len;
	spinLock = 0;

	NIC_INFO(	"TX to Ring [%d] : packet [0x%p] First mbuf [0x%p] Total Length [%d]",
				txPkthdrRingIndex,
				txPkt_p,
				txFirstMbuf_p,
				txPktLen	);

	/* ====================================================================
		Preparation
			- Do post-process before actually sending packet
	    ==================================================================== */

	if (	(txEnable == FALSE) ||
		(txPkthdrRing[txPkthdrRingIndex] == NULL))
	{
		NIC_ERR(	"TX disabled : Tx Enable : %d Tx ring (%d) : %s",
					txEnable,
					txPkthdrRingIndex,
					(txPkthdrRing[txPkthdrRingIndex] == NULL)?"DISABLED":"ENABLED")
		goto errout;
	}

#ifdef CONFIG_RTL865X_MULTILAYER_BSP
	PROFILING_START(ROMEPERF_INDEX_TXPKTPOST);

	rtl8651_txPktPostProcessing(txPkt_p);

	PROFILING_END(ROMEPERF_INDEX_TXPKTPOST);
#endif

	PROFILING_START(ROMEPERF_INDEX_MBUFPAD);
	if (	(txPktLen < 60) &&
		(mBuf_padding(txFirstMbuf_p, 60 - txPktLen, MBUF_DONTWAIT) == NULL))
	{
		PROFILING_END(ROMEPERF_INDEX_MBUFPAD);
		goto errout;
	}
	PROFILING_END(ROMEPERF_INDEX_MBUFPAD);

	PROFILING_START(ROMEPERF_INDEX_TXALIGN);
#ifdef SWNIC_TXALIGN
	if (_swNic_TxAlign_Support == TRUE)
	{
		_swNic_txAlign(txPkt_p, txFirstMbuf_p);
	}
#endif

	txPkt_p->ph_nextHdr = (struct rtl_pktHdr*)NULL;

	/* ====================================================================
		Start TX process
	    ==================================================================== */

	spin_lock_irqsave(rtl865xSpinlock, spinLock);

	/* If TX ring is full : recycle sent packet and retry it */
	if (txPktHdrRingFull(txPkthdrRingIndex))
	{
		int32 recycleCnt;

		NIC_INFO(	"TX Ring [%d] is FULL",
					txPkthdrRingIndex);
		NIC_INFO(	"\ttxPkthdrRingFreeIndex[%d] = %d",
					txPkthdrRingIndex,
					txPkthdrRingFreeIndex[txPkthdrRingIndex]);
		NIC_INFO(	"\ttxPkthdrRingIndex[%d] = %d",
					txPkthdrRingIndex,
					txPkthdrRingMaxIndex[txPkthdrRingIndex]);
		NIC_INFO(	"\ttxPkthdrRingDoneIndex[%d] = %d",
					txPkthdrRingIndex,
					txPkthdrRingDoneIndex[txPkthdrRingIndex]);

		PROFILING_START(ROMEPERF_INDEX_ISRTXRECYCLE);
		recycleCnt = _swNic_isrTxRecycle(0, txPkthdrRingIndex);
		PROFILING_END(ROMEPERF_INDEX_ISRTXRECYCLE);
		if (recycleCnt == 0)
		{
			PROFILING_END(ROMEPERF_INDEX_TXALIGN);
			NIC_WARN("TX ring [%d] full and packet [0x%p] can not be sent", txPkthdrRingIndex, txPkt_p);

			if (swNicWriteTXFN == TRUE)
			{
				NIC_INFO(	"Tx FULL detected: Set TXFD (0x%x) to CPUICR (0x%x) to notify HW to receive packet",
							TXFD,
							CPUICR);

				WRITE_MEM32(CPUICR, READ_MEM32(CPUICR) | TXFD);
			}

			goto unlockErrout;
		}
		if (txPktHdrRingFull(txPkthdrRingIndex))
		{
			PROFILING_END(ROMEPERF_INDEX_TXALIGN);
			NIC_FATAL("TX ring release [%d] packets but still FULL", recycleCnt);
			goto unlockErrout;
		}
	}

	/* Put packet into TX ring */
	assert(DESC_SWCORE_OWNED != 0);

	/* get current index to TX packet */
	txFreeIndex = txPkthdrRingFreeIndex[txPkthdrRingIndex];

	/* set content of current TX ring index */
	txDescRingContent = ((uint32)(txPkt_p) | DESC_SWCORE_OWNED);
	if (txFreeIndex == txPkthdrRingMaxIndex[txPkthdrRingIndex])
	{
		txDescRingContent |= DESC_WRAP;
		txNextFreeIndex = 0;
	} else
	{
		txNextFreeIndex = txFreeIndex + 1;
	}

#ifdef NIC_DEBUG_PKTDUMP
	swNic_pktdump(	NIC_PHY_TX_PKTDUMP,
					txPkt_p,
					txPkt_p->ph_mbuf->m_data,
					txPkt_p->ph_len,
					txPkthdrRingIndex );
#endif

	/* Actually put packet into TX ring */
	NIC_INFO(	"Put into TX ring [0x%p] : index (%d) - content [0x%x]",
				&(txPkthdrRing[txPkthdrRingIndex][txPkthdrRingFreeIndex[txPkthdrRingIndex]]),
				txFreeIndex,
				txDescRingContent);

	txPkthdrRing[txPkthdrRingIndex][txPkthdrRingFreeIndex[txPkthdrRingIndex]] = txDescRingContent;
	wmb();

	/* Update pointers */
	txPkthdrRingLastIndex[txPkthdrRingIndex] = txFreeIndex;
	txPkthdrRingFreeIndex[txPkthdrRingIndex] = txNextFreeIndex;

	NIC_INFO(	"New index : txPkthdrRingLastIndex[%d] = %d txPkthdrRingFreeIndex[%d] = %d",
				txPkthdrRingIndex, txFreeIndex,
				txPkthdrRingIndex, txNextFreeIndex);

	spin_unlock_irqrestore(rtl865xSpinlock, spinLock);

	if (swNicWriteTXFN == TRUE)
	{
		NIC_INFO(	"Set TXFD (0x%x) to CPUICR (0x%x) to notify HW to receive packet",
					TXFD,
					CPUICR);

		WRITE_MEM32(CPUICR, READ_MEM32(CPUICR) | TXFD);
	}

	PROFILING_END(ROMEPERF_INDEX_TXALIGN);

	NIC_TRACE_OUT("SUCCESS");

	return SUCCESS;

unlockErrout:

	spin_unlock_irqrestore(rtl865xSpinlock, spinLock);

errout:

	NIC_TRACE_OUT("Error OUT");

	return
#ifdef RTL865X_SWNIC_FAST_EXTDEV_SUPPORT
		_swNic_fastExtDevFreePkt(txPkt_p, txFirstMbuf_p);
#else
		FAILED;
#endif
}

/*
@func int32		| swNic_init					| Nic driver initialization.
@parm uint32		| userNeedRxPkthdrRingCnt[]	| Each Rx ring pkthdr entry amount.
@parm uint32		| userNeedRxMbufRingCnt		| Total mbuf entry amount.
@parm uint32		| userNeedTxPkthdrRingCnt[]	| Each Tx ring pkthdr entry amount.
@parm uint32		| clusterSize					| Each cluster size.
@parm proc_input_pkt_funcptr_t | rxProcFunc		| Callback function for Rx pkts pre-processing.
@parm proc_input_pkt_funcptr_t | rxPreProcFunc	| Callback function for Rx pkts processing.
@rvalue SUCCESS	| Success to setup Rx ring, Tx ring, mbuf and other driver variables.
@rvalue FAILED	| Parameters error or initialization fail.
@comm
1. It needs to check all ring size and cluster size must be power of 2. 
2. It initialize all Rx/Tx ring pkthdr and mbuf variables and allocate the memory space. 
3. rxProcFunc & rxPreProcFunc callback functions will be installed.
4. Another NIC hardware setup function will then be called to setup ASIC NIC configuration registers.
*/
int32 swNic_init(	uint32 userNeedRxPkthdrRingCnt[RTL865X_SWNIC_RXRING_MAX_PKTDESC],
					uint32 userNeedRxMbufRingCnt,
					uint32 userNeedTxPkthdrRingCnt[RTL865X_SWNIC_TXRING_MAX_PKTDESC],
					uint32 clusterSize,
					proc_input_pkt_funcptr_t rxProcFunc, 
					proc_input_pkt_funcptr_t rxPreProcFunc)
{
	int32 retval;
	rtlglue_drvMutexLock();
	retval = _swNic_init(	userNeedRxPkthdrRingCnt,
						userNeedRxMbufRingCnt,
						userNeedTxPkthdrRingCnt,
						clusterSize,
						rxProcFunc,
						rxPreProcFunc);
	rtlglue_drvMutexUnlock();

	return retval;
}

static int32 _swNic_init(	uint32 userNeedRxPkthdrRingCnt[RTL865X_SWNIC_RXRING_MAX_PKTDESC],
						uint32 userNeedRxMbufRingCnt,
						uint32 userNeedTxPkthdrRingCnt[RTL865X_SWNIC_TXRING_MAX_PKTDESC],
						uint32 clusterSize,
						proc_input_pkt_funcptr_t rxProcFunc, 
						proc_input_pkt_funcptr_t rxPreProcFunc)
{
	NIC_TRACE_IN("");

	/* Always turn OFF NIC before initialization */
	_swNic_txRxSwitch(FALSE, FALSE);
	_swNic_enableSwNicWriteTxFN(FALSE);

	/* =====================================================================
			Check the correctness of parameters
	    ===================================================================== */
	NIC_TRACE("Check the correctness of parameters");

	{
		int32 idx;
		uint32 exponent;

		NIC_TRACE("Check the pointer of parameters");
		if (	(userNeedRxPkthdrRingCnt == NULL) ||
			(userNeedTxPkthdrRingCnt == NULL)	)
		{
			NIC_ERR(	"Null pointer found when initiating NIC RxRingCnt[0x%p] TxRingCnt[0x%p]",
						userNeedRxPkthdrRingCnt,
						userNeedTxPkthdrRingCnt);

			goto errout;
		}

		NIC_TRACE("Check RX ring size");
		for ( idx = 0 ; idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; idx ++ )
		{
			NIC_INFO("User configured RX ring[%d] size: %d", idx, userNeedRxPkthdrRingCnt[idx]);
			if (	(userNeedRxPkthdrRingCnt[idx] > 0) &&
				(isPowerOf2(userNeedRxPkthdrRingCnt[idx], &exponent) == FALSE))
			{
				NIC_ERR(	"RX ring size (%d) of index [%d] is not power of 2.",
							userNeedRxPkthdrRingCnt[idx],
							idx);

				goto errout;
			}
		}

		NIC_TRACE("Check TX ring size");
		for ( idx = 0 ; idx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; idx ++ )
		{
			NIC_INFO("User configured TX ring[%d] size: %d", idx, userNeedTxPkthdrRingCnt[idx]);
			if (	(userNeedTxPkthdrRingCnt[idx] > 0) &&
				(isPowerOf2(userNeedTxPkthdrRingCnt[idx], &exponent) == FALSE))
			{
				NIC_ERR(	"TX ring size (%d) of index [%d] is not power of 2.",
							userNeedTxPkthdrRingCnt[idx],
							idx);

				goto errout;
			}
		}

		NIC_TRACE("Check mbuf ring size");
		NIC_INFO("User configured MBUF ring size: %d", userNeedRxMbufRingCnt);
		if (	(userNeedRxMbufRingCnt == 0) ||
			(isPowerOf2(userNeedRxMbufRingCnt, &exponent) == FALSE))
		{
			NIC_ERR(	"mbuf ring size (%d) is 0 or is not power of 2.",
						userNeedRxMbufRingCnt);

			goto errout;
		}

		NIC_TRACE("Check cluster size");
		if (clusterSize != m_clusterSize)
		{
			NIC_ERR(	"Cluster size (%d) is not match to the setting in MBUF system (%d).",
						clusterSize,
						m_clusterSize);

			goto errout;
		}
	}

	/* =====================================================================
			Init all variables
	    ===================================================================== */
	NIC_TRACE("Initiate ALL variables");

	{
		int32 idx, desc;
		int32 totalRxRingCnt;

		/* ============================================================
			RX ring
		    -----------------------------------------------------------------------------*/
		NIC_TRACE("Initate RX ring");

		rxPkthdrRingRunoutMask = 0;	/* No any runout exists */
		rxPkthdrRingRxDoneMask = 0; /* No any RDone exists */

		totalRxRingCnt = 0;
		for ( idx = 0 ; idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; idx ++ )
		{
			struct rtl_pktHdr *pktHdrRingHead;
			struct rtl_pktHdr *pktHdrRingTail;
			struct rtl_pktHdr *pktHdrRingEntry;

			/* -------------------------------------------------------------
					user-configuration independent variables
			    ------------------------------------------------------------- */
			rxPkthdrRingMaxIndex[idx] = 0;
			rxPkthdrRingIndex[idx] = 0;
			rxPkthdrRingLastRecvIndex[idx] = 0;
			rxPkthdrRingLastReclaimIndex[idx] = 0;
			rxPkthdrRingRunoutIndex[idx] = -1;		/* Invalid */
			rxPkthdrRingRunoutEntry[idx] = 0;

			/* -------------------------------------------------------------
					user configurated variables
			    ------------------------------------------------------------- */
			NIC_INFO("RX ring [%d] size : %d", idx, userNeedRxPkthdrRingCnt[idx]);
			rxPkthdrRingCnt[idx] = userNeedRxPkthdrRingCnt[idx];
			totalRxRingCnt += rxPkthdrRingCnt[idx];

			if (rxPkthdrRingCnt[idx] > 0)
			{
				rxPkthdrRingMaxIndex[idx] = rxPkthdrRingCnt[idx] - 1;

				/* Allocate RX descriptor ring */
				rxPkthdrRing[idx] = (uint32*)(UNCACHE(rtlglue_malloc(rxPkthdrRingCnt[idx]*sizeof(struct rtl_pktHdr *))));
				if (rxPkthdrRing[idx] == (uint32*)(UNCACHE(NULL)))
				{
					NIC_ERR("RX ring index [%d] allocation failed", idx);
					goto errout;
				}

				/* =============================================
						NIC driver would put empty Packet headers into RX ring to
						Receive packet.

						We allocate / init them here.
				    ============================================= */
				if (	mBuf_driverGetPkthdr(	rxPkthdrRingCnt[idx],
											&pktHdrRingHead,
											&pktHdrRingTail) != rxPkthdrRingCnt[idx]	)
				{
					NIC_ERR("Packet header allocation for RX ring index [%d] failed", idx);
					goto errout;
				}

				for (	desc = 0, pktHdrRingEntry = pktHdrRingHead ;
						(desc < rxPkthdrRingCnt[idx]) && pktHdrRingEntry ;
						desc ++, pktHdrRingEntry = pktHdrRingEntry->ph_nextHdr )
				{
					/* packet in RxRing must remove the flag PKTHDR_DRIVERHOLD to indicate */
					pktHdrRingEntry->ph_flags &= ~PKTHDR_DRIVERHOLD;

					/* we set descriptor index to RxRing's pkts */
					pktHdrRingEntry->ph_rxdesc = desc;
					pktHdrRingEntry->ph_rxmbufdesc = desc;
					pktHdrRingEntry->ph_rxPkthdrDescIdx = idx;

					rxPkthdrRing[idx][desc] = (uint32)( ((uint32)pktHdrRingEntry) | DESC_SWCORE_OWNED);
				}

				/* Set wrap bit of the last descriptor */
				rxPkthdrRing[idx][rxPkthdrRingMaxIndex[idx]] |= DESC_WRAP;
			}
		}
		NIC_INFO("Total RX ring count: %d", totalRxRingCnt);

		/* ============================================================
			TX ring
		    -----------------------------------------------------------------------------*/
		NIC_TRACE("Initate TX ring");
		for ( idx = 0 ; idx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; idx ++ )
		{
			/* -------------------------------------------------------------
					user-configuration independent variables
			    ------------------------------------------------------------- */
			txPkthdrRingMaxIndex[idx] = 0;
			txPkthdrRingDoneIndex[idx] = 0;
			txPkthdrRingFreeIndex[idx] = 0;
			txPkthdrRingLastIndex[idx] = 0;

			/* -------------------------------------------------------------
					user configurated variables
			    ------------------------------------------------------------- */
			NIC_INFO("TX ring [%d] size : %d", idx, userNeedTxPkthdrRingCnt[idx]);
			txPkthdrRingCnt[idx] = userNeedTxPkthdrRingCnt[idx];

			if (txPkthdrRingCnt[idx] > 0)
			{
				txPkthdrRingMaxIndex[idx] = txPkthdrRingCnt[idx] - 1;

				/* Allocate TX descriptor ring */
				txPkthdrRing[idx] = (uint32*)(UNCACHE(rtlglue_malloc(txPkthdrRingCnt[idx]*sizeof(struct rtl_pktHdr *))));
				if (txPkthdrRing[idx] == (uint32*)(UNCACHE(NULL)))
				{
					NIC_ERR("TX ring index [%d] allocation failed", idx);
					goto errout;
				}

				/* Init TX ring */
				for ( desc = 0 ; desc < txPkthdrRingCnt[idx] ; desc ++ )
				{
					txPkthdrRing[idx][desc] = DESC_RISC_OWNED;
				}

				/* Set wrap bit of the last descriptor */
				txPkthdrRing[idx][txPkthdrRingMaxIndex[idx]] |= DESC_WRAP;
			}
		}

		/* ============================================================
			MBUF ring
		    -----------------------------------------------------------------------------*/
		NIC_TRACE("Initate MBUF ring");
		{
			struct rtl_mBuf *mbufRingHead;
			struct rtl_mBuf *mbufRingTail;
			struct rtl_mBuf *mbufRingEntry;

			/* -------------------------------------------------------------
					user-configuration independent variables
			    ------------------------------------------------------------- */
			rxMbufRingIndex = 0;

			/* -------------------------------------------------------------
					user configurated variables
			    ------------------------------------------------------------- */
			NIC_INFO("MBUF ring size : %d", userNeedRxMbufRingCnt);
			rxMbufRingCnt = userNeedRxMbufRingCnt;
			rxMbufRingMaxIndex = rxMbufRingCnt - 1;

			/* Allocate MBUF descriptor ring */
			rxMbufRing = (uint32*)(UNCACHE(rtlglue_malloc(rxMbufRingCnt*sizeof(struct rtl_mBuf*))));
			if (rxMbufRing == (uint32*)(UNCACHE(NULL)))
			{
				NIC_ERR("MBUF ring allocation failed");
				goto errout;
			}

			/* =============================================
					NIC driver would put empty MBUF into MBUF ring to
					Receive packet.

					We allocate / init them here.
			    ============================================= */

			if (	mBuf_driverGet(	rxMbufRingCnt,
								&mbufRingHead,
								&mbufRingTail) != rxMbufRingCnt)
			{
				NIC_ERR("MBUF allocation for MBUF ring failed");
				goto errout;
			}

			for (	desc = 0, mbufRingEntry = mbufRingHead ;
					(desc < rxMbufRingCnt) && mbufRingEntry ;
					desc ++, mbufRingEntry = mbufRingEntry->m_next	)
			{
				uint16 headRoom;

				mbufRingEntry->m_extbuf = (uint8 *)UNCACHE(mbufRingEntry->m_extbuf);
				mbufRingEntry->m_data = (uint8 *)UNCACHE(mbufRingEntry->m_data);
				mbufRingEntry->m_rxDesc = (MBUF_RXDESC_FROM_RXRING | desc);

				headRoom = 0;

#if defined(CONFIG_RTL865X_MBUF_HEADROOM)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
				headRoom += CONFIG_RTL865X_MBUF_HEADROOM;
#endif

#ifdef SWNIC_RX_ALIGNED_IPHDR
				headRoom += 2;
#endif
				if (	(headRoom > 0) &&
					(mBuf_reserve(mbufRingEntry, headRoom)))
				{
					NIC_ERR("MBUF HeadRoom reserving for entry [%d] failed", desc);
				}

				rxMbufRing[desc] = (uint32)((uint32)mbufRingEntry | DESC_SWCORE_OWNED);
			}

			/* Set wrap bit of the last descriptor */
			rxMbufRing[rxMbufRingMaxIndex] |= DESC_WRAP;
		}

		/* ============================================================
			Others
		    -----------------------------------------------------------------------------*/
		NIC_TRACE("Initiate related variables");
		{

			if (_swNic_installRxCallBackFunc(	rxPreProcFunc,
											rxProcFunc,
											NULL,
											NULL) != SUCCESS)
			{
				NIC_ERR(	"Invalid in callBack function registration: RX (Pre)callback function: 0x%p 0x%p\n",
							rxPreProcFunc,
							rxProcFunc);
				goto errout;
			}

			if (mBuf_setNICRxRingSize(totalRxRingCnt) != SUCCESS)
			{
				NIC_ERR("MBUF set NIC RX ring process FAILED");
				goto errout;
			}

			/* Initialize interrupt statistics counter */
			rxIntNum = 0;
			rxPkthdrRunoutSolved = 0;
			txIntNum = 0;
			rxPkthdrRunoutNum = 0;
			rxMbufRunoutNum = 0;
			rxPktErrorNum = 0;
			txPktErrorNum = 0;
			rxPktCounter = 0;
			txPktCounter = 0;
			linkChanged = 0;
			maxCountinuousRxCount = 0;

#ifdef CONFIG_RTL865X_CLE
			swNic_cleshell_init();
			/* Reset counter */
			swNic_resetCounters();
#endif

#ifdef SWNIC_DEBUG
			nicDbgMesg = NIC_DEBUGMSG_DEFAULT_VALUE;
#endif
#ifdef SWNIC_EARLYSTOP
			nicRxPktBurstSize = 0;	/* Disable RX burst support */
#endif

#ifdef SWNIC_TXALIGN
			{
				char chipVersion[16];
				uint32 rev;

				GetChipVersion(chipVersion, sizeof(chipVersion), &rev);
				if ( rev < 2 )	/* Cut A/B */
				{
					_swNic_TxAlign_Support = TRUE;
				} else
				{
					_swNic_TxAlign_Support = FALSE;
				}
			}
#endif
		}

		/* --------------------------------------------------------------------------------*/
	}

	/* =====================================================================
			Hardware setup - register configuration
	    ===================================================================== */
	if (_swNic_hwSetup() != SUCCESS)
	{
		NIC_ERR("NIC hardware setup failed");
		goto errout;
	}

	_swNic_txRxSwitch(TRUE, TRUE);
	_swNic_enableSwNicWriteTxFN(TRUE);

	NIC_TRACE_OUT("");

	return SUCCESS;

errout:

	NIC_TRACE_OUT("");

	NIC_FATAL("NIC initialization FAILED!");

	return FAILED;

}

/*
@func int32		| _swNic_hwSetup				| Setup ASIC NIC configuration register.
@rvalue SUCCESS	| Setup registers successfully.
@comm
After the memory space of all ring and mbuf is allocated, this function
set NIC feature like CRC, linkchange, burstsize etc., and fill the starting 
address of all ring and mbuf to the NIC configuration register.
*/

int32 swNic_hwSetup(void)
{
	int32 retval;
	rtlglue_drvMutexLock();
	retval = _swNic_hwSetup();
	rtlglue_drvMutexUnlock();
	return retval;
}


static int32 _swNic_hwSetup(void)
{
	NIC_TRACE_IN("");

	/* Set TX ring FDP */
#ifdef CONFIG_RTL865XC
	NIC_INFO("TX Ring (CPUTPDCR0[0x%x] = 0x%p)", CPUTPDCR0, txPkthdrRing[0]);
	NIC_INFO("TX Ring (CPUTPDCR1[0x%x] = 0x%p)", CPUTPDCR1, txPkthdrRing[1]);
	WRITE_MEM32(CPUTPDCR0, (uint32)txPkthdrRing[0]);
	WRITE_MEM32(CPUTPDCR1, (uint32)txPkthdrRing[1]);
#elif defined(CONFIG_RTL865XB)
	NIC_INFO("TX Ring (CPUTPDCR[0x%x] = 0x%p)", CPUTPDCR, txPkthdrRing[0]);
	WRITE_MEM32(CPUTPDCR, (uint32)txPkthdrRing[0]);
#endif

	/* Set RX ring FDP */
#ifdef CONFIG_RTL865XC
	NIC_INFO("RX Ring (CPURPDCR0[0x%x] = 0x%p)", CPURPDCR0, rxPkthdrRing[0]);
	NIC_INFO("RX Ring (CPURPDCR1[0x%x] = 0x%p)", CPURPDCR1, rxPkthdrRing[1]);
	NIC_INFO("RX Ring (CPURPDCR2[0x%x] = 0x%p)", CPURPDCR2, rxPkthdrRing[2]);
	NIC_INFO("RX Ring (CPURPDCR3[0x%x] = 0x%p)", CPURPDCR3, rxPkthdrRing[3]);
	NIC_INFO("RX Ring (CPURPDCR4[0x%x] = 0x%p)", CPURPDCR4, rxPkthdrRing[4]);
	NIC_INFO("RX Ring (CPURPDCR5[0x%x] = 0x%p)", CPURPDCR5, rxPkthdrRing[5]);

	WRITE_MEM32(CPURPDCR0, (uint32)rxPkthdrRing[0]);
	WRITE_MEM32(CPURPDCR1, (uint32)rxPkthdrRing[1]);
	WRITE_MEM32(CPURPDCR2, (uint32)rxPkthdrRing[2]);
	WRITE_MEM32(CPURPDCR3, (uint32)rxPkthdrRing[3]);
	WRITE_MEM32(CPURPDCR4, (uint32)rxPkthdrRing[4]);
	WRITE_MEM32(CPURPDCR5, (uint32)rxPkthdrRing[5]);
#elif defined(CONFIG_RTL865XB)
	NIC_INFO("RX Ring (CPURPDCR[0x%x] = 0x%p)", CPURPDCR, rxPkthdrRing[0]);

	WRITE_MEM32(CPURPDCR, (uint32)rxPkthdrRing[0]);
#endif
	
	/* Fill Rx mbuf FDP */
#ifdef CONFIG_RTL865XC
	NIC_INFO("MBUF Ring (CPURMDCR0[0x%x] = 0x%p)", CPURMDCR0, rxMbufRing);
	WRITE_MEM32(CPURMDCR0, (uint32)rxMbufRing);
#elif defined(CONFIG_RTL865XB)
	NIC_INFO("MBUF Ring (CPURMDCR[0x%x] = 0x%p)", CPURMDCR, rxMbufRing);
	WRITE_MEM32(CPURMDCR, (uint32)rxMbufRing);
#endif
	/* NIC feature configuration */
	WRITE_MEM32(CPUICR, 0);									/* Init CPUICR */

	WRITE_MEM32(CPUICR, READ_MEM32(CPUICR)|EXCLUDE_CRC);	/* exclude CRC */

	/* Set burst size */
#ifdef CONFIG_RTL865XC
	WRITE_MEM32(CPUICR, READ_MEM32(CPUICR)|BUSBURST_128WORDS);
#elif defined (CONFIG_RTL865XB)
	WRITE_MEM32(CPUICR, READ_MEM32(CPUICR) | BUSBURST_32WORDS | MBUF_2048BYTES);
#endif

	/* Enable Link change interrupt */
	WRITE_MEM32(CPUIIMR,READ_MEM32(CPUIIMR)|LINK_CHANGE_IE);

	NIC_TRACE_OUT("");

	return SUCCESS;
}

void swNic_reset(void)
{
	rtlglue_drvMutexLock();
	_swNic_reset();
	rtlglue_drvMutexUnlock();
}

static void _swNic_reset(void)
{
	uint32 orgCPUICR;
	uint32 orgCPUIIMR;

#ifdef CONFIG_RTL865XC
	/* After RTL865xC, CPUQDM register exist. */
	uint16 orgCPUQDM[RTL865X_SWNIC_RXRING_MAX_PKTDESC];
#endif

	uint32 orgSwNicWriteTXFN;
	uint32 orgTxSwitch;
	uint32 orgRxSwitch;
	int32 idx;

	NIC_TRACE_IN("");

	orgSwNicWriteTXFN = swNicWriteTXFN;
	orgTxSwitch = txEnable;
	orgRxSwitch = rxEnable;

	/* ==========================================================
			Reset preparation.

				1. Turn OFF RX/TX
				2. Recycle remaining entries
	    ========================================================== */	

	/* We stop RX first to prevent packet receiving */
	_swNic_txRxSwitch(orgTxSwitch, FALSE);

	/* Recycle TX-ring */
	for ( idx = 0 ; idx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; idx ++ )
	{
		while (_swNic_isrTxRecycle(0, idx) > 0)
		{
			NIC_INFO("Sent-packet recycle for TX ring [%d]", idx);
		}
	}

	if (_swNic_checkRunout() != 0)
	{
		NIC_FATAL("Some RUNOUTs are not be solved before reset NIC: This packet might be queued in some-where!")
	}

	/* We stop TX after tx recycling has done */
	_swNic_txRxSwitch(FALSE, FALSE);
	_swNic_enableSwNicWriteTxFN(FALSE);

	/* Store original register configuration */
	orgCPUICR = READ_MEM32(CPUICR);
	orgCPUIIMR = READ_MEM32(CPUIIMR);

#ifdef CONFIG_RTL865XC
	orgCPUQDM[0] = READ_MEM16(CPUQDM0);
	orgCPUQDM[1] = READ_MEM16(CPUQDM1);
	orgCPUQDM[2] = READ_MEM16(CPUQDM2);
	orgCPUQDM[3] = READ_MEM16(CPUQDM3);
	orgCPUQDM[4] = READ_MEM16(CPUQDM4);
	orgCPUQDM[5] = READ_MEM16(CPUQDM5);
#endif

	/* ==========================================================
			Start NIC reset

				1. Soft-reset ASIC
				2. Reset related MIB counter
				3. Reset all ring related pointer
	    ========================================================== */	

	WRITE_MEM32(CPUICR, SOFTRST);

	{
		int32 idx, desc;

		/* ============================================================
			RX ring
		    -----------------------------------------------------------------------------*/
		NIC_TRACE("Reset RX ring");

		rxPkthdrRingRunoutMask = 0;	/* No any runout exists */
		rxPkthdrRingRxDoneMask = 0; /* No any RDone exists */

		for ( idx = 0 ; idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; idx ++ )
		{
			rxPkthdrRingIndex[idx] = 0;
			rxPkthdrRingLastRecvIndex[idx] = 0;
			rxPkthdrRingLastReclaimIndex[idx] = 0;
			rxPkthdrRingRunoutIndex[idx] = -1;		/* Invalid */
			rxPkthdrRingRunoutEntry[idx] = 0;

			if (rxPkthdrRingCnt[idx] > 0)
			{

				for (	desc = 0 ;
						desc < rxPkthdrRingCnt[idx] ;
						desc ++ )
				{
					rxPkthdrRing[idx][desc] = ((rxPkthdrRing[idx][desc] & ~DESC_OWNED_BIT) | DESC_SWCORE_OWNED);
				}

				/* Set wrap bit of the last descriptor */
				rxPkthdrRing[idx][rxPkthdrRingMaxIndex[idx]] |= DESC_WRAP;
			}
		}

		/* ============================================================
			TX ring
		    -----------------------------------------------------------------------------*/
		NIC_TRACE("Reset TX ring");
		for ( idx = 0 ; idx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; idx ++ )
		{
			txPkthdrRingDoneIndex[idx] = 0;
			txPkthdrRingFreeIndex[idx] = 0;
			txPkthdrRingLastIndex[idx] = 0;

			if (txPkthdrRingCnt[idx] > 0)
			{
				for ( desc = 0 ; desc < txPkthdrRingCnt[idx] ; desc ++ )
				{
					txPkthdrRing[idx][desc] = ((txPkthdrRing[idx][desc] & ~DESC_OWNED_BIT) | DESC_RISC_OWNED);
				}

				/* Set wrap bit of the last descriptor */
				txPkthdrRing[idx][txPkthdrRingMaxIndex[idx]] |= DESC_WRAP;
			}
		}

		/* ============================================================
			MBUF ring
		    -----------------------------------------------------------------------------*/
		NIC_TRACE("Reset MBUF ring");
		{
			rxMbufRingIndex = 0;

			for (	desc = 0 ;
					desc < rxMbufRingCnt ;
					desc ++ )
			{
				rxMbufRing[desc] = ((rxMbufRing[desc] & ~DESC_OWNED_BIT) | DESC_SWCORE_OWNED);
			}

			/* Set wrap bit of the last descriptor */
			rxMbufRing[rxMbufRingMaxIndex] |= DESC_WRAP;
		}

		/* ============================================================
			Others
		    -----------------------------------------------------------------------------*/
		NIC_TRACE("Reset related variables");
		{

			/* Initialize interrupt statistics counter */
			rxIntNum = 0;
			rxPkthdrRunoutSolved = 0;
			txIntNum = 0;
			rxPkthdrRunoutNum = 0;
			rxMbufRunoutNum = 0;
			rxPktErrorNum = 0;
			txPktErrorNum = 0;
			rxPktCounter = 0;
			txPktCounter = 0;
			linkChanged = 0;
			maxCountinuousRxCount = 0;

#ifdef CONFIG_RTL865X_CLE
			swNic_cleshell_init();
			/* Reset counter */
			swNic_resetCounters();
#endif
		}

		/* --------------------------------------------------------------------------------*/
	}

	WRITE_MEM32(CPUIIMR, orgCPUIIMR);
	WRITE_MEM32(CPUICR, orgCPUICR);

#ifdef CONFIG_RTL865XC
	WRITE_MEM16(CPUQDM0, orgCPUQDM[0]);
	WRITE_MEM16(CPUQDM1, orgCPUQDM[1]);
	WRITE_MEM16(CPUQDM2, orgCPUQDM[2]);
	WRITE_MEM16(CPUQDM3, orgCPUQDM[3]);
	WRITE_MEM16(CPUQDM4, orgCPUQDM[4]);
	WRITE_MEM16(CPUQDM5, orgCPUQDM[5]);
#endif

	_swNic_txRxSwitch(orgTxSwitch, orgRxSwitch);
	_swNic_enableSwNicWriteTxFN(orgSwNicWriteTXFN);

	NIC_TRACE_OUT("");
}

int32 swNic_descRingCleanup(void)
{
	int32 retval;

	rtlglue_drvMutexLock();
	retval = _swNic_descRingCleanup();
	rtlglue_drvMutexUnlock();

	return retval;
}

static int32 _swNic_descRingCleanup(void)
{
	NIC_ERR("========swNic_descRingCleanup ... not implement ...........\n");
	return SUCCESS;
}

int32 swNic_getRingSize(uint32 ringType, uint32 *ringSize, uint32 ringIdx)
{
	int32 retval;
	rtlglue_drvMutexLock();
	retval = _swNic_getRingSize(ringType, ringSize, ringIdx);
	rtlglue_drvMutexUnlock();
	return retval;
}

static int32 _swNic_getRingSize(uint32 ringType, uint32 *ringSize, uint32 ringIdx)
{
	switch (ringType)
	{
		case SWNIC_GETRINGSIZE_RXRING:
		{
			if (ringIdx > RTL865X_SWNIC_RXRING_MAX_PKTDESC)
			{
				goto errout;
			}
			if (ringSize)
			{
				*ringSize = rxPkthdrRingCnt[ringIdx];
			}
			return SUCCESS;
		}
		break;
		case SWNIC_GETRINGSIZE_TXRING:
		{
			if (ringIdx > RTL865X_SWNIC_TXRING_MAX_PKTDESC)
			{
				goto errout;
			}
			if (ringSize)
			{
				*ringSize = txPkthdrRingCnt[ringIdx];
			}
			return SUCCESS;
		}
		case SWNIC_GETRINGSIZE_MBUFRING:
		{
			if (ringSize)
			{
				*ringSize = rxMbufRingCnt;
			}
			return SUCCESS;
		}
	}

errout:
	return FAILED;
}

/* =========================================================
		Fast external device support
    ========================================================= */
#define RTL865X_SWNIC_FAST_EXTDEV_FUNCs

#ifdef RTL865X_SWNIC_FAST_EXTDEV_SUPPORT

int32 swNic_registerFastExtDevFreeCallBackFunc(swNic_FastExtDevFreeCallBackFuncType_f callBackFunc)
{
	int32 retval;

	rtlglue_drvMutexLock();
	retval = _swNic_registerFastExtDevFreeCallBackFunc(callBackFunc);
	rtlglue_drvMutexUnlock();

	return retval;
}

static int32 _swNic_registerFastExtDevFreeCallBackFunc(swNic_FastExtDevFreeCallBackFuncType_f callBackFunc)
{
	NIC_INFO("Set FAST extension device callBack function [0x%p] for PACKET free.", callBackFunc);
	swNic_fastExtDevFreeCallBackFunc_p = callBackFunc;
	return SUCCESS;
}

int32 swNic_fastExtDevFreePkt(struct rtl_pktHdr *pktHdr_p, struct rtl_mBuf *mbuf_p)
{
	int32 retval;

	rtlglue_drvMutexLock();
	retval = _swNic_fastExtDevFreePkt(pktHdr_p, mbuf_p);
	rtlglue_drvMutexUnlock();

	return retval;
}

static inline int32 _swNic_fastExtDevFreePkt(struct rtl_pktHdr *pktHdr_p, struct rtl_mBuf *mbuf_p)
{
	if (	(swNic_fastExtDevFreeCallBackFunc_p) &&
		(mbuf_p->m_unused1 == SWNIC_FAST_EXTDEV_ID))
	{
		(*(swNic_fastExtDevFreeCallBackFunc_p))((void*)pktHdr_p);
		return SUCCESS;
	}

	return FAILED;
}

#endif

/* =========================================================
		CLE debugging support
    ========================================================= */
#define RTL865X_SWNIC_CLESHELL_SUPPORT_FUNCs

#ifdef CONFIG_RTL865X_CLE

/*
	Command declaration
*/
cle_exec_t swNic_cmds[] = {
	{	"counter",
		"Dump NIC internal counters",
		"{ dump | clear }",
		swNic_counterCmd,
		CLE_USECISCOCMDPARSER,	
		0,
		0
	},
	{
		"loopback",
		"Layer2 loopback.",
		" { enable %d'loopback port MASK' | disable }",
		swNic_loopbackCmd,
		CLE_USECISCOCMDPARSER,	
		0,
		NULL
	},
	{
		"pktgen",
		"SW Packet generator.",
		"{ start'Start Packet TX' | count %d'Tx Pkt count' | portMask %d'Tx Port MASK' content%s' |content (ex. 01020304050601020304050608004500...)' }",
		swNic_pktGenCmd,
		CLE_USECISCOCMDPARSER,
		0,
		NULL
	},
#ifdef NIC_DEBUG_PKTDUMP
	{	"pktdump",
		"NIC pkt dump debug message on/off",
		"{ { rx'Turn on Rx pkt dump' | tx'Turn on Tx pkt dump' }  { all'Dump all' | phy'physical port' | ps'protocol stack' | ext'extension port' | off } }",
		swNic_pktdumpCmd,
		CLE_USECISCOCMDPARSER,	
		0,
		0
	},
#endif	
	{
		"hub",
		"Hub mode",
		" { enable (1~5)%d'Hub member port' | disable }",
		swNic_hubCmd,
		CLE_USECISCOCMDPARSER,	
		0,
		NULL
	},
	{	"ring",
		"NIC internal ring cmd",
		"{ txRing'dump Tx ring' | rxPkthdrRing'dump Rx ring' | rxMbufRing'dump Rx mbuf ring' | reset'Reset all rescriptor rings and NIC.' }",
		swNic_ringCmd,
		CLE_USECISCOCMDPARSER,	
		0,
		0
	},
	{	"cmd",
		"NIC internal cmds",
		"{ burst  %d'#( 2^n-1) of continuous pkt processed in ISR, 0 for infinite' | { { tx | rx } { suspend | resume } } }",
		swNic_cmd,
		CLE_USECISCOCMDPARSER,	
		0,
		0
	},
};

static int32 swNic_cleshell_init(void)
{
	/* Loopback process */
	loopBackEnable = FALSE;
	loopBackOrgMSCR = 0;
	loopbackPortMask = 0;
	loopBackOrgRxPreProcFunc = NULL;
	loopBackOrgRxProcFunc = NULL;

	/* Hub mode process */
	hubEnable = FALSE;
	hubOrgMSCR = 0;
	hubMbrMask = 0;
	hubOrgRxPreProcFunc = NULL;
	hubOrgRxProcFunc = NULL;

	/* Packet Generator process */
#ifdef NIC_DEBUG_PKTGEN
	pktGenCount = NIC_PKTGEN_DEFAULT_PKTCNT;
	pktGenTxPortMask = NIC_PKTGEN_DEFAULT_TXPORTMASK;
	pktGenMinPktLen = NIC_PKTGEN_CONTENT_LEN;
	pktGenMaxPktLen = NIC_PKTGEN_CONTENT_LEN;
	memset(pktGenContent, 0, sizeof(pktGenContent));
	memcpy(pktGenContent, defaultPktGenContent, strlen(defaultPktGenContent));
#endif

	return SUCCESS;
}

#define STRING_IS_SAME(s1, s2)	((s1) && (s2) && (strlen(s1) == strlen(s2)) && ((strcmp(s1, s2) == 0)))

static int32 swNic_cmd(uint32 userId, int32 argc, int8 **saved)
{
	int8 *nextToken;
	int32 size;

	cle_getNextCmdToken(&nextToken,&size,saved);
	if (STRING_IS_SAME(nextToken, "burst"))
	{
		cle_getNextCmdToken(&nextToken,&size,saved); 
#ifdef SWNIC_EARLYSTOP
		nicRxPktBurstSize = U32_value( nextToken );
#endif
	} else if (STRING_IS_SAME(nextToken, "rx"))
	{
		cle_getNextCmdToken(&nextToken,&size,saved); 
		if (STRING_IS_SAME(nextToken, "suspend"))
			swNic_txRxSwitch(txEnable, FALSE);
		else
			swNic_txRxSwitch(txEnable, TRUE);
	} else if (STRING_IS_SAME(nextToken, "tx"))
	{
		cle_getNextCmdToken(&nextToken,&size,saved); 	
		if (STRING_IS_SAME(nextToken, "suspend"))
			swNic_txRxSwitch(FALSE, rxEnable);
		else
			swNic_txRxSwitch(TRUE, rxEnable);
	}
	return SUCCESS;
}

#ifdef NIC_DEBUG_PKTDUMP
static int32 swNic_pktdumpCmd(uint32 userId,  int32 argc,int8 **saved)
{
	int8 *nextToken;
	int32 size;
	int32 rx = FALSE;

	cle_getNextCmdToken(&nextToken,&size,saved); 

	if (STRING_IS_SAME(nextToken, "rx"))
	{
		rx = TRUE;
	} else if (STRING_IS_SAME(nextToken, "tx"))
	{
		rx = FALSE;
	} else
	{
		return FAILED;
	}

	cle_getNextCmdToken(&nextToken,&size,saved); 

	if (STRING_IS_SAME(nextToken, "all"))
	{
		if (rx == TRUE)
		{
			nicDbgMesg |= NIC_ALL_RX_PKTDUMP;
		} else
		{
			nicDbgMesg |= NIC_ALL_TX_PKTDUMP;
		}
	} else if (STRING_IS_SAME(nextToken, "phy"))
	{
		if (rx == TRUE)
		{
			nicDbgMesg |= NIC_PHY_RX_PKTDUMP;
		} else
		{
			nicDbgMesg |= NIC_PHY_TX_PKTDUMP;
		}
	} else if (STRING_IS_SAME(nextToken, "ps"))
	{
		if (rx == TRUE)
		{
			nicDbgMesg |= NIC_PS_RX_PKTDUMP;
		} else
		{
			nicDbgMesg |= NIC_PS_TX_PKTDUMP;
		}
	} else if (STRING_IS_SAME(nextToken, "ext"))
	{
		if (rx == TRUE)
		{
			nicDbgMesg |= NIC_EXT_RX_PKTDUMP;
		} else
		{
			nicDbgMesg |= NIC_EXT_TX_PKTDUMP;
		}
	} else
	{
		if (rx == TRUE)
		{
			nicDbgMesg &= ~NIC_ALL_RX_PKTDUMP;
		} else
		{
			nicDbgMesg &= ~NIC_ALL_TX_PKTDUMP;
		}
	}
	return SUCCESS;
}


static void _swNic_pktdump(	uint32 currentCase,
								struct rtl_pktHdr *pktHdr_p,
								char *pktContent_p,
								uint32 pktLen,
								uint32 additionalInfo)
{
	switch ( currentCase )
	{
		case NIC_PS_RX_PKTDUMP:
		{
			/* additionalInfo means RX-device name */
			NIC_PRINT("Packet RX by [%s]", (char*)additionalInfo);
			break;
		}
		case NIC_PS_TX_PKTDUMP:
		{
			/* additionalInfo means TX-device name */
			NIC_PRINT("Packet TX from [%s]", (char*)additionalInfo);
			break;
		}
		case NIC_EXT_RX_PKTDUMP:
		{
			/* additionalInfo means TX-device name */
			NIC_PRINT("Packet RX from LinkID [%d]", additionalInfo);
			break;
		}
		case NIC_EXT_TX_PKTDUMP:
		{
			/* additionalInfo means TX-device name */
			NIC_PRINT("Packet TX to [%s]", (char*)additionalInfo);
			break;
		}
		case NIC_PHY_RX_PKTDUMP:
		{
			if ((pktHdr_p == NULL) || (pktHdr_p->ph_portlist >= RTL8651_MAC_NUMBER))
			{
				/* Stop packet dump */
				goto done;
			}

			NIC_PRINT(	"Packet RX from Ring [%d] Index [%d]",
						pktHdr_p->ph_rxPkthdrDescIdx,
						pktHdr_p->ph_rxdesc);

			break;
		}
		case NIC_PHY_TX_PKTDUMP:
		{
			if (	(pktHdr_p == NULL) ||
				(pktHdr_p->ph_flags & PKTHDR_HWLOOKUP))
			{
				/* Stop packet dump */
				goto done;
			}
			/* additionalInfo means TX-ring index */
			NIC_PRINT(	"Packet TX to Ring [%d]",
						additionalInfo);
			break;
		}
		default:
			goto done;
	}

	/* Common information */

	if (pktHdr_p)
	{
		struct rtl_mBuf *mbuf_p;
		int32 idx;

		NIC_PRINT("Packet header [0x%p]", pktHdr_p);
		NIC_PRINT(	"Packet info : portlist [0x%x] srcExtPortNum [%d] extPortList [0x%x] flags [0x%x]",
					pktHdr_p->ph_portlist,
					pktHdr_p->ph_srcExtPortNum,
					pktHdr_p->ph_extPortList,
					pktHdr_p->ph_flags);

		mbuf_p = pktHdr_p->ph_mbuf;
		idx = 0;
		while (mbuf_p)
		{
			NIC_PRINT(	"Mbuf[%d] : [0x%p] - MData [0x%p] Length [%d] ExtBuf [0x%p] BuffSize [%d] rx-mbuf-ring-Idx [%d]",
						idx,
						mbuf_p,
						mbuf_p->m_data,
						mbuf_p->m_len,
						mbuf_p->m_extbuf,
						mbuf_p->m_extsize,
						mbuf_p->m_rxDesc);
			mbuf_p = mbuf_p->m_next;
			idx ++;
		}
	}
	NIC_PRINT("Packet Length: %d", pktLen);
	NIC_PRINT("");

	memDump(	pktContent_p,
				(pktLen > NIC_PKTDUMP_MAXLEN) ?
					NIC_PKTDUMP_MAXLEN:
					pktLen,
				"Packet content");
	NIC_PRINT("===============================================");

done:
	return;
}

#endif

static int32 swNic_counterCmd(uint32 userId, int32 argc, int8 **saved)
{
	int8 *nextToken;
	int32 size;

	cle_getNextCmdToken(&nextToken, &size, saved);

	if (STRING_IS_SAME(nextToken, "dump"))
	{
		int32 idx;
		swNicCounter_t counter;
		uint32 iisr;
		uint32 iimr;
		uint32 iicr;

		swNic_getCounters(&counter);
		iisr = READ_MEM32(CPUIISR);
		iimr = READ_MEM32(CPUIIMR);
		iicr = READ_MEM32(CPUICR);

		rtlglue_printf("Switch NIC statistics:\n\n");
		rtlglue_printf(	"rxIntNum: %d, txIntNum: %d, pktRunout: %d, rxErr: %d, txErr: %d\n",
					counter.rxIntNum,
					counter.txIntNum,
					counter.rxPkthdrRunoutNum,
					counter.rxPktErrorNum,
					counter.txPktErrorNum);
		rtlglue_printf("Max-ContinousRX: %d\n",
					counter.maxCountinuousRxCount);
		rtlglue_printf("\n");

		rtlglue_printf("Interrupt status register 0x%08x interrupt mask 0x%08x\n", iisr, iimr);
		rtlglue_printf("\tStatus: ");
		rtlglue_printf("%s", (iisr & LINK_CHANGE_IP)?"[LinkChange]":"");
		rtlglue_printf("%s", (iisr & RX_ERR_IP_ALL)?"[RxErr]":"");
		rtlglue_printf("%s", (iisr & TX_ERR_IP_ALL)?"[TxErr]":"");
		rtlglue_printf("%s", (iisr & PKTHDR_DESC_RUNOUT_IP_ALL)?"[RxRunout]":"");
		rtlglue_printf("%s", (iisr & TX_DONE_IP_ALL)?"[TxDone]":"");
		rtlglue_printf("%s", (iisr & TX_ALL_DONE_IP_ALL)?"[TxAllDone]":"");
		rtlglue_printf("%s", (iisr & RX_DONE_IP_ALL)?"[RxDone]":"");
		rtlglue_printf("\n");
		rtlglue_printf("\tMask: ");
		rtlglue_printf("%s", (iimr & LINK_CHANGE_IE)?"[LinkChange]":"");
		rtlglue_printf("%s", (iimr & RX_ERR_IE_ALL)?"[RxErr]":"");
		rtlglue_printf("%s", (iimr & TX_ERR_IE_ALL)?"[TxErr]":"");
		rtlglue_printf("%s", (iimr & PKTHDR_DESC_RUNOUT_IE_ALL)?"[RxRunout]":"");
		rtlglue_printf("%s", (iimr & TX_DONE_IE_ALL)?"[TxDone]":"");
		rtlglue_printf("%s", (iimr & TX_ALL_DONE_IE_ALL)?"[TxAllDone]":"");
		rtlglue_printf("%s", (iimr & RX_DONE_IE_ALL)?"[RxDone]":"");
		rtlglue_printf("\n\n");

		rtlglue_printf("Interface control register 0x%08x\n", iicr);
		rtlglue_printf("\tMask:\t");
		rtlglue_printf("%s", (iicr & TXCMD)?"[TxCmd]":"");
		rtlglue_printf("%s", (iicr & RXCMD)?"[RxCmd]":"");
		rtlglue_printf("%s", (iicr & BUSBURST_32WORDS)?"[BUS Burst-32Words]":"");
		rtlglue_printf("%s", (iicr & BUSBURST_64WORDS)?"[BUS Burst-64Words]":"");
		rtlglue_printf("%s", (iicr & BUSBURST_128WORDS)?"[BUS Burst-128Words]":"");
		rtlglue_printf("%s", (iicr & BUSBURST_256WORDS)?"[BUS Burst-256Words]":"");
		rtlglue_printf("%s", (iicr & MBUF_128BYTES)?"[MBUF-128Bytes]":"");
		rtlglue_printf("%s", (iicr & MBUF_256BYTES)?"[MBUF-256Bytes]":"");
		rtlglue_printf("%s", (iicr & MBUF_512BYTES)?"[MBUF-512Bytes]":"");
		rtlglue_printf("%s", (iicr & MBUF_1024BYTES)?"[MBUF-1024Bytes]":"");
		rtlglue_printf("%s", (iicr & MBUF_2048BYTES)?"[MBUF-2048Bytes]":"");
		rtlglue_printf("%s", (iicr & TXFD)?"[TxFD]":"");
		rtlglue_printf("\n\t\t");
		rtlglue_printf("%s", (iicr & SOFTRST)?"[SoftRst]":"");
		rtlglue_printf("%s", (iicr & STOPTX)?"[StopTx]":"");
		rtlglue_printf("%s", (iicr & SWINTSET)?"[SwIntrrupt]":"");
		rtlglue_printf("%s", (iicr & LBMODE)?"[LoopBackMode]":"");
		rtlglue_printf("%s", (iicr & LB10MHZ)?"[LB10MHz]":"[LB100MHz]");
#if (defined CONFIG_RTL865XC) || defined (CONFIG_RTL865XB)
		rtlglue_printf("%s", (iicr & MITIGATION)?"[IntrMitigation]":"");
		rtlglue_printf("%s", (iicr & EXCLUDE_CRC)?"[ExcludeCRC]":"");
#ifdef CONFIG_RTL865XB
		rtlglue_printf("\n\t\t");
		rtlglue_printf("RxShift: %d", (iicr & 0xff));
#endif
#endif
		rtlglue_printf("\n\n");

		rtlglue_printf(	"Rx Packet Counter: %d Tx Packet Counter: %d\n",
					counter.rxPktCounter,
					counter.txPktCounter);
		rtlglue_printf(	"Run Out: %d Rx solved: %d linkChanged: %d\n",
					counter.rxPkthdrRunoutNum,
					counter.rxPkthdrRunoutSolved,
					counter.linkChanged);

		rtlglue_printf("RX packet header ring pointer:\n");
		for ( idx = 0 ; idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; idx ++ )
		{
			rtlglue_printf("\tCurrent Index[%d] :\t%d\n", idx, counter.currRxPkthdrRingIndex[idx]);
		}

		rtlglue_printf("TX packet header ring pinter:\n");
		for ( idx = 0 ; idx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; idx ++ )
		{
			rtlglue_printf(	"\tIndex[%d] :\tTX DONE ptr - %d\tTX FREE ptr - %d\n",
						idx,
						counter.currTxPkthdrRingDoneIndex[idx],
						counter.currTxPkthdrRingFreeIndex[idx]);
		}

		rtlglue_printf("ASIC dropped %d.\n", rtl8651_returnAsicCounter(0x4));

	}else
	{
		rtlglue_printf("Reset all NIC internal counters\n");
		swNic_resetCounters();
	}

	return SUCCESS;
}

static int32 swNic_getCounters(swNicCounter_t *counter_p)
{
	int32 idx;

	rtlglue_drvMutexLock();

	/* Statistics */
	counter_p->rxIntNum = rxIntNum;
	counter_p->txIntNum = txIntNum;
	counter_p->rxPkthdrRunoutNum = rxPkthdrRunoutNum;
	counter_p->rxPkthdrRunoutSolved = rxPkthdrRunoutSolved;
	counter_p->rxMbufRunoutNum = rxMbufRunoutNum;
	counter_p->rxPktErrorNum = rxPktErrorNum;
	counter_p->txPktErrorNum = txPktErrorNum;
	counter_p->rxPktCounter = rxPktCounter;
	counter_p->txPktCounter = txPktCounter;
	counter_p->linkChanged = linkChanged;
	counter_p->maxCountinuousRxCount = maxCountinuousRxCount;

	/* ring info */

	/* RX ring */
	for ( idx = 0 ; idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; idx ++ )
	{
		counter_p->currRxPkthdrRingIndex[idx] = rxPkthdrRingIndex[idx];
	}

	/* MBUF ring */
	counter_p->currMbufRingIndex = rxMbufRingIndex;

	/* TX ring */
	for ( idx = 0 ; idx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; idx ++ )
	{
		counter_p->currTxPkthdrRingDoneIndex[idx] = txPkthdrRingDoneIndex[idx];
		counter_p->currTxPkthdrRingFreeIndex[idx] = txPkthdrRingFreeIndex[idx];
	}

	rtlglue_drvMutexUnlock();

	return SUCCESS;
}

static int32 swNic_resetCounters(void)
{
	rxIntNum = 0;
	txIntNum = 0;
	rxPkthdrRunoutNum = 0;
	rxPkthdrRunoutSolved = 0;
	rxMbufRunoutNum = 0;
	rxPktErrorNum = 0;
	txPktErrorNum = 0;
	rxPktCounter = 0;
	txPktCounter = 0;
	linkChanged = 0;

	return SUCCESS;
}

static int32 swNic_loopbackCmd(uint32 userId, int32 argc, int8 **saved)
{
	int8 *nextToken;
	int32 size;

	cle_getNextCmdToken(&nextToken, &size, saved); 

	/* Enable Loopback command */
	if (STRING_IS_SAME(nextToken, "enable"))
	{
		if (loopBackEnable == FALSE)
		{
			int32 portNum;

			loopBackOrgMSCR = READ_MEM32(MSCR);
			WRITE_MEM32(MSCR, loopBackOrgMSCR & (~0x7));

			/* Get Loopback ports : loopback would between Loopback port 1/2 */
			cle_getNextCmdToken(&nextToken, &size, saved); 
			loopbackPortMask = U32_value(nextToken);

			_swNic_installRxCallBackFunc(	NULL,
											(proc_input_pkt_funcptr_t)re865x_testL2loopback,
											&loopBackOrgRxPreProcFunc,
											&loopBackOrgRxProcFunc);

			rtlglue_printf("Configue NIC to LOOPBACK mode:\n");
			rtlglue_printf("\tTurn off ASIC L2/3/4 functions.\n");
			rtlglue_printf("\tEnter NIC loopback mode.\n");
			if (loopbackPortMask == 0)
			{
				/*
					RX only mode: Packet RX from each port would ALWAYS be dropped.
				*/
				rtlglue_printf("\tRX only mode: Always drop RXed packets\n");
			} else if ((loopbackPortMask & (loopbackPortMask - 1)) == 0)
			{
				/*
					Single Port Loopback mode: Packet RX from one port would be bounced back to same port.
				*/
				for ( portNum = 0 ; portNum < 32 ; portNum ++ )
				{
					if ((1 << portNum) & loopbackPortMask)
					{
						rtlglue_printf("\tSingle Port loopback mode: Bounce port [%d]\n", portNum);
					}
				}
			} else
			{
				rtlglue_printf("\tMultiple Ports bridge mode: bridge pkts between port: ");
				for ( portNum = 0 ; portNum < 32 ; portNum ++ )
				{
					if ((1 << portNum) & loopbackPortMask)
					{
						rtlglue_printf("%d ", portNum);
					}
				}
				rtlglue_printf("\n");
			}

			loopBackEnable = TRUE;

		}else
		{
			/* Get Loopback ports : loopback would between Loopback port 1/2 */
			cle_getNextCmdToken(&nextToken, &size, saved); 
			loopbackPortMask = U32_value(nextToken);

			rtlglue_printf("=> Update port mask to 0x%x\n", loopbackPortMask);
		}
	} else if (loopBackOrgRxProcFunc)
	{
		if (loopBackEnable == TRUE)
		{
			rtlglue_printf("Turn ON original ASIC L2/3/4 functions. Exit NIC loopback mode\n");

			_swNic_installRxCallBackFunc(	loopBackOrgRxPreProcFunc,
											loopBackOrgRxProcFunc,
											NULL,
											NULL);

			WRITE_MEM32(MSCR, READ_MEM32(MSCR)|(loopBackOrgMSCR & 0x07));

			loopBackEnable = FALSE;
		}
	} else
	{
		return FAILED;
	}

	return SUCCESS;
}

static int32 re865x_testL2loopback(struct rtl_pktHdr *pPkt)
{
	if ((1 << pPkt->ph_portlist) & loopbackPortMask)
	{
		if ((loopbackPortMask & (loopbackPortMask - 1)) == 0)
		{	/* Single port Loopback mode */
			pPkt->ph_portlist = loopbackPortMask;
		} else
		{	/* Multiple port broadcast mode */
			pPkt->ph_portlist = loopbackPortMask & ~(pPkt->ph_portlist);
		}
	} else
	{
		goto reclaim;
	}

	pPkt->ph_portlist &= RTL8651_PHYSICALPORTMASK;

	if (	(pPkt->ph_portlist == 0) ||
		(swNic_write((void *)pPkt, 0) != SUCCESS))
	{
reclaim:
		swNic_isrReclaim(	pPkt->ph_rxPkthdrDescIdx,
							pPkt->ph_rxdesc,
							pPkt,
							pPkt->ph_mbuf);
	}

	return SUCCESS;
}

static int32 swNic_hubCmd(uint32 userId, int32 argc, int8 **saved)
{
	int8 *nextToken;
	int32 size;

	cle_getNextCmdToken(&nextToken, &size, saved); 

	if (STRING_IS_SAME(nextToken, "enable"))
	{
		if (hubEnable == FALSE)
		{
			hubOrgMSCR = READ_MEM32(MSCR);
			WRITE_MEM32(MSCR, hubOrgMSCR & (~0x7));

			hubMbrMask = 0;
			while (cle_getNextCmdToken(&nextToken, &size, saved) != FAILED)
			{
				uint32 currPort;

				currPort = U32_value(nextToken);
				if (currPort < RTL8651_MAC_NUMBER)
				{
					hubMbrMask |= (1 << currPort);
				}
			}

			_swNic_installRxCallBackFunc(	NULL,
											(proc_input_pkt_funcptr_t)swNic_hubMode,
											&hubOrgRxPreProcFunc,
											&hubOrgRxProcFunc);

			rtlglue_printf("Turn off ASIC L2/3/4 functions. Enter Hub mode. Portmask:%08x\n", hubMbrMask);			

			hubEnable = TRUE;
		}
	}else if (hubOrgRxProcFunc)
	{
		if (hubEnable == TRUE)
		{
			rtlglue_printf("Turn on ASIC L2/3/4 functions. Exit Hub mode\n");

			_swNic_installRxCallBackFunc(	hubOrgRxPreProcFunc,
											hubOrgRxProcFunc,
											NULL,
											NULL);

			WRITE_MEM32(MSCR, READ_MEM32(MSCR)|(hubOrgMSCR & 0x07));

			hubEnable = FALSE;
		}
	} else
	{
		return FAILED;
	}

	return SUCCESS;
}

static int32 swNic_hubMode(struct rtl_pktHdr *pPkt)
{
	uint16 bcastPorts;

	bcastPorts = RTL8651_PHYSICALPORTMASK & ~(1 << pPkt->ph_portlist);
	pPkt->ph_portlist = bcastPorts;

	if (swNic_write((void *)pPkt, 0) != SUCCESS)
	{
		swNic_isrReclaim(	pPkt->ph_rxPkthdrDescIdx,
							pPkt->ph_rxdesc,
							pPkt,
							pPkt->ph_mbuf);
	}

	return SUCCESS;
}

#ifdef NIC_DEBUG_PKTGEN
static int32 _swNic_pktGenDump(void)
{
	/* Dump conclusion */
	rtlglue_printf("\n");
	rtlglue_printf("@ Packet Generating count : %d\n", pktGenCount);
	rtlglue_printf("@ Packet TX-PortMASK : %d\n", pktGenTxPortMask);
	rtlglue_printf("@ Packet TX-Length (%d - %d)\n", pktGenMinPktLen, pktGenMaxPktLen);
	rtlglue_printf(" ======================================== ");
	memDump(	pktGenContent,
				sizeof(pktGenContent),
				"@ Packet Content");
	rtlglue_printf(" ======================================== ");
	rtlglue_printf("\n");

	return SUCCESS;
}

static int32 _swNic_pktGen(void)
{
	int32 idx;
	int32 totalTxDone, totalTxFailed;

	struct rtl_pktHdr *txPktHdr_p;
	struct rtl_mBuf *txMbuf_p;
	int32 headRoom = 0;
	int32 currentPktLen = pktGenMinPktLen;

	_swNic_pktGenDump();

	for ( idx = 0 ; idx < pktGenCount ; idx ++ )
	{
		int32 txRingIdx;

		/* Always re-cycle TX ring before processing */
		for ( txRingIdx = 0 ; txRingIdx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; txRingIdx ++ )
		{
			while (_swNic_isrTxRecycle(0, txRingIdx) > 0){};
		}

		/* Allocate MBUF and generate packet */
		if ((txMbuf_p = mBuf_get(MBUF_DONTWAIT, MBUFTYPE_DATA, 1)) == NULL)
		{
			rtlglue_printf("Mbuf Allocation FAILED!\n");
			return FAILED;
		}
		if (mBuf_getPkthdr(txMbuf_p, MBUF_DONTWAIT) == (struct rtl_mBuf *) NULL)
		{
			rtlglue_printf("Packet header Allocation FAILED!\n");
			mBuf_freeMbufChain(txMbuf_p);
			return FAILED;
		}
#if defined(CONFIG_RTL865X_MBUF_HEADROOM)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
		headRoom += CONFIG_RTL865X_MBUF_HEADROOM;
#endif

#ifdef SWNIC_RX_ALIGNED_IPHDR
		headRoom += 2;
#endif
		if (mBuf_reserve(txMbuf_p, headRoom))
		{
			rtlglue_printf("MBUF headRoom reservation FAILED!\n");
			mBuf_freeMbufChain(txMbuf_p);
			return FAILED;
		}

		if (txMbuf_p->)

		txPktHdr_p = txMbuf_p->m_pkthdr;

		/* Config packet content and header */
		memcpy(txMbuf_p->m_data, pktGenContent, sizeof(pktGenContent));

		txMbuf_p->m_len = currentPktLen;

		txPktHdr_p->ph_len = currentPktLen;
		txPktHdr_p->ph_portlist = pktGenTxPortMask;
		txPktHdr_p->ph_srcExtPortNum = 0;
		txPktHdr_p->ph_pppoeIdx = 0;
		txPktHdr_p->ph_pppeTagged = 0;
		txPktHdr_p->ph_LLCTagged = 0;
		txPktHdr_p->ph_vlanTagged = 0;
		txPktHdr_p->ph_proto = PKTHDR_ETHERNET;
		txPktHdr_p->ph_flags &= ~PKTHDR_FLAGS_DRVCFG_MASK;
		txPktHdr_p->ph_dvlanId = 0;
		txPktHdr_p->ph_txPriority = 0;
		txPktHdr_p->ph_txCVlanTagAutoAdd = 0;

		if (swNic_write(txPktHdr_p, 0) == SUCCESS)
		{
			rtlglue_printf(".");
		} else
		{
			rtlglue_printf("F(%d)", currentPktLen);
			mBuf_freeMbufChain(txMbuf_p);
		}

		/* decide next packet's property */
		if ( currentPktLen < pktGenMaxPktLen )
		{
			currentPktLen ++;
		} else
		{
			currentPktLen = pktGenMinPktLen;
		}
	}
	return SUCCESS;
}
#endif

static int32 swNic_pktGenCmd(uint32 userId, int32 argc, int8 **saved)
{
#ifdef NIC_DEBUG_PKTGEN
	int8 *nextToken;
	int32 size;

	cle_getNextCmdToken(&nextToken, &size, saved);

	if (STRING_IS_SAME(nextToken, "start"))
	{
		if (_swNic_pktGen() != SUCCESS)
		{
			goto errout;
		}
	} else if (STRING_IS_SAME(nextToken, "count"))
	{	/* Config current count of packet generation */
		if (cle_getNextCmdToken(&nextToken, &size, saved) == FAILED)
		{
			goto errout;
		}

		pktGenCount = U32_value(nextToken);

		_swNic_pktGenDump();

	} else if (STRING_IS_SAME(nextToken, "portMask"))
	{	/* Config current TX-Port of packet generation */
		if (cle_getNextCmdToken(&nextToken, &size, saved) == FAILED)
		{
			goto errout;
		}

		pktGenTxPortMask = U32_value(nextToken);

		_swNic_pktGenDump();
		
	} else if (STRING_IS_SAME(nextToken, "content"))
	{	/* Config current TX content */
		int32 contentLen, idx;
		if (cle_getNextCmdToken(&nextToken, &size, saved) == FAILED)
		{
			goto errout;
		}

		contentLen = strlen(nextToken);

		/* Check for correctness */
		if ( contentLen > NIC_PKTGEN_CONTENT_LEN)
		{
			rtlglue_printf("Too Long (> %d)\n", NIC_PKTGEN_CONTENT_LEN);
			goto errout;
		}

		for ( idx = 0 ; idx < contentLen ; idx ++ )
		{
			if (! isdigit(nextToken[idx]))
			{
				rtlglue_printf("Content is not correct. Each text would be [0-9] [a-f] [A-F]\n");
				goto errout;
			}
		}

		/* Start to update content */
		for ( idx = 0 ; idx < contentLen ; idx ++ )
		{
			pktGenContent[idx] = charToInt(nextToken[idx]);
		}

		_swNic_pktGenDump();

	} else
	{
		goto errout;
	}

	return SUCCESS;
errout:
#endif
	return FAILED;
}

static int32 swNic_ringCmd(uint32 userId, int32 argc, int8 **saved)
{
	int8 *nextToken;
	int32 size;

	cle_getNextCmdToken(&nextToken, &size, saved);

	if (STRING_IS_SAME(nextToken, "rxMbufRing"))
	{
		swNic_dumpMbufDescRing();
		return SUCCESS;
	} else if (STRING_IS_SAME(nextToken, "rxPkthdrRing"))
	{
		swNic_dumpPkthdrDescRing();
		return SUCCESS;
	} else if (STRING_IS_SAME(nextToken, "txRing"))
	{
		swNic_dumpTxDescRing();
		return SUCCESS;
	} else
	{
		struct rtl_mBufStatus mbs;
	
		rtlglue_printf("Reset all NIC rings...\n");
		swNic_reset();
		rtlglue_printf("  mbuf pool...\n");
		if(mBuf_getBufStat(&mbs) == SUCCESS)
		{
			rtlglue_printf( "\tSizeof\tTotal\tFree\n");
			rtlglue_printf( "Cluster\t%d\t%d\t%d\n",mbs.m_mclbytes, mbs.m_totalclusters, mbs.m_freeclusters);
			rtlglue_printf( "Mbuf\t%d\t%d\t%d\n",mbs.m_msize, mbs.m_totalmbufs, mbs.m_freembufs);
			rtlglue_printf( "Pkthdr\t%d\t%d\t%d\n",mbs.m_pkthdrsize, mbs.m_totalpkthdrs, mbs.m_freepkthdrs);
		}
	}
	return SUCCESS;
}

#endif

#ifdef NIC_DEBUG_PKTDUMP
void swNic_pktdump(	uint32 currentCase,
									struct rtl_pktHdr *pktHdr_p,
									char *pktContent_p,
									uint32 pktLen,
									uint32 additionalInfo)
{
	if ( currentCase & nicDbgMesg )
	{
		_swNic_pktdump(	currentCase,
							pktHdr_p,
							pktContent_p,
							pktLen,
							additionalInfo			);
	}
}
#endif

/* =========================================================
		Ring infomation support
    ========================================================= */
#define RTL865X_SWNIC_RING_INFO_FUNCs

void swNic_dumpPkthdrDescRing(void)
{
	int32 idx, descIdx;
	uint32 currEntry, asicIdx = 0;

	rtlglue_printf("RX ring:\n");
	rtlglue_printf("============================================\n");

	for ( descIdx = 0 ; descIdx < RTL865X_SWNIC_RXRING_MAX_PKTDESC ; descIdx ++ )
	{
		rtlglue_printf("Index [%d]\n", descIdx);
		if (rxPkthdrRingCnt[descIdx] == 0)
		{
			rtlglue_printf("\tDISABLED\n");
		} else
		{
			rtlglue_printf("\tSize (%d) Starting address 0x%p\n", rxPkthdrRingCnt[descIdx], rxPkthdrRing[descIdx]);
			asicIdx = (((uint32 *)READ_MEM32(CPURPDCR(descIdx))) - rxPkthdrRing[descIdx]);

			rtlglue_printf("\t-----------------------------------------------------\n");
			for ( idx = 0 ; idx < rxPkthdrRingCnt[descIdx] ; idx ++ )
			{
				struct rtl_pktHdr *pktHdr;
				struct rtl_mBuf *mbuf;

				currEntry = rxPkthdrRing[descIdx][idx];

				rtlglue_printf("\t%03d.", idx);
				pktHdr = (struct rtl_pktHdr*)(currEntry & ~(DESC_OWNED_BIT|DESC_WRAP));
				if ((currEntry & DESC_OWNED_BIT) == DESC_SWCORE_OWNED)
				{
					rtlglue_printf("[NULL pktHdr] ");
					mbuf = NULL;
				} else
				{
					rtlglue_printf("p:%p ", pktHdr);
					mbuf = pktHdr->ph_mbuf;
				}
				rtlglue_printf("%s ", ((currEntry & DESC_OWNED_BIT) == DESC_RISC_OWNED)?"(CPU)":"(SWC)");
				rtlglue_printf("%s ", (asicIdx == idx)?"ASIC":"    ");
				rtlglue_printf("%s ", (rxPkthdrRingLastReclaimIndex[descIdx] == idx)?"RECLAIMED":"    ");
				rtlglue_printf("%s ", (currEntry & DESC_WRAP)?"WRAP":"    ");				
				rtlglue_printf("\n");

				while (mbuf != NULL)
				{
					rtlglue_printf(	"\t\t=> m: 0x%p extB:0x%p len:%d mData:0x%p\n",
								mbuf, mbuf->m_extbuf,
								mbuf->m_len,
								mbuf->m_data);
					mbuf = mbuf->m_next;
				}

			}
			rtlglue_printf("\t-----------------------------------------------------\n");
		}
	}
}

void swNic_dumpTxDescRing(void)
{
	int32 idx, descIdx;
	uint32 currEntry, asicIdx = 0;

	rtlglue_printf("TX ring:\n");
	rtlglue_printf("============================================\n");

	for ( descIdx = 0 ; descIdx < RTL865X_SWNIC_TXRING_MAX_PKTDESC ; descIdx ++ )
	{
		rtlglue_printf("Index [%d]\n", descIdx);

		if (txPkthdrRingCnt == 0)
		{
			rtlglue_printf("\tDISABLED\n");
		} else
		{
			rtlglue_printf(	"\tSize (%d) Starting address 0x%p [%s]\n",
						txPkthdrRingCnt[descIdx],
						txPkthdrRing[descIdx],
						txPktHdrRingFull(descIdx)?"FULL":"non-FULL");
			asicIdx = (((uint32 *)READ_MEM32(CPUTPDCR(descIdx))) - txPkthdrRing[descIdx]);

			rtlglue_printf("\t-----------------------------------------------------\n");
			for ( idx = 0 ; idx < txPkthdrRingCnt[descIdx] ; idx ++ )
			{
				struct rtl_pktHdr *pktHdr;
				struct rtl_mBuf *mbuf;

				currEntry = txPkthdrRing[descIdx][idx];

				rtlglue_printf("\t%03d.", idx);
				pktHdr = (struct rtl_pktHdr*)(currEntry & ~(DESC_OWNED_BIT|DESC_WRAP));
				if ((currEntry & DESC_OWNED_BIT) == DESC_RISC_OWNED)
				{
					rtlglue_printf("[NULL pktHdr] ");
					mbuf = NULL;
				} else
				{
					rtlglue_printf(	"p:%p rxD%d rxR%d len%d sE%d eX%02x p%03x",
								pktHdr,
								pktHdr->ph_rxdesc,
								pktHdr->ph_rxPkthdrDescIdx,
								pktHdr->ph_len,
								pktHdr->ph_srcExtPortNum,
								pktHdr->ph_extPortList,
								pktHdr->ph_portlist);
					mbuf = pktHdr->ph_mbuf;
				}
				rtlglue_printf("%s ", ((currEntry & DESC_OWNED_BIT) == DESC_RISC_OWNED)?"(CPU)":"(SWC)");
				rtlglue_printf("%s ", (asicIdx == idx)?"ASIC":"    ");
				rtlglue_printf("%s ", (txPkthdrRingDoneIndex[descIdx] == idx)?"DONE":"    ");
				rtlglue_printf("%s ", (txPkthdrRingFreeIndex[descIdx] == idx)?"FREE":"    ");
				rtlglue_printf("%s ", (currEntry & DESC_WRAP)?"WRAP":"    ");				
				rtlglue_printf("\n");

				while (mbuf != NULL)
				{
					rtlglue_printf(	"\t\t=> m:0x%p rxD:%d eb:0x%p len:%d d:0x%p\n",
								mbuf,
								(mbuf->m_rxDesc & MBUF_RXDESC_FROM_RXRING)?
									(mbuf->m_rxDesc & MBUF_RXDESC_IDXMASK):
									(-1),
								mbuf->m_extbuf,
								mbuf->m_len,
								mbuf->m_data);
					mbuf = mbuf->m_next;
				}

			}
			rtlglue_printf("\t-----------------------------------------------------\n");
		}
	}
}

void swNic_dumpMbufDescRing(void)
{
	int32 idx;
	uint32 currEntry;
	uint32 asicIdx = 0;

	rtlglue_printf("MBUF ring:\n");
	rtlglue_printf("============================================\n");

	{
		if (rxMbufRingCnt == 0)
		{
			rtlglue_printf("\tDISABLED\n");
		} else
		{
			rtlglue_printf(	"\tSize (%d) Starting address 0x%p\n",
						rxMbufRingCnt,
						rxMbufRing);
#ifdef CONFIG_RTL865XC
			asicIdx = (((uint32 *)READ_MEM32(CPURMDCR0)) - rxMbufRing);
#else
			asicIdx = (((uint32 *)READ_MEM32(CPURMDCR)) - rxMbufRing);
#endif

			rtlglue_printf("\t-----------------------------------------------------\n");
			for ( idx = 0 ; idx < rxMbufRingCnt ; idx ++ )
			{
				struct rtl_mBuf *mbuf;

				currEntry = rxMbufRing[idx];

				rtlglue_printf("\t%03d.", idx);
				mbuf = (struct rtl_mBuf*)(currEntry & ~(DESC_OWNED_BIT|DESC_WRAP));
				if ((currEntry & DESC_OWNED_BIT) == DESC_SWCORE_OWNED)
				{
					rtlglue_printf("[NULL mbuf] ");
				} else
				{
					rtlglue_printf(	"m:0x%p extB:0x%p len:%d mData:0x%p ",
								mbuf, mbuf->m_extbuf,
								mbuf->m_len,
								mbuf->m_data);
				}
				rtlglue_printf("%s ", ((currEntry & DESC_OWNED_BIT) == DESC_RISC_OWNED)?"(CPU)":"(SWC)");
				rtlglue_printf("%s ", (asicIdx == idx)?"ASIC":"    ");
				rtlglue_printf("%s ", (currEntry & DESC_WRAP)?"WRAP":"    ");				
				rtlglue_printf("\n");

			}
			rtlglue_printf("\t-----------------------------------------------------\n");
		}
	}
}

/* =========================================================
		Tx-Alignment support for NIC TX.
    ========================================================= */
#define RTL865X_SWNIC_TXALIGN_FUNCs

#ifdef SWNIC_TXALIGN

/*
	Let TX packets being 4-bytes align before sending it.
	For RTL865xB Cut A/B only.
 */
static void _swNic_txAlign(struct rtl_pktHdr *pPkt, struct rtl_mBuf *pMbuf)
{
	uint16 pLen;
	pLen = pPkt->ph_len + 4;	/* CRC is included */

	if (	((pLen&0x1c) == 0) &&
		((pLen&0x03) != 0)	)
	{
		pPkt->ph_len = (pPkt->ph_len&0xFFFC) + 0x4;	/* let it become 4-byte alignment */
		pMbuf->m_len = pPkt->ph_len;
	}
}
#endif


