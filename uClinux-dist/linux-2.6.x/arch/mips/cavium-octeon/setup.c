/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Cavium Networks
 * Copyright (C) 2008 Wind River Systems
 */
#include <linux/init.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/serial.h>
#include <linux/types.h>
#include <linux/string.h>	/* for memset */
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/time.h>
#include <linux/serial_core.h>
#include <linux/serial_8250.h>
#include <linux/string.h>

#include <asm/processor.h>
#include <asm/reboot.h>
#include <asm/smp-ops.h>
#include <asm/system.h>
#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/bootinfo.h>
#include <asm/sections.h>
#include <asm/time.h>
#include "hal.h"
#include "cvmx-l2c.h"
#include "cvmx-bootmem.h"

#ifdef CONFIG_CAVIUM_DECODE_RSL
extern void cvmx_interrupt_rsl_decode(void);
extern int __cvmx_interrupt_ecc_report_single_bit_errors;
extern void cvmx_interrupt_rsl_enable(void);
#endif

extern struct plat_smp_ops octeon_smp_ops;
extern void octeon_user_io_init(void);
extern void putDebugChar(char ch);

#ifdef CONFIG_CAVIUM_OCTEON_BOOTBUS_COMPACT_FLASH
extern void ebt3000_cf_enable_dma(void);
#endif

#ifdef CONFIG_CAVIUM_RESERVE32
extern uint64_t octeon_reserve32_memory;
#endif
static unsigned long long MAX_MEMORY = 512ull << 20;

/**
 * Reboot Octeon
 *
 * @param command Command to pass to the bootloader. Currently ignored.
 */
static void octeon_restart(char *command)
{
	/* Disable all watchdogs before soft reset. They don't get cleared */
#ifdef CONFIG_SMP
	int cpu;
	for (cpu = 0; cpu < NR_CPUS; cpu++)
		if (cpu_online(cpu))
			cvmx_write_csr(CVMX_CIU_WDOGX(cpu_logical_map(cpu)), 0);
#else
	cvmx_write_csr(CVMX_CIU_WDOGX(cvmx_get_core_num()), 0);
#endif

	mb();
	while (1)
		cvmx_write_csr(CVMX_CIU_SOFT_RST, 1);
}


/**
 * Permanently stop a core.
 *
 * @param arg
 */
static void octeon_kill_core(void *arg)
{
	mb();
	if (octeon_is_simulation()) {
		/* The simulator needs the watchdog to stop for dead cores */
		cvmx_write_csr(CVMX_CIU_WDOGX(cvmx_get_core_num()), 0);
		/* A break instruction causes the simulator stop a core */
		asm volatile ("sync\nbreak");
	}
}


/**
 * Halt the system
 */
static void octeon_halt(void)
{
	smp_call_function(octeon_kill_core, NULL, 0, 0);
	octeon_poweroff();
	octeon_kill_core(NULL);
}


/**
 * Platform time init specifics.
 * @return
 */
void __init plat_time_init(void)
{
	/* Nothing special here, but we are required to have one */
}


/**
 * Handle all the error condition interrupts that might occur.
 *
 * @param cpl
 * @param dev_id
 * @return
 */
#ifdef CONFIG_CAVIUM_DECODE_RSL
static irqreturn_t octeon_rlm_interrupt(int cpl, void *dev_id)
{
	cvmx_interrupt_rsl_decode();
	return IRQ_HANDLED;
}
#endif

/**
 * Return a string representing the system type
 *
 * @return
 */
const char *get_system_type(void)
{
	return octeon_board_type_string();
}


/**
 * Early entry point for arch setup
 */
