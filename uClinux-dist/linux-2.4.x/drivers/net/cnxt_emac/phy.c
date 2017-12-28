/****************************************************************************
*
*	Name:			phy.c
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
*  $Modtime:   Mar 27 2003 13:14:40  $
****************************************************************************/

/*
 *  Module Name: phy.c
 *  Author: Joe Bonaker
 *
 *  Copyright © Conexant Systems 1999.
 *  Copyright © Rockwell Semiconductor Systems 1997-1998.
 *
 *  Module Description: 100Base-T PHY driver module.  Consists of PHY
 *  primitives supported by underlying "mii.c" MII and adapter/OS
 *  services.  The primary services used are PhyInit() and a periodic
 *  PhyCheck(), the latter of which can be periodically invoked and/or
 *  dispatched upon PHY-to-MAC generated interrupt, depending on
 *  adapter configuration.
 *
 *  Conditional Compilation Switches:
 *
 *     The following conditional compilation switches apply, and may
 *     be overridden by the user at compile-time.
 *
 *     LINK_DOWN_HOP_COUNT - default 5.  When a multiple-device (i.e.,
 *     both HomePNA and MII PHY devices populated) PHY experiences
 *     link down for 5 consecutive calls of PhyCheck(), this routine
 *     may switch or "hop" ports based on the following logic:
 *
 *     select current, "active" port;
 *     if (++link_down_count > LINK_DOWN_HOP_COUNT) {
 *         clear link_down_count;
 *         if (PHY_HWOPTS_PORT_HOPPING) {
 *             select the other, "inactive" port;
 *         } else if (PHY_HWOPTS_MII_PORT_PRIORITY) {
 *             select the MII port;
 *         } else if (PHY_HWOPTS_HPNA_PORT_PRIORITY) {
 *             select the HomePNA port;
 *         } else {
 *             // retain the current, "active" port;
 *         }
 *     }
 *
 *     The PHY_HWOPTS_PORT_HOPPING option gives devices which
 *     cannot function when isolated a chance to achieve a media link.
 *     Some examples are the 1st generation HomePNA PHY devices plus
 *     National, Enable and Level One MII PHY devices.  The
 *     PHY_HWOPTS_HPNA_PORT_PRIORITY option is a good idea when using
 *     MII PHY devices which can function isolated, such as the TDK,
 *     QSI, GEC-Plessey, and ICS devices.  This allows the PHY
 *     Driver to emit MAC-level link test frames on the foreground
 *     "active" HomePNA PHY to determine a soft link indication while
 *     the background "inactive" MII PHY attempts to establish its
 *     hardware link indication.  The PHY_HWOPTS_MII_PORT_PRIORITY
 *     will effectively preclude HomePNA operation unless used in
 *     conjunction with PHY_HWOPTS_PORT_HOPPING as a way of
 *     establishing an initial MII port state, e.g., subsequent to
 *     DriverReset() where the initial state will be the last known
 *     link state.
 *
 *     PORTABILITY_CHECK - default #undefined.  This should only be
 *     #defined when initially porting this module to a new compiler/OS
 *     environment.  This switch enables some simple PhyInit() sizeof()
 *     tests of BYTE, WORD and DWORD types to ensure proper compiler
 *     sizing of register-based fields.  Typically, most compilers
 *     will generate "Unreachable Code" warnings which actually
 *     indicates proper sizing.  Relaxed warnings or older compilers
 *     may warrant a run-time check, for which the PhyInit() function
 *     will return a PHY_STATUS_PORTABILITY_ERROR result.
 **********************************************************************/

#define PHYOBJECT_MODULE	// this is one of the PHY object module
#define PHY_MODULE

#include "osutil.h"

#include "phyobj.h"
#include "phyregs.h"

#ifdef JEDIPHY	// Bright
#include "ramse.h"
#include "jediphy.h"
#endif


#define PHYID_KENDIN_KS8737(p)  (((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == 0x00221720L)

/* For Kendin KS8993, KS8995 and Marvell 88E6050 which do not support MDIO interface
 required for MII management channel. */
#define SUPPORT_NO_MDIO_PHY 1

#ifdef SUPPORT_NO_MDIO_PHY
#define	NO_MDIO_PHY				0xA0A05050
#define PHYID_NO_MDIO(p)		(((p)->mii_phy[ETH_MII_PHY].mii_id & ~0xFL) == NO_MDIO_PHY)
#else
#define PHYID_NO_MDIO(p)		(FALSE)
#endif

#ifndef LINK_DOWN_HOP_COUNT
#define LINK_DOWN_HOP_COUNT     5       /* 10 seconds @ 0.5 Hz */
#endif

PHY_STATUS 	 PhyReset( MIIOBJ *pMiiObj ) ;	// internal interface for jediphy

static PHY_STATUS   PhyDisable(PHY *phy, PHY_PORT port);
static PHY_STATUS   PhyEnable(PHY  *phy, PHY_PORT port);
static DWORD        PhyConnectGet(PHY *phy);
PHY_STATUS   PhyConnectSet(PHY *phy, DWORD connect);
static PHY_STATUS   PhyConfigurePort(PHY *phy, DWORD link_state);
static DWORD PhyCapabilityGet(PHY *phy);
static DWORD PhyLastLinkUp(PHY *phy) ;

#if 0
/*
 * PHY test and diagnostic support functions
 *
 * PhyLoopback() - Start/stop one of four modes of PHY loopback
 * PhyColTest()  - Start/stop collision test via register.bit 0.7
 * PhyIdGet()    - Read PHY Identifier registers 2-3
 */
static PHY_STATUS   PhyLoopback(PHY *phy, PHY_LOOPBACK mode);
static PHY_STATUS   PhyColTest(PHY *phy, BOOLEAN enable);
#endif


/* Local (static) function prototypes */
static void     ETHParsePhyHwConfig(PHY *phy);
static void     ETHSetPhyHwOptions(PHY *phy);

PHY * PhyAllocateMemory( void)
{
	PHY *pPhy ;

	if( !SUCCESS(H20_ALLOC_MEM( &pPhy, sizeof(PHY) )) )
		return NULL ;	// allocate fail
	H20_ZERO_MEM( pPhy, sizeof(PHY) ) ;
	return (void *)pPhy ;
}

void PhyFreeMemory( PHY *pPhy )
{
	H20_FREE_MEM( pPhy, sizeof(PHY) ) ;
}

static void Set_Active_Port( PHY *phy, PHY_PORT port )
{
	if( port & PHY_PORT_HPNA_DEVICES)
	{
		phy->active_port = phy->hpna_port ;
		phy->inactive_port = phy->mii_port ;
		phy->pActive_miiphy = &phy->mii_phy[JEDI_MII_PHY];
#ifdef NON_BYPASSMODE	// test
		phy->frame_ctl_length = 0 ;
#else
		phy->frame_ctl_length = 4 ;
#endif
	}
	else
	{
		phy->inactive_port = phy->hpna_port ;
		phy->active_port = phy->mii_port ;
		phy->pActive_miiphy = &phy->mii_phy[ETH_MII_PHY];
		phy->frame_ctl_length = 0 ;
	}
}

/* 
 * Local an_lookup[] table maps BMSR and ANAR hardware to internal,
 * soft PHY_LINK_* (or associated PHY_CONN_* alias) bit assignments.
 * The table is ordered in decreasing Auto-Negotiation priority per
 * IEEE 802.3u and 802.3y, and is terminated by a null (zero) record.
 * Searches may be keyed on any of the fields, however, <phy_link>
 * keys should mask out non-PHY_LINK_TECHNOLOGY bits, otherwise the
 * search may fail to achieve a match.
 *
 * This table may have to be extended to include an SNMP MIB field,
 * which contains yet another enumeration of the PHY technology types.
 */
typedef struct {
    WORD            bmsr;       /* BMSR HW capability bit */
    WORD            anar;       /* ANAR/ANLPAR HW bit */
    DWORD           phy_link;   /* internal PHY_LINK_* state */
}                AN_LOOKUP;
static AN_LOOKUP an_lookup[] = {
    {BMSR_100BASE_T2FD, ANAR_100BASE_T2FD,
     PHY_LINK_100BASE_T2FD | PHY_LINK_100 | PHY_LINK_FDX},

    {BMSR_100BASE_TXFD, ANAR_100BASE_TXFD,
     PHY_LINK_100BASE_TXFD | PHY_LINK_100 | PHY_LINK_FDX},

    {BMSR_100BASE_T2,   ANAR_100BASE_T2,
     PHY_LINK_100BASE_T2   | PHY_LINK_100},

    {BMSR_100BASE_T4,   ANAR_100BASE_T4,
     PHY_LINK_100BASE_T4   | PHY_LINK_100},

    {BMSR_100BASE_TX,   ANAR_100BASE_TX,
     PHY_LINK_100BASE_TX   | PHY_LINK_100},

    {BMSR_10BASE_TFD,   ANAR_10BASE_TFD,
     PHY_LINK_10BASE_TFD   | PHY_LINK_10  | PHY_LINK_FDX},

    {BMSR_10BASE_T,     ANAR_10BASE_T,
     PHY_LINK_10BASE_T     | PHY_LINK_10},

    /* null-terminating record */
    {0, 0, 0}
};


