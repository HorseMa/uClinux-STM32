/*=============================================================================
 *  FILE:           regs_ac97.h
 *
 *  DESCRIPTION:    Ac'97 Register Definition
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
 *=============================================================================
 */
#ifndef _REGS_AC97_H_
#define _REGS_AC97_H_

//-----------------------------------------------------------------------------
// Bit definitionses
//-----------------------------------------------------------------------------
#define AC97ISR_RIS                     8
#define AC97ISR_TIS                     4
#define AC97ISR_RTIS                    2
#define AC97ISR_TCIS                    1

#define AC97RGIS_SLOT1TXCOMPLETE     0x01
#define AC97RGIS_SLOT2RXVALID        0x02
#define AC97RGIS_GPIOTXCOMPLETE      0x04
#define AC97RGIS_GPIOINTRX           0x08
#define AC97RGIS_RWIS                0x10
#define AC97RGIS_CODECREADY          0x20
#define AC97RGIS_SLOT2TXCOMPLETE     0x40

#define AC97SR_RXFE                 0x0001
#define AC97SR_TXFE                 0x0002      
#define AC97SR_RXFF                 0x0004
#define AC97SR_TXFF                 0x0008
#define AC97SR_TXBUSY               0x0010
#define AC97SR_RXOE                 0x0020
#define AC97SR_TXUE                 0x0040

#define AC97GSR_IFE                     0x1
#define AC97GSR_LOOP                    0x2
#define AC97GSR_OVERRIDECODECREADY      0x4

#define AC97RESET_TIMEDRESET            0x1
#define AC97RESET_FORCEDRESET           0x2
#define AC97RESET_EFORCER               0x4

#define AC97RXCR_REN                    0x1

#define AC97TXCR_TEN                    0x1


//****************************************************************************
//
// The Ac97 Codec registers, accessable through the Ac-link.
// These are not controller registers and are not memory mapped.
// Includes registers specific to CS4202 (Beavis).
//
//****************************************************************************
#define AC97_REG_OFFSET_MASK                0x0000007E

