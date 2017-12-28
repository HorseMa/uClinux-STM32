/*
 *
 * Filename: clocks.h 
 *
 * Description: Header file for the clocks.
 *
 * Copyright(c) Cirrus Logic Corporation 2003, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <asm/hardware.h>
#ifndef _H_CLOCKS
#define _H_CLOCKS

//
// How this file works:
// Choose your FCLOCK value this causes FDIV, HDIV, PDIV values to be set
//
// The Real FDIV, HDIV, and PDIV are set according to the following table.
// Processor runs at (PLL1 speed/PLL Divide specified by FDIV)
// AHB runs at (PLL1 speed/PLL Divide specified by HDIV)
// APB runs at AHB/(2^PDIV)
// ------------------------------------------------------------------
// HDIV   PLL Divide    FDIV    PLL Divide 
// 0      1             0       1
// 1      2             1       2
// 2      4             2       4
// 3      5             3       8
// 4      6             4       16
// 5      8
// 6      16
// 7      32
//
//
#ifdef CONFIG_ARCH_EP9301
#define FCLOCK 166
#endif

#if defined(CONFIG_ARCH_EP9312) || defined(CONFIG_ARCH_EP9315)
#define FCLOCK 200
#endif

/*
 * PLL1 Clock =442  Mhz
 *
 * FClock    = 221  Mhz
 * HCLock    = 73   Mhz
 * PClock    = 46   Mhz
 */
#if (FCLOCK == 221)
#define FDIV                        1
#define HDIV                        5
#define PDIV                        1
#define PRE_CLKSET1_VALUE           0x0080b3b6
#define PLL1_CLOCK                  442368000
#endif /* (FCLOCK == 221) */

/*
 * PLL1 Clock =400  Mhz
 *
 * FClock    = 200  Mhz
 * HCLock    = 100  Mhz
 * PClock    = 50   Mhz
 *
 */
#if (FCLOCK == 200)
#define FDIV                        1
#define HDIV                        2
#define PDIV                        1
#define PRE_CLKSET1_VALUE           0x0080a3d7
#define PLL1_CLOCK                  399974400
#endif /* (FCLOCK == 200) */

/*
 * PLL1 Clock =368  Mhz
 *
 * FClock    = 184  Mhz
 * HCLock    = 73   Mhz
 * PClock    = 46   Mhz             
 *
 */
#if (FCLOCK == 183)
#define FDIV                        1
#define HDIV                        4
#define PDIV                        1
#define PRE_CLKSET1_VALUE           0x0080ab15
#define PLL1_CLOCK                  368640000
#endif /* (FCLOCK == 183) */

/*
 * PLL1 Clock =332  Mhz
 *
 * FClock    = 166  Mhz
 * HCLock    = 66   Mhz
 * PClock    = 33   Mhz             
 *
 */
#if (FCLOCK == 166)
#define FDIV                        1
#define HDIV                        3
#define PDIV                        1
#define PRE_CLKSET1_VALUE           0x0080fa5a
#define PLL1_CLOCK                  332049067
#endif /* (FCLOCK == 166) */


#define CLKSET1_VALUE   ( PRE_CLKSET1_VALUE | \
                        ( PDIV << SYSCON_CLKSET1_PCLK_DIV_SHIFT ) | \
                        ( HDIV << SYSCON_CLKSET1_HCLK_DIV_SHIFT ) | \
                        ( FDIV << SYSCON_CLKSET1_FCLK_DIV_SHIFT ) )



/*
 * Value for the PLL 2 register.
 */ 
#define CLKSET2_VALUE           0x300dc317
#define PLL2_CLOCK              192000000

#endif /*  _H_CLOCKS  */
