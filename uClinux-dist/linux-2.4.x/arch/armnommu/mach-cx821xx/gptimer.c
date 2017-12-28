/****************************************************************************
*
*	Name:			gptimer.c
*
*	Description:	General Purpose timing functions
*
*	Copyright:		(c) 2003 Conexant Systems Inc.
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
*  $Revision:   1.0  $
*  $Modtime:   Jul 18 2003 14:26:10  $
****************************************************************************/


#include <linux/config.h>

#include <linux/module.h>
#include <linux/spinlock.h>

#include <asm/arch/bsptypes.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/cnxttimer.h>
#include <asm/arch/gptimer.h>
#include <asm/arch/cnxtirq.h>
#include <asm/arch/list.h>

#define MIN_TIMER_RELOAD 1		// Minimum value we ever allow for the timer reload.
								// Too low a value and we might miss interrupts due to latency.
								// The size of this value will cause some timers to expire late
								// by this amount, although repeating timers will NOT
								// accumulate this delay.
#define MAX_TIMER_RELOAD 0xFFFF	// Maximum value we allow for reload due to hardware timer limit


typedef struct GP_TIMER_T
{
	LIST_LINK_T	 Link ;			// links for listing
	WORD		 Index ;		// Index
	BOOLEAN		 Running ;		// Flag to indicate whether timer is currently running
	PGPCALLBACK	 pCallback;		// Routine to call when timer expires
	void		*pContext ;		// Pointer to context passed to Callback
	LONG		 Delta ;		// Delta time remaining
	LONG		 Period ;		// Period of timer
	BOOLEAN		 Repeats;		// TRUE = periodic, FALSE = one_shot
} GP_TIMER_T;

static GP_TIMER_T	GPTimers[NUMBER_OF_GP_TIMERS];	// Array of all timers
static LIST_LINK_T	GPTimerList ;					// Anchor for ordered chain of running timers
static LIST_LINK_T	GPCallbackList ;				// Anchor for ordered chain of callbacks
static spinlock_t	GPTimerLock ;					// Spinlock for critical areas


/******************************************************************************
FUNCTION:	GPTimerInit
ABSTRACT:	Initializes structures that support gp timers
RETURN:		
*******************************************************************************/
#if 0
GP_TIMER_T LINK_TO_TIMER ( LIST_LINK_T pLink ) {}
#endif
#define LINK_TO_TIMER(pLink) ( (GP_TIMER_T *) ( pLink - offsetof ( GP_TIMER_T, Link ) ) )



/******************************************************************************
FUNCTION:	GPTimerInit
ABSTRACT:	Initializes structures that support gp timers
RETURN:		
*******************************************************************************/
void GPTimerInit
(
	void
)
{
	int Index ;

	printk ( "Conexant General Purpose timers initialized\n" ) ;

	// Initialize our list of chained timers and callbacks(empty to begin with)
	ListInit ( &GPTimerList ) ;
	ListInit ( &GPCallbackList ) ;

	// Initialize our lock
	spin_lock_init ( &GPTimerLock ) ;

	// initialize index member of each timer
	for ( Index = 0  ; Index < NUMBER_OF_GP_TIMERS ; Index++ )
	{
		GPTimers [Index].Index = Index ;
	}

	// Enable the programmable hardware timer
	{
		void GPTimerExpired
		(
			WORD ElapsedTime,
			void *pContext
		) ;

		CnxtTimerEnable ( HW_PROG_TIMER_GP, GPTimerExpired ) ;
	}
}



/******************************************************************************
FUNCTION:	GPTimerConnect
ABSTRACT:	Connects a function to the timer that will be called when the timer
			expires
RETURN:		TRUE = function connected
			FALSE = function not connected
*******************************************************************************/
int GPTimerConnect
(
	WORD		TimerIndex,
	PGPCALLBACK	pCallback,
	void		*pContext
)
{
	GP_TIMER_T	*pGPTimer = &GPTimers[TimerIndex];
	
	// Is there already a connected callback?
	if ( pGPTimer->pCallback != NULL )
	{
		return -ETMRBUSY;
	}
	else
	{
		pGPTimer->pCallback	= pCallback;
		pGPTimer->pContext	= pContext;
		pGPTimer->Running	= FALSE ;

		return 0;
	}
}	 



