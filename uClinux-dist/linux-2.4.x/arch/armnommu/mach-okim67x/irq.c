/*
 *  linux/arch/arm/mach-okim67x/irq.c
 *
 *  Copyright (C) 2005 Simtec Electronics
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
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

extern struct irqdesc irq_desc[];

#define IS_ILC0 (0 << 24)
#define IS_ILC1 (1 << 24)
#define IS_ILC  (2 << 24)

#define IS_ILR0 (0x00)
#define IS_ILR1 (0x04)
#define IS_ILR4 (0x10)
#define IS_ILR6 (0x18)
#define IS_ILR8 (0x00)
#define IS_ILR9 (0x04)
#define IS_ILR10 (0x08)
#define IS_ILR11 (0x0C)
#define IS_ILR12 (0x10)
#define IS_ILR13 (0x14)
#define IS_ILR14 (0x18)
#define IS_ILR15 (0x1C)
#define IS_ILC16 (0x00)
#define IS_ILC18 (0x04)
#define IS_ILC20 (0x08)
#define IS_ILC22 (0x0c)
#define IS_ILC24 (0x10)
#define IS_ILC26 (0x14)
#define IS_ILC28 (0x18)
#define IS_ILC30 (0x1c)

#define PRIO(x)  ((x) << 16)

#define __arch_writel(data,addr) \
  do { *((volatile unsigned int *)(addr)) = (data); } while(0)

#define __arch_readl(addr) ((*((volatile unsigned int *)(addr))))

static unsigned int irqtab[32] = {
  IS_ILC0 | PRIO(1) | IS_ILR0,             /* 0 */
  IS_ILC0 | PRIO(1) | IS_ILR1,             /* 1 */
  IS_ILC0 | PRIO(1) | IS_ILR1,             /* 2 */
  IS_ILC0 | PRIO(1) | IS_ILR1,             /* 3 */
  IS_ILC0 | PRIO(1) | IS_ILR4,             /* 4 */
  IS_ILC0 | PRIO(1) | IS_ILR4,             /* 5 */
  IS_ILC0 | PRIO(1) | IS_ILR6,             /* 6 */
  IS_ILC0 | PRIO(1) | IS_ILR6,             /* 7 */
  IS_ILC1 | PRIO(1) | IS_ILR8,             /* 8 */
  IS_ILC1 | PRIO(1) | IS_ILR9,             /* 9 */
  IS_ILC1 | PRIO(5) | IS_ILR10,            /* 10 */
  IS_ILC1 | PRIO(1) | IS_ILR11,            /* 11 */
  IS_ILC1 | PRIO(1) | IS_ILR12,            /* 12 */
  IS_ILC1 | PRIO(1) | IS_ILR13,            /* 13 */
  IS_ILC1 | PRIO(1) | IS_ILR14,            /* 14 */
  IS_ILC1 | PRIO(1) | IS_ILR15,            /* 15 */
  IS_ILC  | PRIO(1) | IS_ILC16,            /* 16 */
  IS_ILC  | PRIO(1) | IS_ILC16,            /* 17 */
  IS_ILC  | PRIO(1) | IS_ILC18,            /* 18 */
  IS_ILC  | PRIO(1) | IS_ILC18,            /* 19 */
  IS_ILC  | PRIO(1) | IS_ILC20,            /* 20 */
  IS_ILC  | PRIO(1) | IS_ILC20,            /* 21 */
  IS_ILC  | PRIO(1) | IS_ILC22,            /* 22 */
  IS_ILC  | PRIO(1) | IS_ILC22,            /* 23 */
  IS_ILC  | PRIO(1) | IS_ILC24,            /* 24 */
  IS_ILC  | PRIO(1) | IS_ILC24,            /* 25 */
  IS_ILC  | PRIO(1) | IS_ILC26,            /* 26 */
  IS_ILC  | PRIO(1) | IS_ILC26,            /* 27 */
  IS_ILC  | PRIO(1) | IS_ILC28,            /* 28 */
  IS_ILC  | PRIO(1) | IS_ILC28,            /* 29 */
  IS_ILC  | PRIO(1) | IS_ILC30,            /* 30 */
  IS_ILC  | PRIO(1) | IS_ILC30,            /* 31 */
};

static int irqregs[] = {
  OKI_ILC0,
  OKI_ILC1,
  OKI_ILC
};

#define irqreg(x) (irqregs[ ((x) >> 24)] )
#define irqpos(x) ((x) & 0xff)
#define irqprio(x) (((x) >> 16) & 0xff)

void m67x_mask_irq(unsigned int irq)
{
  unsigned int irqinf = irqtab[irq];
  unsigned int irqreg = irqreg(irqinf);
  unsigned int tmp;

  //printk("mask %d\n", irq);

  tmp = __arch_readl(irqreg);
  tmp &= ~(irqprio(irqinf) << irqpos(irqinf));

  __arch_writel(tmp, irqreg);
}

void m67x_unmask_irq(unsigned int irq)
{
  unsigned int irqinf = irqtab[irq];
  unsigned int irqreg = irqreg(irqinf);
  unsigned int tmp;

  //printk("unmask %d\n", irq);

  tmp = __arch_readl(irqreg);
  tmp &= ~(irqprio(irqinf) << irqpos(irqinf));
  tmp |= irqprio(irqinf) << irqpos(irqinf);

  //printk("unask: r=%08x, v=%08x\n", irqreg, tmp);

  __arch_writel(tmp, irqreg);
}

void m67x_mask_ack_irq(unsigned int irq)
{
  unsigned int irqinf = irqtab[irq];
  unsigned int irqreg = irqreg(irqinf);

  //printk("mask ack %d\n", irq);

  m67x_mask_irq(irq);

  __arch_writel(1<<(irqprio(irqinf) - 1), OKI_CILCL);

}

void m67x_init_aic(void)
{
  printk("clearing irq controller\n");

  __arch_writel(0x00, OKI_ILC0);
  __arch_writel(0x00, OKI_ILC1);

}