static PHY_STATUS ETHPhy_SetUp( PHY *phy )
{
	if(phy->hw_options & PHY_HWOPTS_NO_ETH_SEARCH)
		return PHY_STATUS_OK ;

	if (!(phy->hw_options & PHY_HWOPTS_DONT_SWRESET)) 
		if( PhyReset( &phy->mii_phy[ETH_MII_PHY]) != PHY_STATUS_OK)
			return PHY_STATUS_RESET_ERROR;

	/* Terminate upon successful reset(s) or excess restarts */
	phy->devices |= PHY_PORT_MII_DEVICE;
	phy->mii_port = PHY_PORT_MII;

	ETHParsePhyHwConfig( phy );
	ETHSetPhyHwOptions( phy );

	MII_CLR_MASK( &phy->mii_phy[ETH_MII_PHY], MII_BMCR, (BMCR_RESTART_AUTONEG |
		BMCR_POWERDOWN  |
		BMCR_LOOPBACK   |
		BMCR_COLLISION_TEST) );

	return PHY_STATUS_OK ;
}

#define MAX_PHY_NUM	2		// assume at most 1 ETH and 1 JEDI
#define SINGLE_PHY	1			// Bright, 4/27/01, only one PHY on one EMAC (MII BUS)
// currently this is ture

#if HOMEPLUGPHY

// TBD , need improvement	
#define HOMEPLUGID 0xffffffff
#endif

PHY_STATUS
MiiDetectPorts( PHY   *phy,             /* allocated PHY data structure space */
        BYTE   phy_addr ) 
{
	BYTE        last_addr;
	WORD        bmsr;
	int	num_phy = 0 ;
	int	num_of_eth_phy = 0 ;
	DWORD  hw_options = phy->hw_options ;
	MIIOBJ TmpMii ;

    /*
     * MII search, reset and optionally disable (isolate) PHY.  The
     * search logic simply reads the BMSR, for which a responding PHY
     * should return a value that is non-zero and not all one's.
     * Otherwise, assume there isn't an MII-capable PHY at <phy_addr>.
     * The ANAR probe is repeated after post-HW and/or software reset to
     * guard against the PHY losing its identity (address) upon reset.
     *
     * If the caller specified PHY_ADDR_ANY or PHY_ADDR_NEXT(phy#), 
     * we'll walk the list of valid PHY addresses until the first/next
     * responding PHY is detected.  Some PHY devices have side-effects
     * associated with PHY_ADDR_MIN (zero) -- this address is often
     * (erroneously) treated as a broadcast by some PHYs, others
     * won't display vendor-specific registers when reading from
     * address-0, etc.  As such, we'll initiate a PHY_ADDR_ANY search
     * from (PHY_ADDR_MIN + 1) on up to PHY_ADDR_MAX, concluding the
     * search using PHY_ADDR_MIN only if no other addresses are found.
     * This can be overriden by the PHY_HWOPTS_MII_SEARCH_0 <hw_options>
     * switch, which initiates a search from address 0..31, rather
     * than the default 1..31 followed by 0.
     */

#ifdef P51
	// the driver can run on ELAN or HLAN under Yukon or Athens
	if(hw_options & PHY_HWOPTS_P51_HLAN) 
		TmpMii.P51_HLAN = TRUE ;
	else
		TmpMii.P51_HLAN = FALSE ;
#endif
	TmpMii.adapter = phy->mii_phy[JEDI_MII_PHY].adapter ;
	TmpMii.preamble_suppression = FALSE ;
	phy->pActive_miiphy = &phy->mii_phy[JEDI_MII_PHY];	// set default

	if (phy_addr == PHY_ADDR_ANY) 
	{
		last_addr = PHY_ADDR_MAX;
		phy_addr  = PHY_ADDR_MIN + 1;    /* start search at address-1 */
		if (hw_options & PHY_HWOPTS_MII_SEARCH_0) 
			--phy_addr;                 /* start search at address-0 */
	} 
	else if( phy_addr <= PHY_ADDR_MAX ) 
	{
		last_addr = phy_addr;          /* single MII search, below */
	} 
	else 
	{
		phy_addr = -phy_addr;         /* partial? MII search per */
		last_addr  = PHY_ADDR_MAX;      /* phy.h PHY_ADDR_NEXT() */
	}

Search_Mii :
	while (phy_addr <= last_addr) 
	{
		TmpMii.mii_addr = phy_addr++;
		if (((bmsr = MiiRead(&TmpMii, MII_BMSR)) != 0x0000) &&
			(bmsr != 0xFFFF)) 
		{	/* Assume PHY found, so nominally perform post-SW reset */
			num_phy++ ;

			TmpMii.mii_id = (((DWORD) MiiRead(&TmpMii, MII_PHYIDR1) << 16) |
			((DWORD) MiiRead(&TmpMii, MII_PHYIDR2)));

			printk("PHY #%x = %lx\n", phy_addr,TmpMii.mii_id );

#ifdef JEDIPHY	// Bright
			if( JediPhy(TmpMii.mii_id) )
			{	// this is Jedi Phy (HPNA2)
				phy->mii_phy[JEDI_MII_PHY] = TmpMii ;
				JediPhy_SetUp( phy) ;
#ifndef SINGLE_PHY
				if(hw_options & PHY_HWOPTS_NO_ETH_SEARCH)
#endif
					return PHY_STATUS_OK ;
			}
			else
#endif
			{	// assume this is Ether Phy
				if( num_of_eth_phy == 0 )
				{
					phy->mii_phy[ETH_MII_PHY] = TmpMii ;
					phy->pActive_miiphy = &phy->mii_phy[ETH_MII_PHY];
					ETHPhy_SetUp( phy ) ;	

#ifndef SINGLE_PHY
					if(hw_options & PHY_HWOPTS_NO_HPNA_SEARCH)
#endif
						return PHY_STATUS_OK ;
				}
				num_of_eth_phy++ ;
			}
		}
		if( num_phy == MAX_PHY_NUM )
			return PHY_STATUS_OK ;
	}

	/* Go back to addr-0 once full MII search 1..31 fails */
	if ((phy_addr == (PHY_ADDR_MAX+1)) &&
		!(hw_options & PHY_HWOPTS_MII_SEARCH_0)) 
	{
		phy_addr = last_addr = PHY_ADDR_MIN ;
		goto Search_Mii ;
	}
	
	if( num_phy != 0 )
		return PHY_STATUS_OK ;
	else
		return PHY_STATUS_NOT_FOUND ;
}
/***********************************************************************
 * PhyInit() - Initialize PHY and attach to media (w/optional override)
		PhyInit do not	perform hardware reset of the PHYs. 
		Hardware reset is MAC operation, should be called before PhyInit(if required).
 **********************************************************************/
