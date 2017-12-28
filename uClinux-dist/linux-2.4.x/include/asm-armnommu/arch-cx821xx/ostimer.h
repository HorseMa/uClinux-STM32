/****************************************************************************
*
*	Name:			ostimer.h
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
*  $Modtime:   Jul 18 2003 09:08:22  $
****************************************************************************/

#ifndef OSTIMER_H
#define OSTIMER_H

int		sysClkRateGet	( void ) ;
DWORD	CnxtGetTime		( void ) ;
ULONG	tickGet			( void ) ;
int		sysTimerDelay	( UINT32 uMicroSec ) ;
int		TaskDelayConnect( WORD TimerIndex ) ;
int		TaskDelayMsec	( DWORD TimeDuration ) ;


#endif // OSTIMER_H
