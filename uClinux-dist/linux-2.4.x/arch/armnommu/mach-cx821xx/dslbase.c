/****************************************************************************
 *
 *  Name:			DSLBASE.C
 *
 *  Description:	Basic DSL routines for DMT pump control
 *
 *  Copyright:		(c) 2001 - 2003  Conexant Systems Inc.
 *
 ****************************************************************************
 *
 *  $Author:   belljl  $
 *  $Revision:   1.3  $
 *  $Modtime:   Jun 27 2003 18:00:24  $
 *
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/init.h>

#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE) || defined (CONFIG_CNXT_GSHDSL) || defined (CONFIG_CNXT_GSHDSL_MODULE)
#include "bsptypes.h"
#include "bspcfg.h"
#include "gpio.h"
#include "dslbase.h"
#include "cnxtbsp.h"
#include "OsTools.h"
#include "cnxttimer.h"
#include "cnxtirq.h"

#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
extern void *pFalconContext ;
extern void ( *pF2_IRQ1_ISR ) ( void * ) ;
extern void ( *pF2_IRQ2_ISR ) ( void * ) ;

/****************************************************************
 ****************************************************************/

static BOOL FalconScanEnableCache ;
void FalconScanEnable ( BOOL State ) 
{
	WriteGPIOData ( GPOUT_F2_SCANEN, State ) ;	/* GPIO30 ( P50 ) , GPIO26 ( ZIG ) */
	FalconScanEnableCache = State ;
}

/****************************************************************
 ****************************************************************/

BOOL ReadFalconScanEnable ( void ) 
{
	//return ReadGPIOData ( GPOUT_F2_SCANEN ) ;	/* GPIO30 ( P50 ) , GPIO26 ( ZIG ) */
	return FalconScanEnableCache ;
}

/****************************************************************
 ****************************************************************/

/* Showtime in F2 IRQ handlers */
/* GPIO26 for Pyramid, GPIO19 for Ziggy */
void SetGPIOF2PInt ( BOOL value ) 
{
	//WriteGPIOData ( GPOUT_TXD_LED, ! value ) ;
}

/****************************************************************
 ****************************************************************/

/* Showtime in critical section */
void SetGPIOF2PCS ( BOOL value ) 
{
	// GPIO27 for Pyramid, GPIO20 for Ziggy
	// WriteGPIOData ( GPOUT_RXD_LED, ! value ) ;
}

/****************************************************************
 ****************************************************************/

static BOOL LinedriverPowerCache ;
void LinedriverPower ( BOOL State ) 
{
	/* GPIO13 */	
	WriteGPIOData ( GPOUT_LDRVPWR, State ) ;
	LinedriverPowerCache = State ;
}

/****************************************************************
 ****************************************************************/

BOOL ReadLinedriverPower ( void ) 
{
	/* GPIO13 */	
	// return ReadGPIOData ( GPOUT_LDRVPWR ) ;
	return LinedriverPowerCache ;
}

/****************************************************************
 ****************************************************************/

static BOOL AFEResetCache ;
void AFEReset ( BOOL State ) 
{
	/* GPIO8 */	
	WriteGPIOData ( GPOUT_AFERESET, ! State ) ;
	AFEResetCache = State ;
}

/****************************************************************
 ****************************************************************/

BOOL ReadAFEReset ( void ) 
{
	/* GPIO8 */	
	//return ! ReadGPIOData ( GPOUT_AFERESET ) ;
	return AFEResetCache ;
}

/****************************************************************
 ****************************************************************/

static BOOL FalconResetCache ;
void FalconReset ( BOOL State ) 
{
	/* GPIO6 ( P46 ) , GPIO9 ( P50 ) */	
	WriteGPIOData ( GPOUT_F2RESET, State ) ;
	FalconResetCache = State ;
}

/****************************************************************
 ****************************************************************/

BOOL ReadFalconReset ( void ) 
{
	/* GPIO6 ( P46 ) , GPIO9 ( P50 ) */	
	//return ReadGPIOData ( GPOUT_F2RESET ) ;
	return FalconResetCache ;
}

/****************************************************************
 ****************************************************************/

static BOOL FalconPowerDownCache ;
void FalconPowerDown ( BOOL State ) 
{
	/* GPIO0 ( P46 ) , GPIO12 ( P50 ) */
	WriteGPIOData ( GPOUT_F2PWRDWN, State ) ;
	FalconPowerDownCache = State ;
}

/****************************************************************
 ****************************************************************/

BOOL ReadFalconPowerDown ( void ) 
{
	/* GPIO0 ( P46 ) , GPIO12 ( P50 ) */
	//return ReadGPIOData ( GPOUT_F2PWRDWN ) ;
	return FalconPowerDownCache ;
}

/****************************************************************
 ****************************************************************/