PHY_STATUS
PhyInit(DWORD  adapter,         /* MAC device I/O base address */
        PHY   *phy,             /* allocated PHY data structure space */
        DWORD  connect,         /* PhyConnectSet() directive */
        DWORD  hw_options,      /* zero or more PHY_HWOPTS_* values */
        void  *LinkLayerAdapter )	// for HPNA 2.0 LL compliance
{
    PHY_STATUS  status;

#ifdef PORTABILITY_CHECK        /* ################################## */
    /* Not a bad idea during initial PHY driver porting effort */
    if ((sizeof(BYTE) != 1) ||
            (sizeof(WORD) != 2) || (sizeof(DWORD) != 4)) {
        return (PHY_STATUS_PORTABILITY_ERROR);
    }
#endif  /* PORTABILITY_CHECK */ /* ################################## */


	if( IS_PHY_INITIALIZED(phy) )
	{	// not first time PHY Init
		if( !(PhyCapabilityGet(phy) & PHY_LINK_MII_TECHNOLOGY ) )
		{
			/* No MII PHY, so don't waste time searching henceforth */
			hw_options |= PHY_HWOPTS_NO_ETH_SEARCH;
		}
		else
		{
			if( phy->mii_phy[ETH_MII_PHY].mii_addr == PHY_ADDR_0) 
			{
					/*
					 * Reduce subsequent PhyInit() MII search times by
					 * starting at MII address zero.  The default sequence
					 * is 1..31 then zero due to some funky side-effects
					 * on some MII PHY devices.  This option re-orders the
					 * search sequence to 0..31, thereby reducing
					 * subsequent PhyInit() times by a few hundred milliseconds!
					 */
				hw_options |= PHY_HWOPTS_MII_SEARCH_0;
			}
		}

		if (!(PhyCapabilityGet(phy) & PHY_LINK_HPNA_TECHNOLOGY)) 
			/* No HomePNA PHY, so don't waste time searching henceforth */
			hw_options |= PHY_HWOPTS_NO_HPNA_SEARCH;
		else
		{
			if( phy->mii_phy[JEDI_MII_PHY].mii_addr == PHY_ADDR_0) 
			{
					/*
					 * Reduce subsequent PhyInit() MII search times by
					 * starting at MII address zero.  The default sequence
					 * is 1..31 then zero due to some funky side-effects
					 * on some MII PHY devices.  This option re-orders the
					 * search sequence to 0..31, thereby reducing
					 * subsequent PhyInit() times by a few hundred milliseconds!
					 */
				hw_options |= PHY_HWOPTS_MII_SEARCH_0;
			}
		}

		if (PhyLastLinkUp(phy) & PHY_LINK_MII_TECHNOLOGY)
		{
			/* Last known link was MII, so pre-hop default to MII port */
			hw_options |= (PHY_HWOPTS_MII_PORT_PRIORITY |
						   PHY_HWOPTS_PORT_HOPPING);
		} 
		else if (PhyLastLinkUp(phy) & PHY_LINK_HPNA_TECHNOLOGY) 
		{
			/* Last known link was HLAN, so don't pre-hop default to MII port */
			hw_options &= ~PHY_HWOPTS_MII_PORT_PRIORITY;
		}
	} 


    /*
     * Set-up <phy> data and perform optional pre-MII search HW reset,
     * which may be required only in the case of unusual adapter design.
     * This case has been included to satisfy the RSS11606 test adapter
     * board, wherein a MAC general purpose I/O (GPIO) pin holds the PHY
     * in hardware reset.  Unless programmed to release the PHY prior to
     * searching the MII bus, the subsequent MII search will fail, as
     * the PHY obviously cannot respond when held in hardware reset.
     *
     * Bug warning:  unless there's a crafty HW design, this could
     * cause unexpected failures in a multiple-PHY environment if
     * PHY_ADDR_ANY or PHY_ADDR_NEXT() is specified to search for a
     * responding PHY.  For now, we'll simply do as we're told.  This
     * should probably be moved into the search loop below, and rely
     * upon a PHY_HWRESET() routine that parses the phy_addr argument...
     */
#ifdef JEDIPHY	// Bright
	H20_ZERO_MEM( phy, sizeof(PHY)-sizeof(OSFILEOBJ) ) ;
#else
	H20_ZERO_MEM( phy, sizeof(PHY) ) ;
#endif

//    if (hw_options & PHY_HWOPTS_READ_ONLY) {
//        phy->read_only = TRUE;
//    }
    phy->mii_phy[JEDI_MII_PHY].adapter = adapter ;
    phy->mii_phy[ETH_MII_PHY].adapter = adapter ;

    phy->LinkLayerAdapter = LinkLayerAdapter ;


//    if ( (hw_options & PHY_HWOPTS_HWRESET_MASK) &&
//		 (hw_options & PHY_HWOPTS_GP0_SPI_DOUT) ) 
//        PHY_HWRESET( adapter );

	phy->hw_options = hw_options;

	status = MiiDetectPorts( phy, PHY_ADDR_ANY) ;

	if( status != PHY_STATUS_OK )
	{
#ifdef SUPPORT_NO_MDIO_PHY
		// TBD : not tested yet
		phy->mii_phy[ETH_MII_PHY].mii_id = NO_MDIO_PHY;
        phy->initialized = PHY_INIT_CODE;
		phy->media.capability = PHY_LINK_100BASE_TXFD |
								PHY_LINK_100BASE_T2FD | 
								PHY_LINK_100 | 
								PHY_LINK_FDX;
		phy->media.connect = PHY_CONN_MII_TECHNOLOGY;
		phy->devices |= PHY_PORT_MII_DEVICE;
		phy->mii_port = PHY_PORT_MII;

		Set_Active_Port( phy, phy->mii_port ) ;		
	    status = PHY_STATUS_OK;
#endif
		return status ;
	}

#if HOMEPLUGPHY
	if( phy->pActive_miiphy->mii_id == HOMEPLUGID )
	{
		connect = PHY_CONN_100BASE_TX ;		// HomePlug as 100 half
	}	
#endif

	if( hw_options & PHY_HWOPTS_ISOLATE )
		PhyDisable(phy, phy->devices);

	if (phy->mii_port == PHY_PORT_NONE) 
	{
		/* Coerce soft non-MII Auto-Negotiation capability */
		phy->media.capability |= PHY_LINK_AUTONEG;
		phy->media.connect    |= PHY_CONN_AUTONEG;
	}

	if ((status = PhyConnectSet(phy, connect)) == PHY_STATUS_OK) 
	{
		phy->initialized = PHY_INIT_CODE;
		phy->hw_options &= ~PHY_HWOPTS_ISOLATE;
	}
	return (status);
}

/***********************************************************************
 * ParsePhyHwConfig() - Process PHY HW configuration options
 **********************************************************************/
static void
ETHParsePhyHwConfig(PHY *phy)
{
    WORD        bmcr;
    WORD        bmsr;
    WORD        anar;
    DWORD       mode;
    AN_LOOKUP  *lookup;

// no need
//    phy->media.capability = 0;
//    phy->media.connect = 0;

    /* Determine media capabilities based on BMSR */
    bmsr = MiiRead( &phy->mii_phy[ETH_MII_PHY], MII_BMSR);
    if (bmsr & BMSR_AUTONEG_ABILITY) {
        phy->media.capability |= PHY_LINK_AUTONEG;
    }
    for (lookup = &an_lookup[0]; lookup->bmsr != 0; lookup++) {
        if (bmsr & lookup->bmsr)
        {
            phy->media.capability |= (lookup->phy_link & PHY_LINK_TECHNOLOGY);
        }
    }

    /*
     * Determine adapter (NIC) unique HW configuration strap
     * settings based on BMCR (and ANAR if Auto-Negotiation is enabled).
     */
    bmcr = MiiRead( &phy->mii_phy[ETH_MII_PHY], MII_BMCR);
    if (bmcr & BMCR_AUTONEG_ENABLE) {
        /* Read default Auto-Negotiation configuration per ANAR */
        phy->media.connect |= PHY_CONN_AUTONEG;
        anar = MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_ANAR);
        for (lookup = &an_lookup[0]; lookup->anar != 0; lookup++) {
            if (anar & lookup->anar) {
                phy->media.connect |=
                    (lookup->phy_link & PHY_LINK_TECHNOLOGY);
            }
        }
    } else {
        /* Infer default forced mode configuration per BMCR */
        mode = 0;
        if (bmcr & BMCR_SPEED_100) {
            mode |= PHY_LINK_100;
        } else {
            mode |= PHY_LINK_10;
        }
        if (bmcr & BMCR_DUPLEX_MODE) {
            mode |= PHY_LINK_FDX;
        }
        for (lookup = &an_lookup[0]; lookup->phy_link != 0; lookup++) {
            if ((phy->media.capability & lookup->phy_link) &&
                    (mode == (lookup->phy_link &
                              (PHY_LINK_SPEED | PHY_LINK_FDX)))) {
                phy->media.connect |= lookup->phy_link;
                break;
            }
        }
    }

    /* Check for isolated (tri-stated) MII PHY */
    if (bmcr & BMCR_ISOLATE) {
        phy->mii_isolated = TRUE;
    }

    /* 
     * Finally, set <preamble_suppression> if suported by the device.
     * This can subsequently be overriden by SetPhyHwOptions()
     * via the PHY_HWOPTS_MII_PREAMBLE directive, which may be used
     * in a multi-PHY environment or in case a device's BMSR register
     * falsely indicates preamble suppression capability.
     */
    if (bmsr & BMSR_PREAMBLE_SUPPRESS) {
        phy->mii_phy[ETH_MII_PHY].preamble_suppression = TRUE;
    }
}

/***********************************************************************
 * SetPhyHwOptions() - Configure non-default MII PHY hardware features
 **********************************************************************/
