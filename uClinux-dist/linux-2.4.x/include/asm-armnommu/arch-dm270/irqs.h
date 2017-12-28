/***********************************************************************
 * include/asm-armnommu/arch-dm270/irqs.h
 *
 *   Derived from asm/arch-armnommu/arch-c5471/irqs.h
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


#ifndef __ASM_ARCH_IRQS_H__
#define __ASM_ARCH_IRQS_H__

#include <linux/config.h>

#define DM270_INTERRUPT_TMR0           0
#define DM270_INTERRUPT_TMR1           1
#define DM270_INTERRUPT_TMR2           2
#define DM270_INTERRUPT_TMR3           3
#define DM270_INTERRUPT_CCDVD0         4
#define DM270_INTERRUPT_CCDVD1         5
#define DM270_INTERRUPT_CCDVD2         6
#define DM270_INTERRUPT_OSD            7
#define DM270_INTERRUPT_SP0            8
#define DM270_INTERRUPT_SP1            9
#define DM270_INTERRUPT_EXTHOST       10
#define DM270_INTERRUPT_DSPHINT       11
#define DM270_INTERRUPT_UART0         12
#define DM270_INTERRUPT_UART1         13
#define DM270_INTERRUPT_USB           14
#define DM270_INTERRUPT_MTC0          15
#define DM270_INTERRUPT_MTC1          16
#define DM270_INTERRUPT_MMCSD0        17
#define DM270_INTERRUPT_MMCSD1        18
#define DM270_INTERRUPT_RSVINT0       19
#define DM270_INTERRUPT_EXT1          20
#define DM270_INTERRUPT_EXT2          21
#define DM270_INTERRUPT_EXT3          22
#define DM270_INTERRUPT_EXT4          23
#define DM270_INTERRUPT_EXT5          24
#define DM270_INTERRUPT_EXT6          25
#define DM270_INTERRUPT_EXT7          26
#define DM270_INTERRUPT_EXT8          27
#define DM270_INTERRUPT_EXT9          28
#define DM270_INTERRUPT_EXT10         29
#define DM270_INTERRUPT_EXT11         30
#define DM270_INTERRUPT_EXT12         31
#define DM270_INTERRUPT_EXT13         32
#define DM270_INTERRUPT_EXT14         33
#define DM270_INTERRUPT_EXT15         34
#define DM270_INTERRUPT_PREV          35
#define DM270_INTERRUPT_WDT           36
#define DM270_INTERRUPT_I2C           37
#define DM270_INTERRUPT_CLKC          38
/*#define DM270_INTERRUPT_RSVINT1       39*/

#define IRQ_TIMER                     (DM270_INTERRUPT_TMR0)
#define NR_IRQS                       (DM270_INTERRUPT_CLKC + 1)

/* flags for request_irq().  IRQ_FLG_STD is apparently a uClinux-ism,
 * so I'll keep it around even though it is just a mapping for the
 * real SA_INTERRUPT. -- skj 
 */

#define IRQ_FLG_STD                   (SA_INTERRUPT)

#endif /* __ASM_ARCH_IRQS_H__ */
