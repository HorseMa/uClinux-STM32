/****************************************************************************
*
*	Name:			cnxtEmac.c
*
*	Description:	Emac driver for CX821xx products
*
*	Copyright:		(c) 2001,2002 Conexant Systems Inc.
*					Personal Computing Division
*
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

*****************************************************************************
*  $Author: gerg $
*  $Revision: 1.1 $
*  $Modtime:   Aug 08 2003 08:13:00  $
****************************************************************************/

static char *version =
"EMAC Network Driver Cnxt 2003/07/16";

/* Always include 'config.h' first in case the user wants to turn on
   or override something. */
#define __KERNEL_SYSCALLS__

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/config.h>

#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/arch/bspcfg.h>
#include <asm/hardware.h>
#include "osutil.h"
#include <asm/arch/irq.h>
#include <asm/arch/sysmem.h>
#include "cnxtEmac.h"
#include <asm/delay.h>

#if HOMEPLUGPHY
#include "tesla.h"
#endif

#define SDRAM_TX_MEM			1
#define HW_RESET_EVENT			1
#define EMAC_TX_WD_TIMER		1
#define EMAC_MUTUALLY_EX_OPR	1
#define EMAC_CHK_HW_4_MUTEX_OPR	0
#define	TX_FULL_DUPLEX			1
#define RX_SKB_DEBUG			0
#define SETUP_FRAME_VERIFY		0
#define RX_TASKLET				1
#ifdef CONFIG_BD_TIBURON
#define TX_TASKLET				1
#define	TX_DMA_INT				1
#else
#define TX_TASKLET				0
#define	TX_DMA_INT				0
#endif
#define	RX_DMA_INT				1
#define USE_TU_INTERRUPT		1
#define USE_HASHTABLE			0
#define	EMAC_ISR_ERR_MSG_PRINT	1
#define EMAC_DISABLE_CHK		1
#define EMAC_TX_UDELAY			1

#define RX_COPY_MEMORY( pDest, pSrc, size ) memcpy( pDest, pSrc, size )
#define TX_COPY_MEMORY( pDest, pSrc, size ) memcpy( pDest, pSrc, size )

#if CONFIG_CPU_ARM940_D_CACHE_ON
#define EMAC_KMALLOC(s,flg)     emac_AllocNonCachedMem((s),(flg))
#define	EMAC_KFREE(p)			
#else
#define EMAC_KMALLOC(s,flg)     kmalloc((s),(flg))
#define	EMAC_KFREE(p)			kfree(p)
#endif

#define	DWORD_ALIGN_IP_HDR	2
#define TX_TIMEOUT_DELAY	15		// 150 ms delay
//#define TX_TIMEOUT_DELAY	10		// 100 ms delay
#define	TX_MSG_Q_THRESHOLD			4
#define	TX_MSG_Q_AWAKE_THRESHOLD	TX_MSG_Q_THRESHOLD + 0
//#define	TX_MSG_Q_THRESHOLD	TX_RING_SIZE
#define LOW_QWORD_MASK		0xFFFFFFF0

#define SLAVE_ADDRESS		0xA0	// bit 1 is page block number
#define CLOSED				FALSE
#define	OPEN				TRUE
#define	DRAM_8M_SIZE		0x800000
#define	ENABLED				1

#if CONFIG_NUM_ETHERNET_IS_1
	#define MAX_ETH_PORTS	1
#endif

#if CONFIG_NUM_ETHERNET_IS_2
	#define MAX_ETH_PORTS	2
#endif

#if CONFIG_NUM_ETHERNET_IS_3
	#define MAX_ETH_PORTS	3
#endif

#if CONFIG_NUM_ETHERNET_IS_4
	#define MAX_ETH_PORTS	4
#endif

// NOTE: EMAC_tx_pkt_q2 will be the common tx queue for
// mutually exclusive transmitter operation.
DATA_QUEUE_T	*EMAC_tx_pkt_q1;
DATA_QUEUE_T	*EMAC_tx_pkt_q2;	

DWORD			EMAC_num_in_operation = NONE;
DWORD			EMAC_hdw_cntr = 0;
DWORD			unhandled_emac_tx_error = 0;
DWORD			emac_rx_proc_no_skb;
struct net_device	*emac_dev[MAX_ETH_PORTS];
BOOLEAN			emac_mutually_ex_opr;

#define RX_LOOKBACK_THRESHOLD	20
WORD	rx_lookback_idx[RX_RING_SIZE];

#if SETUP_FRAME_VERIFY
BOOLEAN		sf_verify;
#endif

// Emac 1 registers
volatile unsigned long* DMAC_1_Ptr1 = (unsigned long*)0x00300000;
volatile unsigned long* DMAC_1_Ptr2 = (unsigned long*)0x00300030;
volatile unsigned long* DMAC_2_Ptr2 = (unsigned long*)0x00300034;
volatile unsigned long* DMAC_1_Cnt1 = (unsigned long*)0x00300060;
volatile unsigned long* DMAC_2_Cnt1 = (unsigned long*)0x00300064;
volatile unsigned long* DMAC_2_Cnt2 = (unsigned long*)0x00300094;

// Emac 2 registers
volatile unsigned long* DMAC_3_Ptr1 = (unsigned long*)0x00300008;
volatile unsigned long* DMAC_3_Ptr2 = (unsigned long*)0x00300038;
volatile unsigned long* DMAC_4_Ptr2 = (unsigned long*)0x0030003C;
volatile unsigned long* DMAC_3_Cnt1 = (unsigned long*)0x00300068;
volatile unsigned long* DMAC_4_Cnt1 = (unsigned long*)0x0030006C;
volatile unsigned long* DMAC_4_Cnt2 = (unsigned long*)0x0030009C;

volatile unsigned long* INT_Stat = (unsigned long*)0x00350044;
volatile unsigned long* INT_Msk = (unsigned long*)0x0035004c;
volatile unsigned long* INT_MStat = (unsigned long*)0x00350090;

volatile unsigned long* E_STAT_1_reg	= (volatile unsigned long*)0x00310008;
volatile unsigned long* E_NA_1_reg  	= (volatile unsigned long*)0x00310004;
volatile unsigned long* E_IE_1_reg	 	= (volatile unsigned long*)0x0031000C;
volatile unsigned long* E_LP_1_reg	 	= (volatile unsigned long*)0x00310010;

volatile unsigned long* E_STAT_2_reg	= (volatile unsigned long*)0x00320008;
volatile unsigned long* E_NA_2_reg  	= (volatile unsigned long*)0x00320004;
volatile unsigned long* E_IE_2_reg		= (volatile unsigned long*)0x0032000C;
volatile unsigned long* E_LP_2_reg		= (volatile unsigned long*)0x00320010;

volatile unsigned long* M2M_Cnt   = (volatile unsigned long* ) 0x00350004;
volatile unsigned long* DMA7_Ptr1 = (volatile unsigned long* ) 0x00300018;
volatile unsigned long* DMA8_Ptr1 = (volatile unsigned long* ) 0x0030001C;

#define NUM_EMACS		2
#define	MAC_ADDR_OFFSET_SIZE	1
#define ADDRESS_FILTER_SIZE   192

extern void CnxtBsp_Get_Mac_Address( int emac_num, char *mac_addr_array );
extern int cnxt_irq_mask_state_chk( unsigned int irq_num );
extern PHY_STATUS PhyConnectSet(PHY *phy, DWORD connect);
extern void TaskDelayMsec(UINT32 TimeDuration);
extern BOOLEAN CnxtBsp_Mac_MutEx_Chk( void );

static int EMAC_restart_hw( struct net_device *dev, BOOLEAN flush_txq, BOOLEAN flush_rxq );
static int EMAC_restart_rx( struct net_device *dev, BOOLEAN flush_rxq );

/* Index to functions, as function prototypes. */
int emac_probe(struct net_device *dev);
#if USE_HASHTABLE
	static void SetupHashTable(struct net_device *dev);
#endif
void ResetChip( EMAC_DRV_CTRL *thisadapter );

static int emac_open(struct net_device *dev);
static int emac_send_packet(struct sk_buff *skb, struct net_device *dev);
static int emac_close(struct net_device *dev);
static struct net_device_stats *emac_get_stats(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);

void EMAC_Linkcheck(EMAC_DRV_CTRL *thisadapter, BOOLEAN force_update );
BOOLEAN emac_SetupFrame ( struct net_device *dev );

#if RX_SKB_DEBUG
	static void skb_debug(const struct sk_buff *skb)
	{
	#define NUM2PRINT 80
		char buf[NUM2PRINT * 3 + 1];	/* 3 chars per byte */
		int i = 0;
		for (i = 0; i < skb->len && i < NUM2PRINT; i++) {
			sprintf(buf + i * 3, "%2.2x ", 0xff & skb->data[i]);
		}
		printk("skb->data: %s\n", buf);
	}
#endif

#if CONFIG_CPU_ARM940_D_CACHE_ON

/******************************************************************************
|
|  Function:    emacAllocNonCachedMem
|
|  Description: This function emulate the calloc () call to allocate a block
|               of memory space from the non-cached memory space.  When DATA
|               Cache is turned on, EMAC must use the non-cached memory space
|               for its Tx descriptors, Rx descriptors, and data buffers which
|               will be accessed by DMA.
|
|  Parameters:  size_t  n       number of memory elements
|               size_t  size    number of bytes for each memory elements
|
|  Returns:     void    *pMem   pointer to the start of the memory allocated
|               NULL            don't have enough room for the requested space
|
*******************************************************************************/

void *emac_AllocNonCachedMem ( size_t size, int flags )
{
	// Request non-cached SDRAM memory
	return SYSMEM_Allocate_BlockMemory( 0, (int) size, 16);
}
#endif

DWORD	emac1_tx_reset = 0;
DWORD	emac2_tx_reset = 0;
DWORD	emac1_rx_reset = 0;
DWORD	emac2_rx_reset = 0;

void EMAC_Disabled_Check( struct net_device *dev )
{
	EMAC_DRV_CTRL *thisadapter = ( EMAC_DRV_CTRL *)dev->priv; 

	if
	(		
		//	thisadapter->phyState.duplexMode == PHY_STAT_DUPLEX_HALF
		//&&	thisadapter->emac_state == OPEN
			thisadapter->emac_state == OPEN
		&&	thisadapter->hw_reset_required == TRUE
	)
	{
		thisadapter->hw_reset_required = FALSE;
		switch( thisadapter->emac_num )
		{
			// emac1
			case 1:
				emac1_tx_reset++;
				break;

			case 2:
				emac2_tx_reset++;
				break;
		}

		// Tell the OS we are closed
		netif_stop_queue(dev);

		// Restart the hardware to bring it into a known state
		EMAC_restart_hw( dev, TRUE, TRUE );
	}
}


/*
	 Parameters :
		BOOLEAN Port_Mii: 
			TRUE: configure MAC MII interface
			FALSE: configure MAC 7WS interface 
		BOOLEAN Speed100m: 
			TRUE: configure MAC device speed 100M
			FALSE: configure MAC device speed 1/10M
		BOOLEAN Full_Duplex:
			TRUE: configure MAC full duplex
			FALSE: configure MAC half duplex
*/
void EMACHW_Configure_Mac
(
	EMAC_DRV_CTRL	*thisadapter,
	int 			Port_Mii,
	int 			Speed100m,
	int 			Full_Duplex
)
{
	unsigned long E_NA_settings ;

	// don't care Port_Mii, CX82100 do not support 7WS mode
	E_NA_settings = *thisadapter->emac_na_reg;

	// Set default conditions
	E_NA_settings &= ~(E_NA_NS|E_NA_FD);

	if( !Speed100m)
	{ 
		E_NA_settings |= E_NA_NS;	// 10m
	}

	if( Full_Duplex )
	{
		E_NA_settings |= E_NA_FD;   // Full Duplex 
	}

	*thisadapter->emac_na_reg = E_NA_settings;
	thisadapter->E_NA_settings = E_NA_settings;

// related to ADI DSLAM/>6 Mbps/Win XP/FTP issue.  During FTP transfer, session dies but
// it will work if we force 10 HDX
	if ( thisadapter->phyState.linkStatus == PHY_STAT_LINK_UP )
	{
		if( !Speed100m && !Full_Duplex )
		{
			PhyConnectSet( thisadapter->pPhy, PHY_CONN_10BASE_T );
		}
	}
	else
	{
		PhyConnectSet( thisadapter->pPhy, PHY_CONN_AUTONEG_ALL_MEDIA );
	}

#if 0
	if( thisadapter->emac_num == 1)
	{
		printk("\neth0 hw update: ");
		if( !Speed100m)
		{ 
			printk(" 10M");
		}
		else
		{
			printk(" 100M");
		}
		if( Full_Duplex )
		{ 
			printk(" FULL\n");
		}
		else
		{
			printk(" HALF\n");
		}
		if( emac_mutually_ex_opr == FALSE )
		{
			printk("emac_mutually_ex_opr = FALSE\n");
		}
		else
		{
			printk("emac_mutually_ex_opr = TRUE\n");
		}
		if(thisadapter->phyState.linkStatus == PHY_STAT_LINK_UP)
		{
			printk("PHY_LINK_UP\n");
		}
		else
		{
			printk("PHY_LINK_DOWN\n");
		}
	}
#endif
}


BOOL EMAC_GetPortConfig( int dev_num,  EMAC_DRV_CTRL *pthisadapter )
{
    pthisadapter->MacFilteringMode = 2; 		// Not sure if we need this
    pthisadapter->phyState.autoNegotiate = 1;	// 0: disable,  1: enable
    pthisadapter->phyState.networkSpeed = 1; 	// 0: 10M,  1: 100M
    pthisadapter->phyState.duplexMode = 1;		// 0: HDX,  1: FDX

    return TRUE;
}


