#ifndef	__asm_crunch_h__
#define	__asm_crunch_h__

#include "asm/arch/regmap.h"

#define	CRUNCH_INIT	0x00900000
#define	CRUNCH_IRQ	58

#ifndef	__ASSEMBLY__

/* enable the MaverickCrunch clock */
static inline void crunch_enable(void)
{
	int tmp, aa = 0xAA;
	int *lock = (int *)SYSCON_SWLOCK;
	int *syscfg = (int *)SYSCON_DEVCFG;

	asm volatile (
	"str    %3, [%1]\n\t"
	"ldr    %0, [%2]\n\t"
	"orr    %0, %0, #(1 << 23)\n\t"
	"str    %0, [%2]"
	:"=r" (tmp)
	:"r" (lock), "r" (syscfg), "r" (aa)
	:"memory" );
}


/* disable the MaverickCrunch clock */
static inline void crunch_disable(void)
{
	int tmp, aa = 0xAA;
	int *lock = (int *)SYSCON_SWLOCK;
	int *syscfg = (int *)SYSCON_DEVCFG;

	asm volatile (
	"str    %3, [%1]\n\t"
	"ldr    %0, [%2]\n\t"
	"bic    %0, %0, #(1 << 23)\n\t"
	"str    %0, [%2]"
	:"=r" (tmp)
	:"r" (lock), "r" (syscfg), "r" (aa)
	:"memory" );
}


unsigned int read_dspsc_low(void);
unsigned int read_dspsc_high(void);
void write_dspsc(unsigned int);
void save_crunch(struct task_struct *);
void restore_crunch(struct task_struct *);
int setup_crunch(void);
void crunch_init(void);
#endif

#endif	/* __asm_crunch_h__ */