// ETH only
static void
ETHSetPhyHwOptions(PHY *phy)
{
    DWORD hw_opts = phy->hw_options ;

    /* Force MDIO preamble, i.e., explicitly disable suppression */
    if (hw_opts & PHY_HWOPTS_MII_PREAMBLE) {
        phy->mii_phy[ETH_MII_PHY].preamble_suppression = FALSE;
    }

    if (PHYID_NSC_DP83840(phy)) {
        /* Force node or repeater mode, which affects CRS signal */
        if (hw_opts & PHY_HWOPTS_REPEATER) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_PCR, PCR_REPEATER_MODE);
        } else {
            MII_CLR_BIT(&phy->mii_phy[ETH_MII_PHY], MII_PCR, PCR_REPEATER_MODE);
        }

        /* Disable CLK25 output line, reducing noise when unused */
        if (hw_opts & PHY_HWOPTS_CK25DIS) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_PCR, PCR_CLK25DIS);
        }

        /* Bypass portions of the transmitter and receiver coding */
        if (hw_opts & PHY_HWOPTS_TX_BPALIGN) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_LBREMR, LBREMR_BP_ALIGN);
        }
        if (hw_opts & PHY_HWOPTS_TX_BPSCR) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_LBREMR, LBREMR_BP_SCR);
        }
        if (hw_opts & PHY_HWOPTS_TX_BP4B5B) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_LBREMR, LBREMR_BP_4B5B);
        }

        /* Assert FDX CRS during transmit, not receive (default) */
        if (hw_opts & PHY_HWOPTS_FDX_CRS) {
            if (PHYID_REV(phy) >= 1) {
                /* New NSC DP83840 Rev A (revision code 1) feature */
                MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_LBREMR, LBREMR_ALT_CRS);
            }
        }

    } else if (PHYID_QSI_6612(phy)) {
        /* Bypass portions of the transmitter and receiver coding */
        if (hw_opts & PHY_HWOPTS_TX_BPALIGN) {
            /* No QSI support for bypassing the alignment function */
        }
        if (hw_opts & PHY_HWOPTS_TX_BPSCR) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R31, QSI_R31_SCR_DISABLE);
        }
        if (hw_opts & PHY_HWOPTS_TX_BP4B5B) {
            MII_CLR_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R31, QSI_R31_4B5BEN);
        }

    } else if (PHYID_LXT_970(phy)) {
        /* Bypass portions of the transmitter and receiver coding */
        if (hw_opts & PHY_HWOPTS_TX_BPALIGN) {
            /* No Level One support for bypassing alignment function */
        }
        if (hw_opts & PHY_HWOPTS_TX_BPSCR) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R19, LXT_R19_SCR_BYPASS);
        }
        if (hw_opts & PHY_HWOPTS_TX_BP4B5B) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R19, LXT_R19_4B5B_BYPASS);
        }

    } else if (PHYID_GEC_NWK936(phy)) {
        /* Bypass portions of the transmitter and receiver coding */
        if (hw_opts & PHY_HWOPTS_TX_BPALIGN) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R24, GEC_R24_BPALIGN);
        }
        if (hw_opts & PHY_HWOPTS_TX_BPSCR) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R24, GEC_R24_BPSCR);
        }
        if (hw_opts & PHY_HWOPTS_TX_BP4B5B) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R24, GEC_R24_BPENC);
        }

        /* Assert FDX CRS during transmit, not receive (default) */
        if (!(hw_opts & PHY_HWOPTS_FDX_CRS)) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], 24, 0x0800);
        }

    } else if (PHYID_TDK_2120(phy)) {
        /* Force node or repeater mode, which affects CRS signal */
        if (hw_opts & PHY_HWOPTS_REPEATER) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R16, TDK_R16_RPTR);
        } else {
            MII_CLR_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R16, TDK_R16_RPTR);
        }

        /*
         * Disable CLK25 output line, reducing noise when unused.  Due
         * to Auto-Negotiation side-effects, the TDK RXCC option is
         * now performed by PhyCheck() on link up and down transitions.
         *
         * if (hw_opts & PHY_HWOPTS_CK25DIS) {
         *     MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R16, TDK_R16_RXCC);
         * }
         */

        /* Bypass portions of the transmitter and receiver coding */
        if (hw_opts & (PHY_HWOPTS_TX_BPSCR | PHY_HWOPTS_TX_BP4B5B)) {
            /* TDK PHY doesn't have separate PCS, scrambler bypass */
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R16, TDK_R16_PCSBP);
        }
    }
}

/***********************************************************************
 * PhyReset() - Issue PHY software reset, waiting 500 mSec to complete
 **********************************************************************/
PHY_STATUS 	 PhyReset( MIIOBJ *pMiiObj ) 
{
    PHY_STATUS  phy_status = PHY_STATUS_RESET_ERROR;
    int         n;

    /*
     * Set BMCR Reset bit, expected to auto-clear in 500 milliseconds
     * Some devices may require a 500-microsecond guard against serial
     * MII access, so we'll delay after setting the reset bit and after
     * detecting auto-clear, just to be safe.
     */
    MII_SET_BIT(pMiiObj, MII_BMCR, BMCR_RESET);
    MICROSECOND_WAIT(500);
    for (n = 0; n < 50; n++) {
        if (!(MiiRead( pMiiObj, MII_BMCR) & BMCR_RESET)) {
            phy_status = PHY_STATUS_OK;
            break;
        }
        MILLISECOND_WAIT(10);
    }
    MICROSECOND_WAIT(500);
    return (phy_status);
}


/***********************************************************************
 * PhyDisable() - Disable PHY by asserting BMCR isolate command
 **********************************************************************/
PHY_STATUS
PhyDisable(PHY *phy, PHY_PORT port)
{

    /* (1) Validate the selected port(s) */
    port &= phy->devices;

    /* (2) MII PMD isolation (TP tri-state) */
    if (port & PHY_PORT_MII_PMD) {
        if (PHYID_QSI_6612(phy)) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R31, QSI_R31_TX_ISOLATE);
        } else if (PHYID_LXT_970(phy)) {
            MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R19, LXT_R19_TX_DISCONNECT);
        }
    }

    /* (3) MII data isolation (tri-state) */
    if ((port & PHY_PORT_MII) && !phy->mii_isolated) {
        phy->mii_isolated = TRUE;
        MII_SET_BIT( &phy->mii_phy[ETH_MII_PHY], MII_BMCR, BMCR_ISOLATE);
    }

#ifdef JEDIPHY	// Bright
	 JediPhyDisable( phy, port) ;
#endif

    return (PHY_STATUS_OK);
}

/***********************************************************************
 * PhyEnable() - (Re)Enable PHY by deasserting BMCR isolate command
 **********************************************************************/
PHY_STATUS
PhyEnable(PHY *phy, PHY_PORT port)
{
    WORD        bmcr;

    /* (1) Validate the selected port(s) */
    port &= phy->devices;

    /* (2) MII PMD de-isolation (TP non tri-state) */
    if (port & PHY_PORT_MII_PMD) {
        if (PHYID_QSI_6612(phy)) {
            MII_CLR_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R31, QSI_R31_TX_ISOLATE);
        } else if (PHYID_LXT_970(phy)) {
            MII_CLR_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R19, LXT_R19_TX_DISCONNECT);
        }
    }

    /* (3) MII data de-isolation (no longer tri-stated) */
    if ((port & PHY_PORT_MII) && phy->mii_isolated) {
        phy->mii_isolated = FALSE;
			// Bright fixed 7/18/02
        bmcr = MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_BMCR) & 
			~(BMCR_ISOLATE |BMCR_POWERDOWN) ;
        if (phy->media.connect & PHY_CONN_AUTONEG) {
            bmcr |= (BMCR_AUTONEG_ENABLE | BMCR_RESTART_AUTONEG);
            /*
             * Side-effect:  some PHYs (well, at least one vendor's)
             * require the BMCR_AUTONEG_ENABLE bit (reg 0.12) be cleared
             * then set in the case where the device is coming up out
             * of reset or isolated state, otherwise, Auto-Negotiation
             * won't be initiated.  We'll temporarily clear A-N bits,
             * which will then be immediately be set, below.
             */
            if (PHYID_NSC_DP83840(phy) ||
                    (phy->hw_options & PHY_HWOPTS_ACTIVE_AN_RESTART)) {
                MiiWrite(&phy->mii_phy[ETH_MII_PHY], MII_BMCR,
                         (WORD) (bmcr & ~(BMCR_AUTONEG_ENABLE |
                                          BMCR_RESTART_AUTONEG)));
            }
        }
        MiiWrite(&phy->mii_phy[ETH_MII_PHY], MII_BMCR, bmcr);
    }

#ifdef JEDIPHY	// Bright
	JediPhyEnable(phy, port) ;
#endif
    return (PHY_STATUS_OK);
}

/***********************************************************************
 * PhyConnectGet() - Get PHY connect setting(s)
 **********************************************************************/
DWORD
PhyConnectGet(PHY *phy)
{
    return (phy->media.connect);
}

/***********************************************************************
 * PhyConnectSet() - Execute PHY connect settings
 **********************************************************************/
