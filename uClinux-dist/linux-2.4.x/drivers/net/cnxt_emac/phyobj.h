/****************************************************************************
*
*	Name:			phyobj.h
*
*	Description:	
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
*  $Modtime:   Mar 19 2003 12:25:20  $
****************************************************************************/

#ifndef _PHYOBJ_H_
#define _PHYOBJ_H_

//#include "typdef.h"
#include "phytypes.h"
#include "mii.h"
#ifdef JEDIPHY	// Bright
#include "osfile.h"
#endif

/*
 * PHY_LINK_* -- link state information returned by PhyCheck()
 *
 * (1) Link summary (PHY_LINK_SUMMARY)
 *
 * PHY_LINK_UP        - Set when the link is operational
 *
 * PHY_LINK_NEW_MEDIA - Set upon initial PHY_LINK_UP, and whenever
 *                      PHY_LINK_UP is true and a media state change
 *                      (speed and/or duplex mode) has been detected
 *                      since the last PhyCheck().  This status
 *                      can be used as a signal to reconfigure a MAC's
 *                      speed, duplex and port select settings.
 *
 * PHY_LINK_FDX       - Set when PHY_LINK_UP && operating Full-Duplex
 *
 * PHY_LINK_AUTONEG   - Set when Auto-Negotiating:  if PHY_LINK_UP,
 *                      this indicates Auto-Negotiation complete,
 *                      otherwise negotiation is still in progress
 *
 * PHY_LINK_SPEED     - Set with PHY_LINK_UP to indicate 1/10/100
 *                      Mbit/s line speed, as follows.  One and only
 *                      one of the following PHY_LINK_SPEED bits will
 *                      be set when PHY_LINK_UP is set.  If PHY_LINK_UP
 *                      is not set, then none of the PHY_LINK_SPEED
 *                      bits will be set.
 *
 *                      PHY_LINK_100 - if PHY_LINK_UP && 100 Mbit/s
 *                      PHY_LINK_10  - if PHY_LINK_UP &&  10 Mbit/s
 *                      PHY_LINK_HPNA_10  - if PHY_LINK_UP &&   HPNA 10 Mbit/s
 *
 * PHY_LINK_PORT      - Set to indicate the current, active PHY port
 *                      specific to an RSS 11617 or 11623 series
 *                      Ethernet MAC device.  The caller should
 *                      (carefully) reconfigure the MAC R6 Network
 *                      Access Register (NAR, I/O offset 0x30)
 *                      accordingly, carefully switching upon changes
 *                      in state.  Exactly one of the following
 *                      bits will always be set, regardless of the
 *                      state of PHY_LINK_UP:
 *
 *                      PHY_LINK_PORT_MII  - Use external MII port
 *                      PHY_LINK_PORT_7WS  - Use external 7-wire
 *                                           serial port
 *                      PHY_LINK_PORT_HPNA - Use internal HomePNA 
 *                                           PHY port (not use for JEDI, RSS 11623
 *                                           and newer devices only)
 *                       
 *
 * (2) Error status (PHY_LINK_ERRORS)
 *
 * One or more of the following error bits are latched when ...
 *
 * PHY_LINK_JABBER         - Jabber condition detected (10Base-T only)
 * PHY_LINK_PARALLEL_ERROR - Auto-Negotiation/parallel detection failure
 * PHY_LINK_REMOTE_FAULT   - Auto-Negotiation Remote Fault received
 *                           from the link partner
 * PHY_LINK_INIT_ERROR     - Device not initialized
 *
 * (3) Media specifics (PHY_LINK_TECHNOLOGY)
 *
 * The PhyCheck() return code will contain exactly one of the following
 * PHY_LINK_TECHNOLOGY bits if (PHY_LINK_UP || !PHY_LINK_AUTONEG), i.e.,
 * whenever the link is up or if the link is down and we're trying to
 * force a single media technology.  Otherwise, in the case that
 * (!PHY_LINK_UP && PHY_LINK_AUTONEG), i.e., the link is down but
 * we're advertising multiple media types, then one or more of the
 * following bits will be set to indicate the current capability
 * advertisement.  Depending on device capabilities, this may be a
 * reduced subset of the <connect> argument passed via the PhyInit() or
 * PhyConnectSet() APIs.
 *
 * PHY_LINK_HPNA20             - HomePNA mode 
 *
 * PHY_LINK_100BASE_T2FD       - 100Base-T2 Full-Duplex mode
 * PHY_LINK_100BASE_T2         - 100Base-T2 (Half-Duplex mode)
 * PHY_LINK_100BASE_T4         - 100Base-T4 (Half-Duplex mode)
 * PHY_LINK_100BASE_TXFD       - 100Base-TX Full-Duplex mode
 * PHY_LINK_100BASE_TX         - 100Base-TX (Half-Duplex mode)
 * PHY_LINK_10BASE_TFD         - 10Base-T   Full-Duplex mode
 * PHY_LINK_10BASE_T           - 10Base-T   (Half-Duplex mode)
 *
 * (4) PhyCheck()'s PHY_LINK_* Return Code Pseudo-Code
 *
 * The aggregate PHY_LINK_* status returned by PhyCheck() is
 * contructed as follows:
 *
 * if (PHY_LINK_UP) {
 *     One and only one PHY_LINK_1* PHY_LINK_TECHNOLOGY status bit is
 *     set to indicate the selected link technology/speed/mode which
 *     are currently active (PHY_LINK_TECHNOLOGY defines the
 *     aggregation of specific technologies and maps to the 802.3u
 *     aAutoNegLocalTechnologyAbility MIB attribute) plus some HomePNA
 *     PHY extensions;
 *     if (first-time PHY_LINK_UP or change in PHY_LINK_TECHNOLOGY) {
 *         report PHY_LINK_NEW_MEDIA;
 *     }
 * } else { // the following should probably be ignored ... too complex
 *     if (!PHY_LINK_AUTONEG) {
 *         One and only one PHY_LINK_1* status bit is set to
 *         indicate the forced mode connection attempt pending link
 *         activation;
 *     } else {
 *         One or more PHY_LINK_1* status bits are set to indicate
 *         the Auto-Negotiation advertisement pending link activation;
 *     }
 * }
 *
 * (5) PHY_LINK_DEFAULT - assume a conservative, default PHY link
 *     state of 10 Mbit/s, half-duplex, until configured or detected
 *     otherwise.
 */