/******************************************************************************
|
|  Function:    EMAC_MapToPhyConn
|
|  Description: Convert PHY settings to PHY_CONN_XXXX as defined in phy.h
|               established link rate
|
|  Parameters:  *pPhySettings - pointer to PHY_SETTING structure containing
|                               speed/duplex configurations
|
|  Returns:     The PHY_CONN_XXXX corresponds to the specified PHY configurations
|
*******************************************************************************/

UINT32 EMAC_MapToPhyConn ( PHY_SETTING *pPhySettings )
{
    UINT32  conn;

    if ( pPhySettings->autoNegotiate == PHY_AUTONEGOTIATE )
	{
        conn = PHY_CONN_AUTONEG_ALL_MEDIA;
	}
    else
    {
        if ( pPhySettings->networkSpeed == PHY_STAT_SPEED_100 )
        {
            if ( pPhySettings->duplexMode == PHY_STAT_DUPLEX_FULL )
			{
                conn = PHY_CONN_100BASE_TXFD | PHY_CONN_AUTONEG;
			}
            else
			{
                conn = PHY_CONN_100BASE_TX;
			}
        }
        else
        {
            if ( pPhySettings->duplexMode == PHY_STAT_DUPLEX_FULL )
			{
                conn = PHY_CONN_10BASE_TFD | PHY_CONN_AUTONEG;
            }
            else
            {
                conn = PHY_CONN_10BASE_T;
			}
        }
    }
    return ( conn );
}

	
/*******************************************************************************

FUNCTION NAME:
		EMAC_Queue_Data
		
ABSTRACT:
		Places data on the queue of the data queue context
		Returns TRUE if successful.  FALSE if not.

  		This function is intended to be called from within an interrupt handler
		therefor it does not wrap the checking of the queue status with IRQ protection.
		
DETAILS:
		Returns TRUE if the data queueing operation was successful.

*******************************************************************************/

static BOOLEAN EMAC_Queue_Data
(
	DATA_QUEUE_T	*QueueCtx,	// Data queue context
	DWORD			Data1,		// Data1 value to be queued
	DWORD			Data2		// Data2 value to be queued
)
{

	DATA_REG_T *data_q_entry;
	
	// Is there room for the data on the queue?
	if ( QueueCtx->entry_cnt < SIZE_OF_DATA_QUEUES )
	{
		// Put the data on the queue context given to us
		data_q_entry = &QueueCtx->queue[ QueueCtx->put_idx++ ];
		data_q_entry->Data2 = Data2;
		data_q_entry->Data1 = Data1;

		// Process the put index to account for queue wrapping
		QueueCtx->put_idx %= SIZE_OF_DATA_QUEUES;

		// Indicate an entry was placed on the queue
		QueueCtx->entry_cnt++;

		// Response successfully put on queue
		return TRUE;

	}
	else
	{
		// Response was not put on queue because it was full.
		return FALSE;
	}
}


/*******************************************************************************

FUNCTION NAME:
		EMAC_Dequeue_Data
		
ABSTRACT:
		Removes data from the queue of the data queue context
		Returns TRUE if successful.  FALSE if not.

  		This function is intended to be called from within an interrupt handler
		therefor it does not wrap the checking of the queue status with IRQ protection.
		
DETAILS:
		Returns TRUE if the data dequeueing operation was successful.

*******************************************************************************/

static BOOLEAN EMAC_Dequeue_Data
(
	DATA_QUEUE_T	*QueueCtx,	// Data queue context
	DWORD           *Data1,		// Pointer to variable to be filled with dequeued Data1 value
	DWORD 			*Data2		// Pointer to variable to be filled with dequeued Data2 value
)
{
	DATA_REG_T *data_q_entry;
	
	// Is there data on the queue?
	if ( QueueCtx->entry_cnt > 0 )
	{
		// Get data entries from the queue context given to us
		data_q_entry = &QueueCtx->queue[ QueueCtx->get_idx++ ];
		*Data2 = data_q_entry->Data2;
		*Data1 = data_q_entry->Data1;

		// Process the get index to account for queue wrapping
		QueueCtx->get_idx %= SIZE_OF_DATA_QUEUES;

		// Indicate an entry was removed from the queue
		QueueCtx->entry_cnt--;

		// Response successfully dequeued
		return TRUE;

	}
	else
	{
		// No response to remove from the queue.
		return FALSE;
	}
}


/*******************************************************************************

FUNCTION NAME:
		EMAC_Queue_Count
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

static DWORD EMAC_Queue_Count( DATA_QUEUE_T *QueueCtx )
{
	return	QueueCtx->entry_cnt;	
}
	

/*******************************************************************************

FUNCTION NAME:
		EMAC_Queue_Tx_Pkt
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

static void EMAC_Queue_Tx_Pkt
(
	struct net_device	*dev,
	DWORD				tx_entry
)
{
	EMAC_DRV_CTRL	*thisadapter=(EMAC_DRV_CTRL*)dev->priv;

	// Always queue the ethernet tx packet to be sent
	if( EMAC_Queue_Data( thisadapter->emac_tx_q, (DWORD)dev, tx_entry ) == FALSE )
	{
		// Queuing of data failed!!
		printk( KERN_DEBUG "%s transmit queue full!\n", dev->name );
	}
}


/*******************************************************************************

FUNCTION NAME:
		EMAC_Tx_Pkt_Transmit
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

static void EMAC_Tx_Pkt_Transmit( struct net_device *pdevice )
{
	EMAC_DRV_CTRL	*pthisadapter;
	DWORD			tx_entry_num;
	DWORD			buf_ptr;
	DWORD			buf_len;
    ULONG			*pL;
    unsigned int	idx;
	unsigned long	flags;

	pthisadapter = ( EMAC_DRV_CTRL *)pdevice->priv;

#if EMAC_MUTUALLY_EX_OPR
	// Check for transmission of an ethernet packet already in progress.
	// The value contained in EMAC_num_in_operation represents the emac
	// device that currently is in operation in mutually exclusive operation.
	
	if
	(
			( emac_mutually_ex_opr == FALSE && pthisadapter->tx_pkt_in_progress == FALSE )
		||	( emac_mutually_ex_opr == TRUE && EMAC_num_in_operation == NONE )
	)
	{
		// Get the next tx ethernet packet to be sent
		if( EMAC_Dequeue_Data( pthisadapter->emac_tx_q, (DWORD *)&pdevice, &tx_entry_num ) == FALSE )
		{
			// No more ethernet tx data to send on either emac
			return;
		}
		
		pthisadapter = (EMAC_DRV_CTRL*)pdevice->priv;
		
		// Indicate which EMAC is currently in operation 
		//  AND
		// that it has a transmit packet in progress.
		EMAC_num_in_operation = pthisadapter->emac_num;
		pthisadapter->tx_pkt_in_progress = TRUE;
		
		buf_ptr = (DWORD)(pthisadapter->tx_msg_q[ tx_entry_num ]).Data;
		buf_len = (DWORD)(pthisadapter->tx_msg_q[ tx_entry_num ]).ulLength;
		
		// Program the DMA registers to transfer the ethernet packet into the TMAC.
		*pthisadapter->emac_tx_dma_ptr = (ULONG) buf_ptr;
		*pthisadapter->emac_tx_dma_cnt = (ULONG) buf_len;		// # of QWORDs to transfer

		if(	emac_mutually_ex_opr == TRUE )
		{
			*pthisadapter->emac_tx_dma_ptrx = (ULONG) buf_ptr;
		}

		// Compute dword (UINT32) offset to the end of the data in the txd buffer.
		// Since ulLength is in QWORDS, the number of DWORDS is 2 times ulLength.
		idx = (unsigned int)( buf_len << 1 );

		// Add the END TMAC TRANSMIT DESCRIPTOR pointer and qword count
		// to the end of the txd message buffer.

		// Get the txd buffer starting address.
		pL          = (ULONG*)buf_ptr;
		
		// Write the pointer to the END transmit frame structure.
	    pL[idx]		= (ULONG)(pthisadapter->TxD_end);

		// Initialize the QWORD count used by the DMA to xfer
		// the END transmit frame structure to the TMAC.
		pL[idx + 1]	= (ULONG) 3;

		// Indicate which tx_msg we are currently sending
		pthisadapter->tx_msg_q_current = tx_entry_num;
		
		// Indicate there is one less message in the tx queue
		if( pthisadapter->tx_msg_q_size > 0 )
		{
			pthisadapter->tx_msg_q_size--;
		}
		
		// Put a time stamp on the message
		pdevice->trans_start = jiffies;

		// Begin the DMA transfer of the START transmit descriptor into the TMAC.
		*pthisadapter->emac_na_reg = (pthisadapter->E_NA_settings | E_NA_STRT);
	#if EMAC_TX_WD_TIMER
		#if 1
		// Disable interrupts
		local_irq_save(flags);

		#endif
		// Start the timer that will check for missed emac tx dma interrupts
		mod_timer(&pthisadapter->wd_timer, jiffies + TX_TIMEOUT_DELAY );
		#if 1

		// Reenable interrupts
		local_irq_restore(flags);
		#endif
	#endif
	}
#else

	if( pthisadapter->tx_pkt_in_progress == FALSE )
	{
		// Get the next tx ethernet packet to be sent
		if( EMAC_Dequeue_Data( pthisadapter->emac_tx_q, (DWORD *)&pdevice, &tx_entry_num ) == FALSE )
		{
			// No more ethernet tx data to send on either emac
			return;
		}
		
		pthisadapter = (EMAC_DRV_CTRL*)pdevice->priv;
		
		// Indicate which EMAC is currently in operation 
		//  AND
		// that it has a transmit packet in progress.
		EMAC_num_in_operation = pthisadapter->emac_num;
		pthisadapter->tx_pkt_in_progress = TRUE;
		
		buf_ptr = (DWORD)(pthisadapter->tx_msg_q[ tx_entry_num ]).Data;
		buf_len = (DWORD)(pthisadapter->tx_msg_q[ tx_entry_num ]).ulLength;
		
		// Program the DMA registers to transfer the ethernet packet into the TMAC.
		*pthisadapter->emac_tx_dma_ptr = (ULONG) buf_ptr;
		*pthisadapter->emac_tx_dma_cnt = (ULONG) buf_len;		// # of QWORDs to transfer

		// Compute dword (UINT32) offset to the end of the data in the txd buffer.
		// Since ulLength is in QWORDS, the number of DWORDS is 2 times ulLength.
		idx = (unsigned int)( buf_len << 1 );

		// Add the END TMAC TRANSMIT DESCRIPTOR pointer and qword count
		// to the end of the txd message buffer.

		// Get the txd buffer starting address.
		pL          = (ULONG*)buf_ptr;
		
		// Write the pointer to the END transmit frame structure.
	    pL[idx]		= (ULONG)(pthisadapter->TxD_end);

		// Initialize the QWORD count used by the DMA to xfer
		// the END transmit frame structure to the TMAC.
		pL[idx + 1]	= (ULONG) 3;

		// Indicate which tx_msg we are currently sending
		pthisadapter->tx_msg_q_current = tx_entry_num;
		
		// Indicate there is one less message in the tx queue
		if( pthisadapter->tx_msg_q_size > 0 )
		{
			pthisadapter->tx_msg_q_size--;
		}

		// Begin the DMA transfer of the START transmit descriptor into the TMAC.
		*pthisadapter->emac_na_reg = (pthisadapter->E_NA_settings | E_NA_STRT);

		// Put a time stamp on the message
		pdevice->trans_start = jiffies;
	}
#endif
}	

void EMAC_Tx_Err_Cnt( EMAC_DRV_CTRL *thisadapter )
{
    register unsigned long 	estatus;
    	
	estatus = *thisadapter->emac_stat_reg;
	*thisadapter->emac_stat_reg = estatus;

    if ( estatus & E_S_TOF )	
    {
		#if EMAC_ISR_ERR_MSG_PRINT
		printk( KERN_DEBUG "ETH%d: TX FIFO overflow\n", thisadapter->unit);  
		#endif
		thisadapter->stats.tx_fifo_errors++;
		thisadapter->stats.tx_errors++;
    }
    if ( estatus & E_S_TUF )	
    {
		#if EMAC_ISR_ERR_MSG_PRINT
		printk( KERN_DEBUG "ETH%d: TX FIFO underflow\n", thisadapter->unit);  
		#endif
		thisadapter->stats.tx_fifo_errors++;
		thisadapter->stats.tx_errors++;
    }

	if ( estatus & E_S_RLD )
	{
		thisadapter->stats.collisions += (estatus & TSTAT_CC);	
	}
    if ( estatus & E_S_ED )	
    {
    }
    if ( estatus & E_S_DF )	
    {
    }
    if ( estatus & E_S_TF )	
    {
    }
    if ( estatus & E_S_TJT )	
    {
    }
}

/******************************************************************************
|
|  Function:  EMAC_Tx_Pkt_Done
|
|  Description:  

|  Parameters:  
|
|  Returns:
*******************************************************************************/

