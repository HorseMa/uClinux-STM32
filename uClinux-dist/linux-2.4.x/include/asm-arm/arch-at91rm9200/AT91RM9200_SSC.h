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
// Object              : AT91RM9200 / SSC definitions
// Generated           : AT91 SW Application Group  12/03/2002 (10:48:02)
// 
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_SSC_H
#define AT91RM9200_SSC_H

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Synchronous Serial Controller Interface
// *****************************************************************************
#ifndef __ASSEMBLY__

typedef struct _AT91S_SSC {
	AT91_REG	 SSC_CR; 	// Control Register
	AT91_REG	 SSC_CMR; 	// Clock Mode Register
	AT91_REG	 Reserved0[2]; 	// 
	AT91_REG	 SSC_RCMR; 	// Receive Clock ModeRegister
	AT91_REG	 SSC_RFMR; 	// Receive Frame Mode Register
	AT91_REG	 SSC_TCMR; 	// Transmit Clock Mode Register
	AT91_REG	 SSC_TFMR; 	// Transmit Frame Mode Register
	AT91_REG	 SSC_RHR; 	// Receive Holding Register
	AT91_REG	 SSC_THR; 	// Transmit Holding Register
	AT91_REG	 Reserved1[2]; 	// 
	AT91_REG	 SSC_RSHR; 	// Receive Sync Holding Register
	AT91_REG	 SSC_TSHR; 	// Transmit Sync Holding Register
	AT91_REG	 SSC_RC0R; 	// Receive Compare 0 Register
	AT91_REG	 SSC_RC1R; 	// Receive Compare 1 Register
	AT91_REG	 SSC_SR; 	// Status Register
	AT91_REG	 SSC_IER; 	// Interrupt Enable Register
	AT91_REG	 SSC_IDR; 	// Interrupt Disable Register
	AT91_REG	 SSC_IMR; 	// Interrupt Mask Register
	AT91_REG	 Reserved2[44]; 	// 
	AT91_REG	 SSC_RPR; 	// Receive Pointer Register
	AT91_REG	 SSC_RCR; 	// Receive Counter Register
	AT91_REG	 SSC_TPR; 	// Transmit Pointer Register
	AT91_REG	 SSC_TCR; 	// Transmit Counter Register
	AT91_REG	 SSC_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 SSC_RNCR; 	// Receive Next Counter Register
	AT91_REG	 SSC_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 SSC_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 SSC_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 SSC_PTSR; 	// PDC Transfer Status Register
} AT91S_SSC, *AT91PS_SSC;

#endif

