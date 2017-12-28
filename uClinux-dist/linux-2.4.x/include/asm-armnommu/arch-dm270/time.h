/***********************************************************************
 * linux/include/asm-arm/arch-dm270/time.h
 *
 *   Derived from asm/arch-armnommu/arch-c5471/time.h
 *
 *   Copyright (C) 2004 InnoMedia Pte Ltd. All rights reserved.
 *   cheetim_loh@innomedia.com.sg  <www.innomedia.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
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

#ifndef __ASM_ARCH_TIME_H__
#define __ASM_ARCH_TIME_H__

/* The actual timer initialization is performed in the file
 * arch/armnommu/mach-dm270/time.c.  This inline function is
 * just a thin candy coating to get to that function.
 */

extern void dm270_setup_timer(struct irqaction *timer_irq);

/* setup_timer() is "called" from arch/armnommu/kernel/time.c
 * In the context where setup_timer runs (in this context, timer_irq
 * is declared as a static intialized variable.  Provided that
 * the asm/arch/time.h is included AFTER this declaration, the
 * following should be quite happy:
 */

void __inline__ setup_timer(void)
{
  dm270_setup_timer(&timer_irq);
}

#endif /* __ASM_ARCH_TIME_H__ */