/******************************************************************************
FUNCTION:	GPTimerDisconnect
ABSTRACT:	Disconnects a callback from the timer.
RETURN:		TRUE = function disconnected
			FALSE = function not disconnected
*******************************************************************************/
int GPTimerDisconnect
(
	WORD		TimerIndex
)
{
	BOOLEAN		Return ;
	GP_TIMER_T	*pGPTimer = &GPTimers[TimerIndex];
	
	// Is this timer already disconnected?
	if ( pGPTimer->pCallback == NULL )
	{
		Return = -ETMRIDLE ;
	}
	else
	{
		// if this timer is still running
		if ( pGPTimer->Running )
		{
			// stop it
			GPTimerStop ( TimerIndex ) ;
		}

		// Clear the connection between timer and callback
		pGPTimer->pCallback	= NULL ;

		// clear timer members to lessen confusion
		pGPTimer->pContext	= NULL ;
		pGPTimer->Delta		= 0 ;
		pGPTimer->Period	= 0 ;
		pGPTimer->Repeats	= FALSE ;

		Return = 0 ;
	}

	// return
	return Return ;
}



/******************************************************************************
FUNCTION:	gpTimerStartIrq
ABSTRACT:	Starts the general purpose timer without spinlocks
DETAILS:	This is used by the GPTimerExpired when it handles a repeating timer
			because it already has the spin lock and doesn't want to release it
			to restart a repeating timer.
*******************************************************************************/
static int gpTimerStartIrq
(
	WORD		TimerIndex,
	LONG		Period,
	BOOLEAN		Repeats,		// If TRUE, we must restart this timer automatically after it expires
	BOOLEAN		Restart			// If TRUE, this is a restarted repeating timer
)
{
	LIST_LINK_T *pLink ;
	GP_TIMER_T	*pGPTimer = &GPTimers[TimerIndex];
	LONG		TotalDeltas = 0 ;
	BOOLEAN		Return ;
	
	// Is this timer unconnected?
	if ( pGPTimer->pCallback == NULL )
	{
		printk ( "%s: GPTimer started without being connected\n", __FILE__ ) ;
		Return = -ETMRNRDY ;
	}

	// Is this timer already running
	else if ( pGPTimer->Running )
	{
		printk ( "%s: GPTimer started while already running\n", __FILE__ ) ;
		Return = -ETMRBUSY ;
	}
	else
	{
		GP_TIMER_T	*pChainedGPTimer = NULL ;

		// Save the args
		pGPTimer->Period = Period ;
		pGPTimer->Repeats = Repeats ;

		// If this is an automatic restart of a repeating timer
		if ( Restart )
		{
			// Add existing HW timer value to our new timer's delay in order
			// that the time that has already been run off the HW timer is
			// accounted for
			Period += CnxtTimerRead ( HW_PROG_TIMER_GP ) ;
		}

		// search the timer on each position of the chain of timers for the
		// point where this new one should be inserted.
		for
		(
			pLink = ListNext ( &GPTimerList ) ;	// beginning of timer chain
			pLink != &GPTimerList ;				// do until we reach end of chain
			pLink = ListNext (pLink)			// advance search to next timer on chain
		)
		{
			// Convert new search position link to timer
			pChainedGPTimer = LINK_TO_TIMER ( pLink ) ;

			// if this new timer's period is smaller than search position's remaining time (cumulative)
			if ( Period < ( TotalDeltas + pChainedGPTimer->Delta ) )
			{
				// done
				break ;
			}
			else
			{
				// add search position's timer to the total deltas to this point
				TotalDeltas += pChainedGPTimer->Delta ;
			}

			// Next search position may be the end of the list (an anchor)
			// rather than a timer. Clear pointer to search position timer
			// since it is not yet valid and may not be.
			pChainedGPTimer = NULL ;
		}

		// add this new timer prior to the final search position
		// (even if the final search position is the anchor)
		ListInsertPrev ( pLink, &pGPTimer->Link ) ;

		// Mark the new timer as running
		pGPTimer->Running = TRUE ;

		// Compute the delta of the new timer based on the total deltas 
		// of all preceeding timers
		pGPTimer->Delta = Period - TotalDeltas ;

		// if the search position is a timer and not an anchor
		if ( pChainedGPTimer )
		{
			// adjust period of the search position's timer by new timer's delta
			pChainedGPTimer->Delta -= pGPTimer->Delta ;
		}

		// If our new timer is the head of the chained timers
		if ( ListNext ( &GPTimerList ) == &pGPTimer->Link )
		{
			LONG CnxtTimer ;	// Snapshot of the HW timer

			// Read the hardware timer
			CnxtTimer = CnxtTimerRead ( HW_PROG_TIMER_GP ) ;

			// We are about to reload the HW timer and that will discard
			// any time built up in the HW timer. Reflect this discarded
			// time in the new timer's delta
			pGPTimer->Delta -= CnxtTimer ;

			// If our new timer has already expired
			if ( pGPTimer->Delta <= CnxtTimer )
			{
				// Setup timer to expire "immediately" so our expired
				// timer will be serviced.
				CnxtTimerStart ( HW_PROG_TIMER_GP, MIN_TIMER_RELOAD, (void *)(DWORD)pGPTimer->Index ) ;
			}
			else
			{
				LONG Timeout ;		// Make it long and signed to account for negative values

				// New HW timeout is new timer's new delta
				Timeout = pGPTimer->Delta ;

				// MIN_TIMER_RELOAD <= Timeout <= MAX_TIMER_RELOAD
				Timeout = max ( MIN_TIMER_RELOAD, Timeout ) ;
				Timeout = min ( Timeout, MAX_TIMER_RELOAD ) ;

				// Set HW timer
				CnxtTimerStart ( HW_PROG_TIMER_GP, (WORD)Timeout, (void *)(DWORD)pGPTimer->Index ) ;
			}
		}

		// Success	
		Return = 0;
	}
	
	return Return ;
}