#define PHY_LINK_UP             0x80000000L /* (1) PHY_LINK_SUMMARY ... */
#define PHY_LINK_NEW_MEDIA      0x40000000L
#define PHY_LINK_FDX            0x20000000L
#define PHY_LINK_AUTONEG        0x10000000L
#define PHY_LINK_SPARE1         0x08000000L
#define PHY_LINK_100            0x04000000L // PHY_LINK_SPEED ...       
#define PHY_LINK_10             0x02000000L
#define PHY_LINK_HPNA_10        0x01000000L
#define PHY_LINK_PORT_MII       0x00800000L // PHY_LINK_PORT ...
#define PHY_LINK_PORT_7WS       0x00400000L
//#define PHY_LINK_PORT_HPNA      0x00200000L	
#define PHY_LINK_SPARE2         0x00100000L
#define PHY_LINK_JABBER         0x00080000L /* (2) PHY_LINK_ERRORS ... */
#define PHY_LINK_PARALLEL_ERROR 0x00040000L
#define PHY_LINK_REMOTE_FAULT   0x00020000L
#define PHY_LINK_INIT_ERROR     0x00010000L
#define PHY_LINK_SPARE3         0x0000F000L
#define PHY_LINK_HPNA20         0x00000800L /* (3) PHY_LINK_TECHNOLOGY ... */
//#define PHY_LINK_1HPNA_HIPWR    0x00000400L
//#define PHY_LINK_1HPNA_LOSPD    0x00000200L
//#define PHY_LINK_1HPNA_HIPWR_LOSPD   0x00000100L
#define PHY_LINK_100BASE_T2FD   0x00000080L
#define PHY_LINK_100BASE_T2     0x00000040L
#define PHY_LINK_FDX_PAUSE      0x00000020L
#define PHY_LINK_100BASE_T4     0x00000010L
#define PHY_LINK_100BASE_TXFD   0x00000008L
#define PHY_LINK_100BASE_TX     0x00000004L
#define PHY_LINK_10BASE_TFD     0x00000002L
#define PHY_LINK_10BASE_T       0x00000001L

#define PHY_LINK_SPEED          \
    (PHY_LINK_100  | PHY_LINK_10   | PHY_LINK_HPNA_10)

#define PHY_LINK_PORT           \
    (PHY_LINK_PORT_MII | PHY_LINK_PORT_7WS)
//    (PHY_LINK_PORT_MII | PHY_LINK_PORT_7WS | PHY_LINK_PORT_HPNA)

#define PHY_LINK_SUMMARY        \
    (PHY_LINK_UP       | PHY_LINK_NEW_MEDIA | PHY_LINK_FDX |    \
     PHY_LINK_AUTONEG  | PHY_LINK_PORT      | PHY_LINK_SPEED)

#define PHY_LINK_ERRORS                                 \
    (PHY_LINK_JABBER       | PHY_LINK_PARALLEL_ERROR |  \
     PHY_LINK_REMOTE_FAULT | PHY_LINK_INIT_ERROR)

#define PHY_LINK_HPNA_TECHNOLOGY                            \
    PHY_LINK_HPNA20

#define PHY_LINK_MII_TECHNOLOGY                             \
    (PHY_LINK_100BASE_T2FD | PHY_LINK_100BASE_T2    |       \
     PHY_LINK_100BASE_T4   |                                \
     PHY_LINK_100BASE_TXFD | PHY_LINK_100BASE_TX    |       \
     PHY_LINK_10BASE_TFD   | PHY_LINK_10BASE_T)

#define PHY_LINK_TECHNOLOGY                                 \
    (PHY_LINK_MII_TECHNOLOGY | PHY_LINK_HPNA_TECHNOLOGY)

#define PHY_LINK_DEFAULT        PHY_LINK_10

/*
 * PhyInit(), PhyConnectGet() and PhyConnectSet() <connect> parameters,
 * used either to (a) force a selected line speed (10 or 100 Mbps) and
 * mode (full- or half-duplex), or (b) to set a list of Auto-Negotiation
 * (A-N) options when the PHY_CONN_AUTONEG bit is set.  These represent
 * a subset of the PHY_LINK_* codes returned by PhyCheck().
 *
 * PHY_CONN_CURRENT - indicates that the current connection settings
 *     are to be retained; these settings were set by a previous
 *     PhyInit()/PhyConnSet(), otherwise they were established by the
 *     default hardware strapping of the device's AN[2:0] pins.
 *
 * PHY_CONN_AUTONEG - when set, indicates that one or more
 *     Auto-Negotiation settings are to advertised, otherwise a single
 *     configuration mode is to be forced.  Warning:  Set by itself,
 *     this flag directs the PHY to advertise *NO* capabilities,
 *     which may not be the desired result.  This flag must be or-ed
 *     with one or more of the PHY_CONN_TECHNOLOGY bits.  Optionally,
 *     the caller can use PHY_CONN_AUTONEG_ALL_MEDIA to utilize any
 *     available media.
 *
 * PHY_CONN_PORT_* - when one of [MII|7WS|HPNA] is set, indicates a
 *     current port selection specified by the caller of PhyInit() or
 *     PhyConnectSet().  When not set, the caller isn't indicating
 *     the current PHY device port selection.
 *
 * PHY_CONN_AUTONEG_REMOTE_FAULT - a Remote Fault indication should
 *     be advertised during Auto-Negotiation, applicable only when
 *     accompanied by PHY_CONN_AUTONEG.
 *
 * PHY_CONN_AUTONEG_FDX_PAUSE - adveritise Full-Duplex Pause mode (?),
 *     applicable only when accompanied by PHY_CONN_AUTONEG.
 *
 * PHY_CONN_1* - various link speeds and duplex modes whose aggregation
 *     is defined by PHY_CONN_TECHNOLOGY.
 *
 * PHY_CONN_ANY_* - used to request any available 1-, 10- or 100-Mbps
 *     technology supported by the PHY device.  Use these settings when
 *     uncertain of the precise technology (100Base-TX, 100Base-T2, or
 *     even the presently unsupported 100Base-T4) to be deployed.
 *     The "FD" suffix denotes full-duplex, otherwise half-duplex applies.
 *     PHY_CONN_FORCE_ANY_MEDIA and PHY_CONN_AUTONEG_ALL_MEDIA are
 *     all-technology wildcards which respectively force the highest
 *     priority technology or advertise all capabilities supported by the
 *     device, in the following order of precedence:  100Base-T2FD,
 *     100Base-TXFD, 100Base-T2, 100Base-T4, 100Base-TX, 10Base-TFD,
 *     10Base-T).  Warning:  These PHY_CONN_ANY_* selections will fail if
 *     the "phy.c" module is compiled with the non-default,
 *     STRICT_PARAMETER_CHECKING switch #defined.
 *
 * PHY_CONN_DEFAULT - default connection values, dependent upon
 *     conditional *_PHY compilation switch.
 */
#define PHY_CONN_CURRENT                0
#define PHY_CONN_AUTONEG                PHY_LINK_AUTONEG