/* Showtime code registers callbacks */
void SetF2Interrupts ( void ( * ( pIRQ1 ) ) ( void * ) , void ( * ( pIRQ2 ) ) ( void * ) , void *pContext ) 
{
	UINT32 OldLevel = 0 ;

	OldLevel = intLock ( ) ;

	pFalconContext = pContext ;
	if ( pIRQ1 ) 
	{
		pF2_IRQ1_ISR = pIRQ1 ;

		GPIO_SetGPIOIRQRoutine ( GPIOINT_F2_IRQ1, GPIOB10_Handler ) ;
		SetGPIOInputEnable ( GPIOINT_F2_IRQ1, GP_INPUTON ) ;

		SetGPIOIntEnable ( GPIOINT_F2_IRQ1, IRQ_OFF ) ;
		#ifdef LEVEL_MODE_ADSL
			SetGPIOIntMode ( GPIOINT_F2_IRQ1, GP_IRQ_MODE_LEVEL ) ;
		#endif
		SetGPIOIntPolarity ( GPIOINT_F2_IRQ1, GP_IRQ_POL_NEGATIVE ) ;
		SetGPIOIntEnable ( GPIOINT_F2_IRQ1, IRQ_ON ) ;
	}
	else
	{
		SetGPIOIntEnable ( GPIOINT_F2_IRQ1, IRQ_OFF ) ;
		ClearGPIOIntStatus ( GPIOINT_F2_IRQ1 ) ;
		pF2_IRQ1_ISR = pIRQ1 ;
	}

	if ( pIRQ2 ) 
	{
		pF2_IRQ2_ISR = pIRQ2 ;

		GPIO_SetGPIOIRQRoutine ( GPIOINT_F2_IRQ2, GPIOB11_Handler ) ;
		SetGPIOInputEnable ( GPIOINT_F2_IRQ2, GP_INPUTON ) ;

		SetGPIOIntEnable ( GPIOINT_F2_IRQ2, IRQ_OFF ) ;
		#ifdef LEVEL_MODE_ADSL
			SetGPIOIntMode ( GPIOINT_F2_IRQ2, GP_IRQ_MODE_LEVEL ) ;
		#endif
		SetGPIOIntPolarity ( GPIOINT_F2_IRQ2, GP_IRQ_POL_NEGATIVE ) ;
		SetGPIOIntEnable ( GPIOINT_F2_IRQ2, IRQ_ON ) ;
	}
	else
	{
		SetGPIOIntEnable ( GPIOINT_F2_IRQ2, IRQ_OFF ) ;
		ClearGPIOIntStatus ( GPIOINT_F2_IRQ2 ) ;
		pF2_IRQ2_ISR = pIRQ2 ;
	}

	intUnlock ( OldLevel ) ;

}

/****************************************************************
 ****************************************************************/

#ifndef LEVEL_MODE_ADSL
	static int oldValue[32] ;
#endif

static int DisableInterruptCount = 0 ;

/****************************************************************
 ****************************************************************/

void ADSL_CS_Init ( void ) 
{
	DisableInterruptCount = 0 ;
	pF2_IRQ1_ISR = 0 ;
	pF2_IRQ2_ISR = 0 ;
}

/****************************************************************
 ****************************************************************/

void ADSL_EnableIRQ1_2 ( void ) 
{
	register UINT32 temp ;

	if ( pF2_IRQ1_ISR == 0
	  && pF2_IRQ2_ISR == 0 ) 
	{
		return ;
	}

	temp = intLock ( ) ;

	#ifdef LEVEL_MODE_ADSL

		if ( DisableInterruptCount ) 
		{
			DisableInterruptCount-- ;
		}
		if ( DisableInterruptCount == 0 ) 
		{
			/* Indicate out of critical section for debug */
			SetGPIOF2PCS ( FALSE ) ;

			if ( pF2_IRQ1_ISR ) 
			{
				SetGPIOIntEnable ( GPIOINT_F2_IRQ1, 1 ) ;
			}
			if ( pF2_IRQ2_ISR ) 
			{
				SetGPIOIntEnable ( GPIOINT_F2_IRQ2, 1 ) ;
			}
		}

		intUnlock ( temp ) ;

	#else /* LEVEL_MODE_ADSL */

		if ( DisableInterruptCount == 0 ) 
		{
			intUnlock ( temp ) ;
			return ;
		}

		DisableInterruptCount -- ;
		if ( DisableInterruptCount == 0 ) 
		{
			/* Indicate out of critical section for debug */
			SetGPIOF2PCS ( FALSE ) ;
		}

		/* Allow for nesting, push values onto a stack */
		intUnlock ( temp ) ;
		intUnlock ( oldValue[DisableInterruptCount] ) ;

	#endif /* LEVEL_MODE_ADSL */
}

/****************************************************************
 ****************************************************************/