static BOOLEAN	EMAC_Tx_Pkt_Done(struct net_device *dev )
{
    TX_MSG_QUEUE	tx_msg;
	unsigned long	flags;
	BOOLEAN			status = FALSE;

	EMAC_DRV_CTRL 	*thisadapter=( EMAC_DRV_CTRL *)dev->priv; 

#if EMAC_MUTUALLY_EX_OPR
	// Disable interrupts
	local_irq_save(flags);

	// Did this EMAC really have a tx packet transmission in progress?
	if( emac_mutually_ex_opr == FALSE || EMAC_num_in_operation == thisadapter->emac_num )
	{
		EMAC_num_in_operation = NONE;
		thisadapter->tx_pkt_in_progress = FALSE;
				
		if( netif_queue_stopped(dev))
		{
			if ( (TX_RING_SIZE - thisadapter->tx_msg_q_size) > TX_MSG_Q_AWAKE_THRESHOLD )
			{
				netif_wake_queue( dev );
			}
		}

		tx_msg = thisadapter->tx_msg_q[ thisadapter->tx_msg_q_current ];

		// Reenable interrupts
		local_irq_restore(flags);

		if( ((EMAC_TXD *)tx_msg.Data)->status & (TSTAT_TDN | TSTAT_CD) )
		{
			status = TRUE;
			thisadapter->stats.tx_packets++;
			thisadapter->stats.tx_bytes += (DWORD)tx_msg.ulLength;
		#if 1
		// DOES THIS NEED TO BE MOVED OUTSIDE OF THE IF-ELSE STATEMENT
		// TO THE END OF THE FUNCTION ????
			// Disable interrupts
			local_irq_save(flags);

			EMAC_Tx_Pkt_Transmit( dev );

			// Reenable interrupts
			local_irq_restore(flags);
		#endif
		}
		else
		{
			EMAC_Tx_Err_Cnt( thisadapter );
		}
	}
	else
	{
		// Reenable interrupts
		local_irq_restore(flags);
	}
#else
	thisadapter->tx_pkt_in_progress = FALSE;
			
	tx_msg = thisadapter->tx_msg_q[ thisadapter->tx_msg_q_current ];

	if( netif_queue_stopped(dev))
	{
		netif_wake_queue( dev );
	}

	if( ((EMAC_TXD *)tx_msg.Data)->status & (TSTAT_TDN | TSTAT_CD) )
	{
		status = TRUE;
		thisadapter->stats.tx_packets++;
		thisadapter->stats.tx_bytes += (DWORD)tx_msg.ulLength;
		// Disable interrupts
		local_irq_save(flags);

		EMAC_Tx_Pkt_Transmit( dev );

		// Reenable interrupts
		local_irq_restore(flags);
	}
	else
	{
		EMAC_Tx_Err_Cnt( thisadapter );
	}

#endif
	return status;
}


#if (TX_FULL_DUPLEX && EMAC_MUTUALLY_EX_OPR)

/******************************************************************************
|
|  Function:  EMAC_Mutually_Ex_Op_Chk
|
|  Description:
|			   
|  Parameters:  
|
|  Returns:
*******************************************************************************/

static void EMAC_Mutually_Ex_Op_Chk( struct net_device *dev )
{
	unsigned int		idx;
	DWORD				tx_entry_num;
	struct net_device	*pdevice;	
	unsigned long		flags;
	

	EMAC_DRV_CTRL	*thisadapter= (EMAC_DRV_CTRL *)dev->priv;

	// Check for emac mutually exclusive transmitter operation
	if(	thisadapter->emac_num == 1 )
	{
		// Disable interrupts
		local_irq_save(flags);

		// Half Duplex
		if(		(thisadapter->phyState.duplexMode == PHY_STAT_DUPLEX_HALF)
#if EMAC_CHK_HW_4_MUTEX_OPR
			&&	(thisadapter->tx_mutex_part)
#endif
			&&	(emac_mutually_ex_opr == FALSE) )
		{
			printk( KERN_DEBUG "eth0 changing to half duplex mutually exclusive operation.\n");
			emac_mutually_ex_opr = TRUE;
			thisadapter->emac_tx_q = EMAC_tx_pkt_q2;
			
			// Clear out the emac 1 transmit packet queue of 
			// any queued transmit packets and put them	on
			// the common emac transmit queue.
			idx = EMAC_Queue_Count( EMAC_tx_pkt_q1 );
			if( idx != 0 )
			{
				do
				{
					// Get an entry from the emac 1 tx pkt q
					EMAC_Dequeue_Data( EMAC_tx_pkt_q1, (DWORD *)&pdevice, &tx_entry_num );

					// Now put it on the common transmit queue
					if( EMAC_Queue_Data( EMAC_tx_pkt_q2, (DWORD)pdevice, tx_entry_num ) == FALSE )
					{
						// Queuing of data failed!!
						printk("EMAC2 transmit queue full!\n");
						break;
					}
					idx--;		
				} while( idx != 0 );	
			}
		}
		
		// Full Duplex
		if( (thisadapter->phyState.duplexMode == PHY_STAT_DUPLEX_FULL) && emac_mutually_ex_opr == TRUE )
		{
			printk( KERN_DEBUG "eth0 changing to full duplex simultaneous operation.\n");
			// When leaving mutually exclusive transmitter operation
			// flush the common emac transmitter queue of any
			// tx packets for emac 1 and put them on the emac 1 queue.
			emac_mutually_ex_opr = FALSE;
			thisadapter->emac_tx_q = EMAC_tx_pkt_q1;
			
			idx = EMAC_Queue_Count( EMAC_tx_pkt_q2 );
			if( idx != 0 )
			{
				do
				{
					// Get an entry from the common tx pkt q
					EMAC_Dequeue_Data( EMAC_tx_pkt_q2, (DWORD *)&pdevice, &tx_entry_num );

					// If it is not emac 1 then return it to the common tx pkt q 
					if( pdevice != dev )
					{
						if( EMAC_Queue_Data( EMAC_tx_pkt_q2, (DWORD)pdevice, tx_entry_num ) == FALSE )
						{
							// Queuing of data failed!!
							printk("EMAC2 transmit queue full!\n");
							break;
						}
					}
					else
					{
						if( EMAC_Queue_Data( EMAC_tx_pkt_q1, (DWORD)pdevice, tx_entry_num ) == FALSE )
						{
							// Queuing of data failed!!
							printk("EMAC1 transmit queue full!\n");
							break;
						}
					}
					idx--;		
				} while( idx != 0 );	
			}
		}
		
		// Reenable interrupts
		local_irq_restore(flags);
	}
}	
#endif


/******************************************************************************
|
|  Function:  EMAC_stop_hw
|
|  Description:  restarts the hardware and attempts to retransmit the
|				last ethernet packet
|  Parameters:  
|
|  Returns:    OK on success, ERROR otherwise.
*******************************************************************************/

void EMAC_stop_hw( struct net_device *dev)
{

 	EMAC_DRV_CTRL	*thisadapter= (EMAC_DRV_CTRL *)dev->priv;

    cnxt_mask_irq( dev->irq );

#if TX_DMA_INT
    cnxt_mask_irq( thisadapter->tx_dma_int );	
#endif
#if RX_DMA_INT
    cnxt_mask_irq( thisadapter->rx_dma_int );	
#endif
	
	netif_stop_queue(dev);

	/* Clear the status registers  */
	*INT_Stat = thisadapter->int_stat_clr_dev_irq;

#if TX_DMA_INT
	/* Clear the INT1_Stat register of pending TX interrupts */
	*INT_Stat = thisadapter->int_stat_clr_tx;
#endif
#if RX_DMA_INT
	/* Clear the INT1_Stat register of pending RX interrupts */
	*INT_Stat = thisadapter->int_stat_clr_rx;
#endif

	// Stop the transmitter and receiver

	// NOTE: The transmitter does not stop immediately when the stop
	// bit is set.  Depending upon where the dma engine may be in
	// the tranferring of a TX frame to the emac, it may take up to a maximum
	// time interval that is equal to the value of a 1514 MTU in bits times the
	// period of the networkspeed clock period.  Since there is no hardware
	// indication of when the dma has stopped, all you can do is delay
	// for this maximum time interval and hope for the best. 

    thisadapter->E_NA_settings = 0;
	*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

}


/******************************************************************************
|
|  Function:  EMAC_restart_hw
|
|  Description:  restarts the hardware and attempts to retransmit the
|				last ethernet packet
|  Parameters:  
|
|  Returns:    OK on success, ERROR otherwise.
*******************************************************************************/

static int EMAC_restart_hw( struct net_device *dev, BOOLEAN flush_txq, BOOLEAN flush_rxq )
{
	EMAC_RXD_HW_QTYPE	*rxd_hw;
	BYTE				*rxd_buf;
	unsigned int		idx;
    unsigned long 		estatus;	/* contains the status of the ethernet hardware */
    unsigned long 		lstatus;	/* contains the status of the last packet register */
	DWORD				tx_entry_num;
	struct net_device	*pdevice;	
	
	EMAC_DRV_CTRL	*thisadapter= (EMAC_DRV_CTRL *)dev->priv;

    cnxt_mask_irq( dev->irq );

#if TX_DMA_INT
    cnxt_mask_irq( thisadapter->tx_dma_int );	
#endif
#if RX_DMA_INT
    cnxt_mask_irq( thisadapter->rx_dma_int );	
#endif
	
	netif_stop_queue(dev);

	/* Clear the status registers  */
	*INT_Stat = thisadapter->int_stat_clr_dev_irq;

#if TX_DMA_INT
	/* Clear the INT1_Stat register of pending TX interrupts */
	*INT_Stat = thisadapter->int_stat_clr_tx;
#endif
#if RX_DMA_INT
	/* Clear the INT1_Stat register of pending RX interrupts */
	*INT_Stat = thisadapter->int_stat_clr_rx;
#endif

	// Clear the EMAC exception condition registers
	estatus = *thisadapter->emac_stat_reg;
	*thisadapter->emac_stat_reg = estatus;

    lstatus = *thisadapter->emac_lp_reg;
    *thisadapter->emac_lp_reg = lstatus;

	// Stop the transmitter and receiver

	// NOTE: The transmitter does not stop immediately when the stop
	// bit is set.  Depending upon where the dma engine may be in
	// the tranferring of a TX frame to the emac, it may take up to a maximum
	// time interval that is equal to the value of a 1514 MTU in bits times the
	// period of the networkspeed clock period.  Since there is no hardware
	// indication of when the dma has stopped, all you can do is delay
	// for this maximum time interval and hope for the best. 

    thisadapter->E_NA_settings = 0;
//	*thisadapter->emac_na_reg = (thisadapter->E_NA_settings | E_NA_STOP);
	*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

	if( thisadapter->phyState.networkSpeed == PHY_STAT_SPEED_100 )
	{
		TaskDelayMsec(10);
	}
	else
	{
		TaskDelayMsec(10);
	}
	*thisadapter->emac_na_reg = thisadapter->E_NA_settings;
    
    ResetChip( thisadapter ); 
    
	rxd_hw = thisadapter->rxd_hw;

	if( flush_rxq == TRUE )
	{
		// Reset the entire receive packet mess
		rxd_buf = thisadapter->rxd_buf;
	    QWORD_ALIGN_PTR(rxd_buf);

		for( idx = 0; idx < RXD_NUM; idx++ ) 
		{
			rxd_hw->q[idx].mData = rxd_buf + (ETH_PKT_BUF_SZ * idx);
			rxd_hw->q[idx].reserved = 0;
			rxd_hw->q[idx].status = 0;
			rxd_hw->q[idx].mib = 0;
		}
		rxd_hw->qndx = 0;	

	}

	*thisadapter->emac_rx_dma_ptr2 = (unsigned long)(&rxd_hw->q[0]); /* address of descriptor list */
	*thisadapter->emac_rx_dma_cnt1 = (unsigned long)((CL_SIZ/8)-4);/* size of cluster in qwords */
	*thisadapter->emac_rx_dma_cnt2 = (unsigned long)(2 * RXD_NUM);   /* number of descriptors * 2 */
  	
	if( flush_txq == TRUE )
	{
		// Clear out the emac transmit packet queue of 
		// any queued transmit packets for this emac.
		idx = EMAC_Queue_Count( thisadapter->emac_tx_q );
		if( idx != 0 )
		{
			do
			{
				// Get an entry from the tx pkt q
				EMAC_Dequeue_Data( thisadapter->emac_tx_q, (DWORD *)&pdevice, &tx_entry_num );

				// If it is not this device then return it to the tx pkt q 
				if( pdevice != dev )
				{
					if( EMAC_Queue_Data( thisadapter->emac_tx_q, (DWORD)pdevice, tx_entry_num ) == FALSE )
					{
						// Queuing of data failed!!
						printk("EMAC transmit queue full!\n");
						break;
					}
				}
				idx--;		
			} while( idx != 0 );	
		}
		
	    //	Initialize the tx msg queue.
	    thisadapter->tx_msg_q_next = 0;
	    thisadapter->tx_msg_q_current = 0;
	    thisadapter->tx_msg_q_size = 0;
	}

	*thisadapter->emac_tx_dma_ptr = 0;
	*thisadapter->emac_tx_dma_cnt = 0;
	thisadapter->tx_pkt_in_progress = FALSE;

#if EMAC_MUTUALLY_EX_OPR
	// Release the EMAC operation lock if we had it
	// and allow the other EMAC to run if it needs to.
	if(	emac_mutually_ex_opr == FALSE || EMAC_num_in_operation == thisadapter->emac_num  )
	{
		EMAC_num_in_operation = NONE;		
	}		
#endif
	if( thisadapter->emac_state == OPEN )
	{
		// Get the current emac line state
		thisadapter->E_NA_settings = 0;
		EMAC_Linkcheck( thisadapter, TRUE );

		// Set up the emac hardware filtering for perfect address filtering 
		thisadapter->E_NA_settings |= E_NA_FILTER_MC1PERFECT;	
		*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

		// Configure the emac hardware filtering 
		emac_SetupFrame ( dev );

#if 0
	#if HOMEPLUGPHY
	    if( PhyActiveHOMEPLUG( thisadapter->pPhy) )
	    {
		    TeslaControl_Str TeslaControl ;

	        Emac_TeslaConfigRead( &TeslaControl ) ;
	        Emac_TeslaSetup( (PVOID)thisadapter, &TeslaControl) ;
	    }
	#endif
#endif	
		*thisadapter->emac_ie_reg = thisadapter->E_IE_settings;
	    
		// Start the receiver operating now
	    thisadapter->E_NA_settings |= (E_NA_SR|E_NA_SB);
		*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

	    cnxt_unmask_irq( dev->irq );
	#if TX_DMA_INT
	    cnxt_unmask_irq( thisadapter->tx_dma_int );	
	#endif
	#if RX_DMA_INT
	    cnxt_unmask_irq( thisadapter->rx_dma_int );	
	#endif

	}

	if( netif_queue_stopped(dev))
	{
		netif_wake_queue( dev );
	}

    return 0;
}