#define PHY_CONN_AUTONEG_REMOTE_FAULT   PHY_LINK_REMOTE_FAULT

#define PHY_CONN_HPNA20                 PHY_LINK_HPNA20
#define PHY_CONN_100BASE_T2FD           PHY_LINK_100BASE_T2FD
#define PHY_CONN_100BASE_T2             PHY_LINK_100BASE_T2
#define PHY_CONN_AUTONEG_FDX_PAUSE      PHY_LINK_FDX_PAUSE
#define PHY_CONN_100BASE_T4             PHY_LINK_100BASE_T4
#define PHY_CONN_100BASE_TXFD           PHY_LINK_100BASE_TXFD
#define PHY_CONN_100BASE_TX             PHY_LINK_100BASE_TX
#define PHY_CONN_10BASE_TFD             PHY_LINK_10BASE_TFD
#define PHY_CONN_10BASE_T               PHY_LINK_10BASE_T
#define PHY_CONN_HPNA_TECHNOLOGY        PHY_LINK_HPNA_TECHNOLOGY
#define PHY_CONN_MII_TECHNOLOGY         PHY_LINK_MII_TECHNOLOGY
#define PHY_CONN_TECHNOLOGY             PHY_LINK_TECHNOLOGY

/* Any (defaults to highest capability) HomePNA settting */
#define PHY_CONN_ANY_HPNA20                                     \
     PHY_CONN_HPNA20

/* Any 10Base-T half-duplex setting */
#define PHY_CONN_ANY_10BASE_T           PHY_CONN_10BASE_T

/* Any 10Base-T full-duplex setting */
#define PHY_CONN_ANY_10BASE_TFD         PHY_CONN_10BASE_TFD

/* Any available 100Base-T half-duplex capability */
#define PHY_CONN_ANY_100BASE_T          \
    (PHY_CONN_100BASE_T2 | PHY_CONN_100BASE_T4 | PHY_CONN_100BASE_TX)

/* Any available 100Base-T full-duplex capability */
#define PHY_CONN_ANY_100BASE_TFD        \
    (PHY_CONN_100BASE_T2FD | PHY_CONN_100BASE_TXFD)

/* Force highest available HomePNA or 10/100 MII capability */
#define PHY_CONN_FORCE_ANY_MEDIA        \
    (PHY_CONN_ANY_HPNA20     |          \
     PHY_CONN_ANY_10BASE_T   |          \
     PHY_CONN_ANY_10BASE_TFD |          \
     PHY_CONN_ANY_100BASE_T  |          \
     PHY_CONN_ANY_100BASE_TFD)

/* Auto-Negotiate all available HomePNA and/or 10/100 MII capabilities */
#define PHY_CONN_AUTONEG_ALL_MEDIA      \
    (PHY_CONN_AUTONEG | PHY_CONN_FORCE_ANY_MEDIA)

/* Only Auto-Negotiate 10/100 MII capabilities: exclude HomePNA */
#define PHY_CONN_AUTONEG_ALL_MII_MEDIA  \
    (PHY_CONN_AUTONEG_ALL_MEDIA & ~PHY_CONN_ANY_HPNA20)

/* Only Auto-Negotiate HomePNA capabilities: exclude 10/100 MII */
#define PHY_CONN_AUTONEG_ALL_HPNA_MEDIA  \
    (PHY_CONN_AUTONEG | PHY_CONN_ANY_HPNA20)

/* Default: Auto-Negotiate HomePNA, 10Base-T and 100Base-TX */
#define PHY_CONN_DEFAULT                           \
    (PHY_CONN_AUTONEG    |                         \
     PHY_CONN_ANY_HPNA20 |                         \
     PHY_CONN_10BASE_T   | PHY_CONN_10BASE_TFD   | \
     PHY_CONN_100BASE_TX | PHY_CONN_100BASE_TXFD   \
    )

/*
 * The PhyInit() <phy_addr> parameter conveys either an explicit PHY
 * address value in the range of PHY_ADDR_MIN to PHY_ADDR_MAX, or
 * specifies PHY_ADDR_ANY which directs PhyInit() to search for
 * the first responding PHY in the range PHY_ADDR_MIN..PHY_ADDR_MAX.
 * Moreover, in a multiple PHY environment, the caller can use the
 * PHY_ADDR_NEXT() macro to search for the next responding PHY
 * starting at an address above the specified value.
 *
 * The following examples demonstrate search and initialization of
 * (a) a single PHY at MII address 5, (b) the first -- and perhaps
 * only -- PHY found on the MII bus, (c) the first PHY found on the
 * MII bus excluding PHY 0, which may be reserved for a special
 * purpose such as hub management.
 * 
 *     static PHY       phy_space[MAX_SUPPORTED_PHYS];
 *     PHY             *phy = &phy_space[0];
 *     DWORD            my_adapter;     // e.g., MAC register address
 *
 *     (a) PhyInit(my_adapter, phy, 5, ...)
 *     (b) PhyInit(my_adapter, phy, PHY_ADDR_ANY, ...)
 *     (c) PhyInit(my_adapter, phy, PHY_ADDR_NEXT(PHY_ADDR_MGMT), ...)
 * 
 * A multiple PHY (e.g., intelligent hub) search might look something
 * like this:
 *
 *     static PHY       phy_space[MAX_SUPPORTED_PHYS];
 *     PHY             *phy = &phy_space[0];
 *     DWORD            my_adapter;     // e.g., MAC register address
 *     BYTE             phy_addr = PHY_ADDR_ANY;
 *     PHY_STATUS       status;
 *
 *     // Search (and isolate) multiple MII PHYs
 *     do {
 *         status = PhyInit(my_adapter, phy, phy_addr,
 *                          PHY_CONN_CURRENT,
 *                          PHY_HWOPTS_DONT_CONNECT);
 *         if (status == PHY_STATUS_NOT_FOUND) {
 *             break;
 *         } else if (status != PHY_STATUS_OK) {
 *             // process other PhyInit() failures ...
 *         }
 *         phy_addr = PHY_ADDR_NEXT(phy->addr);
 *     } while (++phy < &phy_space[MAX_SUPPORTED_PHYS]);
 */
#define PHY_ADDR_MIN            ((BYTE) 0)
#define PHY_ADDR_MAX            ((BYTE) 31)
#define PHY_ADDR_NEXT(addr)     ((BYTE) -((addr) + 1))
#define PHY_ADDR_ANY            PHY_ADDR_NEXT(PHY_ADDR_MAX)
#define PHY_ADDR_MGMT           PHY_ADDR_MIN
#define PHY_ADDR_0              ((BYTE) 0)

