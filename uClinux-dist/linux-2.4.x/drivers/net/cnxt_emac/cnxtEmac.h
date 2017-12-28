/****************************************************************************
*
*	Name:			cnxtEmac.h
*
*	Description:	Header file for cnxtEmac.c
*
*	Copyright:		(c) 2001,2002 Conexant Systems Inc.
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
*  $Revision:   1.8  $
*  $Modtime:   Jul 11 2003 07:36:44  $
****************************************************************************/

#ifndef __EMAC_END_H
#define __EMAC_END_H

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * 
 * includes 
 *
 */

#include  <linux/skbuff.h>

/* emac */
#define INC_PHY
#include "cnxtEmac_hw.h"      /* HW regs & related defines. */
#include "filters.h"       /* Hashing, Address filtering & setup frame. */
//#include "emac_type.h"
#include <linux/if_ether.h>

#include "phy.h"           /* Physical Layer Driver: types */
#include <asm/arch/OsTools.h>


/* PHY link status data structure and related #defines. */
typedef struct phy_setting
{
    unsigned long   autoNegotiate;      // 0: disable,  1: enable
    unsigned long   networkSpeed;       // 0: 10M,  1: 100M
    unsigned long   duplexMode;         // 0: HDX,  1: FDX
    unsigned long   linkStatus;         // 0: down, 1: up
    unsigned long   phySettings;        // returned by PhyCheck ()
} PHY_SETTING;

#define PHY_FORCEDMODE          0
#define PHY_AUTONEGOTIATE       1
#define PHY_STAT_SPEED_10       0
#define PHY_STAT_SPEED_100      1
#define PHY_STAT_DUPLEX_HALF    0
#define PHY_STAT_DUPLEX_FULL    1
#define PHY_STAT_LINK_DOWN      0
#define PHY_STAT_LINK_UP        1



#if 0
	#define	RX_RING_SIZE		64
#else
	#define	RX_RING_SIZE		80
#endif
#if 0
	#define	TX_RING_SIZE		64
#else
	#define	TX_RING_SIZE		80
#endif
#if 1
	#define ETH_PKT_BUF_SZ		1536
#else
	#define ETH_PKT_BUF_SZ		1600
#endif


/* Assign the appropriate number of RXD's based on the link speed. */
#define RXD_NUM  RX_RING_SIZE
/* Assign the mask number to allow wrap around in the RXD table. */
#define RXD_NUM_WRAP  (RXD_NUM-1)

#define SIZE_OF_DATA_QUEUES	(TX_RING_SIZE * 2)

typedef struct
{
	DWORD	Data1;
	DWORD	Data2;
	DWORD	Data3;

} DATA_REG_T;

typedef struct
{
	
	BYTE		get_idx;
	BYTE		put_idx;
	BYTE		entry_cnt;
	BYTE		fill;		// Dword align the structure
	DATA_REG_T	queue[SIZE_OF_DATA_QUEUES];
	
} DATA_QUEUE_T;


#define DRV_NAME		"mac"
#define DRV_NAME_LEN    5
#define EDRV_DESC		"Conexant CX82100 EMAC Enhanced Network Driver"

#define	NONE			0

/*******************************************************************************
 * Ethernet Standards:
 *
 * 1. Ethernet Frame fields - order left to right - are:
 *    preamble[7],sfd[1],da[6],sa[6],type(or len)[2],data[var],pad[var],fcs[4].
 *    Where, [x] = length in bytes. [var] = variable length.
 *
 *    da, sa & type(or len) put together is called ethernet header.
 *
 *    ISO802.3 has length field instead of Type field.
 *    But in both cases ethernet & ISO802.3, the length of the 3rd field
 *    remains 2 bytes. 
 *
 *    Ethernet address, Physical address & MAC address are synonymous.
 *
 *    ENET_HDR_REAL_SIZ: len of destination & source addr & type = 14 bytes.
 *    ETHERMTU: max length of data & pad fields = 1500 bytes.
 *    ETHERMIN: min length of data & pad fields (60-14) = 46 bytes.
 *
 * 2. MAC HW:
 *    Preamble & sfd are removed/added by Rx/Tx MAC HW before the frame
 *    is sent up/down to the protocols/cable. So as far as MAC SW (driver) goes, 
 *    the frame starts from DA and ends with FCS.
 */


