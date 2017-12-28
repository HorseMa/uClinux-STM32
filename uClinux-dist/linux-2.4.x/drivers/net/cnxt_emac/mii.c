/****************************************************************************
*
*	Name:			mii.c
*
*	Description:	MII driver for CX821xx products
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
*****************************************************************************
*  $Author:   davidsdj  $
*  $Revision:   1.0  $
*  $Modtime:   Mar 19 2003 12:25:20  $
****************************************************************************/

/*
 *  Module Name: mii.c  
 *  Author: Joe Bonaker
 *
 *  MII.C : OS_DEPENDENT, MAC_DEPENDENT 
 *  100Base-T Media Independent Interface (MII)
 *  module, which contains adapter and OS-specific support routines for
 *  the "phy.c" PHY driver module.  This file must be tailored for each
 *  unique adapter/operating system environment pair.
 *
 *  Conditional Compilation Switches:
 *
 *     The following conditional compilation switches apply, and may
 *     be overridden by the user at compile-time.
 *
 *     DRV_OS - default DOS.  Establishes the OS-specific controller
 *     (MAC or repeater) device I/O (or memory-mapped) access to the
 *     PHY's serial MII plus microsecond and/or millisecond timer
 *     support as detailed in [Step 1] of the Porting Guide below and
 *     within this module.  DRV_OS types are defined in "phytypes.h."
 *
 *  Porting Guide:
 *
 *  There are two steps which must be performed to tailor this module
 *  for a specific adapter (NIC) or repeater configuration.
 *
 *  [Step 1] entails mapping the OS_MICROSECOND_WAIT() and/or
 *  OS_MILLISECOND_WAIT(), IO_WRITE(), IO_READ(), plus optional
 *  OS_NANOSECOND_WAIT() macros into the PHY driver's operating system,
 *  as defined by DRV_OS.
 *
 *  [Step 2] alters the adapter-specific register configuration.  At
 *  present, only a MAC (NIC) controller interface, the RSS1161x 10/100
 *  Mbit Fast Ethernet series, is supported.  [Step 2a] contains
 *  MAC general purpose I/O (GPIO) settings which may vary among
 *  different adapter configurations.  [Step 2b] contains an optional
 *  PHY hardware reset sequence, which is dependent upon the [Step 2a]
 *  interconnection between the MAC and PHY devices/modules.
 *
 *  Search this module for occurances of [Step 1] and [Step 2], editing
 *  accordingly.
 *
 **********************************************************************/
#if 0
	#if defined(LINUX_DRIVER)
	#include "linuxinc.h"
	#endif
#endif
#define P52
#include "phytypes.h"
#include "mii.h"


 /*--------------------------------------------------------------------*
 * [Step 1] Enter DRV_OS-dependent support for OS_MICROSECOND_WAIT(),
 *          and/or OS_MILLISECOND_WAIT(), optional OS_NANOSECOND_WAIT(),
 *          plus 32-bit IO_WRITE()/IO_READ() services.  Edit one or
 *          more of [Step 1a] through [Step 1h] as necessary.
 *
 *          Either one or both of OS_MICROSECOND_WAIT() and
 *          OS_MILLISECOND_WAIT() must be #defined, depending on DRV_OS
 *          environment support.  If the DRV_OS supports microsecond
 *          granularity delays (preferred), then #define the
 *          OS_MICROSECOND_WAIT() macro accordingly.  Otherwise,
 *          #define OS_MILLISECOND_WAIT().  If the DRV_OS supports both
 *          microsecond and millisecond delays, and advocates a scheme
 *          for relinquishing the CPU during longer delays, then
 *          OS_MILLISECOND_WAIT() may also be #defined using the more
 *          system-friendly long delay service.  As long as at least
 *          one of these two macros is #defined, code outside this
 *          [Step 1] DRV_OS-specific block will ensure that the other
 *          macro is properly derived.
 *
 *          Unless the DRV_OS supports nanosecond granularity (highly
 *          unlikely), OS_NANOSECOND_WAIT() should NOT be #defined,
 *          and will be derived from OS_MICROSECOND_WAIT() outside this
 *          code block.  There is one exception.  Since the nanosecond
 *          delay is only used to ensure that MiiRead() and MiiWrite()
 *          clock timing doesn't exceed the 802.3u minimum hold time
 *          of 200-300 nanoseconds, OS_NANOSECOND_WAIT() can be
 *          disabled in a DRV_OS environment where IO_READ() and
 *          IO_WRITE() take a considerable length of time to complete.
 *          This is accomplished as follows:
 *
 *              #define OS_NANOSECOND_WAIT(n)           // No delay
 *
 *          For example, the DRV_OS=WIN32_DIAG relies on driver ioctls
 *          to perform IO_READ() and IO_WRITE().  This, in conjunction
 *          with a 10-millisecond delay granularity, results in
 *          MiiRead() and MiiWrite() times approaching one second per
 *          register!  Therein, OS_NANONSECOND_WAIT() was disabled.
 *
 *          Though their names imply I/O device access, the IO_READ()
 *          and IO_WRITE() macros may also be crafted to support a
 *          memory-mapped controller -- MAC or repeater -- device
 *          interface.  Regardless, they also support the notion of an
 *          <adapter> reference for DRV_OS environments requiring
 *          abstraction to touch hardware, e.g., NDIS, WIN32_DIAG,
 *          NOVELL_CHSM.  Other environments can ignore the <adapter>
 *          or use it as a device base address parameter.
 *--------------------------------------------------------------------*/

