

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
#include "swNic2.h"
#include "rtl865x/assert.h"
#include "rtl865x/asicRegs.h"
#include "rtl865x/rtl8651_layer2.h"

#include "rtl865x/rtl8651_tblAsicDrv.h"


#include "rtl865x/rtl_utils.h"


#ifdef CONFIG_RTL865X_ROMEREAL
	//Turn on In-memory "ethereal" like pkt sniffing code.
	#define START_SNIFFING rtl8651_romerealRecord
#else
	#define START_SNIFFING(x,y) do{}while(0)	
#endif


#ifdef AIRGO_FAST_PATH
	//For all WLAN router projects based on Airgo MIMO cards.
	//turn on this flag in Makefile
	#define AIRGO_MAGIC 0x3478
	typedef unsigned int (*fast_free_fn)(void * pkt);
	extern fast_free_fn rtl865x_freeMbuf_callBack_f;
#endif


int8 swNic_Id[] = "$Id: rtl865xb_swNic.c,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $";
/* descriptor ring related variables */
static proc_input_pkt_funcptr_t installedProcessInputPacket;



uint32	totalRxPkthdr, totalRxMbuf,totalTxPkthdr, totalTxPkthdrShift;  /*total desc count on each ring */
uint32 	rxMbufIndex, rxPhdrIndex, txDoneIndex, txFreeIndex,lastTxIndex; /* current desc index */
volatile uint32 	*RxPkthdrRing,*RxMbufRing, *TxPkthdrRing; /* descriptor ring, should be volatile */
static int32	rxRunoutIdx, lastReclaim, lastIntRxDescIdx;




static int32 nicTxAlignFix;

#ifdef SWNIC_DEBUG 
	int32 nicDbgMesg;
#endif
 int32  rxEnable, txEnable;

#undef SWNIC_EARLYSTOP 
#ifdef SWNIC_EARLYSTOP
static int32 nicRxEarlyStop;
static int32 nicRxAbort;
#endif

/* swnic debug counters */
static int32   rxIntNum;
static int32 linkChanged;
static int32   txIntNum;
static int32   rxMbufRunoutNum;
static int32   rxPktErrorNum;
static int32   txPktErrorNum;
static int32   rxPktCounter;
static int32   txPktCounter;
 int32 rxPkthdrRunoutNum,rxPkthdrRunoutSolved;
 int32 rxPkthdr_runout;

void swNic_rxThread(unsigned long);

void swNic_isrTxRecycle(int32);


#ifdef CONFIG_RTL865X_CLE
static int32 swNic_resetCounters(void);
#endif


 int32 swNic_write(void*);


#ifdef CONFIG_RTL865XB_EXP_PERFORMANCE_EVALUATION
static int32 _pernicStart = FALSE;
static int32 _pernicInst = TRUE;
static uint32 _pernicPktLimit = 10000;
static uint32 _pernicPktCount = 0;
static uint32 _pernicByteCount = 0;

void swNic_pernicrxStart(int32 instMode, uint32 totalPkt){
	_pernicPktCount = _pernicByteCount = 0;
	_pernicInst = instMode;
	_pernicPktLimit = totalPkt;
	_pernicStart = TRUE;
}

void swNic_pernicrxEnd(uint32 *pktCount, uint32 *byteCount){
	*pktCount = _pernicPktCount;
	*byteCount = _pernicByteCount;
	_pernicStart = FALSE;
}
#endif


//For 865xB A&B cut only. 
static void _nicTxAlignFix(struct rtl_pktHdr *pPkt, struct rtl_mBuf *pMbuf){
	uint16 pLen;
	pLen = pPkt->ph_len + 4;	/* CRC is included */
	if (((pLen&0x1c) == 0) && ((pLen&0x03) != 0))
	{
		assert(pPkt->ph_len == pMbuf->m_len);
		pPkt->ph_len = (pPkt->ph_len&0xFFFC) + 0x4;	/* let it become 4-byte alignment */
		pMbuf->m_len = pPkt->ph_len;
	}
}



int32 swNic_linkChgIntHandler(void){
//	REG32(CPUIISR) = (uint32)LINK_CHANG_IP;
	WRITE_MEM32(CPUIISR,(uint32)LINK_CHANG_IP);
	//rtlglue_printf("interrupt link change %x\n",(uint32)jiffies);
	rtl8651_updateLinkChangePendingCount();
	if(SUCCESS==rtl8651_updateLinkStatus()){
		#if defined(CONFIG_RTL865X_BICOLOR_LED)
		#ifdef BICOLOR_LED_VENDOR_BXXX
		int32 i;
		for(i=0; i<5; i++) {  
			//while(!(REG32(PHY_BASE+(i<<5)+0x4) & 0x20));
			if(READ_MEM32((PHY_BASE+(i<<5)+0x4)) & 0x4) { //link is up
				//printf("port %d phyControlRegister 0[%08x] 4[%08x]\n",i,REG32(PHY_BASE+(i<<5)),REG32(PHY_BASE+(i<<5)+4));
				//printf("Port %d Link Change!\n",i);
					if(READ_MEM32((PHY_BASE+(i<<5))) & 0x2000){
					/* link up speed 100Mbps */
					/* set gpio port  with high */
					//printf("port %d speed 100M %08x\n",i,REG32(PHY_BASE+(i<<5)));
//					REG32(PABDAT) |= (1<<(16+i));
					WRITE_MEM32(PABDAT, READ_MEM32(PABDAT) | (1<<(16+i)));
				}else{
					/* link up speed 10Mbps */
					//rtlglue_printf("linkSpeed10M\n");
					/* set gpio port  with high */
					//printf("port %d speed 10M %08x\n",i,REG32(PHY_BASE+(i<<5)));
					//REG32(PABDAT) &= ~(1<<(16+i));
					WRITE_MEM32(PABDAT, READ_MEM32(PABDAT) & ~(1<<(16+i)));
				} /* end if  100M */
			}/* end if  link up */
		}/* end for */
		#endif /* BICOLOR_LED_VENDOR_BXXX */
		#endif /* CONFIG_RTL865X_BICOLOR_LED */
		return SUCCESS;
	}else
		rtlglue_printf("Link chg int handle error!\n");
	//rtlglue_printf("interrupt link change %x\n",jiffies);	
	return FAILED;
}