// -------- SSC_CR : (SSC Offset: 0x0) SSC Control Register -------- 
#define AT91C_SSC_RXEN        ( 0x1 <<  0) // (SSC) Receive Enable
#define AT91C_SSC_RXDIS       ( 0x1 <<  1) // (SSC) Receive Disable
#define AT91C_SSC_TXEN        ( 0x1 <<  8) // (SSC) Transmit Enable
#define AT91C_SSC_TXDIS       ( 0x1 <<  9) // (SSC) Transmit Disable
#define AT91C_SSC_SWRST       ( 0x1 << 15) // (SSC) Software Reset
// -------- SSC_RCMR : (SSC Offset: 0x10) SSC Receive Clock Mode Register -------- 
#define AT91C_SSC_CKS         ( 0x3 <<  0) // (SSC) Receive/Transmit Clock Selection
#define 	AT91C_SSC_CKS_DIV                  ( 0x0) // (SSC) Divided Clock
#define 	AT91C_SSC_CKS_TK                   ( 0x1) // (SSC) TK Clock signal
#define 	AT91C_SSC_CKS_RK                   ( 0x2) // (SSC) RK pin
#define AT91C_SSC_CKO         ( 0x7 <<  2) // (SSC) Receive/Transmit Clock Output Mode Selection
#define 	AT91C_SSC_CKO_NONE                 ( 0x0 <<  2) // (SSC) Receive/Transmit Clock Output Mode: None RK pin: Input-only
#define 	AT91C_SSC_CKO_CONTINOUS            ( 0x1 <<  2) // (SSC) Continuous Receive/Transmit Clock RK pin: Output
#define 	AT91C_SSC_CKO_DATA_TX              ( 0x2 <<  2) // (SSC) Receive/Transmit Clock only during data transfers RK pin: Output
#define AT91C_SSC_CKI         ( 0x1 <<  5) // (SSC) Receive/Transmit Clock Inversion
#define AT91C_SSC_CKG         ( 0x3 <<  6) // (SSC) Receive/Transmit Clock Gating Selection
#define 	AT91C_SSC_CKG_NONE                 ( 0x0 <<  6) // (SSC) Receive/Transmit Clock Gating: None, continuous clock
#define 	AT91C_SSC_CKG_LOW                  ( 0x1 <<  6) // (SSC) Receive/Transmit Clock enabled only if RF Low
#define 	AT91C_SSC_CKG_HIGH                 ( 0x2 <<  6) // (SSC) Receive/Transmit Clock enabled only if RF High
#define AT91C_SSC_START       ( 0xF <<  8) // (SSC) Receive/Transmit Start Selection
#define 	AT91C_SSC_START_CONTINOUS            ( 0x0 <<  8) // (SSC) Continuous, as soon as the receiver is enabled, and immediately after the end of transfer of the previous data.
#define 	AT91C_SSC_START_TX                   ( 0x1 <<  8) // (SSC) Transmit/Receive start
#define 	AT91C_SSC_START_LOW_RF               ( 0x2 <<  8) // (SSC) Detection of a low level on RF input
#define 	AT91C_SSC_START_HIGH_RF              ( 0x3 <<  8) // (SSC) Detection of a high level on RF input
#define 	AT91C_SSC_START_FALL_RF              ( 0x4 <<  8) // (SSC) Detection of a falling edge on RF input
#define 	AT91C_SSC_START_RISE_RF              ( 0x5 <<  8) // (SSC) Detection of a rising edge on RF input
#define 	AT91C_SSC_START_LEVEL_RF             ( 0x6 <<  8) // (SSC) Detection of any level change on RF input
#define 	AT91C_SSC_START_EDGE_RF              ( 0x7 <<  8) // (SSC) Detection of any edge on RF input
#define 	AT91C_SSC_START_0                    ( 0x8 <<  8) // (SSC) Compare 0
#define AT91C_SSC_STOP        ( 0x1 << 12) // (SSC) Receive Stop Selection
#define AT91C_SSC_STTOUT      ( 0x1 << 15) // (SSC) Receive/Transmit Start Output Selection
#define AT91C_SSC_STTDLY      ( 0xFF << 16) // (SSC) Receive/Transmit Start Delay
#define AT91C_SSC_PERIOD      ( 0xFF << 24) // (SSC) Receive/Transmit Period Divider Selection
// -------- SSC_RFMR : (SSC Offset: 0x14) SSC Receive Frame Mode Register -------- 
#define AT91C_SSC_DATLEN      ( 0x1F <<  0) // (SSC) Data Length
#define AT91C_SSC_LOOP        ( 0x1 <<  5) // (SSC) Loop Mode
#define AT91C_SSC_MSBF        ( 0x1 <<  7) // (SSC) Most Significant Bit First
#define AT91C_SSC_DATNB       ( 0xF <<  8) // (SSC) Data Number per Frame
#define AT91C_SSC_FSLEN       ( 0xF << 16) // (SSC) Receive/Transmit Frame Sync length
#define AT91C_SSC_FSOS        ( 0x7 << 20) // (SSC) Receive/Transmit Frame Sync Output Selection
#define 	AT91C_SSC_FSOS_NONE                 ( 0x0 << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: None RK pin Input-only
#define 	AT91C_SSC_FSOS_NEGATIVE             ( 0x1 << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Negative Pulse
#define 	AT91C_SSC_FSOS_POSITIVE             ( 0x2 << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Positive Pulse
#define 	AT91C_SSC_FSOS_LOW                  ( 0x3 << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Driver Low during data transfer
#define 	AT91C_SSC_FSOS_HIGH                 ( 0x4 << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Driver High during data transfer
#define 	AT91C_SSC_FSOS_TOGGLE               ( 0x5 << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Toggling at each start of data transfer
#define AT91C_SSC_FSEDGE      ( 0x1 << 24) // (SSC) Frame Sync Edge Detection
// -------- SSC_TCMR : (SSC Offset: 0x18) SSC Transmit Clock Mode Register -------- 
// -------- SSC_TFMR : (SSC Offset: 0x1c) SSC Transmit Frame Mode Register -------- 
#define AT91C_SSC_DATDEF      ( 0x1 <<  5) // (SSC) Data Default Value
#define AT91C_SSC_FSDEN       ( 0x1 << 23) // (SSC) Frame Sync Data Enable
// -------- SSC_SR : (SSC Offset: 0x40) SSC Status Register -------- 
#define AT91C_SSC_TXRDY       ( 0x1 <<  0) // (SSC) Transmit Ready
#define AT91C_SSC_TXEMPTY     ( 0x1 <<  1) // (SSC) Transmit Empty
#define AT91C_SSC_ENDTX       ( 0x1 <<  2) // (SSC) End Of Transmission
#define AT91C_SSC_TXBUFE      ( 0x1 <<  3) // (SSC) Transmit Buffer Empty
#define AT91C_SSC_RXRDY       ( 0x1 <<  4) // (SSC) Receive Ready
#define AT91C_SSC_OVRUN       ( 0x1 <<  5) // (SSC) Receive Overrun
#define AT91C_SSC_ENDRX       ( 0x1 <<  6) // (SSC) End of Reception
#define AT91C_SSC_RXBUFF      ( 0x1 <<  7) // (SSC) Receive Buffer Full
#define AT91C_SSC_CP0         ( 0x1 <<  8) // (SSC) Compare 0
#define AT91C_SSC_CP1         ( 0x1 <<  9) // (SSC) Compare 1
#define AT91C_SSC_TXSYN       ( 0x1 << 10) // (SSC) Transmit Sync
#define AT91C_SSC_RXSYN       ( 0x1 << 11) // (SSC) Receive Sync
#define AT91C_SSC_TXENA       ( 0x1 << 16) // (SSC) Transmit Enable
#define AT91C_SSC_RXENA       ( 0x1 << 17) // (SSC) Receive Enable
// -------- SSC_IER : (SSC Offset: 0x44) SSC Interrupt Enable Register -------- 
// -------- SSC_IDR : (SSC Offset: 0x48) SSC Interrupt Disable Register -------- 
// -------- SSC_IMR : (SSC Offset: 0x4c) SSC Interrupt Mask Register -------- 

#endif
