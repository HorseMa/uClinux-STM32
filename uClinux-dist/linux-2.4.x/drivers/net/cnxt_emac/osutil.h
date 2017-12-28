/****************************************************************************
*
*	Name:			osutil.h
*
*	Description:	This header file define all the OS dependent services
*					required by HPNALL and PHY
*
*	Copyright:		(c) 2000-2002 Conexant Systems Inc.
*					Personal Computing Division
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
*
****************************************************************************
*  $Author:   davidsdj  $
*  $Revision:   1.0  $
*  $Modtime:   Mar 19 2003 12:25:20  $
****************************************************************************/

#ifndef OSUTIL_H_
#define	OSUTIL_H_

#if defined(NDIS_MINIPORT_DRIVER)

#include <ndis.h>
//---------------------------------------------------------------
// NIDS Data Type
//---------------------------------------------------------------

// #define HANDLE		NDIS_HANDLE			// NDIS handle 
#define STATUS		NDIS_STATUS
#define SUCCESS		NT_SUCCESS		// ((NT_STATUS)(Status) >= 0)
#define FAILURE		NDIS_STATUS_FAILURE

#define H20_ALLOC_MEM(_pbuffer, _length) NdisAllocateMemoryWithTag( \
    (void *)(_pbuffer), \
    (_length), \
    'TXNC')

#define H20_FREE_MEM(_buffer,_length) NdisFreeMemory((_buffer), (_length), 0)
#define H20_ZERO_MEM(_buffer,_length) NdisZeroMemory((_buffer), (_length))

#ifndef  MOVE_MEMORY
#define MOVE_MEMORY(Destination,Source,Length)   \
                         \
    NdisMoveMemory((PVOID)(Destination),     \
               (PVOID)(Source),      \
               (ULONG)(Length)       \
              )              

#endif

#ifndef  ZERO_MEMORY
#define ZERO_MEMORY(_buffer,_length) NdisZeroMemory((_buffer), (_length))
#endif

#elif defined(LINUX_DRIVER)
#include <linux/slab.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/bsptypes.h>


// PORT
#define AcquireSpinLock( pLock, flags) spin_lock_irqsave( pLock, flags) 
#define ReleaseSpinLock( pLock, flags) spin_unlock_irqrestore( pLock, flags )

// #define HANDLE		NDIS_HANDLE			// NDIS handle 
//typedef int STATUS;
#define SUCCESS(Status)		((STATUS)(Status) != 0)
#define FAILURE		0

#define H20_ALLOC_MEM(_pbuffer, _length) *(_pbuffer) = (void *)kmalloc( \
    (_length), \
    GFP_KERNEL )

#define H20_FREE_MEM(_buffer,_length) kfree( _buffer ) 
#define H20_ZERO_MEM(_buffer,_length) memset((_buffer), 0, (_length))

#ifndef  MOVE_MEMORY
#define MOVE_MEMORY(Destination,Source,Length)   \
                         \
    memcpy((void *)(Destination),     \
               (void *)(Source),      \
               (ULONG)(Length)       \
              )              \

#endif

#ifndef  ZERO_MEMORY
#define ZERO_MEMORY(_buffer,_length) memset((_buffer), 0, (_length))
#endif

#elif defined(VXWORKS_DRIVER)
#include "vxwinc.h"

// #define HANDLE		NDIS_HANDLE			// NDIS handle 
// typedef int STATUS ;
#define SUCCESS(Status)		((STATUS)(Status) != 0)
#define FAILURE		0

#define H20_ALLOC_MEM(_pbuffer, _length) *(_pbuffer) = (void *)malloc((_length))

#define H20_FREE_MEM(_buffer,_length) free( (_buffer) ) 
#define H20_ZERO_MEM(_buffer,_length) memset((_buffer), 0, (_length))

#ifndef  MOVE_MEMORY
#define MOVE_MEMORY(Destination,Source,Length)   \
                         \
    memcpy((PVOID)(Destination),     \
               (PVOID)(Source),      \
               (ULONG)(Length)       \
              )              \

#endif

#ifndef  ZERO_MEMORY
#define ZERO_MEMORY(_buffer,_length) memset((_buffer), 0, (_length))
#endif
#endif	

#endif