#if (DRV_OS == NDIS)            /*------------------------------------*
                                 * [Step 1a] Win95/WinNT NDIS
                                 *
                                 * Assumes <adapter> is the starting
                                 * I/O port address of the MAC device
                                 *------------------------------------*/
#define OS_MICROSECOND_WAIT(u)          NdisStallWrapper(u)
#ifdef P51
ULONG P51_GlobalReadVariable;
#define IO_WRITE(adapter, port, val) {                                         \
    NdisWriteRegisterUlong((PULONG) ((adapter) + (port)), (ULONG) (val));      \
    NdisReadRegisterUlong((PULONG) ((adapter) + (port)), (PULONG) &P51_GlobalReadVariable); \
}
#else
#define IO_WRITE(adapter, port, val)    \
    NdisRawWritePortUlong((PULONG) ((adapter) + (port)), (ULONG) (val))
#endif
#define IO_READ(adapter, port)          \
    NdisIoReadWrapper((adapter) + (port))

/***********************************************************************
 * NdisStallWrapper() - "OS-friendly" wait in 100 uSec chunks
 **********************************************************************/
static void
NdisStallWrapper(DWORD num_microseconds)
{
    /*
     * Semi-hog:  relinquish the CPU every 100 microseconds to prevent
     * other drivers (e.g., tty character devices) from starving
     */
    while (num_microseconds > 25) {
        NdisStallExecution(25);
        num_microseconds -= 25;
    }
//	if( num_microseconds == 1)
//		num_microseconds = 10;
    NdisStallExecution(num_microseconds);
}

/***********************************************************************
 * NdisIoReadWrapper() - expected NdisRawReadPortUlong() interface
 **********************************************************************/
static DWORD
NdisIoReadWrapper(DWORD port)
{
    ULONG       val;

#ifdef P51
    NdisReadRegisterUlong((PULONG) port, &val);
#else
    NdisRawReadPortUlong((PULONG) port, &val);
#endif
    return ((DWORD) val);
}

#elif (DRV_OS == NOVELL_CHSM)   /*------------------------------------*
                                 * [Step 1b] Novell 32-bit ODI
                                 *
                                 * Assumes <adapter> has been cast as
                                 * pointer to a PHY_ADAPTER structure
                                 *------------------------------------*/

#define OS_MICROSECOND_WAIT(u)          Odi32MicroWait(u)
#define OS_NANOSECOND_WAIT(n)           Slow()  /* 0.5 uSec NOP */
#ifndef IO_MAPPED               /* default:  memory-mapped device */
#define IO_WRITE(adapter, port, val)                            \
    Wrt32(((PHY_ADAPTER *) (adapter))->bus_tag, NULL,           \
          (void *) (((port) + (UINT8 *)                         \
                     ((PHY_ADAPTER *) (adapter))->dev_addr)),   \
          (UINT32) (val))
#define IO_READ(adapter, port)          (DWORD)                 \
    Rd32(((PHY_ADAPTER *) (adapter))->bus_tag, NULL,            \
         (void *) (((port) + (UINT8 *)                          \
                   ((PHY_ADAPTER *) (adapter))->dev_addr)))
#else                           /* non-default:  I/O-mapped device */
#define IO_WRITE(adapter, port, val)                            \
    Out32(((PHY_ADAPTER *) (adapter))->bus_tag,                 \
          (void *) (((port) + (UINT8 *)                         \
                     ((PHY_ADAPTER *) (adapter))->dev_addr)),   \
          (UINT32) (val))
#define IO_READ(adapter, port)          (DWORD)                 \
    In32(((PHY_ADAPTER *) (adapter))->bus_tag,                  \
         (void *) (((port) + (UINT8 *)                          \
                   ((PHY_ADAPTER *) (adapter))->dev_addr)))
