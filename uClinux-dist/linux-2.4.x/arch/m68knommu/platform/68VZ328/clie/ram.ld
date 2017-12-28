MEMORY
{
        ramvec : ORIGIN = 0x0000000, LENGTH = 0x0000400
	bvec   : ORIGIN = 0x0004000, LENGTH = 0x0000400
        ram    : ORIGIN = 0x0004400, LENGTH = 0x1000000 - 0x0004400
        eram   : ORIGIN = 0x01000000, LENGTH = 0
}

SECTIONS
{
  .romvec :
	{
	_romvec = . ;
	} > bvec

  .ramvec :
	{
	__ram_start = . ;
	__ramvec = . ;
	} > ramvec
  
  .text :
	{
	_stext = . ;
        	*(.text)
	. = ALIGN(0x4) ;
		*(.text.*)
	. = ALIGN(0x4) ;
		*(.exitcall.exit)
	. = ALIGN(0x4) ;
		*(.kstrtab)
	. = ALIGN(16);		/* Exception table */
	__start___ex_table = .;
		*(__ex_table)
	__stop___ex_table = .;

	__start___ksymtab = .; /* Kernel symbol table */
		*(__ksymtab)
	__stop___ksymtab = .;

	. = ALIGN(0x4);
	_etext = . ;
	__data_rom_start = . ;
        } > ram

  .data : 
	{
	. = ALIGN(0x4) ;
	_sdata = . ;
	__data_start = . ;

	. = ALIGN(0x4) ;
		*(.rodata)
	. = ALIGN(0x4) ;
		*(.data)
	. = ALIGN(0x4) ;
		*(.data.*)

	. = ALIGN(0x4) ;
	__setup_start = .;
		*(.setup.init)
	. = ALIGN(0x4) ;
	__setup_end = .;

	. = ALIGN(0x4) ;
	__initcall_start = .;
		*(.initcall.init)
	. = ALIGN(0x4) ;
	__initcall_end = .;

	. = ALIGN(0x2000) ;
		*(.data.init_task)
	. = ALIGN(0x2000) ;

	_edata = . ;
        } > ram

  .bss :
        {
	. = ALIGN(0x4) ;
	_sbss = . ;
		*(.bss)
	. = ALIGN(0x4) ;
		*(COMMON)
	. = ALIGN(0x4) ;
	_ebss = . ;
	_end = . ;
	} > ram

  .eram :
	{
        __ramend = . ;
	} > eram
}