#define AC97_00_RESET                          0x00000000
#define AC97_02_MASTER_VOL                     0x00000002
#define AC97_04_HEADPHONE_VOL                  0x00000004
#define AC97_06_MONO_VOL                       0x00000006
#define AC97_08_TONE                           0x00000008
#define AC97_0A_PC_BEEP_VOL                    0x0000000A
#define AC97_0C_PHONE_VOL                      0x0000000C
#define AC97_0E_MIC_VOL                        0x0000000E
#define AC97_10_LINE_IN_VOL                    0x00000010
#define AC97_12_CD_VOL                         0x00000012
#define AC97_14_VIDEO_VOL                      0x00000014
#define AC97_16_AUX_VOL                        0x00000016
#define AC97_18_PCM_OUT_VOL                    0x00000018
#define AC97_1A_RECORD_SELECT                  0x0000001A
#define AC97_1C_RECORD_GAIN                    0x0000001C
#define AC97_1E_RESERVED_1E                    0x0000001E
#define AC97_20_GENERAL_PURPOSE                0x00000020
#define AC97_22_3D_CONTROL                     0x00000022
#define AC97_24_MODEM_RATE                     0x00000024
#define AC97_26_POWERDOWN                      0x00000026
#define AC97_28_EXT_AUDIO_ID                   0x00000028
#define AC97_2A_EXT_AUDIO_POWER                0x0000002A
#define AC97_2C_PCM_FRONT_DAC_RATE             0x0000002C
#define AC97_2E_PCM_SURR_DAC_RATE              0x0000002E
#define AC97_30_PCM_LFE_DAC_RATE               0x00000030
#define AC97_32_PCM_LR_ADC_RATE                0x00000032
#define AC97_34_MIC_ADC_RATE                   0x00000034
#define AC97_36_6CH_VOL_C_LFE                  0x00000036
#define AC97_38_6CH_VOL_SURROUND               0x00000038
#define AC97_3A_SPDIF_CONTROL                  0x0000003A
#define AC97_3C_EXT_MODEM_ID                   0x0000003C
#define AC97_3E_EXT_MODEM_POWER                0x0000003E
#define AC97_40_LINE1_CODEC_RATE               0x00000040
#define AC97_42_LINE2_CODEC_RATE               0x00000042
#define AC97_44_HANDSET_CODEC_RATE             0x00000044
#define AC97_46_LINE1_CODEC_LEVEL              0x00000046
#define AC97_48_LINE2_CODEC_LEVEL              0x00000048
#define AC97_4A_HANDSET_CODEC_LEVEL            0x0000004A
#define AC97_4C_GPIO_PIN_CONFIG                0x0000004C
#define AC97_4E_GPIO_PIN_TYPE                  0x0000004E
#define AC97_50_GPIO_PIN_STICKY                0x00000050
#define AC97_52_GPIO_PIN_WAKEUP                0x00000052
#define AC97_54_GPIO_PIN_STATUS                0x00000054
#define AC97_56_RESERVED                       0x00000056
#define AC97_58_RESERVED                       0x00000058
#define AC97_5A_CRYSTAL_REV_N_FAB_ID           0x0000005A
#define AC97_5C_TEST_AND_MISC_CTRL             0x0000005C
#define AC97_5E_AC_MODE                        0x0000005E
#define AC97_60_MISC_CRYSTAL_CONTROL           0x00000060
#define AC97_62_VENDOR_RESERVED                0x00000062
#define AC97_64_DAC_SRC_PHASE_INCR             0x00000064
#define AC97_66_ADC_SRC_PHASE_INCR             0x00000066
#define AC97_68_RESERVED_68                    0x00000068
#define AC97_6A_SERIAL_PORT_CONTROL            0x0000006A
#define AC97_6C_VENDOR_RESERVED                0x0000006C
#define AC97_6E_VENDOR_RESERVED                0x0000006E
#define AC97_70_BDI_CONFIG                     0x00000070
#define AC97_72_BDI_WAKEUP                     0x00000072
#define AC97_74_VENDOR_RESERVED                0x00000074
#define AC97_76_CAL_ADDRESS                    0x00000076
#define AC97_78_CAL_DATA                       0x00000078
#define AC97_7A_VENDOR_RESERVED                0x0000007A
#define AC97_7C_VENDOR_ID1                     0x0000007C
#define AC97_7E_VENDOR_ID2                     0x0000007E


#ifndef __ASSEMBLY__

//
// enum type for use with reg AC97_RECORD_SELECT
//
typedef enum{
    RECORD_MIC          = 0x0000,
    RECORD_CD           = 0x0101,
    RECORD_VIDEO_IN     = 0x0202,
    RECORD_AUX_IN       = 0x0303,
    RECORD_LINE_IN      = 0x0404,
    RECORD_STEREO_MIX   = 0x0505,
    RECORD_MONO_MIX     = 0x0606,
    RECORD_PHONE_IN     = 0x0707
} Ac97RecordSources;

#endif /* __ASSEMBLY__ */

//
// Sample rates supported directly in AC97_PCM_FRONT_DAC_RATE and 
// AC97_PCM_LR_ADC_RATE.
//
#define Ac97_Fs_8000        0x1f40
#define Ac97_Fs_11025       0x2b11
#define Ac97_Fs_16000       0x3e80
#define Ac97_Fs_22050       0x5622
#define Ac97_Fs_32000       0x7d00
#define Ac97_Fs_44100       0xac44
#define Ac97_Fs_48000       0xbb80

//
// RSIZE and TSIZE in AC97RXCR and AC97TXCR
//
#define Ac97_SIZE_20            2
#define Ac97_SIZE_18            1
#define Ac97_SIZE_16            0
#define Ac97_SIZE_12            3

//=============================================================================
//=============================================================================


#endif /* _REGS_AC97_H_ */
