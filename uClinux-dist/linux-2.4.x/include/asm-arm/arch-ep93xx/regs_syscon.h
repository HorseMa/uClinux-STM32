/*=============================================================================
 *
 *  FILE:       	reg_syscon.h
 *
 *  DESCRIPTION:    ep93xx Syscon Register Definition
 *
 *  Copyright Cirrus Logic, 2001-2003
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *=============================================================================
 */
#ifndef _REGS_SYSCON_H_
#define _REGS_SYSCON_H_

//=============================================================================
#ifndef __ASSEMBLY__

#define SysconSetLocked(registername,value)     \
    {                                           \
        outl( 0xAA, SYSCON_SWLOCK);             \
        outl( value, registername);             \
    }

#endif /* Not __ASSEMBLY__ */
//=============================================================================

//-----------------------------------------------------------------------------
// SYSCON_CLKSET1
//-----------------------------------------------------------------------------
#define SYSCON_CLKSET1_PLL1_X2IPD_SHIFT     0
#define SYSCON_CLKSET1_PLL1_X2IPD_MASK      0x0000001f
#define SYSCON_CLKSET1_PLL1_X2FBD2_SHIFT    5
#define SYSCON_CLKSET1_PLL1_X2FBD2_MASK     0x000007e0
#define SYSCON_CLKSET1_PLL1_X1FBD1_SHIFT    11
#define SYSCON_CLKSET1_PLL1_X1FBD1_MASK     0x0000f800
#define SYSCON_CLKSET1_PLL1_PS_SHIFT        16
#define SYSCON_CLKSET1_PLL1_PS_MASK         0x00030000
#define SYSCON_CLKSET1_PCLKDIV_SHIFT        18
#define SYSCON_CLKSET1_PCLKDIV_MASK         0x000c0000
#define SYSCON_CLKSET1_HCLKDIV_SHIFT        20
#define SYSCON_CLKSET1_HCLKDIV_MASK         0x00700000
#define SYSCON_CLKSET1_nBYP1                0x00800000
#define SYSCON_CLKSET1_SMCROM               0x01000000
#define SYSCON_CLKSET1_FCLKDIV_SHIFT        25
#define SYSCON_CLKSET1_FCLKDIV_MASK         0x0e000000

#define SYSCON_CLKSET1_HSEL                 0x00000001
#define SYSCON_CLKSET1_PLL1_EXCLKSEL        0x00000002

#define SYSCON_CLKSET1_PLL1_P_MASK          0x0000007C
#define SYSCON_CLKSET1_PLL1_P_SHIFT         2

#define SYSCON_CLKSET1_PLL1_M1_MASK         0x00000780
#define SYSCON_CLKSET1_PLL1_M1_SHIFT        7
#define SYSCON_CLKSET1_PLL1_M2_MASK         0x0000F800
#define SYSCON_CLKSET1_PLL1_M2_SHIFT        11
#define SYSCON_CLKSET1_PLL1_PS_MASK         0x00030000
#define SYSCON_CLKSET1_PLL1_PS_SHIFT        16
#define SYSCON_CLKSET1_PCLK_DIV_MASK        0x000C0000
#define SYSCON_CLKSET1_PCLK_DIV_SHIFT       18
#define SYSCON_CLKSET1_HCLK_DIV_MASK        0x00700000
#define SYSCON_CLKSET1_HCLK_DIV_SHIFT       20
#define SYSCON_CLKSET1_SMCROM               0x01000000
#define SYSCON_CLKSET1_FCLK_DIV_MASK        0x0E000000
#define SYSCON_CLKSET1_FCLK_DIV_SHIFT       25

#define SYSCON_CLKSET2_PLL2_EN              0x00000001
#define SYSCON_CLKSET2_PLL2EXCLKSEL         0x00000002
#define SYSCON_CLKSET2_PLL2_P_MASK          0x0000007C
#define SYSCON_CLKSET2_PLL2_P_SHIFT         2
#define SYSCON_CLKSET2_PLL2_M2_MASK         0x00000F80
#define SYSCON_CLKSET2_PLL2_M2_SHIFT        7
#define SYSCON_CLKSET2_PLL2_M1_MASK         0x0001F000
#define SYSCON_CLKSET2_PLL2_M1              12
#define SYSCON_CLKSET2_PLL2_PS_MASK         0x000C0000
#define SYSCON_CLKSET2_PLL2_PS_SHIFT        18
#define SYSCON_CLKSET2_USBDIV_MASK          0xF0000000
#define SYSCON_CLKSET2_USBDIV_SHIFT         28

