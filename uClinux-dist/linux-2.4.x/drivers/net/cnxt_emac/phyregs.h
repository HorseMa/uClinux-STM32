/****************************************************************************
*
*	Name:			phyregs.h

*  Module Description: PHY device register definitions.

*  Copyright © Conexant Systems 1999-2002.
*  Copyright © Rockwell Semiconductor Systems 1997-1998.
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

#ifndef _PHYREGS_H_
#define _PHYREGS_H_

#include "phytypes.h"

/*----------------------------------------------------------------------
 * (1) Register Map
 *
 * 100Base-X PHY and repeater port Media Independent Interface (MII)
 * register offsets.  The first sixteen registers are defined by
 * IEEE 802.3u (100Base-X) and 802.3y (100Base-T2); the next sixteen
 * registers are reserved for vendor-specific extensions.
 *--------------------------------------------------------------------*/

/* IEEE-defined standard register set */
#define MII_BMCR        0x00    /* Basic Mode Control Register */
#define MII_BMSR        0x01    /* Basic Mode Status Reg */

/* IEEE-defined extended register set */
#define MII_PHYIDR1     0x02    /* PHY Identifier Register #1 */
#define MII_PHYIDR2     0x03    /* PHY Identifier Register #2 */
#define MII_ANAR        0x04    /* Auto-Negotiation Advertisement Reg */
#define MII_ANLPAR      0x05    /* A-N Link Partner Availability Reg */
#define MII_ANER        0x06    /* Auto-Negotiation Expansion Reg */

/* Generic vendor extension registers (decimal) */
#define MII_R16         16
#define MII_R17         17
#define MII_R18         18
#define MII_R19         19
#define MII_R20         20
#define MII_R21         21
#define MII_R22         22
#define MII_R23         23
#define MII_R24         24
#define MII_R25         25
#define MII_R26         26
#define MII_R27         27
#define MII_R28         28
#define MII_R29         29
#define MII_R30         30
#define MII_R31         31

/*----------------------------------------------------------------------
 * (2) IEEE-defined register set
 *--------------------------------------------------------------------*/

/* Generic bit-value definitions */
#define MII_BIT0        0x0001
#define MII_BIT1        0x0002
#define MII_BIT2        0x0004
#define MII_BIT3        0x0008
#define MII_BIT4        0x0010
#define MII_BIT5        0x0020
#define MII_BIT6        0x0040
#define MII_BIT7        0x0080
#define MII_BIT8        0x0100
#define MII_BIT9        0x0200
#define MII_BIT10       0x0400
#define MII_BIT11       0x0800
#define MII_BIT12       0x1000
#define MII_BIT13       0x2000
#define MII_BIT14       0x4000
#define MII_BIT15       0x8000

/* Register 0, Basic Mode Control Register (BMCR) */
#define BMCR_RESET              0x8000
#define BMCR_LOOPBACK           0x4000
#define BMCR_SPEED_100          0x2000
#define BMCR_AUTONEG_ENABLE     0x1000
#define BMCR_POWERDOWN          0x0800
#define BMCR_ISOLATE            0x0400
#define BMCR_RESTART_AUTONEG    0x0200
#define BMCR_DUPLEX_MODE        0x0100
#define BMCR_COLLISION_TEST     0x0080
#define BMCR_RESERVED           0x007F
#define BMCR_HW_DEFAULT         \
    (BMCR_SPEED_100 | BMCR_AUTONEG_ENABLE | BMCR_DUPLEX_MODE)

