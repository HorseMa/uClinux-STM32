/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2008 Cavium Networks
 */
#include "linux/irq.h"
#include "linux/hardirq.h"
#include "linux/kernel_stat.h"
#include "hal.h"

asmlinkage void plat_irq_dispatch(void)
{
	const unsigned long core_id = cvmx_get_core_num();
	const uint64_t ciu_sum0_address = CVMX_CIU_INTX_SUM0(core_id * 2);
	const uint64_t ciu_en0_address = CVMX_CIU_INTX_EN0(core_id * 2);
	const uint64_t ciu_sum1_address = CVMX_CIU_INT_SUM1;
	const uint64_t ciu_en1_address = CVMX_CIU_INTX_EN1(core_id * 2 + 1);
	unsigned long cop0_cause;
	unsigned long cop0_status;
	uint64_t ciu_en;
	uint64_t ciu_sum;

	while (1) {
		cop0_cause = read_c0_cause();
		cop0_status = read_c0_status();
		cop0_cause &= cop0_status;
		cop0_cause &= ST0_IM;

		if (unlikely(cop0_cause & STATUSF_IP2)) {
			asm volatile ("ld	%[sum], 0(%[sum_address])\n"
				      "ld	%[en], 0(%[en_address])\n" :
				      [sum] "=r"(ciu_sum),
				      [en] "=r"(ciu_en) :
				      [sum_address] "r"(ciu_sum0_address),
				      [en_address] "r"(ciu_en0_address));
			ciu_sum &= ciu_en;
			if (likely(ciu_sum))
				do_IRQ(fls64(ciu_sum) + OCTEON_IRQ_WORKQ0 - 1);
			else
				spurious_interrupt();
		} else if (unlikely(cop0_cause & STATUSF_IP3)) {
			asm volatile ("ld	%[sum], 0(%[sum_address])\n"
				      "ld	%[en], 0(%[en_address])\n" :
				      [sum] "=r"(ciu_sum),
				      [en] "=r"(ciu_en) :
				      [sum_address] "r"(ciu_sum1_address),
				      [en_address] "r"(ciu_en1_address));
			ciu_sum &= ciu_en;
			if (likely(ciu_sum))
				do_IRQ(fls64(ciu_sum) + OCTEON_IRQ_WDOG0 - 1);
			else
				spurious_interrupt();
		} else if (likely(cop0_cause)) {
			do_IRQ(fls(cop0_cause) - 9);
		} else {
			break;
		}
	}
}
