/*****************************************************************************
*	$Author:   lewisrc  $
*	$Revision:   1.1  $
*	$Modtime:   Apr 01 2003 14:02:26  $
****************************************************************************/


/******************************************************************************/
/*  Base addresses for P50 memory-mapped peripherals                          */
/******************************************************************************/

/* P50 registers bases */
#define FALCONBase			0x280000
#define	P50ADSLCSRBase		0x34001c
#define	P50ADSLAFEBase		0x340030

/* Stuff the BSP needs to know to kill Showtime tasks */
#define SHOWTIME_TASK1_NAME "tDmtHi"
#define SHOWTIME_TASK2_NAME "tDmtDt"
#define SHOWTIME_TASK3_NAME "tDmtLo"
#define SHOWTIME_TASK4_NAME "tDmtLm"
#define SHOWTIME_TASK5_NAME "tDmtMed"

void FalconScanEnable		(BOOL State);
BOOL ReadFalconScanEnable	(void);
void SetGPIOF2PInt			(BOOL value);
void SetGPIOF2PCS			(BOOL value);
void LinedriverPower		(BOOL State);
BOOL ReadLinedriverPower	(void);
void AFEReset				(BOOL State);
BOOL ReadAFEReset			(void);
void FalconReset			(BOOL State);
BOOL ReadFalconReset		(void);
void FalconPowerDown		(BOOL State);
BOOL ReadFalconPowerDown	(void);
void SetDslRxDataInd		(BOOL value);
void SetDslTxDataInd		(BOOL value);

void ADSL_CS_Init			(void);
void SetF2Interrupts
(
							void (*(pIRQ1))(void *),
							void (*(pIRQ2))(void *),
							void *
);
void ADSL_EnableIRQ1_2		(void);
void ADSL_DisableIRQ1_2		(void);
void ADSL_EnableIRQ2		(void);
void ADSL_DisableIRQ2		(void);

void DpResetWiring			(void);
void DpSetAFEHybridSelect
(
	UINT8					Select_Line, 
	BOOL					State
);
BOOL DpReadAFEHybridSelect
(
	UINT8					Select_Line
);
BOOL DpQueryAnnexPin		(void);
void DpResetWiring			(void);

BOOL DpSwitchHookStateEnq	(void);

