/****************************************************************************
*
*	Name:			gptimer.h
*
*	Description:	
*
*	Copyright:		(c) 2002, 2004 Conexant Systems Inc.
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
*  $Modtime:   Jul 18 2003 14:04:30  $
****************************************************************************/

#ifndef GPTIMER_H
#define GPTIMER_H

//#define NUMBER_OF_GP_TIMERS 

typedef enum GP_TIMER_E
{
	GP_TIMER_DMT_HI,
	GP_TIMER_DMT_MED,
	GP_TIMER_DMT_LO,
	GP_TIMER_DMT_DT, 
	GP_TIMER_DMT_LM,
	GP_TIMER_DMT_LED,
	GP_TIMER_BSP_LED,
	GP_TIMER_VOP,

	NUMBER_OF_GP_TIMERS
} GP_TIMER_E ;

// Callback Function Prototype
typedef void (*PGPCALLBACK) ( void *pContext ) ;

void GPTimerInit
(
	void
) ;

int GPTimerConnect
(
	WORD		TimerIndex,
	PGPCALLBACK	pCallback,
	void		*pContext
) ;

int GPTimerDisconnect
(
	WORD		TimerIndex
) ;

int GPTimerStart
(
	WORD		TimerIndex,
	LONG		Period,
	BOOLEAN		Repeats
) ;

int GPTimerStop
(
	WORD TimerIndex
) ;

#endif // GPTIMER_H
