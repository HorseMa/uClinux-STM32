/*******************************************************************************
 *
 * Copyright (c) 2003 Cypress Semiconductor
 *
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
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

/*********************************************************/
/* FILE        : 63700.H                                 */
/*********************************************************/
/* DATE        : 06/04/03                                */
/* VERSION     : 01.00                                   */
/*                                                       */
/* DESCRIPTION : This file contains the register and     */
/*               field definitions for the CY7C63700.    */
/*				 and CY7C67200							 */
/*                                                       */
/* NOTICE      : This file is provided as-is for         */
/*               reference purposes only. No warranty is */
/*               made as to functionality or suitability */
/*               for any particular purpose or           */
/*               application.                            */
/*                                                       */
/*********************************************************/

#ifndef __CY7C67200_300_h_
#define __CY7C67200_300_h_

/*********************************************************/
/* REGISTER/FIELD NAMING CONVENTIONS                     */
/*********************************************************/
/*                                                       */
/* Fields can be considered either:                      */
/* (a) BOOLEAN (1 or 0, On or Off, True or False) or     */
/* (b) BINARY (Numeric value)                            */
/*                                                       */
/* Multiple-bit fields contain numeric values only.      */
/*                                                       */
/* Boolean fields are identified by the _EN suffix?      */
/*                                                       */
/* Binary fields are defined by the field name. In       */
/* addition, all legal values for the binary field are   */
/* identified.                                           */
/*                                                       */
/* Either ALL register names should include REG as part  */
/* of the label or NO register names should include REG  */
/* as part of the label.                                 */
/*                                                       */
/* Certain nomenclature is applied universally within    */
/* this file. Commonly applied abbreviations include:    */
/*                                                       */
/*  EN      Enable                                       */
/*  DIS     Disable                                      */
/*  SEL     Select                                       */
/*  FLG     Flag                                         */
/*  STB     Strobe                                       */
/*                                                       */
/*  ADDR    Address                                      */
/*  CTL     Control                                      */
/*  CNTRL   Control                                      */
/*  CFG     Config                                       */
/*  RST     Reset                                        */
/*  BFR     Buffer                                       */
/*  REG     Register                                     */
/*  SIE     Serial Interface Engine                      */
/*  DEV     Device                                       */
/*  HOST    Host                                         */
/*  EP      Endpoint                                     */
/*  IRQ     Interrupt                                    */
/*  BKPT    Breakpoint                                   */
/*  STAT    Status                                       */
/*  CNT     Count                                        */
/*  CTR     Counter                                      */
/*  TMR     Timer                                        */
/*  MAX     Maximum                                      */
/*  MIN     Minimum                                      */
/*  POL     Polarity                                     */
/*  BLK     Block                                        */
/*  WDT     Watchdog Timer                               */
/*  RX      Receive                                      */
/*  RXD     Received                                     */
/*  TX      Transmit                                     */
/*  TXD     Transmitted                                  */
/*  ACK     Acknowledge                                  */
/*  ACKD    Acknowledged                                 */
/*  MBX     Mailbox                                      */
/*  CLR     Clear                                        */
/*  bm      Bit Mask  (prefix)                           */
/*  XRAM    External RAM                                 */
/*                                                       */
/*********************************************************/

/*********************************************************/
/* REGISTER SUMMARY                                      */
/*********************************************************/
/*                                                       */
/* PROC_FLAGS_REG                                        */
/* REG_BANK_REG                                          */
/* HW_REV_REG                                            */
/* IRQ_EN_REG                                            */
/* CPU_SPEED_REG                                         */
/* POWER_CTL_REG                                         */
/* BKPT_REG                                              */
/* USB_DIAG_REG                                          */
/* MEM_DIAG_REG                                          */
/*                                                       */
/* PGn_MAP_REG                                           */
/* DRAM_CTL_REG                                          */
/* XMEM_CTL_REG                                          */
/*                                                       */
/* WDT_REG                                               */
/* TMRn_REG                                              */
/*                                                       */
/* USBn_CTL_REG                                          */
/*                                                       */
/* HOSTn_IRQ_EN_REG                                      */
/* HOSTn_STAT_REG                                        */
/* HOSTn_CTL_REG                                         */
/* HOSTn_ADDR_REG                                        */
/* HOSTn_CNT_REG                                         */
/* HOSTn_PID_REG                                         */
/* HOSTn_EP_STAT_REG                                     */
/* HOSTn_DEV_ADDR_REG                                    */
/* HOSTn_CTR_REG                                         */
/* HOSTn_SOF_EOP_CNT_REG                                 */
/* HOSTn_SOF_EOP_CTR_REG                                 */
/* HOSTn_FRAME_REG                                       */
/*                                                       */
/* DEVn_IRQ_EN_REG                                       */
/* DEVn_STAT_REG                                         */
/* DEVn_ADDR_REG                                         */
/* DEVn_FRAME_REG                                        */
/* DEVn_EPn_CTL_REG                                      */
/* DEVn_EPn_ADDR_REG                                     */
/* DEVn_EPn_CNT_REG                                      */
/* DEVn_EPn_STAT_REG                                     */
/* DEVn_EPn_CTR_REG                                      */
/*                                                       */
/* OTG_CTL_REG                                           */
/*                                                       */
/* GPIO_CTL_REG                                          */
/* GPIOn_OUT_DATA_REG                                    */
/* GPIOn_IN_DATA_REG                                     */
/* GPIOn_DIR_REG                                         */
/*                                                       */
/* IDE_MODE_REG                                          */
/* IDE_START_ADDR_REG                                    */
/* IDE_STOP_ADDR_REG                                     */
/* IDE_CTL_REG                                           */
/* IDE_PIO_DATA_REG                                      */
/* IDE_PIO_ERR_REG                                       */
/* IDE_PIO_SECT_CNT_REG                                  */
/* IDE_PIO_SECT_NUM_REG                                  */
/* IDE_PIO_CYL_LO_REG                                    */
/* IDE_PIO_CYL_HI_REG                                    */
/* IDE_PIO_DEV_HD_REG                                    */
/* IDE_PIO_CMD_REG                                       */
/* IDE_PIO_DEV_CTL_REG                                   */
/*                                                       */
/* HSS_CTL_REG                                           */
/* HSS_BAUD_REG                                          */
/* HSS_TX_GAP_REG                                        */
/* HSS_DATA_REG                                          */
/* HSS_RX_ADDR_REG                                       */
/* HSS_RX_CTR_REG                                        */
/* HSS_TX_ADDR_REG                                       */
/* HSS_TX_CTR_REG                                        */
/*                                                       */
/* SPI_CFG_REG                                           */
/* SPI_CTL_REG                                           */
/* SPI_IRQ_EN_REG                                        */
/* SPI_STAT_REG                                          */
/* SPI_IRQ_CLR_REG                                       */
/* SPI_CRC_CTL_REG                                       */
/* SPI_CRC_VALUE_REG                                     */
/* SPI_DATA_REG                                          */
/* SPI_TX_ADDR_REG                                       */
/* SPI_TX_CNT_REG                                        */
/* SPI_RX_ADDR_REG                                       */
/* SPI_RX_CNT_REG                                        */
/*                                                       */
/* UART_CTL_REG                                          */
/* UART_STAT_REG                                         */
/* UART_DATA_REG                                         */
/*                                                       */
/* PWM_CTL_REG                                           */
/* PWM_MAX_CNT_REG                                       */
/* PWMn_START_REG                                        */
/* PWMn_STOP_REG                                         */
/* PWM_CYCLE_CNT_REG                                     */
/*                                                       */
/* SIEnMSG_REG                                           */
/* HPI_MBX_REG                                           */
/* HPI_BKPT_REG                                          */
/* IRQ_ROUTING_REG                                       */
/*                                                       */
/* HPI_DATA_PORT                                         */
/* HPI_ADDR_PORT                                         */
/* HPI_MBX_PORT                                          */
/* HPI_STAT_PORT                                         */
/*                                                       */
/*********************************************************/

/*********************************************************/
/*********************************************************/
/* CPU REGISTERS                                         */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* CPU FLAGS REGISTER [R]                                */
/*********************************************************/
#define CPU_FLAGS_REG                                        0xC000 /* CPU Flags Register [R] */

/* FIELDS */

#define GLOBAL_IRQ_EN                                        0x0010 /* Global Interrupt Enable */
#define NEG_FLG                                              0x0008 /* Negative Sign Flag */
#define OVF_FLG                                              0x0004 /* Overflow Flag */
#define CARRY_FLG                                            0x0002 /* Carry/Borrow Flag */
#define ZER0_FLG                                             0x0001 /* Zero Flag */

/*********************************************************/
/* BANK REGISTER [R/W]                                   */
/*********************************************************/

#define BANK_REG                                             0xC002 /* Bank Register [R/W] */
#define BANK                                                 0xFFE0 /* Bank */

/*********************************************************/
/* HARDWARE REVISION REGISTER [R]                        */
/*********************************************************/
/* First Silicon Revision is 0x0101. Revision number     */
/* will be incremented by one for each revision change.  */
/*********************************************************/

#define HW_REV_REG                                           0xC004 /* Hardware Revision Register [R] */

/*********************************************************/
/* INTERRUPT ENABLE REGISTER [R/W]                       */
/*********************************************************/

#define IRQ_EN_REG                                           0xC00E /* Interrupt Enable Register [R/W] */

/* FIELDS */

#define OTG_IRQ_EN                                           0x1000 /* OTG Interrupt Enable */
#define SPI_IRQ_EN                                           0x0800 /* SPI Interrupt Enable */
#define HOST2_IRQ_EN                                         0x0200 /* Host 2 Interrupt Enable */
#define DEV2_IRQ_EN                                          0x0200 /* Device 2 Interrupt Enable */
#define HOST1_IRQ_EN                                         0x0100 /* Host 1 Interrupt Enable */
#define DEV1_IRQ_EN                                          0x0100 /* Device 1 Interrupt Enable */
#define HSS_IRQ_EN                                           0x0080 /* HSS Interrupt Enable */
#define IN_MBX_IRQ_EN                                        0x0040 /* In Mailbox Interrupt Enable */
#define OUT_MBX_IRQ_EN                                       0x0020 /* Out Mailbox Interrupt Enable */
#define UART_IRQ_EN                                          0x0008 /* UART Interrupt Enable */
#define GPIO_IRQ_EN                                          0x0004 /* GPIO Interrupt Enable */
#define TMR1_IRQ_EN                                          0x0002 /* Timer 1 Interrupt Enable */
#define TMR0_IRQ_EN                                          0x0001 /* Timer 0 Interrupt Enable */

/*********************************************************/
/* CPU SPEED REGISTER [R/W]                              */
/*********************************************************/

#define CPU_SPEED_REG                                        0xC008 /* CPU Speed Register [R/W] */

/* CPU SPEED REGISTER FIELDS 
**
** The Speed field in the CPU Speed Register provides a mechanism to
** divide the external clock signal down to operate the CPU at a lower 
** clock speed (presumedly for lower-power operation). The value loaded 
** into this field is a divisor and is calculated as (n+1). For instance, 
** if 3 is loaded into the field, the resulting CPU speed will be PCLK/4.
*/

