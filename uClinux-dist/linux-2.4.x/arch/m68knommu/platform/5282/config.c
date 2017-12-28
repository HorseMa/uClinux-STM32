/***************************************************************************/

/*
 *	linux/arch/m68knommu/platform/5282/config.c
 *
 *	Copyright (C) 1999-2003, Greg Ungerer (gerg@snapgear.com)
 *	Copyright (C) 2001-2003, SnapGear Inc. (www.snapgear.com)
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/ledman.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfpit.h>
#include <asm/mcfsim.h>
#include <asm/mcfdma.h>
#include <asm/delay.h>

/***************************************************************************/

#if defined (CONFIG_UCBOOTSTRAP)
#include <asm/uCbootstrap.h>

int    ConTTY = 0;
static int errno;
_bsc0(char *, getserialnum)
_bsc1(unsigned char *, gethwaddr, int, a)
_bsc1(char *, getbenv, char *, a)
unsigned char fec_hwaddr[6];
unsigned char *sn;
#endif




/*
 *	DMA channel base address table.
 */
unsigned int   dma_base_addr[MAX_DMA_CHANNELS] = {
        MCF_MBAR + MCFDMA_BASE0,
};

unsigned int dma_device_address[MAX_DMA_CHANNELS];

/***************************************************************************/

void coldfire_tick(void)
{
	volatile struct mcfpit	*tp;

	/* Reset the ColdFire timer */
	tp = (volatile struct mcfpit *) (MCF_IPSBAR + MCFPIT_BASE1);
	tp->pcsr |= MCFPIT_PCSR_PIF;
}

/***************************************************************************/

void coldfire_timer_init(void (*handler)(int, void *, struct pt_regs *))
{
	volatile unsigned char	*icrp;
	volatile unsigned long	*imrp;
	volatile struct mcfpit	*tp;

	tp = (volatile struct mcfpit *) (MCF_IPSBAR + MCFPIT_BASE1);
	request_irq(64+55, handler, SA_INTERRUPT, "ColdFire Timer", tp);

	icrp = (volatile unsigned char *) (MCF_IPSBAR + MCFICM_INTC0 +
		MCFINTC_ICR0 + MCFINT_PIT1);
	*icrp = 0x2b; /* PIT1 with level 5, priority 3 */

	imrp = (volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRH);
	*imrp &= ~(1 << (55 - 32));

	/* Set up PIT timer 1 as poll clock */
	tp->pcsr = MCFPIT_PCSR_DISABLE;

	tp->pmr = ((MCF_CLK / 2) / 64) / HZ;
	tp->pcsr = MCFPIT_PCSR_EN | MCFPIT_PCSR_PIE | MCFPIT_PCSR_OVW |
		MCFPIT_PCSR_RLD | MCFPIT_PCSR_CLK64;
}

/***************************************************************************/

unsigned long coldfire_timer_offset(void)
{
	volatile struct mcfpit *tp;
	volatile unsigned long *ipr;
	unsigned long pmr, pcntr, offset;

	tp = (volatile struct mcfpit *) (MCF_IPSBAR + MCFPIT_BASE1);
	ipr = (volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IPRH);

	pmr = *(&tp->pmr);
	pcntr = *(&tp->pcntr);

	/*
	 * If we are still in the first half of the upcount and a
	 * timer interupt is pending, then add on a ticks worth of time.
	 */
	offset = ((pmr - pcntr) * (1000000 / HZ)) / pmr;
	if (((offset * 2) < (1000000 / HZ)) && (*ipr & (1 << (55 - 32))))
		offset += 1000000 / HZ;
	return offset;	
}

/***************************************************************************/

/*
 *	Program the vector to be an auto-vectored.
 */

void mcf_autovector(unsigned int vec)
{
	/* Everything is auto-vectored on the 5282 */
}

/***************************************************************************/

extern e_vector	*_ramvec;

void set_evector(int vecnum, void (*handler)(void))
{
	if (vecnum >= 0 && vecnum <= 255)
		_ramvec[vecnum] = handler;
}

/***************************************************************************/

/* assembler routines */
asmlinkage void buserr(void);
asmlinkage void trap(void);
asmlinkage void system_call(void);
asmlinkage void inthandler(void);