PHY_STATUS
PhyConnectSet(PHY *phy, DWORD connect)
{
    WORD        bmcr;
    WORD        anar;
    AN_LOOKUP  *lookup;

    /* (1) Establish/adjust the <connect> directive */
    switch (connect & (PHY_CONN_AUTONEG | PHY_CONN_TECHNOLOGY)) {
        default:
            /* Use non-zero <connect> directive as-is */
            break;

        case PHY_CONN_CURRENT:
            /* Zero: use HW default or previous setting */
            connect = phy->media.connect; break;

        case PHY_CONN_AUTONEG:
            /* A-N bit only: Auto-Negotiate HW default or previous */
            connect = phy->media.connect | PHY_CONN_AUTONEG; break;
    }

    if (phy->hw_options & PHY_HWOPTS_STRICT_PARM_CHECKING) {
        /* Strict checking:  A-N and full media agreement required */
        if ((connect & (PHY_CONN_AUTONEG | PHY_CONN_TECHNOLOGY)) !=
                (connect & phy->media.capability)) {
            return (PHY_STATUS_UNSUPPORTED_MEDIA);
        }

    } else {
        /* Relaxed checking:  A-N and partial media agreement needed */
        if (((connect & PHY_CONN_AUTONEG)
             && !(phy->media.capability & PHY_CONN_AUTONEG)) ||
            ((connect & PHY_CONN_TECHNOLOGY) &&
             !(connect & phy->media.capability &
               PHY_CONN_TECHNOLOGY))) {
            return (PHY_STATUS_UNSUPPORTED_MEDIA);
        }
        /* Truth in advertising:  shave unsupported media settings */
        connect &= (phy->media.capability | ~PHY_CONN_TECHNOLOGY);
    }

#ifdef JEDIPHY	// Bright
    /* (2) HomePNA device: force highest-priority setting */
	connect = JediPhyConnectSet(phy, connect) ;
#endif

    /* (3) MII device: Auto-Negotiate or force highest capability */
    if (connect & PHY_CONN_MII_TECHNOLOGY) {

        /* Read BMCR, momentarily clearing speed and duplex bits */
        bmcr = MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_BMCR) &
            ~(BMCR_SPEED_100 | BMCR_DUPLEX_MODE);

        if (connect & PHY_CONN_AUTONEG) {
            /*
             * Auto-Negotiation connection mode.  Walk the entire
             * an_lookup[] table, latching all matching <phy_link> types
             * into the PHY's Auto-Negotiation Advertisement Register
             * (ANAR).  Set ANAR and BMCR when the table is exhausted.
             */
            bmcr |= (BMCR_AUTONEG_ENABLE | BMCR_RESTART_AUTONEG);
            anar = MiiRead( &phy->mii_phy[ETH_MII_PHY], MII_ANAR) &
                (ANAR_NEXT_PAGE | /* ANAR_ACKNOWLEDGE | */
                 ANAR_PROTOCOL_SELECTOR);
            if (connect & PHY_CONN_AUTONEG_REMOTE_FAULT)
                anar |= ANAR_REMOTE_FAULT;
            if (connect & PHY_CONN_AUTONEG_FDX_PAUSE)
                anar |= ANAR_FDX_PAUSE;
            for (lookup = &an_lookup[0]; lookup->phy_link != 0; lookup++) {
                if (connect & lookup->phy_link & PHY_CONN_TECHNOLOGY)
                    anar |= lookup->anar;
            }
            MiiWrite( &phy->mii_phy[ETH_MII_PHY], MII_ANAR, anar);
            phy->anar = anar;       /* cached for PhyCheck() */
            /*
             * Side-effect:  some PHYs (well, at least one vendor's)
             * require the BMCR_AUTONEG_ENABLE bit (reg 0.12) be cleared
             * and then set in the case where the device is coming up out
             * of reset or isolated state, otherwise, Auto-Negotiation
             * won't be initiated.  We'll temporarily clear the A-N bits,
             * which will then be immediately be set, below.
             */
            MiiWrite( &phy->mii_phy[ETH_MII_PHY], MII_BMCR,
                     (WORD) (bmcr & ~(BMCR_AUTONEG_ENABLE |
                                      BMCR_RESTART_AUTONEG)));

        } else {
            /*
             * Forced connect mode.  Walk the an_lookup[] table until a
             * matching <phy_link> type is found.  Set BMCR accordingly
             * and terminate the loop.  Doesn't yet support a dual -T2/-TX
             * device (R7145 is TBD), which likely requires an additional
             * control to select the desired 100Base-T PMA technology.
             */
            bmcr &= ~(BMCR_AUTONEG_ENABLE | BMCR_RESTART_AUTONEG);
            for (lookup = &an_lookup[0]; lookup->phy_link != 0; lookup++) {
                if (connect & lookup->phy_link & PHY_CONN_TECHNOLOGY) {
                    connect = lookup->phy_link & PHY_CONN_TECHNOLOGY;
                    if (lookup->phy_link & PHY_LINK_FDX)
                        bmcr |= BMCR_DUPLEX_MODE;
                    if (lookup->phy_link & PHY_LINK_100)
                        bmcr |= BMCR_SPEED_100;
                    break;
                }
            }
        }

        /* Issue the MII BMCR hardware command */
        MiiWrite(&phy->mii_phy[ETH_MII_PHY], MII_BMCR, bmcr);

        /* National PHY bug:  kick (read) PHY to restart A-N (?) */
        if ((connect & PHY_CONN_AUTONEG) && PHYID_NSC_DP83840(phy)) {
            (void) MiiRead( &phy->mii_phy[ETH_MII_PHY], MII_BMCR);
        }
    }

    /*
     * (4) Select default MII/HPNA ports to activate/enable and
     *     deactivate/disable.  If Auto-Negotiating both HomePNA
     *     and MII ports, activate the HomePNA port unless MII
     *     priority is specified.  If forcing or Auto-Negotiating
     *     a single port, then activate that port.
     */
	 Set_Active_Port( phy, phy->hpna_port ) ;

    if ((connect & PHY_CONN_MII_TECHNOLOGY) &&
            (connect & PHY_CONN_HPNA_TECHNOLOGY)) {
        /* Both ports active */
        if (connect & PHY_CONN_AUTONEG) {
            if (phy->hw_options & PHY_HWOPTS_MII_PORT_PRIORITY) {
                /* Auto-Negotiating and MII priority option */
                Set_Active_Port( phy, phy->mii_port ) ;
            }

        } else {
            /* Forced connections: disable lower priority port */
            if (phy->hw_options & PHY_HWOPTS_HPNA_PORT_PRIORITY) {
                connect &= ~PHY_CONN_MII_TECHNOLOGY;
            } else {
                connect &= ~PHY_CONN_HPNA_TECHNOLOGY;
                Set_Active_Port( phy, phy->mii_port ) ;
            }
        }

    } else if (connect & PHY_CONN_MII_TECHNOLOGY) {
        Set_Active_Port( phy, phy->mii_port ) ;
    }

    /* (5) Shave unused, residual <connect> settings and save */
    connect &= (PHY_CONN_AUTONEG              |
                PHY_CONN_AUTONEG_REMOTE_FAULT |
                PHY_CONN_AUTONEG_FDX_PAUSE    |
                PHY_CONN_TECHNOLOGY);
    if (!(connect & PHY_CONN_AUTONEG) ||
            !(connect & PHY_CONN_MII_TECHNOLOGY)) {
        connect &= ~(PHY_CONN_AUTONEG_REMOTE_FAULT |
                     PHY_CONN_AUTONEG_FDX_PAUSE);
    }
    phy->media.connect = connect;
    phy->media.cum_link_state      =
        phy->media.hpna_link_state =
        phy->media.mii_link_state  = 0;

    /*
     * (6) Activate/deactivate selected ports whenever called from
     *     PhyInit().  
     */
    if(!(phy->hw_options & PHY_HWOPTS_ISOLATE)) {
        PhyConfigurePort(phy, phy->media.cum_link_state);
    }
    
    return (PHY_STATUS_OK);
}

/***********************************************************************
 * PhyConfigurePort() - Configure PHY (and optional MAC) port settings
 **********************************************************************/
#if 0
	#include "macphy.h"
#endif

