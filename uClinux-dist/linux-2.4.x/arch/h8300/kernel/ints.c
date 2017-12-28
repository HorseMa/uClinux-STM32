/*
 * linux/arch/h8300/kernel/ints.c
 * Interrupt handling common routines.
 *
 * Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * Based on linux/arch/$(ARCH)/platform/$(PLATFORM)/ints.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Copyright 1996 Roman Zippel
 * Copyright 1999 D. Jeff Dionne <jeff@uClinux.org>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/bootmem.h>
#include <linux/random.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/gpio.h>
#include <asm/hardirq.h>

/* table for system interrupt handlers */
static irq_handler_t *irq_list[NR_IRQS];
static int use_kmalloc;

extern unsigned long *interrupt_redirect_table;
extern const int h8300_saved_vectors[];
extern const unsigned long h8300_trap_table[];

#define CPU_VECTOR ((unsigned long *)0x000000)
#define ADDR_MASK (0xffffff)

int h8300_enable_irq_pin(unsigned int irq);
void h8300_disable_irq_pin(unsigned int irq);

static inline unsigned long *get_vector_address(void)
{
	unsigned long *rom_vector = CPU_VECTOR;
	unsigned long base,tmp;
	int vec_no;

	base = rom_vector[EXT_IRQ0] & ADDR_MASK;
	
	/* check romvector format */
	for (vec_no = EXT_IRQ1; vec_no <= EXT_IRQ0+EXT_IRQS; vec_no++) {
		if ((base+(vec_no - EXT_IRQ0)*4) != (rom_vector[vec_no] & ADDR_MASK))
			return NULL;
	}

	/* ramvector base address */
	base -= EXT_IRQ0*4;

	/* writerble check */
	tmp = ~(*(volatile unsigned long *)base);
	(*(volatile unsigned long *)base) = tmp;
	if ((*(volatile unsigned long *)base) != tmp)
		return NULL;
	return (unsigned long *)base;
}

void __init init_IRQ(void)
{
#if defined(CONFIG_RAMKERNEL)
	int i;
	unsigned long *ramvec,*ramvec_p;
	const unsigned long *trap_entry;
	const int *saved_vector;

	ramvec = get_vector_address();
	if (ramvec == NULL)
		panic("interrupt vector serup failed.");
	else
		printk("virtual vector at 0x%08lx\n",(unsigned long)ramvec);

	/* create redirect table */
	ramvec_p = ramvec;
	trap_entry = h8300_trap_table;
	saved_vector = h8300_saved_vectors;
	for ( i = 0; i < NR_TRAPS; i++) {
		if (i == *saved_vector) {
			ramvec_p++;
			saved_vector++;
		} else {
			if (*trap_entry)
				*ramvec_p++ = VECTOR(*trap_entry++);
			else {
				ramvec_p++;
				trap_entry++;
			}
		}
	}
	for ( ; i < NR_IRQS; i++)
		if (i == *saved_vector) {
			ramvec_p++;
			saved_vector++;
		} else
			*ramvec_p++ = REDIRECT(interrupt_entry);
	interrupt_redirect_table = ramvec;
#ifdef DUMP_VECTOR
	ramvec_p = ramvec;
	for (i = 0; i < NR_IRQS; i++) {
		if ((i % 8) == 0)
			printk("\n%p: ",ramvec_p);
		printk("%p ",*ramvec_p);
		ramvec_p++;
	}
	printk("\n");
#endif
#endif
}

int request_irq(unsigned int irq, 
		void (*handler)(int, void *, struct pt_regs *),
                unsigned long flags, const char *devname, void *dev_id)
{
	irq_handler_t *irq_handle;
	if (irq < 0 || irq >= NR_IRQS) {
		printk("Incorrect IRQ %d from %s\n", irq, devname);
		return -EINVAL;
	}
	if (irq_list[irq] || (h8300_enable_irq_pin(irq) == -EBUSY))
		return -EBUSY;  /* already used */
	if(use_kmalloc)
		irq_handle = (irq_handler_t *)kmalloc(sizeof(irq_handler_t), GFP_ATOMIC);
	else {
		irq_handle = (irq_handler_t *)alloc_bootmem(sizeof(irq_handler_t));
		irq_handle = ((irq_handler_t *)
			      ((unsigned long) irq_handle | 0x80000000UL));
	}

	if (irq_handle == NULL)
		return -ENOMEM;

	irq_handle->handler = handler;
	irq_handle->flags   = flags;
	irq_handle->count   = 0;
	irq_handle->dev_id  = dev_id;
	irq_handle->devname = devname;
	irq_list[irq] = irq_handle;

	if (irq_handle->flags & SA_SAMPLE_RANDOM)
		rand_initialize_irq(irq);

	/* enable interrupt */
	/* compatible i386  */
	enable_irq(irq);
	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
	if (irq >= NR_IRQS) {
		return;
	}
	if (!irq_list[irq] || irq_list[irq]->dev_id != dev_id)
		printk("Removing probably wrong IRQ %d from %s\n",
		       irq, irq_list[irq]->devname);
	/* disable interrupt & release IRQ pin */
	h8300_disable_irq_pin(irq);
	if (((unsigned long)irq_list[irq] & 0x80000000UL) == 0) {
		kfree(irq_list[irq]);
		irq_list[irq] = NULL;
	}
}

void enable_irq(unsigned int irq)
{
	if (irq >= EXT_IRQ0 && irq <= (EXT_IRQ0 + EXT_IRQS))
		IER_REGS |= 1 << (irq - EXT_IRQ0);
}

void disable_irq(unsigned int irq)
{
	if (irq >= EXT_IRQ0 && irq <= (EXT_IRQ0 + EXT_IRQS))
		IER_REGS &= ~(1 << (irq - EXT_IRQ0));
}

/*
 * Do we need these probe functions on the m68k?
 */
unsigned long probe_irq_on (void)
{
	return 0;
}

int probe_irq_off (unsigned long irqs)
{
	return 0;
}

asmlinkage void process_int(int irq, struct pt_regs *fp)
{
	irq_enter(0,0);
	/* ISR clear       */
	/* compatible i386 */
	h8300_clear_isr(irq);
	if (irq < NR_IRQS) {
		if (irq_list[irq]) {
			irq_list[irq]->handler(irq, irq_list[irq]->dev_id, fp);
			irq_list[irq]->count++;
			if (irq_list[irq]->flags & SA_SAMPLE_RANDOM)
				add_interrupt_randomness(irq);
		}
	} else {
		BUG();
	}
	irq_exit(0,0);
}

int get_irq_list(char *buf)
{
	int i, len = 0;

	for (i = 0; i < NR_IRQS; i++) {
		if (irq_list[i]) {
			len += sprintf(buf+len, "irq %2d: %10u ", i,
			               irq_list[i]->count);
			len += sprintf(buf+len, "%s\n", irq_list[i]->devname);
		}
	}
	return len;
}

void init_irq_proc(void)
{
}

static int __init enable_kmalloc(void)
{
	use_kmalloc = 1;
	return 0;
}
__initcall(enable_kmalloc);
