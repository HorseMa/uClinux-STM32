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
// Object              : AT91RM9200 definitions
// Generated           : AT91 SW Application Group  12/03/2002 (10:48:02)
// 
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_TC_H
#define AT91RM9200_TC_H

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Timer Counter Channel Interface
// *****************************************************************************
#ifndef __ASSEMBLY__

typedef struct _AT91S_TC {
	AT91_REG	 TC_CCR; 	// Channel Control Register
	AT91_REG	 TC_CMR; 	// Channel Mode Register
	AT91_REG	 Reserved0[2]; 	// 
	AT91_REG	 TC_CV; 	// Counter Value
	AT91_REG	 TC_RA; 	// Register A
	AT91_REG	 TC_RB; 	// Register B
	AT91_REG	 TC_RC; 	// Register C
	AT91_REG	 TC_SR; 	// Status Register
	AT91_REG	 TC_IER; 	// Interrupt Enable Register
	AT91_REG	 TC_IDR; 	// Interrupt Disable Register
	AT91_REG	 TC_IMR; 	// Interrupt Mask Register
} AT91S_TC, *AT91PS_TC;

typedef struct _AT91S_TCB {
	AT91S_TC	 TCB_TC0; 	// TC Channel 0
	AT91_REG	 Reserved0[4]; 	// 
	AT91S_TC	 TCB_TC1; 	// TC Channel 1
	AT91_REG	 Reserved1[4]; 	// 
	AT91S_TC	 TCB_TC2; 	// TC Channel 2
	AT91_REG	 Reserved2[4]; 	// 
	AT91_REG	 TCB_BCR; 	// TC Block Control Register
	AT91_REG	 TCB_BMR; 	// TC Block Mode Register
} AT91S_TCB, *AT91PS_TCB;

#endif

