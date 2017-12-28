/*
* ----------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2006  
* All rights reserved.
* 
* $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/Attic/rtl865xb_swNic.h,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $
*
* Abstract: Switch core NIC header file for RTL865XB.
*
* $Author: davidm $
*
* ---------------------------------------------------------------
*/

struct rtl_pktHdr;
typedef int    (*proc_input_pkt_funcptr_t) (struct rtl_pktHdr*);

/* --------------------------------------------------------------------
 * ROUTINE NAME - swNic_init
 * --------------------------------------------------------------------
 * FUNCTION: This service initializes the switch NIC.
 * INPUT   : 
        nRxPkthdrDesc: Number of Rx pkthdr descriptors.
        nRxMbufDesc: Number of Rx mbuf descriptors.
        nTxPkthdrDesc: Number of Tx pkthdr descriptors.
        clusterSize: Size of a mbuf cluster.
        processInputPacket: Received packet processing function.
        shortCut: Short cut callback function. User can install a 
            routine which could decide whether the received packet 
            should be forwarded and (if necessary) modify the packet 
            in a very short time. If this routine returns 0, the 
            packet is immediately sent out, otherwise it is handled 
            as usual. Note that this routine must be fast, never 
            allocate or release pkthdr or mbuf, and all shared data 
            must be protected if this routine touches it, because this 
            routine will be called inside the interrupt handler.
 * OUTPUT  : None.
 * RETURN  : Upon successful completion, the function returns ENOERR. 
        Otherwise, 
		EINVAL: Invalid argument.
		ENFILE: Packet header or mbuf available.
 * NOTE    : None.
 * -------------------------------------------------------------------*/
int32 swNic_init(uint32 nRxPkthdrDesc, uint32 nRxMbufDesc, uint32 nTxPkthdrDesc, 
                        uint32 clusterSize, 
                        proc_input_pkt_funcptr_t processInputPacket, 
                        int32 shortCut(struct rtl_pktHdr *));

                        
proc_input_pkt_funcptr_t swNic_installedProcessInputPacket(proc_input_pkt_funcptr_t);


/* --------------------------------------------------------------------
 * ROUTINE NAME - swNic_intHandler
 * --------------------------------------------------------------------
 * FUNCTION: This function is the NIC interrupt handler.
 * INPUT   :
		intPending: Pending interrupts.
 * OUTPUT  : None.
 * RETURN  : None.
 * NOTE    : None.
 * -------------------------------------------------------------------*/

int32 swNic_intHandler(int32 *lastRxDescIdx);

#define SWNIC_RX_ALIGNED_IPHDR

typedef struct swNicCounter_s {
    int32 rxIntNum, txIntNum, rxPkthdrRunoutNum, rxMbufRunoutNum, rxPkthdrRunoutSolved, rxPktErrorNum, txPktErrorNum, rxPktCounter, txPktCounter;
    int32 currRxPkthdrDescIndex, currRxMbufDescIndex, currTxPkthdrDescIndex, freeTxPkthdrDescIndex;
	int32 linkChanged;
} swNicCounter_t;


void swNic_rxThread(unsigned long param);
 int32 swNic_write(void * output);
 int32 swNic_isrReclaim(uint32 rxDescIdx, struct rtl_pktHdr*pPkthdr,struct rtl_mBuf *pMbuf);

extern int8 swNic_Id[];
extern int32 miiPhyAddress;
extern spinlock_t *rtl865xSpinlock;

extern uint32	totalRxPkthdr, totalRxMbuf,totalTxPkthdr, totalTxPkthdrShift;  /*total desc count on each ring */
extern uint32 	rxMbufIndex, rxPhdrIndex, txDoneIndex, txFreeIndex, lastTxIndex; /* current desc index */
extern volatile uint32 	*RxPkthdrRing,*RxMbufRing, *TxPkthdrRing; /* descriptor ring */
#define TxPkthdrDescFull	(((txFreeIndex+1)&((1<<totalTxPkthdrShift)-1))==txDoneIndex)

extern int32  rxEnable, txEnable;


#define SWNIC_DEBUG  
#ifdef SWNIC_DEBUG 
extern int32 nicDbgMesg;
#define NIC_RX_PKTDUMP			0x15
#define NIC_TX_PKTDUMP			0x2a
#define NIC_PHY_RX_PKTDUMP	0x1
#define NIC_PHY_TX_PKTDUMP		0x2
#define NIC_EXT_RX_PKTDUMP		0x4
#define NIC_EXT_TX_PKTDUMP		0x8
#define NIC_PS_RX_PKTDUMP		0x10
#define NIC_PS_TX_PKTDUMP		0x20
#endif


#ifdef CONFIG_RTL865X_CLE
#include "cle/rtl_cle.h" 
extern cle_exec_t swNic_cmds[];
#define CMD_RTL8651_SWNIC_CMD_NUM		6
#endif