#define MAC_ADDR_LEN          ( 6 ) 
           
/* return value for MAC_cmp */
#define MAC_GREATER           ( 1 )    
#define MAC_EQUAL             ( 0 )
#define MAC_LESS              ( -1 )

/* size of multicast address and hash tables */
#define MULTI_TABLE_SIZE      ( 256 )
#define HASH_TABLE_SIZE       ( 32 )

/* bit which defines whether a MAC Address is multi or uni cast */
#define MULTICAST_MSK         ( 0x01 )

/* hash table initialization values */
#define FILTER_ALL            ( 0x00 )
#define PASS_ALL              ( 0xFF )

typedef enum
{
	NORMAL = 0,
	SETUP

} TX_PKT_TYPE;


/****************************************************************************
 * Structures
 ****************************************************************************/

#define EMAC_SETUP_PERFECT_ENTRIES     16
typedef union _EMAC_SETUP_FRAME 
{
    struct 
    {
        unsigned long PhysicalAddress[EMAC_SETUP_PERFECT_ENTRIES][3];
    } Perfect;
} EMAC_SETUP_FRAME, *PEMAC_SETUP_FRAME;

#define ADDRESS_FILTER_SIZE     192

/* MAC Address */
typedef UINT8 MAC_ADDR[MAC_ADDR_LEN];

/* aligned MAC address */
typedef struct aligned_mac_addr
{
   UINT16      usLSW;      /* least significant word */
   UINT16      usCSW;      /* middle word */
   UINT16      usMSW;      /* most significant word */
} ALIGNED_MAC_ADDR;




#define EMAC_CLUSTER_SZ  2048

#define EADDR_LEN	        6       /* ethernet address length */

#define NET_SPEED_10	10000000             /* 10 Mbps		*/
#define NET_SPEED_100	100000000            /* 100 Mbps	*/

/* Setup Frame Type */

#define	MT_SETUP_FRAME	   ( NUM_MBLK_TYPES )   /* setup frame emac tx identifier */

/* Call these in endMCastAddrAdd[Del] funcs. */
#define DRV_END_DECR_MULTI(pDrvCtrl) ((pDrvCtrl)->endObj.nMulti--)
#define DRV_END_INCR_MULTI(pDrvCtrl) ((pDrvCtrl)->endObj.nMulti++)

/* 
 * Mem alignment macros 
 */

#define QWORD_ALIGN_PTR(x) \
	{  \
		register unsigned int* p; \
		p = (unsigned int*)(void*)&(x); \
		*p=((*p)&0x7)?(((*p)|7)+1):(*p); \
	}

#define	QWORD_ALIGN_PAD		8
#define QWORD_ALIGN_CHK		7
#define SIZEOF_QWORD		8
#define QWORD_ROUND_UP_FACTOR	7

/* alignment sizes: always power of 2*/
#define W_AL  2				   /* WORD alignment? */
#define DW_AL (sizeof (DWORD))    /* DWORD alignment */
#define QW_AL (2*sizeof (DWORD))  /* QWORD alignment */

/* Align qty to forward(next) al-boundry.*/
#define ALIGN_FW(qty,al)       (((int)(qty) + ((al)-1)) & ~((al)-1))

/*******************************************************************************
 * DMA Chain elements joining.
 */

/* Joins two dma chains. */
typedef struct _DMA_Chain_joint
{
	volatile BYTE* pNext; /* Pointer to next contiguous location */
	volatile ULONG cNext; /* # of QWORDS in next contiguous location */
} JOINT;

#define JOINT_SIZ	sizeof(JOINT)

/*******************************************************************************
 * Descriptors: definitions & defines.
 *
 * Note:
 * 1 The order in which the fields are listed is important.
 * 2 Both descriptors have status, a 64 bit qword, represented as two ULONGs.
 *   The order in which the two ULONGs are listed in the structure definition, 
 *   depends on the endianism: in our case little endian.
 */