static int EMAC_restart_rx( struct net_device *dev, BOOLEAN flush_rxq )
{
	EMAC_RXD_HW_QTYPE	*rxd_hw;
	BYTE				*rxd_buf;
	unsigned int		idx;
	
	EMAC_DRV_CTRL	*thisadapter= (EMAC_DRV_CTRL *)dev->priv;

    cnxt_mask_irq( dev->irq );

#if RX_DMA_INT
    cnxt_mask_irq( thisadapter->rx_dma_int );	
#endif
	
	netif_stop_queue(dev);

	/* Clear the status registers  */
	*INT_Stat = thisadapter->int_stat_clr_dev_irq;

#if RX_DMA_INT
	/* Clear the INT1_Stat register of pending RX interrupts */
	*INT_Stat = thisadapter->int_stat_clr_rx;
#endif

	// Stop the receiver

    thisadapter->E_NA_settings &= ~E_NA_SR;
	*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

	if( thisadapter->phyState.networkSpeed == PHY_STAT_SPEED_100 )
	{
		TaskDelayMsec(10);
	}
	else
	{
		TaskDelayMsec(10);
	}
    
    EMAC_REG_WRITE( E_NA, E_NA_RRX, thisadapter->emac_hdw_num );
	udelay( 100 );
	udelay( 100 );
	udelay( 50 );
    EMAC_REG_WRITE( E_NA, 0, thisadapter->emac_hdw_num );
	udelay( 100 );
	udelay( 100 );
	udelay( 50 );
    EMAC_REG_WRITE( E_NA, 0, thisadapter->emac_hdw_num );
	udelay( 100 );
	udelay( 100 );
	udelay( 50 );
    
	rxd_hw = thisadapter->rxd_hw;

	if( flush_rxq == TRUE )
	{
		// Reset the entire receive packet mess
		rxd_buf = thisadapter->rxd_buf;
	    QWORD_ALIGN_PTR(rxd_buf);

		for( idx = 0; idx < RXD_NUM; idx++ ) 
		{
			rxd_hw->q[idx].mData = rxd_buf + (ETH_PKT_BUF_SZ * idx);
			rxd_hw->q[idx].reserved = 0;
			rxd_hw->q[idx].status = 0;
			rxd_hw->q[idx].mib = 0;
		}
		rxd_hw->qndx = 0;	
	}

	*thisadapter->emac_rx_dma_ptr2 = (unsigned long)(&rxd_hw->q[0]); /* address of descriptor list */
	*thisadapter->emac_rx_dma_cnt1 = (unsigned long)((CL_SIZ/8)-4);/* size of cluster in qwords */
	*thisadapter->emac_rx_dma_cnt2 = (unsigned long)(2 * RXD_NUM);   /* number of descriptors * 2 */
  	
	// Start the receiver operating now
    thisadapter->E_NA_settings |= E_NA_SR;
	*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

    cnxt_unmask_irq( dev->irq );
#if RX_DMA_INT
    cnxt_unmask_irq( thisadapter->rx_dma_int );	
#endif

	if( netif_queue_stopped(dev))
	{
		netif_wake_queue( dev );
	}

    return 0;
}


/******************************************************************************
|
|  Function:  EMAC_Linux_tx_timeout
|
|  Description:  

|  Parameters:  
|
|  Returns:
*******************************************************************************/

static void EMAC_Linux_tx_timeout(struct net_device *dev)
{

	EMAC_DRV_CTRL *thisadapter = ( EMAC_DRV_CTRL *)dev->priv; 

	thisadapter->stats.tx_errors++;
	thisadapter->stats.tx_aborted_errors++;
	if( netif_queue_stopped(dev))
	{
		netif_wake_queue( dev );
	}
	printk( KERN_DEBUG "%s tx timeout\n", dev->name);

	thisadapter->hw_reset_required = TRUE;
}

/*******************************************************************************

FUNCTION NAME:
		EMAC_tx_timeout
		
ABSTRACT:
		
DETAILS:

RETURNS:
		
NOTES:
*******************************************************************************/

static void EMAC_tx_timeout(struct net_device *dev)
{
	unsigned long		flags;
	EMAC_DRV_CTRL *thisadapter = ( EMAC_DRV_CTRL *)dev->priv; 

	// Check to see if this was a case of us failing to detect
	// the completion of a valid transmit frame or that there
	// was an emac lockup or something else.
#if 0
	if( EMAC_Tx_Pkt_Done( dev ) == FALSE )
#endif
	{
		printk( KERN_DEBUG "%s tx_timeout\n", dev->name );		
#if 1
		// Disable interrupts
		local_irq_save(flags);

		netif_stop_queue( dev );
#endif
		EMAC_Tx_Err_Cnt( thisadapter );
		thisadapter->stats.tx_errors++;
		thisadapter->stats.tx_aborted_errors++;
		thisadapter->hw_reset_required = TRUE;
#if 1

		// Reenable interrupts
		local_irq_restore(flags);
#endif
	#if HW_RESET_EVENT
		SET_EVENT( thisadapter->hw_reset_event );
	#endif
	}
}

	 
/*******************************************************************************

FUNCTION NAME:
		EMAC_Tx_Pkt_Process
		
ABSTRACT:
		
DETAILS:

RETURNS:
		
NOTES:
To transmit an ethernet packet, three items are necessary.  Two TMAC
TRANSMIT DESCRIPTORS and a transmit message buffer.   In this particular
implementation the START transmit descriptor is not prepended to the
buffer to be transmitted so the first DWORD of the buffer must be copied
into this transmit descriptor. 

See section 7.7 of the CX82100 HNP data sheet for more details on how the 
ethernet TMAC function operates.	In particular see figures 7.5 EMAC Transmit
Frame Structure and 7.6 TMAC DMA Operation for Channel {x} = 1 or 3.

Also see section 4.6.3 Linked List Mode for an explanation of the Embedded Tail
Linked List mode of DMA operation.

The START ethernet TMAC TRANSMIT DESCRIPTOR consists of the following
DWORD (32 bit) sized variables:

	Relative 		Variable
	Byte Address	Name			Notes
	--------------	------------	----------------------------------------------
	0x00			STATUS			Status of transmitted message. Updated by TMAC
	0x04			STATUS_ZERO		Always all zeros
	0x08			DESCRIPTOR		Contains bit fields that indicate readiness of
									the frame, frame size and other items.  Set by
									ARM CPU.
	0x0C			DATA			First DWORD of data from the transmit message buffer.
	 	.
		.
		.
	0x0C + n		DATA			Last DWORD of data in transmit message buffer.
	0x0C + n + 4	PAD				Optional padding to QWORD align POINTER and COUNT
	0x0C + n + 4(8)	POINTER			Pointer to the END transmit descriptor structure.
									The END transmit descriptor is used to terminate
									the transmission of this transmit message buffer.
	0x0C + n + 8(12)	COUNT		Size in QWORDS of the END transmit descriptor structure.

The END ethernet TMAC TRANSMIT DESCRIPTOR consists of the following
DWORD (32 bit) sized variables:

	Relative 		Variable
	Byte Address	Name			Notes
	--------------	------------	----------------------------------------------
	0x00			STATUS			Set to zero.
	0x04			STATUS_ZERO		Set to zero.
	0x08			DESCRIPTOR		Set to zero.
	0x0C			DATA			Set to zero.
	0x10			POINTER			Set to zero.
	0x14			COUNT			Set to zero.

*******************************************************************************/

static int	EMAC_Tx_Pkt_Process
(
	EMAC_DRV_CTRL	*thisadapter,
	TX_PKT_INFO		*tx_pkt_info,
	DWORD			*entry
)
{
    TX_MSG_QUEUE	tx_msg;
	BYTE            *txd_buf;

	// Determine if the transmitter has any buffers left
	if( thisadapter->tx_msg_q_size < TX_RING_SIZE )
	{
		// Get a buffer from the transmitter message queue to copy the data into.
		*entry = thisadapter->tx_msg_q_next++ % TX_RING_SIZE;
		thisadapter->tx_msg_q_next %= TX_RING_SIZE;
		txd_buf = thisadapter->txd_buf[*entry];
			
		// Copy the transmit data into our own buffer so that we can
		// prepend the START transmit descriptor to the data.
		TX_COPY_MEMORY
		(
		 	txd_buf + EMAC_TXD_SIZE,
	        tx_pkt_info->tx_pkt_data,
	        tx_pkt_info->tx_pkt_len
		);

		tx_msg.Data = txd_buf;

		tx_msg.ulControl = TX_FRM_LEN_SET(tx_pkt_info->tx_pkt_len) | (TCTRL_RDY);

		// Determine the length of the tx message in QWORDS
		// NOTE: This length must also include the START TMAC
		// TRANSMIT DESCRIPTOR fields.
		tx_msg.ulLength =  ((tx_pkt_info->tx_pkt_len + EMAC_TXD_SIZE + QWORD_ROUND_UP_FACTOR ) / SIZEOF_QWORD );

		// Initialize START TMAC TRANSMIT DESCRIPTOR
		((EMAC_TXD *)tx_msg.Data)->status = 0;
		((EMAC_TXD *)tx_msg.Data)->status_zero = 0;
		((EMAC_TXD *)tx_msg.Data)->control = tx_msg.ulControl;

		/* insert tx_msg into queue  */
		thisadapter->tx_msg_q[ *entry ] = tx_msg; /* this is a copy*/
		thisadapter->tx_msg_q_size++;
	}

	// Return number of empty buffers left in our ring
	return (TX_RING_SIZE - thisadapter->tx_msg_q_size);
}	


/******************************************************************************
|
|  Function:    emac_SetupFrame
|
|  Description: This routine sets up the EMAC Setup Frame based on the 
|               filtering mode configuration.  This is to configure the 
|               hardware assisted address filtering.  
|
|               Currently it only supports All Multicast plus One Perfect mode.
|               No setting is needed for promiscuous mode.
|
|  Returns:     OK/ERROR
*******************************************************************************/

BOOLEAN emac_SetupFrame ( struct net_device *dev )
{
    UINT8       		index;
    char        		*macaddr;
    EMAC_SETUP_FRAME  	*cam;
    ULONG				*pL;
    unsigned int		idx;
    TX_MSG_QUEUE		tx_msg;

	EMAC_DRV_CTRL	*thisadapter = (EMAC_DRV_CTRL*)dev->priv;
	
#if SETUP_FRAME_VERIFY
	if(	sf_verify == FALSE )
	{
		return FALSE;
	}
#endif

	// Check to see if we are configured for something 
	// other than perfect MAC address filtering.
    if 
	(
			( thisadapter->E_NA_settings & E_NA_FILTER_MODE ) != E_NA_FILTER_MC1PERFECT
		||	( dev->flags & IFF_PROMISC )
	)
    {
        return FALSE;
    }
        
    /*
		Prepare the setup frame with the unicast addresses.  CX821xx specifies that each entry 
		occupies 3 double words (12 bytes) and the address data go into the low word of each 
		double words.  CX821xx also requires the empty entries in the setup frame be filled with 
		the last valid entry.
    */
		
	memset( &thisadapter->setup_frame_buf[0], 0, sizeof ( thisadapter->setup_frame_buf ));

    cam = ( EMAC_SETUP_FRAME * )((BYTE *) &thisadapter->setup_frame_buf[0] + EMAC_TXD_SIZE);
    
    macaddr = ( char * ) &thisadapter->PortNetAddress[0];

    for (index = 0; index < EMAC_SETUP_PERFECT_ENTRIES; index++)
    {
        *(UINT16 *) & cam->Perfect.PhysicalAddress[index][0] = (UINT16)((macaddr[0] & 0xff) + ((macaddr[1] & 0xff) <<8));
        *(UINT16 *) & cam->Perfect.PhysicalAddress[index][1] = (UINT16)((macaddr[2] & 0xff) + ((macaddr[3] & 0xff) <<8));
        *(UINT16 *) & cam->Perfect.PhysicalAddress[index][2] = (UINT16)((macaddr[4] & 0xff) + ((macaddr[5] & 0xff) <<8));
    }
    
	tx_msg.Data = &thisadapter->setup_frame_buf[0];
	tx_msg.ulControl = TX_FRM_LEN_SET(ADDRESS_FILTER_SIZE) | (TCTRL_RDY | TCTRL_SET );

	// Determine the length of the tx message in QWORDS
	// NOTE: This length must also include the START TMAC
	// TRANSMIT DESCRIPTOR fields.
	tx_msg.ulLength =  ((ADDRESS_FILTER_SIZE + EMAC_TXD_SIZE + QWORD_ROUND_UP_FACTOR ) / SIZEOF_QWORD );

	// Initialize START TMAC TRANSMIT DESCRIPTOR
	((EMAC_TXD *)tx_msg.Data)->status = 0;
	((EMAC_TXD *)tx_msg.Data)->status_zero = 0;
	((EMAC_TXD *)tx_msg.Data)->control = tx_msg.ulControl;

	// Compute dword (UINT32) offset to the end of the data in the txd buffer.
	// Since ulLength is in QWORDS, the number of DWORDS is 2 times ulLength.
	idx = (unsigned int)(tx_msg.ulLength << 1 );

	// Add the END TMAC TRANSMIT DESCRIPTOR pointer and qword count
	// to the end of the setup frame buffer.

	// Get the setup frame buffer starting address.
	pL          = (ULONG*)tx_msg.Data;
	
	// Write the pointer to the END transmit frame structure.
    pL[idx]		= (ULONG)(thisadapter->TxD_end);

	// Initialize the QWORD count used by the DMA to xfer
	// the END transmit frame structure to the TMAC.
	pL[idx + 1]	= (ULONG)TD_DMA_SIZ;

	// Initialize END TMAC TRANSMIT DESCRIPTOR
	thisadapter->TxD_end->status		= 0;
	thisadapter->TxD_end->status_zero	= 0;
	thisadapter->TxD_end->control		= 0;
	thisadapter->TxD_end->data			= 0;             
	thisadapter->TxD_end->joint.pNext	= (BYTE *)&thisadapter->TxD_end;
	thisadapter->TxD_end->joint.cNext	= 0;          

	// Program the DMA registers to transfer the ethernet packet into the TMAC.
	*thisadapter->emac_tx_dma_ptr = (ULONG) tx_msg.Data;
	*thisadapter->emac_tx_dma_cnt = (ULONG) tx_msg.ulLength;

	// Begin the DMA transfer of the START transmit descriptor into the TMAC.
	*thisadapter->emac_na_reg |= E_NA_STRT;

	return TRUE;
}


