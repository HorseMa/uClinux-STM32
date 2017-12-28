/*
 * File:         arch/nios2nommu/kernel/vmlinux.lds.S
 * Based on:     arch/blackfin/kernel/vmlinux.lds.S
 */

#include <asm-generic/vmlinux.lds.h>
#include <asm/nios.h>
#include <asm/page.h>
		
OUTPUT_FORMAT("elf32-littlenios2", "elf32-littlenios2", "elf32-littlenios2")

OUTPUT_ARCH(nios)
ENTRY(_start)	/* Defined in head.S */

jiffies = jiffies_64;

SECTIONS
{
	. = nasys_program_mem;
	.text : 
	{
		_stext = . ;
		_text = .;
		TEXT_TEXT
		SCHED_TEXT
		LOCK_TEXT
		KPROBES_TEXT
		*(.fixup)

		. = ALIGN(32);		/* Exception table */
		__start___ex_table = .;
		*(__ex_table)
		__stop___ex_table = .;

		_etext = .;
	} =0

	/* Just in case the first read only is a 32-bit access */
	RO_DATA(4)

	.bss :
	{
		. = ALIGN(4);
		__bss_start = .;
		*(.bss .bss.*)
		*(COMMON)
		*(.sbss)
		*(.scommon)	 
		__bss_stop = .;
	}

	.data :
	{
		_sdata = .;
		/* This gets done first, so the glob doesn't suck it in */
		. = ALIGN(32);
		*(.data.cacheline_aligned)

		DATA_DATA
		*(.data.*)
		*(.sdata)
		CONSTRUCTORS

		/* make sure the init_task is aligned to the
		 * kernel thread size so we can locate the kernel
		 * stack properly and quickly.
		 */
		. = ALIGN(THREAD_SIZE);
		*(.init_task.data)

		_edata = .;
	}

	/* The init section should be last, so when we free it, it goes into
	 * the general memory pool, and (hopefully) will decrease fragmentation
	 * a tiny bit. The init section has a _requirement_ that it be
	 * PAGE_SIZE aligned
	 */
	. = ALIGN(PAGE_SIZE);
	__init_begin = .;

	.init.text :
	{
		. = ALIGN(PAGE_SIZE);
		_sinittext = .;
		INIT_TEXT
		_einittext = .;
	}
	.init.data :
	{
		. = ALIGN(16);
		INIT_DATA
	}
	.init.setup :
	{
		. = ALIGN(16);
		__setup_start = .;
		*(.init.setup)
		__setup_end = .;
	}
	.initcall.init :
	{
		__initcall_start = .;
		INITCALLS
		__initcall_end = .;
	}
	.con_initcall.init :
	{
		__con_initcall_start = .;
		*(.con_initcall.init)
		__con_initcall_end = .;
	}
	SECURITY_INIT
	.init.ramfs :
	{
		. = ALIGN(4);
		__initramfs_start = .;
		*(.init.ramfs)
		__initramfs_end = .;
	}
  	__init_end = .;
  	_end = . ;

	STABS_DEBUG

	DWARF_DEBUG

	NOTES

	/DISCARD/ :
	{
		EXIT_TEXT
		EXIT_DATA
		*(.exitcall.exit)
	}
}