#endif

/***********************************************************************
 * Odi32MicroWait() - Microsecond delay under 32-bit Novell ODI OS
 **********************************************************************/
static void
Odi32MicroWait(DWORD num_microseconds)
{
    /*
     * CMSMGetMicroTimer() support is suspect under some client
     * operating systems as only 16 bits of returned 32-bit timer value
     * may be valid.  Instead, use the (hopefully) more trustworthy
     * Slow() service.
     */
    while (num_microseconds-- > 0) {
        Slow();         /* 0.5 uSec NOP per CHSM ODI spec v1.11 */
        Slow();
    }
}

#elif (DRV_OS == SCO_MDI)       /*------------------------------------*
                                 * [Step 1c] SCO Unix drivers: MDI,
                                 * LLI, DLPI, and Gemini.
                                 *
                                 * Assumes <adapter> is the starting
                                 * I/O port address of the MAC device
                                 *------------------------------------*/
#define OS_MICROSECOND_WAIT(u)          suspend(u)
#define IO_WRITE(adapter, port, val)    outd((adapter) + (port), (val))
#define IO_READ(adapter, port)          ind((adapter) + (port))

#elif (DRV_OS == SCO_LLI)
#define OS_MICROSECOND_WAIT(u)          TBD
#define IO_WRITE(adapter, port, val)    TBD
#define IO_READ(adapter, port)          TBD

#elif (DRV_OS == SCO_DLPI)
#define OS_MICROSECOND_WAIT(u)          TBD
#define IO_WRITE(adapter, port, val)    TBD
#define IO_READ(adapter, port)          TBD

#elif (DRV_OS == SCO_GEMINI)
#define OS_MICROSECOND_WAIT(u)          TBD
#define IO_WRITE(adapter, port, val)    TBD
#define IO_READ(adapter, port)          TBD

#elif (DRV_OS == SOLARIS)       /*------------------------------------*
                                 * [Step 1d] Sun Solaris driver
                                 *
                                 * Assumes <adapter> is the starting
                                 * I/O port address of the MAC device
                                 *------------------------------------*/
#define OS_MICROSECOND_WAIT(u)          drv_usecwait(u)
#define IO_WRITE(adapter, port, val)    outl((adapter) + (port), (val))
#define IO_READ(adapter, port)          inl((adapter) + (port))

#elif (DRV_OS == LINUX)         /*------------------------------------*
                                 * [Step 1e] Linux driver support
                                 *
                                 * Assumes <adapter> is the starting
                                 * I/O port address of the MAC device
                                 *------------------------------------*/
/*
 * Caution:  per the outb(9) man page, the outl() arguments are the
 * opposite of most DOS-based output routines:  outl(value, portAddr)
 * instead of outl(portAddr, value).
 */
#ifdef __KERNEL__

#define OS_MICROSECOND_WAIT(u)          udelay(u)
#else
	/* #include <unistd.h> */
	/* #define OS_MICROSECOND_WAIT(u)          usleep(u) */
	#define OS_MICROSECOND_WAIT(u)          LinuxMicroWait(u)
	void LinuxMicroWait(int microseconds)
	{
	    do {
	    } while (--microseconds > 0);
	}
#endif

// Turn the requested nanosecond delay into a 20 times longer usec delay
#define OS_NANOSECOND_WAIT(u) udelay(u/50)

#ifdef P51
	#define IO_WRITE(adapter, port, val)  { \
		writel((val), (adapter) + (port)) ; \
		readl( (adapter) + (port) );		\
	}
	#define IO_READ(adapter, port)          readl((adapter) + (port))
#else
	#define IO_WRITE(adapter, port, val)    outl((val), (adapter) + (port))
	#define IO_READ(adapter, port)          inl((adapter) + (port))
#endif

#elif (DRV_OS == PKTDRV)        /*------------------------------------*
                                 * [Step 1f] DOS/PCTCP packet driver
                                 *
                                 * Assumes <adapter> is the starting
                                 * I/O port address of the MAC device
                                 *------------------------------------*/
#include <???.h>

#define OS_MICROSECOND_WAIT(u)          TBD
#define IO_WRITE(adapter, port, val)    TBD
#define IO_READ(adapter, port)          TBD

#elif (DRV_OS == WIN32_DIAG)    /*------------------------------------*
                                 * [Step 1g] RSS' Win95/WinNT Diagnostic
                                 *
                                 * Assumes <adapter> is the adapter
                                 * handle used by the diagnostic.  Also,
                                 * nullifies OS_NANOSECOND_WAIT(), as
                                 * IO_READ() and IO_WRITE() driver
                                 * ioctl's provide sufficient clock
                                 * spacing, i.e., >> one uSec.
                                 *------------------------------------*/

