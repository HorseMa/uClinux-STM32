/****************************************************************************
*
*	Name:			phytypes.h
*
*	Description:	PHY driver OS/compiler mapping of simple
*					BYTE/WORD/DWORD types.
*
*	Copyright:		(c) 1997-2002 Conexant Systems Inc.
*					Personal Computing Division
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
*  $Author:   davidsdj  $
*  $Revision:   1.0  $
*  $Modtime:   Mar 19 2003 12:25:22  $
****************************************************************************/

#ifndef _PHYTYPES_H_
#define _PHYTYPES_H_

/*----------------------------------------------------------------------
 * OS and compiler-specific types here, based on DRV_OS complier switch
 *--------------------------------------------------------------------*/
#define NDIS            1
#define NOVELL_CHSM     2
#define DRV_OS_UNUSED   3       /* was "SCO" ... see SCO_*, below */
#define SOLARIS         4
#define LINUX           5
#define PKTDRV          6
#define DOS             7
#define WIN32_DIAG      8       /* RSS' Win95/NT diagnostic tool */
#define SCO_MDI         9
#define SCO_LLI        10
#define SCO_DLPI       11
#define SCO_GEMINI     12
#define VXWORKS        13		/* VxWorks Tornado RTOS */
#define NO_OS          14		/* No OS Services */

#define DRV_OS	LINUX

#ifndef DRV_OS
#define DRV_OS  DOS     /* THE lowest common denominator :-/ */
#endif

#if (DRV_OS == NDIS)
#include <ndis.h>
#if !defined(_TYPDEF_H_)
typedef ULONG DWORD;
typedef USHORT WORD;
typedef UCHAR BYTE;
#endif

#elif (DRV_OS == NOVELL_CHSM)
#include <odi.h>
#include <odi_nbi.h>
#include <cmsm.h>

#ifndef BYTE
typedef UINT8           BYTE;
#endif

#ifndef WORD
typedef UINT16          WORD;
#endif

#ifndef DWORD
/* Warning:  <odi.h> UINT32 == unsigned int, not unsigned long */
typedef unsigned long   DWORD;
#endif

#elif (DRV_OS == SCO_MDI)
typedef int             BOOLEAN;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;

#elif (DRV_OS == SCO_LLI)
typedef int             BOOLEAN;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;

#elif (DRV_OS == SCO_DLPI)
typedef int             BOOLEAN;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;

#elif (DRV_OS == SCO_GEMINI)
typedef int             BOOLEAN;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;

#elif (DRV_OS == SOLARIS)
#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
typedef int             BOOLEAN;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;

#elif (DRV_OS == LINUX)
#include <linux/delay.h>
/* #include <asm/delay.h> */
#include <asm/io.h>
#include <asm/arch/bsptypes.h>

#if !defined(_TYPDEF_H_) 
	#if 0
		typedef int             BOOLEAN;
		typedef unsigned char   BYTE;
		typedef unsigned short  WORD;
		typedef unsigned long   DWORD;
	#endif
#endif

#elif (DRV_OS == VXWORKS)	/* VxWorks Tornado RTOS */
#if !defined(_TYPDEF_H_) && !defined(BSPTYPES_H)
typedef int             BOOLEAN;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;
#endif

#elif (DRV_OS == PKTDRV)
#include <pktdrv???.h>

#elif (DRV_OS == WIN32_DIAG)
#include <windows.h>
#include "RockDLL.h"    /* NIC diagnostics DLL */

#else                   /* DOS -- Roll our own basic types */
#if !defined(_TYPDEF_H_)
	#if 0
		typedef int             BOOLEAN;
		typedef unsigned char   BYTE;
		typedef unsigned short  WORD;
		typedef unsigned long   DWORD;
	#endif
#endif

#endif
/*--------------------------------------------------------------------*/

/* Be sure the essentials are in place */
#ifndef TRUE
#define TRUE            1
#endif
#ifndef FALSE
#define FALSE           0
#endif
#ifndef NULL
#define NULL            ((void *) 0)
#endif

#endif /* _PHYTYPES_H_ */