/* Transmit Descriptor - hardware defined. */
typedef struct _emac_txd
{
    volatile ULONG   status;         /* status - see TSTAT_XXX in emac_desc.h                    */
    volatile ULONG   status_zero;    /* zeroes                                                   */
    volatile ULONG   control;        /* TDES -see TCTRL_XXX in emac_desc.h (incl. len in bytes)  */
} EMAC_TXD;

#define EMAC_TXD_SIZE	sizeof( EMAC_TXD )

/* Transmit Message Descriptor Entry. */
typedef struct tDesc
{
    volatile ULONG	status;	     /* status				*/
    volatile ULONG	status_zero; /* zeroes				*/
    volatile ULONG	control;     /* control parameters	*/
    ULONG   data;        		/* first ulong in the cluster */
    JOINT   joint;
} EMAC_TD;

#define TD_SIZ	        ( sizeof(EMAC_TD)/ SIZEOF_QWORD ) /* 3 qwords */
#define TD_DMA_SIZ	    (( TD_SIZ - JOINT_SIZ ) / SIZEOF_QWORD )


typedef struct
{
	BYTE			*tx_pkt_data;
	unsigned int	tx_pkt_len;
	TX_PKT_TYPE		pkt_type;

} TX_PKT_INFO;

/*
 * Tx Msg Queue 
 */

typedef struct tx_msg_queue
{
	char *      Data;
    ULONG		ulControl;
	ULONG		ulLength;

} TX_MSG_QUEUE;

/*********************************************************************
	This is a hardware structure found in the CX82100
	Design Specification. 
*********************************************************************/
typedef struct 
{
	volatile unsigned char*	mData;
    volatile unsigned long	reserved;
    volatile unsigned long	status;
    volatile unsigned long	mib;

}EMAC_RXD;


/*********************************************************************
	This structure contains the main Q information for tracking the 
    hardware packet information and which clusters are to be
    transferred to the MUX.  The ordering of the elements is 
    important because the DMA channel is expecting the first
    Q element of the EMAC_RXD to be qword aligned!
*********************************************************************/
typedef struct
{
	EMAC_RXD        q[RXD_NUM];				/* pointer to the beginning of the descriptor list */
	struct sk_buff*	skb[RXD_NUM];
	unsigned long   qndx;   				/* index to next available element when queue size is non-zero  */

} EMAC_RXD_HW_QTYPE;

