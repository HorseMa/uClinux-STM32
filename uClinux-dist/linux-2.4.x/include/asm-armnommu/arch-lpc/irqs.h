#ifndef __ASM_ARCH_IRQS_H__
#define __ASM_ARCH_IRQS_H__

#define	NR_IRQS 32
#define VALID_IRQ(i) (i>=0 && i<NR_IRQS)

#define IRQ_WD	0
#define IRQ_TC0	4
#define IRQ_TC1	5
#define IRQ_UART0	6
#define IRQ_UART1	7
#define IRQ_PWM		8
#define IRQ_I2C		9
#define IRQ_SPI0	10
#define IRQ_SPI1	11
#define IRQ_PLL		12
#define IRQ_RTC		13
#define IRQ_EXT0	14
#define IRQ_EXT1	15
#define IRQ_EXT2	16
#define IRQ_EXT3	17
#define IRQ_AD	18

#endif /* __ASM_ARCH_IRQS_H__ */