#define Unknown_OS      0
#define Windows95_OS    1
#define Windows98_OS    2
#define WindowsNT4_OS   3

#define OS_MILLISECOND_WAIT(m)                  SleepEx((m), FALSE)
#define OS_NANOSECOND_WAIT(n)                   /* No delay */

#define IO_WRITE(adapter, port, val)            \
    (void) _PutCSRRegisterWrapper((ULONG) adapter, (ULONG) (port), (ULONG) (val))

#define IO_READ(adapter, port)          \
    _GetCSRRegisterWrapper((ULONG) (adapter), (ULONG) (port))

/***********************************************************************
 * _PutCSRRegisterWrapper() - expected _PutCSRRegister() interface
 **********************************************************************/
static DWORD
_PutCSRRegisterWrapper(ULONG adapter, ULONG port, ULONG val)
{

    HANDLE      drvhandle;
    DWORD       RunningOS;

    RunningOS = CheckOS();

    if (RunningOS != WindowsNT4_OS) {
        _getdriverhandle(&drvhandle);   /* get handle to driver */
        
        if (!drvhandle)                 /* if fail, return -1 */
            return (-1);
    }

    (void) _putcsrregister(drvhandle,adapter, port, val);

    return ((DWORD) val);
}

/***********************************************************************
 * _GetCSRRegisterWrapper() - expected _GetCSRRegister() interface
 **********************************************************************/
static DWORD
_GetCSRRegisterWrapper(ULONG adapter, ULONG port)
{
    ULONG       val = -1;
    HANDLE      drvhandle;
    DWORD       RunningOS;

    RunningOS = CheckOS();

    if (RunningOS != WindowsNT4_OS) {
        _getdriverhandle(&drvhandle);   /* get handle to driver */
        
        if (!drvhandle)                 /* if fail, return -1 */
            return (-1);
    }
    
    (void) _getcsrregister(drvhandle,adapter, port, &val);

    return ((DWORD) val);
}

#elif (DRV_OS == VXWORKS)       /*------------------------------------*
                                 * [Step 1g.1] VxWorks
                                 *------------------------------------*/

#define OS_MICROSECOND_WAIT(u)          VxWorksMicroWait(u)
#define OS_NANOSECOND_WAIT(n)           VxWorksSlow() /* 0.5 uSec NOP */

/* default:  memory-mapped device */
#define IO_WRITE(adapter, port, val)                            \
	* ((BYTE *) adapter + port) = val
#define IO_READ(adapter, port)          (DWORD)                 \
	* ((BYTE *) adapter + port)

static void VxWorksSlow();
/***********************************************************************
 * VxWorksMicroWait() - Microsecond delay under VxWorks OS
 **********************************************************************/
static void
VxWorksMicroWait(DWORD num_microseconds)
{
    /*
     * Use the VxWorksSlow() function which busywaits for 0.5 microsec.
     */
    while (num_microseconds-- > 0) {
        VxWorksSlow();         /* 0.5 uSec NOP loop */
        VxWorksSlow();
    }
}

/***********************************************************************
 * VxWorksSlow() - Microsecond delay under VxWorks OS
 **********************************************************************/
static void
VxWorksSlow()
{
	volatile int	iLoopCount;
    /*
     * Loop for 25 times to pass 0.5 micro.
     */
	for (iLoopCount=25; iLoopCount; iLoopCount--)
		;
	for (iLoopCount=25; iLoopCount; iLoopCount--)
		;
}


#elif (DRV_OS == NO_OS)         /*------------------------------------*
                                 * [Step 1g.2] No OS Services
                                 *------------------------------------*/

#define OS_MICROSECOND_WAIT(u)          NoOSMicroWait(u)
#define OS_NANOSECOND_WAIT(n)           NoOSSlow() /* 0.5 uSec NOP */

/* default:  memory-mapped device */
#define IO_WRITE(adapter, port, val)                            \
	* ((BYTE *) adapter + port) = val
#define IO_READ(adapter, port)          (DWORD)                 \
	* ((BYTE *) adapter + port)

/***********************************************************************
 * VxWorksMicroWait() - Microsecond delay under No OS Services
 **********************************************************************/
static void
NoOSMicroWait(DWORD num_microseconds)
{
    /*
     * Use the NoOSSlow() function which busywaits for 0.5 microsec.
     */
    while (num_microseconds-- > 0) {
        NoOSSlow();         /* 0.5 uSec NOP loop */
        NoOSSlow();
    }
}

/***********************************************************************
 * NoOSSlow() - Microsecond delay under VxWorks OS
 **********************************************************************/