void ADSL_DisableIRQ1_2 ( void ) 
{
	register UINT32 temp ;

	if ( pF2_IRQ1_ISR == 0 
	  && pF2_IRQ2_ISR == 0 ) 
	{
		return ;
	}

	temp = intLock ( ) ;

	#ifdef LEVEL_MODE_ADSL

		DisableInterruptCount++ ;
		if ( pF2_IRQ1_ISR ) 
		{
			SetGPIOIntEnable ( GPIOINT_F2_IRQ1, 0 ) ;
		}
		if ( pF2_IRQ2_ISR ) 
		{
			SetGPIOIntEnable ( GPIOINT_F2_IRQ2, 0 ) ;
		}

		/* Indicate start of critical section for debug */
		SetGPIOF2PCS ( TRUE ) ;

		intUnlock ( temp ) ;

	#else /* LEVEL_MODE_ADSL */

		oldValue [DisableInterruptCount] = temp ;
		if ( DisableInterruptCount == 0 ) 
		{
			/* Indicate start of critical section for debug */
			SetGPIOF2PCS ( TRUE ) ;
		}
		DisableInterruptCount++ ;

	#endif /* LEVEL_MODE_ADSL */
}


/****************************************************************
 ****************************************************************/

void ADSL_EnableIRQ2 ( void ) 
{
	register UINT32 temp ;

	if ( pF2_IRQ2_ISR == 0 ) 
	{
		return ;
	}

	temp = intLock ( ) ;

	#ifdef LEVEL_MODE_ADSL

		if ( DisableInterruptCount ) 
		{
			DisableInterruptCount-- ;
		}

		if ( DisableInterruptCount == 0 ) 
		{
			/* Indicate out of critical section for debug */
			SetGPIOF2PCS ( FALSE ) ;

			if ( pF2_IRQ2_ISR ) 
			{
				SetGPIOIntEnable ( GPIOINT_F2_IRQ2, 1 ) ;
			}
		}

		intUnlock ( temp ) ;

	#else /* LEVEL_MODE_ADSL */

		if ( DisableInterruptCount == 0 ) 
		{
			intUnlock ( temp ) ;
			return ;
		}

		DisableInterruptCount-- ;
		if ( DisableInterruptCount == 0 ) 
		{
			/* Indicate out of critical section for debug */
			SetGPIOF2PCS ( FALSE ) ;
		}

		/* Allow for nesting, push values onto a stack */
		intUnlock ( temp ) ;
		intUnlock ( oldValue [DisableInterruptCount] ) ;

	#endif /* LEVEL_MODE_ADSL */
}

/****************************************************************
 ****************************************************************/

void ADSL_DisableIRQ2 ( void ) 
{
	register UINT32 temp ;

	if ( pF2_IRQ2_ISR == 0 ) 
	{
		return ;
	}

	temp = intLock ( ) ;

	#ifdef LEVEL_MODE_ADSL

		DisableInterruptCount++ ;
		if ( pF2_IRQ2_ISR ) 
		{
			SetGPIOIntEnable ( GPIOINT_F2_IRQ2, 0 ) ;
		}

		/* Indicate start of critical section for debug */
		SetGPIOF2PCS ( TRUE ) ;

		intUnlock ( temp ) ;

	#else /* LEVEL_MODE_ADSL */

		oldValue[DisableInterruptCount] = temp ;
		if ( DisableInterruptCount == 0 ) 
		{
			/* Indicate start of critical section for debug */
			SetGPIOF2PCS ( TRUE ) ;
		}

		DisableInterruptCount++ ;

	#endif /* LEVEL_MODE_ADSL */
}


/*********************************************************************************
    Function     : void DpResetWiring ( SYS_WIRING_TYPE wire_pair ) 
    Description  : This routine will deenergize the relay used for wire switching.
**********************************************************************************/

void DpResetWiring ( void )	
{
	// Outer pair
	WriteGPIOData ( GPOUT_OUTER_PAIR, 0 ) ;

	// Inner pair
	WriteGPIOData ( GPOUT_INNER_PAIR, 0 ) ;

	// Short/Long loop switch
	WriteGPIOData ( GPOUT_SHORTLOOPSW, 1 ) ;

}


/*********************************************************************************
    Function     : void DpSetAFEHybridSelect ( UINT8	 Select_Line, BOOLEAN State ) 
    Description  : This routine will deenergize the relay used for wire switching.
**********************************************************************************/
static BOOL DpSetAFEHybridSelectCache[2] ;
void DpSetAFEHybridSelect
( 
 	UINT8					  Select_Line, 
	BOOL					  State
) 
{
	switch ( Select_Line ) 
	{
		case 1:
			WriteGPIOData ( GPOUT_SHORTLOOPSW, State ) ;
			DpSetAFEHybridSelectCache[Select_Line-1] = State ;
			break ;
		case 2:
			DpSetAFEHybridSelectCache[Select_Line-1] = State ;
			break ;
	}
}