PHY_STATUS
PhyConfigurePort(PHY *phy, DWORD link_state)
{
	BOOLEAN 			Port_Mii, Speed100m, Full_Duplex ;

	if ((link_state & PHY_LINK_NEW_MEDIA) ||
		(phy->active_port != phy->enabled_port)) 
	{
		/*
		 * Upon new media and/or port change ...
		 *
		 * (1) Optionally stop MAC Tx/Rx, delay up to 5 maximum packet
		 *     times (5 x 1518 bytes = 60,720 bits, yielding respective
		 *     100/10/1 Mbit/s delays of 607.2uS/6.072mS/60.72mS)
		 *     for the current transmit and/or receive processing to
		 *     complete, and then disable the soon-to-be-active PHY if
		 *     we're changing (MAC and PHY) ports.
		 */
#ifndef SINGLE_PHY
		MacPhy_Stop_And_Wait_TXRX_Complete( phy->mii_phy[JEDI_MII_PHY].adapter ) ;
#endif
		/*
		* Temporarily tri-state the active, soon-to-be-enabled port
		* only upon initial or subsequent port changes.
		*/
		if (phy->active_port != phy->enabled_port )
			PhyDisable(phy, phy->active_port);

		/* (2) Disable the inactive PHY */
		PhyDisable(phy, phy->inactive_port);

		/* (3) reconfigure the MAC's */
		Full_Duplex = FALSE ;
		Port_Mii = TRUE ;
		if (phy->active_port & PHY_PORT_HPNA_MII ) 
		{
			Speed100m = TRUE ;
		}
		else if (phy->active_port & PHY_PORT_HPNA_7WS)
		{
			Port_Mii = FALSE ;
			Speed100m = FALSE ;
		}
		else 
		{	// 10/100
			Speed100m = TRUE ;

			if ((link_state & PHY_LINK_UP) ||
               ((link_state = phy->media.mii_link_state) != 0)) 
			{
				/* Go with current or previous MII link settings */
				if (link_state & PHY_LINK_10) 
					Speed100m = FALSE ;	// 10M

				if (link_state & PHY_LINK_FDX) 
					Full_Duplex = TRUE ;
			} 
			else 
			{
				/* MII link never established: go with connect */
				link_state = phy->media.connect;
				if (link_state & (PHY_LINK_100BASE_T2FD |
					PHY_LINK_100BASE_TXFD)) 
				{
					if (!(link_state & PHY_LINK_AUTONEG))
						/* A-N @ half-duplex, forced @ full-duplex */
						Full_Duplex = TRUE ;

				} else if (link_state & (PHY_LINK_100BASE_T2 |
                                     PHY_LINK_100BASE_TX |
                                     PHY_LINK_100BASE_T4)) 
				{
                ;       /* nada */

				} 
				else if (link_state & PHY_LINK_10BASE_TFD) 
				{
					Speed100m = FALSE ;
					if (!(link_state & PHY_LINK_AUTONEG))
					/* A-N @ half-duplex, forced @ full-duplex */
						Full_Duplex = TRUE ;
				} else {
					Speed100m = FALSE ;
				}
			}
		}
#if !defined(DEVICE_YELLOWSTONE)
	#if 0
		MacPhy_Configure_Mac( phy->mii_phy[JEDI_MII_PHY].adapter, Port_Mii, Speed100m, Full_Duplex ) ;
	#endif
#endif

		/* (4) Enable the active PHY */
		PhyEnable(phy, phy->active_port) ;
		phy->enabled_port = phy->active_port;
	}
	return (PHY_STATUS_OK);
}

/***********************************************************************
 * PhyCapabilityGet() - Get PHY technology capabilities
 **********************************************************************/
DWORD
PhyCapabilityGet(PHY *phy)
{
    return (phy->media.capability);
}

/***********************************************************************
 * PhyCheck() - Assess link status, speed and duplex mode
 **********************************************************************/
static DWORD ETHPhyCheck(PHY *phy)
{ 
	DWORD mii_link_state  = phy->media.mii_link_state;
	DWORD connect = phy->media.connect;
	WORD            bmsr;
	WORD            aner;
	WORD            negotiation;
	AN_LOOKUP      *table;
	DWORD mii_link_error = 0 ;	// no checking now

   if( (connect & PHY_CONN_MII_TECHNOLOGY) && (phy->mii_isolated == FALSE)){

	/* For no-MDIO PHYs assume link is up, 100 Mbps, full duplex. What else?
	 * We have no way to verify that anyway */
        if( PHYID_NO_MDIO(phy) )
		{
			mii_link_state = PHY_LINK_UP | PHY_LINK_AUTONEG | PHY_LINK_100 | PHY_LINK_100BASE_TX | PHY_LINK_FDX;
	        phy->media.mii_link_state = mii_link_state;
	        phy->media.connect = PHY_CONN_AUTONEG | PHY_LINK_UP;
			return mii_link_state ;
		}	

        /*
         * Important note:  must double-clutch BMSR when link status
         * is down, since the BMSR_LINK_STATUS bit is latched LOW until
         * read.  A second BMSR read ensures current link state.
         */
        bmsr = MiiRead( &phy->mii_phy[ETH_MII_PHY], MII_BMSR);
        if (!(bmsr & BMSR_LINK_STATUS)) {
            bmsr |= MiiRead( &phy->mii_phy[ETH_MII_PHY], MII_BMSR); /* |= retains error latches */

            /*
             * Upon "long-term" link down transition, disable power-saving
             * TDK RXCC option (RX_CLK is held low when no data is
             * received), as it adversely impacts Auto-Negotiation.  We'll
             * restart Auto-Negotiation as a precautionary measure.
             */
            if ((phy->hw_options & PHY_HWOPTS_CK25DIS) &&
                    PHYID_TDK_2120(phy) &&
                    (mii_link_state & PHY_LINK_UP) &&
                    !(bmsr & BMSR_LINK_STATUS)) {
                MII_CLR_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R16, TDK_R16_RXCC);
                if (connect & PHY_CONN_AUTONEG) {
                    MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_BMCR, BMCR_RESTART_AUTONEG);
                }
            }

            /* Link down: report current connect state */
            phy->media.mii_link_state &= ~PHY_LINK_UP;
            mii_link_state = connect & (PHY_CONN_MII_TECHNOLOGY       |
                                        PHY_CONN_AUTONEG_REMOTE_FAULT |
                                        PHY_CONN_AUTONEG_FDX_PAUSE);

        }

        /*
         * Re-evaluate link configuration for down-to-up transitions,
         * otherwise report present (connecting or operational) link state.
         */
        if ((bmsr & BMSR_LINK_STATUS) && !(mii_link_state & PHY_LINK_UP)) {
            mii_link_state = 0;
            if (!(connect & PHY_CONN_AUTONEG)) {
                /*
                 * Forced connection mode:  assume the forced state,
                 * walking the an_lookup[] table to pick up associated
                 * PHY_LINK_FDX and PHY_LINK_100/10 status bits.
                 */
                for (table = &an_lookup[0]; table->phy_link != 0; table++) {
                    if (connect & table->phy_link & PHY_LINK_TECHNOLOGY) {
                        mii_link_state = table->phy_link | PHY_LINK_UP;
                        break;
                    }
                }

            } else {
                /*
                 * Auto-Negotiation mode:  infer link speed/mode based
                 * on Link Control Words (LCWs) advertised and received
                 * from our link partner.  Simply walk the negotiated
                 * settings against the an_lookup[] table, ordered from
                 * highest to lowest Auto-Negotiation priority, reporting
                 * first (highest) matching capability.  Special case:
                 * the Davicom PHY doesn't cleanse (zero) the ANLPAR link
                 * partner register when transitioning from an A-N partner
                 * to a non A-N partner, so check its ANER expansion
                 * register for a new link page notification.  Not all PHYs
                 * indicate new page in all modes, e.g., the QSI 970 and
                 * Level One (LXT 901) PHYs' "ANLPAR spoofing" won't
                 * indicate ANER new page when linked to a non A-N partner,
                 * and the pre-RevA NSC 83840 apparently doesn't indicate
                 * next page even when linked with an A-N partner.
                 */
                if ((bmsr & BMSR_AUTONEG_COMPLETE) &&
                        (((aner = MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_ANER)) &
                           ANER_PAGE_RECEIVED) ||
                         !PHYID_DAVICOM_9101(phy))) {
                    negotiation = (phy->anar & MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_ANLPAR));
                    for (table = &an_lookup[0]; table->anar != 0; table++) {
                        if (negotiation & table->anar) {
                            mii_link_state = (PHY_LINK_UP      |
                                              PHY_LINK_AUTONEG |
                                              table->phy_link);
                            break;
                        }
                    }
                    if (aner & ANER_PARALLEL_DETECTION_FAULT) {
                        mii_link_error |= PHY_LINK_PARALLEL_ERROR;
                    }
                }

				// Bright new 9/26/01
                if (PHYID_KENDIN_KS8737(phy)) {
                /*
                 * Though the Kendin PHY supports "ANLPAR spoofing,"
                 * subsequent to link with Auto-Negotiating peer,
                 * it fails to update its ANLPAR register, resulting
                 * in false mode selection.  We'll correct via
                 * vendor-specific register 31.
                 */
                   WORD    r31 = MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_R31);
                   if (!(r31 & MII_BIT4)) 
                   {
                    /*
                     * Not full-duplex, which can only result from
                     * a true Auto-Negotiation handshake.
                     */
                    mii_link_state = (PHY_LINK_UP | PHY_LINK_AUTONEG);
                    if (r31 & MII_BIT2) {
                        mii_link_state |= PHY_LINK_10BASE_T;
                    } else {
                        mii_link_state |=
                            (PHY_LINK_100 | PHY_LINK_100BASE_TX);
                    }
                   }
                } else if (mii_link_state == 0) {
                    /*
                     * Parallel Detection: link but no Auto-Negotiation
                     * match, which implies our link partner is not A-N
                     * capable.  Determine link technology via PHY
                     * vendor-specific status registers.
                     */
                    if (PHYID_NSC_DP83840(phy)) {
                        mii_link_state = PHY_LINK_UP | PHY_LINK_AUTONEG;
                        if (MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_PAR) & PAR_SPEED_10) {
                            mii_link_state |=
                                (PHY_LINK_10  | PHY_LINK_10BASE_T);
                        } else {
                            mii_link_state |=
                                (PHY_LINK_100 | PHY_LINK_100BASE_TX);
                        }

                    } else if (PHYID_GEC_NWK936(phy)) {
                        /*
                         * The GEC PHY apparently sets BMCR speed/duplex
                         * bits based on the A-N outcome.  Though their spec
                         * says to use extended register 25 for current
                         * speed and duplex state, we'll stick to the
                         * standard BMCR register, although most PHYs
                         * don't reflect current speed/duplex state in
                         * the BMCR when Auto-Negotiating....
                         */
                        mii_link_state = PHY_LINK_UP | PHY_LINK_AUTONEG;
                        if (MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_BMCR) & BMCR_SPEED_100) {
                            mii_link_state |=
                                (PHY_LINK_100 | PHY_LINK_100BASE_TX);
                        } else {
                            mii_link_state |=
                                (PHY_LINK_10  | PHY_LINK_10BASE_T);
                        }

                    } else if (PHYID_TDK_2120(phy)) {
                        /*
                         * The TDK PHY's diagnostic register 18 (decimal)
                         * MR18.RATE, bit-10 (0x400) indicates the result of
                         * Auto-Negotiation data rate arbitration.  However,
                         * when using their pre-production Rev 3 device,
                         * our testing apparently revealed bit-9 (0x200) as
                         * the RATE indicator when connecting to dumb,
                         * non Auto-Negotiating hubs at 10 or 100 Mbit/sec,
                         * half-duplex.  TDK support confirms that bit-10
                         * RATE is a new feature in the Rev 7 device.  As
                         * such, we'll go with this apparent bit-9 heuristic
                         * on the earlier, Rev-3 device.
                         */
                        mii_link_state = PHY_LINK_UP | PHY_LINK_AUTONEG;
                        if (MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_R18) &
                                ((PHYID_REV(phy) >= 7) ? TDK_R18_RATE_100
                                                       : MII_BIT9)) {
                            mii_link_state |=
                                (PHY_LINK_100 | PHY_LINK_100BASE_TX);
                        } else {
                            mii_link_state |=
                               (PHY_LINK_10   | PHY_LINK_10BASE_T);
                        }

                    } else if (PHYID_DAVICOM_9101(phy)) {
                        /*
                         * The Davicom PHY apparently (as of 21Jan98 we
                         * have no data sheet) identifies auto-negotiation
                         * results in bits 12-15 of register 17 (decimal).
                         * 8nnn = 100-FDX, 4nnn = 100-HDX, 2nnn = 10-FDX,
                         * and 1nnn = 10-HDX.  Since an A-N partner's
                         * advertisement should be correctly reflected in
                         * the ANLPAR (register 5), we'll only troll for
                         * dumb, non A-N partners' half-duplex results here.
                         */
                        mii_link_state = PHY_LINK_UP | PHY_LINK_AUTONEG;
                        switch (MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_R17) & 0xF000) {
                            default:
                                mii_link_state = 0;
                                break;
                            case 0x4000:
                                mii_link_state |=
                                    (PHY_LINK_100 | PHY_LINK_100BASE_TX);
                                break;
                            case 0x1000:
                                mii_link_state |=
                                    (PHY_LINK_10  | PHY_LINK_10BASE_T);
                                break;
                        }
                    } else if (PHYID_LXT_971(phy)) {

                        // Unspecified, based on lab test observation 03Dec99                        */
                        mii_link_state = PHY_LINK_UP | PHY_LINK_AUTONEG;
                        if(MiiRead(&phy->mii_phy[ETH_MII_PHY], MII_R17) & 0x4000) {
							mii_link_state |= (PHY_LINK_100 | PHY_LINK_100BASE_TX);
						}
						else {
							mii_link_state |= (PHY_LINK_10  | PHY_LINK_10BASE_T);
                        }
                    }
                } /* end if - SW parallel detection logic */
            } /* end if - Auto-Negotiating */

            /* Latch 1st and subsequent PHY_LINK_NEW_MEDIA changes */
            if (mii_link_state == 0) {
                /* ??? Couldn't resolve link; need PHY_LINK_UNKNOWN err */
                mii_link_state = connect & (PHY_CONN_MII_TECHNOLOGY       |
                                            PHY_CONN_AUTONEG_REMOTE_FAULT |
                                            PHY_CONN_AUTONEG_FDX_PAUSE);
            } else if ((mii_link_state ^ phy->media.mii_link_state) &
                     (PHY_LINK_SPEED | PHY_LINK_FDX |
                      PHY_LINK_MII_TECHNOLOGY)) {
                phy->media.mii_link_state = mii_link_state;
                mii_link_state |= PHY_LINK_NEW_MEDIA;
            } else {
                phy->media.mii_link_state = mii_link_state;
            }

            /* Enable TDK RXCC option only on link down-to-up @ 100 Mbit */
            if ((phy->hw_options & PHY_HWOPTS_CK25DIS) &&
                    PHYID_TDK_2120(phy)) {
                if (mii_link_state & PHY_LINK_100) {
                    MII_SET_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R16, TDK_R16_RXCC);
                } else {
                    MII_CLR_BIT(&phy->mii_phy[ETH_MII_PHY], MII_R16, TDK_R16_RXCC);
                }
            }
        } /* endif - MII link down-to-up transition */

        /* Affix newly detected errors latched by PHY hardware */
        if ((bmsr & BMSR_JABBER_DETECT) &&
                (mii_link_state & (PHY_LINK_10BASE_TFD |
                                   PHY_LINK_10BASE_T))) {
            mii_link_error |= PHY_LINK_JABBER;
        }
        if (bmsr & BMSR_REMOTE_FAULT) {
            mii_link_error |= PHY_LINK_REMOTE_FAULT;
        }
    }
	return mii_link_state ;
}