/* Register 1, Basic Mode Status Register (BMSR) */
#define BMSR_100BASE_T4         0x8000
#define BMSR_100BASE_TXFD       0x4000
#define BMSR_100BASE_TX         0x2000
#define BMSR_10BASE_TFD         0x1000
#define BMSR_10BASE_T           0x0800
#define BMSR_100BASE_T2FD       0x0400
#define BMSR_100BASE_T2         0x0200
#define BMSR_RESERVED           0x0180
#define BMSR_PREAMBLE_SUPPRESS  0x0040
#define BMSR_AUTONEG_COMPLETE   0x0020
#define BMSR_REMOTE_FAULT       0x0010
#define BMSR_AUTONEG_ABILITY    0x0008
#define BMSR_LINK_STATUS        0x0004
#define BMSR_JABBER_DETECT      0x0002
#define BMSR_EXTENDED_CAPABLE   0x0001
#define BMSR_HW_DEFAULT                 \
    (BMSR_100BASE_TXFD | BMSR_100BASE_TX |      \
     BMSR_10BASE_TFD   | BMSR_10BASE_T   |      \
     BMSR_AUTONEG_ABILITY | BMSR_EXTENDED_CAPABLE)

/* Register 2-3, PHY Identifier Register 1-2 (PHYIDR1-2) */
#define GET_OUI_OF(phyidr1, phyidr2)                            \
    ((DWORD) ((phyidr1) << 6) | ((phyidr2) >> 10) & 0x003F))
#define GET_CANONICAL_OUI_OF(phyidr1, phyidr2)                  \
    ((BitSwap(((phyidr1) >> 10) & 0x003F) << 16) |              \
     (BitSwap(((phyidr1) >> 2)  & 0x00FF) <<  8) |              \
     (BitSwap(((phyidr1) << 6)  & 0x00C0) | (phyidr2 >> 10) & 0x003F)) 

/*
 * Register 4, Auto-Negotiation Advertisement Register (ANAR).
 *
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Warning!  Need to verify that the newer 802.3x and 802.3y bits
 * match the HW register definitions, as the PHY hardware design
 * may predate the IEEE specifications.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */
#define ANAR_NEXT_PAGE                  0x8000
#define ANAR_ACKNOWLEDGE                0x4000
#define ANAR_REMOTE_FAULT               0x2000
#define ANAR_100BASE_T2FD               0x1000  /* per 802.3y -T2 */
#define ANAR_100BASE_T2                 0x0800  /* per 802.3y -T2 */
#define ANAR_FDX_PAUSE                  0x0400  /* per 802.3x FDX */
#define ANAR_100BASE_T4                 0x0200
#define ANAR_100BASE_TXFD               0x0100
#define ANAR_100BASE_TX                 0x0080
#define ANAR_10BASE_TFD                 0x0040
#define ANAR_10BASE_T                   0x0020
#define ANAR_PROTOCOL_SELECTOR          0x001F
#define IS_ANAR_PROTOCOL_IEEE_802_3(anar)       \
    ((anar) & ANAR_PROTOCOL_SELECTOR) == 0x0001

/*
 * Register 5, Auto-Negotiation Link Partner Ability Register (ANLPAR),
 * which is the same format as the ANAR (Register 4)
 */
#define ANLPAR_NEXT_PAGE                ANAR_NEXT_PAGE
#define ANLPAR_ACKNOWLEDGE              ANAR_ACKNOWLEDGE
#define ANLPAR_REMOTE_FAULT             ANAR_REMOTE_FAULT
#define ANLPAR_100BASE_T2FD             ANAR_100BASE_T2FD
#define ANLPAR_100BASE_T2               ANAR_100BASE_T2
#define ANLPAR_FDX_PAUSE                ANAR_FDX_PAUSE
#define ANLPAR_100BASE_T4               ANAR_100BASE_T4
#define ANLPAR_100BASE_TXFD             ANAR_100BASE_TXFD
#define ANLPAR_100BASE_TX               ANAR_100BASE_TX
#define ANLPAR_10BASE_TFD               ANAR_10BASE_TFD
#define ANLPAR_10BASE_T                 ANAR_10BASE_T
#define ANLPAR_PROTOCOL_SELECTOR        ANAR_PROTOCOL_SELECTOR
#define IS_ANLPAR_PROTOCOL_IEEE_802_3   IS_ANPAR_PROTOCOL_IEEE_802_3

