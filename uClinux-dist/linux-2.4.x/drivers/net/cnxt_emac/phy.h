/****************************************************************************
*
*	Name:			phy.h
*
*	Description:	
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
*  $Author:   richarjc  $
*  $Revision:   1.1  $
*  $Modtime:   Mar 27 2003 08:01:52  $
****************************************************************************/

#ifndef _PHY_H_
#define _PHY_H_

#include "phytypes.h"
// PHY object interfaces for other objects
// #define NON_BYPASSMODE	1	// test

#ifndef _PHYOBJ_H_
// these definition are required for interfaces
// the superset defined inside phyobj.h

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

#define PHY_LINK_UP             0x80000000L
#define PHY_LINK_NEW_MEDIA      0x40000000L
#define PHY_LINK_FDX            0x20000000L
#define PHY_LINK_AUTONEG        0x10000000L
#define PHY_LINK_100            0x04000000L
#define PHY_LINK_10             0x02000000L
#define PHY_LINK_HPNA_10        0x01000000L

#define PHY_LINK_HPNA20         0x00000800L /* (3) PHY_LINK_TECHNOLOGY ... */

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



#define PHY_CONN_CURRENT                0
#define PHY_CONN_AUTONEG                PHY_LINK_AUTONEG
#define PHY_CONN_HPNA20                 PHY_LINK_HPNA20
#define PHY_CONN_100BASE_T2FD           PHY_LINK_100BASE_T2FD
#define PHY_CONN_100BASE_T2             PHY_LINK_100BASE_T2
#define PHY_CONN_AUTONEG_FDX_PAUSE      PHY_LINK_FDX_PAUSE
#define PHY_CONN_100BASE_T4             PHY_LINK_100BASE_T4
#define PHY_CONN_100BASE_TXFD           PHY_LINK_100BASE_TXFD
#define PHY_CONN_100BASE_TX             PHY_LINK_100BASE_TX
#define PHY_CONN_10BASE_TFD             PHY_LINK_10BASE_TFD
#define PHY_CONN_10BASE_T               PHY_LINK_10BASE_T

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

/* Supported Options */
#define PHY_HWOPTS_NONE                 0x00000000L
#define PHY_HWOPTS_ISOLATE              0x00000400L
#define PHY_HWOPTS_DONT_SWRESET         0x00002000L
#define PHY_HWOPTS_MII_PREAMBLE         0x00008000L
#define PHY_HWOPTS_NO_ETH_SEARCH        0x00010000L
#define PHY_HWOPTS_NO_HPNA_SEARCH       0x00020000L
#define PHY_HWOPTS_HPNA_PORT_PRIORITY   0x00080000L
#define PHY_HWOPTS_MII_PORT_PRIORITY    0x00100000L
#define PHY_HWOPTS_PORT_HOPPING         0x00200000L
#define PHY_HWOPTS_HPNA7WS              0x20000000L
#ifdef P51
#define PHY_HWOPTS_P51_HLAN             0x40000000L
#endif

#define PHY void
#endif


PHY * PhyAllocateMemory( void) ;
void PhyFreeMemory( PHY *pPhy ) ;

PHY_STATUS   PhyInit(
						DWORD	mac_address,
						PHY		*phy,
                        DWORD	connect,
                        DWORD	hw_options, 
						void	*pHpnaObj
					);	// for HPNA 2.0 LL compliance

DWORD PhyCheck(PHY *phy);
BOOLEAN PhyActiveHPNA(PHY *phy) ;

#if HOMEPLUGPHY
BOOLEAN PhyActiveHOMEPLUG(PHY *phy) ;
#endif

DWORD PhyIdGet(PHY *phy) ;
void PhyShutdown(PHY *phy) ;

// Check if Any Phy attached to EMAC port
// input : 
//		Mac_Addr : beginnig of the emac mac address
//		mii_addr : if 0xff : search 0 to 31
//					  else search specified mii_addr (faster)
BOOLEAN Phy_EmacPortAttached(DWORD Mac_Addr, BYTE mii_addr) ;

/*
	PHY * PhyAllocateMemory( void) ;
		Allocte memory for PHY object
	 Parameters :
	 Return Value :
		NULL if fail, else return a pointer to the PHY object
	 Comment :
		. the caller should save the returned value for other services provided 
		by this module.
*/

/*
	void PhyFreeMemory( PHY *pPhy ) ;
		Free memory for PHY object
	 Parameters :
		PHY *pPhy: pointer to the PHY object
	 Return Value :
	 Comment :
*/