/*******************************************************************************

FUNCTION NAME:
		emac_send_packet
		
ABSTRACT:
		
DETAILS:

RETURNS:

*******************************************************************************/

static int	emac_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	TX_PKT_INFO		tx_pkt_info;
	unsigned long	flags;
	DWORD			entry;
	int				num_buf;

	EMAC_DRV_CTRL	*thisadapter=(EMAC_DRV_CTRL*)dev->priv;

#if 0
	if(	thisadapter->phyState.linkStatus != PHY_STAT_LINK_UP )
	{
		dev_kfree_skb_any (skb);
		return 0;
	}
#endif
		
	// Generalize the data about the transmit packet
	tx_pkt_info.tx_pkt_data = skb->data;
	tx_pkt_info.tx_pkt_len = skb->len;

	// Disable interrupts
	spin_lock_irqsave(&thisadapter->lock, flags);
	 
	// Preprocess the transmit packet data for the emac driver
	num_buf = EMAC_Tx_Pkt_Process( thisadapter, &tx_pkt_info, &entry );

	// If the number of remaining tx buffers is less than
	// some amount then tell the OS to stop giving us more.
	if( num_buf < TX_MSG_Q_THRESHOLD )
	{
		netif_stop_queue( dev );
	}

	EMAC_Queue_Tx_Pkt( dev, entry );
	
	// Transmit a packet now
	EMAC_Tx_Pkt_Transmit( dev );

	// Reenable interrupts
	spin_unlock_irqrestore(&thisadapter->lock, flags);

	// Return the skb back to the pool now that
	// we have copied the data from it.
	dev_kfree_skb_any (skb);
	return 0;
}


/*******************************************************************************

FUNCTION NAME:
		EMAC_task
		
ABSTRACT:
		
DETAILS:

RETURNS:
		
NOTES:
*******************************************************************************/

BOOLEAN print_debug_msg = FALSE;
DWORD	disabled_irq_cnt = 0;
DWORD	enabled_irq_cnt = 0;

static void EMAC_task(struct net_device *dev)
{
	EMAC_DRV_CTRL *thisadapter = ( EMAC_DRV_CTRL *)dev->priv; 

	// Check to see if the emac has been put into
	// promiscuious mode by the operating system.
	
	// NOTE: The EMAC_Linkcheck function will update
	// the emac NA register with the promiscuity settings.
	if( dev->flags & IFF_PROMISC )
	{
		thisadapter->E_NA_settings &= ~E_NA_FILTER_MODE;
		thisadapter->E_NA_settings |= E_NA_FILTER_PROMIS;	
	}
	else
	{
		thisadapter->E_NA_settings &= ~E_NA_FILTER_MODE;
		thisadapter->E_NA_settings |= E_NA_FILTER_MC1PERFECT;	
	}
	
	// Get the current emac line state
	EMAC_Linkcheck( thisadapter, FALSE );

#if 0
	// If the physical link is not up then blow off any transmit packets
	if(	thisadapter->phyState.linkStatus == PHY_STAT_LINK_UP )
	{
		netif_carrier_on( dev );
	}
	else
	{
		netif_carrier_off( dev );
	}
#endif

	if( thisadapter->emac_state == OPEN )
	{
#if 1
		// If the physical link is not up then blow off any transmit packets
		if(	thisadapter->phyState.linkStatus == PHY_STAT_LINK_UP )
		{
			netif_carrier_on( dev );
		}
		else
		{
			netif_carrier_off( dev );
		}

#endif
		#if EMAC_DISABLE_CHK
		EMAC_Disabled_Check( dev );
		#endif

	#if (TX_FULL_DUPLEX && EMAC_MUTUALLY_EX_OPR)
		// Check for and configure emacs for mutally
		// exclusive transmitter operation.
		EMAC_Mutually_Ex_Op_Chk( dev );
	#endif
		
		if( netif_queue_stopped(dev) )
		{
			// Inform the OS that we can accept packets now
			netif_wake_queue(dev);
		}
	}
}

typedef struct
{
	EVENT_HNDL	taskevent;
	
} EMAC_EVENT_T;

EMAC_EVENT_T emac1_event;
EMAC_EVENT_T emac2_event;

void EMAC1_Task( struct net_device *dev )
{
	EMAC_DRV_CTRL *thisadapter = ( EMAC_DRV_CTRL *)dev->priv;
	EMAC_EVENT_T *pemac_event = &emac1_event;

	// Initialize task event
	INIT_EVENT( &pemac_event->taskevent );
	thisadapter->hw_reset_event = &pemac_event->taskevent;

	while(1)
	{
	#if HW_RESET_EVENT
		RESET_EVENT( thisadapter->hw_reset_event );
		WAIT_EVENT ( thisadapter->hw_reset_event, 1070 );
	#else
		TaskDelayMsec(1070);
	#endif
		EMAC_task( dev );
	}
}

void EMAC2_Task( struct net_device *dev )
{

	EMAC_DRV_CTRL *thisadapter = ( EMAC_DRV_CTRL *)dev->priv;
	EMAC_EVENT_T *pemac_event = &emac2_event;

	// Initialize task event
	INIT_EVENT( &pemac_event->taskevent );
	thisadapter->hw_reset_event = &pemac_event->taskevent;

	while(1)
	{
	#if HW_RESET_EVENT
		RESET_EVENT( thisadapter->hw_reset_event );
		WAIT_EVENT ( thisadapter->hw_reset_event, 1070 );
	#else
		TaskDelayMsec(1070);
	#endif
		EMAC_task( dev );
	}
}


/*******************************************************************************

FUNCTION NAME:
		sp_emac_close
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

static int emac_close(struct net_device *dev)
{

	unsigned long	flags;
	EMAC_DRV_CTRL *thisadapter=(EMAC_DRV_CTRL*)dev->priv;

#if 1
	// Disable interrupts
	local_irq_save(flags);

#endif
	// Effectively disable the tx watchdog timer by setting
	// it to a very long timeout.
	del_timer(&thisadapter->periodic_task);
	
	// Tell the OS we are closed
	netif_stop_queue(dev);

	// Indicate the emac is closed
	thisadapter->emac_state = CLOSED;
	
#if 0
	// Disable interrupts
	local_irq_save(flags);

#endif
	// Restart the hardware to bring it into a known state
	EMAC_restart_hw( dev, TRUE, TRUE );

#if 0
	// Reenable interrupts
	local_irq_restore(flags);

#endif
	// Zero out the statistics to start out clean when we open.
	memset( &thisadapter->stats, 0, sizeof ( struct net_device_stats ));

#if 1
	// Reenable interrupts
	local_irq_restore(flags);

#endif
	return 0;
}


/*******************************************************************************

FUNCTION NAME:
		sp_emac_get_stats
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

static struct net_device_stats *emac_get_stats(struct net_device *dev)
{
	EMAC_DRV_CTRL *thisadapter=(EMAC_DRV_CTRL*)dev->priv;

	return &(thisadapter->stats);
}


/*******************************************************************************

FUNCTION NAME:
		set_multicast_list
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

static void set_multicast_list(struct net_device *dev)
{}


/*******************************************************************************

FUNCTION NAME:
		EMAC_Rx_Process
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

#if 0
void EMAC_Rx_Process( unsigned long data )
{
	// Exists so CodeWright will so this function in its list
}	
#endif
#if RX_TASKLET
static void EMAC_Rx_Process( unsigned long data )
#else
static void EMAC_Rx_Process( struct net_device	*dev )
#endif
{

	register EMAC_DRV_CTRL		*thisadapter;
	register int				i;
	register int				idx;
	int							lookback_idx;
	unsigned long	    		status;
	unsigned long				pktlen;
	DWORD						cdt_ptr;
	DWORD						dma_ptr2;
	struct sk_buff				*skb;
	volatile EMAC_RXD_HW_QTYPE	*rxd_hw;
	BOOLEAN						rx_drop_pkts;	
	BOOLEAN						rx_disabled;
#if RX_TASKLET
	struct net_device	*dev;
	dev	= (struct net_device *)*((unsigned long *)data);
#endif
	thisadapter = ( EMAC_DRV_CTRL *)dev->priv;

	rx_drop_pkts = FALSE;
	rx_disabled = FALSE;
		
	rxd_hw = thisadapter->rxd_hw;
	
	i = 0;
	while( i < (RX_RING_SIZE / 1))
	{
		// The 'i' variable determines the maximum entries we will check.
		// The 'idx' variable determines where in the receive buffer CDT
		// 	that we will begin.  When the loop finishes 'idx' points to 
		// 	the last receive buffer that we looked at.
    	idx = rxd_hw->qndx;

		// Check to see if the DMA engine is catching up to us
		// and if so disable the receiver so that we do not overrun
		lookback_idx = rx_lookback_idx[ idx ];
		if(( rxd_hw->q[lookback_idx].status != 0 ) && (	rx_disabled == FALSE ))
		{
		    thisadapter->E_NA_settings &= ~E_NA_SR;
			*thisadapter->emac_na_reg = thisadapter->E_NA_settings;
			rx_disabled = TRUE;
		#if 0
			printk(KERN_DEBUG "%s rx lookback threshold limit\n", dev->name );
		#else
			thisadapter->stats.rx_over_errors++;
		#endif
		}

		i++;

		// Check to see if we have reached the CDT entry that
		// the dma hardware is currently working on.
		cdt_ptr = ((DWORD)(&rxd_hw->q[idx].mData)) & LOW_QWORD_MASK;
		dma_ptr2 = ((DWORD)(*thisadapter->emac_rx_dma_ptr2)) & LOW_QWORD_MASK;
		
		if(	cdt_ptr != dma_ptr2 )
		{
			rxd_hw->qndx = ( rxd_hw->qndx + 1 ) % RX_RING_SIZE;
	
			// Do we process or drop the packet?
			if( rx_drop_pkts == FALSE )
			{
				status = rxd_hw->q[idx].status;
		
				// Check the magic status bit
				// Always 1 for good frame?
				if( ( status & ( RX_A1 )) != 0 ) 
				{
					// Get the packet length
					pktlen = RX_FRM_LEN_GET( status );

					// Make sure the frame is neither too short or too long
					// AND that it did not have a CRC error
					if
					(
							( pktlen >= MIN_ETHERNET_SIZE )
						&&	( pktlen <= MAX_ETHERNET_SIZE )
						&&	!( status & (RSTAT_TS | RSTAT_TL | RSTAT_CE ))
					)
					{
						// Get a kernel buffer to put the data in
						skb = dev_alloc_skb( pktlen + DWORD_ALIGN_IP_HDR );
						if (skb != NULL)
						{
							skb->dev = dev;
							skb_reserve(skb, DWORD_ALIGN_IP_HDR );	/* Align IP on 16 byte boundaries */

							// Adjust the packet len pointer to remove the ethernet CRC from it
							pktlen -= ETHERNET_CRC_SIZE;

					        // NOTE: In the following memcpy the first QWORD is used by
							// the RMAC hardware - it means nothing to us except that we
							// must account for it in our data pointer calculations and
							// cluster allocations.

							RX_COPY_MEMORY
							(
							 	skb_put(skb,pktlen),
						        (void*)rxd_hw->q[idx].mData + SIZEOF_QWORD,
						        pktlen
							);

					  		dev->last_rx = jiffies;

						#if RX_SKB_DEBUG
							skb_debug(skb);
						#endif
							// Indicate to Linux that it is an ethernet packet
							skb->protocol = eth_type_trans(skb,dev);

							thisadapter->stats.rx_packets++;
							thisadapter->stats.rx_bytes+=pktlen;

							// Deliver the received packet to the protocol stack
							switch ( netif_rx(skb) )
							{
								case NET_RX_SUCCESS:
								case NET_RX_CN_LOW:
								case NET_RX_CN_MOD:
									break;
									
								case NET_RX_CN_HIGH:
									printk( KERN_DEBUG "%s NET_RX_CN_HIGH\n", dev->name);
									break;
									
								case NET_RX_DROP:
									printk( KERN_DEBUG "%s NET_RX_DROP\n", dev->name);
									rx_drop_pkts = TRUE;
									break;
									
								default:
									printk( KERN_DEBUG "unknown feedback return code\n");
									break;
							}
						}
						else
						{
							// No skbuffer to put the data into!
							emac_rx_proc_no_skb++;
						}
					}
					rxd_hw->q[idx].status = 0;
				}
				else
				{
					// Exit the loop because we have reached a receive
					// buffer that has not had any data put into it.
					break;
				}
 			}
			else
			{
				// Drop rx packets here
 				rxd_hw->q[idx].status = 0;
				thisadapter->stats.rx_dropped++;
			}
		}
		else
		{
			// Exit the loop because we are now up to the receive
			// buffer that the dma hardware is currently working in.
			break;
		}
	}

    thisadapter->E_NA_settings |= E_NA_SR;
	*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

	// Set the receive frame interrupt mask bit so
	// that receive frame interrupts will start
	// occurring again.