/* Register 6, Auto-Negotiation Expansion Register (ANER) */
#define ANER_RESERVED                           0xFFE0
#define ANER_PARALLEL_DETECTION_FAULT           0x0010
#define ANER_LINK_PARTNER_NEXT_PAGE_ABLE        0x0008
#define ANER_NEXT_PAGE_ABLE                     0x0004
#define ANER_PAGE_RECEIVED                      0x0002
#define ANER_LINK_PARNTER_AUTONEG_ABLE          0x0001

/*----------------------------------------------------------------------
 * (3a) RSS 20417 100Base-T2 extended register set
 *--------------------------------------------------------------------*/

/* RSS vendor extension registers */
#define MII_RSS_TBD     MII_R16

/* Register TBD, TBD */
#define RSS_RXX_BIT0    MII_BIT0

/*----------------------------------------------------------------------
 * (3b) NSC DP83840 10/100Base-TX extended register set
 *--------------------------------------------------------------------*/

/* NSC vendor extension registers */
#define MII_DCR         0x12    /* Disconnect Counter Reg */
#define MII_FCSCR       0x13    /* False Carrier Sense Counter Reg */
#define MII_RECR        0x15    /* Receive Error Counter Reg */
#define MII_SRR         0x16    /* Silicon Revision Reg */
#define MII_PCR         0x17    /* PCS Sub-Layer Configuration Reg */
#define MII_LBREMR      0x18    /* Loopback, Bypass & Rcvr Error Mask */
#define MII_PAR         0x19    /* PHY Address Reg */
#define MII_10BTSR      0x1B    /* 10Base-T Status Reg */
#define MII_10BTCR      0x1C    /* 10Base-T Configuration Reg */

/* Register 23, PCS sublayer Configuration Register (PCR) */
#define PCR_NRZI_ENABLE         0x8000
#define PCR_RESERVED            0x4F0B
#define PCR_TO_DIS              0x2000
#define PCR_REPEATER_MODE       0x1000
#define PCR_CLK25DIS            0x0080
#define PCR_FORCE_LINK_100      0x0040
#define PCR_FORCE_CONNECT       0x0020
#define PCR_TX_OFF              0x0010
#define PCR_LED_COLLISION       0x0004

/* Register 24, Loopback, Bypass & Rx Error Mask Register (LBREMR) */
#define LBREMR_RESERVED         0x84E1
#define LBREMR_BP_4B5B          0x4000
#define LBREMR_BP_SCR           0x2000
#define LBREMR_BP_ALIGN         0x1000
#define LBREMR_LB_MASK          0x0B00
#define LBREMR_LB_OFF           0x0000
#define LBREMR_LB_10BT          0x0800
#define LBREMR_LB_XCVR          0x0100
#define LBREMR_LB_REMOTE        0x0200
#define LBREMR_LB_UNDEFINED     0x0300
#define LBREMR_ALT_CRS          0x0040  /* 83840 RevA device feature */
#define LBREMR_XMT_DS           0x0020  /* 83840 RevA device feature */
#define LBREMR_CODE_ERR         0x0010
#define LBREMR_PE_ERR           0x0008
#define LBREMR_LINK_ERR         0x0004
#define LBREMR_PKT_ERR          0x0002

/* Register 25, PHY Address Register (PAR) */
#define PAR_RESERVED            0xFF80
#define PAR_SPEED_10            0x0040
#define PAR_CONNECT_STATUS      0x0020
#define PAR_ADDR_MASK           0x001F

/*----------------------------------------------------------------------
 * (3c) QSI 6612 10/100Base-TX extended register set
 *--------------------------------------------------------------------*/

/* Register 17, Mode Control */
#define QSI_R17_RESERVED_0_1   (MII_BIT0 | MII_BIT1)
#define QSI_R17_TEST            MII_BIT2
#define QSI_R17_PHY_ADDR       (MII_BIT3 | MII_BIT4 |           \
                                MII_BIT5 | MII_BIT6 | MII_BIT7)