/*
	PHY_STATUS   PhyInit(DWORD  mac_address, PHY   *phy, DWORD  connect,
	DWORD  hw_options, PVOID pHpnaObj );	
		Initialize PHY and attach to media (w/optional override), PhyInit do not
		perform hardware reset of the PHYs. Hardware reset is MAC operation, should
		be called before PhyInit(if required).
	 Parameters :
		DWORD  mac_address: MAC device I/O base address
		PHY   *phy: allocated PHY data structure space 
		DWORD  connect: used either to (a) force a selected line speed (10 or 100 Mbps)
			) and mode (full- or half-duplex), or (b) to set a list of Auto-Negotiation
			(A-N) options when the PHY_CONN_AUTONEG bit is set.  It can be a OR value of 
			PHY_CONN-xxx bits defined above. The default value is PHY_CONN_AUTONEG_ALL_MEDIA.
		DWORD  hw_options: one or more PHY_HWOPTS_* bits as follows.
			ISOLATE - Disable (tri-state) the active HomePNA or MII device's
				data interface and underlying PMD interface.  The
				associated 7WS or MDC/MDIO Serial Management Interface
				remains enabled for subsequent management commands.
				This is equivalent to calling PhyDisable().
			DONT_SWRESET - Don't issue (otherwise) mandatory software reset
			MII_PREAMBLE - If this option is activated, the PHY driver will
				always send a 32-bit MDIO preamble during MII reads
				and writes.  Otherwise, the PHY driver will attempt
				to suppress the 32-bit preamble, provided the device
				hardware supports this feature as indicated by the
				MII register 1.6 status bit.
			PHY_HWOPTS_NO_ETH_SEARCH - do not search Ethernet PHY
			PHY_HWOPTS_NO_HPNA_SEARCH - do not search HPNA(JEDI) PHY
			HPNA_PORT_PRIORITY - If this option is selected, a link indication 
				from a HPNA device will take precedence over a
				simultaneous link indication from a ethernet 10/100 MII
				device.  Otherwise the default case is for the higher
				speed 10/100 MII device precedence.  This affects
				the PhyCheck() link indication in the unlikely case
				that both devices report link, as only the highest
				priority capability will be reported.  It also has a
				side-effect in that the PhyCheck() routine will switch
				from an MII to HomePNA PHY device whenever the MII
				link goes down (i.e., cable removed from 10/100 Ether
				hub).

			MII_PORT_PRIORITY  - Similar to HPNA_PORT_PRIORITY.  Since by default,
				MII > HPNA, this doesn't affect the simultaneous link status. 
				However, it does determine the default port selected (MII) 
				at PhyInit() time and subsequent to link down.

			PORT_HOPPING - If this option is selected and multi-port PHY devices
				are present, i.e., both HomePNA and 10/100 MII
				devices found, then the PhyCheck() routine will
				force a port hop every LINK_DOWN_HOP_COUNT calls
				when the link is down.  The default value of the
				"phy.c" LINK_DOWN_HOP_COUNT is 5, which allows
				ten seconds per port for a 2-second check period.
				This option should not be enabled when the app/driver
				polls PhyCheck() repeatedly in an initialization loop.

				This option is recommended for broken PHY devices which
				cannot achieve link while configured in the MII isolate
				mode.  To date, we've found that the National
				Semiconductor DP83840 won't work at all when isolated
				(it also disables its twisted pair PMD interface), and
				the Level One LXT 970 and Enable Semiconductor PHYs
				curiously can achieve link when connected to 10 but
				not 100 Mbit hubs and switches.

				------------------------------------------------------

				Note: the following truth table depicts the complex
				interaction among the PHY_HWOPTS_PORT_HOPPING (HOP),
				PHY_HWOPTS_HPNA_PORT_PRIORITY (HPNA) and the
				PHY_HWOPTS_MII_PORT_PRIORITY (MII) options.  This
				table is only applicable when BOTH devices (HomePNA and
				MII) are present.  As can be seen, MII priority
				primarily affects the PhyInit() default port select,
				HPNA priority primarily affects PhyCheck() in the
				rare (unlikely?) case that both links are up, and
				HOP determines whether a port switch is made when
				both links are down.  As a secondary effect when
				HOP is disabled, MII supercedes HPNA priority when
				both links are down, otherwise HPNA supercedes the
				the current, active port which is retained in the
				default case where no options are in effect.  Whew!

				What's not depicted is the simple case where only one
				port is reporting link: that port is always selected,
				regardless of multi-port options.
 
                          Multi-Port Selection Truth Table           
 
                 +==================++=================+===========+
                 |                  ||    PhyCheck()   |           |
                 |  PHY HW option   |+---------+-------+           |
                 |                  || Both    | Both  | PhyInit() |
                 +=====+======+=====+| Links   | Links | Default   |
                 | HOP | HPNA | MII || Down**  | Up    | Port      |
                 +=====+======+=====++=========+=======+===========+
      (default)  |  0  |   0  |  0  || current |  MII  |   HPNA    |
                 +-----+------+-----++---------+-------+-----------+
                 |  0  |   0  |  1  ||   MII*  |  MII  |    MII*   |
                 +-----+------+-----++---------+-------+-----------+
                 |  0  |   1  |  0  ||  HPNA*  | HPNA* |   HPNA    |
                 +-----+------+-----++---------+-------+-----------+
                 |  0  |   1  |  1  ||   MII*  | HPNA* |    MII*   |
                 +=====+======+=====++=========+=======+===========+
                 |  1  |   0  |  0  ||  other* |  MII  |   HPNA    |
                 +-----+------+-----++---------+-------+-----------+
                 |  1  |   0  |  1  ||  other* |  MII  |    MII*   |
                 +-----+------+-----++---------+-------+-----------+
                 |  1  |   1  |  0  ||  other* | HPNA* |   HPNA    |
                 +-----+------+-----++---------+-------+-----------+
                 |  1  |   1  |  1  ||  other* | HPNA* |    MII*   |
                 +-----+------+-----++---------+-------+-----------+
 
                 *  - Indicates behavior change with respect to the
                      default action.
 
                 ** - With both links down, "current" denotes the
                      currently active port (default or last link),
                      whereas "other" denotes a swap to the other,
                      presently inactive port.  All port switching is
                      governed by the nominal LINK_DOWN_HOP_COUNT
                      value of 5, which yields a 10-second rate
                      assuming a periodic, 2-second PhyCheck() cycle.
                      As such, a link-down port switch won't occur
                      for ten seconds, though a single- or dual-port
                      link-up event will instanly effect a port switch.
			HPNA7WS - If this option is activated, the PHY driver will support
				HPNA(JEDI) with 7 wire istead of MII mode


		PVOID pHpnaObj : a pointer of HPNALLOBJ
	 Return Value
*/