/******************************************************************************
FUNCTION:	GPTimerStart
ABSTRACT:	Starts the general purpose timer
DETAILS:
*******************************************************************************/
int GPTimerStart
(
	WORD		TimerIndex,
	LONG		Period,
	BOOLEAN		Repeats
)
{
	unsigned long int	Flags ;
	BOOLEAN				Return ;
	
	// Begin Critical Area
	spin_lock_irqsave  ( &GPTimerLock, Flags ) ;

	Return = gpTimerStartIrq ( TimerIndex, Period, Repeats, FALSE ) ;

	// End Critical Area
	spin_unlock_irqrestore  ( &GPTimerLock, Flags ) ;

	// Return
	return Return ;
}



/******************************************************************************
FUNCTION:	GPTimerStop
ABSTRACT:	Stops the general purpose timer
DETAILS:
*******************************************************************************/
int GPTimerStop
(
	WORD TimerIndex
)
{
	GP_TIMER_T	*pGPTimer = &GPTimers[TimerIndex];
	BOOLEAN		Return ;
	
	// Is this timer disconnected?
	if ( pGPTimer->pCallback == NULL )
	{
		// Failure
		Return = -ETMRNRDY;
	}
	else
	{
		unsigned long int Flags ;
		GP_TIMER_T	*pChainedGPTimer ;

		// Begin Critical Area
		spin_lock_irqsave  ( &GPTimerLock, Flags ) ;

			// Get pointer to following timer
			pChainedGPTimer = LINK_TO_TIMER ( ListNext ( &pGPTimer->Link ) ) ;

			// remove the desired timer from the chain
			ListRemove ( &pGPTimer->Link ) ;
			pGPTimer->Running = FALSE ;

			// if the following timer is a timer and not an anchor
			if ( &pChainedGPTimer->Link != &GPTimerList )
			{
				// Add the removed timer's delta period into following chained
				// timer's delta period.
				pChainedGPTimer->Delta += pGPTimer->Delta ;
			}

			// If following timer is now the head of the chained timers
			if ( ListNext ( &GPTimerList ) == &pChainedGPTimer->Link )
			{
				LONG Timeout ;		// Make it long and signed to account for negative values
				LONG CnxtTimer ;	// Snapshot of the HW timer

				// Read the hardware timer
				CnxtTimer = CnxtTimerRead ( HW_PROG_TIMER_GP ) ;

				// New HW timer timeout is HW timer's time left (Old first timer - HW Timer)
				// plus new head timer's delta
				Timeout = ( pGPTimer->Delta - CnxtTimer ) + pChainedGPTimer->Delta ;

				// MIN_TIMER_RELOAD <= Timeout <= MAX_TIMER_RELOAD
				Timeout = max ( MIN_TIMER_RELOAD, Timeout ) ;
				Timeout = min ( Timeout, MAX_TIMER_RELOAD ) ;

				// Set HW timer
				CnxtTimerStart ( HW_PROG_TIMER_GP, (WORD) Timeout, (void *)(DWORD)pChainedGPTimer->Index ) ;
			}

		// End Critical Area
		spin_unlock_irqrestore  ( &GPTimerLock, Flags ) ;

		// Success	
		Return = 0;
	}

	// Return
	return Return ;
}



