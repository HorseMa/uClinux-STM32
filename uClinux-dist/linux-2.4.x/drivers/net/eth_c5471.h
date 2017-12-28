/***********************************************************************
 * linux/drivers/net/eth_c5471.h
 *
 *   Copyright (C) 2004-2006 Arcturus Networks Inc. 
 *                 by Ted Ma and David Wu 
 *                 <www.ArcturusNetworks.com>
 *
 *   Copyright (C) 2003 Cadenux, LLC. All rights reserved.
 *   todd.fischer@cadenux.com  <www.cadenux.com>
 *
 *   Copyright (C) 2001 RidgeRun, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***********************************************************************/

#ifndef _ETH_C5471_H_
#define _ETH_C5471_H_

/***********************************************************************
 *Included Files
 ***********************************************************************/

#include <asm/arch/sysdep.h> /* LINUX_20 */
#include <linux/netdevice.h>
/***********************************************************************
 * Compilation Switches
 ***********************************************************************/

//#define ETHER_DEBUG 1 
//#define VERBOSE     1 
//#define PHY_DEBUG   1 

/* This is the most common case for modern hardware. */

#ifdef LINUX_20
# undef TX_RING
# define TX_RING    0
#else
# ifndef TX_RING
#  define TX_RING   0
# endif
#endif

#ifndef TX_EVENT
# define TX_EVENT   1
#endif

/* gives all descriptors which belong to a TX packet to HW at the same time
   not descriptor after desriptor which causes TX problems (lost TX packets) 
   because of TX underruns when running via nfs */
#define ETH_C5471_ALL_DESC_AT_ONCE 	1
//#undef ETH_C5471_ALL_DESC_AT_ONCE

/* define if you want to use the ESM otherwise use directly ENET0 */
//#define ETH_C5471_USE_ESM 			1
#undef ETH_C5471_USE_ESM

/***********************************************************************
 * Definitions
 ***********************************************************************/

/* Common debug definitions */

#ifdef ETHER_DEBUG

 /* Debug is enabled. */

/* define MY_KERN_DEBUG KERN_DEBUG */