#define CPU_SPEED                                            0x000F /* CPU Speed */

/*********************************************************/
/* POWER CONTROL REGISTER [R/W]                          */
/*********************************************************/

#define POWER_CTL_REG                                        0xC00A /* Power Control Register [R/W] */

/* FIELDS */

#define HOST2B_WAKE_EN                                       0x8000 /* Host 2B Wake Enable */
#define DEV2B_WAKE_EN                                        0x8000 /* Device 2B Wake Enable */
#define HOST2A_WAKE_EN                                       0x4000 /* Host 2A Wake Enable */
#define DEV2A_WAKE_EN                                        0x4000 /* Device 2A Wake Enable */
#define HOST1B_WAKE_EN                                       0x2000 /* Host 1B Wake Enable */
#define DEV1B_WAKE_EN                                        0x2000 /* Device 1B Wake Enable */
#define HOST1A_WAKE_EN                                       0x1000 /* Host 1A Wake Enable */
#define DEV1A_WAKE_EN                                        0x1000 /* Device 1A Wake Enable */
#define OTG_WAKE_EN                                          0x0800 /* OTG Wake Enable  */
#define HSS_WAKE_EN                                          0x0200 /* HSS Wake Enable  */
#define SPI_WAKE_EN                                          0x0100 /* SPI Wake Enable  */
#define HPI_WAKE_EN                                          0x0080 /* HPI Wake Enable  */
#define GPIO_WAKE_EN                                         0x0010 /* GPIO Wake Enable  */
#define BOOST_OK_FLG                                         0x0004 /* Boost 3V OK Flag */
#define SLEEP_EN                                             0x0002 /* Sleep Enable */
#define HALT_EN                                              0x0001 /* Halt Enable */

/*********************************************************/
/* BREAKPOINT REGISTER [R/W]                             */
/*********************************************************/

#define BKPT_REG                                             0xC014 /* Breakpoint Register [R/W] */

/*********************************************************/
/* USB DIAGNOSTIC REGISTER [W]                           */
/*********************************************************/

#define USB_DIAG_REG                                         0xC03C /* USB Diagnostic Register [R/W] */

/* FIELDS */

#define c2B_DIAG_EN                                          0x8000 /* Port 2B Diagnostic Enable */
#define c2A_DIAG_EN                                          0x4000 /* Port 2A Diagnostic Enable */
#define c1B_DIAG_EN                                          0x2000 /* Port 1B Diagnostic Enable */
#define c1A_DIAG_EN                                          0x1000 /* Port 1A Diagnostic Enable */
#define PULLDOWN_EN                                          0x0040 /* Pull-down resistors enable */
#define LS_PULLUP_EN                                         0x0020 /* Low-speed pull-up resistor enable */
#define FS_PULLUP_EN                                         0x0010 /* Full-speed pull-up resistor enable */
#define FORCE_SEL                                            0x0007 /* Control D+/- lines */

/* FORCE FIELD VALUES */

#define ASSERT_SE0                                           0x0004 /* Assert SE0 on selected ports */
#define TOGGLE_JK                                            0x0002 /* Toggle JK state on selected ports */
#define ASSERT_J                                             0x0001 /* Assert J state on selected ports */
#define ASSERT_K                                             0x0000 /* Assert K state on selected ports */

/*********************************************************/
/* MEMORY DIAGNOSTIC REGISTER [W]                        */
/*********************************************************/

#define MEM_DIAG_REG                                         0xC03E /* Memory Diagnostic Register [W] */

/* FIELDS */

#define MEM_ARB_SEL                                          0x0700 /* Memory Arbitration */
#define MONITOR_EN                                           0x0001 /* Monitor Enable (Echoes internal address bus externally) */

/* MEMORY ARBITRATION SELECT FIELD VALUES */
#define MEM_ARB_7                                            0x0700 /* Number of dead cycles out of 8 possible */
#define MEM_ARB_6                                            0x0600 /* Should not use any cycle >= 6 */
#define MEM_ARB_5                                            0x0500 /*  */
#define MEM_ARB_4                                            0x0400 /* */
#define MEM_ARB_3                                            0x0300 /* */
#define MEM_ARB_2                                            0x0200 /* */
#define MEM_ARB_1                                            0x0100 /*  */
#define MEM_ARB_0                                            0x0000 /* Power up default */

/*********************************************************/
/* EXTENDED PAGE n MAP REGISTER [R/W]                    */
/*********************************************************/

#define PG1_MAP_REG                                          0xC018 /* Page 1 Map Register [R/W] */
#define PG2_MAP_REG                                          0xC01A /* Page 2 Map Register [R/W] */

/*********************************************************/
/* EXTERNAL MEMORY CONTROL REGISTER [R/W]                */
/*********************************************************/

#define XMEM_CTL_REG                                         0xC03A /* External Memory Control Register [R/W] */

/* FIELDS */
#define XRAM_MERGE_EN                                        0x2000 /* Overlay XRAMSEL w/ XMEMSEL */
#define XROM_MERGE_EN                                        0x1000 /* Overlay XROMSEL w/ XMEMSEL */
#define XMEM_WIDTH_SEL                                       0x0800 /* External MEM Width Select */
#define XMEM_WAIT_SEL                                        0x0700 /* Number of Extended Memory wait states (0-7) */
#define XROM_WIDTH_SEL                                       0x0080 /* External ROM Width Select */
#define XROM_WAIT_SEL                                        0x0070 /* Number of External ROM wait states (0-7) */
#define XRAM_WIDTH_SEL                                       0x0008 /* External RAM Width Select */
#define XRAM_WAIT_SEL                                        0x0007 /* Number of External RAM wait states (0-7) */

/* XMEM_WIDTH FIELD VALUES */

#define XMEM_8                                               0x0800 /*  */
#define XMEM_16                                              0x0000 /*  */

/* XRAM_WIDTH FIELD VALUES */

#define XROM_8                                               0x0080 /*  */
#define XROM_16                                              0x0000 /*  */

/* XRAM_WIDTH FIELD VALUES */

#define XRAM_8                                               0x0008 /*  */
#define XRAM_16                                              0x0000 /*  */


/*********************************************************/
/* WATCHDOG TIMER REGISTER [R/W]                         */
/*********************************************************/

#define WDT_REG                                              0xC00C /* Watchdog Timer Register [R/W] */

/* FIELDS */

#define WDT_TIMEOUT_FLG                                      0x0020 /* WDT timeout flag */
#define WDT_PERIOD_SEL                                       0x0018 /* WDT period select (options below) */
#define WDT_LOCK_EN                                          0x0004 /* WDT enable */
#define WDT_EN                                               0x0002 /* WDT lock enable */
#define WDT_RST_STB                                          0x0001 /* WDT reset Strobe */

/* WATCHDOG PERIOD FIELD VALUES */

#define WDT_66MS                                             0x0003 /* 66.0 ms */
#define WDT_22MS                                             0x0002 /* 22.0 ms */
#define WDT_5MS                                              0x0001 /* 5.5 ms */
#define WDT_1MS                                              0x0000 /* 1.4 ms */

/*********************************************************/
/* TIMER n REGISTER [R/W]                                */
/*********************************************************/

#define TMR0_REG                                             0xC010 /* Timer 0 Register [R/W] */
#define TMR1_REG                                             0xC012 /* Timer 1 Register [R/W] */

/*********************************************************/
/*********************************************************/
/* USB REGISTERS                                         */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* USB n CONTROL REGISTERS [R/W]                         */
/*********************************************************/

#define USB1_CTL_REG                                         0xC08A /* USB 1 Control Register [R/W] */
#define USB2_CTL_REG                                         0xC0AA /* USB 2 Control Register [R/W] */

/* FIELDS */
#define B_DP_STAT                                            0x8000 /* Port B D+ status */
#define B_DM_STAT                                            0x4000 /* Port B D- status */
#define A_DP_STAT                                            0x2000 /* Port A D+ status */
#define A_DM_STAT                                            0x1000 /* Port A D- status */
#define B_SPEED_SEL                                          0x0800 /* Port B Speed select (See below) */
#define A_SPEED_SEL                                          0x0400 /* Port A Speed select (See below) */
#define MODE_SEL                                             0x0200 /* Mode (See below) */
#define B_RES_EN                                             0x0100 /* Port B Resistors enable */
#define A_RES_EN                                             0x0080 /* Port A Resistors enable */
#define B_FORCE_SEL                                          0x0060 /* Port B Force D+/- state (See below) */
#define A_FORCE_SEL                                          0x0018 /* Port A Force D+/- state (See below) */
#define SUSP_EN                                              0x0004 /* Suspend enable */
#define B_SOF_EOP_EN                                         0x0002 /* Port B SOF/EOP enable */
#define A_SOF_EOP_EN                                         0x0001 /* Port A SOF/EOP enable */

/* MODE FIELD VALUES */

#define HOST_MODE                                            0x0200 /* Host mode */
#define DEVICE_MODE                                          0x0000 /* Device mode */

/* p_SPEED SELECT FIELD VALUES */

#define LOW_SPEED                                            0xFFFF /* Low speed */
#define FULL_SPEED                                           0x0000 /* Full speed */

#define B_SPEED_LOW                                          0x0800
#define B_SPEED_FULL                                         0x0000
#define A_SPEED_LOW                                          0x0400
#define A_SPEED_FULL                                         0x0000

/* FORCEn FIELD VALUES */

#define FORCE_K                                              0x0078 /* Force K state on associated port */
#define FORCE_SE0                                            0x0050 /* Force SE0 state on associated port */
#define FORCE_J                                              0x0028 /* Force J state on associated port */
#define FORCE_NORMAL                                         0x0000 /* Don't force associated port */

#define A_FORCE_K                                            0x0018 /* Force K state on A port */
#define A_FORCE_SE0                                          0x0010 /* Force SE0 state on associated port */
#define A_FORCE_J                                            0x0008 /* Force J state on associated port */
#define A_FORCE_NORMAL                                       0x0000 /* Don't force associated port */

#define B_FORCE_K                                            0x0060 /* Force K state on associated port */
#define B_FORCE_SE0                                          0x0040 /* Force SE0 state on associated port */
#define B_FORCE_J                                            0x0020 /* Force J state on associated port */
#define B_FORCE_NORMAL                                       0x0000 /* Don't force associated port */


/*********************************************************/
/*********************************************************/
/* HOST REGISTERS                                        */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* HOST n INTERRUPT ENABLE REGISTER [R/W]                */
/*********************************************************/

#define HOST1_IRQ_EN_REG                                     0xC08C /* Host 1 Interrupt Enable Register [R/W] */ 
#define HOST2_IRQ_EN_REG                                     0xC0AC /* Host 2 Interrupt Enable Register [R/W] */


/* FIELDS */

