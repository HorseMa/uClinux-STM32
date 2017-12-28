/*
 *  linux/include/asm-arm/arch-lpc28xx/irqs.h
 *
 *  Copyright (C) 2004 Philips Semiconductors
 *
 *  IRQ number definition
 *  All IRQ numbers of the LPC28xx CPUs.
 *
 */

#ifndef __LPC28XX_IRQS_H
#define __LPC28XX_IRQS_H                        1

#define NR_IRQS			32

#define LPC28XX_IRQ_RSV0	0	/* reserved */
#define LPC28XX_IRQ_ER0		1	/* Event Router IRQ0 */
#define LPC28XX_IRQ_ER1		2	/* Event Router IRQ1 */
#define LPC28XX_IRQ_ER2		3	/* Event Router IRQ2 */
#define LPC28XX_IRQ_ER3		4	/* Event Router IRQ3 */
#define LPC28XX_IRQ_TIMER0	5	/* Timer 0 zero count */
#define LPC28XX_IRQ_TIMER1	6	/* Timer 1 zero count */
#define LPC28XX_IRQ_RTCC	7	/* Real Time Clock Counter Increment */
#define LPC28XX_IRQ_RTCA	8	/* Real Time Clock Alarm */
#define LPC28XX_IRQ_ADC		9	/* ADC conversion complete */
#define LPC28XX_IRQ_MCI0	10	/* MCI interrupt 1 from Secure Digital and Multimedia Card Interface */
#define LPC28XX_IRQ_MCI1	11	/* MCI interrupt 2 from Secure Digital and Multimedia Card Interface */
#define LPC28XX_IRQ_UART	12	/* UART Receiver Error Flag */
#define LPC28XX_IRQ_I2C		13	/* I2C Transmit Done */
#define LPC28XX_IRQ_RSV1	14	/* reserved */
#define LPC28XX_IRQ_RSV2	15	/* reserved */
#define LPC28XX_IRQ_SAI1	16	/* SAI1 */
#define LPC28XX_IRQ_RSV3	17	/* reserved */
#define LPC28XX_IRQ_RSV4	18	/* reserved */
#define LPC28XX_IRQ_SAI4	19	/* SAI4 */
#define LPC28XX_IRQ_SAO1	20	/* SAO1 */
#define LPC28XX_IRQ_SAO2	21	/* SAO2 */
#define LPC28XX_IRQ_RSV5	22	/* reserved */
#define LPC28XX_IRQ_FLASH	23	/* Flash */
#define LPC28XX_IRQ_LCD		24	/* LCD Interface */
#define LPC28XX_IRQ_GPDMA	25	/* GPDMA */
#define LPC28XX_IRQ_USB0	26	/* USB 2.0 High Speed Device interface, low priority interrupt */
#define LPC28XX_IRQ_USB1	27	/* USB 2.0 High Speed Device interface, high priority interrupt */
#define LPC28XX_IRQ_UDMA0	28	/* USB DMA channel 0 */
#define	LPC28XX_IRQ_UDMA1	29	/* USB DMA channel 0 */
#define LPC28XX_IRQ_RSV6	30	/* reserved */
#define LPC28XX_IRQ_RSV7	31	/* reserved */

#endif /* End of __irqs_h */