//-----------------------------------------------------------------------------
// DEV_CFG Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_DEVCFG_SHena            0x00000001
#define SYSCON_DEVCFG_KEYS             0x00000002
#define SYSCON_DEVCFG_ADCPD            0x00000004
#define SYSCON_DEVCFG_RAS              0x00000008
#define SYSCON_DEVCFG_RASonP3          0x00000010
#define SYSCON_DEVCFG_TTIC             0x00000020
#define SYSCON_DEVCFG_I2SonAC97        0x00000040
#define SYSCON_DEVCFG_I2SonSSP         0x00000080
#define SYSCON_DEVCFG_EonIDE           0x00000100
#define SYSCON_DEVCFG_PonG             0x00000200
#define SYSCON_DEVCFG_GonIDE           0x00000400
#define SYSCON_DEVCFG_HonIDE           0x00000800
#define SYSCON_DEVCFG_HC1CEN           0x00001000
#define SYSCON_DEVCFG_HC1IN            0x00002000
#define SYSCON_DEVCFG_HC3CEN           0x00004000
#define SYSCON_DEVCFG_HC3IN            0x00008000
#define SYSCON_DEVCFG_TIN              0x00020000
#define SYSCON_DEVCFG_U1EN             0x00040000
#define SYSCON_DEVCFG_EXVC             0x00080000
#define SYSCON_DEVCFG_U2EN             0x00100000
#define SYSCON_DEVCFG_A1onG            0x00200000
#define SYSCON_DEVCFG_A2onG            0x00400000
#define SYSCON_DEVCFG_CPENA            0x00800000
#define SYSCON_DEVCFG_U3EN             0x01000000
#define SYSCON_DEVCFG_MonG             0x02000000
#define SYSCON_DEVCFG_TonG             0x04000000
#define SYSCON_DEVCFG_GonK             0x08000000
#define SYSCON_DEVCFG_IonU2            0x10000000
#define SYSCON_DEVCFG_D0onG            0x20000000
#define SYSCON_DEVCFG_D1onG            0x40000000
#define SYSCON_DEVCFG_SWRST            0x80000000

//-----------------------------------------------------------------------------
// VIDDIV Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_VIDDIV_VDIV_MASK         0x0000007f              
#define SYSCON_VIDDIV_VDIV_SHIFT        0
#define SYSCON_VIDDIV_PDIV_MASK         0x00000300
#define SYSCON_VIDDIV_PDIV_SHIFT        8
#define SYSCON_VIDDIV_PSEL              0x00002000
#define SYSCON_VIDDIV_ESEL              0x00004000
#define SYSCON_VIDDIV_VENA              0x00008000

//-----------------------------------------------------------------------------
// MIRDIV Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_MIRDIV_MDIV_MASK         0x0000003f
#define SYSCON_MIRDIV_MDIV_SHIFT        0
#define SYSCON_MIRDIV_PDIV_MASK         0x00000300
#define SYSCON_MIRDIV_PDIV_SHIFT        8
#define SYSCON_MIRDIV_PSEL              0x00002000              
#define SYSCON_MIRDIV_ESEL              0x00004000
#define SYSCON_MIRDIV_MENA              0x00008000

//-----------------------------------------------------------------------------
// I2SDIV Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_I2SDIV_MDIV_MASK         0x0000007f
#define SYSCON_I2SDIV_MDIV_SHIFT        0
#define SYSCON_I2SDIV_PDIV_MASK         0x00000300
#define SYSCON_I2SDIV_PDIV_SHIFT        8
#define SYSCON_I2SDIV_PSEL              0x00002000
#define SYSCON_I2SDIV_ESEL              0x00004000
#define SYSCON_I2SDIV_MENA              0x00008000
#define SYSCON_I2SDIV_SDIV              0x00010000
#define SYSCON_I2SDIV_LRDIV_MASK        0x00060000
#define SYSCON_I2SDIV_LRDIV_SHIFT       17
#define SYSCON_I2SDIV_SPOL              0x00080000
#define SYSCON_I2SDIV_DROP              0x00100000
#define SYSCON_I2SDIV_ORIDE             0x20000000
#define SYSCON_I2SDIV_SLAVE             0x40000000
#define SYSCON_I2SDIV_SENA              0x80000000

#define SYSCON_I2SDIV_PDIV_OFF          0x00000000
#define SYSCON_I2SDIV_PDIV_2            0x00000100
#define SYSCON_I2SDIV_PDIV_25           0x00000200
#define SYSCON_I2SDIV_PDIV_3            0x00000300

#define SYSCON_I2SDIV_LRDIV_32          0x00000000
#define SYSCON_I2SDIV_LRDIV_64          0x00020000
#define SYSCON_I2SDIV_LRDIV_128         0x00040000

//-----------------------------------------------------------------------------
// KTDIV Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_KTDIV_KDIV               0x00000001
#define SYSCON_KTDIV_KEN                0x00008000
#define SYSCON_KTDIV_ADIV               0x00010000
#define SYSCON_KTDIV_TSEN               0x80000000

//-----------------------------------------------------------------------------
// CHIPID Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_CHIPID_ID_MASK           0x0000ffff
#define SYSCON_CHIPID_ID_SHIFT          0
#define SYSCON_CHIPID_PKID              0x00010000
#define SYSCON_CHIPID_BND               0x00040000
#define SYSCON_CHIPID_FAB_MASK          0x0e000000
#define SYSCON_CHIPID_FAB_SHIFT         25
#define SYSCON_CHIPID_REV_MASK          0xf0000000
#define SYSCON_CHIPID_REV_SHIFT         28