#define VBUS_IRQ_EN                                          0x8000 /* VBUS Interrupt Enable (Available on HOST1 only)  */
#define ID_IRQ_EN                                            0x4000 /* ID Interrupt Enable (Available on HOST1 only) */
#define SOF_EOP_IRQ_EN                                       0x0200 /* SOF/EOP Interrupt Enable  */
#define B_WAKE_IRQ_EN                                        0x0080 /* Port B Wake Interrupt Enable  */
#define A_WAKE_IRQ_EN                                        0x0040 /* Port A Wake Interrupt Enable  */
#define B_CHG_IRQ_EN                                         0x0020 /* Port B Connect Change Interrupt Enable  */
#define A_CHG_IRQ_EN                                         0x0010 /* Port A Connect Change Interrupt Enable  */
#define DONE_IRQ_EN                                          0x0001 /* Done Interrupt Enable  */


/*********************************************************/
/* HOST n STATUS REGISTER [R/W]                          */
/*********************************************************/
/* In order to clear status for a particular IRQ bit,    */
/* write a '1' to that bit location.                     */
/*********************************************************/

#define HOST1_STAT_REG                                       0xC090 /* Host 1 Status Register [R/W] */
#define HOST2_STAT_REG                                       0xC0B0 /* Host 2 Status Register [R/W] */


/* FIELDS */

#define VBUS_IRQ_FLG                                         0x8000 /* VBUS Interrupt Request (HOST1 only) */
#define ID_IRQ_FLG                                           0x4000 /* ID Interrupt Request (HOST1 only) */

#define SOF_EOP_IRQ_FLG                                      0x0200 /* SOF/EOP Interrupt Request  */
#define B_WAKE_IRQ_FLG                                       0x0080 /* Port B Wake Interrupt Request  */
#define A_WAKE_IRQ_FLG                                       0x0040 /* Port A Wake Interrupt Request  */
#define B_CHG_IRQ_FLG                                        0x0020 /* Port B Connect Change Interrupt Request  */
#define A_CHG_IRQ_FLG                                        0x0010 /* Port A Connect Change Interrupt Request  */
#define B_SE0_STAT                                           0x0008 /* Port B SE0 status */
#define A_SE0_STAT                                           0x0004 /* Port A SE0 status */
#define DONE_IRQ_FLG                                         0x0001 /* Done Interrupt Request  */

/*********************************************************/
/* HOST n CONTROL REGISTERS [R/W]                        */
/*********************************************************/

#define HOST1_CTL_REG                                        0xC080 /* Host 1 Control Register [R/W] */
#define HOST2_CTL_REG                                        0xC0A0 /* Host 2 Control Register [R/W] */


/* FIELDS */
#define PREAMBLE_EN                                          0x0080 /* Preamble enable */
#define SEQ_SEL                                              0x0040 /* Data Toggle Sequence Bit Select (Write next/read last) */
#define SYNC_EN                                              0x0020 /* (1:Send next packet at SOF/EOP, 0: Send next packet immediately) */
#define ISO_EN                                               0x0010 /* Isochronous enable  */
#define ARM_EN                                               0x0001 /* Arm operation */
#define BSY_FLG                                              0x0001 /* Busy flag */

/*********************************************************/
/* HOST n ADDRESS REGISTERS [R/W]                        */
/*********************************************************/

#define HOST1_ADDR_REG                                       0xC082 /* Host 1 Address Register [R/W] */
#define HOST2_ADDR_REG                                       0xC0A2 /* Host 2 Address Register [R/W] */

/*********************************************************/
/* HOST n COUNT REGISTERS [R/W]                          */
/*********************************************************/

#define HOST1_CNT_REG                                        0xC084 /* Host 1 Count Register [R/W] */
#define HOST2_CNT_REG                                        0xC0A4 /* Host 2 Count Register [R/W] */


/* FIELDS */
#define PORT_SEL                                             0x4000 /* Port Select (1:PortB, 0:PortA) */
#define HOST_CNT                                             0x03FF /* Host Count */

/*********************************************************/
/* HOST n PID REGISTERS [W]                              */
/*********************************************************/

#define HOST1_PID_REG                                        0xC086 /* Host 1 PID Register [W] */
#define HOST2_PID_REG                                        0xC0A6 /* Host 2 PID Register [W] */

/* FIELDS */
#define PID_SEL                                              0x00F0 /* Packet ID (see below) */
#define EP_SEL                                               0x000F /* Endpoint number */

/* PID FIELD VALUES */
#define SETUP_PID                                            0x000D /* SETUP */
#define IN_PID                                               0x0009 /* IN */
#define OUT_PID                                              0x0001 /* OUT */
#define SOF_PID                                              0x0005 /* SOF */
#define PRE_PID                                              0x000C /* PRE */
#define NAK_PID                                              0x000A /* NAK */
#define STALL_PID                                            0x000E /* STALL */
#define DATA0_PID                                            0x0003 /* DATA0 */
#define DATA1_PID                                            0x000B /* DATA1 */

/*********************************************************/
/* OTG-Host Define value                                 */
/*********************************************************/

#define PortA                                               0x0000
#define PortB                                               0x0001
#define PortC                                               0x0002
#define PortD                                               0x0003

#define PID_SETUP                                           0x000D
#define PID_IN                                              0x0009
#define PID_OUT                                             0x0001
#define PID_SOF                                             0x0005
#define PID_PRE                                             0x000C
#define PID_NAK                                             0x000A
#define PID_STALL                                           0x000E
#define PID_DATA0                                           0x0003
#define PID_DATA1                                           0x000B
#define PID_ACK                                             0x0002

/*********************************************************/
/* HOST n ENDPOINT STATUS REGISTERS [R]                  */
/*********************************************************/

#define HOST1_EP_STAT_REG                                    0xC086 /* Host 1 Endpoint Status Register [R] */
#define HOST2_EP_STAT_REG                                    0xC0A6 /* Host 2 Endpoint Status Register [R] */

/* FIELDS */
#define OVERFLOW_FLG                                         0x0800 /* Receive overflow */
#define UNDERFLOW_FLG                                        0x0400 /* Receive underflow */
#define STALL_FLG                                            0x0080 /* Device returned STALL */
#define NAK_FLG                                              0x0040 /* Device returned NAK */
#define LENGTH_EXCEPT_FLG                                    0x0020 /* Overflow or Underflow occured */
#define SEQ_STAT                                             0x0008 /* Data Toggle value */
#define TIMEOUT_FLG                                          0x0004 /* Timeout occurred */
#define ERROR_FLG                                            0x0002 /* Error occurred */
#define ACK_FLG                                              0x0001 /* Transfer ACK'd        */

/*********************************************************/
/* HOST n DEVICE ADDRESS REGISTERS [W]                   */
/*********************************************************/

#define HOST1_DEV_ADDR_REG                                   0xC088 /* Host 1 Device Address Register [W] */
#define HOST2_DEV_ADDR_REG                                   0xC0A8 /* Host 2 Device Address Register [W] */


/* FIELDS */

#define DEV_ADDR                                             0x007F /* Device Address */

/*********************************************************/
/* HOST n COUNT RESULT REGISTERS [R]                     */
/*********************************************************/

#define HOST1_CTR_REG                                        0xC088 /* Host 1 Counter Register [R] */
#define HOST2_CTR_REG                                        0xC0A8 /* Host 2 Counter Register [R] */

/* FIELDS*/

#define HOST_RESULT                                          0x00FF /* Host Count Result */

/*********************************************************/
/* HOST n SOF/EOP COUNT REGISTER [R/W]                   */
/*********************************************************/

#define HOST1_SOF_EOP_CNT_REG                                0xC092 /* Host 1 SOF/EOP Count Register [R/W] */
#define HOST2_SOF_EOP_CNT_REG                                0xC0B2 /* Host 2 SOF/EOP Count Register [R/W] */


/* FIELDS */

#define SOF_EOP_CNT                                          0x3FFF /* SOF/EOP Count */

/*********************************************************/
/* HOST n SOF/EOP COUNTER REGISTER [R]                       */
/*********************************************************/

#define HOST1_SOF_EOP_CTR_REG                                0xC094 /* Host 1 SOF/EOP Counter Register [R] */
#define HOST2_SOF_EOP_CTR_REG                                0xC0B4 /* Host 2 SOF/EOP Counter Register [R] */


/* FIELDS */

#define SOF_EOP_CTR                                          0x3FFF /* SOF/EOP Counter */

/*********************************************************/
/* HOST n FRAME REGISTER [R]                             */
/*********************************************************/

#define HOST1_FRAME_REG                                      0xC096 /* Host 1 Frame Register [R] */
#define HOST2_FRAME_REG                                      0xC0B6 /* Host 2 Frame Register [R] */


/* FIELDS */

#define HOST_FRAME_NUM                                       0x07FF /* Frame */


/*********************************************************/
/*********************************************************/
/* DEVICE REGISTERS                                      */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* DEVICE n PORT SELECT REGISTERS [R/W]                  */
/*********************************************************/

#define DEV1_SEL_REG                                         0xC084 /* Device 1 Port Select Register [R/W] */
#define DEV2_SEL_REG                                         0xC0A4 /* Device 2 Port Select Register [R/W] */

/* FIELDS */

/*********************************************************/
/* DEVICE n INTERRUPT ENABLE REGISTER [R/W]              */
/*********************************************************/

#define DEV1_IRQ_EN_REG                                      0xC08C /* Device 1 Interrupt Enable Register [R/W] */
#define DEV2_IRQ_EN_REG                                      0xC0AC /* Device 2 Interrupt Enable Register [R/W] */

/* FIELDS */

#define SOF_EOP_TMOUT_IRQ_EN                                 0x0800 /* SOF/EOP Timeout Interrupt Enable */
/* Field name defined in Host Enable Register
 * #define DEV_SOF_EOP_IRQ_EN                                0x0200 /# SOF/EOP Interrupt Enable #/
 */
#define RST_IRQ_EN                                           0x0100 /* Reset Interrupt Enable */                    
#define EP7_IRQ_EN                                           0x0080 /* EP7 Interrupt Enable  */
#define EP6_IRQ_EN                                           0x0040 /* EP6 Interrupt Enable  */
#define EP5_IRQ_EN                                           0x0020 /* EP5 Interrupt Enable  */
#define EP4_IRQ_EN                                           0x0010 /* EP4 Interrupt Enable  */
#define EP3_IRQ_EN                                           0x0008 /* EP3 Interrupt Enable  */
#define EP2_IRQ_EN                                           0x0004 /* EP2 Interrupt Enable  */
#define EP1_IRQ_EN                                           0x0002 /* EP1 Interrupt Enable  */
#define EP0_IRQ_EN                                           0x0001 /* EP0 Interrupt Enable  */

/*********************************************************/
/* DEVICE n STATUS REGISTER [R/W]                        */
/*********************************************************/
/* In order to clear status for a particular IRQ bit,    */
/* write a '1' to that bit location.                     */
/*********************************************************/

#define DEV1_STAT_REG                                        0xC090 /* Device 1 Status Register [R/W] */
#define DEV2_STAT_REG                                        0xC0B0 /* Device 2 Status Register [R/W] */