/*********************************************************************************
    Function     : void DpSetAFEHybridSelect ( UINT8	 Select_Line, BOOLEAN State ) 
    Description  : This routine will deenergize the relay used for wire switching.
**********************************************************************************/
BOOL DpReadAFEHybridSelect
( 
 	UINT8					  Select_Line
) 
{
	BOOL State ;

	switch ( Select_Line ) 
	{
		case 1:
			//State = ReadGPIOData ( GPOUT_SHORTLOOPSW ) ;
			State = DpSetAFEHybridSelectCache[Select_Line-1] ;
			break ;
		case 2:
			//State = 0 ;
			State = DpSetAFEHybridSelectCache [Select_Line-1] ;
			break ;
		default:
			State = 0 ;
	}
	return State ;
}

/*********************************************************************************
    Function     : BOOL DpQueryAnnexPin ( void ) 
    Description  : This routine reads hardware circuitry to indicate annex mode
**********************************************************************************/
BOOL DpQueryAnnexPin ( void ) 
{
	#ifdef CONFIG_BD_RUSHMORE
		BOOL AnnexPinValue ;

		// Set to input then read
		SetGPIODir ( GPOUT_SHORTLOOPSW, GP_INPUT ) ;
		AnnexPinValue = ReadGPIOData ( GPOUT_SHORTLOOPSW ) ;

		// Restore to output, write out the same value that was read.
		SetGPIODir ( GPOUT_SHORTLOOPSW, GP_OUTPUT ) ;
		WriteGPIOData ( GPOUT_SHORTLOOPSW, AnnexPinValue ) ;

		return AnnexPinValue ;
	#else
		return TRUE ;
	#endif
}



/*********************************************************************************
    Function     : void DpSwitchHookStateEnq ( UINT8	 Select_Line, BOOLEAN State ) 
    Description  : This routine reads remote off-hook hardware circuitry
**********************************************************************************/
BOOL DpSwitchHookStateEnq ( void ) 
{
	return ( ReadGPIOData ( GPIN_OH_DET ) ) ;
}


#endif	//if defined ( CONFIG_CNXT_ADSL )  ||  defined ( CONFIG_CNXT_ADSL_MODULE ) 



/****************************************************************
 ****************************************************************/

/* DSL Rx */
void SetDslRxDataInd ( BOOL value ) 
{
   #if COMMON_TX_RX_INDICATION
	   WriteGPIOData ( GPOUT_TXD_LED, ! value ) ;
   #else
	   WriteGPIOData ( GPOUT_RXD_LED, ! value ) ;
   #endif
}

/****************************************************************
 ****************************************************************/

/* DSL Tx */
void SetDslTxDataInd ( BOOL value ) 
{
   WriteGPIOData ( GPOUT_TXD_LED, ! value ) ;
}


#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
EXPORT_SYMBOL ( DpResetWiring ) ;
EXPORT_SYMBOL ( FalconReset ) ;
EXPORT_SYMBOL ( FalconPowerDown ) ;
EXPORT_SYMBOL ( SetF2Interrupts ) ;
EXPORT_SYMBOL ( ADSL_CS_Init ) ;
EXPORT_SYMBOL ( ADSL_EnableIRQ1_2 ) ;
EXPORT_SYMBOL ( ADSL_DisableIRQ1_2 ) ;
EXPORT_SYMBOL ( DpSetAFEHybridSelect ) ;
EXPORT_SYMBOL ( DpQueryAnnexPin ) ;
EXPORT_SYMBOL ( DpSwitchHookStateEnq ) ;
EXPORT_SYMBOL ( FalconScanEnable ) ;
EXPORT_SYMBOL ( ADSL_DisableIRQ2 ) ;
EXPORT_SYMBOL ( ADSL_EnableIRQ2 ) ;
EXPORT_SYMBOL ( AFEReset ) ;
EXPORT_SYMBOL ( LinedriverPower ) ;
EXPORT_SYMBOL ( ReadFalconReset ) ;
EXPORT_SYMBOL ( ReadFalconPowerDown ) ;
EXPORT_SYMBOL ( ReadFalconScanEnable ) ;
EXPORT_SYMBOL ( ReadAFEReset ) ;
EXPORT_SYMBOL ( ReadLinedriverPower ) ;
EXPORT_SYMBOL ( DpReadAFEHybridSelect ) ;
#endif	//if defined ( CONFIG_CNXT_ADSL )  ||  defined ( CONFIG_CNXT_ADSL_MODULE ) 

#endif	//if defined ( CONFIG_CNXT_ADSL )  ||  defined ( CONFIG_CNXT_ADSL_MODULE ) || defined (CONFIG_CNXT_GSHDSL) || defined (CONFIG_CNXT_GSHDSL_MODULE)