#ifdef _H2INC_                  /* MASM H2INC.exe utility workaround */
#undef PHY_ADDR_MIN
#undef PHY_ADDR_MAX
#undef PHY_ADDR_ANY
#undef PHY_ADDR_MGMT
#define PHY_ADDR_MIN            0       /* can't handle type cast?!! */
#define PHY_ADDR_MAX            31
#define PHY_ADDR_ANY            -32     /* can't handle macro func */
#define PHY_ADDR_MGMT           PHY_ADDR_MIN
#define PHY_ADDR_0              0
#endif

/*
 * PhyInit() <hw_options> -- one or more PHY_HWOPTS_* bits as follows.
 * Many HWOPTS are PHY- or adapter-dependent, and will be silently
 * ignored if not supported.
 *
 * PHY_HWOPTS_* Supported Options:
 *
 * NONE         - Don't override any HW defaults
 *
 * REPEATER     - Force PHY Repeater mode such that CRS is only asserted
 *                due to Rx activity.  The default is to force PHY Node
 *                mode which asserts CRS due to Rx or Tx in half-duplex
 *                mode only (802.3u CRS behavior is undefined in full-
 *                duplex mode)
 *
 * READ_ONLY    - Read, but don't write to PHY devices.  Useful as a
 *                diagnostic tool, e.g., the phyck.exe utility to query
 *                MII and SPI registers without disturbing the
 *                configuration maintained by a background NDIS driver.
 *
 * CK25DIS      - PHY dependent, use with caution.  On the NSC,
 *                disables CLK25 pin (Hi-Z) if external clock unused.
 *                This breaks some NICs (SMC 9332, DEC DE500) which
 *                apparently rely on this output.  On the TDK, invokes
 *                their R16.RXCC option, which may have adverse side
 *                effects upon Auto-Negotiation, so the PhyCheck()
 *                routine trys to compensate by enabling upon link up
 *                and disabling upon link down.
 *
 * TX_BP4B5B    - 100Base-TX bypass 4B/5B endec (repeater applications)
 * TX_BPSCR     - 100Base-TX bypass scrambler (100Base-FX applications)
 * TX_BPALIGN   - 100Base-TX bypass all Tx/Rx conditioning
 *
 * HWRESET_MASK - Enumerated set of 15 GPIO hardware reset options:
 *                    HWRESET_GPn     - Active-high reset using GPn
 *                    HWRESET_GPn_NOT - Active-low  reset using GPn
 *                    HWRESET_GPn_R12 - Active-high reset using GPn
 *                                      via obsolete RSS 1161x MAC R12
 *                    HWRESET_ALL     - Active-high reset using GP[3:0]
 *                    HWRESET_ALL_NOT - Active-low  reset using GP[3:0]
 *                    HWRESET_ALL_R12 - Active-high reset using GP[3:0]
 *                                      via obsolete RSS 1161x MAC R12
 *                Where GPn is LAN General Purpose I/O GP0, GP1, GP2
 *                or GP3. The resultant hardware reset pulse is an
 *                active-high signal, unless the _NOT suffix is
 *                specified to generate an active-low hardware reset
 *                pulse.  The _R12 suffix provides a backwards-
 *                compatible option for use with pre-production devices
 *                (RSS 11606, 11610 and 11611 and compatibles).
 *
 *                The _ALL suffix provides a shotgun, wildcard reset
 *                against all GPIO signals, GP[3:0].  This is useful in
 *                cases where none of the GPIO signals are dedicated
 *                as inputs, and a driver is supporting multiple adapter
 *                GPIO configurations and there's no adverse affect
 *                from driving unused GPIO lines.  The _ALL_NOT suffix
 *                provides an active-low signal on all GP[3:0], and the
 *                _ALL_R12 provides an active-high signal on all GP[3:0]
 *                using the antiquated RSS 11611 R12 register interface.
 *
 * ISOLATE      - Disable (tri-state) the active HomePNA or MII device's
 *                data interface and underlying PMD interface.  The
 *                associated SPI or MDC/MDIO Serial Management Interface
 *                remains enabled for subsequent management commands.
 *                This is equivalent to calling PhyDisable().
 *
 * HPNA_        - Allow a remote HomePNA master device to reconfigure
 * CMD_ENABLE     the local HomePNA PHY device's speed and power mode
 *                settings.  This is normally disabled, as the v1.0
 *                device implementations have a bug in which noise
 *                can be mistaken for a remote command, causing
 *                inadvertant reconfiguration.
 *
 * FDX_CRS      - Set PHY-specific CRS option which alters
 *                the CRS behavior in full-duplex mode to assert CRS
 *                during transmission rather than during reception
 *                (its default).  This is an RSS61x MAC SAR-26
 *                workaround which may (?) alleviate Tx-hangs (SAR-23)
 *                in full-duplex mode.
 *
 * DONT_SWRESET - Don't issue (otherwise) mandatory software reset
 *
 * MII_SEARCH_0 - Begin a <PHY_ADDR_ANY> MII search with address-0,
 *                which was the pre-v1.50 PHY Driver default.
 *                Otherwise, perform an MII search from addresses 1..31,
 *                attempting address-0 only if no other MII PHY is
 *                found.  (Some MII PHYs erroneously treat address-0 as
 *                a broadcast address, others have side-effects.)
 *                Unless this option is specified, we try 1..31 followed
 *                by zero.  If the option is specified, we try 0..31.
 *
 * MII_PREAMBLE - If this option is activated, the PHY driver will
 *                always send a 32-bit MDIO preamble during MII reads
 *                and writes.  Otherwise, the PHY driver will attempt
 *                to suppress the 32-bit preamble, provided the device
 *                hardware supports this feature as indicated by the
 *                MII register 1.6 status bit.
 *
 * NO_MII_      - HomePNA-only NICs can save time by eliminating a
 * SEARCH         PhyInit() MII device search.
 *
 * NO_HPNA      - MII-only NICs can save time by eliminating a
 * SEARCH         PhyInit() search for both internal (RSS 11623) and
 *                external HomePNA devices.
 *
 * NO_HPNA_     - RSS 11623 NICs can save time by eliminating a
 * EXT_SEARCH     PhyInit() search for external HomePNA devices.
 *
 * HPNA_PORT_   - If this option is selected, a link indication from
 * PRIORITY       a HPNA device will take precedence over a
 *                simultaneous link indication from a ethernet 10/100 MII
 *                device.  Otherwise the default case is for the higher
 *                speed 10/100 MII device precedence.  This affects
 *                the PhyCheck() link indication in the unlikely case
 *                that both devices report link, as only the highest
 *                priority capability will be reported.  It also has a
 *                side-effect in that the PhyCheck() routine will switch
 *                from an MII to HomePNA PHY device whenever the MII
 *                link goes down (i.e., cable removed from 10/100 Ether
 *                hub).
 *
 * MII_PORT_    - Similar to HPNA_PORT_PRIORITY.  Since by default,
 * PRIORITY       MII > HPNA, this doesn't affect the simultaneous
 *                link status.  However, it does determine the default
 *                port selected (MII) at PhyInit() time and subsequent
 *                to link down.
 *
 * PORT_HOPPING - If this option is selected and multi-port PHY devices
 *                are present, i.e., both 1M8 HomePNA and 10/100 MII
 *                devices found, then the PhyCheck() routine will
 *                force a port hop every LINK_DOWN_HOP_COUNT calls
 *                when the link is down.  The default value of the
 *                "phy.c" LINK_DOWN_HOP_COUNT is 5, which allows
 *                ten seconds per port for a 2-second check period.
 *                This option should not be enabled when the app/driver
 *                polls PhyCheck() repeatedly in an initialization loop.
 *
 *                This option is recommended for broken PHY devices which
 *                cannot achieve link while configured in the MII isolate
 *                mode.  To date, we've found that the National
 *                Semiconductor DP83840 won't work at all when isolated
 *                (it also disables its twisted pair PMD interface), and
 *                the Level One LXT 970 and Enable Semiconductor PHYs
 *                curiously can achieve link when connected to 10 but
 *                not 100 Mbit hubs and switches.
 *
 *                ------------------------------------------------------
 *
 *                Note: the following truth table depicts the complex
 *                interaction among the PHY_HWOPTS_PORT_HOPPING (HOP),
 *                PHY_HWOPTS_HPNA_PORT_PRIORITY (HPNA) and the
 *                PHY_HWOPTS_MII_PORT_PRIORITY (MII) options.  This
 *                table is only applicable when BOTH devices (HomePNA and
 *                MII) are present.  As can be seen, MII priority
 *                primarily affects the PhyInit() default port select,
 *                HPNA priority primarily affects PhyCheck() in the
 *                rare (unlikely?) case that both links are up, and
 *                HOP determines whether a port switch is made when
 *                both links are down.  As a secondary effect when
 *                HOP is disabled, MII supercedes HPNA priority when
 *                both links are down, otherwise HPNA supercedes the
 *                the current, active port which is retained in the
 *                default case where no options are in effect.  Whew!
 *
 *                What's not depicted is the simple case where only one
 *                port is reporting link: that port is always selected,
 *                regardless of multi-port options.
 *
 *                         Multi-Port Selection Truth Table           
 *
 *                +==================++=================+===========+
 *                |                  ||    PhyCheck()   |           |
 *                |  PHY HW option   |+---------+-------+           |
 *                |                  || Both    | Both  | PhyInit() |
 *                +=====+======+=====+| Links   | Links | Default   |
 *                | HOP | HPNA | MII || Down**  | Up    | Port      |
 *                +=====+======+=====++=========+=======+===========+
 *     (default)  |  0  |   0  |  0  || current |  MII  |   HPNA    |
 *                +-----+------+-----++---------+-------+-----------+
 *                |  0  |   0  |  1  ||   MII*  |  MII  |    MII*   |
 *                +-----+------+-----++---------+-------+-----------+
 *                |  0  |   1  |  0  ||  HPNA*  | HPNA* |   HPNA    |
 *                +-----+------+-----++---------+-------+-----------+
 *                |  0  |   1  |  1  ||   MII*  | HPNA* |    MII*   |
 *                +=====+======+=====++=========+=======+===========+
 *                |  1  |   0  |  0  ||  other* |  MII  |   HPNA    |
 *                +-----+------+-----++---------+-------+-----------+
 *                |  1  |   0  |  1  ||  other* |  MII  |    MII*   |
 *                +-----+------+-----++---------+-------+-----------+
 *                |  1  |   1  |  0  ||  other* | HPNA* |   HPNA    |
 *                +-----+------+-----++---------+-------+-----------+
 *                |  1  |   1  |  1  ||  other* | HPNA* |    MII*   |
 *                +-----+------+-----++---------+-------+-----------+
 *
 *                *  - Indicates behavior change with respect to the
 *                     default action.
 *
 *                ** - With both links down, "current" denotes the
 *                     currently active port (default or last link),
 *                     whereas "other" denotes a swap to the other,
 *                     presently inactive port.  All port switching is
 *                     governed by the nominal LINK_DOWN_HOP_COUNT
 *                     value of 5, which yields a 10-second rate
 *                     assuming a periodic, 2-second PhyCheck() cycle.
 *                     As such, a link-down port switch won't occur
 *                     for ten seconds, though a single- or dual-port
 *                     link-up event will instanly effect a port switch.
 *
 *                ------------------------------------------------------
 *
 * MAC_         - RSS 11617 and 11623 feature: If this option is selected,
 * PORT_MGMT      the PHY Driver's PhyInit() and PhyCheck() routines will
 *                assume management of the MAC Network Access Register
 *                (NAR, R6 offset 0x30) configuration of PHY port select
 *                bits [18:17], full-duplex mode select bit [09], and
 *                the 10/100 speed select bit [22].  All other NAR register
 *                bits will be retained intact, with the exception that
 *                if the MAC's transmitter and/or receiver are currently
 *                running, they may be temporarily stopped (this can take
 *                up to 5-10 milliseconds) while all MII and HomePNA PHY
 *                devices are tri-stated, the MAC port is switched, an
 *                active MII or HomePNA PHY is enabled and then the prior
 *                receiver and transmitter states are restored.
 *
 *                In addition, if a non-null <soft_nar> is passed to the
 *                PhyInit() routine, the caller can specify the address of
 *                its 32-bit "soft NAR" register value field which
 *                presumably contains a driver's local, shadow copy of
 *                the NAR register contents.  This is HIGHLY recommended
 *                in conjunction with PHY_HWOPTS_MAC_PORT_MGMT to avoid
 *                coherency mismatches between the hardware and shadow
 *                software copies of the NAR register.  In this manner,
 *                both the MAC device driver and the PhyCheck() routine
 *                can safely manipulate the NAR register in a single-
 *                threaded (or blocked deserialized) driver.
 *
 * FAST_INIT    - Avoids time consuming MII and SPI searches and device
 *                parsing PHY devices previously initialized via
 *                PhyInit().  Effectively re-uses the values discovered
 *                during the initial PhyInit() call.
 *
 *                (Not yet supported)
 *
 * STRICT_      - Default unspecified provides "loose" checking by the
 * PARM_          PhyConnectSet() routine, which is called by PhyInit().
 * CHECKING       When specified, PhyConnectSet() will reject a
 *                connection, reporting PHY_STATUS_UNSUPPORTED_MEDIA,
 *                if *any* <connect> parameter media setting is not
 *                supported by the device.  In contrast, when
 *                unspecified, PhyConnectSet() will accept the
 *                directive if *at least one* <connect> media setting
 *                is supported by the device(s).  For example, assume
 *                a 100Base-TX device supporting both full- and
 *                half-duplex 100Base-TX and 10Base-T operation.  By
 *                default (#undefined), a <connect> request to
 *                Auto-Negotiate for these capabilities plus an
 *                unsupported 100Base-T2 mode is accepted.  In contrast,
 *                the same <connect> request is rejected due to
 *                unsupported -T2 service, even though the other
 *                services are available.
 *
 *                It's likely that NIC drivers (NDIS) won't specify
 *                this option in order to wildcard the connect
 *                directive to "whatever's possible" whereas diagnostic
 *                or test utilities may use the option to verify that
 *                expected devices are present.
 *
 * ACTIVE_AN_   - Restarts Auto-negotiation whenever the MII port
 * RESTART        becomes newly active, i.e., transitions from disabled
 *                (isolated) to enabled (de-isolated) state.  The
 *                NSC PHY automatically gets this setting; other PHYs
 *                may also need to be kicked when coming out of
 *                isolation.
 *
 * ALT_1HZ_     - Use alternate, 1-Hz Conexant link detection algorithm
 * HPNA_LINK      instead of the standard 0.5 Hz HomePNA algorithm.
 *                This version only transmits a heartbeat packet if
 *                neither a PHY-level transmit or receive indication
 *                occurred during the previous 1-second sample interval.
 *                This is the preferred option for the 1161x chipset
 *                family to avoid excessive or late collision
 *                problems.
 *
 * X3_DEVICE    - Force CN7221 (11625) "Ramses" X3 device version
 *                settings.
 *
 * GP0_SPI_DOUT - Use alternate Ramses SPI DOUT interface such that
 *                LAN_GPIO[0] is read in lieu of EEDO and ROM_CS#
 *                is used in lieu of LAN_GPIO[0] for Ramses RESET#.
 *
 * --------------
 *
 * NIC_DEFAULT  - Recommended default (your implementation may vary)
 * NIC_*        - Various adapter-specific options
 *
 * --------------
 * Discontinued PHY_HWOPTS_* Options (No Longer Supported):
 *
 * DEFAULTS     - Replaced by PHY_HWOPTS_NONE to avoid confusion
 * NODE         - !REPEATER implies NODE, no need for separate control
 * LED_COL      - Obscure NSC-only option to change COL LED definition
 * PRE_HWRESET  - Superceded by HWRESET_GP2..5* settings
 * POST_HWRESET - No compelling need for a hardware reset subsequent
 *                to the MII search phase of PhyInit().  The updated
 *                HWRESET_GP2..5* options present ample flexibility for
 *                NICs whose GPIO are tied to a PHY Reset.
 * DONT_CONNECT - Replaced by ISOLATE
 * ISR          - Enable PHY interrupts upon change in link state
 */

