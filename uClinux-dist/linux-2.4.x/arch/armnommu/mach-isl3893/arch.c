/*  $Header$
 *
 *  linux/arch/arm/mach-isl3893/arch.c
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  Architecture specific fixups.  This is where any
 *  parameters in the params struct are fixed up, or
 *  any additional architecture specific information
 *  is pulled from the params struct.
 */
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/arch/serial.h>
#include <asm/arch/gpio.h>
#include <asm/arch/blobcalls.h>

extern void genarch_init_irq(void);
extern void init_ram_mpu_area(unsigned long , unsigned long );

/* These are initialized in head-armv.S */
struct boot_struct *bootstruct = NULL;
struct boot_parms *boot_param_block = NULL;

struct param_struct common_params = { { {
	page_size:		4096,
	nr_pages:		0,
	flags:    		FLAG_RDLOAD,
	rootdev:		0
} } };

static void __init
fixup_isl3893(struct machine_desc *desc, struct param_struct *params,
             char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_DEBUG_LL
	uart_initialize();
#endif

	// MTHU: debugging - remove
	printk("Boot Struct at %p\n", bootstruct);
	printk("Boot parameter block at %p\n", boot_param_block);
    // MTHU: debugging - remove

    /* The bootrom uses Ethernet buffers in the area 0x07ff7b40 to 0x08000000
     * When these buffers are flushed by the MVC, this memory should not be used
     * by the kernel
     */
    if((bootstruct->sram_size & 0xffff) > 0x7b40) {
        bootstruct->sram_size = (bootstruct->sram_size & 0xffff0000) + 0x7b40;
    }
    printk("SRAM size 0x%lx\n", bootstruct->sram_size);

#ifndef ISL3893_NO_MVC
	if(!bootstruct || bootstruct->magic != BOOTMAGIC_BSv2) {
		printk("Invalid bootstruct %p (magic %lx)\n", bootstruct, bootstruct->magic);
		panic("Invalid Bootstruct");
	}
	params->u1.s.nr_pages = bootstruct->sram_size >> PAGE_SHIFT;

	if(!boot_param_block || boot_param_block->magic != BOOTMAGIC_PARM) {
		printk("Invalid Boot Parameter Block %p (magic %lx)\n",
			boot_param_block, boot_param_block->magic);
	}
#endif

	/* The bank should start at the physical start of RAM, while our
	 * sram_base is the start of RAM that is available after MVC init.
	 */
	mi->nr_banks      = 1;
	mi->bank[0].start = PHYS_OFFSET;
#ifdef ISL3893_NO_MVC
	mi->bank[0].size  = 0x00800000;	// for now set to 8MByte
#else
	mi->bank[0].size  = bootstruct->sram_base + bootstruct->sram_size - PHYS_OFFSET;
#endif /* ISL3893_NO_MVC */
	mi->bank[0].node  = 0;
	mi->end = mi->bank[0].start + mi->bank[0].size;

#ifndef ISL3893_SIMSONLY
	/* Initialize an MPU area for the Entire RAM */
	init_ram_mpu_area(mi->bank[0].start, mi->bank[0].size);
	/* Switch to KERNEL_DS. We switch to USER_DS when the first app is started */
	set_fs(get_ds());
#endif
}

MACHINE_START(ISL3893, "ISL3893")
	BOOT_MEM(PHYS_OFFSET, 0x12345678, 0x12345678)	// phys RAM, phys IO, virt IO
	MAINTAINER("TBD")
	INITIRQ(genarch_init_irq)
	BOOT_PARAMS((unsigned int)&common_params)
	FIXUP(fixup_isl3893)
MACHINE_END