static void
NoOSSlow()
{
	volatile int	iLoopCount;
    /*
     * Loop for 25 times to pass 0.5 micro.
     */
	for (iLoopCount=25; iLoopCount; iLoopCount++)
		;
}


#else                           /*------------------------------------*
                                 * [Step 1h] Miscellaneous (DOS, etc.)
                                 *
                                 * Assumes <adapter> is the starting
                                 * I/O port address of the MAC device
                                 *------------------------------------*/
#include <conio.h>

#if   defined(__WATCOMC__)
#elif defined(__BORLANDC__)
#elif defined(_MSVC_VER)
#endif

#ifdef _MAPTEST_
BYTE    phy_data[0x10] = {0};   /* DATA - initialized */
BYTE    phy_bss[0x10];          /* BSS  - uninitialized */
#endif

extern DWORD    inpd(int port);
extern DWORD    outpd(int port, DWORD val);
#define OS_MICROSECOND_WAIT(u)          DosMicroWait(u)
#define IO_WRITE(adapter, port, val)    \
    (void) outpd((int) (adapter) + (port), (val))
#define IO_READ(adapter, port)          \
    inpd((int) (adapter) + (port))

/*
 * Convert microseconds to PC system ticks.  The 16-bit Timer0, in
 * Mode 3 (square wave), decrements two counts per 1.1932 MHz input
 * clock on most systems.  We round this 2.3864 ticks/uSec to integer
 * (239 / 100) here, yielding a 32-bit unsigned range of 17.971 seconds.
 * Warning:  Some AMI BIOSes have a bug, configuring Timer0 into Mode 2
 * (rate generator), which only decrements one count per clock.  On
 * such systems, we'll suffer a delay that is twice as long as
 * expected.  This could be a problem for longer, millisecond-to-second
 * duration delays, however, most MII related delays are microsecond
 * or less.
 */
#define PC_TIMER0_TICKS(usec)           \
    (1 + (((DWORD) (usec) * 239) / 100))

/***********************************************************************
 * DosMicroWait() - Microsecond delay under 16-bit DOS environment
 **********************************************************************/
static void
DosMicroWait(DWORD count)
{
    DWORD       t0;
    DWORD       t1;
    DWORD       delta_t;
    BYTE        timer0_lsb;

    /*
     * Writing a command value of 0x00 to Port 0x43 transfers
     * the Timer 0 counter into the output latch register.  The value
     * is held until read from port 0x40 (two successive byte reads,
     * LSB followed by the MSB).  A truly bullet-proof implementation
     * would lock interrupts prior to latching and reading the counter.
     */
    outp(0x43, 0x00);
    timer0_lsb = inp(0x40);
    t1 = ((DWORD) inp(0x40) << 8) + timer0_lsb;
    delta_t = 0;
    count = PC_TIMER0_TICKS(count);     /* uSec to Timer0 ticks */

    do {
        count -= delta_t;
        t0 = t1;
        outp(0x43, 0x00);
        timer0_lsb = inp(0x40);
        t1 = ((DWORD) inp(0x40) << 8) + timer0_lsb;
        if (t1 > t0) {
            t0 += 0x10000UL;            /* handle 16-bit rollover */
        }
        delta_t = t0 - t1;
    } while (count > delta_t);
}

#ifdef UNSUPPORTED      /* Most C/C++ only support 16-bit in-line ASM */
DWORD
outpd(int port, DWORD val)
{
    __asm {
        mov     dx, port;
        mov     eax, val;
        out     dx, eax;
    }
    return (val);
}

DWORD
inpd(int port)
{
    DWORD       val;

    __asm {
        mov     dx, port;
        in      eax, dx;
        mov     val, eax;
    }
    return (val);
}
#endif  /* UNSUPPORTED, as most C/C++ only support 16-bit in-line ASM */

#endif                          /*------------------------------------*
                                 * [Step 1] End of first porting step
                                 *------------------------------------*/

/*
 * Post- [Step 1] timer derivation.  OS_MICROSECOND_WAIT() and/or
 * OS_MILLISECOND_WAIT() must be #defined for this to work....
 */

#ifndef OS_MICROSECOND_WAIT
#define OS_MICROSECOND_WAIT(u)                  \
    OS_MILLISECOND_WAIT((DWORD) ((u) + 999) / 1000)
#endif

#ifndef OS_MILLISECOND_WAIT
#define OS_MILLISECOND_WAIT(m)  MICROSECOND_WAIT((DWORD) (m) * 1000)
#endif