/* Supported Options */
#define PHY_HWOPTS_NONE                 0x00000000L
#define PHY_HWOPTS_REPEATER             0x00000001L
#define PHY_HWOPTS_READ_ONLY            0x00000002L
#define PHY_HWOPTS_CK25DIS              0x00000004L
#define PHY_HWOPTS_TX_BP4B5B            0x00000008L
#define PHY_HWOPTS_TX_BPSCR             0x00000010L
#define PHY_HWOPTS_TX_BPALIGN           0x00000020L
#define PHY_HWOPTS_HWRESET_MASK         0x000003C0L /* listed below */
#define PHY_HWOPTS_ISOLATE              0x00000400L
#define PHY_HWOPTS_HPNA_CMD_ENABLE      0x00000800L
#define PHY_HWOPTS_FDX_CRS              0x00001000L
#define PHY_HWOPTS_DONT_SWRESET         0x00002000L
#define PHY_HWOPTS_MII_SEARCH_0         0x00004000L
#define PHY_HWOPTS_MII_PREAMBLE         0x00008000L
#define PHY_HWOPTS_NO_ETH_SEARCH        0x00010000L
#define PHY_HWOPTS_NO_HPNA_SEARCH       0x00020000L
#define PHY_HWOPTS_NO_HPNA_EXT_SEARCH   0x00040000L
#define PHY_HWOPTS_HPNA_PORT_PRIORITY   0x00080000L
#define PHY_HWOPTS_MII_PORT_PRIORITY    0x00100000L
#define PHY_HWOPTS_PORT_HOPPING         0x00200000L
#define PHY_HWOPTS_MAC_PORT_MGMT        0x00400000L
#define PHY_HWOPTS_FAST_INIT            0x00800000L
#define PHY_HWOPTS_STRICT_PARM_CHECKING 0x01000000L
#define PHY_HWOPTS_ACTIVE_AN_RESTART    0x02000000L
#define PHY_HWOPTS_ALT_1HZ_HPNA_LINK    0x04000000L // obsolete for JEDI
#define PHY_HWOPTS_X3_DEVICE            0x08000000L
#define PHY_HWOPTS_GP0_SPI_DOUT         0x10000000L
#define PHY_HWOPTS_HPNA7WS              0x20000000L
#ifdef P51
#define PHY_HWOPTS_P51_HLAN             0x40000000L
#endif