DWORD PhyCheck(PHY *phy)
{
    DWORD           connect          = phy->media.connect;
    DWORD           hpna_link_state = 0 ;
    DWORD           cum_link_state   = phy->media.cum_link_state;
    DWORD           mii_link_state ;
    PHY_PORT        port_select;

    if (!IS_PHY_INITIALIZED(phy)) {
        return (PHY_LINK_INIT_ERROR);
    }

#if HOMEPLUGPHY
	if( PhyActiveHOMEPLUG( phy) )
	{
		mii_link_state = PHY_LINK_UP | PHY_LINK_AUTONEG | PHY_LINK_100 | PHY_LINK_100BASE_TX ;
		phy->media.mii_link_state = mii_link_state;
		phy->media.connect = PHY_CONN_AUTONEG | PHY_LINK_UP;
		return mii_link_state ;
	}	
#endif

    /*----------------------------------------------------------------*
     * (1) Examine HomePNA PHY
     *----------------------------------------------------------------*/
#ifdef JEDIPHY	// Bright
    hpna_link_state = JediPhyCheck(phy) ;
#endif

    /*----------------------------------------------------------------*
     * (2) Examine MII PHY
     *----------------------------------------------------------------*/
    mii_link_state = ETHPhyCheck(phy) ;

    /*----------------------------------------------------------------*
     * (3) Form cumulative link state based on HomePNA and MII states
     *----------------------------------------------------------------*/

    /* Initial state indicates Auto-Negotiation directive */
    cum_link_state = (connect & PHY_LINK_AUTONEG);

    if (!(connect & PHY_CONN_HPNA_TECHNOLOGY)) {
        /*
         * (3a) No HomePNA connect, so infer MII-only operation
         */
        cum_link_state |= mii_link_state;
        port_select = phy->mii_port;

    } else if (!(connect & PHY_CONN_MII_TECHNOLOGY)) {
        /*
         * (3b) No MII connect, so infer HomePNA-only operation
         */
        cum_link_state |= hpna_link_state;
        port_select = phy->hpna_port;

        /*
         * (3c) Simultaneous HomePNA and MII operation ...
         */
    } else if (!((hpna_link_state | mii_link_state) & PHY_LINK_UP)) {

        /*
         * (3c-1) Neither device reporting link: choose a port.
         *
         * By default, retain the current, active port.  After
         * LINK_DOWN_HOP_COUNT (nominally 5) successive cycles of
         * link down, we can optionally switch (hop) to the other,
         * inactive port or lock-in on the MII or HomePNA port if
         * the respective PHY_HWOPTS_PORT_HOPPING, PHY_HWOPTS_MII_ 
         * PORT_PRIORITY or PHY_HWOPTS_HPNA_PORT_PRIORITY options
         * are in effect.  Special port-hopping note:  certain
         * tested MII PHYs such as the TDK, QSI and GEC don't require
         * hopping to achieve link, thus we won't hop thereupon
         * to improve our odds of achieving link on the HomePNA
         * device.
         */
        cum_link_state |= (hpna_link_state | mii_link_state);
        port_select = phy->active_port;
        if (++phy->link_down_count >= LINK_DOWN_HOP_COUNT) {
            phy->link_down_count = 0;
            if ((phy->hw_options & PHY_HWOPTS_PORT_HOPPING) &&
                    (phy->inactive_port != PHY_PORT_NONE)) {
                port_select = phy->inactive_port;   /* (hop) */
            } else if (phy->hw_options & PHY_HWOPTS_MII_PORT_PRIORITY) {
                port_select = phy->mii_port;
            } else if (phy->hw_options & PHY_HWOPTS_HPNA_PORT_PRIORITY) {
                port_select = phy->hpna_port;
            }
        }

    } else {
        /*
         * (3c-2) One or both devices reporting link: choose a port.
         *
         * If both devices report link, retain the active port if
         * previously linked.  Otherwise, default to the MII link
         * status and select the MII port, since 10/100 Mbit/s is
         * considered a greater capability than 1 Mbit/s HomePNA.
         * This can be overriden via the PHY_HWOPTS_HPNA_PORT_PRIORITY
         * option.
         *
         * If only one device is reporting link, select that device's
         * port and use its associated link status.
         */
        phy->link_down_count = 0;
        if (hpna_link_state & mii_link_state & PHY_LINK_UP) {
            /* Both devices reporting link: choose a winner */
            if (phy->media.cum_link_state & PHY_LINK_UP) {
                port_select = phy->active_port;
                if (port_select == phy->hpna_port) {
                    cum_link_state |= hpna_link_state;
                } else {
                    cum_link_state |= mii_link_state;
                }

            } else if (phy->hw_options &
                       PHY_HWOPTS_HPNA_PORT_PRIORITY) {
                cum_link_state |= hpna_link_state;
                port_select = phy->hpna_port;

            } else {
                /* 10/100 Mbit/s MII > 1-Mit/s HomePNA */
                cum_link_state |= mii_link_state;
                port_select = phy->mii_port;
            }

        } else if (mii_link_state & PHY_LINK_UP) {
            /* MII-only link up */
            cum_link_state |= mii_link_state;
            port_select = phy->mii_port;

        } else {
            /* HomePNA-only link up */
            cum_link_state |= hpna_link_state;
            port_select = phy->hpna_port;
        }
    }

    /*----------------------------------------------------------------*
     * (4) Select port and merge into cumulative link state
     *----------------------------------------------------------------*/

    switch (port_select) {
        case PHY_PORT_HPNA_7WS:  
           cum_link_state |= PHY_LINK_PORT_7WS;  break;
        case PHY_PORT_HPNA_MII:  
           cum_link_state |= PHY_LINK_PORT_MII;  break;
        case PHY_PORT_MII:       
           cum_link_state |= PHY_LINK_PORT_MII;  break;
        default:                 /* ??? */                             break;
    }
    if (!(cum_link_state & PHY_LINK_UP)) {
        phy->media.cum_link_state &= ~PHY_LINK_UP;
    } else {
        if ((cum_link_state ^ phy->media.cum_link_state) &
                (PHY_LINK_PORT | PHY_LINK_FDX | PHY_LINK_SPEED |
                 PHY_LINK_TECHNOLOGY)) {
            cum_link_state |= PHY_LINK_NEW_MEDIA;
        }
        phy->media.cum_link_state = cum_link_state & ~PHY_LINK_NEW_MEDIA;
    }

    /*----------------------------------------------------------------*
     * (5) Optionally reconfigure upon new-media and/or port switch
     *----------------------------------------------------------------*/
    if ((cum_link_state & PHY_LINK_NEW_MEDIA) ||
            ((port_select != phy->active_port) &&
             (port_select != PHY_PORT_NONE))) {
        if ((port_select != phy->active_port) &&
                (port_select != PHY_PORT_NONE)) {
#if OLD
            phy->inactive_port = phy->active_port;
            phy->active_port = port_select;
#else
            Set_Active_Port( phy, port_select ) ;
#endif
        }
        if( !(phy->hw_options & PHY_HWOPTS_ISOLATE)) 
            PhyConfigurePort(phy, cum_link_state);
    }

    return (cum_link_state);
}

