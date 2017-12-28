/*
 * linux/arch/m68knommu/platform/5307/ints.c -- General interrupt handling code
 *
 * Copyright (C) 1999  Greg Ungerer (gerg@snapgear.com)
 * Copyright (C) 1998  D. Jeff Dionne <jeff@ArcturusNetworks.com>
 *                     Kenneth Albanowski <kjahds@kjahds.com>,
 * Copyright (C) 2000  Lineo Inc. (www.lineo.com) 
 *
 * Based on:
 *
 * linux/arch/m68k/kernel/ints.c -- Linux/m68k general interrupt handling code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>
#include <linux/config.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/page.h>
#include <asm/machdep.h>

#define KGH 0
/*
 *	This table stores the address info for each vector handler used as
 *      fast interrupt handlers.
 */
irq_handler_t irq_list[SYS_IRQS];
#if (SYS_IRQS != NR_IRQS)
#error "SYS IRQS != NR IRQS"
#endif
/*
 *  If interrupts are not flaged with the IRQ_FLG_FAST, use this to store a
 *  linked list of possible handlers. The linked list allows sharing of
 *  interrupts.
 */
struct irq_node *std_vec_list[SYS_IRQS];
/*
 *  I cannot use kmalloc to allocate/deallocate memory as it is not ready when
 *  interrupts are first requested. I need my on allocator/deallocator of
 *  irq nodes. This is a list of irq nodes that can be used.
 */
enum { POOL_SIZE = NR_IRQS};
static struct irq_node  pool[POOL_SIZE];
static struct irq_node *get_irq_node(void);

unsigned int *mach_kstat_irqs;

/* The number of spurious interrupts */
volatile unsigned int num_spurious;

static void default_irq_handler(int irq, void *ptr, struct pt_regs *regs)
{
#if 1
	printk("%s(%d): default irq handler vec=%d [0x%x]\n",
		__FILE__, __LINE__, irq, irq);
#endif
}


#if defined(CONFIG_UCBOOTSTRAP)
 
asm (
	"\t.global _start, __ramend\n\t"
	".section .romvec\n"
"e_vectors:\n\t"
	".long _ramend-4, _start, buserr, trap, trap, trap, trap, trap\n\t"
	".long trap, trap, trap, trap, trap, trap, trap, trap\n\t"
	".long trap, trap, trap, trap, trap, trap, trap, trap\n\t"
	".long inthandler, inthandler, inthandler, inthandler\n\t"
	".long inthandler, inthandler, inthandler, inthandler\n\t"
	/* TRAP #0-15 */
	".long system_call, trap, trap, trap, trap, trap, trap, trap\n\t"
	".long trap, trap, trap, trap, trap, trap, trap, trap\n\t"
	".long 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n\t"
	".text\n"
"ignore: rte\n"
);
#endif /* CONFIG_UCBOOTSTRAP */



/*
 * void init_IRQ(void)
 *
 * Parameters:	None
 *
 * Returns:	Nothing
 *
 * This function should be called during kernel startup to initialize
 * the IRQ handling routines.
 */

void init_IRQ(void)
{
	int i;
	

	for (i = 0; i < SYS_IRQS; i++) {
		if (mach_default_handler)
			irq_list[i].handler = (*mach_default_handler)[i];
		else
			irq_list[i].handler = default_irq_handler;
		irq_list[i].flags   = IRQ_FLG_STD;
		irq_list[i].dev_id  = NULL;
		irq_list[i].devname = NULL;
		std_vec_list[i] = NULL;
	}

	if (mach_init_IRQ)
		mach_init_IRQ ();
	mach_kstat_irqs = &kstat.irqs[0][0];
	memset(pool, 0, sizeof(pool));
}

