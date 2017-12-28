/*
 * TouCAN - definitions for the Motorola TouCAN module
 *
 * Copyright (c) 1998,2000 port GmbH Halle (Saale)
 *------------------------------------------------------------------
 * $Header: /z2/cvsroot/common_include/TouCAN.h,v 1.3 2001/04/19 15:06:30 ro Exp $
 *
 *--------------------------------------------------------------------------
 *
 * 
 *
 * modification history
 * --------------------
 * $Log: TouCAN.h,v $
 * Revision 1.3  2001/04/19 15:06:30  ro
 * adapt to CANopenLIB V4.2
 * macros for user adaptation moved at the top of the file
 * Set_Baudrate added
 *
 * Revision 1.2  2000/02/10 13:28:34  boe
 * Vereinbarungen für Motorola TPU CAN-chip
 *
 *
 *
 *--------------------------------------------------------------------------
 */


/*
DESCRIPTION
Version $Revision: 1.3 $
.sp
*/

#ifndef __TOU_CAN_H
#define __TOU_CAN_H

/* Makros to manipulate TouCAN control registers */
#define CAN_READ(reg)			(reg)

#define CAN_WRITE(reg, source)		((reg) = (source))

#define CAN_SET_BIT(reg, mask)		((reg) |= (mask))

#define CAN_RESET_BIT(reg, mask)	((reg) &= ~(mask))

#define CAN_TEST_BIT(breg, mask)	((breg) & (mask))


/*=== Register Layout of the TouCAN module ==============================*/
typedef struct {
    volatile unsigned short     canmcr;
    volatile unsigned short     cantcr;
    volatile unsigned short     canicr;
    volatile unsigned char      canctrl0;
    volatile unsigned char      canctrl1;
    volatile unsigned char      presdiv;
    volatile unsigned char      canctrl2;
    volatile unsigned short     timer;
    volatile unsigned short     _08C, _08E;
    volatile unsigned short     rxgmskhi;
    volatile unsigned short     rxgmsklo;
    volatile unsigned short     rx14mskhi;
    volatile unsigned short     rx14msklo;
    volatile unsigned short     rx15mskhi;
    volatile unsigned short     rx15msklo;
    volatile unsigned short     _09C, _09E;
    volatile unsigned short     estat;
    volatile unsigned short     imask;
    volatile unsigned short     iflag;
    volatile unsigned char      rxectr;
    volatile unsigned char      txectr;
} tou_can_t;

/* FlexCAN mcf5282 located at MCF_MBAR + 0x1C'0000 */
typedef struct {
    volatile unsigned short     canmcr;		/* module config register */
    volatile unsigned short     reserved00;

    volatile unsigned short     reserved06;
    volatile unsigned char      canctrl0;	/* control register 0     */
    volatile unsigned char      canctrl1;	/* control register 1     */
    
    volatile unsigned char      presdiv;	/* prescaler divider      */
    volatile unsigned char      canctrl2;	/* control register 2     */
    volatile unsigned short     timer;

    volatile unsigned short     reserved0E;
    volatile unsigned short     reserved0C;

#if 0
    volatile unsigned int       rxgmsk;		/* RX global mask         */
    volatile unsigned int       rx14msk;	/* RX Buffer 14 Mask      */
    volatile unsigned int       rx15msk;	/* RX Buffer 15 Mask      */
#else
/* according errata sheet:
errata id 8:   FlexCAN
07/23/03   32-bit accesses to FlexCAN registers do not work properly
*/

    volatile unsigned short     rxgmskhi;
    volatile unsigned short     rxgmsklo;
    volatile unsigned short     rx14mskhi;
    volatile unsigned short     rx14msklo;
    volatile unsigned short     rx15mskhi;
    volatile unsigned short     rx15msklo;
#endif
    volatile unsigned int       reserverd1C;
    
    volatile unsigned short     estat;		/* Error and status ESTAT */
    volatile unsigned short     imask;		/* Interrupt masks IMASK  */
    
    volatile unsigned short     iflag;		/* Interrupt flags IFLAG  */
    volatile unsigned char      rxectr;		/* Rx error counter RXECTR*/
    volatile unsigned char      txectr;		/* Tx error counter TXECTR*/
} flex_can_t;




/* To have two different CAN structures here, TouCan and FlexCan
 * to be used with -
 * - CANopen library without OS we use a global pointer tou_can
 * - can4linux we use the generic name canregs_t
 *   /this name is used also in the sja1000 driver)
 */
