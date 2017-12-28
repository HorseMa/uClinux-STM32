/****************************************************************************
*
*	Name:			cnxttimer.c
*
*	Description:	CNXT timer driver
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
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

****************************************************************************
*  $Author:   lewisrc  $
*  $Revision:   1.3  $
*  $Modtime:   Jul 18 2003 13:29:16  $
****************************************************************************/

/*
DESCRIPTION
This library contains routines to manipulate the timer functions on a 
CNXT timer. This file implements low level timer hardware access.

There are 4 programmable timers available with a range up to 65ms.
The timers are based on 16-bit counters that increment at a 1.0MHz rate.
This gives a resolution of 1 microsecond.  The third timer is also used
as a system watchdog.  If the interrupt for this timer is not cleared
before the next interrupt event a reset occurs.

The timers increment from 0 up to a limit value.  The limit value is
is programmed into the TM_LMT1 register for timer 1, TM_LMT2 for timer 2
and TM_LMT3 for timer 3.  When the timer reaches the limit value, it resets
back to zero and sets a corresponding interrupt.  If TM_LMT is set to zero,
then TM_CNT stays reset and never causes an interrupt.  TM_CNT is reset to
zero whenever the TM_LMT register is written to.

The timer is loaded by writing to TM_LMT register, and then, if
enabled, the timer register TM_CNT will count up to the limit value set 
in TM_LMT. 

At any time, the current timer value may be read from the TM_CNT register.


REGISTERS:

Note: x used in for example, TM_CNT(x) represents timers 1,2,3,4  There
are a total of 8 timer registers for the 4 timers.

TM_CNT(x): (read only)  Current count value of the timer.  The timer
                        increments every 1 uS from 0 to the limit
                        value, then resets to 0 and counts again. TM_CNT
                        is reset whenever TM_LMT is written.

TM_LMT(x): (read/write) When the current count value of the timer reaches
                        the limit value, an interrupt INT_TM is set. 
                        the periodic timer interrupt event rate is = 
                        1 MHz / (TM_LMT + 1).  If TM_LMT is set to 0, 
                        TM_CNT remains reset.  TM_CNT is reset whenever
                        TM_LMT is written.


BSP
Apart from defining such macros described above as are needed, the BSP
will need to connect the interrupt handlers (typically in sysHwInit2()).

SEE ALSO:
.I "CX82100 Silicon System Design Engineering Specification"
*/

#include <linux/config.h>

#include <linux/sched.h>
#include <asm/arch/irq.h>

#include <asm/arch/bsptypes.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/cnxttimer.h>
#include <asm/arch/gptimer.h>
#include <asm/arch/cnxtirq.h>

/* register definitions */
#ifdef DEVICE_YELLOWSTONE
	#define TIMER_BASE					0x00601400
	#define HW_TIMER_LIMIT_BASE			(TIMER_BASE + 0x108)
	#define HW_TIMER_COUNT_BASE			(TIMER_BASE + 0x10C)
	#define HW_TIMER_REG_OFFSET			0xC
#else
	#define HW_TIMER_COUNT_BASE			0x00350020
	#define HW_TIMER_LIMIT_BASE			(HW_TIMER_COUNT_BASE + 0x10)
	#define HW_TIMER_REG_OFFSET			4
#endif

#define STOP 			0

typedef void (* P_TIMER_HANDLER) ( WORD ElapsedTime, void *pContext ) ;

typedef struct
{
	HW_TIMER_T		 Index ;			// Quick reference to our index
	BOOLEAN			 Enabled ;			// flag
	PTMR_CALLBACK	 pCallback ;		// Callback Handler
	void			*pCallbackContext ;	// arg to callback
	WORD			 TimerLimit ;		// cache of readonly timer limit

	// Precomputed hardware constants
	DWORD			 TimerStatus ;		// INT_TM1 etc.
	DWORD			 TimerIrq ; 		// INT_LVL_TIMER_1 etc
	DWORD volatile	*pTimerLimit ;		// Address of hardware timer's limit
	DWORD volatile	*pTimerCount ;		// Address of hardware timer's count
} PROG_HW_TIMER ;

PROG_HW_TIMER TimerContext [NUM_PROG_HW_TIMERS] ;

