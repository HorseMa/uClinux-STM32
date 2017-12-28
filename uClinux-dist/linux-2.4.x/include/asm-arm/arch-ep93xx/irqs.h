/*
 *  File:	linux/include/asm-arm/arch-ep93xx/irqs.h
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
 */

/* 
 *  Linux IRQ interrupts definitions here.
 *	TBD not worth the effort to put duplicate bit defines in platform.h
 *
 *	Current IRQ implementation
 *
 *	1.  Current implementation does not support vectored 
 *          interrupts.  support for this feature may be 
 *          added later.
 *	2.  FIQs are ignored.  None are assigned.
 *	3.  All interrupts are assigned to IRQs.
 *	5.  IRQ numbers are same as interrupt bit numbers.
 *
 */

#define IRQ_RFU0	0
#define IRQ_RFU1	1
#define IRQ_COMMRX	2
#define IRQ_COMMTX	3
#define IRQ_TIMER1	4
#define IRQ_TIMER2	5
#define IRQ_AAC         6
#define IRQ_DMAM2P0	7
#define IRQ_DMAM2P1	8
#define IRQ_DMAM2P2     9
#define IRQ_DMAM2P3	10
#define IRQ_DMAM2P4	11
#define IRQ_DMAM2P5	12
#define IRQ_DMAM2P6	13
#define IRQ_DMAM2P7	14
#define IRQ_DMAM2P8	15
#define IRQ_DMAM2P9     16
#define IRQ_DMAM2M0	17
#define IRQ_DMAM2M1	18
#define IRQ_GPIO0	19
#define IRQ_GPIO1	20
#define IRQ_GPIO2	21
#define IRQ_GPIO3	22
#define IRQ_UARTRX1	23
#define IRQ_UARTTX1	24
#define IRQ_UARTRX2	25
#define IRQ_UARTTX2	26
#define IRQ_UARTRX3	27
#define IRQ_UARTTX3	28
#define IRQ_KEY	        29
#define IRQ_TOUCH	30
#define IRQ_GRAPHICS	31

#define INT1_IRQS       0xfffffffc

#define IRQ_EXT0	32
#define IRQ_EXT1	33
#define IRQ_EXT2	34
#define IRQ_64HZ	35
#define IRQ_WEINT	36
#define IRQ_RTC 	37
#define IRQ_IRDA	38
#define IRQ_MAC 	39
#define IRQ_EXT3	40
#define IRQ_EIDE	IRQ_EXT3
#define IRQ_PROG	41
#define IRQ_1HZ 	42
#define IRQ_VSYNC	43
#define IRQ_VIDEOFIFO	44
#define IRQ_SSPRX	45
#define IRQ_SSPTX	46
#define IRQ_GPIO4	47
#define IRQ_GPIO5	48
#define IRQ_GPIO6	49
#define IRQ_GPIO7	50
#define IRQ_TIMER3	51
#define IRQ_UART1	52
#define IRQ_SSP 	53
#define IRQ_UART2	54
#define IRQ_UART3	55
#define IRQ_USH 	56
#define IRQ_PME 	57
#define IRQ_DSP 	58
#define IRQ_GPIO	59
#define IRQ_SAI		60
#define IRQ_RFU61	61
#define IRQ_RFU62	62
#define IRQ_RFU63	63

#define INT2_IRQS       0x1fffffff

#define NR_IRQS         61