#if RX_DMA_INT
    cnxt_unmask_irq( thisadapter->rx_dma_int );	
#else
	thisadapter->E_IE_settings |= E_IE_RI;
	*thisadapter->emac_ie_reg = thisadapter->E_IE_settings;
#endif
}


#if RX_TASKLET
DECLARE_TASKLET( emac1_rx_tasklet, EMAC_Rx_Process, (unsigned long) &emac_dev[0]);
DECLARE_TASKLET( emac2_rx_tasklet, EMAC_Rx_Process, (unsigned long) &emac_dev[1]);
#endif


#if RX_DMA_INT
/******************************************************************************
|
|  Function:  EMAC_RxISR
|
|  Description:  handle device RX interrupts
|				 This interrupt service routine, reads device interrupt status, acknowledges
|				 the interrupting events, and schedules transmit when required.
|				
|  Parameters:  *pDrvCtrl - Pointer Driver Cntrl
|
|  Returns:   NONE
*******************************************************************************/

static void EMAC_RxISR(int irq, void *dev_id, struct pt_regs * regs)
{
	register EMAC_DRV_CTRL *thisadapter=( EMAC_DRV_CTRL *)((struct net_device *)dev_id)->priv; 

	/* Clear the INT1_Stat register of pending TX interrupts */
	*INT_Stat = thisadapter->int_stat_clr_rx;

#if RX_TASKLET		
	// Process the received frames(s) via a tasklet
	// Clear out the receive frame interrupt mask after
	// getting the interrupt.  The tasklet will set the
	// mask bit again after it processes the received
	// frames.  This keeps the system from getting more
	// receive frame interrupts while it is already 
	// scheduled to run the tasklet or is already running
	// the tasklet.
    cnxt_mask_irq( thisadapter->rx_dma_int );	
	 
	switch( thisadapter->emac_num )
	{
		case 1:
			tasklet_schedule(&emac1_rx_tasklet);
			break;

		case 2:
			tasklet_schedule(&emac2_rx_tasklet);
			break;
	}
#else
	EMAC_Rx_Process( dev );
#endif
}
#endif

#if TX_TASKLET
static void EMAC_Tx_Process( unsigned long data )
{
#if EMAC_TX_UDELAY
	udelay( 2 );
#endif
	EMAC_Tx_Pkt_Done((struct net_device *)*((unsigned long *)data) );
}

DECLARE_TASKLET( emac1_tx_tasklet, EMAC_Tx_Process, (unsigned long) &emac_dev[0]);
DECLARE_TASKLET( emac2_tx_tasklet, EMAC_Tx_Process, (unsigned long) &emac_dev[1]);
#endif

#if TX_DMA_INT
/******************************************************************************
|
|  Function:  EMAC_TxISR
|
|  Description:  handle device TX interrupts
|				 This interrupt service routine, reads device interrupt status, acknowledges
|				 the interrupting events, and schedules transmit when required.
|				
|  Parameters:  *pDrvCtrl - Pointer Driver Cntrl
|
|  Returns:   NONE
*******************************************************************************/

static void EMAC_TxISR(int irq, void *dev_id, struct pt_regs * regs)
{
	register EMAC_DRV_CTRL *thisadapter=( EMAC_DRV_CTRL *)((struct net_device *)dev_id)->priv; 

	/* Clear the INT1_Stat register of pending TX interrupts */
	*INT_Stat = thisadapter->int_stat_clr_tx;

#if EMAC_TX_WD_TIMER
	// Effectively disable the tx watchdog timer by setting
	// it to a very long timeout.
	del_timer(&thisadapter->wd_timer );
#endif
#if TX_TASKLET		
	switch( thisadapter->emac_num )
	{
		case 1:
			tasklet_schedule(&emac1_tx_tasklet);
			break;

		case 2:
			tasklet_schedule(&emac2_tx_tasklet);
			break;
	}
#else
	// TRANSMIT FRAMES
	// Check to see if an EMAC frame transmitted OK
	// Perform housekeeping on the just transmitted frame
#if EMAC_TX_UDELAY
	udelay( 1 );
#endif
	EMAC_Tx_Pkt_Done( dev_id );
#endif
}
#endif


/******************************************************************************
|
|  Function:  EMAC_Exception_ISR
|
|  Description:   handle ethernet DMA device errors
|				
|  Returns:   None
*******************************************************************************/

static void EMAC_Exception_ISR(int irq, void *dev_id, struct pt_regs * regs)
{
    register unsigned long 	estatus;	/* contains the status of the ethernet hardware */
#if (!RX_DMA_INT || !TX_DMA_INT)
    register unsigned long 	lstatus;	/* contains the status of the last packet register */
#endif
	register EMAC_DRV_CTRL *thisadapter=( EMAC_DRV_CTRL *)((struct net_device *)dev_id)->priv; 

	// Get the EMAC exception condition registers
	estatus = *thisadapter->emac_stat_reg;
#if (!RX_DMA_INT || !TX_DMA_INT)
    lstatus = *thisadapter->emac_lp_reg;
#endif

	// Now clear out the ARM INT_Stat register interrupt that got us here
	*thisadapter->emac_stat_reg = estatus;

#if (!RX_DMA_INT || !TX_DMA_INT)
    *thisadapter->emac_lp_reg = lstatus;
#endif
	*INT_Stat = thisadapter->int_stat_clr_dev_irq;

#if !RX_DMA_INT
	// RECEIVE FRAMES
	// Check to see if an EMAC frame was received properly
	if( lstatus & ( E_LP_RI ))
	{
	#if RX_TASKLET		
		// Process the received frames(s) via a tasklet
		// Clear out the receive frame interrupt mask after
		// getting the interrupt.  The tasklet will set the
		// mask bit again after it processes the received
		// frames.  This keeps the system from getting more
		// receive frame interrupts while it is already 
		// scheduled to run the tasklet or is already running
		// the tasklet.
		thisadapter->E_IE_settings &= ~E_IE_RI;
		*thisadapter->emac_ie_reg = thisadapter->E_IE_settings;
		 
		switch( thisadapter->emac_num )
		{
			case 1:
				tasklet_schedule(&emac1_rx_tasklet);
				break;

			case 2:
				tasklet_schedule(&emac2_rx_tasklet);
				break;
		}
	#else
		EMAC_Rx_Process( dev );
	#endif
	}
#endif
#if !TX_DMA_INT
	#if USE_TU_INTERRUPT
	// TRANSMIT FRAMES
	// Check to see if the EMAC transmitter has stopped
    if( estatus & (E_S_TU) )
	{
		#if EMAC_TX_WD_TIMER
		del_timer(&thisadapter->wd_timer );
		#endif
		#if TX_TASKLET		
		switch( thisadapter->emac_num )
		{
			case 1:
				tasklet_schedule(&emac1_tx_tasklet);
				break;

			case 2:
				tasklet_schedule(&emac2_tx_tasklet);
				break;
		}
		#else
		// TRANSMIT FRAMES
		// Check to see if an EMAC frame transmitted OK
		// Perform housekeeping on the just transmitted frame
			#if EMAC_TX_UDELAY
		udelay( 1 );
			#endif
		EMAC_Tx_Pkt_Done( dev_id );
		#endif
	}
	#else
	// TRANSMIT FRAMES
	// Check to see if an EMAC frame transmitted OK
    if
    ( 
    		( lstatus & ( E_LP_TI ) )
		#if HOMEPLUGPHY
			// The CX11656 HomePlug device uses the mii CRS bit
			// to indicate when it has a free tx buffer.
    	||	( PhyActiveHOMEPLUG( thisadapter->pPhy) & ( estatus & ( E_S_LCRS) ) )
		#endif
    )
	{
		// Perform housekeeping on the just transmitted frame
		EMAC_Tx_Pkt_Done( dev ); 
	}
	#endif
#endif

	// Check for tx/rx frame errors
	if(	estatus & ( E_S_LC | E_S_16 ))
	{
		// FATAL ERROR CONDITIONS
		// NOTE: These error conditions must fall through to
		// the EMAC_restart_hw at the end of the list.
		if ( estatus & E_S_LC )
		{
			#if EMAC_ISR_ERR_MSG_PRINT
			printk( KERN_DEBUG "ETH%d: TX Late Collision\n", thisadapter->unit);  
			#endif
		}

		if( estatus &  E_S_16 )
	    {
			#if EMAC_ISR_ERR_MSG_PRINT
			printk( KERN_DEBUG "ETH%d: TX Excessive Collisions\n", thisadapter->unit);  
			#endif
		}
		thisadapter->stats.collisions += (estatus & TSTAT_CC);	

		EMAC_stop_hw( dev_id );
		
		// FATAL error in mutually exclusive tx parts
		thisadapter->hw_reset_required = TRUE;
		
	#if HW_RESET_EVENT
		SET_EVENT( thisadapter->hw_reset_event );
	#endif
	}
}


/******************************************************************************
|
|  Function:  EMAC_Linkcheck
|
|  Description: Read the PHY link status and set the MAC according to the 
|				established link rate
|
|  Returns:   NONE
|
*******************************************************************************/

void EMAC_Linkcheck(EMAC_DRV_CTRL *thisadapter, BOOLEAN force_update )
{
    
	unsigned long phy_settings;
    
	// If TRUE then clear out the adapter phySettings variable so
	// that the logical flow of this function will cause the
	// adapter E_NA_settings and the E_NA_x hardware register for
	// this adapter to be updated.
	if( force_update == TRUE )
	{
		thisadapter->phyState.phySettings = 0;
	}

	/*************************************************
	* 	Read the current link settings from the PHY. *
	*************************************************/
    phy_settings = PhyCheck(thisadapter->pPhy);

    /* Check settings and record the link state.
	   If we are in FORCEDMODE, skip speed and duplex check.
       If we are in auto-negotiation mode, deduce what the link speed and duplex
       should be.  If the link is down, default it to 10 HDX.
    */
    if ( phy_settings != thisadapter->phyState.phySettings )
    {
        thisadapter->phyState.phySettings = phy_settings;

        if ( phy_settings & PHY_LINK_UP )
        {
            if ( thisadapter->phyState.linkStatus != PHY_STAT_LINK_UP )
            {
                thisadapter->phyState.linkStatus = PHY_STAT_LINK_UP;
            }

        	if ( thisadapter->phyState.autoNegotiate == PHY_AUTONEGOTIATE )
			{
	            if ( phy_settings & PHY_LINK_100 )
                {
	                thisadapter->phyState.networkSpeed = PHY_STAT_SPEED_100;
                }
	            else
                {
	                thisadapter->phyState.networkSpeed = PHY_STAT_SPEED_10;
                }

	            if ( phy_settings & PHY_LINK_FDX )
				{
	                thisadapter->phyState.duplexMode = PHY_STAT_DUPLEX_FULL;
	            }
	            else
	            {
	                thisadapter->phyState.duplexMode = PHY_STAT_DUPLEX_HALF;
				}
			}
        }
        else
        {
            if ( thisadapter->phyState.linkStatus != PHY_STAT_LINK_DOWN )
            {
                thisadapter->phyState.linkStatus = PHY_STAT_LINK_DOWN;
            }

        	if ( thisadapter->phyState.autoNegotiate == PHY_AUTONEGOTIATE )
			{
	            thisadapter->phyState.networkSpeed = PHY_STAT_SPEED_100;
	            thisadapter->phyState.duplexMode = PHY_STAT_DUPLEX_FULL;
			}
        }

		// Update the E_NA_settings and E_NA_x hardware register
        EMACHW_Configure_Mac
        (
        	thisadapter,
        	TRUE,
			(thisadapter->phyState.networkSpeed == PHY_STAT_SPEED_100 ),
			(thisadapter->phyState.duplexMode == PHY_STAT_DUPLEX_FULL )
		);
	}
}


/******************************************************************************
|
|  Function:  EMAC_init_tx
|
|  Description: initializes all queues and registers necessary to begin receiving
| 				data from the RMAC.
|
|  Returns:   OK on success; otherwise ERROR.
|
*******************************************************************************/