/*
 * PHY HW Reset enumerations via GPIO.  A _GP0.._GP3 suffix identifies
 * the general purpose I/O output port used to generate a nominal
 * one microsecond, active-high hardware reset pulse.  An additional
 * _NOT suffix specifies an active-low reset pulse.  The QSI PHY is an
 * example of a device requiring an active-low reset.
 *
 * Beginning with the production RSS 11617 devices, a.k.a. 11611
 * version 5 (v5), the GPIO register migrated from MAC register R12 to
 * R15.  As such, use of pre-production RSS 11611 (v3 and v4), 11610,
 * 11606 and compatible devices is facilitated via the _R12 suffix
 * which directs the PHY driver to access GPIO via the antiquated R12
 * device interface.  Note that the _NOT active-low reset capability
 * is not supported using these older, obsoleted pre-production devices.
 * This is strictly a PHY Driver software constraint, and not a hardware
 * restriction of the earlier devices.
 *
 * The _ALL suffixes provide wildcard (shotgun) resets using all four
 * GPIO[3:0] signal lines.  _ALL and _ALL_NOT provide respective
 * active-high and active-low reset pulses via the MAC's R15 register,
 * whereas _ALL_R12 provides an active-high reset pulse using the
 * antiquated MAC R12 register for older device support.
 *
 *      PHY_HWOPTS_HWRESET_MASK         0x03C0L
 */