#define QSI_R17_AN_TEST_MODE    MII_BIT8
#define QSI_R17_RESERVED_9_10  (MII_BIT9 | MII_BIT10)
#define QSI_R17_BTEXT           MII_BIT11
#define QSI_R17_T4_PRESENT      MII_BIT12
#define QSI_R17_RESERVED_13_15 (MII_BIT13 | MII_BIT14 | MII_BIT15)

/* Register 29, Interrupt Source */
#define QSI_R29_RXERR_CTR_FULL  MII_BIT0
#define QSI_R29_AN_PAGE_RCVD    MII_BIT1
#define QSI_R29_PARALLEL_FAULT  MII_BIT2
#define QSI_R29_AN_LP_ACK       MII_BIT3
#define QSI_R29_LINK_DOWN       MII_BIT4
#define QSI_R29_REMOTE_FAULT    MII_BIT5
#define QSI_R29_AN_COMPLETE     MII_BIT6
#define QSI_R29_RESERVED_7_15   0xFF80

/* Register 30, Interrupt Mask */
#define QSI_R30_ENABLE_RXERR_CTR_FULL   QSI_R29_RXERR_CTR_FULL
#define QSI_R30_ENABLE_AN_PAGE_RCVD     QSI_R29_AN_PAGE_RCVD
#define QSI_R30_ENABLE_PARALLEL_FAULT   QSI_R29_PARALLEL_FAULT
#define QSI_R30_ENABLE_AN_LP_ACK        QSI_R29_AN_LP_ACK
#define QSI_R30_ENABLE_LINK_DOWN        QSI_R29_LINK_DOWN
#define QSI_R30_ENABLE_REMOTE_FAULT     QSI_R29_REMOTE_FAULT
#define QSI_R30_ENABLE_AN_COMPLETE      QSI_R29_AN_COMPLETE
#define QSI_R30_RESERVED_7_14           0x7F80
#define QSI_R30_INTERRUPT_MODE          MII_BIT15

/* Register 31, BASE-TX PHY Control */
#define QSI_R31_SCR_DISABLE     MII_BIT0
#define QSI_R31_MLT3_DISABLE    MII_BIT1
#define QSI_R31_OPMODE         (MII_BIT2 | MII_BIT3 | MII_BIT4)
#define QSI_R31_TX_ISOLATE      MII_BIT5
#define QSI_R31_4B5BEN          MII_BIT6
#define QSI_R31_RESERVED_7      MII_BIT7
#define QSI_R31_DCREN           MII_BIT8
#define QSI_R31_RLBEN           MII_BIT9
#define QSI_R31_RESERVED_10_11 (MII_BIT10 | MII_BIT11)
#define QSI_R31_AN_COMPLETE     MII_BIT12
#define QSI_R31_DISABLE_RXERRCT MII_BIT13
#define QSI_R31_RESERVED_14_15 (MII_BIT14 | MII_BIT15)

/*----------------------------------------------------------------------
 * (3d) Level One LXT970 10/100Base-TX extended register set
 *--------------------------------------------------------------------*/

/* Register 17, Interrupt Enable Register */
#define LXT_R17_TINT            MII_BIT0
#define LXT_R17_INTEN           MII_BIT1

/* Register 18, Interrupt Status Register */
#define LXT_R18_XTALOK          MII_BIT14
#define LXT_R18_MINT            MII_BIT15

/* Register 19, Configuration Register */
#define LXT_R19_TX_DISCONNECT   MII_BIT0
#define LXT_R19_RESERVED_1      MII_BIT1
#define LXT_R19_100BASE_FX      MII_BIT2
#define LXT_R19_SCR_BYPASS      MII_BIT3
#define LXT_R19_4B5B_BYPASS     MII_BIT4
#define LXT_R19_ADVANCE_TXCLK   MII_BIT5
#define LXT_R19_LEDC_BITS      (MII_BIT6 | MII_BIT7)
#define LXT_R19_LEDC_COLLISION  0
#define LXT_R19_LEDC_OFF        MII_BIT6
#define LXT_R19_LEDC_ACTIVITY   MII_BIT7
#define LXT_R19_LEDC_ON        (MII_BIT6 | MII_BIT7)
#define LXT_R19_LINK_TEST       MII_BIT8
#define LXT_R19_JABBER_DISABLE  MII_BIT9
#define LXT_R19_SQE             MII_BIT10
#define LXT_R19_TP_LPBK_DISABLE MII_BIT11
#define LXT_R19_MDIO_INT        MII_BIT12
#define LXT_R19_REPEATER_MODE   MII_BIT13
#define LXT_R19_TXTEST_100      MII_BIT14
#define LXT_R19_RESERVED_15     MII_BIT15