typedef struct EMAC_drv_ctrl
{
	BYTE			*txd_buf_mem;			// Pointer to the original non-qword aligned
											// memory block obtained from kmalloc

	BYTE			*txd_buf[TX_RING_SIZE];	// Array of pointers to ethernet sized blocks
											// of memory from the memory pointed to by
											// txd_buf_mem
	TX_MSG_QUEUE   	tx_msg_q [TX_RING_SIZE];
	DATA_QUEUE_T	*emac_tx_q;

   	EMAC_TD*		TxD_end;

	UINT32   		tx_msg_q_next;   /* index to next available element when queue is not full */
	UINT32   		tx_msg_q_current;  /* current transmitting TxD                               */
	UINT32   		tx_msg_q_size;    /* if size is zero, then queue is empty                   */
	BOOLEAN			tx_pkt_in_progress;
	BOOLEAN			tx_mutex_part;
	BOOLEAN			hw_reset_required;
	EVENT_HNDL		*hw_reset_event;

	BYTE			setup_frame_buf[256];

	/* Tx control lock.  This protects the transmit buffer ring
	 * state along with the "tx full" state of the driver.  This
	 * means all netif_queue flow control actions are protected
	 * by this lock as well.
	 */
	spinlock_t		lock;

	struct net_device_stats	stats;
	struct net_device_stats	prev_stats;

	// Receiver hardware queue
	EMAC_RXD_HW_QTYPE	*rxd_hw;
	BYTE				*rxd_buf;
	DATA_QUEUE_T		EMAC_rx_buf_q;

	struct timer_list	wd_timer;
	struct timer_list	periodic_task;

   	/* Multicast Address Table is array */
    int         multiCount;                     /* Number of multicast address in Table */
    MAC_ADDR    multiTable[MULTI_TABLE_SIZE];   /* Multicast address table */
    UINT8       hashTable[HASH_TABLE_SIZE];     /* Multicast hash table */

    int			flags;               /* driver flags					*/

	BOOLEAN		emac_dev_q_state;	// TRUE = STARTED,  FALSE = STOPPED
	BOOLEAN		emac_state;			// TRUE = OPEN,  FALSE = CLOSED
	DWORD		emac_num;			// emac_num = unit + 1 -> emac1 = eth0 and emac2 = eth1
	DWORD		emac_hdw_num;		// emac_hdw_num = 0 -> emac1 and 1 -> emac2
    int			unit;                /* unit number	*/

	UINT8		PortNetAddress[MAC_ADDR_LEN];
	   
	volatile unsigned int	E_NA_settings;
	volatile unsigned int	E_IE_settings;

	int						tx_dma_int;
	int						rx_dma_int;

	volatile unsigned long	int_stat_clr_dev_irq;
	volatile unsigned long	int_stat_clr_tx;
	volatile unsigned long	int_stat_clr_rx;

	volatile unsigned long*	emac_tx_dma_ptr;
	volatile unsigned long*	emac_tx_dma_ptrx;
	volatile unsigned long* emac_rx_dma_ptr2;
	volatile unsigned long* emac_tx_dma_cnt;
	volatile unsigned long* emac_rx_dma_cnt1;
	volatile unsigned long* emac_rx_dma_cnt2;
	volatile unsigned long* emac_na_reg;
	volatile unsigned long* emac_stat_reg;
	volatile unsigned long* emac_lp_reg;
	volatile unsigned long* emac_ie_reg;

	ULONG	MacFilteringMode;

#ifdef INC_PHY

/*
 *  Physical Layer Driver struct-type.
 */

    PHY_SETTING     phyState;
	
/* # of Phys connected to the MAC via the MII bus.*/
#ifndef MAX_SUPPORTED_PHYS
#define MAX_SUPPORTED_PHYS	2 /* Must be < 31 */
#endif

    PHY		*pPhy;
#endif /* INC_PHY */
	void	*pHpna ;	//PHPNALLOBJ pHpna ;

#if HOMEPLUGPHY
	struct net_device *dev;
		
	#ifdef HOMEPLUG_PHYMAC_OVERFLOW_WORKAROUND
	struct semaphore	tx_delay_sem_id;
	int		Current_packet_Collision_cnt ;
	#endif
#endif


} EMAC_DRV_CTRL;

#define DRV_CTRL_SIZ	sizeof(DRV_CTRL)
#define	DRV_CTRL	EMAC_DRV_CTRL

/*******************************************************************************
 * EMAC specific ethernet frame size & cluster size requirements.
 */

#define MIN_ETHERNET_SIZE 18        /* used in end send */
#define MAX_ETHERNET_SIZE 1518
#define ETHERNET_CRC_SIZE	  4
#define CL_SIZ             2048

#define	NET_SPEED_DEF	NET_SPEED_10


/*
 * END specific defines
 */

#define END_FLAGS_ISSET(pEnd, setBits)                                  \
        ((pEnd)->flags & (setBits))

#define END_HADDR(pEnd)                                                 \
		((pEnd)->mib2Tbl.ifPhysAddress.phyAddress)

#define END_HADDR_LEN(pEnd)                                             \
		((pEnd)->mib2Tbl.ifPhysAddress.addrLength)

#define	END_MIB_SPEED_SET(pEndObj, speed)                               \
	    ((pEndObj)->mib2Tbl.ifSpeed=speed)


/*
 * DRV_CTRL flags & access macros 
 */

#define DRV_MEMOWN		0x0001	/* TDs and RDs allocated by driver	*/
#define EMAC_POLLING	0x0004	/* Poll mode, io mode				*/
#define FLT_PROMISC		0x0008	/* Promiscuous, rx mode				*/
#define FLT_MCAST		0x0010	/* Multicast, rx mode				*/

