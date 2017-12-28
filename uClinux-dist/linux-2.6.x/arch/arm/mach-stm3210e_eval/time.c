/*
 * 2009		MCD Application TEAM.
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>

#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/sched.h>

#include <linux/timex.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/hardware/arm_timer.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>

#include <asm/hardware/gic.h>

#include <asm/arch/stm32f10x_conf.h>


#include "time.h"


#if 0
//def SYSTICK_AS_SHED_CLK
#define ReadSysTickCounter() ((unsigned long long )SysTick->VAL)


unsigned long long sched_clock(void)
{
	unsigned long long v;

	v = (unsigned long long)((~ReadSysTickCounter()) * 125);
	do_div(v, 9);

	return v;
}
#endif


/* used by entry-macro.S */
void __iomem *gic_cpu_base_addr;

#define TIMER_AR  360

static void timer_set_mode(enum clock_event_mode mode,
			   struct clock_event_device *clk)
{
	switch(mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		TIM_SetAutoreload(TIMx,TIMER_AR);
		TIM_ARRPreloadConfig(TIMx, ENABLE);
		TIM_Cmd(TIMx,ENABLE);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		/* period set, and timer enabled in 'next_event' hook */
		TIM_ARRPreloadConfig(TIMx, DISABLE);
		TIM_Cmd(TIMx,ENABLE);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	default:
		TIM_Cmd(TIMx,DISABLE);
	}

}
/* FIXME Use a different timer */
static int timer_set_next_event(unsigned long evt,
				struct clock_event_device *unused)
{
	TIM_Cmd(TIMx,DISABLE);
	TIM_ARRPreloadConfig(TIMx, (u16)evt);
	TIM_Cmd(TIMx,ENABLE);

	return 0;
}

static struct clock_event_device timer0_clockevent =	 {
	.name		= "STM3210E-EVAL System Timer",
	.shift		= 24,
	.features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_mode	= timer_set_mode,
	.set_next_event	= timer_set_next_event,
	.rating		= 300,
	.cpumask	= CPU_MASK_CPU0,
};

static void __init stm3210e_eval_clockevents_init(unsigned int timer_irq)
{
	timer0_clockevent.irq = timer_irq;
	timer0_clockevent.mult =
		div_sc(36000, NSEC_PER_SEC,timer0_clockevent.shift);
	timer0_clockevent.max_delta_ns =
		clockevent_delta2ns(0x7fff, &timer0_clockevent);
	timer0_clockevent.min_delta_ns =
		clockevent_delta2ns(0xf, &timer0_clockevent);
	clockevents_register_device(&timer0_clockevent);
}

/*
 * IRQ handler for the timer
 */
static irqreturn_t stm3210e_eval_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = &timer0_clockevent;
	//write_seqlock(&xtime_lock);
	
	/* clear the interrupt */
	TIM_ClearITPendingBit(TIMx, TIM_IT_Update);
		
	/* Handle Event */
	evt->event_handler(evt);
	
	//write_sequnlock(&xtime_lock);
	return IRQ_HANDLED;
}

static struct irqaction stm3210e_eval_timer_irq = {
	.name		= "STM3210E-EVAL kernel Time Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= stm3210e_eval_timer_interrupt,
};

static cycle_t stm3210e_eval_get_cycles(void)
{
	return (cycle_t)(TIM_GetCounter(TIM6));
}

static struct clocksource clocksource_stm3210e = {
	.name	= "STM3210E-EVAL RTC Clock",
	.rating	= 200,
	.read	= stm3210e_eval_get_cycles,
	.mask	= CLOCKSOURCE_MASK(16),
	.shift	= 1,
	.flags	= CLOCK_SOURCE_MUST_VERIFY,
};

/*unsigned long read_persistent_clock(void)
{
	return RTC_GetCounter();
}
*/

static void __init stm3210e_eval_clocksource_init(void)
{
	/* Initialisation  */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
	
	/* TIM6 DeInit registers */
	TIM_DeInit(TIM6);
	/* TIM6 Set Prescaler */
	TIM_PrescalerConfig(TIM6, 2000, TIM_PSCReloadMode_Immediate);
	/* TIM6 Set Autoreload Val to 0xFFFF */
#define AutoReloadVal 0xFFFF
	TIM_SetAutoreload(TIM6,AutoReloadVal); 
	
	TIM_ARRPreloadConfig(TIM6, ENABLE);
	
	TIM_Cmd(TIM6,ENABLE);
	
	clocksource_stm3210e.mult=clocksource_hz2mult(36000,
						clocksource_stm3210e.shift);
	clocksource_register(&clocksource_stm3210e);
}

void __init stm3210e_eval_timers_init(unsigned int timer_irq)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure=	{
		.TIM_Period = 65535,
		.TIM_Prescaler = 0,
		.TIM_ClockDivision = 0,
		.TIM_CounterMode = TIM_CounterMode_Up,
	};
	
	/* TIMx clock enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIMx, ENABLE);	
	
	/* TIMx DeInit registers */
	TIM_DeInit(TIMx);
	
  	TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);

  	/* Prescaler configuration */
  	TIM_PrescalerConfig(TIMx, 1000, TIM_PSCReloadMode_Immediate);
	
	/* 
	 * Make irqs happen for the system timer
	 */
	setup_irq(timer_irq, &stm3210e_eval_timer_irq);
	
	/* Enable the TIMx Interrupt */	
	TIM_ITConfig(TIMx, TIM_IT_Update, ENABLE);
	
#if 0
//#ifdef SYSTICK_AS_SHED_CLK
		
	SysTick_Config(72000);
	/* Enable Sched clock */
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
#endif

	/* Real time clock source Init */
	stm3210e_eval_clocksource_init();
	
	/* Setup clock events*/
	stm3210e_eval_clockevents_init(timer_irq);
}
