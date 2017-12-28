/*
 * uclinux/include/asm-armnommu/arch-isl3893/hardware.h
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <linux/config.h>
#include <asm/arch/intctl.h>
#include <asm/arch/gpio.h>
#include <asm/arch/rtc.h>

#ifndef __ASSEMBLER__

#define HARD_RESET_NOW()  { arch_reset('h'); }
#define ERROR_RESET_NOW()  { arch_reset('e'); }

typedef unsigned long u_32;

/* ARM asynchronous clock */
#define ARM_CLK ((u_32)(CONFIG_ARM_CLK))

/* ARM synchronous with OAK clock */
#define A_O_CLK ((u_32)(CONFIG_A_O_CLK))

#else
#define ARM_CLK CONFIG_ARM_CLK
#define A_O_CLK CONFIG_A_O_CLK
#endif

/*
 * Memory definitions
 */

#define MAPTOPHYS(a)      ((unsigned long)a)
#define KERNTOPHYS(a)     ((unsigned long)(&a))

/*
 * Base address for all Peripherals
 */

#define PERIPH_BASE         ( 0xc0000000 )  /* used by entry-armv.S */

#define PCIBIOS_MIN_IO		( 0x1000 )
#define PCIBIOS_MIN_MEM 	( PERIPH_BASE + 0x980 )

#ifdef ISL3893_SIMSONLY
/*
 * TUBE
 */
#ifndef __ASSEMBLER__

#define TUBEData                        (0x0)
#define TUBEDataMask                    (0xf)
#define TUBEDataAccessType              (NO_TEST)
#define TUBEDataInitialValue            (0x0)
#define TUBEDataTestMask                (0xf)

/* Instance values */
#define asicTUBEData                    (0x7fffffc)

/* C struct view */

typedef struct s_TUBE {
 unsigned char Data; /* @0 */
} s_TUBE;

#define uTUBE ((volatile struct s_TUBE *) 0x7fffffc)

#else /* __ASSEMBLER__ */
#define uTUBEData       0x7fffffc
#endif /* __ASSEMBLER__ */

#endif /* ISL3893_SIMSONLY */

#endif /* __ASM_ARCH_HARDWARE_H */
