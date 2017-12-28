/****************************************************************************
*
*	Name:			mii.h
*
*	Description:	The 100Base-T serial Media Independent Interface (MII),
*					which defines OS- and adapter-dependent functions imported by
*  					the PHY driver.  The accompanying "mii.c" module must be
*					tailored to	adhere to the implementation-dependent environment.
*
*	Copyright:		(c) 1997-2002 Conexant Systems Inc.
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

#ifndef _MII_H_
#define _MII_H_

#include "phytypes.h"


typedef struct def_MIIOBJ
{
	DWORD	 adapter;	// beginning of MAC IO address
   DWORD  mii_id;		// not use now 
	WORD   mii_addr;   // Nominal value/usage here 
	BOOLEAN   preamble_suppression;
#ifdef P51
	BOOLEAN P51_HLAN;	// the current mac is P51 HLAN
#endif
} MIIOBJ ;


/*
 * Special "adapter" handle for 32-bit Novell ODI drivers.  Most
 * other drivers assume that the phy->adapter value is simply the
 * starting I/O port (or memory-mapped) address of the MAC (or repeater)
 * controller device.
 */
typedef struct {
    void       *dev_addr;       /* MAC's I/O or memory base address */
    void       *bus_tag;        /* CMSMSearchAdapter() busTag value */
} PHY_ADAPTER;


/*
 * MII_SET_BIT, MII_SET_MASK(), MII_CLR_BIT(), MII_CLR_MASK()
 *
 * Macros which set or clear one or more MII register bits.  In DOS
 * mode, a few hundred bytes can be saved by calling MiiSetBits()
 * and MiiClearBits() instead of the in-line MiiRead() and MiiWrite()
 * calls, as these macros are used extensively.
 */
#if (DRV_OS == DOS)     /* ########################################## */

void             MiiSetBits(MIIOBJ *phy, BYTE reg_addr, WORD bits);
void             MiiClrBits(MIIOBJ *phy, BYTE reg_addr, WORD bits);
#define MII_SET_MASK    MiiSetBits
#define MII_CLR_MASK    MiiClrBits

#else                   /* ########################################## */

#define MII_SET_MASK(phy, regaddr, mask)        \
    MiiWrite((phy),                             \
             (BYTE) (regaddr),                  \
             (WORD) (MiiRead((phy), (BYTE) (regaddr)) | (mask)))
#define MII_CLR_MASK(phy, regaddr, mask)        \
    MiiWrite((phy),                             \
             (BYTE) (regaddr),                  \
             (WORD) (MiiRead((phy), (BYTE) (regaddr)) & ~(mask)))
#endif                  /* ########################################## */

#define MII_SET_BIT     MII_SET_MASK
#define MII_CLR_BIT     MII_CLR_MASK

/*
 * MiiRead(), MiiWrite()
 *
 * (MAC or repeater) User-supplied MII read and write functions.  The
 * PHY's serial MII registers cannot be directly addressed via the
 * (PCI) bus, as the PHY does not implement a 16-bit parallel register
 * interface.  Instead, the PHY's serial Media Independent Interface
 * (MII) must be accessed and accumulated into an external, general
 * general purpose port, e.g., an MII port on a LAN adapter MAC or
 * or a repeater controller device.  See the "mii.c" porting file for
 * implementation-dependent MiiRead() and MiiWrite() example code.
 */
WORD     MiiRead(MIIOBJ *phy, BYTE reg_addr);
void     MiiWrite(MIIOBJ *phy, BYTE reg_addr, WORD reg_value);
void MiiWriteMask(MIIOBJ *phy, WORD reg_addr, WORD reg_value, WORD mask_value) ;

/*
 * MILLISECOND_WAIT() and MICROSECOND_WAIT() - hook for implementation
 * and OS-dependent delay routines defined by the user.  See "mii.c"
 * MiiMillisecondWait() and MiiMicrosecondWait() functions for
 * OS-specific example code.  Two functions are declared to allow the
 * designer to deploy different wait mechanisms.  Short, microsecond
 * delays may be implemented via a non-blocking, busy-wait mechanism
 * whereas longer, millisecond delays might relinquish the CPU via
 * a process or thread sleep mechanism.  Alternately, the millisecond
 * wait function might just call the microsecond wait function.
 */
#ifndef MICROSECOND_WAIT
#define MICROSECOND_WAIT        MiiMicrosecondWait
void     MiiMicrosecondWait(DWORD num_microseconds);
#endif
#ifndef MILLISECOND_WAIT
#define MILLISECOND_WAIT        MiiMillisecondWait
void     MiiMillisecondWait(DWORD num_milliseconds);
#endif

/*
 * MiiIoRead() and MiiIoWrite() - export IO_READ() and IO_WRITE()
 * routines for usage by related functions, 
 * The <io_offset> is relative to the base address of the associated
 * MAC or repeater controller device.  The <io_value> read or
 * written is the 32-bit DWORD I/O register value.
 */
DWORD    MiiIoRead( DWORD mac_address, WORD io_offset);
void     MiiIoWrite(DWORD mac_address, WORD io_offset, DWORD io_value);

#endif /* _MII_H_ */
