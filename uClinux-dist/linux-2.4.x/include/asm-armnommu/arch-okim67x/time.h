/*
 * linux/include/asm-armnommu/arch-oki/time.h
 *
 * Copyright gufff
 *
 * Ben Dooks
*/

#define OKI_TIMECNTL   (0x00)
#define OKI_TIMECNTL_IRQEN (1<<4)
#define OKI_TIMECNTL_START (1<<3)
#define OKI_TIMECNTL_DIV32 (5<<5)

#define OKI_TIMEBASE   (0x04)
#define OKI_TIMECNT    (0x08)
#define OKI_TIMECMP    (0x0C)
#define OKI_TIMESTAT   (0x10)

#define OKI_TIMER0     (0xB7F00000)
#define OKI_TIMER1     (0xB7F00020)
#define OKI_TIMER2     (0xB7F00040)
#define OKI_TIMER3     (0xB7F00060)
#define OKI_TIMER4     (0xB7F00080)
#define OKI_TIMER5     (0xB7F000A0)

#define WHICH_TIMER  (OKI_TIMER5)

#define SYS_CLK (oki_cclk)
#define TMR_CLK (SYS_CLK / 32)

#define TMR_VAL (TMR_CLK / HZ)

#define __arch_readl(addr) (*((unsigned volatile int *)(addr)))

#define __arch_writel(data,addr) \
  do { *((volatile unsigned int *)(addr)) = (data); } while(0)


static unsigned long oki_gettimeoffset (void)
{
  unsigned int tmrcnt = __arch_readl(WHICH_TIMER + OKI_TIMECNT);
  /* get offset from last tick */

  /* check maths on this... */
  tmrcnt *= (TMR_VAL / 1000);
  tmrcnt /= 1000;

  return 0;
}

static void oki_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  /* clear timer */
  __arch_writel(0x01, WHICH_TIMER + OKI_TIMESTAT);

  do_timer(regs);
  do_profile(regs);

}


extern inline void setup_timer (void)
{
  unsigned int tmr = WHICH_TIMER;

  /* shut down before using */
  printk("Initialising timer:\n");

	__arch_writel(OKI_TIMECNTL_DIV32, tmr + OKI_TIMECNTL);
	 
        gettimeoffset = oki_gettimeoffset;
        timer_irq.handler = oki_timer_interrupt;

	printk("registering timer interrupt...\n");

        setup_arm_irq(IRQ_TIMER5, &timer_irq);

	printk("enabling timer...\n");

	/* enable the timer */

	printk("TMR_VAL = %08x\n", TMR_VAL);


	__arch_writel(0x00, tmr + OKI_TIMEBASE);
	__arch_writel(TMR_VAL, tmr + OKI_TIMECMP);

	__arch_writel(OKI_TIMECNTL_DIV32 | OKI_TIMECNTL_START | OKI_TIMECNTL_IRQEN, tmr + OKI_TIMECNTL);

	printk("timer enabled\n");
}