// -------- TC_CCR : (TC Offset: 0x0) TC Channel Control Register -------- 
#define AT91C_TC_CLKEN        ( 0x1 <<  0) // (TC) Counter Clock Enable Command
#define AT91C_TC_CLKDIS       ( 0x1 <<  1) // (TC) Counter Clock Disable Command
#define AT91C_TC_SWTRG        ( 0x1 <<  2) // (TC) Software Trigger Command
// -------- TC_CMR : (TC Offset: 0x4) TC Channel Mode Register: Capture Mode / Waveform Mode -------- 
#define AT91C_TC_TCCLKS       ( 0x7 <<  0) // (TC) Clock Selection
#define		AT91C_TC_TIMER_DIV1_CLOCK             ( 0x0 <<  0) // (TC) MCK/2
#define		AT91C_TC_TIMER_DIV2_CLOCK             ( 0x1 <<  0) // (TC) MCK/8
#define		AT91C_TC_TIMER_DIV3_CLOCK             ( 0x2 <<  0) // (TC) MCK/32
#define		AT91C_TC_TIMER_DIV4_CLOCK             ( 0x3 <<  0) // (TC) MCK/128
#define		AT91C_TC_TIMER_DIV5_CLOCK             ( 0x4 <<  0) // (TC) MCK/256 = SLOW CLOCK
#define		AT91C_TC_TIMER_XC0                    ( 0x5 <<  0) // (TC) XC0
#define		AT91C_TC_TIMER_XC1                    ( 0x6 <<  0) // (TC) XC1
#define		AT91C_TC_TIMER_XC2                    ( 0x7 <<  0) // (TC) XC2
#define	AT91C_TC_CLKI         ( 0x1 <<  3) // (TC) Clock Invert
#define AT91C_TC_BURST        ( 0x3 <<  4) // (TC) Burst Signal Selection
#define AT91C_TC_CPCSTOP      ( 0x1 <<  6) // (TC) Counter Clock Stopped with RC Compare
#define AT91C_TC_CPCDIS       ( 0x1 <<  7) // (TC) Counter Clock Disable with RC Compare
#define AT91C_TC_EEVTEDG      ( 0x3 <<  8) // (TC) External Event Edge Selection
#define 	AT91C_TC_EEVTEDG_NONE                 ( 0x0 <<  8) // (TC) Edge: None
#define 	AT91C_TC_EEVTEDG_RISING               ( 0x1 <<  8) // (TC) Edge: rising edge
#define 	AT91C_TC_EEVTEDG_FALLING              ( 0x2 <<  8) // (TC) Edge: falling edge
#define 	AT91C_TC_EEVTEDG_BOTH                 ( 0x3 <<  8) // (TC) Edge: each edge
#define AT91C_TC_EEVT         ( 0x3 << 10) // (TC) External Event  Selection
#define 	AT91C_TC_EEVT_NONE                 ( 0x0 << 10) // (TC) Signal selected as external event: TIOB TIOB direction: input
#define 	AT91C_TC_EEVT_RISING               ( 0x1 << 10) // (TC) Signal selected as external event: XC0 TIOB direction: output
#define 	AT91C_TC_EEVT_FALLING              ( 0x2 << 10) // (TC) Signal selected as external event: XC1 TIOB direction: output
#define 	AT91C_TC_EEVT_BOTH                 ( 0x3 << 10) // (TC) Signal selected as external event: XC2 TIOB direction: output
#define AT91C_TC_ENETRG       ( 0x1 << 12) // (TC) External Event Trigger enable
#define AT91C_TC_WAVESEL      ( 0x3 << 13) // (TC) Waveform  Selection
#define 	AT91C_TC_WAVESEL_UP                   ( 0x0 << 13) // (TC) UP mode without atomatic trigger on RC Compare
#define 	AT91C_TC_WAVESEL_UP_AUTO              ( 0x1 << 13) // (TC) UP mode with automatic trigger on RC Compare
#define 	AT91C_TC_WAVESEL_UPDOWN               ( 0x2 << 13) // (TC) UPDOWN mode without automatic trigger on RC Compare
#define 	AT91C_TC_WAVESEL_UPDOWN_AUTO          ( 0x3 << 13) // (TC) UPDOWN mode with automatic trigger on RC Compare
#define AT91C_TC_CPCTRG       ( 0x1 << 14) // (TC) RC Compare Trigger Enable
#define AT91C_TC_WAVE         ( 0x1 << 15) // (TC) 
#define AT91C_TC_ACPA         ( 0x3 << 16) // (TC) RA Compare Effect on TIOA
#define 	AT91C_TC_ACPA_NONE                 ( 0x0 << 16) // (TC) Effect: none
#define 	AT91C_TC_ACPA_SET                  ( 0x1 << 16) // (TC) Effect: set
#define 	AT91C_TC_ACPA_CLEAR                ( 0x2 << 16) // (TC) Effect: clear
#define 	AT91C_TC_ACPA_TOGGLE               ( 0x3 << 16) // (TC) Effect: toggle
#define AT91C_TC_ACPC         ( 0x3 << 18) // (TC) RC Compare Effect on TIOA
#define 	AT91C_TC_ACPC_NONE                 ( 0x0 << 18) // (TC) Effect: none
#define 	AT91C_TC_ACPC_SET                  ( 0x1 << 18) // (TC) Effect: set
#define 	AT91C_TC_ACPC_CLEAR                ( 0x2 << 18) // (TC) Effect: clear
#define 	AT91C_TC_ACPC_TOGGLE               ( 0x3 << 18) // (TC) Effect: toggle
#define AT91C_TC_AEEVT        ( 0x3 << 20) // (TC) External Event Effect on TIOA
#define 	AT91C_TC_AEEVT_NONE                 ( 0x0 << 20) // (TC) Effect: none
#define 	AT91C_TC_AEEVT_SET                  ( 0x1 << 20) // (TC) Effect: set
#define 	AT91C_TC_AEEVT_CLEAR                ( 0x2 << 20) // (TC) Effect: clear
#define 	AT91C_TC_AEEVT_TOGGLE               ( 0x3 << 20) // (TC) Effect: toggle
#define AT91C_TC_ASWTRG       ( 0x3 << 22) // (TC) Software Trigger Effect on TIOA
#define 	AT91C_TC_ASWTRG_NONE                 ( 0x0 << 22) // (TC) Effect: none
#define 	AT91C_TC_ASWTRG_SET                  ( 0x1 << 22) // (TC) Effect: set
#define 	AT91C_TC_ASWTRG_CLEAR                ( 0x2 << 22) // (TC) Effect: clear
#define 	AT91C_TC_ASWTRG_TOGGLE               ( 0x3 << 22) // (TC) Effect: toggle
#define AT91C_TC_BCPB         ( 0x3 << 24) // (TC) RB Compare Effect on TIOB
#define 	AT91C_TC_BCPB_NONE                 ( 0x0 << 24) // (TC) Effect: none
#define 	AT91C_TC_BCPB_SET                  ( 0x1 << 24) // (TC) Effect: set
#define 	AT91C_TC_BCPB_CLEAR                ( 0x2 << 24) // (TC) Effect: clear
#define 	AT91C_TC_BCPB_TOGGLE               ( 0x3 << 24) // (TC) Effect: toggle
#define AT91C_TC_BCPC         ( 0x3 << 26) // (TC) RC Compare Effect on TIOB
#define 	AT91C_TC_BCPC_NONE                 ( 0x0 << 26) // (TC) Effect: none
#define 	AT91C_TC_BCPC_SET                  ( 0x1 << 26) // (TC) Effect: set
#define 	AT91C_TC_BCPC_CLEAR                ( 0x2 << 26) // (TC) Effect: clear
#define 	AT91C_TC_BCPC_TOGGLE               ( 0x3 << 26) // (TC) Effect: toggle
#define AT91C_TC_BEEVT        ( 0x3 << 28) // (TC) External Event Effect on TIOB
#define 	AT91C_TC_BEEVT_NONE                 ( 0x0 << 28) // (TC) Effect: none
#define 	AT91C_TC_BEEVT_SET                  ( 0x1 << 28) // (TC) Effect: set
#define 	AT91C_TC_BEEVT_CLEAR                ( 0x2 << 28) // (TC) Effect: clear
#define 	AT91C_TC_BEEVT_TOGGLE               ( 0x3 << 28) // (TC) Effect: toggle
#define AT91C_TC_BSWTRG       ( 0x3 << 30) // (TC) Software Trigger Effect on TIOB
#define 	AT91C_TC_BSWTRG_NONE                 ( 0x0 << 30) // (TC) Effect: none
#define 	AT91C_TC_BSWTRG_SET                  ( 0x1 << 30) // (TC) Effect: set
#define 	AT91C_TC_BSWTRG_CLEAR                ( 0x2 << 30) // (TC) Effect: clear
#define 	AT91C_TC_BSWTRG_TOGGLE               ( 0x3 << 30) // (TC) Effect: toggle
// -------- TC_SR : (TC Offset: 0x20) TC Channel Status Register -------- 
#define AT91C_TC_COVFS        ( 0x1 <<  0) // (TC) Counter Overflow
#define AT91C_TC_LOVRS        ( 0x1 <<  1) // (TC) Load Overrun
#define AT91C_TC_CPAS         ( 0x1 <<  2) // (TC) RA Compare
#define AT91C_TC_CPBS         ( 0x1 <<  3) // (TC) RB Compare
#define AT91C_TC_CPCS         ( 0x1 <<  4) // (TC) RC Compare
#define AT91C_TC_LDRAS        ( 0x1 <<  5) // (TC) RA Loading
#define AT91C_TC_LDRBS        ( 0x1 <<  6) // (TC) RB Loading
#define AT91C_TC_ETRCS        ( 0x1 <<  7) // (TC) External Trigger
#define AT91C_TC_ETRGS        ( 0x1 << 16) // (TC) Clock Enabling
#define AT91C_TC_MTIOA        ( 0x1 << 17) // (TC) TIOA Mirror
#define AT91C_TC_MTIOB        ( 0x1 << 18) // (TC) TIOA Mirror
// -------- TC_IER : (TC Offset: 0x24) TC Channel Interrupt Enable Register -------- 
// -------- TC_IDR : (TC Offset: 0x28) TC Channel Interrupt Disable Register -------- 
// -------- TC_IMR : (TC Offset: 0x2c) TC Channel Interrupt Mask Register -------- 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Timer Counter Interface
// *****************************************************************************
// -------- TCB_BCR : (TCB Offset: 0xc0) TC Block Control Register -------- 
#define AT91C_TCB_SYNC        ( 0x1 <<  0) // (TCB) Synchro Command
// -------- TCB_BMR : (TCB Offset: 0xc4) TC Block Mode Register -------- 
#define AT91C_TCB_TC0XC0S     ( 0x1 <<  0) // (TCB) External Clock Signal 0 Selection
#define 	AT91C_TCB_TC0XC0S_TCLK0                ( 0x0) // (TCB) TCLK0 connected to XC0
#define 	AT91C_TCB_TC0XC0S_NONE                 ( 0x1) // (TCB) None signal connected to XC0
#define 	AT91C_TCB_TC0XC0S_TIOA1                ( 0x2) // (TCB) TIOA1 connected to XC0
#define 	AT91C_TCB_TC0XC0S_TIOA2                ( 0x3) // (TCB) TIOA2 connected to XC0
#define AT91C_TCB_TC1XC1S     ( 0x1 <<  2) // (TCB) External Clock Signal 1 Selection
#define 	AT91C_TCB_TC1XC1S_TCLK1                ( 0x0 <<  2) // (TCB) TCLK1 connected to XC1
#define 	AT91C_TCB_TC1XC1S_NONE                 ( 0x1 <<  2) // (TCB) None signal connected to XC1
#define 	AT91C_TCB_TC1XC1S_TIOA0                ( 0x2 <<  2) // (TCB) TIOA0 connected to XC1
#define 	AT91C_TCB_TC1XC1S_TIOA2                ( 0x3 <<  2) // (TCB) TIOA2 connected to XC1
#define AT91C_TCB_TC2XC2S     ( 0x1 <<  4) // (TCB) External Clock Signal 2 Selection
#define 	AT91C_TCB_TC2XC2S_TCLK2                ( 0x0 <<  4) // (TCB) TCLK2 connected to XC2
#define 	AT91C_TCB_TC2XC2S_NONE                 ( 0x1 <<  4) // (TCB) None signal connected to XC2
#define 	AT91C_TCB_TC2XC2S_TIOA0                ( 0x2 <<  4) // (TCB) TIOA0 connected to XC2
#define 	AT91C_TCB_TC2XC2S_TIOA2                ( 0x3 <<  4) // (TCB) TIOA2 connected to XC2

#endif
