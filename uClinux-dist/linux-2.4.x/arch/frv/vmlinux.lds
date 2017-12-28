/* ld script to make FRV Linux kernel -*- c -*-
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>;
 */
OUTPUT_FORMAT("elf32-frv", "elf32-frv", "elf32-frv")
OUTPUT_ARCH(frv)
ENTRY(_start)

#include <asm/processor.h>
#include <asm/page.h>
#include <asm/cache.h>

__page_offset = 0xc0000000;		/* start of area covered by struct pages */
__kernel_image_start = __page_offset;	/* address at which kernel image resides */

SECTIONS
{
  . = __kernel_image_start;

  /* discardable initialisation code and data */
  . = ALIGN(PAGE_SIZE);			/* Init code and data */
  __init_begin = .;

  .text.init : {
	*(.text.head)
#ifndef CONFIG_FULLDEBUG
	*(.text.init)
	*(.text.exit)
#endif
  }
  .data.init : { *(.data.init) }

  . = ALIGN(8);
  __setup_start = .;
  .setup.init : { KEEP(*(.setup.init)) }
  __setup_end = .;

  __initcall_start = .;
  .initcall.init : { KEEP(*(.initcall.init)) }
  __initcall_end = .;

  . = ALIGN(THREAD_SIZE);
  __init_end = .;

  /* put sections together that have massive alignment issues */
  . = ALIGN(THREAD_SIZE);
  .data.init_task : {
	  /* init task record & stack */
	  *(.data.init_task)
  }

  .trap : {
	/* trap table management - read entry-table.S before modifying */
	. = ALIGN(8192);
	__trap_tables = .;
	*(.trap.user)	
	*(.trap.kernel)	
	. = ALIGN(4096);
	*(.trap.break)	
  }

  . = ALIGN(4096);
  .data.page_aligned : { *(.data.idt) }

  . = ALIGN(L1_CACHE_BYTES);
  .data.cacheline_aligned : { *(.data.cacheline_aligned) }

  /* Text and read-only data */
  . = ALIGN(4);
  _text = .;
  _stext = .;
  .text : {
	*(.text.start .text .text.*)
	*(.fixup)
	*(.gnu.warning)
	*(.exitcall.exit)
	} = 0x9090

  _etext = .;			/* End of text section */

  .rodata : {
	*(.rodata) *(.rodata.*)
	*(.trap.vector)

	/* this clause must not be modified - the ordering and adjacency are imperative */
	__trap_fixup_tables = .;
	*(.trap.fixup.user .trap.fixup.kernel)

	}
  .kstrtab : { *(.kstrtab) }

  . = ALIGN(8);		/* Exception table */
  __start___ex_table = .;
  __ex_table : { KEEP(*(__ex_table)) }
  __stop___ex_table = .;

  __start___ksymtab = .;	/* Kernel symbol table */
  __ksymtab : { *(__ksymtab) }
  __stop___ksymtab = .;

  _sdata = .;
  .data : {			/* Data */
	*(.data .data.*)
	*(.data.exit)
	CONSTRUCTORS
	}

  _edata = .;			/* End of data section */

  /* Sections to be discarded */
/* /DISCARD/ : {
	*(.data.exit)
	*(.exitcall.exit)
	}
*/

  /* GP section */
  . = ALIGN(L1_CACHE_BYTES);
  _gp = . + 2048;
  PROVIDE (gp = _gp);

  .sdata : { *(.sdata .sdata.*) }

  /* BSS */
  . = ALIGN(L1_CACHE_BYTES);
  __bss_start = .;

  .sbss		: { *(.sbss .sbss.*) }
  .bss		: { *(.bss .bss.*) }
  .bss.stack	: { *(.bss) }
  _ebss = .;
  _end = . ;
  . = ALIGN(PAGE_SIZE);
  __kernel_image_end = .;

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }

  .debug_line		0 : { *(.debug_line) }
  .debug_info		0 : { *(.debug_info) }
  .debug_abbrev		0 : { *(.debug_abbrev) }
  .debug_aranges	0 : { *(.debug_aranges) }
  .debug_frame		0 : { *(.debug_frame) }
  .debug_pubnames	0 : { *(.debug_pubnames) }
  .debug_str		0 : { *(.debug_str) }
  .debug_ranges		0 : { *(.debug_ranges) }

  .comment 0 : { *(.comment) }
}

__kernel_image_size_no_bss = __bss_start - __kernel_image_start;
