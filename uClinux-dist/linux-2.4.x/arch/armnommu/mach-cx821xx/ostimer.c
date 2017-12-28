/****************************************************************************
*
*	Name:			ostimer.c
*
*	Description:	OS specific timing functions
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
*  $Modtime:   Jul 18 2003 14:03:40  $
****************************************************************************/

#include <linux/config.h>

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <asm/arch/bsptypes.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/OsTools.h>
#include <asm/arch/ostimer.h>
#include <asm/arch/gptimer.h>
//#include "cnxtirq.h"
//#include "gpio.h"
//#include "ListP.h"

static EVENT_HNDL taskdelayEventTbl[NUMBER_OF_GP_TIMERS];	// Array of all delay events
static int taskdelayPidTbl [NUMBER_OF_GP_TIMERS];			// Array of PIDs of each task delay
/******************************************************************************
FUNCTION:	sysClkRateGet
ABSTRACT:
DETAILS:
*******************************************************************************/
int sysClkRateGet ( void )
{
	return HZ ;
}	


/******************************************************************************
FUNCTION:	CnxtGetTime
ABSTRACT:	Returns the a running time value in milliseconds
DETAILS:	
*******************************************************************************/
DWORD CnxtGetTime ( void )
{
	return jiffies * (1000/HZ) ;
}	


/******************************************************************************
FUNCTION:	tickGet
ABSTRACT:
DETAILS:
*******************************************************************************/
ULONG tickGet ( void )
{
	return jiffies ;
}


/******************************************************************************
FUNCTION:	SysTimerDelay
ABSTRACT:
INPUTS:		UINT32 uMicroSec = number of microseconds to delay minus 1
OUTPUTS:	TRUE = delay succeeded,  FALSE = delay failed.
DETAILS:	Do not use in the task or interrupt context, this is for HW delay loop.
*******************************************************************************/
int sysTimerDelay ( UINT32 uMicroSec )
{

	/* Don't allow a delay any longer than 65535 useconds */
	if ( (uMicroSec > 0xFFFF) || (uMicroSec == 0) )
	{
		return FALSE ;
	}

	udelay ( uMicroSec ) ;
	return TRUE ;
}



/******************************************************************************
FUNCTION:	taskdelayCallback
ABSTRACT:	Callback routine that runs when delay timer expires
*******************************************************************************/
static void taskdelayCallback ( void *pContext )
{
	EVENT_HNDL	*pTaskDelayEvent = (EVENT_HNDL *) pContext ;

	SET_EVENT_FROM_ISR ( pTaskDelayEvent );
}



/******************************************************************************
FUNCTION:	TaskDelayConnect
ABSTRACT:	Connects the desired timer to the delay function
DETAILS:	You must use a different timer for each task.
RETURN:		TRUE = function connected
			FALSE = function not connected
*******************************************************************************/
int TaskDelayConnect
(
	WORD		TimerIndex
)
{
	// Initialize event
	INIT_EVENT ( &taskdelayEventTbl[TimerIndex] ) ; 

	// Associate PID with this task delay
	taskdelayPidTbl [TimerIndex] = current->pid ;

	// Connect timer and delay
	return GPTimerConnect ( TimerIndex, taskdelayCallback, &taskdelayEventTbl[TimerIndex] ) ;
}



/******************************************************************************
FUNCTION:	TaskDelayMsec
ABSTRACT:	
DETAILS:	If duration passed in is smaller than current system 
			timer resolution than by default it will be forced   
			to current resolution.
RETURN:
*******************************************************************************/
int TaskDelayMsec
(
	DWORD	TimeDuration
)
{
	WORD	TimerIndex ;

	// If the delay is zero then just give any other
	// waiting tasks a chance to run.
	if ( TimeDuration < 1 )
	{
		schedule ();
	}
	else
	{
		// find this task's TimerIndex
		for ( TimerIndex = 0 ; TimerIndex < NUMBER_OF_GP_TIMERS ; TimerIndex++ )
		{
			// if found
			if ( taskdelayPidTbl [TimerIndex] == current->pid )
			{
				break ;
			}
		}

		// if we have a GP timer to use
		if ( TimerIndex < NUMBER_OF_GP_TIMERS )
		{
			int Error ;

			// Reset the event before we start the timer that will set it.
			RESET_EVENT ( &taskdelayEventTbl[TimerIndex] ) ;

			// start gp timer
			Error = GPTimerStart ( TimerIndex,  (TimeDuration * 1000), FALSE ) ;
			if ( Error )
			{
				printk ( KERN_DEBUG "cnxttimer.c: General Purpose timer did not start.\n") ;
				GPTimerStop ( TimerIndex ) ; 
				return Error ;
			}
			
			// Wait on event to be set or timeout to expire
			if ( ! WAIT_EVENT ( &taskdelayEventTbl[TimerIndex], TimeDuration + 100 ) )
			{
				printk ( KERN_DEBUG "cnxttimer.c: Delay expired without event being signaled.\n") ;
				GPTimerStop ( TimerIndex ) ; 
			}
		}
		else // otherwise use OS
		{
			if ( TimeDuration < 1 )
			{
				schedule () ;
			}
			else
			{
				int NumTickDelay;

				// Add 999 to round up to the closest ticks
				NumTickDelay = ( ((TimeDuration * HZ) + 999) / 1000) ;

				// Add one to guarantee at least 1 tick delay
				if ( NumTickDelay < 1 )
				{
					NumTickDelay++ ;
				}

				current->state = TASK_INTERRUPTIBLE ;
				schedule_timeout ( NumTickDelay ) ;
			}
			
		}
	}

	return 0 ;
}


EXPORT_SYMBOL(sysClkRateGet);
EXPORT_SYMBOL(CnxtGetTime);
EXPORT_SYMBOL(tickGet);
EXPORT_SYMBOL(sysTimerDelay);
EXPORT_SYMBOL(TaskDelayConnect);
EXPORT_SYMBOL(TaskDelayMsec);