static signed int EMAC_init_tx( EMAC_DRV_CTRL *	thisadapter )
{
	BYTE				*txd_buf;
	BYTE				*txd_end;
	unsigned int		i;

#if SDRAM_TX_MEM
    txd_end = (BYTE *)EMAC_KMALLOC(sizeof(EMAC_TD)+16, GFP_KERNEL);
#else
	txd_end = SYSMEM_Allocate_BlockMemory( 1, sizeof(EMAC_TD)+16, 16 );
#endif
    if (txd_end == NULL)
    {
    	printk("EMAC:TxD_end alloc() FAIL!\n");
    }

    QWORD_ALIGN_PTR( txd_end );
	thisadapter->TxD_end = (EMAC_TD *)txd_end;

	// Zero out thisadapter->TxD_end structure
	// so that everything is at a known state
	memset( thisadapter->TxD_end, 0, sizeof(EMAC_TD) + 16 );

	// Obtain memory for the txd buffers
#if SDRAM_TX_MEM
	if(( txd_buf = (BYTE *) EMAC_KMALLOC((TX_RING_SIZE * ETH_PKT_BUF_SZ) + QWORD_ALIGN_PAD, GFP_KERNEL)) == NULL ) 
#else
	if(( txd_buf = SYSMEM_Allocate_BlockMemory( 1, (TX_RING_SIZE * ETH_PKT_BUF_SZ) + QWORD_ALIGN_PAD, 16 )) == NULL ) 
#endif
    {
    	printk("EMAC:txd_buf alloc() FAIL!\n");
    }

	// Save the unaligned txd buffer pointer originally obtained
	// from kmalloc to subsequent use in freeing the memory.
	thisadapter->txd_buf_mem = txd_buf;
    QWORD_ALIGN_PTR(txd_buf);

	// Initialize the txd buffer array with pointers to the buffers
	for( i = 0; i < TX_RING_SIZE; i++ ) 
	{
		thisadapter->txd_buf[i] = (txd_buf + (ETH_PKT_BUF_SZ * i));
	}

    //	Initialize the tx msg queue.
    thisadapter->tx_msg_q_next = 0;
    thisadapter->tx_msg_q_current = 0;
    thisadapter->tx_msg_q_size = 0;
    memset((char*)thisadapter->tx_msg_q, 0, sizeof(thisadapter->tx_msg_q));
	
    return 0;
}


/******************************************************************************
|
|  Function:  init_RX_EMAC
|
|  Description: initializes all queues and registers necessary to begin receiving
| 				data from the RMAC.
|
|  Returns:   OK on success; otherwise ERROR.
|
*******************************************************************************/

static signed int EMAC_init_rx( EMAC_DRV_CTRL	*thisadapter )
{
	EMAC_RXD_HW_QTYPE	*rxd_hw;
	BYTE				*rxd_buf;
	unsigned int		i;

	/****************************************************************
    	Obtain and clear memory for the RxD table and management 
        information list.
    ****************************************************************/
    if (( rxd_hw = (EMAC_RXD_HW_QTYPE *)EMAC_KMALLOC(sizeof(EMAC_RXD_HW_QTYPE)+16,GFP_KERNEL)) == NULL )
    {
    	printk("EMAC:rxd_hw alloc() FAIL!\n");
    }

    QWORD_ALIGN_PTR(rxd_hw);
	thisadapter->rxd_hw = rxd_hw;
	
	// Zero out thisadapter->rxd_hw structure
	// so that everything is at a known state
	memset( thisadapter->rxd_hw, 0, sizeof(EMAC_RXD_HW_QTYPE) + 16 );
	
	// Obtain memory for the rxd buffers
	if(( rxd_buf = (BYTE *) EMAC_KMALLOC((RX_RING_SIZE * ETH_PKT_BUF_SZ) + QWORD_ALIGN_PAD, GFP_KERNEL)) == NULL ) 
    {
    	printk("EMAC:rxd_buf alloc() FAIL!\n");
    }

	// Save the unaligned rxd buffer pointer originally obtained
	// from kmalloc to subsequent use in freeing the memory.
	thisadapter->rxd_buf = rxd_buf;
    QWORD_ALIGN_PTR(rxd_buf);

	// Initialize the rxd hardware q buffer pointers with buffers
	for( i = 0; i < RXD_NUM; i++ ) 
	{
		rxd_hw->q[i].mData = rxd_buf + (ETH_PKT_BUF_SZ * i);
		rxd_hw->q[i].reserved = 0;
		rxd_hw->q[i].status = 0;
		rxd_hw->q[i].mib = 0;
	}

	/****************************************************************
    	Initialize the hardware RxD Q index.
    ****************************************************************/
    rxd_hw->qndx = 0;

	/****************************************************************
		Initialize DMA4 to generate an interrupt at the end of 
        receiving a packet.  Leave some space for padding at the
        beginning and ending of the cluster.  At least seven bytes
        are needed for cluster alignment, eight bytes are used by
        the RMAC DMA channel, and two bytes may be used by the 
        IP alignment copy if an in-cluster copy is performed.
    ****************************************************************/
	*thisadapter->emac_rx_dma_ptr2 = (unsigned long)(&rxd_hw->q[0]); /* address of descriptor list */
    *thisadapter->emac_rx_dma_cnt1 = (unsigned long)((CL_SIZ/8)-4); /* size of cluster in qwords */
	*thisadapter->emac_rx_dma_cnt2 = (unsigned long)(2 * RXD_NUM);  /* number of descriptors * 2 */

    return 0;
}


static int emac_set_mac_address(struct net_device *dev, void *addr)
{
	int i;

	if(netif_running(dev))
	{
    	return -EBUSY;
	}

	printk("%s : CNXT set mac address\n",dev->name);

	for( i = 0; i < MAC_ADDR_LEN; i++)
	{
	    printk(" %2.2x",dev->dev_addr[i]=((unsigned char *)addr)[i]);
	}
	printk(".\n");
	return 0;
}


/******************************************************************************
|
|  Function:  sp_emac_open
|
|  Returns:   OK on success; otherwise ERROR.
|
*******************************************************************************/

static int emac_open(struct net_device *dev)
{

	EMAC_DRV_CTRL *thisadapter=(EMAC_DRV_CTRL*)dev->priv;

	// Get the current emac line state
	EMAC_Linkcheck( thisadapter, TRUE );

	// If the physical link is not up then blow off any transmit packets
	if(	thisadapter->phyState.linkStatus == PHY_STAT_LINK_UP )
	{
		netif_carrier_on( dev );
	}
	else
	{
		netif_carrier_off( dev );
	}

#if (TX_FULL_DUPLEX && EMAC_MUTUALLY_EX_OPR)
	// Check for and configure emacs for mutally
	// exclusive transmitter operation.
	EMAC_Mutually_Ex_Op_Chk( dev );
#endif
	
#if HOMEPLUGPHY
    if( PhyActiveHOMEPLUG( thisadapter->pPhy) )
    {
	    TeslaControl_Str TeslaControl ;

		// Pointer back to dev needed for homeplug functions
		thisadapter->dev = dev;
		
        Emac_TeslaConfigRead( &TeslaControl ) ;
        Emac_TeslaSetup( (PVOID)thisadapter, &TeslaControl) ;
    }
#endif
	
	// Set up the emac hardware filtering for perfect address filtering 
	thisadapter->E_NA_settings |= E_NA_FILTER_MC1PERFECT;	
	*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

	*thisadapter->emac_stat_reg = 0;

	// Configure the emac hardware filtering 
	emac_SetupFrame ( dev );

	/********************************************
	    Set the operating modes for the EMAC
        receiver and transmitter.
	********************************************/

	thisadapter->E_IE_settings =
	(
#if !RX_DMA_INT
		E_IE_RI		|	/* Receive OK interrupt enable (masks E_LP_RI) */
#endif	
#if !TX_DMA_INT
	#if USE_TU_INTERRUPT
		E_IE_TU		|	/* Transmit process stopped interrupt enable (masks E_S_TU) */
	#endif
#endif	
		E_IE_LC		|	/* Late collision interrupt enable (masks E_S_LC) */
		E_IE_16			/* 16 collisions interrupt enable (masks E_S_16) */
	);

#if HOMEPLUGPHY
	if( PhyActiveHOMEPLUG( thisadapter->pPhy) )
	{
		// The CX11656 device uses the mii CRS signal
		//  to indicate when it has a free tx buffer
		thisadapter->E_IE_settings |= E_IE_LCRS;
	}
	else
	{
	#if !TX_DMA_INT
		thisadapter->E_IE_settings |= E_IE_TI;
	#endif
	}
#endif

	*thisadapter->emac_ie_reg = thisadapter->E_IE_settings;
    
	// Start the receiver operating now
    thisadapter->E_NA_settings |= (E_NA_SR|E_NA_SB);
	*thisadapter->emac_na_reg = thisadapter->E_NA_settings;

	// Indicate the emac is open
	thisadapter->emac_state = OPEN;
	
	// Enable the device irq
    cnxt_unmask_irq( dev->irq );
	
#if TX_DMA_INT
    cnxt_unmask_irq( thisadapter->tx_dma_int );	
#endif
#if RX_DMA_INT
    cnxt_unmask_irq( thisadapter->rx_dma_int );	
#endif

	// Inform the OS that we can accept packets now
	netif_start_queue(dev);

	printk("%s: (CX821xx EMAC) started!\n",dev->name);
	return 0;
}


/******************************************************************************
|
|  Function:  ResetChip
|
|  Description:   Reset the chip and setup E_NA register
|				  This routine stops the transmitter and receiver, masks all interrupts, then
|				  resets the ethernet chip. Once the device comes out of the reset state, it
|				  initializes E_NA register.
|
|  Parameters:  
|
|  Returns:   
*******************************************************************************/

void ResetChip( EMAC_DRV_CTRL *thisadapter )	/* pointer to device control structure */
{
    
	/* Delay between applying the reset condition and clearing it 
		and then delay again after releasing the condition. Sometimes not all
		aspects of the chip have reached their new state with a back to back
		bit set and clear. */

	// NOTE: The following delays between the register writes were empirically
	// derived because there is no value specified in the hardware document
	// as to how long it actually takes.
	// Do the delays as a series of usec delays so that if an interrupt is
	// pending it can occur between them.
    EMAC_REG_WRITE( E_NA, E_NA_RTX, thisadapter->emac_hdw_num );
	udelay( 100 );
	udelay( 100 );
	udelay( 50 );
    EMAC_REG_WRITE( E_NA, 0, thisadapter->emac_hdw_num );
	udelay( 100 );
	udelay( 100 );
	udelay( 50 );
    EMAC_REG_WRITE( E_NA, 0, thisadapter->emac_hdw_num );
	udelay( 100 );
	udelay( 100 );
	udelay( 50 );
}


#if USE_HASHTABLE
HI HashIndex(unsigned char* eAddr)
{
    HI hi;/* hash index - return value */
    hi  = eAddr[0];
    hi ^= eAddr[1];
    hi ^= eAddr[2];
    return hi; 
}