/* Register 20, Chip Status Register */
#define LXT_R20_PLL_LOCK        MII_BIT0
#define LXT_R20_RESERVED_1      MII_BIT1
#define LXT_R20_LOW_VOLTAGE     MII_BIT2
#define LXT_R20_RESERVED_3      MII_BIT3
#define LXT_R20_MLT3_ERROR      MII_BIT4
#define LXT_R20_SYMBOL_ERROR    MII_BIT5
#define LXT_R20_SCR_LOCK        MII_BIT6
#define LXT_R20_RESERVED_7      MII_BIT7
#define LXT_R20_PAGE_RECEIVED   MII_BIT8
#define LXT_R20_AN_COMPLETE     MII_BIT9
#define LXT_R20_RESERVED_10     MII_BIT10
#define LXT_R20_SPEED_100       MII_BIT11
#define LXT_R20_DUPLEX          MII_BIT12
#define LXT_R20_LINK_UP         MII_BIT13
#define LXT_R20_RESERVED_14_15 (MII_BIT14 | MII_BIT15)

/*----------------------------------------------------------------------
 * (3e) GEC Plessey NWK936 10/100Base-TX extended register set
 *--------------------------------------------------------------------*/

/* "GPS (GEC) Defined" Register 24 */
#define GEC_R24_ERROR_CODE_0101 MII_BIT0        /* 0 = 0110, 1 = 0101 */
#define GEC_R24_BPSCR           MII_BIT1
#define GEC_R24_BPENC           MII_BIT2
#define GEC_R24_BPALIGN         MII_BIT3
#define GEC_R24_MF              MII_BIT4
#define GEC_R24_TEST_MODES     (MII_BIT5 | MII_BIT6)
#define GEC_R24_LEDCTL          MII_BIT7
#define GEC_R24_LB10CTL         MII_BIT8
#define GEC_R24_FRC_TX          MII_BIT9
#define GEC_R24_FRC_RX          MII_BIT10
#define GEC_R24_CRS_CTL_FDX_RX  MII_BIT11
#define GEC_R24_JAB_DIS         MII_BIT12
#define GEC_R24_SQE_DIS         MII_BIT13
#define GEC_R24_UNDEF_14_15    (MII_BIT14 | MII_BIT15)

/* "GPS (GEC) Defined" Register 25 */
#define GEC_R25_AN_STATE       (MII_BIT0 | MII_BIT1 |   \
                                MII_BIT2 | MII_BIT3)
#define GEC_R25_AN_ABIL_MATCH   MII_BIT4
#define GEC_R25_SPEED_100       MII_BIT5
#define GEC_R25_DUPLEX          MII_BIT6
#define GEC_R25_AN_COMPLETE     MII_BIT7
#define GEC_R25_PHY_ADDR       (MII_BIT8  | MII_BIT9  | \
                                MII_BIT10 | MII_BIT11 | MII_BIT12)
#define GEC_R25_UNDEF_13_15    (MII_BIT13 | MII_BIT14 | MII_BIT15)

/*----------------------------------------------------------------------
 * (3f) TDK 78Q2120 10/100Base-TX extended register set
 *--------------------------------------------------------------------*/