#define PHY_HWOPTS_HWRESET_GP0          0x0040L /* Active-high resets */
#define PHY_HWOPTS_HWRESET_GP1          0x0080L
#define PHY_HWOPTS_HWRESET_GP2          0x00C0L
#define PHY_HWOPTS_HWRESET_GP3          0x0100L
#define PHY_HWOPTS_HWRESET_GP0_NOT      0x0140L /* Active-low resets */
#define PHY_HWOPTS_HWRESET_GP1_NOT      0x0180L
#define PHY_HWOPTS_HWRESET_GP2_NOT      0x01C0L
#define PHY_HWOPTS_HWRESET_GP3_NOT      0x0200L
#define PHY_HWOPTS_HWRESET_GP0_R12      0x0240L /* Obsoleted R12 GPIO */
#define PHY_HWOPTS_HWRESET_GP1_R12      0x0280L
#define PHY_HWOPTS_HWRESET_GP2_R12      0x02C0L
#define PHY_HWOPTS_HWRESET_GP3_R12      0x0300L
#define PHY_HWOPTS_HWRESET_ALL          0x0340L /* Wildcard, all GPIO */
#define PHY_HWOPTS_HWRESET_ALL_NOT      0x0380L
#define PHY_HWOPTS_HWRESET_ALL_R12      0x03C0L

/*
 * PHY_HWOPTS_NIC_* NIC-specific Hardware Options
 *
 * Usage is primarily intended for RSS test boards.  OEMs should use
 * PHY_HWOPTS_NIC_DEFAULT or may #define their own NIC-specific set of
 * options as necessary.  PHY_HWOPTS_NIC_EN5032 is an example of such
 * an option.
 * 
 * The RSS 11617 (a.k.a. 11611 v5) is the first production LANfinity
 * MAC device to use the newer, R15 GPIO register.  All others are
 * pre-production or compatible devices which use the older R12 GPIO
 * register, hence must use the _R12 suffix to direct the PHY driver
 * to access GPIO via R12.
 *
 * The 11623 (RS7112) with integrated HomePNA PHY, uses GP0 as an
 * external 10/100 PHY link LED to support the optional PC 98/99
 * wake-on-link capability.
 *
 * The 11617 (RS7111A) reference board, plus pre-production 11611 and
 * 11610 test boards have GP0 tied to PHY hardware reset.  The original
 * 11606 test board uses GP3 as PHY hardware reset.
 */
#define PHY_HWOPTS_NIC_DEFAULT          PHY_HWOPTS_NONE
/* Changed PHY_HWOPTS_NIC_RSS11623 define to support 11623/Ramses board.*/
#define PHY_HWOPTS_NIC_RSS11623         \
    (PHY_HWOPTS_ALT_1HZ_HPNA_LINK | PHY_HWOPTS_HWRESET_GP0_NOT)     
#define PHY_HWOPTS_NIC_RSS11611         PHY_HWOPTS_HWRESET_GP0_R12
#define PHY_HWOPTS_NIC_RSS11610         PHY_HWOPTS_HWRESET_GP0_R12
#define PHY_HWOPTS_NIC_RSS11606         PHY_HWOPTS_HWRESET_GP3_R12
#define PHY_HWOPTS_NIC_EN1207           PHY_HWOPTS_NIC_DEFAULT
#define PHY_HWOPTS_NIC_EN5032           PHY_HWOPTS_NIC_DEFAULT
#define PHY_HWOPTS_NIC_OEM_GP0          PHY_HWOPTS_HWRESET_GP0
#define PHY_HWOPTS_NIC_OEM_GP0_NOT      PHY_HWOPTS_HWRESET_GP0_NOT

/* Discontinued Options (no longer supported) */
#undef PHY_HWOPTS_DEFAULTS      /* was 0x0000 */
#undef PHY_HWOPTS_NODE          /* was 0x0002 */
#undef PHY_HWOPTS_LED_COL       /* was 0x0008 */
#undef PHY_HWOPTS_PRE_HWRESET   /* was 0x0080 */
#undef PHY_HWOPTS_POST_HWRESET  /* was 0x0100 */
#undef PHY_HWOPTS_DONT_CONNECT  /* was 0x0400 */
#undef PHY_HWOPTS_ISR           /* was 0x0800 */

/*
 * Result codes for many PHY Driver APIs, e.g., PhyInit(), software
 * PhyReset(), PhyDisable(), PhyEnable(), PhyConnectSet(), etc.
 */
typedef enum _phy_status_ {
    PHY_STATUS_OK = 0,
    PHY_STATUS_NOT_FOUND,               /* PhyInit() errors ... */
    PHY_STATUS_UNSUPPORTED_FUNCTION,
    PHY_STATUS_PORTABILITY_ERROR,
    PHY_STATUS_RESET_ERROR,             /* PhyReset() bit stuck */
    PHY_STATUS_UNSUPPORTED_MEDIA,       /* PhyConnSet() error */
    PHY_STATUS_UNINITIALIZED,
    PHY_STATUS_ERROR_MISC 
    /* etc. */
} PHY_STATUS;

/* PhyEnable() and PhyDisable() <port_select> argument values */
typedef enum _phy_port_ {
    /* physical ports */
    PHY_PORT_NONE     = 0x00,
    PHY_PORT_MII      = 0x01,   /* 10/100 MII data signals ... */
    PHY_PORT_MII_PMD  = 0x02,   /* and associated MDI twisted pair */
    PHY_PORT_HPNA_7WS = 0x04,   /* HPNA 7-wire serial data */
    PHY_PORT_HPNA_MII = 0x08   /* HPNA Mii */
} PHY_PORT;
#define PHY_PORT_MII_DEVICE      (PHY_PORT_MII | PHY_PORT_MII_PMD)
#define PHY_PORT_HPNA_DEVICES    (PHY_PORT_HPNA_7WS | PHY_PORT_HPNA_MII)
#define PHY_PORT_ALL             \
    (PHY_PORT_MII_DEVICE | PHY_PORT_HPNA_DEVICES)

/* PhyLoopback() <mode> options */
typedef enum _phy_loopback_ {
    PHY_LOOPBACK_OFF = 0,       /* default  - loopback disabled */
    PHY_LOOPBACK_MII,           /* short    - MAC/PHY @ MII */
    PHY_LOOPBACK_PMD,           /* long     - MAC/PHY @ PMD Tx/Rx */
    PHY_LOOPBACK_REMOTE,        /* external - PMD Rx to Tx (BER test) */
    PHY_LOOPBACK_10BASE_T       /* ???      - NSC compatibility mode */
} PHY_LOOPBACK;


/* PHY device descriptor */

/* PHY device descriptor */
#define MAX_MII_PHY		2
#define JEDI_MII_PHY		0
#define ETH_MII_PHY		1