int32 swNic_rxRunoutTxPending(struct rtl_pktHdr *pPkthdr){
	//exception handling
	struct rtl_pktHdr *freePkthdrListHead, *freePkthdrListTail;
	struct rtl_mBuf *freeMbufListHead, *freeMbufListTail;
	uint32  wrap=0;
#ifdef SWNIC_DEBUG

	if(nicDbgMesg)
	{
		rtlglue_printf("Desc %d (ph: %08x): Rx runout by pending Tx\n", rxPhdrIndex,(uint32)pPkthdr );

	}
#endif

	/*
		Allocate and init a new packet to to put into Rx Ring to replace the pending one.
	*/
	if ((mBuf_driverGetPkthdr(1, &freePkthdrListHead, &freePkthdrListTail))!=1)
	{
		/* Reset Watchdog to prevent from reboot */
		WRITE_MEM32(WDTCNR, READ_MEM32(WDTCNR) | WDTCLR);

		rtlglue_printf("Fatal. No more pkthdr. runout NOT solved!!\n");
		return FAILED;
	}

	if (1 != mBuf_driverGet(1, &freeMbufListHead, &freeMbufListTail))
	{
		/* Reset Watchdog to prevent from reboot */
		WRITE_MEM32(WDTCNR, READ_MEM32(WDTCNR) | WDTCLR);

		rtlglue_printf("Fatal. No more mbuf. runout NOT solved!!\n");
		mBuf_driverFreePkthdr(freePkthdrListHead, 1, 0);
		return FAILED;
	}

#ifdef RTL865X_MODEL_USER
	freeMbufListHead->m_extbuf=(uint8 *)(freeMbufListHead->m_extbuf);
	freeMbufListHead->m_data=(uint8 *)(freeMbufListHead->m_data);
#else
	freeMbufListHead->m_extbuf=(uint8 *)UNCACHE(freeMbufListHead->m_extbuf);
	freeMbufListHead->m_data=(uint8 *)UNCACHE(freeMbufListHead->m_data);
#endif	

	#if defined(CONFIG_RTL865X_MBUF_HEADROOM)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
	if(mBuf_reserve(freeMbufListHead, CONFIG_RTL865X_MBUF_HEADROOM))
	{
		rtlglue_printf("Failed when init Rx %d\n", rxPhdrIndex);
	}
	#endif


	if (rxPhdrIndex==totalRxPkthdr-1)
	{
		wrap = DESC_WRAP;
	}

	freePkthdrListHead->ph_rxdesc = rxPhdrIndex;		/* pkthdr put to RxRing, we set its rxdesc */
	freePkthdrListHead->ph_rxPkthdrDescIdx = 0;			/* we set it's RX packet descriptor ring, in RTL865xB, it's always being 0 */
	freePkthdrListHead->ph_flags&=~PKTHDR_DRIVERHOLD;	/* pkthdr put to RxRing, we remove this flag */		
	RxMbufRing[rxPhdrIndex] = (uint32) freeMbufListHead | DESC_SWCORE_OWNED | wrap;
	RxPkthdrRing[rxPhdrIndex] = (uint32) freePkthdrListHead | DESC_SWCORE_OWNED | wrap;
	/*
		Original pending packet: we modify its rxdesc to indicate
		this packet is no longer being the Rx Ring packet.
	*/
	pPkthdr->ph_rxdesc = PH_RXDESC_INDRV;
	pPkthdr->ph_rxPkthdrDescIdx = PH_RXPKTHDRDESC_INDRV;

#ifdef AIRGO_FAST_PATH
	{
		//airgo
		freeMbufListHead->m_pkthdr = freePkthdrListHead;
		freePkthdrListHead->ph_mbuf = freeMbufListHead;
		freeMbufListHead->m_next = 0;
		freePkthdrListHead->ph_reserved = 0;
		pPkthdr->ph_reserved = AIRGO_MAGIC;
	}
#endif
	
	return SUCCESS;
}


//#pragma ghs section text=".iram"
__IRAM_FWD int32 swNic_intHandler(int32 *param)
{


	int32 curDesc=-1;
	int32 sched=0;
	uint32  cpuiisr,intPending;
	spin_lock(rtl865xSpinlock);
	while(1){
		/* Read the interrupt status register */
//	    	cpuiisr=intPending = REG32(CPUIISR);
	    	cpuiisr=intPending = READ_MEM32(CPUIISR);
		// filter those intr we don't care. only those intrs with its IIMR bit set would trigger ISR.
//		intPending &= REG32(CPUIIMR);
		intPending &= READ_MEM32(CPUIIMR);
		//rtlglue_printf("mask:%x, iisr:%x, iimr:%x\n", INTPENDING_NIC_MASK,cpuiisr,REG32(CPUIIMR));
		/* Check and handle NIC interrupts */
		if (intPending & INTPENDING_NIC_MASK){
//			REG32(CPUIISR) = intPending;
			WRITE_MEM32(CPUIISR,intPending);
			if(intPending & PKTHDR_DESC_RUNOUT_IP){
//				REG32(CPUIIMR)&=~PKTHDR_DESC_RUNOUT_IE;
				WRITE_MEM32(CPUIIMR,READ_MEM32(CPUIIMR)&(~PKTHDR_DESC_RUNOUT_IE));
				sched=1;
			}
			if ( intPending &RX_DONE_IP){
//				REG32(CPUIIMR)&=~RX_DONE_IE;
				WRITE_MEM32(CPUIIMR,READ_MEM32(CPUIIMR)&~RX_DONE_IE);
				sched=1;
			}
			if (sched){
//				curDesc=(uint32 *)REG32(CPURPDCR)-RxPkthdrRing;
				curDesc=(uint32 *)READ_MEM32(CPURPDCR)-RxPkthdrRing;

				if (intPending & PKTHDR_DESC_RUNOUT_IP){
					rxPkthdrRunoutNum++;
					rxRunoutIdx=curDesc;
					rxPkthdr_runout=1;
				}

				if(curDesc-1<0)
					curDesc=totalRxPkthdr-1;
				else
					curDesc-=1;
				lastIntRxDescIdx=curDesc; //Last pkt of this run. ASIC won't run cross it before we consume this last pkt.

				if(intPending & RX_DONE_IP)
					rxIntNum++;

			}

			if ( intPending & (RX_ERR_IP|TX_ERR_IP)){
				if ( intPending& TX_ERR_IP)
					txPktErrorNum++;
				else
					rxPktErrorNum++;
			}

		}
		/* Check and handle link change interrupt */
		if (intPending & LINK_CHANG_IP) 
			swNic_linkChgIntHandler();
		if ((intPending & (INTPENDING_NIC_MASK|LINK_CHANG_IP)) == 0){
//			REG32(CPUIISR) = cpuiisr & ~(INTPENDING_NIC_MASK|LINK_CHANG_IP); //clear all uninterested intrs 
			WRITE_MEM32(CPUIISR , cpuiisr & ~(INTPENDING_NIC_MASK|LINK_CHANG_IP)); //clear all uninterested intrs 
			break;
		}
	}
	spin_unlock(rtl865xSpinlock);	
	return sched;

}