int request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *),
                unsigned long flags, const char *devname, void *dev_id)
{
	struct irq_node *action = NULL;
	struct irq_node *cur = std_vec_list[irq];

	if(dev_id == NULL)
		printk("Bad boy: %s (at 0x%p) called %s without a dev_id!\n",
				devname, handler, __FUNCTION__);
#if DAVIDM
	if ((irq & IRQ_MACHSPEC) && mach_request_irq) {
		return mach_request_irq(IRQ_IDX(irq), handler, flags,
			devname, dev_id);
	}
#endif

	if (irq < 0 || irq >= NR_IRQS) {
		printk("%s: Incorrect IRQ %d from %s\n", __FUNCTION__,
			irq, devname);
		return -ENXIO;
	}
#if KGH
	if (!(irq_list[irq].flags & IRQ_FLG_STD)) {
		if (irq_list[irq].flags & IRQ_FLG_LOCK) {
			printk("%s: IRQ %d from %s is not replaceable\n",
			       __FUNCTION__, irq, irq_list[irq].devname);
			return -EBUSY;
		}
		if (flags & IRQ_FLG_REPLACE) {
			printk("%s: %s can't replace IRQ %d from %s\n",
			       __FUNCTION__, devname, irq, irq_list[irq].devname);
			return -EBUSY;
		}
	}
#endif
	if(flags & (IRQ_FLG_LOCK | IRQ_FLG_REPLACE))
	{
		printk("%s: Coldfire does not support the IRQ_FLG_LOCK or "
				"IRQ_FLG_REPLACE flags.\n",
				__FUNCTION__);
		printk("%s: At least one of these flags was requested for "
				"%s.\n", __FUNCTION__, devname);
		flags &= (~(IRQ_FLG_LOCK | IRQ_FLG_REPLACE));
	}

	/* Check if the interrupt is busy */
	if(irq_list[irq].devname)
		return(-EBUSY);

	/* If it is a fast intterrupt, these cannot be chained */
	if (flags & IRQ_FLG_FAST) {
		extern asmlinkage void fasthandler(void);
		extern void set_evector(int vecnum, void (*handler)(void));
		/* A standard interrupt uses this interrupt already */
		if(std_vec_list[irq])
			return(-EBUSY);
		set_evector(irq, fasthandler);
		irq_list[irq].handler = handler;
		irq_list[irq].flags   = flags;
		irq_list[irq].dev_id  = dev_id;
		irq_list[irq].devname = devname;
		return 0;
	}

	if(cur)
	{
		/* This checking stuff was taken from i386 interrupt setup */
		/* Can't share interrupts unless both agree to */
		if(!(cur->flags & flags & SA_SHIRQ))
			return(-EBUSY);

		/* Can't share interrupts unless both are same type */
		if((cur->flags ^ flags) & SA_INTERRUPT)
			return(-EBUSY);
	}
	action = get_irq_node();

	if(!action)
		return(-ENOMEM);

	action->handler = handler;
	action->flags = flags;
	action->dev_id = dev_id;
	action->devname = devname;
	action->next = NULL;
	if(std_vec_list[irq])
	{
		struct irq_node *p = std_vec_list[irq];
		while(p->next)
			p = p->next;
		p->next = action;
	}
	else
	{
		std_vec_list[irq] = action;
	}
	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
	struct irq_node *p = std_vec_list[irq];
#if DAVIDM
	if (irq & IRQ_MACHSPEC) {
		mach_free_irq(IRQ_IDX(irq), dev_id);
		return;
	}
#endif

	if (irq < 0 || irq >= NR_IRQS) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	/* It was a fast interrupt */
	if(irq_list[irq].devname)
	{
		if (irq_list[irq].dev_id != dev_id)
			printk("%s: Removing probably wrong IRQ %d from %s\n",
					__FUNCTION__, irq,
					irq_list[irq].devname);

		if (irq_list[irq].flags & IRQ_FLG_FAST) {
			extern asmlinkage void inthandler(void);
			extern void set_evector(int vecnum, void (*handler)(void));
			set_evector(irq, inthandler);
			if (mach_default_handler)
				irq_list[irq].handler =
					(*mach_default_handler)[irq];
			else
				irq_list[irq].handler = default_irq_handler;
			irq_list[irq].flags   = IRQ_FLG_STD;
			irq_list[irq].dev_id  = NULL;
			irq_list[irq].devname = NULL;
			return;
		}
	}

	while(p && (p->dev_id != dev_id))
		p = p->next;
	if(!p)
	{
		printk("%s: Tried to remove wrong IRQ %d.\n",
				__FUNCTION__, irq);
		return;
	}
	if(p == std_vec_list[irq])
	{
		std_vec_list[irq] = p->next;
	}
	else
	{
		struct irq_node *q = std_vec_list[irq];
		while(q->next != p)
			q = q->next;
		q->next = p->next;
	}
	/* Release the irq node for get irq node */
	memset(p, 0, sizeof(struct irq_node));
	return;
}