/* FIELDS */

/* Defined in Host Status Register
 * #define VBUS_IRQ_FLG                                      0x8000 /# VBUS Interrupt Request (DEV1 only) #/
 * #define ID_IRQ_FLG                                        0x4000 /# ID Interrupt Request (DEV1 only) #/
 * #define SOF_EOP_IRQ_FLG                                   0x0200 /# SOF/EOP Interrupt Request #/
 */
#define RST_IRQ_FLG                                          0x0100 /* Reset Interrupt Request */
#define EP7_IRQ_FLG                                          0x0080 /* EP7 Interrupt Request */
#define EP6_IRQ_FLG                                          0x0040 /* EP6 Interrupt Request */
#define EP5_IRQ_FLG                                          0x0020 /* EP5 Interrupt Request */
#define EP4_IRQ_FLG                                          0x0010 /* EP4 Interrupt Request */
#define EP3_IRQ_FLG                                          0x0008 /* EP3 Interrupt Request */
#define EP2_IRQ_FLG                                          0x0004 /* EP2 Interrupt Request */
#define EP1_IRQ_FLG                                          0x0002 /* EP1 Interrupt Request */
#define EP0_IRQ_FLG                                          0x0001 /* EP0 Interrupt Request */

/*********************************************************/
/* DEVICE n ADDRESS REGISTERS [R/W]                      */
/*********************************************************/

#define DEV1_ADDR_REG                                        0xC08E /* Device 1 Address Register [R/W] */
#define DEV2_ADDR_REG                                        0xC0AE /* Device 2 Address Register [R/W] */

/* FIELDS */

#define DEV_ADDR_SEL                                         0x007F /* Device Address */

/*********************************************************/
/* DEVICE n FRAME NUMBER REGISTER [R]                    */
/*********************************************************/

#define DEV1_FRAME_REG                                       0xC092 /* Device 1 Frame Register [R] */
#define DEV2_FRAME_REG                                       0xC0B2 /* Device 2 Frame Register [R] */

/* FIELDS */

#define SOF_EOP_TMOUT_FLG                                    0x8000 /* SOF/EOP Timeout occured */
#define SOF_EOP_TMOUT_CNTR                                   0x7000 /* SOF/EOP Timeout Interrupt Counter */
#define DEV_FRAME_STAT                                       0x07FF /* Device Frame */

/*********************************************************/
/* DEVICE n ENDPOINT n CONTROL REGISTERS [R/W]           */
/*********************************************************/

#define DEV1_EP0_CTL_REG                                     0x0200 /* Device 1 Endpoint 0 Control Register [R/W] */
#define DEV1_EP1_CTL_REG                                     0x0210 /* Device 1 Endpoint 1 Control Register [R/W]       */
#define DEV1_EP2_CTL_REG                                     0x0220 /* Device 1 Endpoint 2 Control Register [R/W] */
#define DEV1_EP3_CTL_REG                                     0x0230 /* Device 1 Endpoint 3 Control Register [R/W] */
#define DEV1_EP4_CTL_REG                                     0x0240 /* Device 1 Endpoint 4 Control Register [R/W] */
#define DEV1_EP5_CTL_REG                                     0x0250 /* Device 1 Endpoint 5 Control Register [R/W] */
#define DEV1_EP6_CTL_REG                                     0x0260 /* Device 1 Endpoint 6 Control Register [R/W] */
#define DEV1_EP7_CTL_REG                                     0x0270 /* Device 1 Endpoint 7 Control Register [R/W] */

#define DEV2_EP0_CTL_REG                                     0x0280 /* Device 2 Endpoint 0 Control Register [R/W] */
#define DEV2_EP1_CTL_REG                                     0x0290 /* Device 2 Endpoint 1 Control Register [R/W]       */
#define DEV2_EP2_CTL_REG                                     0x02A0 /* Device 2 Endpoint 2 Control Register [R/W] */
#define DEV2_EP3_CTL_REG                                     0x02B0 /* Device 2 Endpoint 3 Control Register [R/W] */
#define DEV2_EP4_CTL_REG                                     0x02C0 /* Device 2 Endpoint 4 Control Register [R/W] */
#define DEV2_EP5_CTL_REG                                     0x02D0 /* Device 2 Endpoint 5 Control Register [R/W] */
#define DEV2_EP6_CTL_REG                                     0x02E0 /* Device 2 Endpoint 6 Control Register [R/W] */
#define DEV2_EP7_CTL_REG                                     0x02F0 /* Device 2 Endpoint 7 Control Register [R/W] */

#define SIE1_DEV_REQ                                         0x0300 /* SIE1 Default Setup packet Address */
#define SIE2_DEV_REQ                                         0x0308 /* SIE2 Default Setup packet Address */

/* FIELDS */

#define INOUT_IGN_EN                                         0x0080 /* Ignores IN and OUT requests on EP0     */
#define STALL_EN                                             0x0020 /* Endpoint Stall */
#define NAK_INT_EN                                           0x0080 /* NAK Response Interrupt enable  */

/*********************************************************/
/* DEVICE n ENDPOINT n ADDRESS REGISTERS [R/W]           */
/*********************************************************/

#define DEV1_EP0_ADDR_REG                                    0x0202 /* Device 1 Endpoint 0 Address Register [R/W] */
#define DEV1_EP1_ADDR_REG                                    0x0212 /* Device 1 Endpoint 1 Address Register [R/W]      */
#define DEV1_EP2_ADDR_REG                                    0x0222 /* Device 1 Endpoint 2 Address Register [R/W] */
#define DEV1_EP3_ADDR_REG                                    0x0232 /* Device 1 Endpoint 3 Address Register [R/W] */
#define DEV1_EP4_ADDR_REG                                    0x0242 /* Device 1 Endpoint 4 Address Register [R/W] */
#define DEV1_EP5_ADDR_REG                                    0x0252 /* Device 1 Endpoint 5 Address Register [R/W] */
#define DEV1_EP6_ADDR_REG                                    0x0262 /* Device 1 Endpoint 6 Address Register [R/W] */
#define DEV1_EP7_ADDR_REG                                    0x0272 /* Device 1 Endpoint 7 Address Register [R/W] */

#define DEV2_EP0_ADDR_REG                                    0x0282 /* Device 2 Endpoint 0 Address Register [R/W] */
#define DEV2_EP1_ADDR_REG                                    0x0292 /* Device 2 Endpoint 1 Address Register [R/W] */
#define DEV2_EP2_ADDR_REG                                    0x02A2 /* Device 2 Endpoint 2 Address Register [R/W] */
#define DEV2_EP3_ADDR_REG                                    0x02B2 /* Device 2 Endpoint 3 Address Register [R/W] */
#define DEV2_EP4_ADDR_REG                                    0x02C2 /* Device 2 Endpoint 4 Address Register [R/W] */
#define DEV2_EP5_ADDR_REG                                    0x02D2 /* Device 2 Endpoint 5 Address Register [R/W] */
#define DEV2_EP6_ADDR_REG                                    0x02E2 /* Device 2 Endpoint 6 Address Register [R/W] */
#define DEV2_EP7_ADDR_REG                                    0x02F2 /* Device 2 Endpoint 7 Address Register [R/W] */

/*********************************************************/
/* DEVICE n ENDPOINT n COUNT REGISTERS [R/W]             */
/*********************************************************/

#define DEV1_EP0_CNT_REG                                     0x0204 /* Device 1 Endpoint 0 Count Register [R/W] */
#define DEV1_EP1_CNT_REG                                     0x0214 /* Device 1 Endpoint 1 Count Register [R/W]       */
#define DEV1_EP2_CNT_REG                                     0x0224 /* Device 1 Endpoint 2 Count Register [R/W] */
#define DEV1_EP3_CNT_REG                                     0x0234 /* Device 1 Endpoint 3 Count Register [R/W] */
#define DEV1_EP4_CNT_REG                                     0x0244 /* Device 1 Endpoint 4 Count Register [R/W] */
#define DEV1_EP5_CNT_REG                                     0x0254 /* Device 1 Endpoint 5 Count Register [R/W] */
#define DEV1_EP6_CNT_REG                                     0x0264 /* Device 1 Endpoint 6 Count Register [R/W] */
#define DEV1_EP7_CNT_REG                                     0x0274 /* Device 1 Endpoint 7 Count Register [R/W] */

#define DEV2_EP0_CNT_REG                                     0x0284 /* Device 2 Endpoint 0 Count Register [R/W] */
#define DEV2_EP1_CNT_REG                                     0x0294 /* Device 2 Endpoint 1 Count Register [R/W]      */
#define DEV2_EP2_CNT_REG                                     0x02A4 /* Device 2 Endpoint 2 Count Register [R/W] */
#define DEV2_EP3_CNT_REG                                     0x02B4 /* Device 2 Endpoint 3 Count Register [R/W] */
#define DEV2_EP4_CNT_REG                                     0x02C4 /* Device 2 Endpoint 4 Count Register [R/W] */
#define DEV2_EP5_CNT_REG                                     0x02D4 /* Device 2 Endpoint 5 Count Register [R/W] */
#define DEV2_EP6_CNT_REG                                     0x02E4 /* Device 2 Endpoint 6 Count Register [R/W] */
#define DEV2_EP7_CNT_REG                                     0x02F4 /* Device 2 Endpoint 7 Count Register [R/W] */

/* FIELDS */

#define EP_CNT                                               0x03FF /* Endpoint Count */

/*********************************************************/
/* DEVICE n ENDPOINT n STATUS REGISTERS [R/W]            */
/*********************************************************/

#define DEV1_EP0_STAT_REG                                    0x0206 /* Device 1 Endpoint 0 Status Register [R/W] */
#define DEV1_EP1_STAT_REG                                    0x0216 /* Device 1 Endpoint 1 Status Register [R/W]       */
#define DEV1_EP2_STAT_REG                                    0x0226 /* Device 1 Endpoint 2 Status Register [R/W] */
#define DEV1_EP3_STAT_REG                                    0x0236 /* Device 1 Endpoint 3 Status Register [R/W] */
#define DEV1_EP4_STAT_REG                                    0x0246 /* Device 1 Endpoint 4 Status Register [R/W] */
#define DEV1_EP5_STAT_REG                                    0x0256 /* Device 1 Endpoint 5 Status Register [R/W] */
#define DEV1_EP6_STAT_REG                                    0x0266 /* Device 1 Endpoint 6 Status Register [R/W] */
#define DEV1_EP7_STAT_REG                                    0x0276 /* Device 1 Endpoint 7 Status Register [R/W] */