/***********************************************************************
 * PhyLastLinkUp() - Return current or last known, valid link state
 **********************************************************************/
DWORD
PhyLastLinkUp(PHY *phy)
{
    if (phy->media.cum_link_state != 0) {
        return (phy->media.cum_link_state | PHY_LINK_UP);
    }
    return (0);         /* valid link never established */
}

/***********************************************************************
 * PhyShutdown() - Disable (isolate) and power-down the PHY
 **********************************************************************/
void PhyShutdown(PHY *phy)
{
    (void) PhyDisable(phy, PHY_PORT_MII_DEVICE);
    MII_SET_MASK(phy->pActive_miiphy, MII_BMCR, BMCR_POWERDOWN);
}

#if 0
/***********************************************************************
 * PhyLoopback() - Enable/disable PHY loopback (one of four modes)
 **********************************************************************/
PHY_STATUS
PhyLoopback(PHY *phy, PHY_LOOPBACK mode)
{
    WORD        lbremr;

    if (!IS_PHY_INITIALIZED(phy)) {
        return (PHY_STATUS_UNINITIALIZED);
    }

    switch (mode) {
        default:
            mode = PHY_LOOPBACK_OFF;
            /* fall through ... */
        case PHY_LOOPBACK_MII:       /* MAC loopback at MII Tx/Rx */
            lbremr = LBREMR_LB_OFF;
            break;

        case PHY_LOOPBACK_PMD:       /* Loopback at the PMD Tx/Rx */
            lbremr = LBREMR_LB_XCVR;
            break;

        case PHY_LOOPBACK_REMOTE:    /* Media Rx Data looped onto Tx at
                                      * MII (e.g., link BER testing) */
            lbremr = LBREMR_LB_REMOTE;
            break;

        case PHY_LOOPBACK_10BASE_T:
            lbremr = LBREMR_LB_10BT;
            break;
    }

    /* Configure BMCR and LBREMR registers' loopback control bits */
    phy->loopback = mode;
    if (mode == PHY_LOOPBACK_MII) {
        MII_SET_BIT(phy->pActive_miiphy, MII_BMCR, BMCR_LOOPBACK);
    } else {
        MII_CLR_BIT(phy->pActive_miiphy, MII_BMCR, BMCR_LOOPBACK);
    }
    MII_CLR_MASK(phy->pActive_miiphy, MII_LBREMR, LBREMR_LB_MASK);
    MII_SET_MASK(phy->pActive_miiphy, MII_LBREMR, lbremr);

    return (PHY_STATUS_OK);
}

/***********************************************************************
 * PhyColTest() - Enable/disable collision test (diagnostic)
 **********************************************************************/
PHY_STATUS
PhyColTest(PHY *phy, BOOLEAN enable)
{
    phy = phy;
    return ((enable) ? PHY_STATUS_UNSUPPORTED_FUNCTION : PHY_STATUS_OK);
}
#endif

/***********************************************************************
 * PhyIdGet() - Read HW ID from registers 2 and 3
 **********************************************************************/
DWORD PhyIdGet(PHY *phy)
{
	return phy->pActive_miiphy->mii_id ;
}

BOOLEAN PhyActiveHPNA(PHY *phy)
{
#ifndef NON_BYPASSMODE	// test
	if ( phy->active_port & PHY_PORT_HPNA_DEVICES )
		return TRUE ;
#endif
	return FALSE ;
}

#if HOMEPLUGPHY

// TBD , need improvement	
BOOLEAN PhyActiveHOMEPLUG(PHY *phy)
{

	if ( phy->active_port & PHY_PORT_MII_DEVICE &&
	phy->mii_phy[ETH_MII_PHY].mii_id == HOMEPLUGID )
//	if ( phy->active_port & PHY_PORT_MII_DEVICE &&
//	phy->mii_phy[ETH_MII_PHY].mii_id != 0x221720 )
		return TRUE ;

	return FALSE ;
}
#endif


// Check if Any Phy attached to EMAC port
// input : 
//		Mac_Addr : beginnig of the emac mac address
//		mii_addr : if 0xff : search 0 to 31
//					  else search specified mii_addr (faster)
BOOLEAN Phy_EmacPortAttached(DWORD Mac_Addr, BYTE mii_addr) 
{
	BYTE	last_addr;
	WORD	bmsr;
	MIIOBJ TmpMii ;

	TmpMii.adapter = Mac_Addr ;
	TmpMii.preamble_suppression = FALSE ;

	if( mii_addr == 0xff )
	{
		last_addr = PHY_ADDR_MAX;
		mii_addr  = PHY_ADDR_MIN + 1;    
	}
	else
		last_addr = mii_addr ;

Search_Mii_1 :
	while(mii_addr <= last_addr) 
	{
		TmpMii.mii_addr = mii_addr++;
		if (((bmsr = MiiRead(&TmpMii, MII_BMSR)) != 0x0000) &&
			(bmsr != 0xFFFF)) 
			return TRUE ;
	}

	/* Go back to addr-0 once full MII search 1..31 fails */
	if( mii_addr == (PHY_ADDR_MAX+1) ) 
	{
		mii_addr = last_addr = PHY_ADDR_MIN ;
		goto Search_Mii_1 ;
	}
	
	return FALSE ;
}