/* Register 16, Vendor Specific Register */
#define TDK_R16_RXCC            MII_BIT0
#define TDK_R16_PCSBP           MII_BIT1
#define TDK_R16_RSVD_2_3       (MII_BIT2 | MII_BIT3)
#define TDK_R16_RVSPOL          MII_BIT4
#define TDK_R16_APOL_DISABLE    MII_BIT5
#define TDK_R16_GPIO0_INPUT     MII_BIT6
#define TDK_R16_GPIO0_DAT       MII_BIT7
#define TDK_R16_GPIO1_INPUT     MII_BIT8
#define TDK_R16_GPIO1_DAT       MII_BIT9
#define TDK_R16_10BT_NAT_LPBK   MII_BIT10
#define TDK_R16_SQETEST_INHIBIT MII_BIT11
#define TDK_R16_RSVD_12_13     (MII_BIT12 | MII_BIT13)
#define TDK_R16_INTR_HIGH       MII_BIT14
#define TDK_R16_RPTR            MII_BIT15

/* Register 17, Interrupt Control/Status Register */
#define TDK_R17_ANEG_COMP_INT   MII_BIT0
#define TDK_R17_RFAULT_INT      MII_BIT1
#define TDK_R17_LS_CHG_INT      MII_BIT2
#define TDK_R17_LP_ACK_INT      MII_BIT3
#define TDK_R17_PDF_INT         MII_BIT4
#define TDK_R17_PRX_INT         MII_BIT5
#define TDK_R17_RXER_INT        MII_BIT6
#define TDK_R17_JABBER_INT      MII_BIT7
#define TDK_R17_ANEG_COMP_IE    MII_BIT8
#define TDK_R17_RFAULT_IE       MII_BIT9
#define TDK_R17_LS_CHG_IE       MII_BIT10
#define TDK_R17_LP_ACK_IE       MII_BIT11
#define TDK_R17_PDF_IE          MII_BIT12
#define TDK_R17_PRX_IE          MII_BIT13
#define TDK_R17_RXER_IE         MII_BIT14
#define TDK_R17_JABBER_IE       MII_BIT15

/* Register 18, Diagnostic Register */
#define TDK_R18_RSVD_0_7        0x00FF
#define TDK_R18_RX_LOCK         MII_BIT8
#define TDK_R18_RX_PASS         MII_BIT9
#define TDK_R18_RATE_100        MII_BIT10
#define TDK_R18_DPLX            MII_BIT11
#define TDK_R18_ANEG_FAIL       MII_BIT12
#define TDK_R18_RSVD_13_15     (MII_BIT13 | MII_BIT14 | MII_BIT15)

/*----------------------------------------------------------------------
 * (3g) Davicom 9101 10/100Base-TX extended register set
 *--------------------------------------------------------------------*/

/* No data sheet as of 04Feb98 ... */
#define DAV_RXX_BIT0            MII_BIT0

/*----------------------------------------------------------------------
 * (3h) ICS 1890 10/100Base-TX extended register set per "ICS1890
 *      10Base-T/100Base-TX Integrated PHYceiver(tm)" data sheet,
 *      ICS890RevG, dated 21Oct97.
 *--------------------------------------------------------------------*/

/* Register 16, Extended Control Register */
#define ICS_R16_CIPHER_DISABLE  MII_BIT0
#define ICS_R16_RESERVED_1      MII_BIT1
#define ICS_R16_INVLD_ERR_TEST  MII_BIT2
#define ICS_R16_NRZI_ENCODING   MII_BIT3
#define ICS_R16_RESERVED_4      MII_BIT4
#define ICS_R16_CIPHER_TEST     MII_BIT5
#define ICS_R16_PHY_ADDR       (MII_BIT6  | MII_BIT7  |         \
                                MII_BIT8  | MII_BIT9  | MII_BIT10)
#define ICS_R16_RESERVED_11_14 (MII_BIT11 | MII_BIT12 |         \
                                MII_BIT13 | MII_BIT14)
#define ICS_R16_CMDREG_OVERRIDE MII_BIT15