#define DEV2_EP0_STAT_REG                                    0x0286 /* Device 2 Endpoint 0 Status Register [R/W] */
#define DEV2_EP1_STAT_REG                                    0x0296 /* Device 2 Endpoint 1 Status Register [R/W]      */
#define DEV2_EP2_STAT_REG                                    0x02A6 /* Device 2 Endpoint 2 Status Register [R/W] */
#define DEV2_EP3_STAT_REG                                    0x02B6 /* Device 2 Endpoint 3 Status Register [R/W] */
#define DEV2_EP4_STAT_REG                                    0x02C6 /* Device 2 Endpoint 4 Status Register [R/W] */
#define DEV2_EP5_STAT_REG                                    0x02D6 /* Device 2 Endpoint 5 Status Register [R/W] */
#define DEV2_EP6_STAT_REG                                    0x02E6 /* Device 2 Endpoint 6 Status Register [R/W] */
#define DEV2_EP7_STAT_REG                                    0x02F6 /* Device 2 Endpoint 7 Status Register [R/W] */

/* FIELDS */

/* Defined in HOST n ENDPOINT STATUS REGISTERS
 * #define OVERFLOW_FLG                                      0x0800 /# Receive overflow #/
 * #define UNDERFLOW_FLG                                     0x0400 /# Receive underflow #/
 * #define STALL_FLG                                         0x0080 /# Stall sent #/
 * #define NAK_FLG                                           0x0040 /# NAK sent #/
 * #define LENGTH_EXCEPT_FLG                                 0x0020 /# Overflow or Underflow occured #/
 * #define SEQ_STAT                                          0x0008 /# Last Data Toggle Sequence bit sent or received #/
 * #define TIMEOUT_FLG                                       0x0004 /# Last transmission timed out #/
 * #define ERROR_FLG                                         0x0002 /# CRC Error detected in last reception #/
 * #define ACK_FLG                                           0x0001 /# Last transaction ACK'D (sent or received) #/
 */ 
#define OUT_EXCEPTION_FLG                                    0x0200 /* OUT received when armed for IN */
#define IN_EXCEPTION_FLG                                     0x0100 /* IN received when armed for OUT */
#define SETUP_FLG                                            0x0010 /* SETUP packet received */

/*********************************************************/
/* DEVICE n ENDPOINT n COUNT RESULT REGISTERS [R]      */
/*********************************************************/

#define DEV1_EP0_CTR_REG                                     0x0208 /* Device 1 Endpoint 0 Count Result Register [R] */
#define DEV1_EP1_CTR_REG                                     0x0218 /* Device 1 Endpoint 1 Count Result Register [R]       */
#define DEV1_EP2_CTR_REG                                     0x0228 /* Device 1 Endpoint 2 Count Result Register [R] */
#define DEV1_EP3_CTR_REG                                     0x0238 /* Device 1 Endpoint 3 Count Result Register [R] */
#define DEV1_EP4_CTR_REG                                     0x0248 /* Device 1 Endpoint 4 Count Result Register [R] */
#define DEV1_EP5_CTR_REG                                     0x0258 /* Device 1 Endpoint 5 Count Result Register [R] */
#define DEV1_EP6_CTR_REG                                     0x0268 /* Device 1 Endpoint 6 Count Result Register [R] */
#define DEV1_EP7_CTR_REG                                     0x0278 /* Device 1 Endpoint 7 Count Result Register [R] */

#define DEV2_EP0_CTR_REG                                     0x0288 /* Device 2 Endpoint 0 Count Result Register [R] */
#define DEV2_EP1_CTR_REG                                     0x0298 /* Device 2 Endpoint 1 Count Result Register [R]       */
#define DEV2_EP2_CTR_REG                                     0x02A8 /* Device 2 Endpoint 2 Count Result Register [R] */
#define DEV2_EP3_CTR_REG                                     0x02B8 /* Device 2 Endpoint 3 Count Result Register [R] */
#define DEV2_EP4_CTR_REG                                     0x02C8 /* Device 2 Endpoint 4 Count Result Register [R] */
#define DEV2_EP5_CTR_REG                                     0x02D8 /* Device 2 Endpoint 5 Count Result Register [R] */
#define DEV2_EP6_CTR_REG                                     0x02E8 /* Device 2 Endpoint 6 Count Result Register [R] */
#define DEV2_EP7_CTR_REG                                     0x02F8 /* Device 2 Endpoint 7 Count Result Register [R] */

/* FIELDS */

#define EP_RESULT                                            0x00FF /* Endpoint Count Result */


/*********************************************************/
/*********************************************************/
/* OTG REGISTERS                                         */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* OTG CONTROL REGISTER [R/W]                            */
/*********************************************************/

#define OTG_CTL_REG                                          0xC098 /* On-The-Go Control Register [R/W] */

/* FIELDS */

#define VBUS_PULLUP_EN                                       0x2000 /* Enable VBUS Pullup */
#define OTG_RX_DIS                                           0x1000 /* Disable OTG receiver */
#define CHG_PUMP_EN                                          0x0800 /* OTG Charge Pump enable */
#define VBUS_DISCH_EN                                        0x0400 /* VBUS discharge enable */
#define DPLUS_PULLUP_EN                                      0x200  /* DPlus Pullup enable */
#define DMINUS_PULLUP_EN                                     0x100  /* DMinus Pullup enable */
#define DPLUS_PULLDOWN_EN                                    0x80   /* DPlus Pulldown enable */
#define DMINUS_PULLDOWN_EN                                   0x40   /* DMinus Pulldown enable */
#define OTG_DATA_STAT                                        0x0004 /* TTL logic state of VBUS pin [R] */
#define OTG_ID_STAT                                          0x0002 /* Value of OTG ID pin [R] */
#define VBUS_VALID_FLG                                       0x0001 /* VBUS > 4.4V [R] */


/*********************************************************/
/*********************************************************/
/* GPIO REGISTERS                                        */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* GPIO CONTROL REGISTER [R/W]                           */
/*********************************************************/

#define GPIO_CTL_REG                                         0xC006 /* GPIO Control Register [R/W] */

/* FIELDS */
#define GPIO_WP_EN                                           0x8000 /* GPIO Control Register Write-Protect enable (1:WP) */
#define UD                                                   0x0400 /* Routes Port1A TX enable status to GPIO[30] */
#define GPIO_SAS_EN                                          0x0800 /* 1:SPI SS to GPIO[15] */
#define GPIO_MODE_SEL                                        0x0700 /* GPIO Mode       */
#define GPIO_HSS_EN                                          0x0080 /* Connect HSS to GPIO (Location depends on chip) */
/* EZ-Host: GPIO [26, 18:16] */
/* EZ-OTG: GPIO[15:12] */
#define GPIO_HSS_XD_EN                                       0x0040 /* Connect HSS to XD[15:12] (TQFP only) */
#define GPIO_SPI_EN                                          0x0020 /* Connect SPI to GPIO[11:8] */
#define GPIO_SPI_XD_EN                                       0x0010 /* Connect SPI to XD[11:8] */
#define GPIO_IRQ1_POL_SEL                                    0x0008 /* IRQ1 polarity (1:positive, 0:negative)  */
#define GPIO_IRQ1_EN                                         0x0004 /* IRQ1 enable */
#define GPIO_IRQ0_POL_SEL                                    0x0002 /* IRQ0 polarity (1:positive, 0:negative) */
#define GPIO_IRQ0_EN                                         0x0001 /* IRQ0 enable */

/* GPIO MODE FIELD VALUES */

#define SCAN_MODE                                            0x0006 /* Boundary Scan mode */
#define HPI_MODE                                             0x0005 /* HPI mode */
#define IDE_MODE                                             0x0004 /* IDE mode */
#define EPP_MODE                                             0x0002 /* EPP mode */
#define GPIO_MODE                                            0x0000 /* GPIO only */

/*********************************************************/
/* GPIO n REGISTERS                                      */
/*********************************************************/

#define GPIO0_OUT_DATA_REG                                   0xC01E /* GPIO 0 Output Data Register [R/W] */
#define GPIO1_OUT_DATA_REG                                   0xC024 /* GPIO 1 Output Data Register [R/W] */

#define GPIO0_IN_DATA_REG                                    0xC020 /* GPIO 0 Input Data Register [R] */
#define GPIO1_IN_DATA_REG                                    0xC026 /* GPIO 1 Input Data Register [R] */

#define GPIO0_DIR_REG                                        0xC022 /* GPIO 0 Direction Register [R/W] (1:Output, 0:Input) */
#define GPIO1_DIR_REG                                        0xC028 /* GPIO 1 Direction Register [R/W] (1:Output, 0:Input) */
#define GPIO_HI_IO                                           0xC024 /* Alias for BIOS */
#define GPIO_HI_ENB                                          0xC028 /* Alias for BIOS */

/*********************************************************/
/*********************************************************/
/* IDE REGISTERS                                         */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* IDE MODE REGISTER [R/W]                               */
/*********************************************************/

#define IDE_MODE_REG                                         0xC048 /* IDE Mode Register [R/W] */

/* FIELDS */

#define IDE_MODE_SEL                                         0x0007 /* IDE Mode (See field values below) */

/* MODE FIELD VALUES */

#define MODE_DIS                                             0x0007 /* Disabled */
#define MODE_PIO4                                            0x0004 /* PIO Mode 4 */
#define MODE_PIO3                                            0x0003 /* PIO Mode 3 */
#define MODE_PIO2                                            0x0002 /* PIO Mode 2 */
#define MODE_PIO1                                            0x0001 /* PIO Mode 1 */
#define MODE_PIO0                                            0x0000 /* PIO Mode 0 */

/*********************************************************/
/* IDE START ADDRESS REGISTER [R/W]                      */
/*********************************************************/

#define IDE_START_ADDR_REG                                   0xC04A /* IDE Start Address Register [R/W] */

/*********************************************************/
/* IDE STOP ADDRESS REGISTER [R/W]                       */
/*********************************************************/

#define IDE_STOP_ADDR_REG                                    0xC04C /* IDE Stop Address Register [R/W] */

/*********************************************************/
/* IDE CONTROL REGISTER [R/W]                            */
/*********************************************************/

#define IDE_CTL_REG                                          0xC04E /* IDE Control Register [R/W] */

/* FIELDS */

#define IDE_DIR_SEL                                          0x0008 /* IDE Direction Select */
#define IDE_IRQ_EN                                           0x0004 /* IDE Interrupt Enable */
#define IDE_DONE_FLG                                         0x0002 /* IDE Done Flag (Set by silicon, Cleared by writing 0) */
#define IDE_EN                                               0x0001 /* IDE Enable (Set by writing 1, Cleared by silicon) */

/* DIRECTION SELECT FIELD VALUES */

#define WR_EXT                                               0x0008 /* Write to external device */
#define RD_EXT                                               0x0000 /* Read from external device */

/*********************************************************/
/* IDE PIO PORT REGISTERS [R/W]                          */
/*********************************************************/