//-----------------------------------------------------------------------------
// TESTCR Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_TESTCR_TMODE_MASK        0x000000ff
#define SYSCON_TESTCR_TMODE_SHIFT       0
#define SYSCON_TESTCR_BONDO             0x00000100
#define SYSCON_TESTCR_PACKO             0x00000800
#define SYSCON_TESTCR_ETOM              0x00002000
#define SYSCON_TESTCR_TOM               0x00004000
#define SYSCON_TESTCR_OVR               0x00008000
#define SYSCON_TESTCR_TonIDE            0x00010000
#define SYSCON_TESTCR_RonG              0x00020000

//-----------------------------------------------------------------------------
// SYSCFG Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_SYSCFG_LCSn1             0x00000001
#define SYSCON_SYSCFG_LCSn2             0x00000002
#define SYSCON_SYSCFG_LCSn3             0x00000004
#define SYSCON_SYSCFG_LEECK             0x00000008
#define SYSCON_SYSCFG_LEEDA             0x00000010
#define SYSCON_SYSCFG_LASDO             0x00000020
#define SYSCON_SYSCFG_LCSn6             0x00000040
#define SYSCON_SYSCFG_LCSn7             0x00000080
#define SYSCON_SYSCFG_SBOOT             0x00000100
#define SYSCON_SYSCFG_FAB_MASK          0x0e000000
#define SYSCON_SYSCFG_FAB_SHIFT         25
#define SYSCON_SYSCFG_REV_MASK          0xf0000000
#define SYSCON_SYSCFG_REV_SHIFT         28


//-----------------------------------------------------------------------------
// PWRSR Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_PWRSR_CHIPMAN_MASK       0xFF000000
#define SYSCON_PWRSR_CHIPMAN_SHIFT      24
#define SYSCON_PWRSR_CHIPID_MASK        0x00FF0000
#define SYSCON_PWRSR_CHIPID_SHIFT       16
#define SYSCON_PWRSR_WDTFLG             0x00008000
#define SYSCON_PWRSR_CLDFLG             0x00002000
#define SYSCON_PWRSR_TEST_RESET         0x00001000
#define SYSCON_PWRSR_RSTFLG             0x00000800
#define SYSCON_PWRSR_SWRESET            0x00000400
#define SYSCON_PWRSR_PLL2_LOCKREG       0x00000200
#define SYSCON_PWRSR_PLL2_LOCK          0x00000100
#define SYSCON_PWRSR_PLL1_LOCKREG       0x00000080    
#define SYSCON_PWRSR_PLL1_LOCK          0x00000040    
#define SYSCON_PWRSR_RTCDIV             0x0000003F

//-----------------------------------------------------------------------------
// PWRCNT Register Defines
//-----------------------------------------------------------------------------
#define SYSCON_PWRCNT_FIREN             0x80000000
#define SYSCON_PWRCNT_UARTBAUD          0x20000000
#define SYSCON_PWRCNT_USHEN             0x10000000
#define SYSCON_PWRCNT_DMA_M2MCH1        0x08000000
#define SYSCON_PWRCNT_DMA_M2MCH0        0x04000000
#define SYSCON_PWRCNT_DMA_M2PCH8        0x02000000
#define SYSCON_PWRCNT_DMA_M2PCH9        0x01000000
#define SYSCON_PWRCNT_DMA_M2PCH6        0x00800000
#define SYSCON_PWRCNT_DMA_M2PCH7        0x00400000
#define SYSCON_PWRCNT_DMA_M2PCH4        0x00200000
#define SYSCON_PWRCNT_DMA_M2PCH5        0x00100000
#define SYSCON_PWRCNT_DMA_M2PCH2        0x00080000
#define SYSCON_PWRCNT_DMA_M2PCH3        0x00040000
#define SYSCON_PWRCNT_DMA_M2PCH0        0x00020000
#define SYSCON_PWRCNT_DMA_M2PCH1        0x00010000

//-----------------------------------------------------------------------------
// BMAR Register Defines
//-----------------------------------------------------------------------------
#define BMAR_PRIORD_00              0x00000000
#define BMAR_PRIORD_01              0x00000001
#define BMAR_PRIORD_02              0x00000002
#define BMAR_PRIORD_03              0x00000003
#define BMAR_PRI_CORE               0x00000008
#define BMAR_DMA_ENIRQ              0x00000010
#define BMAR_DMA_ENFIQ              0x00000020
#define BMAR_USB_ENIRQ              0x00000040
#define BMAR_USB_ENFIQ              0x00000080
#define BMAR_MAC_ENIRQ              0x00000100
#define BMAR_MAC_ENFIQ              0x00000200
#define BMAR_GRAPHICS_ENIRQ         0x00000400
#define BMAR_GRAPHICS_ENFIQ         0x00000800


#endif // _REGS_SYSCON_H_