/******************************************************************************
|
|  Function:  SetupHashTable
|
|  Description:  sets up the hash table for multicast addresses.
|				 This routine should be called whenever there is an addition
|				 or a deletion of multicast address. And also in endStart.
|				 While the first call to this routine during chip 
|				 initialization requires that the receiver be turned off, 
|				 subsequent calls do not.
|
|  Returns:    OK/ERROR
*******************************************************************************/
static void SetupHashTable(struct net_device *dev)
{
    EMAC_DRV_CTRL *thisadapter = (EMAC_DRV_CTRL *)dev->priv;
	struct dev_mc_list *dmi = dev->mc_list;
	HI hi;
	int i;


#ifdef SUPPORT_BROADCAST_FILTER
    UINT8	ethBcastAdrs[]={ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#endif /* SUPPORT_BROADCAST_FILTER */

    /* 
     * Initialize the hash table to zeroes so that the "wrong" 
     * multicast address don't get reflected in the table this time.
     * Here the "wrong" are the ones added/deleted between the last two
     * consecutive calls of this function.
     */
    memset( (signed char*)thisadapter->hashTable,0, HASH_TABLE_SIZE );
    
	if (!(dev->flags & (IFF_PROMISC | IFF_ALLMULTI))) 
	{
		for (i = 0; i < dev->mc_count; i++)
		{
			hi = (HI)HASH_INDEX ((unsigned char*)dmi->dmi_addr );
			ADD_TO_HASHTBL(thisadapter, hi);
			dmi = dmi->next;
		}
	}
	
#ifdef SUPPORT_BROADCAST_FILTER
    /* Install the ethernet broadcast address */
    hi = HASH_INDEX ((unsigned char*)ethBcastAdrs);
    ADD_TO_HASHTBL(pDrvCtrl, hi);       /* Add index to table */
#endif /* SUPPORT_BROADCAST_FILTER */


}
#endif

/******************************************************************************
|
|  Function:  sp_emac_probe
|
|  Description: 
|  Parameters:  struct net_device *dev
|
|
*******************************************************************************/

int __init emac_probe(struct net_device *dev)
{
	EMAC_DRV_CTRL	*thisadapter;
	EMAC_DRV_CTRL	*temp;
	
	DATA_QUEUE_T	*tx_pkt_q;
	int				i;
	int				j;
	unsigned long	flags;
	DWORD			dev_num;
	DWORD			hdw_idx;
	ULONG			*txd_end;

	// Determine what ethernet device number we are
	if (dev->name && (strncmp(dev->name, "eth", 3) == 0))
	{
		dev_num = simple_strtoul(dev->name + 3, NULL, 0);

		// Do this stuff only when the first device is probed
		if( dev_num == 0 )
		{
			EMAC_hdw_cntr = 0;

		#if (TX_FULL_DUPLEX && EMAC_MUTUALLY_EX_OPR)
			// Initially assume that we do not
			// need mutually exclusive operation.
			emac_mutually_ex_opr = FALSE;
		#else
			emac_mutually_ex_opr = TRUE;
		#endif
					
		#if SETUP_FRAME_VERIFY
			sf_verify = FALSE;
		#endif
			for( i = 0; i < RX_RING_SIZE; i++ )
			{
				j = ( i + (	RX_RING_SIZE - RX_LOOKBACK_THRESHOLD )) % RX_RING_SIZE;
				rx_lookback_idx[ i ] = j;
			}
			
			// Allocate space for the transmit frame queues
			EMAC_tx_pkt_q1 = ( DATA_QUEUE_T *)(EMAC_KMALLOC( sizeof(DATA_QUEUE_T), GFP_KERNEL));
			EMAC_tx_pkt_q2 = ( DATA_QUEUE_T *)(EMAC_KMALLOC( sizeof(DATA_QUEUE_T), GFP_KERNEL));
		}

		// NOTE: dev_num is zero relative
		if( dev_num > ( MAX_ETH_PORTS - 1 ) || EMAC_hdw_cntr > (MAX_ETH_PORTS - 1))
		{
			#if 0
			printk("emac_probe: %s is not supported by CX821xx Family BSP\n",dev->name);
			#endif
			return -ENODEV;
		}
	}
	else
	{
		// Device name was not an ethernet device
		return -ENODEV;
	}

	thisadapter=( EMAC_DRV_CTRL *)(EMAC_KMALLOC(sizeof(EMAC_DRV_CTRL)+QWORD_ALIGN_PAD, GFP_KERNEL));

	if(thisadapter == NULL)
	{
		printk("emac_probe: Out Of Memory %s\n",dev->name);
		return -ENODEV;
	}

	// Zero out the emac drive control structure
	// so that everything is at a known state
	memset( thisadapter, 0, sizeof(EMAC_DRV_CTRL)+QWORD_ALIGN_PAD);
	
	// Save a copy of the original adapter pointer
	// in case we have to free the memory.
	temp = thisadapter;

	QWORD_ALIGN_PTR(thisadapter);
	dev->priv = thisadapter;

    if( !(thisadapter->pPhy = PhyAllocateMemory()) )	// PORT
    {
		printk("Fatal Error -- PHY OBJ Memory allocation Failure\n");
		return -ENODEV;
    }

	// Get the configuration parameters for the emac from the flash
    if ( EMAC_GetPortConfig( dev_num, thisadapter) == FALSE )
    {
        /* Error!*/

        return -ENODEV;
    }

	// Probe for emac hardware now
	for( hdw_idx = EMAC_hdw_cntr; hdw_idx <= MAX_ETH_PORTS; hdw_idx++)
	{

		EMAC_hdw_cntr = hdw_idx;
		
		// Fill in the emac hardware number field of the first
		// emac hardware that we will try for use by the Phy code.
		// NOTE: This field is zero relative.

		thisadapter->emac_hdw_num = hdw_idx;
		// Check to see if we reached the end of the list
		// potential emac hardware ports.
		if ( hdw_idx >= MAX_ETH_PORTS )
		{
			// Free up the emac adapter context memory
			EMAC_KFREE(temp);				
			EMAC_KFREE( thisadapter->pPhy );
			printk("emac_probe:  PHY INIT failed.\n");
			return -ENODEV;
		}
					
		// Reset the emac hardware 
		ResetChip( thisadapter );
		
		// Probe for the presence of and if found, reset
		// and initialize the PHY and MAC layers.
	    if( PhyInit(
	    				(unsigned long) EMAC_REG_ADDR(E_DMA, thisadapter->emac_hdw_num ),
						thisadapter->pPhy, 
						EMAC_MapToPhyConn ( &thisadapter->phyState ), 
						PHY_HWOPTS_PORT_HOPPING,
						thisadapter->pHpna

					) == PHY_STATUS_OK )
	    {

			// We found an emac device
			hdw_idx++;
			EMAC_hdw_cntr = hdw_idx;
			break;
		}
	}

	thisadapter->flags = 0;

#if EMAC_TX_WD_TIMER
	#if 1
	// Disable interrupts
	local_irq_save(flags);

	#endif
	init_timer(&thisadapter->wd_timer);
	thisadapter->wd_timer.data = (unsigned long)dev;
	thisadapter->wd_timer.function = (void *)&EMAC_tx_timeout;
	#if 1

	// Reenable interrupts
	local_irq_restore(flags);
	#endif

#endif
	spin_lock_init(&thisadapter->lock);
	
	// This is the Linux ethx number
	thisadapter->unit = dev_num;

#if EMAC_MUTUALLY_EX_OPR
	// See if this is a mutually exclusive tx emac part
	thisadapter->tx_mutex_part = CnxtBsp_Mac_MutEx_Chk();
#endif

	//!!! test code !!!
	printk("emac_probe: %s found.\n",dev->name);
	//!!! test code !!!

	// Initialize the device specific stuff
	switch( thisadapter->emac_hdw_num )
	{
		// emac1
		case 0:
			emac_dev[0] = dev;
			thisadapter->emac_num = 1;

			dev->irq = CNXT_INT_LVL_E1_ERR;
			dev->base_addr = (ULONG)(EMAC_BASE_ADDR + ( dev_num * 0x00010000));
			thisadapter->emac_tx_dma_ptr =	DMAC_1_Ptr1;
		#if EMAC_MUTUALLY_EX_OPR
			// CX82100-41 parts and above do not need this nonsense
			if( thisadapter->tx_mutex_part == FALSE )
			{
				// CX82100-41
				thisadapter->emac_tx_dma_ptrx =	DMAC_1_Ptr1;
			}
			else
			{
				thisadapter->emac_tx_dma_ptrx =	DMAC_3_Ptr1;
			}
		#endif
			thisadapter->emac_rx_dma_ptr2 = DMAC_2_Ptr2;
			thisadapter->emac_tx_dma_cnt =	DMAC_1_Cnt1;
			thisadapter->emac_rx_dma_cnt1 = DMAC_2_Cnt1;
			thisadapter->emac_rx_dma_cnt2 = DMAC_2_Cnt2;
			thisadapter->emac_na_reg = E_NA_1_reg;
			thisadapter->emac_stat_reg = E_STAT_1_reg;
			thisadapter->emac_lp_reg = E_LP_1_reg;
			thisadapter->emac_ie_reg = E_IE_1_reg;
			thisadapter->tx_dma_int = CNXT_INT_LVL_DMA1;
			thisadapter->rx_dma_int = CNXT_INT_LVL_DMA2;
			thisadapter->int_stat_clr_dev_irq = CNXT_INT_MASK_E1_ERR;
			thisadapter->int_stat_clr_tx = INT_DMA_EMAC_1_TX;
			thisadapter->int_stat_clr_rx = INT_DMA_EMAC_1_RX;
			thisadapter->tx_pkt_in_progress = FALSE;

		#if TX_FULL_DUPLEX
			thisadapter->emac_tx_q = EMAC_tx_pkt_q1;
		#else
			thisadapter->emac_tx_q = EMAC_tx_pkt_q2;
		#endif
			tx_pkt_q = thisadapter->emac_tx_q;
			tx_pkt_q->entry_cnt = 0;
			tx_pkt_q->get_idx = 0;
			tx_pkt_q->put_idx = 0;

			kernel_thread( (void *)EMAC1_Task, dev , CLONE_FS | CLONE_FILES  );
			break;
			
		// emac2
		case 1:
			emac_dev[1] = dev;
			thisadapter->emac_num = 2;

			dev->irq = CNXT_INT_LVL_E2_ERR;
			dev->base_addr = (ULONG)(EMAC_BASE_ADDR + ( dev_num * 0x00010000));
			thisadapter->emac_tx_dma_ptr =	DMAC_3_Ptr1;
		#if EMAC_MUTUALLY_EX_OPR
			thisadapter->emac_tx_dma_ptrx =	DMAC_3_Ptr1;
		#endif
			thisadapter->emac_rx_dma_ptr2 = DMAC_4_Ptr2;
			thisadapter->emac_tx_dma_cnt =	DMAC_3_Cnt1;
			thisadapter->emac_rx_dma_cnt1 = DMAC_4_Cnt1;
			thisadapter->emac_rx_dma_cnt2 = DMAC_4_Cnt2;
			thisadapter->emac_na_reg = E_NA_2_reg;
			thisadapter->emac_stat_reg = E_STAT_2_reg;
			thisadapter->emac_lp_reg = E_LP_2_reg;
			thisadapter->emac_ie_reg = E_IE_2_reg;
			thisadapter->tx_dma_int = CNXT_INT_LVL_DMA3;
			thisadapter->rx_dma_int = CNXT_INT_LVL_DMA4;
			thisadapter->int_stat_clr_dev_irq = CNXT_INT_MASK_E2_ERR;
			thisadapter->int_stat_clr_tx = INT_DMA_EMAC_2_TX;
			thisadapter->int_stat_clr_rx = INT_DMA_EMAC_2_RX;
			thisadapter->tx_pkt_in_progress = FALSE;
			thisadapter->emac_tx_q = EMAC_tx_pkt_q2;

		#if TX_FULL_DUPLEX
			tx_pkt_q = thisadapter->emac_tx_q;
			tx_pkt_q->entry_cnt = 0;
			tx_pkt_q->get_idx = 0;
			tx_pkt_q->put_idx = 0;
		#endif

			kernel_thread( (void *)EMAC2_Task, dev, CLONE_FS | CLONE_FILES  );
			break;
	}

	// Retrieve the mac address for the device
	CnxtBsp_Get_Mac_Address( dev_num, &thisadapter->PortNetAddress[0] );

    /* memory Initializations.
     * Allocation & Initialization of all the memory required by an 
     * instance of the driver is done in the following function.
     */

	// Initialize the memory used by the transmitter
    EMAC_init_tx( thisadapter );
	printk("Tx DMA Ring %lx \n",(DWORD)thisadapter->txd_buf[0]);

	// Initialize END TMAC TRANSMIT DESCRIPTOR
	// NOTE: This must be done after the EMAC_init_tx function call
	thisadapter->TxD_end->status = 0;     
	thisadapter->TxD_end->status_zero =0;   
	thisadapter->TxD_end->control = 0;

	txd_end = (ULONG *) thisadapter->TxD_end;
	txd_end[6] = (ULONG) txd_end;
	txd_end[7] = 3;				/* avoids prefetch of next ptr and ctr */

	// Initialize the memory used by the receiver
    EMAC_init_rx( thisadapter );
	printk("Rx DMA Ring %lx \n",(DWORD)thisadapter->rxd_buf);
	
	SET_MODULE_OWNER(dev);

	dev->open				= emac_open;
	dev->stop				= emac_close;
	dev->hard_start_xmit	= emac_send_packet;
	dev->get_stats			= emac_get_stats;
	dev->set_multicast_list = &set_multicast_list;
	dev->set_mac_address	= emac_set_mac_address;
#if 0
	dev->tx_timeout			= EMAC_Linux_tx_timeout;
	dev->watchdog_timeo		= TX_TIMEOUT_DELAY + 10;
#endif
	
	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);

	printk("%s irq=%d ",version,dev->irq);
	
	for( i = 0; i < MAC_ADDR_LEN ; i++)
	{
		printk("%2.2x:", dev->dev_addr[i] = thisadapter->PortNetAddress[i]);
	}
	printk("\n");

	// Connect the device irq to the OS interrupt handler
	if (request_irq( dev->irq, &EMAC_Exception_ISR, 0, "Cnxt Ethernet Media Access Ctrl", dev))
	{
		return -EAGAIN;		
	}

	// Disable the device irq
    cnxt_mask_irq( dev->irq );

#if TX_DMA_INT
	if (request_irq( thisadapter->tx_dma_int, &EMAC_TxISR, 0, "emac Tx Isr", dev))
	{
		return -EAGAIN;		
	}
	
    cnxt_mask_irq( thisadapter->tx_dma_int );	
#endif
#if RX_DMA_INT
	if (request_irq( thisadapter->rx_dma_int, &EMAC_RxISR, 0, "emac Rx Isr", dev))
	{
		return -EAGAIN;		
	}
	
    cnxt_mask_irq( thisadapter->rx_dma_int );	
#endif

	return 0;
}

#if HOMEPLUGPHY

struct sk_buff	homeplug_skb;
BYTE			homeplug_msg_data[128];

ADDR_TBL * Get_Free_Tx_Buffer( EMAC_DRV_CTRL *MACAdapter )
{
	homeplug_skb.data = &homeplug_msg_data[0];		
	return &homeplug_skb;
}

void MAC_Assign_MacAddress( EMAC_DRV_CTRL *MACAdapter, UINT8 *S_Addr )
{
	MOVE_MEMORY( S_Addr, &MACAdapter->PortNetAddress[0], 6 ) ;
}


void MAC_Send_A_Frame( EMAC_DRV_CTRL *MACAdapter, UINT32 PacketSize, ADDR_TBL *PtrTxBuffer, BOOLEAN Control_Frame )
{
	TX_PKT_INFO		tx_pkt_info;
	unsigned long	flags;
	DWORD				entry;

	// Generalize the data about the transmit packet
	PtrTxBuffer->len = PacketSize;

	tx_pkt_info.tx_pkt_data = PtrTxBuffer->data;
	tx_pkt_info.tx_pkt_len = PtrTxBuffer->len;
	tx_pkt_info.pkt_type = NORMAL;
	
	// Preprocess the transmit packet data for the emac driver
	if( EMAC_Tx_Pkt_Process( MACAdapter, &tx_pkt_info, &entry ) == TRUE )
	{
		// Disable interrupts
		spin_lock_irqsave(&MACAdapter->lock, flags); 

		EMAC_Queue_Tx_Pkt( MACAdapter->dev, (DWORD)entry );
		
		// Transmit the successfully processed packet now
		EMAC_Tx_Pkt_Transmit( MACAdapter->dev );
		
		// Reenable interrupts
		spin_unlock_irqrestore(&MACAdapter->lock, flags);
	}
	else
	{
	}
}
#endif