#define IDE_PIO_DATA_REG                                     0xC050 /* IDE PIO Data Register [R/W] */
#define IDE_PIO_ERR_REG                                      0xC052 /* IDE PIO Error Register [R/W] */
#define IDE_PIO_SCT_CNT_REG                                  0xC054 /* IDE PIO Sector Count Register [R/W] */
#define IDE_PIO_SCT_NUM_REG                                  0xC056 /* IDE PIO Sector Number Register [R/W] */
#define IDE_PIO_CYL_LO_REG                                   0xC058 /* IDE PIO Cylinder Low Register [R/W] */
#define IDE_PIO_CYL_HI_REG                                   0xC05A /* IDE PIO Cylinder High Register [R/W] */
#define IDE_PIO_DEV_HD_REG                                   0xC05C /* IDE PIO Device/Head Register [R/W] */
#define IDE_PIO_CMD_REG                                      0xC05E /* IDE PIO Command Register [R/W] */
#define IDE_PIO_DEV_CTL_REG                                  0xC06C /* IDE PIO Device Control Register [R/W] */

/*********************************************************/
/*********************************************************/
/* HSS REGISTERS                                         */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* HSS CONTROL REGISTER [R/W]                            */
/*********************************************************/

#define HSS_CTL_REG                                          0xC070 /* HSS Control Register [R/W] */

/* FIELDS */

#define HSS_EN                                               0x8000 /* HSS Enable */
#define RTS_POL_SEL                                          0x4000 /* RTS Polarity Select */
#define CTS_POL_SEL                                          0x2000 /* CTS Polarity Select */
#define XOFF                                                 0x1000 /* XOFF/XON state (1:XOFF received, 0:XON received) [R] */
#define XOFF_EN                                              0x0800 /* XOFF/XON protocol Enable */
#define CTS_EN                                               0x0400 /* CTS Enable  */
#define RX_IRQ_EN                                            0x0200 /* RxRdy/RxPktRdy Interrupt Enable */
#define HSS_DONE_IRQ_EN                                      0x0100 /* TxDone/RxDone Interrupt Enable */
#define TX_DONE_IRQ_FLG                                      0x0080 /* TxDone Interrupt (Write 1 to clear) */
#define RX_DONE_IRQ_FLG                                      0x0040 /* RxDone Interrupt (Write 1 to clear) */
#define ONE_STOP_BIT                                         0x0020 /* Number of TX Stop bits (1:one TX stop bit, 0:2 TX stop bits) */
#define HSS_TX_RDY                                           0x0010 /* Tx ready for next byte */
#define PACKET_MODE_SEL                                      0x0008 /* RxIntr Source (1:RxPktRdy, 0:RxRdy) */
#define RX_OVF_FLG                                           0x0004 /* Rx FIFO overflow (Write 1 to clear and flush RX FIFO) */
#define RX_PKT_RDY_FLG                                       0x0002 /* RX FIFO full */
#define RX_RDY_FLG                                           0x0001 /* RX FIFO not empty */

/* RTS POLARITY SELECT FIELD VALUES */

#define RTS_POL_LO                                           0x4000 /* Low-true polarity */
#define RTS_POL_HI                                           0x0000 /* High-true polarity */

/* CTS POLARITY SELECT FIELD VALUES */

#define CTS_POL_LO                                           0x2000 /* Low-true polarity */
#define CTS_POL_HI                                           0x0000 /* High-true polarity */

/*********************************************************/
/* HSS BAUD RATE REGISTER [R/W]                          */
/*********************************************************/
/* Baud rate is determined as follows: */
/* */
/*     48 MHz */
/* -------------- */
/* (HSS_BAUD + 1) */
/*********************************************************/

#define HSS_BAUD_REG                                         0xC072 /* HSS Baud Register [???] */

/* FIELDS */

#define HSS_BAUD_SEL                                         0x1FFF /* HSS Baud */

/*********************************************************/
/* HSS TX GAP REGISTER [R/W]                             */
/*********************************************************/
/* This register defines the number of stop bits used */
/* for block mode transmission ONLY. The number of stop  */
/* bits is determined as follows: */
/* */
/* (TX_GAP - 7) */
/* */
/* Valid values for TX_GAP are 8-255. */
/*********************************************************/

#define HSS_TX_GAP_REG                                       0xC074 /* HSS Transmit Gap Register [???] */

/* FIELDS */

#define TX_GAP_SEL                                           0x00FF /* HSS Transmit Gap */

/*********************************************************/
/* HSS DATA REGISTER [R/W]                               */
/*********************************************************/

#define HSS_DATA_REG                                         0xC076 /* HSS Data Register [R/W] */

/* FIELDS */

#define HSS_DATA                                             0x00FF /* HSS Data */

/*********************************************************/
/* HSS RECEIVE ADDRESS REGISTER [???]                    */
/*********************************************************/

#define HSS_RX_ADDR_REG                                      0xC078 /* HSS Receive Address Register [???] */

/*********************************************************/
/* HSS RECEIVE COUNTER REGISTER [R/W]                    */
/*********************************************************/

#define HSS_RX_CTR_REG                                       0xC07A /* HSS Receive Counter Register [R/W] */

/* FIELDS */

#define HSS_RX_CTR                                           0x03FF /* Counts from (n-1) to (0-1) */

/*********************************************************/
/* HSS TRANSMIT ADDRESS REGISTER [???]                   */
/*********************************************************/

#define HSS_TX_ADDR_REG                                      0xC07C /* HSS Transmit Address Register [???] */

/*********************************************************/
/* HSS TRANSMIT COUNTER REGISTER [R/W]                   */
/*********************************************************/

#define HSS_TX_CTR_REG                                       0xC07E /* HSS Transmit Counter Register [R/W] */

/* FIELDS */

#define HSS_TX_CTR                                           0x03FF /* Counts from (n-1) to (0-1) */


/*********************************************************/
/*********************************************************/
/* SPI REGISTERS                                         */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* SPI CONFIGURATION REGISTER [R/W]                      */
/*********************************************************/

#define SPI_CFG_REG                                          0xC0C8 /* SPI Config Register [R/W] */

/* FIELDS */

#define c3WIRE_EN                                            0x8000 /* MISO/MOSI data lines common */
#define PHASE_SEL                                            0x0400 /* Advanced SCK phase */
#define SCK_POL_SEL                                          0x2000 /* Positive SCK Polarity */
#define SCALE_SEL                                            0x1E00 /* SPI Clock Frequency Scaling */
#define MSTR_ACTIVE_EN                                       0x0080 /* Master state machine active */
#define MSTR_EN                                              0x0040 /* Master/Slave select */
#define SS_EN                                                0x0020 /* SS enable */
#define SS_DLY_SEL                                           0x001F /* SS delay select */

/* SCALE VALUES */
#define SPI_SCALE_1E                                         0x1E00 /* */
#define SPI_SCALE_1C                                         0x1C00 /* */
#define SPI_SCALE_1A                                         0x1A00 /* */
#define SPI_SCALE_18                                         0x1800 /* */
#define SPI_SCALE_16                                         0x1600 /* */
#define SPI_SCALE_14                                         0x1400 /* */
#define SPI_SCALE_12                                         0x1200 /* */
#define SPI_SCALE_10                                         0x1000 /* */
#define SPI_SCALE_0E                                         0x0E00 /* */
#define SPI_SCALE_0C                                         0x0C00 /* */
#define SPI_SCALE_0A                                         0x0A00 /* */
#define SPI_SCALE_08                                         0x0800 /* */
#define SPI_SCALE_06                                         0x0600 /* */
#define SPI_SCALE_04                                         0x0400 /* */
#define SPI_SCALE_02                                         0x0200 /* */
#define SPI_SCALE_00                                         0x0000 /* */

/*********************************************************/
/* SPI CONTROL REGISTER [R/W]                            */
/*********************************************************/

#define SPI_CTL_REG                                          0xC0CA /* SPI Control Register [R/W] */

/* FIELDS */

#define SCK_STROBE                                           0x8000 /* SCK Strobe */
#define FIFO_INIT                                            0x4000 /* FIFO Init */
#define BYTE_MODE                                            0x2000 /* Byte Mode */
#define FULL_DUPLEX                                          0x1000 /* Full Duplex */
#define SS_MANUAL                                            0x0800 /* SS Manual */
#define READ_EN                                              0x0400 /* Read Enable */
#define SPI_TX_RDY                                           0x0200 /* Transmit Ready */
#define RX_DATA_RDY                                          0x0100 /* Receive Data Ready */
#define TX_EMPTY                                             0x0080 /* Transmit Empty */
#define RX_FULL                                              0x0020 /* Receive Full */
#define TX_BIT_LEN                                           0x0031 /* Transmit Bit Length */
#define RX_BIT_LEN                                           0x0007 /* Receive Bit Length */

/*********************************************************/
/* SPI INTERRUPT ENABLE REGISTER [R/W]                   */
/*********************************************************/

#define SPI_IRQ_EN_REG                                       0xC0CC /* SPI Interrupt Enable Register [R/W] */

/* FIELDS */

#define SPI_RX_IRQ_EN                                        0x0004 /* SPI Receive Interrupt Enable */
#define SPI_TX_IRQ_EN                                        0x0002 /* SPI Transmit Interrupt Enable */
#define SPI_XFR_IRQ_EN                                       0x0001 /* SPI Transfer Interrupt Enable */

/*********************************************************/
/* SPI STATUS REGISTER [R]                               */
/*********************************************************/

#define SPI_STAT_REG                                         0xC0CE /* SPI Status Register [R] */

/* FIELDS */

#define SPI_FIFO_ERROR_FLG                                   0x0100 /* FIFO Error occurred */
#define SPI_RX_IRQ_FLG                                       0x0004 /* SPI Receive Interrupt  */
#define SPI_TX_IRQ_FLG                                       0x0002 /* SPI Transmit Interrupt */
#define SPI_XFR_IRQ_FLG                                      0x0001 /* SPI Transfer Interrupt */

/*********************************************************/
/* SPI INTERRUPT CLEAR REGISTER [W]                      */
/*********************************************************/
/* In order to clear a particular IRQ, write a '1' to    */
/* the appropriate bit location.                         */
/*********************************************************/

#define SPI_IRQ_CLR_REG                                      0xC0D0 /* SPI Interrupt Clear Register [W] */

/* FIELDS */

#define SPI_TX_IRQ_CLR                                       0x0002 /* SPI Transmit Interrupt Clear */
#define SPI_XFR_IRQ_CLR                                      0x0001 /* SPI Transfer Interrupt Clear */

/*********************************************************/
/* SPI CRC CONTROL REGISTER [R/W]                        */
/*********************************************************/

#define SPI_CRC_CTL_REG                                      0xC0D2 /* SPI CRC Control Register [R/W] */

/* FIELDS */

#define CRC_MODE                                             0xC000 /* CRC Mode */
#define CRC_EN                                               0x2000 /* CRC Enable */
#define CRC_CLR                                              0x1000 /* CRC Clear */
#define RX_CRC                                               0x0800 /* Receive CRC */
#define ONE_IN_CRC                                           0x0400 /* One in CRC [R] */
#define ZERO_IN_CRC                                          0x0200 /* Zero in CRC [R] */

/* CRC MODE VALUES */