void coldfire_trap_init(void)
{
	int i;

	*((volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRH)) = 0xffffffff;
	*((volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRL)) = 0xffffffff;

	/*
	 *	There is a common trap handler and common interrupt
	 *	handler that handle almost every vector. We treat
	 *	the system call and bus error special, they get their
	 *	own first level handlers.
	 */
#ifndef ENABLE_dBUG
	for (i = 3; (i <= 23); i++)
		_ramvec[i] = trap;
	_ramvec[33] = trap;
	/* skip trap #2; this is used as the uCbootloader callback */
#if !defined(CONFIG_UCBOOTSTRAP)
	_ramvec[34] = trap;
#endif
	for (i = 35; (i <= 63); i++)
		_ramvec[i] = trap;
#endif

	for (i = 24; (i <= 30); i++)
		_ramvec[i] = inthandler;
#ifndef ENABLE_dBUG
	_ramvec[31] = inthandler;  // Disables the IRQ7 button
#endif

	for (i = 64; (i < 255); i++)
		_ramvec[i] = inthandler;
	_ramvec[255] = 0;

	_ramvec[2] = buserr;
	_ramvec[32] = system_call;
}

/***************************************************************************/

/*
 *	Generic dumping code. Used for panic and debug.
 */

void dump(struct pt_regs *fp)
{
	extern unsigned int sw_usp, sw_ksp;
	unsigned long	*sp;
	unsigned char	*tp;
	int		i;

	printk("\nCURRENT PROCESS:\n\n");
	printk("COMM=%s PID=%d\n", current->comm, current->pid);

	if (current->mm) {
		printk("TEXT=%08x-%08x DATA=%08x-%08x BSS=%08x-%08x\n",
			(int) current->mm->start_code,
			(int) current->mm->end_code,
			(int) current->mm->start_data,
			(int) current->mm->end_data,
			(int) current->mm->end_data,
			(int) current->mm->brk);
		printk("USER-STACK=%08x  KERNEL-STACK=%08x\n\n",
			(int) current->mm->start_stack,
			(int) ((unsigned long) current) + KTHREAD_SIZE);
	}

	printk("PC: %08lx\n", fp->pc);
	printk("SR: %08lx    SP: %08lx\n", (long) fp->sr, (long) fp);
	printk("d0: %08lx    d1: %08lx    d2: %08lx    d3: %08lx\n",
		fp->d0, fp->d1, fp->d2, fp->d3);
	printk("d4: %08lx    d5: %08lx    a0: %08lx    a1: %08lx\n",
		fp->d4, fp->d5, fp->a0, fp->a1);
	printk("\nUSP: %08x   KSP: %08x   TRAPFRAME: %08x\n",
		sw_usp, sw_ksp, (unsigned int) fp);

	printk("\nCODE:");
	tp = ((unsigned char *) fp->pc) - 0x20;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x40);  i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nKERNEL STACK:");
	tp = ((unsigned char *) fp) - 0x40;
	for (sp = (unsigned long *) tp, i = 0; (i < 0xc0); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");
	printk("\n");

	printk("\nUSER STACK:");
	tp = (unsigned char *) (sw_usp - 0x10);
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n\n");
}

/***************************************************************************/

void coldfire_reset(void)
{
	HARD_RESET_NOW();
}

/***************************************************************************/

void config_BSP(char *commandp, int size)
{
#ifdef CONFIG_BOOTPARAM
	strncpy(commandp, CONFIG_BOOTPARAM_STRING, size);
	commandp[size-1] = 0;

#elif defined(CONFIG_UCBOOTSTRAP)
	unsigned char *p;
	printk("Arcturus Networks Inc. MCF528X uCbootstrap systemcalls\n");
	printk("   Serial number: [%s]\n", getserialnum());

	p = gethwaddr(0);
	memcpy (fec_hwaddr, p, 6);
	if (p == NULL) {
		memcpy (fec_hwaddr, "\0x00\0xde\0xad\0xbe\0xef\0x27", 6);
		p = fec_hwaddr;
		printk("Warning: HWADDR0 environment variable not set. Using default.\n");
	}
	printk("   Ethernet addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
		p[0], p[1], p[2], p[3], p[4], p[5]);

	p = getbenv("CONSOLE");
	if (p != NULL) {
		sprintf(commandp, " console=%s", p);
		printk("   Console dev  : %s", p);
	}

	p = getbenv("CONSOLE_SPEED");
	if (p != NULL) {
		strcat(commandp, ",");
		strcat(commandp, p);
		printk("   Console speed: %s", p);
	}
	strcat(commandp, " ");

	p = getbenv("KERNEL_ARGS");
	if (p != NULL)
		strcat(commandp, p);

	p = getbenv("KERNEL_ARGS1");
	if (p != NULL) {
		strcat(commandp, " ");
		strcat(commandp, p);
	}
#else
	memset(commandp, 0, size);
#endif

	mach_sched_init = coldfire_timer_init;
	mach_tick = coldfire_tick;
	mach_trap_init = coldfire_trap_init;
	mach_reset = coldfire_reset;
	mach_gettimeoffset = coldfire_timer_offset;
}

/***************************************************************************/