# ifdef CONFIG_MULT_LINES
#   ifdef MCF5282
    extern flex_can_t *tou_can_addr[];
#   else 
    extern tou_can_t *tou_can_addr[];
#   endif
# else 
#   ifdef MCF5282
    extern flex_can_t *tou_can;
    typedef flex_can_t canregs_t;
#   else 
    extern tou_can_t *tou_can;
    typedef tou_can_t canregs_t;
#   endif
# endif



#define CAN_ControlReg0			(tou_can->canctrl0)
#define CAN_ControlReg1			(tou_can->canctrl1)
#define CAN_ControlReg2			(tou_can->canctrl2)

#define CAN_TestConfigRegister		(tou_can->cantcr)
#define CAN_ModulConfigRegister		(tou_can->canmcr)
#define CAN_InterruptRegister		(tou_can->canicr)
#define CAN_PrescalerDividerRegister	(tou_can->presdiv)
#define CAN_TimerRegister		(tou_can->timer)
#define CAN_ErrorStatusRegister		(tou_can->estat)
#define CAN_InterruptMasks		(tou_can->imask)
#define CAN_InterruptFlags		(tou_can->iflag)
#define CAN_ReceiveErrorCounter		(tou_can->rxectr)
#define CAN_TransmitErrorCounter	(tou_can->txectr)
/* or TouCAN with word access */
#define CAN_ReceiveGlobalMaskHigh	(tou_can->rxgmskhi)
#define CAN_ReceiveGlobalMaskLow	(tou_can->rxgmsklo)
#define CAN_ReceiveBuffer14MaskHigh	(tou_can->rx14mskhi)
#define CAN_ReceiveBuffer14MaskLow	(tou_can->rx14msklo)
#define CAN_ReceiveBuffer15MaskHigh	(tou_can->rx15mskhi)
#define CAN_ReceiveBuffer15MaskLow	(tou_can->rx15msklo)
/* some more for CF FlexCAN with 32bit access */
#define CAN_ReceiveGlobalMask    	(tou_can->rxgmsk)
#define CAN_ReceiveBuffer14Mask		(tou_can->rx14msk)
#define CAN_ReceiveBuffer15Mask		(tou_can->rx15msk)


/* Bit values of the TouCAN module configuration register */

#define CAN_MCR_STOP			0x8000
#define CAN_MCR_FRZ			0x4000
#define CAN_MCR_HALT			0x1000
#define CAN_MCR_NOT_RDY			0x0800
#define CAN_MCR_WAKE_MSK		0x0400
#define CAN_MCR_SOFT_RST		0x0200
#define CAN_MCR_FRZ_ACK			0x0100
#define CAN_MCR_SUPV			0x0080
#define CAN_MCR_SELF_WAKE		0x0040
#define CAN_MCR_APS			0x0020
#define CAN_MCR_STOP_ACK		0x0010
#define CAN_MCR_IARB_MASK		0x000F
#define CAN_MCR_IARB3			0x0008
#define CAN_MCR_IARB2			0x0004
#define CAN_MCR_IARB1			0x0002
#define CAN_MCR_IARB0			0x0001

/* Bits of the TouCAN control register 0 */

#define CAN_CTRL0_BOFF_MSK_BIT		7
#define CAN_CTRL0_ERR_MSK_BIT		6
#ifdef MCF5282
/* only one bit for RX configuartion */
#else
#define CAN_CTRL0_RXMODE1_BIT		3
#endif
#define CAN_CTRL0_RXMODE0_BIT		2
#define CAN_CTRL0_TXMODE1_BIT		1
#define CAN_CTRL0_TXMODE0_BIT		0

/* Bit values of the TouCAN control register 0 */

#define CAN_CTRL0_BOFF_MSK		0x80
#define CAN_CTRL0_ENABLE_BOFF_INT	0x80
#define CAN_CTRL0_DISABLE_BOFF_INT	0x00
#define CAN_CTRL0_ERR_MSK		0x40
#define CAN_CTRL0_ENABLE_ERR_INT	0x40
#define CAN_CTRL0_DISABLE_ERR_INT	0x00
#ifdef MCF5282
/* only one bit for RX configuartion */
#else
#define CAN_CTRL0_RXMODE1		0x08
#endif
#define CAN_CTRL0_RXMODE0		0x04
#define CAN_CTRL0_TXMODE1		0x02
#define CAN_CTRL0_TXMODE0		0x01