//#if (DRV_OS == NDIS)
//#define OS_NANOSECOND_WAIT(n)           // NO_DELAY
//#endif
#ifndef OS_NANOSECOND_WAIT
#define OS_NANOSECOND_WAIT(n)   \
    MICROSECOND_WAIT((DWORD) ((n) + 999) / 1000)
#endif
#define NANOSECOND_WAIT         OS_NANOSECOND_WAIT

/*--------------------------------------------------------------------*
 * [Step 2] Enter (NIC or repeater) adapter's MAC controller-specific
 *          register information.  The MII (MDIO) and any optional
 *          adapter-unique general purpose I/O (GPIO) settings must be
 *          captured here.  In particular, the gpio[] table should be
 *          modified per adapter configuration of the MAC's GPIO
 *          interface [Step 2a].  If additional, custom configuration
 *          is required, the low-level MiiPhyHwReset() routine may be
 *          modifified [Step 2b].
 *
 *          Warning:  at present, only RSS and DEC-compatible fast
 *          ethernet MACs are supported.  Common repeater interfaces
 *          will be added as development (COMET?) progresses at RSS.
 *--------------------------------------------------------------------*/
#ifdef DEVICE_YELLOWSTONE

	#define MAC_MDIO_REG            0x0c
	#define MDIO_RD_DATA            0x8L
	#define MDIO_RD_ENABLE          0x2L
	#define MDIO_WR_ENABLE          0x00000L
	#define MDIO_WR_DATA_1          0x4L
	#define MDIO_WR_DATA_0          0x00000L        
	#define MDIO_CLK                0x1L
	#define MDIO_MASK               0xFL

#elif defined(P51) 

	#define JEDI_MII_ID			0x0032cc10		
	#define JEDI_MII_ID_REVC	0x0032cc13
	// bit 3:0 - revision

	#define JediPhy(mii_id) (BOOLEAN)( (mii_id & 0xfffffff0) == JEDI_MII_ID)

	/* P51- YUKON or ATHENS , ELAN or HLAN MII (MDIO) register offset and values */
	// the following definition is not 100% correct but it work for all our current boards
	#define MAC_MDIO_REG            0x30
	#define MDIO_RD_DATA            (0x8L<< ((phy->P51_HLAN) ? 2 : 0))
	#define MDIO_RD_ENABLE          (0x2L<< ((phy->P51_HLAN) ? 2 : 0))
	#define MDIO_WR_ENABLE          0x00000L
	#define MDIO_WR_DATA_1          (0x4L<< ((phy->P51_HLAN) ? 2 : 0))
	#define MDIO_WR_DATA_0          0x00000L        
	#define MDIO_CLK                (0x1L<<((phy->P51_HLAN) ? 2 : 0))
	#define MDIO_MASK               (0xFL<<((phy->P51_HLAN) ? 2 : 0))

#elif defined(P52) 
	// P52- CX82100,
	#define MAC_MDIO_REG            0x18
	#define MDIO_RD_DATA            0x0002L
	#define MDIO_RD_ENABLE          0x0008L
	#define MDIO_WR_ENABLE          0x0000L
	#define MDIO_WR_DATA_1          0x0004L        
	#define MDIO_WR_DATA_0          0x0000L        
	#define MDIO_CLK                0x0001L
	#ifdef JEDIPHY		// Bright 6/8/01 test
		#define MDIO_CLK_MDIP           0x0010L	/* bit 4 - default 0 */
												/*		0=falling endge sample */
												/*      1=rising endge sample */

		#define MDIO_MASK               0x001FL
	#else
		#define MDIO_CLK_MDIP           0x0010L	/* bit 4 - default 0 */
												/*		0=falling endge sample */
												/*      1=rising endge sample */

		#define MDIO_MASK               0x000FL
	#endif


#else

	/* RSS 1161x MACs' CSR9 (MDIO) register offset and values */
	#define MAC_MDIO_REG            0x48
	#define MDIO_RD_DATA            0x80000L
	#define MDIO_RD_ENABLE          0x40000L
	#define MDIO_WR_ENABLE          0x00000L
	#define MDIO_WR_DATA_1          0x20000L        
	#define MDIO_WR_DATA_0          0x00000L        
	#define MDIO_CLK                0x10000L
	#define MDIO_MASK               0xF0000L
#endif

/* MII frame fields */
#define MII_READ_ST_OP          0x6     /* MII READ start, opcode */
#define MII_WRITE_ST_OP         0x5     /* MII WRITE start, opcode */
#define MII_WRITE_TA            0x2     /* MII WRITE turnaround bits */
#define MII_WRITE_FRAME         \
    (((DWORD) MII_WRITE_ST_OP << 28) | ((DWORD) MII_WRITE_TA << 16))