void __init prom_init(void)
{
	const int coreid = cvmx_get_core_num();
	int i;
	int argc;
	struct uart_port octeon_port;
	int octeon_uart;

	octeon_hal_init();
	octeon_check_cpu_bist();
#ifdef CONFIG_CAVIUM_OCTEON_2ND_KERNEL
	octeon_uart = 1;
#else
	octeon_uart = octeon_get_boot_uart();
#endif

	/* Disable All CIU Interrupts. The ones we need will be
	 * enabled later.  Read the SUM register so we know the write
	 * completed. */
	cvmx_write_csr(CVMX_CIU_INTX_EN0((coreid * 2)), 0);
	cvmx_write_csr(CVMX_CIU_INTX_EN0((coreid * 2 + 1)), 0);
	cvmx_write_csr(CVMX_CIU_INTX_EN1((coreid * 2)), 0);
	cvmx_write_csr(CVMX_CIU_INTX_EN1((coreid * 2 + 1)), 0);
	cvmx_read_csr(CVMX_CIU_INTX_SUM0((coreid * 2)));

#ifdef CONFIG_SMP
	octeon_write_lcd("LinuxSMP");
#else
	octeon_write_lcd("Linux");
#endif

#ifdef CONFIG_CAVIUM_GDB
	/* When debugging the linux kernel, force the cores to enter
	 * the debug exception handler to break in.  */
	if (octeon_get_boot_debug_flag()) {
		cvmx_write_csr(CVMX_CIU_DINT, 1 << cvmx_get_core_num());
		cvmx_read_csr(CVMX_CIU_DINT);
	}
#endif

	/* BIST should always be enabled when doing a soft reset. L2
	 * Cache locking for instance is not cleared unless BIST is
	 * enabled.  Unfortunately due to a chip errata G-200 for
	 * Cn38XX and CN31XX, BIST msut be disabled on these parts */
	if (OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2) ||
	    OCTEON_IS_MODEL(OCTEON_CN31XX))
		cvmx_write_csr(CVMX_CIU_SOFT_BIST, 0);
	else
		cvmx_write_csr(CVMX_CIU_SOFT_BIST, 1);

	/* Default to 64MB in the simulator to speed things up */
	if (octeon_is_simulation())
		MAX_MEMORY = 64ull << 20;

	arcs_cmdline[0] = 0;
	argc = octeon_get_boot_num_arguments();
	for (i = 0; i < argc; i++) {
		const char *arg = octeon_get_boot_argument(i);
		if ((strncmp(arg, "MEM=", 4) == 0) ||
		    (strncmp(arg, "mem=", 4) == 0)) {
			sscanf(arg + 4, "%llu", &MAX_MEMORY);
			MAX_MEMORY <<= 20;
			if (MAX_MEMORY == 0)
				MAX_MEMORY = 32ull << 30;
		} else if (strcmp(arg, "ecc_verbose") == 0) {
#ifdef CONFIG_CAVIUM_REPORT_SINGLE_BIT_ECC
			__cvmx_interrupt_ecc_report_single_bit_errors = 1;
			pr_notice("Reporting of single bit ECC errors is "
				  "turned on\n");
#endif
		} else if (strlen(arcs_cmdline) + strlen(arg) + 1 <
			   sizeof(arcs_cmdline) - 1) {
			strcat(arcs_cmdline, " ");
			strcat(arcs_cmdline, arg);
		}
	}
#ifdef CONFIG_CAVIUM_OCTEON_BOOTBUS_COMPACT_FLASH
	if (strstr(arcs_cmdline, "use_cf_dma"))
		ebt3000_cf_enable_dma();
#endif

	if (strstr(arcs_cmdline, "console=") == NULL) {
#ifdef CONFIG_GDB_CONSOLE
		strcat(arcs_cmdline, " console=gdb");
#else
#ifdef CONFIG_CAVIUM_OCTEON_2ND_KERNEL
		strcat(arcs_cmdline, " console=ttyS0,115200");
#else
		if (octeon_uart == 1)
			strcat(arcs_cmdline, " console=ttyS1,115200");
		else
			strcat(arcs_cmdline, " console=ttyS0,115200");
#endif
#endif
	}

	if (octeon_is_simulation()) {
		/* The simulator uses a mtdram device pre filled with the
		   filesystem. Also specify the calibration delay to avoid
		   calculating it every time */
		strcat(arcs_cmdline, " rw root=1f00"
		       " lpj=60176 slram=root,0x40000000,+1073741824");
	}

	mips_hpt_frequency = octeon_get_clock_rate();

	_machine_restart = octeon_restart;
	_machine_halt = octeon_halt;

	memset(&octeon_port, 0, sizeof(octeon_port));
	/* For early_serial_setup we don't set the port type or
	 * UPF_FIXED_TYPE.  */
	octeon_port.flags = ASYNC_SKIP_TEST | UPF_SHARE_IRQ;
	octeon_port.iotype = UPIO_MEM;
	/* I/O addresses are every 8 bytes */
	octeon_port.regshift = 3;
	/* Clock rate of the chip */
	octeon_port.uartclk = mips_hpt_frequency;
	octeon_port.fifosize = 64;
	octeon_port.mapbase = 0x0001180000000800ull + (1024 * octeon_uart);
	octeon_port.membase = cvmx_phys_to_ptr(octeon_port.mapbase);
	octeon_port.serial_in = octeon_serial_in;
	octeon_port.serial_out = octeon_serial_out;