/* Register 17, QuickPoll Detailed Status Register */
#define ICS_R17_LINK_VALID      MII_BIT0
#define ICS_R17_REMOTE_FAULT    MII_BIT1
#define ICS_R17_JABBER_DETECT   MII_BIT2
#define ICS_R17_SD_100BASE_TX   MII_BIT3
#define ICS_R17_AN_COMPLETE     MII_BIT4
#define ICS_R17_PREMATURE_END   MII_BIT5
#define ICS_R17_HALT_SYMBOL     MII_BIT6
#define ICS_R17_INVALID_SYMBOL  MII_BIT7
#define ICS_R17_FALSE_CARRIER   MII_BIT8
#define ICS_R17_PLL_LOCK_ERROR  MII_BIT9
#define ICS_R17_RX_SIGNAL_ERROR MII_BIT10
#define ICS_R17_AN_PROGRESS    (MII_BIT11 | MII_BIT12 | MII_BIT13)
#define ICS_R17_DUPLEX          MII_BIT14
#define ICS_R17_DATA_RATE_100   MII_BIT15

/*----------------------------------------------------------------------
 * (3i) <Your 10/100 MII PHY Device here>
 *--------------------------------------------------------------------*/

/*----------------------------------------------------------------------
 * (4) HomePNA 1M8 SPI register definitions.  Registers are 8-bits
 *     except where noted.
 *--------------------------------------------------------------------*/

/* (4a) HomePNA SPI register map */
#define HPNA_CONTROL            0x00    /* 16-bit */
#define HPNA_STATUS             0x02    /* 16-bit */
#define HPNA_IMASK              0x04    /* 16-bit */
#define HPNA_ISTAT              0x06    /* 16-bit */
#define HPNA_TX_PCOM            0x08    /* 32-bit */
#define HPNA_RX_PCOM            0x0C    /* 32-bit */
#define HPNA_NOISE              0x10
#define HPNA_PEAK               0x11
#define HPNA_NSE_FLOOR          0x12
#define HPNA_NSE_CEILING        0x13
#define HPNA_NSE_ATTACK         0x14
#define HPNA_NSE_EVENTS         0x15
#define HPNA_AFE_CONTROL        0x16    /* 16-bit CN7221 Ramses dev */
#define HPNA_SPARE_2            0x17
#define HPNA_SLC_PEAK_MID       0x18	/* For Ramses only */
#define HPNA_AID_ADDRESS        0x19
#define HPNA_AID_INTERVAL       0x1A
#define HPNA_AID_ISBI           0x1B
#define HPNA_ISBI_SLOW          0x1C
#define HPNA_ISBI_FAST          0x1D
#define HPNA_TX_PULSE_WIDTH     0x1E
#define HPNA_TX_PULSE_CYCLES    0x1F

/* HomePNA SPI register 0x00, 16-bit HPNA_CONTROL */
#define HPNA_CONTROL_RESERVED           0x7009
#define HPNA_CONTROL_HIGH_POWER         0x0002
#define HPNA_CONTROL_HIGH_SPEED         0x0004
#define HPNA_CONTROL_POWERDOWN          0x0010
#define HPNA_CONTROL_STOP_SLICE_ADAPT   0x0020
#define HPNA_CONTROL_CLEAR_NSE_EVENTS   0x0040
#define HPNA_CONTROL_STOP_AID_AUTO_ADDR 0x0080
#define HPNA_CONTROL_CMD_HIGH_SPEED     0x0100
#define HPNA_CONTROL_CMD_LOW_SPEED      0x0200
#define HPNA_CONTROL_CMD_HIGH_POWER     0x0400
#define HPNA_CONTROL_CMD_LOW_POWER      0x0800
#define HPNA_CONTROL_IGNORE_REMOTE_CMD  0x8000

/* HomePNA SPI register 0x02, 16-bit HPNA_STATUS */
#define HPNA_STATUS_RESERVED            0x0F8F
#define HPNA_STATUS_RX_VERSION          0x0010
#define HPNA_STATUS_RX_SPEED            0x0020
#define HPNA_STATUS_RX_POWER            0x0040
#define HPNA_STATUS_INVERT_RXCLK        0x1000
#define HPNA_STATUS_INVERT_TXCLK        0x2000
#define HPNA_STATUS_INVERT_CLSN         0x4000
#define HPNA_STATUS_INVERT_RXCRS        0x8000

