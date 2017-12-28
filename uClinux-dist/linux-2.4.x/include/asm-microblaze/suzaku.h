/*  include/asm-microblaze/suzaku.h
 *
 *  (C) 2004 Atmark Techno, Inc. <yashi@atmark-techno.com>
 *  (C) 2004 John Williams <jwilliams@itee.uq.edu.au>
 *
 *  Defines board specific stuff for SUZAKU
 *
 */

#ifndef _ASM_MICROBLAZE_SUZAKU_H
#define _ASM_MICROBLAZE_SUZAKU_H

#ifndef HZ
#define HZ 100
#endif

/* System Register (GPIO) */
#ifndef MICROBLAZE_SYSREG_BASE_ADDR
#define MICROBLAZE_SYSREG_BASE_ADDR 0xFFFFA000
#define MICROBLAZE_SYSREG_RECONFIGURE (1 << 0)
#endif

#define NUM_CPU_IRQS 32

#endif /* _ASM_MICROBLAZE_SUZAKU_H */