/*
	DWORD PhyCheck(PHY *phy)
		To get the current PHY states. The driver can call this function at a timer 
		routine to know the current PHY states from the return value to update the
		the driver states. Beware that, if return value indicate PHY_LINK_NEW_MEDIA,
		the PhyCheck would already reconfigure the PHY and MAC to the correct states,
		but the current TX and RX of MAC stopped. Under this scenario, driver need to 
		restart TX and RX of MAC if required.

	 Parameters :
		PHY *phy :
	 Return Value : return a OR value of the following bits, please iqnore the
		other bits not describes in this document.
 		PHY_LINK_SPEED - Set with PHY_LINK_UP to indicate 1/10/100
                       Mbit/s line speed, as follows.  One and only
                       one of the following PHY_LINK_SPEED bits will
                       be set when PHY_LINK_UP is set.  If PHY_LINK_UP
                       is not set, then none of the PHY_LINK_SPEED
                       bits will be set.
 
			PHY_LINK_100 - if PHY_LINK_UP && ETHERNET 100 Mbit/s
			PHY_LINK_10  - if PHY_LINK_UP &&  ETHERNET 10 Mbit/s
			PHY_LINK_HPNA_10  - if PHY_LINK_UP &&   HPNA 10 Mbit/s

		PHY_LINK_UP        - Set when the link is operational
		PHY_LINK_NEW_MEDIA - Set upon initial PHY_LINK_UP, and whenever
                       PHY_LINK_UP is true and a media state change
                       (speed and/or duplex mode) has been detected
                       since the last PhyCheck().  This status
                       can be used as a signal to reconfigure a MAC's
                       speed, duplex and port select settings.
*/

/*
	BOOLEAN PhyActiveHPNA(PHY *phy)
		check if current active PHY is HPNA (JEDI)
	 Parameters :
		PHY *phy :
	 Return Value :
		TRUE : the current active PHY is HPNA (JEDI)
		FLASE : the current active PHY is ETHERNET(10/100) or none
*/

/*
	BOOLEAN PhyActiveHOMEPLUG(PHY *phy)
		check if current active PHY is HOMEPLUG (TESLA)
	 Parameters :
		PHY *phy :
	 Return Value :
		TRUE : the current active PHY is HOMEPLUG
		FLASE : the current active PHY is not HOMEPLUG
*/

/*
	DWORD PhyIdGet(PHY *phy)
		get MII ID
	 Parameters :
		PHY *phy :
	 Return Value :
		DWORD mii id from register 2 and 3
*/
#endif