/* Bits of the TouCAN control register 1 */

#define CAN_CTRL1_SAMP_BIT		7
#define CAN_CTRL1_LOOP_BIT		6
#define CAN_CTRL1_TSYNC_BIT		5
#define CAN_CTRL1_LBUF_BIT		4
#define CAN_CTRL1_PROPSEG2_BIT		2
#define CAN_CTRL1_PROPSEG1_BIT		1
#define CAN_CTRL1_PROPSEG0_BIT		0

/* Bit values of the TouCAN control register 1 */

#define CAN_CTRL1_SAMP			0x80
#define CAN_CTRL1_LOOP			0x40
#define CAN_CTRL1_TSYNC			0x20
#define CAN_CTRL1_LBUF			0x10
#define CAN_CTRL1_PROPSEG2		0x04
#define CAN_CTRL1_PROPSEG1		0x02
#define CAN_CTRL1_PROPSEG0		0x01


/* Bits of the TouCAN control register 2 */

#define CAN_CTRL2_RJW1_BIT		7
#define CAN_CTRL2_RJW0_BIT		6
#define CAN_CTRL2_PSEG1_2_BIT		5
#define CAN_CTRL2_PSEG1_1_BIT		4
#define CAN_CTRL2_PSEG1_0_BIT		3
#define CAN_CTRL2_PSEG2_2_BIT		2
#define CAN_CTRL2_PSEG2_1_BIT		1
#define CAN_CTRL2_PSEG2_0_BIT		0

/* Bit values of the TouCAN control register 2 */

#define CAN_CTRL2_RJW1			0x80
#define CAN_CTRL2_RJW0			0x40
#define CAN_CTRL2_PSEG1_2		0x20
#define CAN_CTRL2_PSEG1_1		0x10
#define CAN_CTRL2_PSEG1_0		0x08
#define CAN_CTRL2_PSEG2_2		0x04
#define CAN_CTRL2_PSEG2_1		0x02
#define CAN_CTRL2_PSEG2_0		0x01


/* Bits of the TouCAN error and status register */

#define CAN_ESTAT_BITERR1_BIT		15
#define CAN_ESTAT_BITERR0_BIT		14
#define CAN_ESTAT_ACK_ERR_BIT		13
#define CAN_ESTAT_CRC_ERR_BIT		12
#define CAN_ESTAT_FORM_ERR_BIT		11
#define CAN_ESTAT_STUFF_ERR_BIT		10
#define CAN_ESTAT_TX_WARN_BIT		9
#define CAN_ESTAT_RX_WARN_BIT		8
#define CAN_ESTAT_IDLE_BIT		7
#define CAN_ESTAT_TX_RX_BIT		6
#define CAN_ESTAT_FCS1_BIT		5
#define CAN_ESTAT_FCS0_BIT		4
#define CAN_ESTAT_BOFF_INT_BIT		2
#define CAN_ESTAT_ERR_INT_BIT		1
#define CAN_ESTAT_WAKE_INT_BIT		0

/* Bit values of the TouCAN error and status register */

#define CAN_ESTAT_BITERR1		0x8000
#define CAN_ESTAT_BITERR0		0x4000
#define CAN_ESTAT_ACK_ERR		0x2000
#define CAN_ESTAT_CRC_ERR		0x1000
#define CAN_ESTAT_FORM_ERR		0x0800
#define CAN_ESTAT_STUFF_ERR		0x0400
#define CAN_ESTAT_TX_WARN		0x0200
#define CAN_ESTAT_RX_WARN		0x0100
#define CAN_ESTAT_IDLE			0x0080
#define CAN_ESTAT_TX_RX			0x0040
#define CAN_ESTAT_FCS 			0x0030
#define CAN_ESTAT_FCS0			0x0010
#define CAN_ESTAT_FCS1			0x0020
#define CAN_ESTAT_FCS0			0x0010
#define CAN_ESTAT_BOFF_INT		0x0004
#define CAN_ESTAT_ERR_INT		0x0002
#define CAN_ESTAT_WAKE_INT		0x0001

/*
 * Macros to handle CAN objects
 *
 * Structure for a single CAN object
 * A total of 15 such object structures exists (starting at CAN_BASE + 0x80)
 */