__IRAM_FWD static int32 _swNic_recvLoop(int32 last){



	volatile struct rtl_pktHdr * pPkthdr; //don't optimize
	volatile struct rtl_mBuf * pMbuf;	//don't optimize
	int32 count=0;
	do{

#ifdef CONFIG_RTL865X_MULTILAYER_BSP
		/* Louis: We shall check per packet to guarantee VoIP realtime process. */
		rtl8651_realtimeSchedule();  //cfliu: check once per interrupt, not per pkt.
#endif

		PROFILING_START(ROMEPERF_INDEX_RECVLOOP);
		/* Increment counter */
		if((RxPkthdrRing[rxPhdrIndex]&DESC_OWNED_BIT)==1){ 
			goto out;
		}
#ifdef SWNIC_EARLYSTOP		
		if(nicRxEarlyStop && ((count & nicRxEarlyStop)==nicRxEarlyStop)){//check global interrupt status
//			uint32 gisrNow = REG32(GISR);
			uint32 gisrNow = READ_MEM32(GISR);
			if(gisrNow & 0x65000000){ //Bit 30: USB, Bit 29:PCMCIA, Bit 26: PCI, Bit 24:GPIO
				nicRxAbort=1;
				goto out;						
			}
		}
#endif		


		pPkthdr = (struct rtl_pktHdr *) (RxPkthdrRing[rxPhdrIndex] & (~(DESC_OWNED_BIT | DESC_WRAP)));
		pMbuf = pPkthdr->ph_mbuf;

#ifdef CONFIG_RTL865XB_EXP_PERFORMANCE_EVALUATION
		if(_pernicStart == TRUE){
			static uint32 start, end;
			if(!_pernicPktCount){
				startCOP3Counters(_pernicInst);
				start = jiffies;
				}
			else if(_pernicPktCount == _pernicPktLimit){
				end = jiffies;
				stopCOP3Counters();
				rtlglue_printf("%d pkts. Total %d bytes, %d ms.  %u KBps\n", _pernicPktCount, _pernicByteCount, (uint32)((end-start)*10), (uint32)(_pernicByteCount/((end-start)*10)));
				_pernicStart = FALSE;
				}
			_pernicPktCount++;
			_pernicByteCount += pPkthdr->ph_len + 4;
			swNic_isrReclaim(rxPhdrIndex, pPkthdr, pMbuf);
			/* Increment index */
			if ( ++rxPhdrIndex == totalRxPkthdr )
				rxPhdrIndex = 0;
			if ( ++rxMbufIndex == totalRxMbuf )
				rxMbufIndex = 0;
			continue;
			}
#endif

		assert(pPkthdr->ph_len>0);
		/*
			(pPkthdr->ph_flags&PKTHDR_DRIVERHOLD) != 0 means :
			
				This packet is held by ROME Driver (ex. it's queued in Driver)
				or held by Tx Ring (ex. Can not be sent immediately due to CPU port were flow controlled.) instead of RxRing.
				And Rx is blocked by this pending packet.

				So we allocate another packet and put it into Rx Ring to solve this problem (done in swNic_rxRunoutTxPending()).
		*/
		if((pPkthdr->ph_flags&PKTHDR_DRIVERHOLD) != 0)
		{
			//exception handling
			swNic_rxRunoutTxPending((struct rtl_pktHdr *)pPkthdr);
			goto out;
		}
		count++;

		#if 0	/* NEVER used in Rome Driver */
		SETBITS(pPkthdr->ph_flags, PKT_INCOMING); /* packet from physical port */
		#endif

		assert(pPkthdr->ph_rxdesc == rxPhdrIndex);

		pPkthdr->ph_flags|=PKTHDR_DRIVERHOLD;	/* this packet is received/processed by Rome Driver or TxRing */


		//Transform extension port numbers to continuous number for fwd engine.

#ifdef CONFIG_RTL865X_MULTILAYER_BSP //Default run this except L2 BSP.
		//must call this API after rxPhdrIndex is assigned...
		if(rtl8651_rxPktPreprocess(pPkthdr)){
			rtlglue_printf("Drop rxDesc=%d\n",rxPhdrIndex );
			//memDump(pPkthdr->ph_mbuf->m_data,  pPkthdr->ph_len,"Loopback Pkt");
			swNic_isrReclaim(rxPhdrIndex, (struct rtl_pktHdr*)pPkthdr, (struct rtl_mBuf *)pMbuf);
		}else
#endif
		{

#ifdef SWNIC_DEBUG
			//must put after rtl8651_rxPktPreprocess()
			if(nicDbgMesg){
				if((nicDbgMesg==NIC_RX_PKTDUMP)||
					((nicDbgMesg&NIC_PHY_RX_PKTDUMP)&&(pPkthdr->ph_portlist<6))||
					((nicDbgMesg&NIC_EXT_RX_PKTDUMP)&&(pPkthdr->ph_portlist>5))
				){
					rtlglue_printf("NIC Rx [%d] L:%d P%04x s%d e%04x\n", pPkthdr->ph_rxdesc, pPkthdr->ph_len, pPkthdr->ph_portlist, pPkthdr->ph_srcExtPortNum, pPkthdr->ph_extPortList);
					memDump(pMbuf->m_data,pMbuf->m_len>64?64:pMbuf->m_len,"");
				}
			}
#endif
			START_SNIFFING( pPkthdr, 0x00000001 );
			/* Invoked installed function pointer to handle packet */
			(*installedProcessInputPacket)((struct rtl_pktHdr*)pPkthdr);
		}

		//assert(rxPhdrIndex==rxMbufIndex);
		/* Increment index */
		if ( ++rxPhdrIndex == totalRxPkthdr )
			rxPhdrIndex = 0;
		if ( ++rxMbufIndex == totalRxMbuf )
			rxMbufIndex = 0;
	}while(last>=0&&rxPhdrIndex!=last);
out:
	return count;

}

__IRAM_FWD void swNic_rxThread(unsigned long param)
{


	int32 rxDescIdx=lastIntRxDescIdx;
	int32 latestIdx;
	int32 s;
	spin_lock_irqsave(rtl865xSpinlock,s);

#if 0 /* Louis: We shall check per packet to guarantee VoIP realtime process. */
	rtl8651_timeUpdate(0);  //cfliu: check once per interrupt, not per pkt.
#endif

next_round:

	PROFILING_END(ROMEPERF_INDEX_UNTIL_RXTHREAD);/* start at irq.c:irq_dispatch() */

#ifdef SWNIC_EARLYSTOP	
	nicRxAbort=0;
#endif
	if(rxPhdrIndex!=rxDescIdx){

		rxPktCounter+=_swNic_recvLoop(rxDescIdx);
	}	

	if(rxPhdrIndex!=rxDescIdx){
		
#ifdef SWNIC_EARLYSTOP	
		if(!nicRxAbort)
#endif
		{
//			REG32(CPUIIMR)|= RX_DONE_IE;
			WRITE_MEM32(CPUIIMR ,READ_MEM32(CPUIIMR)|RX_DONE_IE);
			goto out; //abort in _swNic_recvLoop(). 
		}
	}
	//Last one to recv, but see if we can receive again first
//	latestIdx=(uint32 *)REG32(CPURPDCR)-RxPkthdrRing;
	latestIdx=(uint32 *)READ_MEM32(CPURPDCR)-RxPkthdrRing;
	if(latestIdx-1<0)
		latestIdx = totalRxPkthdr-1;
	else
		latestIdx-=1;
	
	//Did ASIC recv more pkt since we last see it?
	if(rxDescIdx!=latestIdx){
#ifdef SWNIC_EARLYSTOP	
		if(!nicRxAbort)
#endif
		{
			//yes, keep intr closed. we still have more job to do.
			rxDescIdx=latestIdx;
			goto next_round;
		}
	}else{
		//No, this is the last to recv.
//		REG32(CPUIIMR)|= RX_DONE_IE;	//enable rx intr before we recv last pkt, in case intr lost.
		WRITE_MEM32(CPUIIMR,READ_MEM32(CPUIIMR)|RX_DONE_IE);	//enable rx intr before we recv last pkt, in case intr lost.

		rxPktCounter+=_swNic_recvLoop(-1);//just recv one pkt.
	}	
	//No more pkts in this run.

out:
	
//	if(rxPkthdr_runout==0 &&(REG32(CPUIISR) & PKTHDR_DESC_RUNOUT_IP)){
	if(rxPkthdr_runout==0 &&(READ_MEM32(CPUIISR) & PKTHDR_DESC_RUNOUT_IP)){
		//Runout occurred during this run. 
//		rxRunoutIdx=(uint32 *)REG32(CPURPDCR)-RxPkthdrRing;
		rxRunoutIdx=(uint32 *)READ_MEM32(CPURPDCR)-RxPkthdrRing;
		rxPkthdr_runout=1;
		rxPkthdrRunoutNum++;
	}
	if(rxPkthdr_runout==1){
		//Before we leave, handle runout event again.
		if(txEnable)
			swNic_isrTxRecycle(1);
		if(rxRunoutIdx<0)
			rxPkthdr_runout=0;
		//reenable runout intr.
//		REG32(CPUIIMR)|=PKTHDR_DESC_RUNOUT_IE;
		WRITE_MEM32(CPUIIMR, READ_MEM32(CPUIIMR) |PKTHDR_DESC_RUNOUT_IE);
	}
	spin_unlock_irqrestore(rtl865xSpinlock,s);


}
	