#if 0
int sys_request_irq(unsigned int irq, 
                    void (*handler)(int, void *, struct pt_regs *), 
                    unsigned long flags, const char *devname, void *dev_id)
{
	if (irq < IRQ1 || irq > IRQ7) {
		printk("%s: Incorrect IRQ %d from %s\n",
		       __FUNCTION__, irq, devname);
		return -ENXIO;
	}

#if 0
	if (!(irq_list[irq].flags & IRQ_FLG_STD)) {
		if (irq_list[irq].flags & IRQ_FLG_LOCK) {
			printk("%s: IRQ %d from %s is not replaceable\n",
			       __FUNCTION__, irq, irq_list[irq].devname);
			return -EBUSY;
		}
		if (!(flags & IRQ_FLG_REPLACE)) {
			printk("%s: %s can't replace IRQ %d from %s\n",
			       __FUNCTION__, devname, irq, irq_list[irq].devname);
			return -EBUSY;
		}
	}
#endif

	irq_list[irq].handler = handler;
	irq_list[irq].flags   = flags;
	irq_list[irq].dev_id  = dev_id;
	irq_list[irq].devname = devname;
	return 0;
}

void sys_free_irq(unsigned int irq, void *dev_id)
{
	if (irq < IRQ1 || irq > IRQ7) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (irq_list[irq].dev_id != dev_id)
		printk("%s: Removing probably wrong IRQ %d from %s\n",
		       __FUNCTION__, irq, irq_list[irq].devname);

	irq_list[irq].handler = (*mach_default_handler)[irq];
	irq_list[irq].flags   = 0;
	irq_list[irq].dev_id  = NULL;
	// irq_list[irq].devname = default_names[irq];
}
#endif

/*
 * Do we need these probe functions on the m68k?
 *
 *  ... may be usefull with ISA devices
 */
unsigned long probe_irq_on (void)
{
	return 0;
}

int probe_irq_off (unsigned long irqs)
{
	return 0;
}

asmlinkage void process_int(unsigned long vec, struct pt_regs *fp)
{
	if(std_vec_list[vec])
	{
		struct irq_node *p = std_vec_list[vec];

		while(p)
		{
			p->handler(vec, p->dev_id, fp);
			p = p->next;
		}
	}
	else
	{
		++num_spurious;
		printk("Spurious interrupt %d\n", num_spurious);
	}
}


int get_irq_list(char *buf)
{
	int i, len = 0;

	for (i = 0; i < NR_IRQS; ++i)
	{
		if(irq_list[i].devname)
		{
			len += sprintf(buf+len, "%3d: %10u F ", i,
					i ? kstat.irqs[0][i] : num_spurious);
			len += sprintf(buf+len, "%s\n", irq_list[i].devname);
		}
		if(std_vec_list[i])
		{
			struct irq_node *p = std_vec_list[i];
			len += sprintf(buf+len, "%3d: %10u   ", i,
					i ? kstat.irqs[0][i] : num_spurious);
			len += sprintf(buf+len, "%s", p->devname);
			p = p->next;
			while(p)
			{
				len+=sprintf(buf+len, ",%s", p->devname);
				p = p->next;
			}
			len += sprintf(buf+len, "\n");
		}
	}
	return len;
}

struct irq_node *get_irq_node(void)
{
	size_t i;
	unsigned long flags;

	save_flags(flags); cli();
	for(i = 0; i < POOL_SIZE; ++i)
	{
		if(pool[i].devname == NULL)
		{
			pool[i].devname = (void *)0xffffffff;
			restore_flags(flags);
			return(&pool[i]);
		}
	}
	restore_flags(flags);
	printk("%s(%s:%d): No more irq nodes, I suggest you increase POOL_SIZE",
			__FUNCTION__, __FILE__, __LINE__);
	return(NULL);
}

void init_irq_proc(void);
void init_irq_proc(void)
{
	/* Insert /proc/irq driver here */
}