typedef struct _phy_ {
#ifndef _H2INC_
    struct _phy_       *next;   /* Nominal value/usage here */
#else
    DWORD              *next;   /* MASM H2INC.exe utility workaroud */
#endif
    void				*LinkLayerAdapter;	// HPNALLOBJ
    int					frame_ctl_length ; // 4 bytes prefix control for JEDI phy, 0: for ethernet phy
    PHY_PORT            devices;			// indicate what PHYS on the board
	// set by ??Phy_SetUp can be PHY_PORT_HPNA_DEVICES [|] PHY_PORT_MII_DEVICE
    PHY_PORT            active_port;
    PHY_PORT            inactive_port;
    PHY_PORT            enabled_port;   /* nominally == active_port */
    DWORD               link_down_count;

	MIIOBJ *pActive_miiphy ;		// point to active mii_phy
	MIIOBJ mii_phy[MAX_MII_PHY] ;

//	{	// for Ethernet Phy
	PHY_PORT            mii_port;
	BOOLEAN             mii_isolated;

	PHY_LOOPBACK        loopback;		// not work for JEDI 
	WORD                anar;			// ETHERNET only, not for JEDI
//	}

//	{	// for Jedi HPNA Phy
	PHY_PORT            hpna_port;
   BOOLEAN             hpna_link_pending;
	BOOLEAN             hpna_isolated;

	WORD                ramse_ctrl_reg; /* For ramses only. Shadow of control register */

	int	V1_Detect_Count ;
#ifdef H2_TX_WORKAROUND
	DWORD JediPhy_Tx_Buffer_Len ;		
	DWORD JediPhy_Tx_Buffer_Left ;
#endif
//	}
    struct {
        DWORD               capability;
        DWORD               connect;
        DWORD               mii_link_state;
        DWORD               hpna_link_state;
        DWORD               cum_link_state;     /* PhyCheck() report */
    }                   media;
    DWORD               initialized;

    DWORD               hw_options;

#ifdef JEDIPHY	// Bright
    OSFILEOBJ	osfile ;	
#endif
} PHY;


/* Device initialization code used by the PHY Driver */
#define PHY_INIT_CODE           0x29FD7A64L  /* magic number */
#define IS_PHY_INITIALIZED(p)   ((p)->initialized == PHY_INIT_CODE)

/* PHY ID checks, exclusive of 4-bit vendor revision code */
#define PHYID_RSS_20417(p)      (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x01825010L)
#define PHYID_NSC_DP83840(p)    (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x20005C00L)
#define PHYID_QSI_6612(p)       (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x01814400L)
#define PHYID_LXT_970(p)        (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x78100000L)
#define PHYID_LXT_971(p)        (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x001378E0L)
#define PHYID_GEC_NWK936(p)     (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x02821C20L)
#define PHYID_TDK_2120(p)       (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x0300E540L)
#define PHYID_DAVICOM_9101(p)   (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x0181B800L)
#define PHYID_ICS_1890(p)       (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x0015F420L)
#define PHYID_ENABLE_5V(p)      (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x00437410L)
#define PHYID_ENABLE_3V(p)      (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x00437420L)

/* PHY ID revision codes */
#define PHYID_REV(p)            ((p)->mii_phy[ETH_MII_PHY].mii_id & 0xFL)

/*
 * PHY operational mode functions
 *
 * The three most commonly used PHY Driver functions are PhyInit(),
 * PhyCheck(), and PhyWakeOnLink().  PhyInit() is called
 * upon DriverInit() and subsequent DriverReset() routines.  PhyCheck()
 * may be repetitively called
 * at DriverInit() or DriverReset() time to determine an intial
 * link status, and then periodically invoked, e.g., upon a two-second
 * DriverCheck() interval.  PhyWakeOnLink() simply enables or disables
 * the MII and/or HomePNA device's wake-up signals.  The caller is
 * responsible for other set-ups (e.g., OS and 11623 dependent).
 *
 *
 * PhyInit()           - Find, reset, initialize and attach PHY to
 *                       media, PhyInit() is the first function which
 *                       must be called before accessing a PHY device.
 *                       Returns PHY_STATUS_OK upon successful search
 *                       and initialization of single or combination
 *                       10/100 MII and 1M8 HomePNA PHY devices.  The
 *                       Parameter list is as follows:
 *
 *                         DWORD  adapter    - MAC device I/O base addr
 *                         PHY   *phy        - allocated data struct space
 *                         BYTE   phy_addr   - MII PHY address or wildcard
 *                         DWORD  connect    - PhyConnectSet() directive
 *                         DWORD  hw_options - zero or more PHY_HWOPTS_*
 *                         DWORD *soft_nar   - null, or used with
 *                                             PHY_HWOPTS_MAC_PORT_MGMT
 *
 * PhyReset()          - Issue PHY software reset
 *
 * PhyConnectGet()     - Get PHY connection parameters (Auto-Negotiation
 *                       on/off, 10/100 Mbps speed, Full-Duplex on/off),
 *                       plus PHY_CONN_TECHNOLOGY specific settings
 *
 * PhyConnectSet()     - Set PHY connection parameters
 *
 * PhyDisable()        - Disable/isolate MII (or SPI) and/or PMD ports
 *
 * PhyEnable()         - Attach (previously isolated) PHY(s) to media
 *
 * PhyCapbilityGet()   - Get device-dependent PHY_CONNECT_ABILITY mask
 *
 * PhyCheck ()         - Report operational link state (link down/up,
 *                       1/10/100 Mbps speed, half/full-duplex mode)
 *                       plus any error indications
 *
 * PhyLastLinkUp()     - Report last known link state reported by
 *                       PhyCheck().  A zero return value indicates link
 *                       has never been established.  Note that the link
 *                       state is zeroed (cleared) whenever PhyInit() or
 *                       PhyConnectSet() is invoked.
 *
 * PhyShutdown()       - Disables PHY and places in power-down mode
 *
 * PhyConfigurePort()  - Manually switches a multi-port PHY device per
 *                       PhyCheck() <link_state>.  If the PhyInit() option
 *                       PHY_HWOPTS_MAC_PORT_MGMT is selected, this
 *                       function is automatically called by PhyCheck()
 *                       upon port change, and will additionally manage
 *                       the RSS 116xx Network Access Register (NAR)
 *                       port/speed/duplex/heartbeat settings.  Otherwise,
 *                       the caller must manually manage the NAR and
 *                       call this function whenever a PHY port change
 *                       or PHY_LINK_NEW_MEDIA is detected.  Note that a
 *                       PHY port change can occur even when the link
 *                       is down!
 *
 * PhyWakeOnLink()     - When enabled, enables the HomePNA interrupt
 *                       (IMASK) based on receive activity and/or
 *                       sets external, MII-based GPIO for input to
 *                       receive a device LINK LED transition.
 */
#include "phy.h"
                             
#endif /* _PHY_H_ */