#define POLYNOMIAL_3                                         0x0003 /* CRC POLYNOMIAL 1 */
#define POLYNOMIAL_2                                         0x0002 /* CRC POLYNOMIAL X^16+X^15+X^2+1 */
#define POLYNOMIAL_1                                         0x0001 /* CRC POLYNOMIAL X^7+X^3+1 */
#define POLYNOMIAL_0                                         0x0000 /* CRC POLYNOMIAL X^16+X^12+X^5+1 */

/*********************************************************/
/* SPI CRC VALUE REGISTER [R/W]                          */
/*********************************************************/

#define SPI_CRC_VALUE_REG                                    0xC0D4 /* SPI CRC Value Register [R/W] */

/*********************************************************/
/* SPI DATA REGISTER [R/W]                               */
/*********************************************************/

#define SPI_DATA_REG                                         0xC0D6 /* SPI Data Register [R/W] */

/* FIELDS */

#define SPI_DATA                                             0x00FF /* SPI Data */

/*********************************************************/
/* SPI TRANSMIT ADDRESS REGISTER [R/W]                   */
/*********************************************************/

#define SPI_TX_ADDR_REG                                      0xC0D8 /* SPI Transmit Address Register [R/W] */

/*********************************************************/
/* SPI TRANSMIT COUNT REGISTER [R/W]                     */
/*********************************************************/

#define SPI_TX_CNT_REG                                       0xC0DA /* SPI Transmit Count Register [R/W] */

/* FIELDS */

#define SPI_TX_CNT                                           0x07FF /* SPI Transmit Count */

/*********************************************************/
/* SPI RECEIVE ADDRESS REGISTER [R/W]                    */
/*********************************************************/

#define SPI_RX_ADDR_REG                                      0xC0DC /* SPI Receive Address Register [R/W] */

/*********************************************************/
/* SPI RECEIVE COUNT REGISTER [R/W]                      */
/*********************************************************/

#define SPI_RX_CNT_REG                                       0xC0DE /* SPI Receive Count Register [R/W] */

/* FIELDS */

#define SPI_RX_CNT                                           0x07FF /* SPI Receive Count */

/*********************************************************/
/*********************************************************/
/* UART REGISTERS                                        */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* UART CONTROL REGISTER [R/W]                           */
/*********************************************************/

#define UART_CTL_REG                                         0xC0E0 /* UART Control Register [R/W] */

/* FIELDS */

#define UART_SCALE_SEL                                       0x0010 /* UART Scale (1:Divide by 8 prescaler for the UART Clock) */
#define UART_BAUD_SEL                                        0x000E /* UART Baud */
#define UART_EN                                              0x0001 /* UART Enable */

/* BAUD VALUES */

#define UART_7K2                                             0x000F /* 7.2K Baud (0.9K with DIV8_EN Set) */
#define UART_9K6                                             0x000E /* 9.6K Baud (1.2K with DIV8_EN Set) */
#define UART_14K4                                            0x000C /* 14.4K Baud (1.8K with DIV8_EN Set) */
#define UART_19K2                                            0x000D /* 19.2K Baud (2.4K with DIV8_EN Set) */
#define UART_28K8                                            0x000B /* 28.8K Baud (3.6K with DIV8_EN Set) */
#define UART_38K4                                            0x000A /* 38.4K Baud (4.8K with DIV8_EN Set) */
#define UART_57K6                                            0x0009 /* 57.6K Baud (7.2K with DIV8_EN Set) */
#define UART_115K2                                           0x0008 /* 115.2K Baud (14.4K with DIV8_EN Set) */

/*********************************************************/
/* UART STATUS REGISTER [R]                              */
/*********************************************************/

#define UART_STAT_REG                                        0xC0E2 /* UART Status Register [R] */

/* FIELDS */

#define UART_RX_FULL                                         0x0002 /* UART Receive Full */
#define UART_TX_EMPTY                                        0x0001 /* UART Transmit Empty */

/*********************************************************/
/* UART DATA REGISTER [R/W]                              */
/*********************************************************/

#define UART_DATA_REG                                        0xC0E4 /* UART Data Register [R/W] */
#define UART_RX_REG                                          0xC0E4 /* Alias for BIOS code */
#define UART_TX_REG                                          0xC0E4 /* Alias for BIOS code */

/* FIELDS */
#define UART_DATA                                            0x00FF /* UART Data */


/*********************************************************/
/*********************************************************/
/* PWM REGISTERS                                         */
/*********************************************************/
/*********************************************************/

/*********************************************************/
/* PWM CONTROL REGISTER [R/W]                            */
/*********************************************************/

#define PWM_CTL_REG                                          0xC0E6 /* PWM Control Register [R/W] */

/* FIELDS */

#define PWM_EN                                               0x8000 /* 1:Start, 0:Stop */
#define PWM_PRESCALE_SEL                                     0x0E00 /* Prescale field (See values below) */
#define PWM_MODE_SEL                                         0x0100 /* 1:Single cycle, 0:Repetitive cycle */
#define PWM3_POL_SEL                                         0x0080 /* 1:Positive polarity, 0:Negative polarity  */
#define PWM2_POL_SEL                                         0x0040 /* 1:Positive polarity, 0:Negative polarity  */
#define PWM1_POL_SEL                                         0x0020 /* 1:Positive polarity, 0:Negative polarity  */
#define PWM0_POL_SEL                                         0x0010 /* 1:Positive polarity, 0:Negative polarity  */
#define PWM3_EN                                              0x0008 /* PWM3 Enable */
#define PWM2_EN                                              0x0004 /* PWM2 Enable */
#define PWM1_EN                                              0x0002 /* PWM1 Enable */
#define PWM0_EN                                              0x0001 /* PWM0 Enable */

/* PRESCALER FIELD VALUES */

#define PWM_5K9                                              0x0007 /* 5.9 KHz  */
#define PWM_23K5                                             0x0006 /* 23.5 KHz */
#define PWM_93K8                                             0x0005 /* 93.8 KHz */
#define PWM_375K                                             0x0004 /* 375 KHz */
#define PWM_1M5                                              0x0003 /* 1.5 MHz */
#define PWM_6M                                               0x0002 /* 6.0 MHz */
#define PWM_24M                                              0x0001 /* 24.0 MHz */
#define PWM_48M                                              0x0000 /* 48.0 MHz */

/*********************************************************/
/* PWM MAXIMUM COUNT REGISTER [R/W]                      */
/*********************************************************/

#define PWM_MAX_CNT_REG                                      0xC0E8 /* PWM Maximum Count Register [R/W] */

/* FIELDS */

#define PWM_MAX_CNT                                          0x03FF /* PWM Maximum Count */

/*********************************************************/
/* PWM n START REGISTERS [R/W]                           */
/*********************************************************/

#define PWM0_START_REG                                       0xC0EA /* PWM 0 Start Register [R/W] */
#define PWM1_START_REG                                       0xC0EE /* PWM 1 Start Register [R/W] */
#define PWM2_START_REG                                       0xC0F2 /* PWM 2 Start Register [R/W] */
#define PWM3_START_REG                                       0xC0F6 /* PWM 3 Start Register [R/W] */

/* FIELDS */

#define PWM_START_CNT                                        0x03FF /* */

/*********************************************************/
/* PWM n STOP REGISTERS [R/W]                            */
/*********************************************************/

#define PWM0_STOP_REG                                        0xC0EC /* PWM 0 Stop Register [R/W] */
#define PWM1_STOP_REG                                        0xC0F0 /* PWM 1 Stop Register [R/W] */
#define PWM2_STOP_REG                                        0xC0F4 /* PWM 2 Stop Register [R/W] */
#define PWM3_STOP_REG                                        0xC0F8 /* PWM 3 Stop Register [R/W] */

/* FIELDS */

#define PWM_STOP_CNT                                         0x03FF /* PWM Stop Count */

/*********************************************************/
/* PWM CYCLE COUNT REGISTER [R/W]                        */
/*********************************************************/

#define PWM_CYCLE_CNT_REG                                    0xC0FA /* PWM Cycle Count Register [R/W] */


/*********************************************************/
/*********************************************************/
/* HPI REGISTERS                                         */
/*********************************************************/
/*********************************************************/


/*********************************************************/
/* SIEnMSG REGISTER [R/W]                                  */
/*********************************************************/

#define SIE1MSG_REG                                          0x0144 /* SIE1msg Register [R/W] */
#define SIE2MSG_REG                                          0x0148 /* SIE2msg Register [R/W] */

/*********************************************************/
/* HPI MAILBOX REGISTER [R/W]                            */
/*********************************************************/

#define HPI_MBX_REG                                          0xC0C4 /* HPI Mailbox Register [R/W] */

/*********************************************************/
/* HPI BREAKPOINT REGISTER [R]                           */
/*********************************************************/

#define HPI_BKPT_REG                                         0x0140 /* HPI Breakpoint Register [R] */

/*********************************************************/
/* INTERRUPT ROUTING REGISTER [R]                        */
/*********************************************************/

#define HPI_IRQ_ROUTING_REG                                  0x0142 /* HPI Interrupt Routing Register [R] */

/* FIELDS */

#define VBUS_TO_HPI_EN                                       0x8000 /* Route OTG VBUS Interrupt to HPI */
#define ID_TO_HPI_EN                                         0x4000 /* Route OTG ID Interrupt to HPI */
#define SOFEOP2_TO_HPI_EN                                    0x2000 /* Route SIE2 SOF/EOP Interrupt to HPI */
#define SOFEOP2_TO_CPU_EN                                    0x1000 /* Route SIE2 SOF/EOP Interrupt to CPU */
#define SOFEOP1_TO_HPI_EN                                    0x0800 /* Route SIE1 SOF/EOP Interrupt to HPI */
#define SOFEOP1_TO_CPU_EN                                    0x0400 /* Route SIE1 SOF/EOP Interrupt to CPU */
#define RST2_TO_HPI_EN                                       0x0200 /* Route SIE2 Reset Interrupt to HPI */
#define HPI_SWAP_1_EN                                        0x0100 /* Swap HPI MSB/LSB */
#define RESUME2_TO_HPI_EN                                    0x0080 /* Route SIE2 Resume Interrupt to HPI */
#define RESUME1_TO_HPI_EN                                    0x0040 /* Route SIE1 Resume Interrupt to HPI */
#define DONE2_TO_HPI_EN                                      0x0008 /* Route SIE2 Done Interrupt to HPI */
#define DONE1_TO_HPI_EN                                      0x0004 /* Route SIE1 Done Interrupt to HPI */
#define RST1_TO_HPI_EN                                       0x0002 /* Route SIE1 Reset Interrupt to HPI */
#define HPI_SWAP_0_EN                                        0x0001 /* Swap HPI MSB/LSB (*MUST MATCH HPI_SWAP_1) */


/*********************************************************/
/*********************************************************/
/* HPI PORTS                                             */
/*********************************************************/
/*********************************************************/

#define HPI_BASE                                             0x0000

/*********************************************************/
/* HPI DATA PORT                                         */
/*********************************************************/

#define HPI_DATA_PORT                                        0x0000 /* HPI Data Port */

/*********************************************************/
/* HPI ADDRESS PORT                                      */
/*********************************************************/

