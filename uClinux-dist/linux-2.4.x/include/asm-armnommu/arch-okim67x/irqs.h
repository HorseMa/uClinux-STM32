/*
 * asm/arch-oki/irqs.h:
 * (c) 2004 V. R. Sanders
 */
#ifndef __ASM_ARCH_IRQS_H__
#define __ASM_ARCH_IRQS_H__


#if defined(CONFIG_CPU_OKIM67400X) || defined(CONFIG_CPU_OKIM67500X)
/*
 ******************* OKI67[45]00X ********************
 */

#define NR_IRQS		32
#define VALID_IRQ(i)	((i)>=IRQ_SYSTIMER && (i)<NR_IRQS)

#define CPU_IRQ(x) (x)

#define IRQ_SYSTIMER           CPU_IRQ(0)
#define IRQ_WATCHDOG           CPU_IRQ(1)
#define IRQ_WATCHDOG_INTERVAL  CPU_IRQ(2)
#define IRQ_GPIOA              CPU_IRQ(4)
#define IRQ_UNUSED5            CPU_IRQ(5)
#define IRQ_CPIOB              CPU_IRQ(6)
#define IRQ_CPIOC              CPU_IRQ(7)
#define IRQ_CPIODE             CPU_IRQ(8)
#define IRQ_SOFT               CPU_IRQ(9)

#define IRQ_SIO                CPU_IRQ(10)
#define IRQ_AD                 CPU_IRQ(11)
#define IRQ_OWM0               CPU_IRQ(12)
#define IRQ_PWM1               CPU_IRQ(13)
#define IRQ_SSIO               CPU_IRQ(14)

#define IRQ_I2C                CPU_IRQ(15)
#define IRQ_TIMER0             CPU_IRQ(16)
#define IRQ_TIMER1             CPU_IRQ(17)
#define IRQ_TIMER2             CPU_IRQ(18)
#define IRQ_TIMER3             CPU_IRQ(19)

#define IRQ_TIMER4             CPU_IRQ(20)
#define IRQ_TIMER5             CPU_IRQ(21)
#define IRQ_EXT0               CPU_IRQ(22)
#define IRQ_UNUSED23           CPU_IRQ(23)
#define IRQ_DMA0               CPU_IRQ(24)

#define IRQ_DMA1               CPU_IRQ(25)
#define IRQ_EXT1               CPU_IRQ(26)
#define IRQ_UNUSED27           CPU_IRQ(27)
#define IRQ_EXT2               CPU_IRQ(28)
#define IRQ_UNUSED29           CPU_IRQ(29)
#define IRQ_UNUSED30           CPU_IRQ(30)

#define IRQ_EXT3               CPU_IRQ(31)

#else
  #error "Configuration error: No CPU defined"
#endif

#endif /* __ASM_ARCH_IRQS_H__ */