#define DRV_FLAGS_SET(setBits)                                          \
	    (pDrvCtrl->flags |= (setBits))

#define DRV_FLAGS_ISSET(setBits)                                        \
	    (pDrvCtrl->flags & (setBits))

#define DRV_FLAGS_CLR(clrBits)                                          \
	    (pDrvCtrl->flags &= ~(clrBits))

#define DRV_FLAGS_GET()                                                 \
        (pDrvCtrl->flags)

        
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

#define E_NA_FILTER_MODE        ( E_NA_PM | E_NA_PR | E_NA_IF | E_NA_HO | E_NA_HP )
#define E_NA_FILTER_PROMIS      ( E_NA_PM | E_NA_PR )
#define E_NA_FILTER_MC1PERFECT  ( E_NA_PM | E_NA_HP )
#define E_NA_FILTER_INVERSE	    ( E_NA_IF )

/*
 * END Specific interfaces. 
 */

#define INT_DMA_EMAC_1_TX   ((unsigned long)1<<15)
#define INT_DMA_EMAC_1_RX   ((unsigned long)1<<14)
#define INT_DMA_EMAC_2_TX   ((unsigned long)1<<13)
#define INT_DMA_EMAC_2_RX   ((unsigned long)1<<12)

/**********************************************************/    
/* Global shadow register for the E_NA register           */
/**********************************************************/    


#define E_S_TU    ((unsigned long)1<<31)
#define E_S_RO    ((unsigned long)1<<26)
#define E_S_RWT   ((unsigned long)1<<25)
#define E_S_TOF   ((unsigned long)1<<16)
#define E_S_TUF   ((unsigned long)1<<15)
#define E_S_ED    ((unsigned long)1<<14)
#define E_S_DF    ((unsigned long)1<<13)
#define E_S_CD    ((unsigned long)1<<12)
#define E_S_ES    ((unsigned long)1<<11)
#define E_S_RLD   ((unsigned long)1<<10)
#define E_S_TF    ((unsigned long)1<<9)
#define E_S_TJT   ((unsigned long)1<<8)
#define E_S_NCRS  ((unsigned long)1<<7)
#define E_S_LCRS  ((unsigned long)1<<6)
#define E_S_16    ((unsigned long)1<<5)
#define E_S_LC    ((unsigned long)1<<4)
#define E_S_CC    ((unsigned long)0xf<<0)

#define BF_CRC 	   0x0ff00000
#define BF_ALN     0x000f0000
#define BF_LONG    0x0000f000
#define BF_RUNT    0x00000ff0 
#define BF_OFLW    0x0000000f
#define BF_MASK    (BF_CRC+BF_ALN+BF_LONG+BF_RUNT+BF_OFLW)

#define GF_TS      0x00000800
#define GF_MF      0x00000400
#define GF_TL      0x00000080
#define GF_LC      0x00000040
#define GF_OFT     0x00000020
#define GF_RW      0x00000010
#define GF_DB      0x00000004
#define GF_CE      0x00000002
#define GF_OF      0x00000001
#define GF_MASK    (GF_TS+GF_MF+GF_TL+GF_LC+GF_OFT+GF_RW+GF_DB+GF_CE+GF_OF)
#define RX_ES		0x00008000
#define RX_A1		0x00000008


#if HOMEPLUGPHY
typedef	struct sk_buff ADDR_TBL ;

#define Packet_Buffer_Virtual_Adr( _pBuffer )	((ADDR_TBL *)(_pBuffer))->data
ADDR_TBL * Get_Free_Tx_Buffer( EMAC_DRV_CTRL *MACAdapter );

void MAC_Assign_MacAddress( EMAC_DRV_CTRL *MACAdapter, BYTE *S_Addr );
void MAC_Send_A_Frame( EMAC_DRV_CTRL *MACAdapter, DWORD PacketSize, ADDR_TBL *PtrTxBuffer, BOOLEAN Control_Frame );
#endif

#ifdef __cplusplus
}
#endif

#endif /* __EMAC_END_H */