struct can_obj {
  unsigned short int ctl_status;/* 8-bit time stamp + code + length	*/
  union {
    unsigned short int std_id;	/* Standard Identifier			*/
    unsigned short int id_high;	/* ID high + SRR + IDE			*/
    } id_high;
  union {
    unsigned short int timeStamp; /* 2 byte time stamp counter		*/
    unsigned short int id_low;	/* ID low + RTR				*/
    } id_low;
  unsigned char msg[8];      	/* Message Data 0 .. 7   		*/
  unsigned char reserved0;	/* Reserved Byte         		*/
  unsigned char reserved1;	/* Reserved Byte         		*/
};

/* The firs data byte ov a message */
#define MSG0 msg[0]

#define STDID_RTR_BIT		0x10
#define EXTID_RTR_BIT		0x01

/* CAN object definition */
#define CAN_OBJ \
   ((struct can_obj volatile *) (((void *)can_base[board]) + 0x80))

/*
 * the above definition can be used as follows:
 * CAN_OBJ[Msg].ctl_status = 0x5595;
 * val = CAN_OBJ[Msg].ctl_status;
 * with Msg 0....15
 */

/* ---------------------------------------------------------------------------
 * CAN_READ_OID(obj) is a macro to read the CAN-ID of the specified object.
 * It delivers the value as 16 bit from the standard ID registers.
 */

#define CAN_READ_OID(bChannel) (CAN_OBJ[bChannel].id_high.std_id >> 5)
#define CAN_READ_OID_AND_RTR(bChannel) (CAN_OBJ[bChannel].id_high.std_id)


/* ---------------------------------------------------------------------------
 * CAN_WRITE_OID(obj, id) is a macro to write the CAN-ID
 * of the specified object with identifier id.
 * CAN_WRITE_XOID(obj, id) is a macro to write the extended CAN-ID
 */
#define CAN_WRITE_OID(bChannel, Id) (CAN_OBJ[bChannel].id_high.std_id = (Id) << 5)

#define CAN_WRITE_XOID(bChannel, Id)  \
	do { (CAN_OBJ[bChannel].id_high.id_high \
	      = 0x18 + (((Id) & 0x1ffC0000) >> 13) + (((Id)  >> 15) & 0x7)); \
	CAN_OBJ[bChannel].id_low.id_low = (Id /* & 0x7fff */ << 1); \
	} while(0);

/* ---------------------------------------------------------------------------
 * CAN_WRITE_OID_RTR(obj, id) is a macro to write the CAN-ID
 * of the specified object with identifier id and set the RTR Bit.
 */
#define CAN_WRITE_OID_RTR(bChannel, Id) \
	(CAN_OBJ[bChannel].id_high.std_id = ((Id) << 5) + STDID_RTR_BIT)

#define CAN_WRITE_XOID_RTR(bChannel, Id)  \
	do { (CAN_OBJ[bChannel].id_high.id_high \
	      = 0x18 + (((Id) & 0x1ffC0000) >> 13) + (((Id)  >> 15) & 0x7)); \
	CAN_OBJ[bChannel].id_low.id_low = (Id /* & 0x7fff */ << 1) + EXTID_RTR_BIT; \
	} while(0);

/* ---------------------------------------------------------------------------
 * CAN_WRITE_CTRL(obj, code, length) is a macro to write to the 
 * specified objects control register
 *
 * Writes 2 byte, TIME STAMP is overwritten with 0.
 */
#define CAN_WRITE_CTRL(bChannel, code, length) \
	(CAN_OBJ[bChannel].ctl_status = (code << 4) + (length))

/* ---------------------------------------------------------------------------
 * CAN_READ_CTRL(obj) is a macro to read the CAN-Ctrl register
 *
 * Read 2 byte
 */
/* #define CAN_READ_CTRL(bChannel) (CAN_OBJ[bChannel].ctl_status >> 4) */
#define CAN_READ_CTRL(bChannel) (CAN_OBJ[bChannel].ctl_status)


/***** receive message object codes *************************************/
/* Message buffer is not active */
#define REC_CODE_NOT_ACTIVE	0
/* Message buffer is active and empty */
#define REC_CODE_EMPTY		4
/* Message buffer is full */
#define REC_CODE_FULL		2
/* second frame was received into a full buffer before CPU read the first */
#define REC_CODE_OVERRUN	6
/* message buffer is now being filled with a new receive frame.
 * This condition will be cleared within 20 cycles.
 */
#define REC_CODE_BUSY		1