__IRAM_FWD int32 swNic_isrReclaim(uint32 rxDescIdx, struct rtl_pktHdr*pPkthdr,struct rtl_mBuf *pMbuf){
	int32 retval=FAILED;


	pPkthdr->ph_len = pMbuf->m_len =0;	//reset pkt length.
	pPkthdr->ph_srcExtPortNum=pPkthdr->ph_extPortList=pPkthdr->ph_portlist=0;
	pPkthdr->ph_nextHdr = NULL;

	if(rxDescIdx<totalRxPkthdr){

		pPkthdr->ph_flags&=~PKTHDR_DRIVERHOLD;	/* we put this packet back to RxRing, so we clear the flag to indicate it */

		assert(!(RxPkthdrRing[rxDescIdx] & DESC_SWCORE_OWNED));
		assert(!(RxMbufRing[rxDescIdx] & DESC_SWCORE_OWNED));
		
#if defined(CONFIG_RTL865X_MBUF_HEADROOM)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
		//reset data pointer and reserve header room.
		pMbuf->m_data=pMbuf->m_extbuf +CONFIG_RTL865X_MBUF_HEADROOM;
#else
		pPkthdr->ph_mbuf->m_data = pPkthdr->ph_mbuf->m_extbuf;	//recover buffer pointer
#endif
		/* Reclaim packet header and mbuf descriptors. reset OWN bit */
		RxMbufRing[rxDescIdx] |= DESC_SWCORE_OWNED;
		wmb();
		RxPkthdrRing[rxDescIdx] |= DESC_SWCORE_OWNED;
		wmb();
		lastReclaim=rxDescIdx;
		retval=SUCCESS;
	}else{
		//cfliu: All pkthdrs index is by default "totalRxPkthdr", not -1 anymore
#ifdef AIRGO_FAST_PATH
		  if (pPkthdr->ph_reserved == AIRGO_MAGIC)
		  {
			pPkthdr->ph_reserved = 0;
			pPkthdr->ph_mbuf->m_next = 0;
			mBuf_freeOne(pPkthdr->ph_mbuf);
		  }else
#endif
		{
			pPkthdr->ph_flags|=PKTHDR_DRIVERHOLD;
			pPkthdr->ph_rxdesc = PH_RXDESC_INDRV;
			pPkthdr->ph_rxPkthdrDescIdx = PH_RXPKTHDRDESC_INDRV;
		}
	}
	
	return retval;
	//rtlglue_printf("%d ",rxDescIdx);
}


void swNic_isrTxRecycle(int32 flag)
{


	struct rtl_pktHdr *     pPkthdr;
	int32 rxDescIdx;
	struct rtl_mBuf *pMbuf;

	/* Continuously check OWN bit of descriptors */
	while ( ((TxPkthdrRing[txDoneIndex] & DESC_OWNED_BIT) == DESC_RISC_OWNED)&&txDoneIndex!=txFreeIndex)
	{
		/* Fetch pkthdr */
		pPkthdr = (struct rtl_pktHdr *) (TxPkthdrRing[txDoneIndex] & ~(DESC_OWNED_BIT | DESC_WRAP));
		pMbuf = pPkthdr->ph_mbuf;
		rxDescIdx = pPkthdr->ph_rxdesc;

#ifdef AIRGO_FAST_PATH

		if ((pMbuf->m_unused1==0x66)&&(rtl865x_freeMbuf_callBack_f))
		{
			//was from Airgo driver. Reclaim the buffer back to Airgo driver's internal pool
			(*rtl865x_freeMbuf_callBack_f)(pPkthdr);
		} 
		else 

#endif

		if (pPkthdr->ph_rxPkthdrDescIdx < PH_RXPKTHDRDESC_MINIDX)
		{
			int32 retval;
			pPkthdr->ph_rxdesc = PH_RXDESC_INDRV;
			pPkthdr->ph_rxPkthdrDescIdx = PH_RXPKTHDRDESC_INDRV;

		       retval = mBuf_driverFreeMbufChain(pPkthdr->ph_mbuf);
		} else
		{
			/* otherwise, recycle buffer to Rx ring. */
			swNic_isrReclaim(rxDescIdx, pPkthdr,pMbuf);
		}		
		TxPkthdrRing[txDoneIndex]&=DESC_WRAP; //this clears Tx desc but keeps WRAP bit if any.
		wmb();		
		txPktCounter++;

		/* Increment index */
		if ( ++txDoneIndex == totalTxPkthdr )
			txDoneIndex = 0;
	}
	if(rxRunoutIdx>=0&&(RxPkthdrRing[rxRunoutIdx] & DESC_OWNED_BIT) != DESC_RISC_OWNED){
		rxPkthdrRunoutSolved++;
		rxRunoutIdx=-1;
	}

}



__IRAM_FWD int32 swNic_write(void * output)
{
    	struct rtl_pktHdr * pPkt = (struct rtl_pktHdr *) output;
	struct rtl_mBuf	*pMbuf=pPkt->ph_mbuf;
	uint32 len=pPkt->ph_len;
	int32 s;

	PROFILING_START(ROMEPERF_INDEX_12);
	spin_lock_irqsave(rtl865xSpinlock,s);
	if(!txEnable){ //Tx is not enabled now
		spin_unlock_irqrestore(rtl865xSpinlock,s);

#ifdef AIRGO_FAST_PATH
		if (	(pMbuf->m_unused1==0x66) &&
			(rtl865x_freeMbuf_callBack_f))
		{
			(*rtl865x_freeMbuf_callBack_f)(pPkt);
			return SUCCESS;
		}
#endif
		return FAILED;
	}

	PROFILING_END(ROMEPERF_INDEX_12);

#ifdef CONFIG_RTL865X_MULTILAYER_BSP
	PROFILING_START(ROMEPERF_INDEX_13);

	rtl8651_txPktPostProcessing(pPkt);
	PROFILING_END(ROMEPERF_INDEX_13);
#endif

	PROFILING_START(ROMEPERF_INDEX_14);

	if(len <60 && mBuf_padding(pMbuf, 60-len, MBUF_DONTWAIT)==NULL)
	{		
		spin_unlock_irqrestore(rtl865xSpinlock,s);

#ifdef AIRGO_FAST_PATH

		if (	(pMbuf->m_unused1==0x66) &&
			(rtl865x_freeMbuf_callBack_f))
		{
			(*rtl865x_freeMbuf_callBack_f)(pPkt);

			return SUCCESS;
		}

#endif
		return FAILED;
	}
   	assert(pPkt->ph_len>=60);

	PROFILING_END(ROMEPERF_INDEX_14);
	PROFILING_START(ROMEPERF_INDEX_15);

	if (nicTxAlignFix)
	{
		_nicTxAlignFix(pPkt,pMbuf);
	}

	pPkt->ph_nextHdr = (struct rtl_pktHdr *) NULL;

	if (TxPkthdrDescFull)
	{
		PROFILING_START(ROMEPERF_INDEX_16);
		swNic_isrTxRecycle(0);//force reclaim Tx descriptors if any.
		PROFILING_END(ROMEPERF_INDEX_16);

		if (TxPkthdrDescFull)
		{	/* check again, if still full, drop the pkt. */

			spin_unlock_irqrestore(rtl865xSpinlock,s);

#ifdef AIRGO_FAST_PATH

		if (	(pMbuf->m_unused1==0x66) &&
			(rtl865x_freeMbuf_callBack_f))
		{
			(*rtl865x_freeMbuf_callBack_f)(pPkt);

			return SUCCESS;
		}

#endif
			return FAILED;
		}
	}

	START_SNIFFING( pPkt, 0x00000002 );

	//yes, we have a Tx desc fro sending this pkt.
	//mark desc as swcore own to send the packet...
	//printfByPolling("T%d ,",txFreeIndex);	
      TxPkthdrRing[txFreeIndex]|=((uint32)pPkt|DESC_OWNED_BIT);

	wmb();
	lastTxIndex=txFreeIndex;
	txFreeIndex++;
	txFreeIndex&=(1<<totalTxPkthdrShift)-1;
	spin_unlock_irqrestore(rtl865xSpinlock,s);

#ifdef SWNIC_DEBUG
	if(nicDbgMesg){
		if((nicDbgMesg==NIC_TX_PKTDUMP)||
			((nicDbgMesg&NIC_PHY_TX_PKTDUMP)&&(pPkt->ph_srcExtPortNum==0))||
			((nicDbgMesg&NIC_EXT_TX_PKTDUMP)&&(pPkt->ph_srcExtPortNum))
		){
			rtlglue_printf("NIC Tx [%d] L:%d P%04x s%d e%04x\n", pPkt->ph_rxdesc, pPkt->ph_len, pPkt->ph_portlist, pPkt->ph_srcExtPortNum, pPkt->ph_extPortList);
			memDump(pMbuf->m_data,pMbuf->m_len>64?64:pMbuf->m_len,"");
		}
	}
#endif
//rtlglue_printf("%s %d portlist=%x srcextportnum=%x extPortlist=%x vlanidx=%x vlanTagged=%x flags=%x\n\n",__FUNCTION__,__LINE__,pPkt->ph_portlist,pPkt->ph_srcExtPortNum,pPkt->ph_extPortList,pPkt->ph_vlanIdx,pPkt->ph_vlanTagged,pPkt->ph_flags);	

//	REG32(CPUICR) |= TXFD;
	WRITE_MEM32(CPUICR , READ_MEM32(CPUICR) | TXFD );
	
	PROFILING_END(ROMEPERF_INDEX_15);
	return SUCCESS;
}