#ifdef CONFIG_CAVIUM_OCTEON_2ND_KERNEL
	octeon_port.line = 0;
#else
	octeon_port.line = octeon_uart;
#endif
	/* Only CN38XXp{1,2} has errata with uart interrupt */
	if (!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2))
		octeon_port.irq = 42 + octeon_uart;
	early_serial_setup(&octeon_port);

	octeon_user_io_init();
	register_smp_ops(&octeon_smp_ops);

#ifdef CONFIG_KGDB
	{
		const char *s = "\r\nConnect GDB to this port\r\n";
		while (*s)
			putDebugChar(*s++);
	}
#endif
}



void __init plat_mem_setup(void)
{
	uint64_t mem_alloc_size;
	uint64_t total;
	int64_t memory;

	total = 0;

	/* First add the init memory we will be returning.  */
	memory = __pa_symbol(&__init_begin) & PAGE_MASK;
	mem_alloc_size = (__pa_symbol(&__init_end) & PAGE_MASK) - memory;
	if (mem_alloc_size > 0) {
		add_memory_region(memory, mem_alloc_size, BOOT_MEM_RAM);
		total += mem_alloc_size;
	}

	/* The Mips memory init uses the first memory location for some memory
	   vectors. When SPARSEMEM is in use, it doesn't verify that the size
	   is big enough for the final vectors. Making the smallest chuck 4MB
	   seems to be enough to consistantly work. This needs to be debugged
	   more */
	mem_alloc_size = 4 << 20;
	if (mem_alloc_size > MAX_MEMORY)
		mem_alloc_size = MAX_MEMORY;

#ifdef CONFIG_SG590
	memory = PAGE_ALIGN(__pa_symbol(_end));
	mem_alloc_size = octeon_get_dram_size() - memory;
	add_memory_region(memory, mem_alloc_size, BOOT_MEM_RAM);
	total += mem_alloc_size;
#else
	/* When allocating memory, we want incrementing addresses from
	   bootmem_alloc so the code in add_memory_region can merge regions
	   next to each other */
	cvmx_bootmem_lock();
	while ((boot_mem_map.nr_map < BOOT_MEM_MAP_MAX)
		&& (total < MAX_MEMORY)) {
#if defined(CONFIG_64BIT) || defined(CONFIG_64BIT_PHYS_ADDR)
		memory = cvmx_bootmem_phy_alloc(mem_alloc_size,
						__pa_symbol(&__init_end), -1,
						0x100000,
						CVMX_BOOTMEM_FLAG_NO_LOCKING);
#elif defined(CONFIG_HIGHMEM)
		memory = cvmx_bootmem_phy_alloc(mem_alloc_size, 0, 1ull << 31,
						0x100000,
						CVMX_BOOTMEM_FLAG_NO_LOCKING);
#else
		memory = cvmx_bootmem_phy_alloc(mem_alloc_size, 0, 512 << 20,
						0x100000,
						CVMX_BOOTMEM_FLAG_NO_LOCKING);
#endif
		if (memory >= 0) {
			/* This function automatically merges address regions
			   next to each other if they are received in
			   incrementing order */
			add_memory_region(memory, mem_alloc_size, BOOT_MEM_RAM);
			total += mem_alloc_size;
		} else
			break;
	}
	cvmx_bootmem_unlock();
#endif

#ifdef CONFIG_CAVIUM_RESERVE32
	/* Now that we've allocated the kernel memory it is safe to free the
		reserved region. We free it here so that builtin drivers can
		use the memory */
	if (octeon_reserve32_memory)
		cvmx_bootmem_free_named("CAVIUM_RESERVE32");
#endif /* CONFIG_CAVIUM_RESERVE32 */

	if (total == 0)
		panic("Unable to allocate memory from cvmx_bootmem_phy_alloc\n");
}


void prom_free_prom_memory(void)
{
#ifdef CONFIG_CAVIUM_DECODE_RSL
	cvmx_interrupt_rsl_enable();

	/* Add an interrupt handler for general failures. */
	if (request_irq(OCTEON_IRQ_RML, octeon_rlm_interrupt, IRQF_SHARED,
			"RML/RSL", octeon_rlm_interrupt)) {
		panic("Unable to request_irq(OCTEON_IRQ_RML)\n");
	}
#endif

	/* This call is here so that it is performed after any TLB
	   initializations. It needs to be after these in case the
	   CONFIG_CAVIUM_RESERVE32_USE_WIRED_TLB option is set */
	octeon_hal_setup_reserved32();
}
