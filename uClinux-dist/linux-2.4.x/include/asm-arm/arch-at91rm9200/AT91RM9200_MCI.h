// ----------------------------------------------------------------------------
//          ATMEL Microcontroller Software Support  -  ROUSSET  -
// ----------------------------------------------------------------------------
//  The software is delivered "AS IS" without warranty or condition of any
//  kind, either express, implied or statutory. This includes without
//  limitation any warranty or condition with respect to merchantability or
//  fitness for any particular purpose, or against the infringements of
//  intellectual property rights of others.
// ----------------------------------------------------------------------------
// File Name           : AT91RM9200.h
// Object              : AT91RM9200 / MCI definitions
// Generated           : AT91 SW Application Group  12/03/2002 (10:48:02)
// 
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_MCI_H
#define AT91RM9200_MCI_H

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Multimedia Card Interface
// *****************************************************************************
#ifndef __ASSEMBLY__

typedef struct _AT91S_MCI {
	AT91_REG	 MCI_CR; 	// MCI Control Register
	AT91_REG	 MCI_MR; 	// MCI Mode Register
	AT91_REG	 MCI_DTOR; 	// MCI Data Timeout Register
	AT91_REG	 MCI_SDCR; 	// MCI SD Card Register
	AT91_REG	 MCI_ARGR; 	// MCI Argument Register
	AT91_REG	 MCI_CMDR; 	// MCI Command Register
	AT91_REG	 Reserved0[2]; 	// 
	AT91_REG	 MCI_RSPR[4]; 	// MCI Response Register
	AT91_REG	 MCI_RDR; 	// MCI Receive Data Register
	AT91_REG	 MCI_TDR; 	// MCI Transmit Data Register
	AT91_REG	 Reserved1[2]; 	// 
	AT91_REG	 MCI_SR; 	// MCI Status Register
	AT91_REG	 MCI_IER; 	// MCI Interrupt Enable Register
	AT91_REG	 MCI_IDR; 	// MCI Interrupt Disable Register
	AT91_REG	 MCI_IMR; 	// MCI Interrupt Mask Register
	AT91_REG	 Reserved2[44]; 	// 
	AT91_REG	 MCI_RPR; 	// Receive Pointer Register
	AT91_REG	 MCI_RCR; 	// Receive Counter Register
	AT91_REG	 MCI_TPR; 	// Transmit Pointer Register
	AT91_REG	 MCI_TCR; 	// Transmit Counter Register
	AT91_REG	 MCI_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 MCI_RNCR; 	// Receive Next Counter Register
	AT91_REG	 MCI_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 MCI_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 MCI_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 MCI_PTSR; 	// PDC Transfer Status Register
} AT91S_MCI, *AT91PS_MCI;

#endif