# define MY_KERN_DEBUG
# define dbg(format, arg...) \
    printk(MY_KERN_DEBUG __FUNCTION__ ": " format "\n" , ## arg)

 /* Check if verbose debug output is enabled. */

# ifdef VERBOSE

#   define verbose(format, arg...) \
      printk(MY_KERN_DEBUG __FUNCTION__ ": " format "\n" , ## arg)

# else /* VERBOSE */

#   define verbose(format, arg...) do {} while (0)

# endif /* VERBOSE */

#else /* ETHER_DEBUG */

 /* Can't have VERBOSE debug without ETHER_DEBUG */

# undef  VERBOSE
# define dbg(format, arg...) do {} while (0)
# define verbose(format, arg...) do {} while (0)

#endif /* ETHER_DEBUG */

#define err(format, arg...) \
  printk(KERN_ERR __FUNCTION__ ": " format "\n" , ## arg)
#define info(format, arg...) \
  printk(KERN_INFO __FUNCTION__ ": " format "\n" , ## arg)
#define warn(format, arg...) \
  printk(KERN_WARNING __FUNCTION__ ": " format "\n" , ## arg)

#ifndef nop
#define nop() asm("    nop");
#endif /* nop */

/* Byte management macros */

#define __swap_16(x)       \
  ((((x) & 0xff00) >> 8) | \
   (((x) & 0x00ff) << 8))

#define __swap_32(x)            \
  ((((x) & 0xff000000) >> 24) | \
   (((x) & 0x00ff0000) >>  8) | \
   (((x) & 0x0000ff00) <<  8) | \
   (((x) & 0x000000ff) << 24))

#define EIM_RAM_START        ((void *)0xffd00000)

/* Register definitions */

#define ETHER_BASE           0xffff0000 
#define EIM_CTRL_REG         ((volatile unsigned long *)0xffff0000)
#define EIM_STATUS_REG       ((volatile unsigned long *)0xffff0004)
#define EIM_CPU_TXBA_REG     ((volatile unsigned long *)0xffff0008)
#define EIM_CPU_RXBA_REG     ((volatile unsigned long *)0xffff000c)
#define EIM_BUFSIZE_REG      ((volatile unsigned long *)0xffff0010)
#define EIM_CPU_FILTER_REG   ((volatile unsigned long *)0xffff0014)
#define EIM_CPU_DAHI_REG     ((volatile unsigned long *)0xffff0018)
#define EIM_CPU_DALO_REG     ((volatile unsigned long *)0xffff001c)
#define EIM_MFVHI_REG        ((volatile unsigned long *)0xffff0020)
#define EIM_MFVLO_REG        ((volatile unsigned long *)0xffff0024)
#define EIM_MFMHI_REG        ((volatile unsigned long *)0xffff0028)
#define EIM_MFMLO_REG        ((volatile unsigned long *)0xffff002c)
#define EIM_RXTH_REG         ((volatile unsigned long *)0xffff0030)
#define EIM_CPU_RXRDY_REG    ((volatile unsigned long *)0xffff0034)
#define EIM_INT_EN_REG       ((volatile unsigned long *)0xffff0038)

#define ENET0_MODE_REG       ((volatile unsigned long *)0xffff0100)
#define ENET0_BOFFSEED_REG   ((volatile unsigned long *)0xffff0104)
#define ENET0_FLWPAUSE_REG   ((volatile unsigned long *)0xffff010c)
#define ENET0_FLWCONTROL_REG ((volatile unsigned long *)0xffff0110)
#define ENET0_VTYPE_REG      ((volatile unsigned long *)0xffff0114)
#define ENET0_SYS_ERR_REG    ((volatile unsigned long *)0xffff0118)
#define ENET0_TXBUF_RDY_REG  ((volatile unsigned long *)0xffff011c)
#define ENET0_TDBA_REG       ((volatile unsigned long *)0xffff0120)
#define ENET0_RDBA_REG       ((volatile unsigned long *)0xffff0124)
#define ENET0_PARHI_REG      ((volatile unsigned long *)0xffff0128)
#define ENET0_PARLO_REG      ((volatile unsigned long *)0xffff012c)
#define ENET0_LARHI_REG      ((volatile unsigned long *)0xffff0130)
#define ENET0_LARLO_REG      ((volatile unsigned long *)0xffff0134)
#define ENET0_ADRMODE_EN_REG ((volatile unsigned long *)0xffff0138)
#define ENET0_DRP_REG        ((volatile unsigned long *)0xffff013c)

#define GPIO_IO_REG          ((volatile unsigned long *)0xffff2800)
#define GPIO_CIO_REG         ((volatile unsigned long *)0xffff2804)
#define GPIO_EN_REG          ((volatile unsigned long *)0xffff2814)

#define CLKM_REG             ((volatile unsigned long *)0xffff2f00)
#define CLKM_CTL_RST_REG     ((volatile unsigned long *)0xffff2f10)
#define CLKM_RESET_REG       ((volatile unsigned long *)0xffff2f18)

/* Bit definitions */

#define EIM_FILTER_UNICAST     	0x00000001  
#define EIM_FILTER_BROADCAST   	0x00000002
#define EIM_FILTER_MULTICAST   	0x00000004
#define EIM_FILTER_HASH_MCAST  	0x00000008

/* EIM ESM Control Register */
#define EIM_CTRL_ESM_EN		   	0x00008000
#define EIM_CTRL_ENET0_EN      	0x00000100
//#define EIM_CTRL_ENET1_DIS     	0x00000100
#define EIM_CTRL_ENET0_FLW     	0x00000040
#define EIM_CTRL_ENET0_RXEN    	0x00000020  
#define EIM_CTRL_ENET0_TXEN    	0x00000010
#define EIM_CTRL_ENET1_RXEN    	0x00000008
#define EIM_CTRL_ENET1_TXEN    	0x00000004
#define EIM_CTRL_CPU_RXEN      	0x00000002
#define EIM_CTRL_CPU_TXEN      	0x00000001

/* EIM ESM Status Register */
#define EIM_CPU_TX_LIF_INT     	0x00000200
#define EIM_CPU_RX_LIF_INT     	0x00000100
#define EIM_CPU_TX_INT         	0x00000080
#define EIM_CPU_RX_INT         	0x00000040
#define EIM_ENET0_ERR_INT      	0x00000004
#define EIM_ENET0_TX_INT       	0x00000002
#define EIM_ENET0_RX_INT       	0x00000001

/* EIM CPU Filtering Control Register */
// tbd

/* ENET0 Mode Register */
#define ENET0_MODE_FIFO_EN     	0x00008000
#define ENET0_MODE_RJCT_SFE     0x00000080
#define ENET0_MODE_DPNET     	0x00000040
#define ENET0_MODE_MWIDTH       0x00000020
#define ENET0_MODE_WRAP       	0x00000010
#define ENET0_MODE_FDWRAP       0x00000008
#define ENET0_MODE_HALFDUPLEX   0x00000000
#define ENET0_MODE_FULLDUPLEX   0x00000004
#define ENET0_MODE_ENABLE       0x00000001

/* ENET0 System Error Interrupt Status Register */
#define ENET0_SE_SR_TX_FE      	0x00000010
#define ENET0_SE_SR_RX_BCE     	0x00000008
#define ENET0_SE_SR_FIFE      	0x00000004
#define ENET0_SE_SR_OFLW      	0x00000002
#define ENET0_SE_SR_UFLW      	0x00000001

/* ENET0 Address Mode Enable Register */
#define ENET_ADR_PROMISCUOUS   	0x00000008
#define ENET_ADR_BROADCAST     	0x00000004
#define ENET_ADR_HAST_FLTR     	0x00000002
#define ENET_ADR_DEST_ADDR     	0x00000001

/* CPU/ENET RX/TX Descriptor Word #0/#1 (32bits) */
#define EIM_DESC_OWNER_HOST    	0x80000000
#define EIM_DESC_OWNER_ENET0   	0x00000000
#define	EIM_DESC_OWNER_DEVICE	0x80000000
#define EIM_DESC_OWNER_ESM		0x00000000

#define EIM_DESC_WRAP        	0x40000000
#define EIM_DESC_FIF        	0x20000000
#define EIM_DESC_LIF         	0x10000000
#define EIM_DESC_RETRY_MASK    	0x0f000000
#define EIM_DESC_INTEN         	0x00800000
#define EIM_DESC_PADCRC       	0x00004000  
// number of data bytes of one descriptor
#define EIM_DESC_PACKET_BYTES  	0x00000040

/* This mask is used to clean the packet size in the RX descriptor */
#define EIM_DESC_BYTES_MASK     0xfffff800

/* Status bit positions for RX Descriptor Word #0/#1 (32bits) */
#define RXSTATUS_MISS_BIT      0x00400000
#define RXSTATUS_VLAN_BIT      0x00200000
#define RXSTATUS_LONG_BIT      0x00100000
#define RXSTATUS_SHORT_BIT     0x00080000
#define RXSTATUS_CRC_BIT       0x00040000
#define RXSTATUS_OVER_BIT      0x00020000
#define RXSTATUS_ALIGN_BIT     0x00010000

/* Status bit positions for TX Descriptor Word #0/#1 (32bits) */
#define TXSTATUS_RETRY_BIT     0x00400000
#define TXSTATUS_HEART_BIT     0x00200000
#define TXSTATUS_LATE_COL_BIT  0x00100000
#define TXSTATUS_COLLISION_BIT 0x00080000
#define TXSTATUS_CRC_BIT       0x00040000
#define TXSTATUS_UNDER_BIT     0x00020000
#define TXSTATUS_LOSS_BIT      0x00010000

/* EIM Reset */
#define CLKM_RESET_EIM          0x00000008
#define CLKM_EIM_CLK_STOP       0x00000010
#define CLKM_CTL_RST_LEAD_RESET 0x00000000
#define CLKM_CTL_RST_EXT_RESET  0x00000002

/* Number of TX/RX descriptors */

#ifdef CONFIG_MACH_UC5471DSP
#define CONFIG_FEC_LXT972  /* may define it in config*/
#define CADENUX_ETHERNET_PHY CONFIG_FEC_LXT972  /* may define it in .config*/

/* 16KB packet memory, over NFS, cannot receive 3 packets(4096Byte or more) and cause overrun error.
 * Total Descriptors are 0x6C0/8 = 216 but only 215(64+4 Bytes) buffers, so NUM_DESC_TX+NUM_DESC_RX should less than 216
 */
#define NUM_DESC_TX             47
#define NUM_DESC_RX             168
#else
#define NUM_DESC_TX             32
#define NUM_DESC_RX             64
#endif

/* IRQ for the c5471 ethernet hardware */

#define ETHER_IRQ               0x04

/* The number of I/O ports used by the ethercard. */

#define NETCARD_IO_EXTENT       0x140

/* The C5471 EVM board (C5472) uses a Lucent LU3X31T-T64 transeiver
 * device to handle the physical layer (PHY). It's a HW block that
 * on the one end offers a Media Independent Interface (MII) which is
 * connected to the Ethernet Interface Module (EIM) internal to the
 * C5472 and on the other end offers either the 10baseT or 100baseT
 * electrical interface connecting to an RJ45 onboard network connector.
 * The PHY transeiver has several internal registers allowing host
 * configuration and status access. These internal registers are
 * accessable by clocking serial data in/out of the MDIO pin of the
 * LU3X31T-T64 chip. For C5471, the MDC and the MDIO pins are
 * connected to the C5472 GPIO15 and GPIO14 pins respectivley. Host
 * software twidles the GPIO pins appropriately to get data serially
 * into and out of the chip. This is typically a one time operation at
 * boot and normal operation of the transeiver involves EIM/Transeiver
 * interaction at the other pins of the transeiver chip and doesn't
 * require host intervention at the MDC and MDIO pins.
 */

#define LU3X31_T64_IDval       0x00437421
#define MD_PHY_MSB_REG         0x02
#define MD_PHY_LSB_REG         0x03
#define MD_PHY_CONTROL_REG     0x00
#define MD_PHY_CTRL_STAT_REG   0x17
#define MODE_AUTONEG           0x1000
#define MODE_10MBIT_HALFDUP    0x0000
#define MODE_10MBIT_FULLDUP    0x0100
#define MODE_100MBIT_FULLDUP   0x2100
#define MODE_100MBIT_HALFDUP   0x2000

#ifdef CONFIG_FEC_LXT972
#define LXT972_ID_val          0x001378E2
#define ETH_5471_LINK_IRQ      3


/* Register definitions for the PHY. */

#define MII_REG_CR          0  /* Control Register                         */
#define MII_REG_SR          1  /* Status Register                          */
#define MII_REG_PHYIR1      2  /* PHY Identification Register 1            */
#define MII_REG_PHYIR2      3  /* PHY Identification Register 2            */
#define MII_REG_ANAR        4  /* A-N Advertisement Register               */ 
#define MII_REG_ANLPAR      5  /* A-N Link Partner Ability Register        */
#define MII_REG_ANER        6  /* A-N Expansion Register                   */
#define MII_REG_ANNPTR      7  /* A-N Next Page Transmit Register          */
#define MII_REG_ANLPRNPR    8  /* A-N Link Partner Received Next Page Reg. */

/* values for phy_status */

#define PHY_CONF_ANE    0x0001  /* 1 auto-negotiation enabled */
#define PHY_CONF_LOOP   0x0002  /* 1 loopback mode enabled */
#define PHY_CONF_SPMASK 0x00f0  /* mask for speed */
#define PHY_CONF_10HDX  0x0010  /* 10 Mbit half duplex supported */
#define PHY_CONF_10FDX  0x0020  /* 10 Mbit full duplex supported */ 
#define PHY_CONF_100HDX 0x0040  /* 100 Mbit half duplex supported */
#define PHY_CONF_100FDX 0x0080  /* 100 Mbit full duplex supported */ 

#define PHY_STAT_LINK   0x0100  /* 1 up - 0 down */
#define PHY_STAT_FAULT  0x0200  /* 1 remote fault */
#define PHY_STAT_ANC    0x0400  /* 1 auto-negotiation complete  */
#define PHY_STAT_SPMASK 0xf000  /* mask for speed */
#define PHY_STAT_10HDX  0x1000  /* 10 Mbit half duplex selected */
#define PHY_STAT_10FDX  0x2000  /* 10 Mbit full duplex selected */ 
#define PHY_STAT_100HDX 0x4000  /* 100 Mbit half duplex selected */
#define PHY_STAT_100FDX 0x8000  /* 100 Mbit full duplex selected */

/***********************************************
 * PHYS defines
 * PHY - LX972 register and defines
 **********************************************/
#define PHYAddr    0x0001       /* Address of the LX972 for HEI board   */
//#define PHYAddr    0xffff0000   // Address of the LX972 for C5471 uCdimm 
/******* registers *********/
#define PHYCtl          0       // Control Register Refer to Table 37 on page 58 
#define PHYStat         1       // Status Register #1 Refer to Table 38 on page 58
#define PHYId1          2       // PHY Identification Register 1 Refer to Table 39
                                // on page 59
#define PHYId2          3       // PHY Identification Register 2 Refer to 
                                // Table 40 on page 60
#define PHYANegAd       4       // Auto-Negotiation Advertisement Register
                                // Refer to Table 41 on page 61
#define PHYANegLink     5       // Auto-Negotiation Link Partner Base Page Ability 
                                // Register Refer to Table 42 on page 62
#define PHYANegEx       6       // Auto-Negotiation Expansion Register Refer to 
                                // Table 43 on page 63
#define PHYANegNext     7       // Auto-Negotiation Next Page Transmit Register
                                // Refer to Table 44 on page 63
#define PHYANegLPart    8       // Auto-Negotiation Link Partner Received Next Page
                                // Register Refer to Table 45 on page 64
#define PHY100Ctl       9       // 1000BASE-T/100BASE-T2 Control Register Not 
                                // Implemented
#define PHY100Stat      10      // 1000BASE-T/100BASE-T2 Status Register Not 
                                // Implemented
#define PHYExStat       15      // Extended Status Register Not Implemented
#define PHYPortCfg      16      // Port Configuration Register Refer to Table 
                                // 46 on page 64
#define PHYStat2        17      // Status Register #2 Refer to Table 47 on page 65
#define PHYIntEn        18      // Interrupt Enable Register Refer to Table 48 
                                // on page 66
#define PHYIntStat      19      // Interrupt Status Register Refer to Table 49
                                // on page 66
#define PHYLedCfg       20      // LED Configuration Register Refer to Table 50
                                // on page 68
                  // 21 - 29 Reserved
#define PHYTxCtl        30      // Transmit Control Register Refer to Table 51
                                //      on page 69
#define PHYCtlReset    0x8000  // reset
#define PHYANegAdMask  0x1e0   // bits for duplex and speed (972)
#define PHYB8          0x100   // bit 8
#define PHYB7          0x080   // bit 7
#define PHYB6          0x040   // bit 6
#define PHYB5          0x020   // bit 5
#define PHYLEDCFG      0xD0F2  // LED 1 (left - link and activity
                               //     2 (right - Speed)
#define PHYLEDANEG     0x1e1   // Advertize AutoNeg all speed / duplex

#define PHY_BUSYLOOP_COUNT    100  // each loop is 10mS -> 1S
#define PHY_RESETLOOP_COUNT   10   // number of times to check reset 
                                   //  10 ms between checks - manual
                                   //  states 300 uS max fro reset to work
#define PHY_NEGLOOP_COUNT     150  // 1.5 seconds for Autoneg to complete

#define ANDONE     0x0080   // auto negotiation interrupt
#define SPEEDCHG   0x0040   // speed change interrupt 
#define DUPLEXCHG  0x0020   // duplex change interrupt
#define LINKCHG    0x0010   // link change interrupt

#define ENET0_ERR  0x00000004  // ENET ERR bit in EIM ESM Status Register

/**********************************
 * Bit Defines for PHY972_CONTROL
 **********************************/
#define PHY_CONTROL_RESET           0x8000
#define PHY_CONTROL_LOOPBACK        0x4000
#define PHY_CONTROL_SELECT_SPEED    0x2000
#define PHY_CONTROL_AUTONEG         0x1000
#define PHY_CONTROL_POWERDOWN       0x0800
#define PHY_CONTROL_ISOLATE         0x0400
#define PHY_CONTROL_RESTART_AUTONEG 0x0200
#define PHY_CONTROL_DUPLEXMODE      0x0100
#define PHY_CONTROL_COLLISION_TEST  0x0080
#define PHY_CONTROL_SPEED_10_HALF   0x0000
#define PHY_CONTROL_SPEED_10_FULL   (PHY_CONTROL_DUPLEXMODE)
#define PHY_CONTROL_SPEED_100_HALF  (PHY_CONTROL_SELECT_SPEED )
#define PHY_CONTROL_SPEED_100_FULL  (PHY_CONTROL_SELECT_SPEED | PHY_CONTROL_DUPLEXMODE)
#define PHY_CONTROL_AUTONEG_HALF_10     (0x1000 | PHY_CONTROL_RESTART_AUTONEG)
#define PHY_CONTROL_AUTONEG_FULL_10     (0X1100 | PHY_CONTROL_RESTART_AUTONEG)
#define PHY_CONTROL_AUTONEG_HALF_100    (0x3000 | PHY_CONTROL_RESTART_AUTONEG)
#define PHY_CONTROL_AUTONEG_FULL_100    (0X3100 | PHY_CONTROL_RESTART_AUTONEG)

/*****************************************
 * Bit Defines for PHY972_STATUS_REG_TWO
 *****************************************/
    /* not used                                     0x8000  */
#define PHY_STATUS_REG_TWO_MODE_100             0x4000
#define PHY_STATUS_REG_TWO_TX_STATUS            0x2000
#define PHY_STATUS_REG_TWO_RX_STATUS            0x1000
#define PHY_STATUS_REG_TWO_COLLISION_STATUS     0x0800
#define PHY_STATUS_REG_TWO_LINK                 0x0400
#define PHY_STATUS_REG_TWO_DUPLEX_MODE          0x0200
#define PHY_STATUS_REG_TWO_AUTO_NEGOTIATION     0x0100
#define PHY_STATUS_REG_TWO_AUTO_NEGO_COMPLETE   0x0080
    /* not used                                 0x0040  */
#define PHY_STATUS_REG_TWO_POLARITY             0x0020
#define PHY_STATUS_REG_TWO_PAUSE                0x0010
#define PHY_STATUS_REG_TWO_ERROR                0x0008
    /* not used                                 0x0004  */
    /* not used                                 0x0002  */
    /* not used                                 0x0001  */
#endif

/* You'll notice a few places that reference ROUNDUP. The idea here is
 * that we want to xfer bytes out of packet memory into the client's
 * buffer in the most efficient way possible. This may involve xfering
 * the data 16bits at a time or even 32bits at a time if possible.
 * What this means is that if the packet actually isn't evenly
 * divisible by the xfer size of 16bits or 32bits then we have to
 * round *up* to insure that we xfer all the bytes of the packet
 * recognizing that we may actually get a bit more data beyond 
 * the last byte of the actual packet as the algorithm proceeds to the
 * nearest whole 16bit or 32bit boundary.  This is harmless *if* the
 * client buffer actually has the extra bytes allocated to allow us to
 * overshoot slightly during the packet transfer -- in the end 
 * we'll report an actual byte length of the packet and the client
 * will not even know that a byte or two of bogus data actually follows
 * the real packet. So this definition will help us manage this
 * mechanism.
 */

#define ROUNDUP 1

#ifndef LINUX_20
# define MY_TX_TIMEOUT  ((400*HZ)/1000)
#endif

/* For now */

#if !TX_RING
# define tx_full(dev) 0
#endif

  /* These two address locations are special in that they are used
   * by the bootloader to communicate a default device MAC to the kernel.
   */

#if 1
  /* Ether MAC string deposited by bootloader to IRAM.  */

#define __EtherMACMagicStr (unsigned char *)0xffc00100
#define __EtherMAC         (unsigned char *)0xffc0010C

#else  
  /* Ether MAC string deposited by bootloader to SDRAM
   * Careful! if you enable this then you are essentially declaring
   * this area of SDRAM "reserved" and need to make sure that your
   * kernel's CADENUX_DRAM_BASE setting (see asm/arch/memconfig.h) is a
   * value larger than the space we are reserving here. Plan on this
   * group of strings representing the kernel command line string and
   * MAC string taking 0x100 bytes. That is your reserved amount of SDRAM.
   */

#define __EtherMACMagicStr (unsigned char *)0x10000100
#define __EtherMAC         (unsigned char *)0x1000010C

#endif  
  
#ifdef LINUX_20
# define NET_DEVICE       device
# define NET_DEVICE_STATS enet_statistics
#else
# define NET_DEVICE       net_device
# define NET_DEVICE_STATS net_device_stats
#endif

/***********************************************************************
 * Public Type Declarations
 ***********************************************************************/

struct net_local
{
  struct NET_DEVICE_STATS stats;
  long   open_time;                 /* Useless example local info. */
  int    phySpeed;               // 00-Auto, 0x1-10 Mbs, 0x2-100Mbs, 0x3-Auto
  int    phyDpx;                 // duplex 00-Half, 0x1-Full (not valid with??)

#ifndef LINUX_20
  /* Tx control lock.  This protects the transmit buffer ring
   * state along with the "tx full" state of the driver.  This
   * means all netif_queue flow control actions are protected
   * by this lock as well.
   */

  spinlock_t lock;
#endif /* LINUX_20 */
};

/***********************************************************************
 * Public Variables
 ***********************************************************************/

extern int LAN_Rate;

/***********************************************************************
 * Public Functions
 ***********************************************************************/
extern int  eth_recv_int_active(void);
extern int  eth_xmit_int_active(void);

extern void eth_get_default_MAC(unsigned char *mac_array);
extern void eth_set_MAC(unsigned char *mac_array);

extern void eth_multicast_accept_multicast(void);
extern void eth_multicast_promiscuous(void);
extern void eth_set_hast_multicast(long hash_table);
extern void eth_set_addressing(int eim_addr);
extern void eth_set_filter(int cpu_fltr);
extern void eth_turn_on_ethernet(struct net_device * dev);
extern void eth_turn_off_ethernet(struct net_device * dev);
extern void eth_get_received_packet_len(int *numbytes);
extern void eth_get_received_packet_data(unsigned char *buf, int *numbytes);
extern void eth_send_packet(unsigned char *buf, int numbytes, struct net_device * dev);
extern void enable_link_interrupt(void);
extern void eth_reset_enet(struct net_device * dev);
extern void eth_5471_link_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
extern unsigned long eth_read_enet_sys_err_reg(void);
extern void eth_free_received_packet(void);
#endif /* _ETH_C5471_H_ */