#define HPI_ADDR_PORT                                        0x0002 /* HPI Address Port */

/*********************************************************/
/* HPI MAILBOX PORT                                      */
/*********************************************************/

#define HPI_MBX_PORT                                         0x0001 /* HPI Mailbox Port */

/*********************************************************/
/* HPI STATUS PORT                                       */
/*********************************************************/

/*
** The HPI Status port is only accessible by an external host over the
** HPI interface. It is accessed by performing an HPI read at the HPI
** base address + 3.
*/

#define HPI_STAT_PORT                                        0x0003 /* HPI Status Port */

/* FIELDS */

#define VBUS_FLG                                             0x8000 /* OTG VBUS Interrupt */
#define ID_FLG                                               0x4000 /* OTG ID Interrupt */
#define SOFEOP2_FLG                                          0x1000 /* Host 2 SOF/EOP Interrupt */
#define SOFEOP1_FLG                                          0x0400 /* Host 1 SOF/EOP Interrupt */
#define RST2_FLG                                             0x0200 /* Host 2 Reset Interrupt */
#define MBX_IN_FLG                                           0x0100 /* Message in pending (awaiting CPU read) */
#define RESUME2_FLG                                          0x0080 /* Host 2 Resume Interrupt */
#define RESUME1_FLG                                          0x0040 /* Host 1 Resume Interrupt */
#define SIE2MSG_FLG                                          0x0020 /* SIE2msg Register has been written */
#define SIE1MSG_FLG                                          0x0010 /* SIE1msg Register has been written */
#define DONE2_FLG                                            0x0008 /* Host 2 Done Interrupt */
#define DONE1_FLG                                            0x0004 /* Host 1 Done Interrupt */
#define RST1_FLG                                             0x0002 /* Host 1 Reset Interrupt */
#define MBX_OUT_FLG                                          0x0001 /* Message out available (awaiting external host read) */


/*********************************************************/
/*  Hardware/Software Interrupt vectors                  */
/*********************************************************/
/* ========= HARWDARE INTERRUPTS ===========             */
#define TIMER0_INT                                           0x0000
#define TIMER0_VEC                                           0x0000 /* Vector location */
#define TIMER1_INT                                           0x0001
#define TIMER1_VEC                                           0x0002

#define GP_IRQ0_INT                                          0x0002
#define GP_IRQ0_VEC                                          0x0004
#define GP_IRQ1_INT                                          0x0003
#define GP_IRQ1_VEC                                          0x0006

#define UART_TX_INT                                          0x0004
#define UART_TX_VEC                                          0x0008
#define UART_RX_INT                                          0x0005
#define UART_RX_VEC                                          0x000A


#define HSS_BLK_DONE_INT                                     0x0006
#define HSS_BLK_DONE_VEC                                     0x000C
#define HSS_RX_FULL_INT                                      0x0007
#define HSS_RX_FULL_VEC                                      0x000E

#define IDE_DMA_DONE_INT                                     0x0008
#define IDE_DMA_DONE_VEC                                     0x0010

#define Reserved9                                            0x0009

#define HPI_MBOX_RX_FULL_INT                                 0x000A
#define HPI_MBOX_RX_FULL_VEC                                 0x0014
#define HPI_MBOX_TX_EMPTY_INT                                0x000B
#define HPI_MBOX_TX_EMPTY_VEC                                0x0016

#define SPI_TX_INT                                           0x000C
#define SPI_TX_VEC                                           0x0018
#define SPI_RX_INT                                           0x000D
#define SPI_RX_VEC                                           0x001A
#define SPI_DMA_DONE_INT                                     0x000E
#define SPI_DMA_DONE_VEC                                     0x001C

#define OTG_ID_VBUS_VALID_INT                                0x000F
#define OTG_ID_VBUS_VALID_VEC                                0x001E

#define SIE1_HOST_DONE_INT                                   0x0010
#define SIE1_HOST_DONE_VEC                                   0x0020
#define SIE1_HOST_SOF_INT                                    0x0011
#define SIE1_HOST_SOF_VEC                                    0x0022
#define SIE1_HOST_INS_REM_INT                                0x0012
#define SIE1_HOST_INS_REM_VEC                                0x0024

#define Reserved19                                           0x0013

#define SIE1_SLAVE_RESET_INT                                 0x0014
#define SIE1_SLAVE_RESET_VEC                                 0x0028
#define SIE1_SLAVE_SOF_INT                                   0x0015
#define SIE1_SLAVE_SOF_VEC                                   0x002A

#define Reserved22                                           0x0016
#define Reserved23                                           0x0017

#define SIE2_HOST_DONE_INT                                   0x0018
#define SIE2_HOST_DONE_VEC                                   0x0030
#define SIE2_HOST_SOF_INT                                    0x0019
#define SIE2_HOST_SOF_VEC                                    0x0032
#define SIE2_HOST_INS_REM_INT                                0x001A
#define SIE2_HOST_INS_REM_VEC                                0x0034

#define Reserved27                                           0x001B

#define SIE2_SLAVE_RESET_INT                                 0x001C
#define SIE2_SLAVE_RESET_VEC                                 0x0038
#define SIE2_SLAVE_SOF_INT                                   0x001D
#define SIE2_SLAVE_SOF_VEC                                   0x003A

#define Reserved30                                           0x001E
#define Reserved31                                           0x001F

#define SIE1_EP0_INT                                         0x0020
#define SIE1_EP0_VEC                                         0x0040
#define SIE1_EP1_INT                                         0x0021
#define SIE1_EP1_VEC                                         0x0042
#define SIE1_EP2_INT                                         0x0022
#define SIE1_EP2_VEC                                         0x0044
#define SIE1_EP3_INT                                         0x0023
#define SIE1_EP3_VEC                                         0x0046
#define SIE1_EP4_INT                                         0x0024
#define SIE1_EP4_VEC                                         0x0048
#define SIE1_EP5_INT                                         0x0025
#define SIE1_EP5_VEC                                         0x004A
#define SIE1_EP6_INT                                         0x0026
#define SIE1_EP6_VEC                                         0x004C
#define SIE1_EP7_INT                                         0x0027
#define SIE1_EP7_VEC                                         0x004E

#define SIE2_EP0_INT                                         0x0028
#define SIE2_EP0_VEC                                         0x0050
#define SIE2_EP1_INT                                         0x0029
#define SIE2_EP1_VEC                                         0x0052
#define SIE2_EP2_INT                                         0x002A
#define SIE2_EP2_VEC                                         0x0054
#define SIE2_EP3_INT                                         0x002B
#define SIE2_EP3_VEC                                         0x0056
#define SIE2_EP4_INT                                         0x002C
#define SIE2_EP4_VEC                                         0x0058
#define SIE2_EP5_INT                                         0x002D
#define SIE2_EP5_VEC                                         0x005A
#define SIE2_EP6_INT                                         0x002E
#define SIE2_EP6_VEC                                         0x005C
#define SIE2_EP7_INT                                         0x002F
#define SIE2_EP7_VEC                                         0x005E

/*********************************************************/
/*  Interrupts 48 - 63 are reserved for future HW        */
/*********************************************************/
/* ========= SOFTWARE INTERRUPTS ===========             */

#define I2C_INT                                              0x0040
#define LI2C_INT                                             0x0041
#define UART_INT                                             0x0042
#define SCAN_INT                                             0x0043
#define ALLOC_INT                                            0x0044
#define IDLE_INT                                             0x0046
#define IDLER_INT                                            0x0047
#define INSERT_IDLE_INT                                      0x0048
#define PUSHALL_INT                                          0x0049
#define POPALL_INT                                           0x004A
#define FREE_INT                                             0x004B
#define REDO_ARENA                                           0x004C
#define HW_SWAP_REG                                          0x004D
#define HW_REST_REG                                          0x004E
#define SCAN_DECODE_INT                                      0x004F

/*********************************************************/
/* -- INTs 80 to 115 for SUSB ---                        */
/*********************************************************/
#define SUSB1_SEND_INT                                       0x0050
#define SUSB1_RECEIVE_INT                                    0x0051
#define SUSB1_STALL_INT                                      0x0052
#define SUSB1_STANDARD_INT                                   0x0053
#define SUSB1_STANDARD_LOADER_VEC                            0x00A8
#define SUSB1_VENDOR_INT                                     0x0055
#define SUSB1_VENDOR_LOADER_VEC                              0x00AC
#define SUSB1_CLASS_INT                                      0x0057
#define SUSB1_CLASS_LOADER_VEC                               0x00B0
#define SUSB1_FINISH_INT                                     0x0059
#define SUSB1_DEV_DESC_VEC                                   0x00B4
#define SUSB1_CONFIG_DESC_VEC                                0x00B6
#define SUSB1_STRING_DESC_VEC                                0x00B8
#define SUSB1_PARSE_CONFIG_INT                               0x005D
#define SUSB1_LOADER_INT                                     0x005E
#define SUSB1_DELTA_CONFIG_INT                               0x005F

#define SUSB2_SEND_INT                                       0x0060
#define SUSB2_RECEIVE_INT                                    0x0061
#define SUSB2_STALL_INT                                      0x0062
#define SUSB2_STANDARD_INT                                   0x0063
#define SUSB2_STANDARD_LOADER_VEC                            0x00C8
#define SUSB2_VENDOR_INT                                     0x0065
#define SUSB2_VENDOR_LOADER_VEC                              0x00CC
#define SUSB2_CLASS_INT                                      0x0067
#define SUSB2_CLASS_LOADER_VEC                               0x00D0
#define SUSB2_FINISH_INT                                     0x0069
#define SUSB2_DEV_DESC_VEC                                   0x00D4
#define SUSB2_CONFIG_DESC_VEC                                0x00D6
#define SUSB2_STRING_DESC_VEC                                0x00D8
#define SUSB2_PARSE_CONFIG_INT                               0x006D
#define SUSB2_LOADER_INT                                     0x006E
#define SUSB2_DELTA_CONFIG_INT                               0x006F
#define USB_INIT_INT                                         0x0070
#define SUSB_INIT_INT                                        0x0071
#define REMOTE_WAKEUP_INT                                    0x0077
#define START_SRP_INT                                        0x0078


/*********************************************************/
/* --- 114 - 117 for Host SW INT's ---                   */
/*********************************************************/
#define HUSB_SIE1_INIT_INT                                   0x0072
#define HUSB_SIE2_INIT_INT                                   0x0073
#define HUSB_RESET_INT                                       0x0074

#define SUSB1_OTG_DESC_VEC                                   0x00EE
#define PWR_DOWN_INT                                         0x0078

/*********************************************************/
/* -- CMD Processor INTs ---                             */
/*********************************************************/
#define SEND_HOST_CMD_INT                                    0x0079
#define PROCESS_PORT_CMD_INT                                 0x007A
#define PROCESS_CRB_INT                                      0x007B

/*********************************************************/
/*--- INT 125, 126 and 127 for Debugger ----             */
/*********************************************************/



#endif /* __CY7C67200_300_h_ */