int32 swNic_txRxSwitch(uint32 tx, uint32 rx){
	uint32 icr = REG32(CPUICR), imr = REG32(CPUIIMR);

		
	if(tx==0){
		icr &= ~TXCMD;
		imr &= ~TX_ERR_IE;
	}else{
		icr |= TXCMD;
		imr |= TX_ERR_IE;
	}
	if(rx==0){
		icr&=~RXCMD;
		imr &= ~(RX_ERR_IE | PKTHDR_DESC_RUNOUT_IE |  RX_DONE_IE);
	}else{
		icr |= RXCMD;
		imr |=(RX_ERR_IE | PKTHDR_DESC_RUNOUT_IE |  RX_DONE_IE);
	}
	rxEnable=rx;
	txEnable=tx;
	REG32(CPUICR) = icr;
	REG32(CPUIIMR) = imr;
	return SUCCESS;
}


//#pragma ghs section text=default
int32 swNic_setup(uint32 pkthdrs, uint32 mbufs, uint32 txpkthdrs)
{
	struct rtl_pktHdr *freePkthdrListHead,*freePkthdrListTail;
	struct rtl_mBuf *freeMbufListHead, *freeMbufListTail;
	int i;
	 /* Disable Rx & Tx ,bus burst size, etc */
	swNic_txRxSwitch(0,0);

#ifdef SWNIC_EARLYSTOP
	nicRxEarlyStop=0;
#endif
	/* Initialize index of Tx pkthdr descriptor */
	txDoneIndex = 0;
	txFreeIndex = 0;

	/* Allocate rx pkthdrs */
	if (pkthdrs!=mBuf_driverGetPkthdr(pkthdrs, &freePkthdrListHead, &freePkthdrListTail))
	{
		rtlglue_printf("Can't allocate all pkthdrs\n");
		return EINVAL;
	}
	assert(freePkthdrListHead);
	assert(freePkthdrListTail);

	/* Allocate rx mbufs and clusters */
	if (mbufs!=mBuf_driverGet(mbufs, &freeMbufListHead, &freeMbufListTail))
	{
		rtlglue_printf("Can't allocate all mbuf/clusters\n");
		return EINVAL;
	}
	assert(freeMbufListHead);
	assert(freeMbufListTail);

	/* =================================================== */

	/* Initialize Tx packet header descriptors */
	for (i=0; i<txpkthdrs; i++)
	{
		TxPkthdrRing[i] = DESC_RISC_OWNED;
	}

	/* Set wrap bit of the last descriptor */
	TxPkthdrRing[txpkthdrs - 1] |= DESC_WRAP;

	/* Fill Tx packet header FDP */
	/* REG32(CPUTPDCR) = (uint32) TxPkthdrRing; */
	WRITE_MEM32(CPUTPDCR,(uint32) TxPkthdrRing);

	/* =================================================== */

	/* Initialize index of current Rx pkthdr descriptor */
	rxPhdrIndex = 0;

	/* Initialize Rx packet header descriptors */
	for (i=0; i<pkthdrs; i++)
	{
		struct rtl_pktHdr *currPktHdr;

		assert( freePkthdrListHead );

		/* packet in RxRing must remove the flag PKTHDR_DRIVERHOLD to indicate */
		freePkthdrListHead->ph_flags &= ~PKTHDR_DRIVERHOLD;
		/* we set descriptor index to RxRing's pkts */
		freePkthdrListHead->ph_rxdesc = i;
		freePkthdrListHead->ph_rxPkthdrDescIdx = 0;

		RxPkthdrRing[i] = (uint32) freePkthdrListHead | DESC_SWCORE_OWNED;

		currPktHdr = freePkthdrListHead;
		if ( (freePkthdrListHead = freePkthdrListHead->ph_nextHdr) == NULL )
		{
			freePkthdrListTail = NULL;
		}
		/* System would ALWAYS clear "ph_nextHdr" after setting complete: Rome Driver would use it !. */
		currPktHdr->ph_nextHdr = NULL;
	}

	/* Set wrap bit of the last descriptor */
	RxPkthdrRing[pkthdrs - 1] |= DESC_WRAP;

	/* Fill Rx packet header FDP */
	/* REG32(CPURPDCR) = (uint32) RxPkthdrRing; */
	WRITE_MEM32(CPURPDCR,(uint32)RxPkthdrRing);

	/* =================================================== */

	/* Initialize index of current Rx pkthdr descriptor */
	rxMbufIndex = 0;

	/* Initialize Rx mbuf descriptors */
	for (i=0; i<mbufs; i++)
	{
		assert( freeMbufListHead );

		RxMbufRing[i] = (uint32) freeMbufListHead | DESC_SWCORE_OWNED;
		freeMbufListHead->m_extbuf=(uint8 *)UNCACHE(freeMbufListHead->m_extbuf);
		freeMbufListHead->m_data=(uint8 *)UNCACHE(freeMbufListHead->m_data);

#if defined(CONFIG_RTL865X_MBUF_HEADROOM)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
		if(mBuf_reserve(freeMbufListHead, CONFIG_RTL865X_MBUF_HEADROOM))
		{
			rtlglue_printf("Failed when init Rx %d\n", i);
		}
#endif
		if ( (freeMbufListHead = freeMbufListHead->m_next) == NULL )
		{
			freeMbufListTail = NULL;
		}
	}

	/* Set wrap bit of the last descriptor */
	RxMbufRing[mbufs - 1] |= DESC_WRAP;

	/* Fill Rx mbuf FDP */
	/* REG32(CPURMDCR) = (uint32) RxMbufRing; */
	WRITE_MEM32(CPURMDCR,(uint32) RxMbufRing);

	/* REG32(CPUICR) =0; */
	WRITE_MEM32(CPUICR,0);

#ifdef CONFIG_RTL865XB
	{
		char chipVersion[16];
		uint32 align=0, rev;

//		REG32(CPUICR)|=EXCLUDE_CRC;
		WRITE_MEM32(CPUICR,READ_MEM32(CPUICR)|EXCLUDE_CRC);
		
		GetChipVersion(chipVersion, sizeof(chipVersion), &rev);
		if(chipVersion[strlen(chipVersion)-1]=='B')
		{
			//865xB chips support free Rx align from 0~256 bytes
			#ifdef SWNIC_RX_ALIGNED_IPHDR					
			align+=2;
			#endif
//			REG32(CPUICR)|=align; 
			WRITE_MEM32(CPUICR,READ_MEM32(CPUICR)|align);
			rtlglue_printf("Rx shift=%x\n",READ_MEM32(CPUICR));
			if(rev<2) //865xB  A & B cut needs to do Tx align fix for certain length of packets.
				nicTxAlignFix=1;
		}
	}
#endif

	/* =================================================== */

	/* Enable Rx & Tx. Config bus burst size and mbuf size. */
	/* REG32(CPUICR) |= BUSBURST_32WORDS | MBUF_2048BYTES; */
	WRITE_MEM32(CPUICR,READ_MEM32(CPUICR)|BUSBURST_32WORDS | MBUF_2048BYTES);
	/* REG32(CPUIIMR) |= LINK_CHANG_IE; */
	WRITE_MEM32(CPUIIMR,READ_MEM32(CPUIIMR)|LINK_CHANG_IE);
	swNic_txRxSwitch(1,1);

	return SUCCESS;
}