/* HomePNA SPI register 0x04, 16-bit HPNA_IMASK */
#define HPNA_IMASK_RESERVED             0x00F0
#define HPNA_IMASK_REMOTE_CMD_SENT      0x0001
#define HPNA_IMASK_REMOTE_CMD_RCVD      0x0002
#define HPNA_IMASK_PACKET_TRANSMITTED   0x0004
#define HPNA_IMASK_PACKET_RECEIVED      0x0008
#define HPNA_IMASK_TXPCOM_READY         0x0100
#define HPNA_IMASK_RXPCOM_VALID         0x0200
#define HPNA_IMASK_SW_INTERRUPTS        0xFC00

/* HomePNA SPI register 0x06, 16-bit HPNA_ISTAT */
#define HPNA_ISTAT_RESERVED             HPNA_IMASK_RESERVED
#define HPNA_ISTAT_REMOTE_CMD_SENT      HPNA_IMASK_REMOTE_CMD_SENT
#define HPNA_ISTAT_REMOTE_CMD_RCVD      HPNA_IMASK_REMOTE_CMD_RCVD
#define HPNA_ISTAT_PACKET_TRANSMITTED   HPNA_IMASK_PACKET_TRANSMITTED
#define HPNA_ISTAT_PACKET_RECEIVED      HPNA_IMASK_PACKET_RECEIVED
#define HPNA_ISTAT_TXPCOM_READY         HPNA_IMASK_TXPCOM_READY
#define HPNA_ISTAT_RXPCOM_VALID         HPNA_IMASK_RXPCOM_VALID         
#define HPNA_ISTAT_SW_INTERRUPTS        HPNA_IMASK_SW_INTERRUPTS

/* HomePNA SPI NSE_FLOOR register   0x12 @ 13 mV granularity */
#define HPNA_NSE_FLOOR_HWDEFAULT        0x04
#define HPNA_NSE_FLOOR_SWOVERRIDE       0x07

/* HomePNA SPI NSE_CEILING register 0x13 @ 13 mV granularity */
#define HPNA_NSE_CEILING_HWDEFAULT      0xD0    /* 7-bit 0x50? */
#define HPNA_NSE_CEILING_SWOVERRIDE     0x7F

/* HomePNA SPI NSE_ATTACK  register 0x14 @ 13 mV granularity */
#define HPNA_NSE_ATTACK_HWDEFAULT       0xF4
#define HPNA_NSE_ATTACK_SWOVERRIDE      0xF4

/* 11625 Ramses (CN7221) 16-bit AFE_CONTROL test register 0x16 */
#define HPNA_AFE_CONTROL_PD_TX          0x0001  /* powerdown Tx */
#define HPNA_AFE_CONTROL_HPCTR          0x0002  /* Shadows reg 0.1 */
#define HPNA_AFE_CONTROL_PD_RXIFAMP     0x0004  /* powerdown Rx amp */
#define HPNA_AFE_CONTROL_RXGAIN         0x0008  /* 1=50, 0=30 */
#define HPNA_AFE_CONTROL_PD_VC          0x0010  /* mid-rail current */
#define HPNA_AFE_CONTROL_PD_RECTI       0x0020  /* Rx rectifier block */
#define HPNA_AFE_CONTROL_PD_ENV         0x0040  /* Rx envelope amp */
#define HPNA_AFE_CONTROL_TRI_REF        0x0080  /* tristate slice ref */
#define HPNA_AFE_CONTROL_PD_COMP        0x0100  /* Rx slice comparatr */
#define HPNA_AFE_CONTROL_PEAK_DELAY     0x0E00  /* Noise-to-Peak wait */
#define HPNA_AFE_CONTROL_NOISE_SLICE    0x3000  /* Noise slice increase */
#define HPNA_AFE_CONTROL_RESERVED       0xC000


#endif /* _PHYREGS_H_ */