/******************************************************************************
FUNCTION:	GPTimerExpired
ABSTRACT:	Called by ISR when hardware timer expires
DETAILS:	If the ISR is delayed by interrupt latency, the timer might have
			expired again (it auto reloads). The ISR detects this and includes
			this time in the Elapsed time and so the value can exceed the 16 bit
			capacity of the time and hence we use a LONG.
*******************************************************************************/
void GPTimerExpired
(
	WORD ElapsedTime,
	void *pContext
)
{
	WORD		TimerIndex ;
	LIST_LINK_T *pLink ;
	LIST_LINK_T *pNext ;
	GP_TIMER_T	*pGPTimer ;
	GP_TIMER_T	*pChainedGPTimer = NULL ;

	TimerIndex = (WORD)(DWORD)pContext ;
	pGPTimer = &GPTimers[TimerIndex];

	// Begin Critical Area
	spin_lock ( &GPTimerLock ) ;

		// search the timer on each position of the chain of timers for the
		// point where this new one should be inserted.
		for
		(
			pLink = ListNext ( &GPTimerList ) ;	// beginning of timer chain
			pLink != &GPTimerList ;				// do until we reach end of chain
			pLink = pNext						// advance search to next timer on chain
		)
		{
			// Get snapshot of next timer on list
			pNext = ListNext (pLink) ;

			// Convert link to timer
			pChainedGPTimer = LINK_TO_TIMER ( pLink ) ;

			// if Elapsed time < search position's timer
			if ( ElapsedTime < pChainedGPTimer->Delta )
			{
				// Subtract ElapsedTime from this delta
				pChainedGPTimer->Delta -= ElapsedTime ;

				// done
				break ;
			}
			else
			{
				// Subtrace the timer's delta from ElapseTime and zero out timer's delta
				ElapsedTime -= pChainedGPTimer->Delta ;
				pChainedGPTimer->Delta = 0 ;

				// Remove timer from chained list
				ListRemove ( pLink ) ;

				// Put timer on chained list of callbacks to do before we return
				ListEnqueue ( &GPCallbackList, pLink ) ;

				// Mark timer as not running
				pChainedGPTimer->Running = FALSE ;
			}

			// Next search position may be the end of the list (an anchor)
			// rather than a timer. Clear pointer to timer since it is not yet
			// valid and may not be.
			pChainedGPTimer = NULL ;
		}

		if ( ListIsEmpty ( &GPTimerList ) )
		{
			// stop HW timer
			CnxtTimerStop ( HW_PROG_TIMER_GP ) ;
		}
		else
		{
			LONG Timeout ;

			// Look at first chained timer
			pLink = ListNext ( &GPTimerList ) ;

			// Convert link to timer
			pChainedGPTimer = LINK_TO_TIMER ( pLink ) ;

			// HW Timer timeout will be first chained timer's delta
			Timeout = pChainedGPTimer->Delta ;

			// MIN_TIMER_RELOAD <= Timeout <= MAX_TIMER_RELOAD
			Timeout = max ( MIN_TIMER_RELOAD, Timeout ) ;
			Timeout = min ( Timeout, MAX_TIMER_RELOAD ) ;

			// Start HW timer
			CnxtTimerStart ( HW_PROG_TIMER_GP, (WORD)Timeout, (void *)(DWORD)pChainedGPTimer->Index ) ;
		}

	// End Critical Area
	spin_unlock ( &GPTimerLock ) ;

	// search the timer on each position of the chain of timers for the
	// point where this new one should be inserted.
	for
	(
		pLink = ListNext ( &GPCallbackList ) ;	// beginning of callback chain
		pLink != &GPCallbackList ;					// do until we reach end of chain
		pLink = pNext							// advance search to next callback on chain
	)
	{
		// Get snapshot of next timer on list
		pNext = ListNext (pLink) ;

		// Remove timer from chained list
		ListRemove ( pLink ) ;

		// Convert link to timer
		pChainedGPTimer = LINK_TO_TIMER ( pLink ) ;

		// Call the timer's callback function
		pChainedGPTimer->pCallback ( pChainedGPTimer->pContext ) ;

		// if timer is a repeating timer
		if ( pChainedGPTimer->Repeats )
		{
			LONG Timeout ;

			Timeout = pChainedGPTimer->Period + ElapsedTime + CnxtTimerRead ( HW_PROG_TIMER_GP ) ;
			gpTimerStartIrq ( pChainedGPTimer->Index, Timeout, TRUE, FALSE ) ;
		}
	}
}



EXPORT_SYMBOL(GPTimerConnect);
EXPORT_SYMBOL(GPTimerDisconnect);
EXPORT_SYMBOL(GPTimerStart);
EXPORT_SYMBOL(GPTimerStop);