int32 swNic_descRingCleanup(void){

	int i;
	swNic_txRxSwitch(0,0);
    /* cleanup Tx packet header descriptors */
    for (i=0; i<totalTxPkthdr; i++){
		struct rtl_pktHdr *ph;
		TxPkthdrRing[i]&= ~DESC_SWCORE_OWNED; //mark it own by CPU
		ph = (struct rtl_pktHdr *) (TxPkthdrRing[i] & ~ (0x3));
		if(ph){
			//has somthing send or sent...
			struct rtl_mBuf *m;
			uint32 rxPkthdrRingIdx;

			rxPkthdrRingIdx = ph->ph_rxPkthdrDescIdx;

			ph->ph_rxdesc = PH_RXDESC_INDRV; 
			ph->ph_rxPkthdrDescIdx = PH_RXPKTHDRDESC_INDRV;

			m = ph->ph_mbuf;

			assert(m);
			if (rxPkthdrRingIdx < PH_RXPKTHDRDESC_MINIDX)
			{
				/* came from protocol stack or fwd engine */
				SETBITS(m->m_flags, MBUF_USED);
				mBuf_freeMbufChain(m);
			}
		}
		TxPkthdrRing[i] &= 0x3;//clean desc on Rx Pkthdr ring
  }	

    /* cleanup Rx packet header descriptors */
    for (i=0; i<totalRxPkthdr; i++)
{
		struct rtl_pktHdr *ph;
		struct rtl_mBuf *m;
		RxPkthdrRing[i]&= ~DESC_SWCORE_OWNED; //mark it own by CPU
		RxMbufRing[i]&= ~DESC_SWCORE_OWNED;
		ph = (struct rtl_pktHdr *) (RxPkthdrRing[i] & ~ (0x3));
		m = (struct rtl_mBuf *)(RxMbufRing[i] & ~ (0x3));
		ph->ph_mbuf=m;
		m->m_pkthdr=ph;
		RxPkthdrRing[i] &= 0x3;//clean desc on Rx Pkthdr ring
		RxMbufRing[i]&=0x3;
		assert(ph);

		ph->ph_rxdesc = PH_RXDESC_INDRV;
		ph->ph_rxPkthdrDescIdx = PH_RXPKTHDRDESC_INDRV;
		m->m_next = NULL;

		SETBITS(ph->ph_flags, PKTHDR_USED);
		SETBITS(m->m_flags, MBUF_USED|MBUF_PKTHDR);
		mBuf_freeMbufChain(ph->ph_mbuf);
    }
    	
    /////////////////////////////////////////////////	
    /* Initialize index of current Rx pkthdr descriptor */
    rxMbufIndex = rxPhdrIndex = txDoneIndex = txFreeIndex = 0;
//	REG32(CPURMDCR) = 0;
//	REG32(CPURPDCR) = 0;
//	REG32(CPUTPDCR) = 0;	
	WRITE_MEM32(CPURMDCR,0);
	WRITE_MEM32(CPURPDCR,0);
	WRITE_MEM32(CPUTPDCR,0);
	
	return SUCCESS;

}

int32 swNic_init(uint32 nRxPkthdrDesc, uint32 nRxMbufDesc, uint32 nTxPkthdrDesc, 
                        uint32 clusterSize, 
                        proc_input_pkt_funcptr_t processInputPacket, 
                        int32 reserved(struct rtl_pktHdr *))
{
	uint32  exponent;

	//check input parameters.
	if(isPowerOf2(nRxPkthdrDesc, &exponent)==FALSE)
		return EINVAL;
	if(isPowerOf2(nRxMbufDesc, &exponent)==FALSE)
		return EINVAL;
	if(isPowerOf2(nTxPkthdrDesc, &totalTxPkthdrShift)==FALSE)
		return EINVAL;
    	if (processInputPacket == NULL)
		return EINVAL;

	//we only accept 2048 byte cluster now.
	if(clusterSize!=m_clusterSize) //mbuf should be init first and cluster size must equal to the number used in mbuf init
		return EINVAL;
	assert(m_clusterSize==2048); //mBuf should be init with 2048 as cluster value
	totalRxPkthdr = nRxPkthdrDesc;
	totalRxMbuf = nRxMbufDesc;
	totalTxPkthdr = nTxPkthdrDesc;
	rxRunoutIdx=-1;
	installedProcessInputPacket = processInputPacket;
	rxPkthdr_runout=0;
	nicTxAlignFix=0;
	nicDbgMesg=0;
	
	/* Allocate Rx/Tx descriptor ring.*/
#ifdef RTL865X_MODEL_USER
	RxPkthdrRing=(uint32 *)((uint32)rtlglue_malloc(totalRxPkthdr * sizeof(struct rtl_pktHdr *)));
	RxMbufRing = (uint32 *)((uint32)rtlglue_malloc(totalRxMbuf * sizeof(struct rtl_mBuf *)));
	TxPkthdrRing = (uint32 *)((uint32)rtlglue_malloc(totalTxPkthdr * sizeof(struct rtl_pktHdr *)));
#else
	RxPkthdrRing=(uint32 *)(UNCACHE_MASK|(uint32)rtlglue_malloc(totalRxPkthdr * sizeof(struct rtl_pktHdr *)));
	RxMbufRing = (uint32 *)(UNCACHE_MASK|(uint32)rtlglue_malloc(totalRxMbuf * sizeof(struct rtl_mBuf *)));
	TxPkthdrRing = (uint32 *)(UNCACHE_MASK|(uint32)rtlglue_malloc(totalTxPkthdr * sizeof(struct rtl_pktHdr *)));
#endif	

	if(!RxPkthdrRing||!RxMbufRing||!TxPkthdrRing)
		return EINVAL;
	mBuf_setNICRxRingSize(totalRxPkthdr);
	/* Initialize interrupt statistics counter */
	rxIntNum = rxPkthdrRunoutSolved= txIntNum = rxPkthdrRunoutNum = rxMbufRunoutNum = rxPktErrorNum = txPktErrorNum = 0;
	rxPktCounter = txPktCounter = linkChanged=0;

	//All Ring buffers allocated. Now initialize all rings and swNic itself.
	return  swNic_setup(totalRxPkthdr,totalRxMbuf,totalTxPkthdr);
}



/*************************************************************************
*   FUNCTION                                                              
*       swNic_installedProcessInputPacket                                         
*                                                                         
*   DESCRIPTION                                                           
*       This function installs given function as the handler of received  
*       packets.
*                                                                         
*   INPUTS                                                                
*       nRxPkthdrDesc   Number of Rx pkthdr descriptors.
*       nRxMbufDesc     Number of Tx mbuf descriptors.
*       nTxPkthdrDesc   Number of Tx pkthdr descriptors.
*       clusterSize     Size of cluster.
*                                                                         
*   OUTPUTS                                                               
*       Status.
*************************************************************************/
proc_input_pkt_funcptr_t swNic_installedProcessInputPacket(
                            proc_input_pkt_funcptr_t processInputPacket)
{
    proc_input_pkt_funcptr_t ret = installedProcessInputPacket;
    installedProcessInputPacket = processInputPacket;
    
    return ret;
}



