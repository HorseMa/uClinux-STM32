/***********************************************************************
 * arch/armnommu/mach-dm270/time.c
 *
 *   Derived from arch/armnommu/mach-c5471/time.c
 *
 *   Copyright (C) 2004 InnoMedia Pte Ltd. All rights reserved.
 *   cheetim_loh@innomedia.com.sg  <www.innomedia.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***********************************************************************/

/* Included Files */

#include <linux/param.h>     /* For HZ */

#include <linux/spinlock.h>  /* spinlock_t needed by linux/interrupt.h */
#include <asm/system.h>      /* smp_mb() needed by linux/interrupt.h */
#include <linux/interrupt.h> /* For struct irqaction */

#include <linux/sched.h>     /* For do_timer() */

#include <asm/io.h>          /* For IO macros */
#include <asm/hardware.h>    /* For TI TMS320DM270 register definitions */
#include <asm/irq.h>         /* For IRQ_TIMER */

#include <asm/mach/irq.h>    /* For setup_arm_irq */

/* Definitions */

#define COUNTS_PER_INTERRUPT (CONFIG_ARM_CLK / (CONFIG_DM270_TIMER_PRESCALE*HZ))
#define USECS_PER_COUNT      ((1000000*CONFIG_DM270_TIMER_PRESCALE + (CONFIG_ARM_CLK >> 1))/CONFIG_ARM_CLK)

/* Global data */

extern unsigned long (*gettimeoffset)(void);

static unsigned long dm270_gettimeoffset(void)
{
	/* Compute the elapsed count since the last interrupt.
	 * Our counter counts up starting at 0.
	 */

	/* Convert the elapsed count to usecs. */
	return (unsigned long)(inw(DM270_TMR0_TMCNT)*USECS_PER_COUNT);
}

static void dm270_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	do_timer(regs);
}

void dm270_setup_timer(struct irqaction *timer_irq)
{
	u_int16_t reg;

	/* Configure timer.c so that dm270_gettimeoffset will be used
	 * to get the elapsed microseconds between interrupts.
	 */

	gettimeoffset = dm270_gettimeoffset;

	/* Capture the interrupt through dm270_timer_interrupt */

	timer_irq->handler = dm270_timer_interrupt;
	setup_arm_irq(IRQ_TIMER, timer_irq);

	/* Start the general purpose timer 0 running in auto-reload mode
	 * so that an interrupt is generated at the rate of HZ (param.h)
	 * which typically is set to 100HZ.
	 * When HZ is set to 100 then COUNTS_PER_INTERRUPT should be
	 * established to reflect timer counts that occur every 10ms
	 * (giving you 100 of them per second -- 100HZ). DM270's clock
	 * is 87.75MHz and we're using a timer prescalar divide value
	 * of 350 (350 == divide input clock frequency by 351) which then
	 * yields a 16 bit COUNTS_PER_INTERRUPT value of 2500. Prescaler
	 * divide value is chosen as 350 since DM270's clock is an
	 * integer multiple of 351.
	 */

	/* Disable clock to timer 0 */
	reg = inw(DM270_CLKC_MOD2);
	outw((reg & ~(DM270_CLKC_MOD2_RSV_MASK | DM270_CLKC_MOD2_CTMR0)), DM270_CLKC_MOD2);

	/* Select CLK_ARM as timer 0 clock source */
	reg = inw(DM270_CLKC_CLKC);
	outw((reg & ~(DM270_CLKC_CLKC_CTM0S)), DM270_CLKC_CLKC);

	/* Enable clock to timer 0 */
	reg = inw(DM270_CLKC_MOD2);
	reg &= ~(DM270_CLKC_MOD2_RSV_MASK);
	outw((reg | DM270_CLKC_MOD2_CTMR0), DM270_CLKC_MOD2);

	/* Stop timer 0 */
	reg = inw(DM270_TMR0_TMMD);
	outw((reg & ~(DM270_TMR_TMMD_MODE_MASK | DM270_TMR_TMMD_TEST_MASK | DM270_TMR_TMMD_RSV_MASK)), DM270_TMR0_TMMD);

	/* Wait till timer 0 stops??? */

	/* Set timer 0 counter */
	outw(((CONFIG_DM270_TIMER_PRESCALE - 1) & DM270_TMR_TMPRSCL_PRSCL_MASK), DM270_TMR0_TMPRSCL);
	outw((u_int16_t)((COUNTS_PER_INTERRUPT - 1) & 0xffff), DM270_TMR0_TMVAL);

	/* Set free run mode for timer 0 */
	reg = inw(DM270_TMR0_TMMD);
	reg &= ~(DM270_TMR_TMMD_MODE_MASK | DM270_TMR_TMMD_TEST_MASK | DM270_TMR_TMMD_RSV_MASK);
	outw((reg | DM270_TMR_TMMD_MODE_FREERUN), DM270_TMR0_TMMD);
}