/* Local (static) function prototypes */
static void     MiiPreamble(MIIOBJ *phy);
static void     MiiIdle(MIIOBJ *phy, int num_clocks);

/***********************************************************************
 * MiiRead() - read serial data from MII register
 **********************************************************************/

WORD MiiRead(MIIOBJ *phy, BYTE reg_addr)
{
    DWORD 	io_data;
    WORD	mii_data;
    int		n;

    /* Output 32-bit preamble (all ones) to re-sync the PHY */
    MiiPreamble(phy);

    /* Preserve non-MDIO bits in shared MAC_MDIO_REG (SPR) */
    io_data = IO_READ(phy->adapter, MAC_MDIO_REG) & ~MDIO_MASK;

    /* Write the first 14 <mii_data> frame bits in MSB-first order */
    mii_data = (MII_READ_ST_OP << 10) | (phy->mii_addr << 5) | (reg_addr & 0x1F);
    for (n = 0x2000; n > 0; n >>= 1)
    {
        io_data &= ~MDIO_MASK;          /* Preserve non-MDIO bits */
        if (mii_data & n)
        {
            io_data |= (MDIO_WR_ENABLE | MDIO_WR_DATA_1);
        }
        else
        {
            io_data |= (MDIO_WR_ENABLE | MDIO_WR_DATA_0);
        }

		// Change output data to phy on falling edge of MDIO_CLK
        IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data);
        NANOSECOND_WAIT(200);

		// Input data sampled by phy on rising edge of MDIO_CLK
        IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data | MDIO_CLK);
        NANOSECOND_WAIT(200);
    }

    mii_data = 0;
    io_data &= ~MDIO_MASK;          /* Preserve non-MDIO bits */
    io_data |= (MDIO_RD_ENABLE | MDIO_CLK_MDIP);

    /*
     * Read the next 18 bits [17:0] in MSB-first order.  The first two
     * bits, turnaround (TA) high-Z and zero, are effectively ignored.
     */
    for (n = 0; n < 18; n++)
    {
		// NOTE: When MDIO_CLK_MDIP bit is a zero the data from 
		// the phy is sampled on the falling edge of the MDIO_CLK.
		// When MDIO_CLK_MDIP bit is a one the data from the phy
		// is sampled on the rising edge of the MDIO_CLK.

        // MDIO_CLK falling edge
		IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data);	
        NANOSECOND_WAIT(200);

		// MDIO_CLK rising edge
        IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data | MDIO_CLK); 
        /*
         * ??? Shouldn't IO_READ() occur *after* MDC_CLK rising edge?
         * The IEEE 802.3u specification -- Clause 22.3.4, "MDIO timing
         * relationship to MDC" and Figure 22-17, "MDIO sourced by
         * PHY" -- doesn't imply that data is latched on the falling
         * edge of MDC.  In any case, that is what works with a National
         * Semiconductor DP83840 PHY, but may be a source of problems
         * with other PHY devices....
		 *
         * IT IS! Data must be sampled on the rising edge for the CX11656
		 * Home Plug device.
         */
        mii_data <<= 1;
        if (IO_READ(phy->adapter, MAC_MDIO_REG) & MDIO_RD_DATA) {
            mii_data |= 0x0001;
        }

        NANOSECOND_WAIT(300);        /* [sic] 300 nSec PHY hold */

    }

    /* Output a single, idle (high-Z, tri-stated) clock transition */
    MiiIdle(phy, 1);

    return (mii_data & 0xFFFF);
}

/***********************************************************************
 * MiiWrite() - write serial data to MII register
 **********************************************************************/
void MiiWrite(MIIOBJ *phy, BYTE reg_addr, WORD reg_value)
{
    DWORD       io_data;
    DWORD       mii_frame;
    DWORD       mask;

    /* Output 32-bit preamble (ones) sequence to re-sync the PHY */
    MiiPreamble(phy);

    /* Preserve non-MDIO bits in shared MAC_MDIO_REG (SPR) */
    io_data = IO_READ(phy->adapter, MAC_MDIO_REG) & ~MDIO_MASK;

    /* Form and write 32-bit [31:0] <mii_frame> in MSB-first order */
    mii_frame = MII_WRITE_FRAME | ((DWORD) phy->mii_addr << 23) |
        ((DWORD) (reg_addr & 0x1F) << 18) | (DWORD) reg_value;
    for (mask = 0x80000000UL; mask > 0; mask >>= 1) {
        io_data &= ~MDIO_MASK;          /* Preserve non-MDIO bits */
        if (mii_frame & mask) {
            io_data |= (MDIO_WR_ENABLE | MDIO_WR_DATA_1);
        } else {
            io_data |= (MDIO_WR_ENABLE | MDIO_WR_DATA_0);
        }
        IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data);
        NANOSECOND_WAIT(200);
        IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data | MDIO_CLK);
        NANOSECOND_WAIT(200);
    }

    /* Output a single, idle (high-Z, tri-stated) clock transition */
    MiiIdle(phy, 1);
}


