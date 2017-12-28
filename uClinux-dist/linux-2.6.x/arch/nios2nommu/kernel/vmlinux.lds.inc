#include <asm-generic/vmlinux.lds.h>
#include <asm/nios.h>
		
OUTPUT_FORMAT("elf32-littlenios2", "elf32-littlenios2", "elf32-littlenios2")

OUTPUT_ARCH(nios)
ENTRY(_start)	/* Defined in head.S */

MEMORY {
	ram	: ORIGIN = nasys_program_mem, LENGTH = nasys_program_mem_size
	eram	: ORIGIN = nasys_program_mem_end, LENGTH = 0
	romfs	: ORIGIN = na_flash_kernel + 0x200000, LENGTH = na_flash_kernel_size - 0x200000
}

jiffies = jiffies_64;

SECTIONS
{
        .text :
	{
	    __ram_start = . ;
	    _stext = . ;
	    text_start = . ;
		    *(.text)

		. = ALIGN(32);          /* Exception table              */
		__start___ex_table = .;
		*(__ex_table)
		__stop___ex_table = .;

		*(.rodata) *(.rodata.*)
		*(__vermagic)		/* Kernel version magic */
		*(.rodata1)

		/* Kernel symbol table: Normal symbols */
		__start___ksymtab = .;
		*(__ksymtab)
		__stop___ksymtab = .;

		/* Kernel symbol table: GPL-only symbols */
		__start___ksymtab_gpl = .;
		*(__ksymtab_gpl)
		__stop___ksymtab_gpl = .;

		/* Kernel symbol table: Normal symbols */
		__start___kcrctab = .;
		*(__kcrctab)
		__stop___kcrctab = .;

		/* Kernel symbol table: GPL-only symbols */
		__start___kcrctab_gpl = .;
		*(__kcrctab_gpl)
		__stop___kcrctab_gpl = .;

		/* Kernel symbol table: strings */
		*(__ksymtab_strings)

		. = ALIGN(4) ;
		_etext = . ;

	    _rambase = __ram_start ;
	    _ramstart = __ram_start ;
	} > ram

        .data :
	{
		. = ALIGN(4); 
		_sdata = . ;
		__data_start = . ;
		*(.sdata)
		*(.data)
		. = ALIGN(8192) ;
		*(.data.init_task)
		_edata = . ;
	} > ram

	.init :	{
		. = ALIGN(4096);
		__init_begin = .;
		_sinittext = .;
		*(.init.text)
		_einittext = .;
		*(.init.data)
		. = ALIGN(16);
		__setup_start = .;
		*(.init.setup)
		__setup_end = .;
		__start___param = .;
		*(__param)
		__stop___param = .;
		__initcall_start = .;
		*(.initcall1.init)
		*(.initcall2.init)
		*(.initcall3.init)
		*(.initcall4.init)
		*(.initcall5.init)
		*(.initcall6.init)
		*(.initcall7.init)
		__initcall_end = .;
		__con_initcall_start = .;
		*(.con_initcall.init)
		__con_initcall_end = .;
		__security_initcall_start = .;
		*(.security_initcall.init)
		__security_initcall_end = .;
		. = ALIGN(4);
		__initramfs_start = .;
		*(.init.ramfs)
		__initramfs_end = .;
		. = ALIGN(4);
		__init_end = .;

	    _kernel_footprint = . -__ram_start ;
        } > ram

        .bss :
        {
	    __bss_start = ALIGN( 0x10 ) ;
	    *(.bss)
	    *(COMMON)
	} > ram

        .sbss :
        {
	    _sbss = ALIGN( 0x10 ) ;
	    __data_end = ALIGN( 0x10 ) ;
	    *(.sbss)
	    _sbss_end = . ;
	    _ebss = . ;
	    __bss_end = . ;
	    /*	end = ALIGN( 0x10 ) ;		*/
	    /*	_end = ALIGN( 0x10 ) ;		*/
	    end = ALIGN( 0x1000 ) ;
	    _end = ALIGN( 0x1000 ) ;
	} > ram

	.eram : {
		__ramend = . ;
		_ramend = . ;
	} > eram

	.romfs.data :
	{
	    _romfs = . ; 
	    *(.romfs.data) ; 
	    _romfs_end = . ; 
	} > romfs
	
 /* Sections to be discarded */
  /DISCARD/ : {
	*(.exit.text)
	*(.exit.data)
	*(.exitcall.exit)
	}

  /*
   * ------------------------------------------------------------
   * dvb say: "I'll leave all this stuff down here exactly
   * as I found it, for debugging info, without
   * understanding it."
   */

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* These must appear regardless of  .  */
}