/***** transmit message object codes ************************************/
/* Message buffer not ready for transmit */
#define TRANS_CODE_NOT_READY		8
/* Data frame to be transmitted once, unconditionally */
#define TRANS_CODE_TRANSMIT_ONCE	12
/* Remote frame to be transmitted once, and message buffer becomes
 * an RX message buffer for dadat frames;
 * RTR bit must be set
 */
#define TRANS_CODE_TRANSMIT_RTR_ONCE	12
/* Data frame to be transmitted only as a response to a remote frame, always */
#define TRANS_CODE_TRANSMIT_ONLY_RTR	10
/* Data frame to be transmitted once, unconditionally
 * and then only as a response to remote frame always
 */
#define TRANS_CODE_TRANSMIT_ONCE_RTR_ALWAYS	14




/***** message object configuration register ****************************/
#define CAN_MSG_Dir		    0x08
#define CAN_MSG_Xtd		    0x04

/* Definitions for Data direction */
#define CAN_Dir_TRANSMIT 			1
#define CAN_Dir_RECEIVE 			0

/***** B I T  --  T I M I N G  ******************************************/

/* Bit Timing Register fuer 500 kBit/s   -- 2µs Bit time
 |       3       |       4       ||       0       |       1       |
 | 0 | 0 | 1 | 1 | 0 | 1 | 0 | 0 || 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
 | 0 |   TSEG2   |     TSEG1     ||  SJW  |          BRP          |
		BTR1             ||             BTR0
 1. possible :
 CLP = Clock Period ( = 0.1 us at 10 MHz)    82527: CLP == SCLK
 1 Tq (Time quantum) = (BRP + 1) * CLP  
 BRP = Baud Rate Prescaler = 1 (=> Tq = 0.2 us)
 SJW = Sync Jump Width = 1 (= SJW + 1 = 0 + 1)
 TSEG1 = Time Segment before Sampling Point = 4
 TSEG2 = Time Segment after Sampling Point = 3
 Daraus folgt:
 Anzahl der Bit Time Segmente (must be  10 )
    n = 1 + TSEG1+1 + TSEG2+1 = 10
   --> Sampling Point bei 60 %
 Baudrate = 10 MHz / ( n * (BRP + 1)) = 500000 Bit/s
   --> Abweichung 0.0 %

 2. CANopen DS301 recommends the sampling point at 1.75 µs:
 CLP = Clock Period ( = 0.1 us at 10 MHz)    82527: CLP == SCLK
 1 Tq (Time quantum) = (BRP + 1) * CLP
 BRP = Baud Rate Prescaler = 0 (=> Tq = 0.1 us)
 SJW = Sync Jump Width = 1 (= SJW + 1 = 0 + 1)
 TSEG1 = Time Segment before Sampling Point = 15
 TSEG2 = Time Segment after Sampling Point = 2
 Daraus folgt:
 Anzahl der Bit Time Segmente (must be  20 )
    n = 1 + TSEG1+1 + TSEG2+1 = 20
   --> Sampling Point bei 1.7 µs == 85 %
 Baudrate = 10 MHz / ( n * (BRP + 1)) = 500000 Bit/s
   --> Abweichung 0.0 %

 With the 82527 additional  divider options must be set in the
 CPU Interface Register (CAN_CPU_IF_Reg) : Bits DMC and DSC
 DSC - Divide System Clock 
     SCKL = XTAL/(1 + DSCbit) ---- SCLK must be <= 10 MHz
 DMC - Divide Memory Clock 
*/

/*-- !!! The TouCAN need a special baud rate table with more ----
 *-- !!! parameters than many other                          ----
 *-- !!! If you use Set_Baudrate you must cast the table pointer 
 */
 
typedef struct {
	UNSIGNED16 rate;
	UNSIGNED8  presdiv;
	UNSIGNED8  propseg;
	UNSIGNED8  pseg1;
	UNSIGNED8  pseg2;
} BTR_TAB_TOUCAN_T;

#ifndef CAN_SYSCLK
#  error Please specify an CAN_SYSCLK value
#endif