// -------- MCI_CR : (MCI Offset: 0x0) MCI Control Register -------- 
#define AT91C_MCI_MCIEN       ((unsigned int) 0x1 <<  0) // (MCI) Multimedia Interface Enable
#define AT91C_MCI_MCIDIS      ((unsigned int) 0x1 <<  1) // (MCI) Multimedia Interface Disable
#define AT91C_MCI_PWSEN       ((unsigned int) 0x1 <<  2) // (MCI) Power Save Mode Enable
#define AT91C_MCI_PWSDIS      ((unsigned int) 0x1 <<  3) // (MCI) Power Save Mode Disable
// -------- MCI_MR : (MCI Offset: 0x4) MCI Mode Register -------- 
#define AT91C_MCI_CLKDIV      ((unsigned int) 0x1 <<  0) // (MCI) Clock Divider
#define AT91C_MCI_PWSDIV      ((unsigned int) 0x1 <<  8) // (MCI) Power Saving Divider
#define AT91C_MCI_PDCPADV     ((unsigned int) 0x1 << 14) // (MCI) PDC Padding Value
#define AT91C_MCI_PDCMODE     ((unsigned int) 0x1 << 15) // (MCI) PDC Oriented Mode
#define AT91C_MCI_BLKLEN      ((unsigned int) 0x1 << 18) // (MCI) Data Block Length
// -------- MCI_DTOR : (MCI Offset: 0x8) MCI Data Timeout Register -------- 
#define AT91C_MCI_DTOCYC      ((unsigned int) 0x1 <<  0) // (MCI) Data Timeout Cycle Number
#define AT91C_MCI_DTOMUL      ((unsigned int) 0x7 <<  4) // (MCI) Data Timeout Multiplier
#define 	AT91C_MCI_DTOMUL_1                    ((unsigned int) 0x0 <<  4) // (MCI) DTOCYC x 1
#define 	AT91C_MCI_DTOMUL_16                   ((unsigned int) 0x1 <<  4) // (MCI) DTOCYC x 16
#define 	AT91C_MCI_DTOMUL_128                  ((unsigned int) 0x2 <<  4) // (MCI) DTOCYC x 128
#define 	AT91C_MCI_DTOMUL_256                  ((unsigned int) 0x3 <<  4) // (MCI) DTOCYC x 256
#define 	AT91C_MCI_DTOMUL_1024                 ((unsigned int) 0x4 <<  4) // (MCI) DTOCYC x 1024
#define 	AT91C_MCI_DTOMUL_4096                 ((unsigned int) 0x5 <<  4) // (MCI) DTOCYC x 4096
#define 	AT91C_MCI_DTOMUL_65536                ((unsigned int) 0x6 <<  4) // (MCI) DTOCYC x 65536
#define 	AT91C_MCI_DTOMUL_1048576              ((unsigned int) 0x7 <<  4) // (MCI) DTOCYC x 1048576
// -------- MCI_SDCR : (MCI Offset: 0xc) MCI SD Card Register -------- 
#define AT91C_MCI_SCDSEL      ((unsigned int) 0x1 <<  0) // (MCI) SD Card Selector
#define AT91C_MCI_SCDBUS      ((unsigned int) 0x1 <<  7) // (MCI) SD Card Bus Width
// -------- MCI_CMDR : (MCI Offset: 0x14) MCI Command Register -------- 
#define AT91C_MCI_CMDNB       ((unsigned int) 0x1F <<  0) // (MCI) Command Number
#define AT91C_MCI_RSPTYP      ((unsigned int) 0x3 <<  6) // (MCI) Response Type
#define 	AT91C_MCI_RSPTYP_NO                   ((unsigned int) 0x0 <<  6) // (MCI) No response
#define 	AT91C_MCI_RSPTYP_48                   ((unsigned int) 0x1 <<  6) // (MCI) 48-bit response
#define 	AT91C_MCI_RSPTYP_136                  ((unsigned int) 0x2 <<  6) // (MCI) 136-bit response
#define AT91C_MCI_SPCMD       ((unsigned int) 0x7 <<  8) // (MCI) Special CMD
#define 	AT91C_MCI_SPCMD_NONE                 ((unsigned int) 0x0 <<  8) // (MCI) Not a special CMD
#define 	AT91C_MCI_SPCMD_INIT                 ((unsigned int) 0x1 <<  8) // (MCI) Initialization CMD
#define 	AT91C_MCI_SPCMD_SYNC                 ((unsigned int) 0x2 <<  8) // (MCI) Synchronized CMD
#define 	AT91C_MCI_SPCMD_IT_CMD               ((unsigned int) 0x4 <<  8) // (MCI) Interrupt command
#define 	AT91C_MCI_SPCMD_IT_REP               ((unsigned int) 0x5 <<  8) // (MCI) Interrupt response
#define AT91C_MCI_OPDCMD      ((unsigned int) 0x1 << 11) // (MCI) Open Drain Command
#define AT91C_MCI_MAXLAT      ((unsigned int) 0x1 << 12) // (MCI) Maximum Latency for Command to respond
#define AT91C_MCI_TRCMD       ((unsigned int) 0x3 << 16) // (MCI) Transfer CMD
#define 	AT91C_MCI_TRCMD_NO                   ((unsigned int) 0x0 << 16) // (MCI) No transfer
#define 	AT91C_MCI_TRCMD_START                ((unsigned int) 0x1 << 16) // (MCI) Start transfer
#define 	AT91C_MCI_TRCMD_STOP                 ((unsigned int) 0x2 << 16) // (MCI) Stop transfer
#define AT91C_MCI_TRDIR       ((unsigned int) 0x1 << 18) // (MCI) Transfer Direction
#define AT91C_MCI_TRTYP       ((unsigned int) 0x3 << 19) // (MCI) Transfer Type
#define 	AT91C_MCI_TRTYP_BLOCK                ((unsigned int) 0x0 << 19) // (MCI) Block Transfer type
#define 	AT91C_MCI_TRTYP_MULTIPLE             ((unsigned int) 0x1 << 19) // (MCI) Multiple Block transfer type
#define 	AT91C_MCI_TRTYP_STREAM               ((unsigned int) 0x2 << 19) // (MCI) Stream transfer type
// -------- MCI_SR : (MCI Offset: 0x40) MCI Status Register -------- 
#define AT91C_MCI_CMDRDY      ((unsigned int) 0x1 <<  0) // (MCI) Command Ready flag
#define AT91C_MCI_RXRDY       ((unsigned int) 0x1 <<  1) // (MCI) RX Ready flag
#define AT91C_MCI_TXRDY       ((unsigned int) 0x1 <<  2) // (MCI) TX Ready flag
#define AT91C_MCI_BLKE        ((unsigned int) 0x1 <<  3) // (MCI) Data Block Transfer Ended flag
#define AT91C_MCI_DTIP        ((unsigned int) 0x1 <<  4) // (MCI) Data Transfer in Progress flag
#define AT91C_MCI_NOTBUSY     ((unsigned int) 0x1 <<  5) // (MCI) Data Line Not Busy flag
#define AT91C_MCI_ENDRX       ((unsigned int) 0x1 <<  6) // (MCI) End of RX Buffer flag
#define AT91C_MCI_ENDTX       ((unsigned int) 0x1 <<  7) // (MCI) End of TX Buffer flag
#define AT91C_MCI_RXBUFF      ((unsigned int) 0x1 << 14) // (MCI) RX Buffer Full flag
#define AT91C_MCI_TXBUFE      ((unsigned int) 0x1 << 15) // (MCI) TX Buffer Empty flag
#define AT91C_MCI_RINDE       ((unsigned int) 0x1 << 16) // (MCI) Response Index Error flag
#define AT91C_MCI_RDIRE       ((unsigned int) 0x1 << 17) // (MCI) Response Direction Error flag
#define AT91C_MCI_RCRCE       ((unsigned int) 0x1 << 18) // (MCI) Response CRC Error flag
#define AT91C_MCI_RENDE       ((unsigned int) 0x1 << 19) // (MCI) Response End Bit Error flag
#define AT91C_MCI_RTOE        ((unsigned int) 0x1 << 20) // (MCI) Response Time-out Error flag
#define AT91C_MCI_DCRCE       ((unsigned int) 0x1 << 21) // (MCI) data CRC Error flag
#define AT91C_MCI_DTOE        ((unsigned int) 0x1 << 22) // (MCI) Data timeout Error flag
#define AT91C_MCI_OVRE        ((unsigned int) 0x1 << 30) // (MCI) Overrun flag
#define AT91C_MCI_UNRE        ((unsigned int) 0x1 << 31) // (MCI) Underrun flag
// -------- MCI_IER : (MCI Offset: 0x44) MCI Interrupt Enable Register -------- 
// -------- MCI_IDR : (MCI Offset: 0x48) MCI Interrupt Disable Register -------- 
// -------- MCI_IMR : (MCI Offset: 0x4c) MCI Interrupt Mask Register -------- 

#endif
