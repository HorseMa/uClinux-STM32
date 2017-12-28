/***********************************************************************
 * arch/armnommu/mach-dm270/irq.c
 *
 *   TI TMS320DM270 IRQ mask, unmask & ack routines.
 *
 *   Derived from arch/armnommu/mach-c5471/irq.c
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

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/irq.h>

#include <asm/mach/irq.h>

#include <asm/arch/irq.h>
#include <asm/arch/hardware.h>

/* Acknowlede the IRQ. */

static inline void irq_ack(unsigned int irq)
{
	outw((1<<DM270_INTC_IRQ_SHIFT(irq)), DM270_INTC_IRQ(irq));
}

/* Acknowledge the FIQ. */

static inline void fiq_ack(unsigned int irq)
{
	outw((1<<DM270_INTC_FIQ_SHIFT(irq)), DM270_INTC_FIQ(irq));
}

/* Mask the IRQ. */

void dm270_mask_irq(unsigned int irq)
{
	u_int32_t eint;
	u_int16_t mask;

	eint = DM270_INTC_EINT(irq);
	mask = inw(eint);
	mask &= ~(1<<DM270_INTC_EINT_SHIFT(irq));
	outw(mask, eint);
}

/* Unmask the IRQ. */

void dm270_unmask_irq(unsigned int irq)
{
	u_int32_t eint;
	u_int16_t mask;

	eint = DM270_INTC_EINT(irq);
	mask = inw(eint);
	mask |= (1<<DM270_INTC_EINT_SHIFT(irq));
	outw(mask, eint);
}

/* Mask the IRQ and acknowledge it. */

void dm270_mask_ack_irq(unsigned int irq)
{
	u_int32_t eint;
	u_int16_t mask;

	eint = DM270_INTC_EINT(irq);
	mask = inw(eint);
	mask &= ~(1<<DM270_INTC_EINT_SHIFT(irq));
	outw(mask, eint);

	outw((1<<DM270_INTC_IRQ_SHIFT(irq)), DM270_INTC_IRQ(irq));
}

void dm270_init_irq(void)
{
	/* Disable all interrupts */

	outw(0, DM270_INTC_EINT0);
	outw(0, DM270_INTC_EINT1);
	outw(0, DM270_INTC_EINT2);

	/* Set all to IRQ mode, not FIQ */

	outw(0, DM270_INTC_FISEL0);
	outw(0, DM270_INTC_FISEL1);
	outw(0, DM270_INTC_FISEL2);

	/* Clear any pending interrupts */

	outw(0xffff, DM270_INTC_IRQ0);
	outw(0xffff, DM270_INTC_IRQ1);
	outw(0xff, DM270_INTC_IRQ2);
	outw(0xffff, DM270_INTC_FIQ0);
	outw(0xffff, DM270_INTC_FIQ1);
	outw(0xff, DM270_INTC_FIQ2);
}