/* generated bit rate table by http://www.port.de/engl/canprod/sv_req_form.html */
/* bitrate table for 10 MHz */
#if CAN_SYSCLK == 10
    #define CAN_SJW			   0

    #define CAN_PRESDIV_10K		0x63
    #define CAN_PROPSEG_10K		0x07
    #define CAN_PSEG1_10K		0x07
    #define CAN_PSEG2_10K		0x02

    #define CAN_PRESDIV_20K		0x31
    #define CAN_PROPSEG_20K		0x07
    #define CAN_PSEG1_20K		0x07
    #define CAN_PSEG2_20K		0x02

    #define CAN_PRESDIV_50K		0x13
    #define CAN_PROPSEG_50K		0x07
    #define CAN_PSEG1_50K		0x07
    #define CAN_PSEG2_50K		0x02

    #define CAN_PRESDIV_100K		0x09
    #define CAN_PROPSEG_100K		0x07
    #define CAN_PSEG1_100K		0x07
    #define CAN_PSEG2_100K		0x02

    #define CAN_PRESDIV_125K		0x07
    #define CAN_PROPSEG_125K		0x07
    #define CAN_PSEG1_125K		0x07
    #define CAN_PSEG2_125K		0x02
    
    #define CAN_PRESDIV_250K		0x03
    #define CAN_PROPSEG_250K		0x07
    #define CAN_PSEG1_250K		0x07
    #define CAN_PSEG2_250K		0x02

    #define CAN_PRESDIV_500K		0x01
    #define CAN_PROPSEG_500K		0x07
    #define CAN_PSEG1_500K		0x07
    #define CAN_PSEG2_500K		0x02

    /* bad samplepoint !!! */ 	
    #define CAN_PRESDIV_800K		0x00
    #define CAN_PROPSEG_800K		0x07
    #define CAN_PSEG1_800K		0x07
    #define CAN_PSEG2_800K		0x07
    
    #define CAN_PRESDIV_1000K		0x00
    #define CAN_PROPSEG_1000K		0x07
    #define CAN_PSEG1_1000K		0x07
    #define CAN_PSEG2_1000K		0x02
    
    #define CAN_SYSCLK_is_ok		1
#endif

/* wdh bitrate table for 20 MHz */
/* using same as for 10 MHz except for PRESDIV */
#if CAN_SYSCLK == 20
    #define CAN_SJW			   0

    #define CAN_PRESDIV_10K		(0x63*2+1)
    #define CAN_PROPSEG_10K		0x07
    #define CAN_PSEG1_10K		0x07
    #define CAN_PSEG2_10K		0x02

    #define CAN_PRESDIV_20K		(0x31*2+1)
    #define CAN_PROPSEG_20K		0x07
    #define CAN_PSEG1_20K		0x07
    #define CAN_PSEG2_20K		0x02

    #define CAN_PRESDIV_50K		(0x13*2+1)
    #define CAN_PROPSEG_50K		0x07
    #define CAN_PSEG1_50K		0x07
    #define CAN_PSEG2_50K		0x02

    #define CAN_PRESDIV_100K		(0x09*2+1)
    #define CAN_PROPSEG_100K		0x07
    #define CAN_PSEG1_100K		0x07
    #define CAN_PSEG2_100K		0x02

    #define CAN_PRESDIV_125K		(0x07*2+1)
    #define CAN_PROPSEG_125K		0x07
    #define CAN_PSEG1_125K		0x07
    #define CAN_PSEG2_125K		0x02
    
    #define CAN_PRESDIV_250K		(0x03*2+1)
    #define CAN_PROPSEG_250K		0x07
    #define CAN_PSEG1_250K		0x07
    #define CAN_PSEG2_250K		0x02

    #define CAN_PRESDIV_500K		(0x01*2+1)
    #define CAN_PROPSEG_500K		0x07
    #define CAN_PSEG1_500K		0x07
    #define CAN_PSEG2_500K		0x02

    /* bad samplepoint !!! */ 	
    #define CAN_PRESDIV_800K		(0x00*2+1)
    #define CAN_PROPSEG_800K		0x07
    #define CAN_PSEG1_800K		0x07
    #define CAN_PSEG2_800K		0x07
    
    #define CAN_PRESDIV_1000K		(0x00*2+1)
    #define CAN_PROPSEG_1000K		0x07
    #define CAN_PSEG1_1000K		0x07
    #define CAN_PSEG2_1000K		0x02
    
    #define CAN_SYSCLK_is_ok		1
#endif

/*
 * with Motorola TouCAN, 32 MHz, sample point at 87.5% 
 * used with ColdFire M5282EVB
 */ 
