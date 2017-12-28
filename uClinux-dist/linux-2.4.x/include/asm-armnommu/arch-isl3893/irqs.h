/*
 *  include/asm-armnommu/arch-isl3893/irqs.h
 *
 *  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

#define IRQ_STWDOG		0
#define IRQ_SOFINT		1
#define IRQ_RTC			2
#define IRQ_PCIRESETINT		3
#define IRQ_GP1FIQINT		4
#define IRQ_GP2FIQINT		5
#define IRQ_PCIINT		6
#define IRQ_BPINT		7
#define IRQ_UART		8
#define IRQ_HOSTTXDMA		9
#define IRQ_HOSTRXDMA		10
#define IRQ_GP1IRQINT		11
#define IRQ_GP2IRQINT		12
#define IRQ_BPTXDMA		13
#define IRQ_BPRXDMA		14
#define IRQ_M2MTXDMA		15
#define IRQ_M2MRXDMA		16
#define IRQ_TIM0INT		17
#define IRQ_TIM1INT		18
#define IRQ_TIM2INT		19
#define IRQ_WEPINT		20
#define IRQ_COMMRX		21
#define IRQ_COMMTX		22
#define IRQ_ETH1		23
#define IRQ_ETH2		24

#define NR_IRQS			( IRQ_ETH2 + 1)

#endif