/******************************************************************************
FUNCTION:	CnxtTimerEnable
ABSTRACT:	Enables programmable timer
DETAILS:
*******************************************************************************/
int CnxtTimerEnable ( HW_TIMER_T TimerIndex, PTMR_CALLBACK pCallback )
{
	PROG_HW_TIMER *pTimer = &TimerContext[TimerIndex] ;
	int Error ;

	void CnxtTimerIsr ( int irq, void *dev_id, struct pt_regs * regs ) ;

	// precompute constants
	pTimer->pTimerLimit = (DWORD *)(((char *)HW_TIMER_LIMIT_BASE) + TimerIndex * HW_TIMER_REG_OFFSET ) ;
	pTimer->pTimerCount = (DWORD *)(((char *)HW_TIMER_COUNT_BASE) + TimerIndex * HW_TIMER_REG_OFFSET ) ;
	pTimer->TimerStatus = INT_TM1 << TimerIndex ;
	pTimer->TimerIrq	= INT_LVL_TIMER_1 + TimerIndex ;
	pTimer->pCallback	= pCallback ;

	// Stop the timer
	*pTimer->pTimerLimit = STOP ;

	// Clear out the timer interrupt
	* ( (UINT32 *) INT_STAT) = pTimer->TimerStatus ;

	// Register IRQ and enable it
	Error = request_irq ( pTimer->TimerIrq, CnxtTimerIsr, 0, "Timer ISR", pTimer ) ;
	if ( Error )
	{
		printk ( "Conexant Hardware Timer IRQ enable failed. Error=%d\n", Error ) ;
		return Error ;
	} 
	cnxt_unmask_irq ( pTimer->TimerIrq );

	printk ( "Conexant hardware timer %d enabled\n", TimerIndex+1 ) ;

	// Note enabled
	pTimer->Enabled = TRUE ;

	return 0 ;
}



/******************************************************************************
FUNCTION:	CnxtTimerDisable
ABSTRACT:	Disables programmable timer
DETAILS:
*******************************************************************************/
int CnxtTimerDisable ( HW_TIMER_T TimerIndex )
{
	PROG_HW_TIMER *pTimer = &TimerContext[TimerIndex] ;

	printk ( "Conexant hardware timer %d disabled\n", TimerIndex ) ;

	// disable & free interrupt
	cnxt_mask_irq ( pTimer->TimerIrq );
	free_irq ( pTimer->TimerIrq, pTimer ) ;

	// Note disabled
	pTimer->Enabled = FALSE ;

	return 0 ;
}



/******************************************************************************
FUNCTION:	CnxtTimerIsr
ABSTRACT:	Cnxt Timer Interrupt Service Routine
DETAILS:
*******************************************************************************/
void CnxtTimerIsr ( int irq, void *dev_id, struct pt_regs * regs )
{
	PROG_HW_TIMER *pTimer = (PROG_HW_TIMER *) dev_id ;
 
	// Clear out the timer2 interrupt
	* ( (UINT32 *) INT_STAT) = pTimer->TimerStatus ;

	// Notify timer handler of how much time has elapsed (namely our timer limit value)
	pTimer->pCallback ( pTimer->TimerLimit, pTimer->pCallbackContext ) ;
}	




/******************************************************************************
FUNCTION:	CnxtTimerStart
ABSTRACT:	Starts timer running
DETAILS:	Starts the timer from 0 to the given TimerValue. The timer automatically
			resets to 0 and begins incrementing again when the value reaches
			the limit TimerValue.
*******************************************************************************/
int CnxtTimerStart ( HW_TIMER_T TimerIndex, WORD TimerValue, void *pContext )
{
	PROG_HW_TIMER *pTimer = &TimerContext[TimerIndex] ;

	if ( ! pTimer->Enabled )
	{
		printk ( "Tried to start disabled Conexant timer!\n" ) ;
		return -ETMRNRDY ;
	}

	// Start the timer with the desired timeout
	*pTimer->pTimerLimit = TimerValue ;

	// Save the time that will be expired when timer ISR runs
	pTimer->TimerLimit = TimerValue ;

	// Save the context
	pTimer->pCallbackContext = pContext ;


	return 0 ;
}



/******************************************************************************
FUNCTION:	CnxtTimerStop
ABSTRACT:	Stops timer
DETAILS:	
*******************************************************************************/
int CnxtTimerStop ( HW_TIMER_T TimerIndex )
{
	PROG_HW_TIMER *pTimer = &TimerContext[TimerIndex] ;

	if ( ! pTimer->Enabled )
	{
		printk ( "Tried to stop disabled Conexant timer!\n" ) ;
		return -ETMRNRDY ;
	}

	// Stop the timer
	*pTimer->pTimerLimit = STOP ;

	// Clear out the timer interrupt
	* ( (UINT32 *) INT_STAT) = pTimer->TimerStatus ;

	return 0 ;
}



/******************************************************************************
FUNCTION:	CnxtTimerRead
ABSTRACT:	Reads the running timer
DETAILS:	If timer has expired and reloaded, the returned time will reflect
			this and thus may exceed the timer limit.
*******************************************************************************/
LONG CnxtTimerRead ( HW_TIMER_T TimerIndex )
{
	PROG_HW_TIMER *pTimer = &TimerContext[TimerIndex] ;
	DWORD TimerValue ;

	// If we have an interrupt pending
	if ( * ( (UINT32 *) INT_STAT) & pTimer->TimerStatus )
	{
		// Time has restarted. Use the full limit value + current value
		TimerValue = *pTimer->pTimerCount + *pTimer->pTimerLimit ;
	}
	else
	{
		// Return current timer value
		TimerValue = *pTimer->pTimerCount ;
	}

	return TimerValue ;
}