#if CAN_SYSCLK == 32
    #define CAN_SJW			   0

    #define CAN_PRESDIV_10K		(199)
    #define CAN_PROPSEG_10K		0x04
    #define CAN_PSEG1_10K		0x07
    #define CAN_PSEG2_10K		0x01

    #define CAN_PRESDIV_20K		(99)
    #define CAN_PROPSEG_20K		0x04
    #define CAN_PSEG1_20K		0x07
    #define CAN_PSEG2_20K		0x01

    #define CAN_PRESDIV_50K		(39)
    #define CAN_PROPSEG_50K		0x04
    #define CAN_PSEG1_50K		0x07
    #define CAN_PSEG2_50K		0x01

    #define CAN_PRESDIV_100K		(19)
    #define CAN_PROPSEG_100K		0x04
    #define CAN_PSEG1_100K		0x07
    #define CAN_PSEG2_100K		0x01

    #define CAN_PRESDIV_125K		(15)
    #define CAN_PROPSEG_125K		0x04
    #define CAN_PSEG1_125K		0x07
    #define CAN_PSEG2_125K		0x01
    
    #define CAN_PRESDIV_250K		(7)
    #define CAN_PROPSEG_250K		0x04
    #define CAN_PSEG1_250K		0x07
    #define CAN_PSEG2_250K		0x01

    #define CAN_PRESDIV_500K		(3)
    #define CAN_PROPSEG_500K		0x04
    #define CAN_PSEG1_500K		0x07
    #define CAN_PSEG2_500K		0x01

    #define CAN_PRESDIV_800K		(1)
    #define CAN_PROPSEG_800K		0x07
    #define CAN_PSEG1_800K		0x07
    #define CAN_PSEG2_800K		0x02
    
    #define CAN_PRESDIV_1000K		(1)
    #define CAN_PROPSEG_1000K		0x04
    #define CAN_PSEG1_1000K		0x07
    #define CAN_PSEG2_1000K		0x01
    
    #define CAN_SYSCLK_is_ok		1
#endif

/* wdh: bitrate table for 40 MHz */
/* using same as for 10 MHz except for PRESDIV */
#if CAN_SYSCLK == 40
    #define CAN_SJW			   0

    #define CAN_PRESDIV_10K		(0x63*4+3)
    #define CAN_PROPSEG_10K		0x07
    #define CAN_PSEG1_10K		0x07
    #define CAN_PSEG2_10K		0x02

    #define CAN_PRESDIV_20K		(0x31*4+3)
    #define CAN_PROPSEG_20K		0x07
    #define CAN_PSEG1_20K		0x07
    #define CAN_PSEG2_20K		0x02

    #define CAN_PRESDIV_50K		(0x13*4+3)
    #define CAN_PROPSEG_50K		0x07
    #define CAN_PSEG1_50K		0x07
    #define CAN_PSEG2_50K		0x02

    #define CAN_PRESDIV_100K		(0x09*4+3)
    #define CAN_PROPSEG_100K		0x07
    #define CAN_PSEG1_100K		0x07
    #define CAN_PSEG2_100K		0x02

    #define CAN_PRESDIV_125K		(0x07*4+3)
    #define CAN_PROPSEG_125K		0x07
    #define CAN_PSEG1_125K		0x07
    #define CAN_PSEG2_125K		0x02
    
    #define CAN_PRESDIV_250K		(0x03*4+3)
    #define CAN_PROPSEG_250K		0x07
    #define CAN_PSEG1_250K		0x07
    #define CAN_PSEG2_250K		0x02

    #define CAN_PRESDIV_500K		(0x01*4+3)
    #define CAN_PROPSEG_500K		0x07
    #define CAN_PSEG1_500K		0x07
    #define CAN_PSEG2_500K		0x02

    /* bad samplepoint !!! */ 	
    #define CAN_PRESDIV_800K		(0x00*4+3)
    #define CAN_PROPSEG_800K		0x07
    #define CAN_PSEG1_800K		0x07
    #define CAN_PSEG2_800K		0x07
    
    #define CAN_PRESDIV_1000K		(0x00*4+3)
    #define CAN_PROPSEG_1000K		0x07
    #define CAN_PSEG1_1000K		0x07
    #define CAN_PSEG2_1000K		0x02
    
    #define CAN_SYSCLK_is_ok		1
#endif

#ifndef CAN_SYSCLK_is_ok
#  error Please specify a valid CAN_SYSCLK value (i.e. 10,20,40) or define new parameters
#endif
/*==========================================================================*/
#endif		/* __TOU_CAN_H  */