#ifdef CONFIG_RTL865X_CLE
static int32 swNic_getCounters(swNicCounter_t * counter) {
    counter->rxIntNum = rxIntNum;
    counter->txIntNum = txIntNum;
    counter->rxPkthdrRunoutNum = rxPkthdrRunoutNum;
	counter->rxPkthdrRunoutSolved=rxPkthdrRunoutSolved;
    counter->rxMbufRunoutNum = rxMbufRunoutNum;
    counter->rxPktErrorNum = rxPktErrorNum;
    counter->txPktErrorNum = txPktErrorNum;
    counter->rxPktCounter = rxPktCounter;
    counter->txPktCounter = txPktCounter;


	
    counter->currRxPkthdrDescIndex = rxPhdrIndex;
    counter->currRxMbufDescIndex = rxMbufIndex;
    counter->currTxPkthdrDescIndex = txDoneIndex;
    counter->freeTxPkthdrDescIndex = txFreeIndex;

	
    counter->rxIntNum = rxIntNum;
    counter->txIntNum = txIntNum;
	counter->linkChanged = linkChanged;
    return SUCCESS;
}
static int32 swNic_resetCounters(void) {
	rxIntNum= txIntNum=rxPkthdrRunoutNum=rxPkthdrRunoutSolved=rxMbufRunoutNum=0;
	rxPktErrorNum= txPktErrorNum = rxPktCounter= txPktCounter= 0;

	rxPhdrIndex=rxMbufIndex=txDoneIndex=txFreeIndex=0;

	rxIntNum=txIntNum=0;
    return SUCCESS;
}

static void swNic_dumpPkthdrDescRing(void){

	uint32  i, *temp=(uint32 *)rtlglue_malloc(sizeof(uint32)*totalRxPkthdr);
	uint32 value,asicIdx = ((uint32 *)READ_MEM32(CPURPDCR))-RxPkthdrRing;
	for(i=0;i<totalRxPkthdr;i++)
		temp[i]=RxPkthdrRing[i];

	rtlglue_printf("Rx phdr ring starts at 0x%x\n",(uint32)RxPkthdrRing );

	for(i=0;i<totalRxPkthdr;i++){
		struct rtl_pktHdr *ph;
		struct rtl_mBuf *m;
		value=temp[i];
		ph=(struct rtl_pktHdr *)(value &~0x3);
		rtlglue_printf("%03d.",(uint16)i);
		if(ph){
			rtlglue_printf("p:%08x ",value &~0x3);
			m=ph->ph_mbuf;
			if(m)
				rtlglue_printf("m:%08x c:%08x d:%08x", (uint32)m, (uint32)m->m_extbuf, (uint32)m->m_data);
			else
				rtlglue_printf("No mbuf!! ");
		}else
			rtlglue_printf("No pkthdr!! ");
		rtlglue_printf("%s ", ((value & 1)== 0)?"(CPU)":"(SWC)");
		if(asicIdx==i)
			rtlglue_printf("ASIC ");
		if(rxPhdrIndex==i)
			rtlglue_printf("Rx ");
		if(lastReclaim==i)
			rtlglue_printf("Reclaim ");
		
		if((value & 2)== 2){
			rtlglue_printf("WRAP!!\n");
			return;
		}else
			rtlglue_printf("\n");
	}
	rtlglue_free(temp);

}


void swNic_dumpTxDescRing(void){	

	int32 i;
	uint32 value, asicIdx = ((uint32 *)READ_MEM32(CPUTPDCR))-TxPkthdrRing;
	rtlglue_printf("Tx phdr ring starts at 0x%x\n",(uint32)TxPkthdrRing );
	for(i=0; i<totalTxPkthdr; i++){
		volatile struct rtl_pktHdr *ph;
		volatile struct rtl_mBuf *m;
		value=TxPkthdrRing[i];
		ph=(struct rtl_pktHdr *)(value &~0x3);
		if(ph){
			rtlglue_printf("%3d. p%08x ", i,(uint32)ph);
			m=ph->ph_mbuf;
			if(m)
				rtlglue_printf("m:%08x d:%08x",(uint32)m,(uint32)m->m_data);
			else
				rtlglue_printf("m=NULL");
		}else
			rtlglue_printf("%3d. p:NULL", i);
		rtlglue_printf("%s ", ((value & 1)== 0)?"(CPU)":"(SWC)");
		if(ph){
			rtlglue_printf("id:%d ", ph->ph_rxdesc);
			rtlglue_printf("s%d:e%02x:p%03x", ph->ph_srcExtPortNum, ph->ph_extPortList, ph->ph_portlist);
		}
		if(asicIdx==i)
			rtlglue_printf("ASIC ");
		if(txDoneIndex==i)
			rtlglue_printf("Done ");
		if(txFreeIndex==i)
			rtlglue_printf("Next ");
		if((value & 2)== 2){
			rtlglue_printf("WRAP!!\n");
			return;
		}else
			rtlglue_printf("\n");
	}
	return;


}

static int32 loopbackPort1,loopbackPort2;
static void re865x_testL2loopback(struct rtl_pktHdr * pPkt)
{
	if(pPkt->ph_portlist ==loopbackPort1)
		pPkt->ph_portlist = 1<<loopbackPort2;
	else
		pPkt->ph_portlist = 1<<loopbackPort1;

	swNic_write((void *) pPkt);

}


static proc_input_pkt_funcptr_t old_funcptr=NULL;
static int32	swNic_loopbackCmd(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;
	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp(nextToken, "enable") == 0){
//		REG32(MSCR)&=~0x7;//disable L2~L4
		WRITE_MEM32(MSCR,READ_MEM32(MSCR)&(~0x7));
		cle_getNextCmdToken(&nextToken,&size,saved); 
		loopbackPort1=U32_value(nextToken);
		cle_getNextCmdToken(&nextToken,&size,saved); 
		loopbackPort2=U32_value(nextToken);
	     old_funcptr = swNic_installedProcessInputPacket((proc_input_pkt_funcptr_t)re865x_testL2loopback);
		rtlglue_printf("Turn off ASIC L2/3/4 functions. Enter NIC loopback mode. Bridge pkts between port %d and %d\n", loopbackPort1, loopbackPort2);
	}else if(old_funcptr){
		rtlglue_printf("Turn on ASIC L2/3/4 functions. Exit NIC loopback mode\n");
	       swNic_installedProcessInputPacket(old_funcptr);
//		REG32(MSCR)|=0x7;//enable L2~L4
		WRITE_MEM32(MSCR,READ_MEM32(MSCR)|0x7);
	}else 
		return FAILED;
	return SUCCESS;
}

static uint32 hubMbrmask;

static void swNic_hubMode(struct rtl_pktHdr * pPkt)
{
	uint16 bcastPorts = 0x1f & ~(1<<pPkt->ph_portlist);
	pPkt->ph_portlist = bcastPorts;

	swNic_write((void *) pPkt);

}


static int32	swNic_hubCmd(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;
	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp(nextToken, "enable") == 0){
		hubMbrmask=0;
//		REG32(MSCR)&=~0x7;//disable L2~L4
		WRITE_MEM32(MSCR,READ_MEM32(MSCR)&(~0x7));
		while(cle_getNextCmdToken(&nextToken,&size,saved) !=FAILED){
			uint32 thisPort;
			thisPort=U32_value(nextToken);
			if(thisPort<5)
				hubMbrmask|=(1<<thisPort);
		}
	     old_funcptr = swNic_installedProcessInputPacket((proc_input_pkt_funcptr_t)swNic_hubMode);
		rtlglue_printf("Turn off ASIC L2/3/4 functions. Enter Hub mode. Portmask:%08x\n", hubMbrmask);
	}else if(old_funcptr){
		rtlglue_printf("Turn on ASIC L2/3/4 functions. Exit Hub mode\n");
	       swNic_installedProcessInputPacket(old_funcptr);
//		REG32(MSCR)|=0x7;//enable L2~L4
		WRITE_MEM32(MSCR,READ_MEM32(MSCR)|0x7);
	}else 
		return FAILED;
	return SUCCESS;
}