void MiiWriteMask(MIIOBJ *phy, WORD reg_addr, WORD reg_value, WORD mask_value)
{

	if( mask_value != 0xffff )
	{
		reg_value = (MiiRead( phy, (BYTE)reg_addr) & ~mask_value) | (reg_value & mask_value) ;
	}
	MiiWrite(phy, (BYTE)reg_addr, reg_value) ;
}
/***********************************************************************
 * MiiPreamble() - Generate 32-bit MII preamble sequence (all ones)
 **********************************************************************/
static void
MiiPreamble(MIIOBJ *phy)
{
    DWORD       io_data;
    int         n;

    /* Preserve non-MDIO bits in shared MAC_MDIO_REG (SPR) */
    io_data = IO_READ(phy->adapter, MAC_MDIO_REG) & ~MDIO_MASK;

    if (!phy->preamble_suppression) 
    {
        io_data |= (MDIO_WR_ENABLE | MDIO_WR_DATA_1);
#if defined(JEDIPHY) && defined(P52)
        for (n = 0; n < 64; n++)
        {
#else
        for (n = 0; n < 32; n++)
        {
#endif
            IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data);
            NANOSECOND_WAIT(200);
            IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data | MDIO_CLK);
            NANOSECOND_WAIT(200);
        }
    }
}

/***********************************************************************
 * MiiIdle() - Generate one or more bits of MII idle sequence (high-Z)
 **********************************************************************/
static void MiiIdle(MIIOBJ *phy, int num_clocks)
{
    DWORD       io_data;

    /* Preserve non-MDIO bits in shared MAC_MDIO_REG (SPR) */
    io_data = IO_READ(phy->adapter, MAC_MDIO_REG) & ~MDIO_MASK;
#if defined(JEDIPHY) && defined(P52)
    io_data |= (MDIO_RD_ENABLE | MDIO_CLK_MDIP);
#else
    io_data |= MDIO_RD_ENABLE;
#endif

	// need more delay for JEDI phy, tested under Win2K , need 5ms to make 
	// MiiRead, Write work for JEDI, (Win98 have longer delay for the same API call)
    do {
        IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data);
//        NANOSECOND_WAIT(200);
        MICROSECOND_WAIT(5);
        IO_WRITE(phy->adapter, MAC_MDIO_REG, io_data | MDIO_CLK);
//        NANOSECOND_WAIT(200);
        MICROSECOND_WAIT(5);
    } while (--num_clocks > 0);
}

/***********************************************************************
 * MiiMillisecondWait() - OS-dependent busy-wait/process sleep wrapper
 **********************************************************************/
void
MiiMillisecondWait(DWORD num_milliseconds)
{
    OS_MILLISECOND_WAIT(num_milliseconds);
}

/***********************************************************************
 * MiiMicrosecondWait() - OS-dependent busy-wait/process sleep wrapper
 **********************************************************************/
void
MiiMicrosecondWait(DWORD num_microseconds)
{
    OS_MICROSECOND_WAIT(num_microseconds);
}

/***********************************************************************
 * MiiIoRead() - Export wrapper for the IO_READ() macro
 **********************************************************************/
DWORD MiiIoRead( DWORD mac_addr, WORD io_offset)
{
    return (IO_READ( mac_addr, io_offset));
}

/***********************************************************************
 * MiiIoRead() - Export wrapper for the IO_WRITE() macro
 **********************************************************************/
void
MiiIoWrite( DWORD mac_addr, WORD io_offset, DWORD io_value)
{
    IO_WRITE(mac_addr, io_offset, io_value);
}

#if (DRV_OS == DOS)     /* ########################################## */

/***********************************************************************
 * MiiSetBits() - Set (logical-OR) one or more bits in MII register
 **********************************************************************/
void
MiiSetBits(MIIOBJ *phy, BYTE reg_addr, WORD bits)
{
    MiiWrite(phy, reg_addr, MiiRead(phy, reg_addr) | bits);
}

/***********************************************************************
 * MiiClrBits() - Clear (logical-NAND) one or more bits in MII register
 **********************************************************************/
void
MiiClrBits(MIIOBJ *phy, BYTE reg_addr, WORD bits)
{
    MiiWrite(phy, reg_addr, MiiRead(phy, reg_addr) & ~bits);
}

#endif                  /* ########################################## */
