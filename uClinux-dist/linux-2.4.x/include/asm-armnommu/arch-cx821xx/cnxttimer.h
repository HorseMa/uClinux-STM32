/****************************************************************************
*
*	Name:			cnxttimer.h
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
*  $Revision:   1.3  $
*  $Modtime:   Jul 18 2003 11:01:22  $
****************************************************************************/

#ifndef CNXTTIMER_H
#define CNXTTIMER_H


// There are 4 hardware timers (at least for most network processors).
// The first one is dedicated for the Linux system clock and handled by special
// purpose code elsewhere. That leaves us 3 hardware timers for other purposes.
typedef enum
{
	HW_PROG_TIMER_LINUX,	// Reserved for Linux - note it here - not handled by cnxttimer.c
	HW_PROG_TIMER_GP,		// Used to generate general purpose timers
	HW_PROG_TIMER_SPARE_1,	// If you need this timer, rename it to reflect its usage (even if you create your own handlers)
	HW_PROG_TIMER_SPARE_2,	// If you need this timer, rename it to reflect its usage (even if you create your own handlers)

	NUM_PROG_HW_TIMERS
} HW_TIMER_T ;

#define ETMRNRDY	1	// Error - Timer: Not Ready
#define ETMRBUSY	2	// Error - Timer: Busy
#define ETMRIDLE	2	// Error - Timer: Idle

typedef void (*PTMR_CALLBACK) ( WORD ElapsedTime, void *pContext ) ;

int		CnxtTimerEnable	( HW_TIMER_T TimerIndex, PTMR_CALLBACK pCallback ) ;
int		CnxtTimerDisable( HW_TIMER_T TimerIndex ) ;
int		CnxtTimerStart	( HW_TIMER_T TimerIndex, WORD TimerValue, void *pContext ) ;
int		CnxtTimerStop	( HW_TIMER_T TimerIndex ) ;
LONG	CnxtTimerRead	( HW_TIMER_T TimerIndex ) ;


#endif	/* CNXTTIMER_H */