static int32	swNic_counterCmd(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;

	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp(nextToken, "dump") == 0){
		swNicCounter_t counter;
		rtlglue_printf("Switch NIC statistics:\n\n");
		swNic_getCounters(&counter);
		rtlglue_printf("rxIntNum: %u, txIntNum: %u, pktRunout: %u, , rxErr: %u, txErr: %u\n",
			counter.rxIntNum, counter.txIntNum, counter.rxPkthdrRunoutNum,  counter.rxPktErrorNum, counter.txPktErrorNum);
		rtlglue_printf("Interrupt register 0x%08x interrupt mask 0x%08x\n", READ_MEM32(CPUIISR), READ_MEM32(CPUIIMR));
		rtlglue_printf("Rx Packet Counter: %d Tx Packet Counter: %d\n", counter.rxPktCounter, counter.txPktCounter);
		rtlglue_printf("Run Out: %d Rx solved: %d linkChanged: %d\n", counter.rxPkthdrRunoutNum, counter.rxPkthdrRunoutSolved, counter.linkChanged);


		rtlglue_printf("currRxPkthdrDescIndex: %d currRxMbufDescIndex: %d\n",counter.currRxPkthdrDescIndex,counter.currRxMbufDescIndex);
		rtlglue_printf("currTxPkthdrDescIndex: %d freeTxPkthdrDescIndex: %d\n",counter.currTxPkthdrDescIndex, counter.freeTxPkthdrDescIndex);

		rtlglue_printf("ASIC dropped %d.\n", rtl8651_returnAsicCounter(0x4));

	}else{
		rtlglue_printf("Reset all NIC internal counters\n");
		swNic_resetCounters();

	}
	return SUCCESS;
}

static int32	swNic_cmd(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;

	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp(nextToken, "burst") == 0){
		cle_getNextCmdToken(&nextToken,&size,saved); 
#ifdef SWNIC_EARLYSTOP
		nicRxEarlyStop = U32_value( nextToken );
#endif
	}else if(strcmp(nextToken, "rx") == 0){
		cle_getNextCmdToken(&nextToken,&size,saved); 
		if(strcmp(nextToken, "suspend") == 0)
			swNic_txRxSwitch(txEnable, 0);
		else
			swNic_txRxSwitch(txEnable, 1);
	}else if(strcmp(nextToken, "tx") == 0){
		cle_getNextCmdToken(&nextToken,&size,saved); 	
		if(strcmp(nextToken, "suspend") == 0)
			swNic_txRxSwitch(0, rxEnable);
		else
			swNic_txRxSwitch(1, rxEnable);
	}
	return SUCCESS;
}

#ifdef SWNIC_DEBUG
static int32	swNic_pktdumpCmd(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;
	int rx=0;
	cle_getNextCmdToken(&nextToken,&size,saved); 

	if(strcmp(nextToken, "rx") == 0){
		rx=1;
	}else if(strcmp(nextToken, "tx") == 0){
		rx=0;
	}

	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp(nextToken, "all") == 0){
		if(rx)
			nicDbgMesg=NIC_RX_PKTDUMP;			
		else
			nicDbgMesg=NIC_TX_PKTDUMP;
	}else if(strcmp(nextToken, "phy") == 0){
		if(rx)
			nicDbgMesg|=NIC_PHY_RX_PKTDUMP;			
		else
			nicDbgMesg|=NIC_PHY_TX_PKTDUMP;
	}else if(strcmp(nextToken, "ps") == 0){
		if(rx)
			nicDbgMesg|=NIC_PS_RX_PKTDUMP;			
		else
			nicDbgMesg|=NIC_PS_TX_PKTDUMP;
	}else if(strcmp(nextToken, "ext") == 0){
		if(rx)
			nicDbgMesg|=NIC_EXT_RX_PKTDUMP;			
		else
			nicDbgMesg|=NIC_EXT_TX_PKTDUMP;
	}else{
		if(rx)
			nicDbgMesg&=~NIC_RX_PKTDUMP;
		else
			nicDbgMesg&=~NIC_TX_PKTDUMP;
	}
	return SUCCESS;
}
#endif


static int32	swNic_ringCmd(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;
	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp(nextToken, "mbufRxRing") == 0) {



		uint32  i, value, asicIdx = ((uint32 *)READ_MEM32(CPURMDCR))-RxMbufRing;
		rtlglue_printf("Rx mbuf ring starts at 0x%x\n",(uint32)RxMbufRing );
		for(i=0;i<totalRxMbuf;i++){
			value=RxMbufRing[i];
			rtlglue_printf("%3d. 0x%08x ",i, value&~0x3 );
			rtlglue_printf("%s ", ((value & 1)== 0)?"(CPU)":"(SWC)");
			if(asicIdx==i)
				rtlglue_printf("ASIC ");
			if(rxMbufIndex==i)
				rtlglue_printf("Rx ");
			if((value & 2)== 2){
				rtlglue_printf("WRAP!!\n");
				return SUCCESS;
			}else
				rtlglue_printf("\n");
		}
	
	}else if(strcmp(nextToken, "pkthdrRxRing") == 0) {
		swNic_dumpPkthdrDescRing();
		return SUCCESS;
	}else if(strcmp(nextToken, "txRing") == 0) {	
		swNic_dumpTxDescRing();
		return SUCCESS;
	}else{
		struct rtl_mBufStatus mbs;
		rtlglue_printf("Reset all NIC rings...\n");
		swNic_descRingCleanup();
		rtlglue_printf("  Init all NIC rings...\n");

		swNic_setup(totalRxPkthdr, totalRxMbuf, totalTxPkthdr);

		rtlglue_printf("  mbuf pool...\n");
		if(mBuf_getBufStat(&mbs)==SUCCESS){
			rtlglue_printf( "\tSizeof\tTotal\tFree\n");
			rtlglue_printf( "Cluster\t%d\t%d\t%d\n",mbs.m_mclbytes, mbs.m_totalclusters, mbs.m_freeclusters);
			rtlglue_printf( "Mbuf\t%d\t%d\t%d\n",mbs.m_msize, mbs.m_totalmbufs, mbs.m_freembufs);
			rtlglue_printf( "Pkthdr\t%d\t%d\t%d\n",mbs.m_pkthdrsize, mbs.m_totalpkthdrs, mbs.m_freepkthdrs);
		}
	
	}
	return SUCCESS;
}


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
		"loopback",		//cmdStr
		"Layer2 loopback.",	//cmdDesc
		" { enable %d'loopback port 1' %d'loopback port 2' | disable }",  //usage
		swNic_loopbackCmd,		//execution function
		CLE_USECISCOCMDPARSER,	
		0,
		NULL
	},	
#ifdef SWNIC_DEBUG	
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
		"hub",		//cmdStr
		"Hub mode",	//cmdDesc
		" { enable (1~5)%d'Hub member port' | disable }",  //usage
		swNic_hubCmd,		//execution function
		CLE_USECISCOCMDPARSER,	
		0,
		NULL
	},
	{	"ring",
		"NIC internal ring cmd",
		"{ txRing'dump Tx ring' | pkthdrRxRing'dump Rx ring' | mbufRxRing'dump Rx mbuf ring' | reset'Reset all rescriptor rings and NIC.' }",
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
#endif

