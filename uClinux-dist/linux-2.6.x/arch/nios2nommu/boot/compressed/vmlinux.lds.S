#include <asm-generic/vmlinux.lds.h>
#include <asm/nios.h>

OUTPUT_FORMAT("elf32-littlenios2", "elf32-littlenios2", "elf32-littlenios2")

OUTPUT_ARCH(nios)
ENTRY(_start)	/* Defined in head.S */

SECTIONS
{
  . =nasys_program_mem + CONFIG_BOOT_LINK_OFFSET;

  _text = .;
  .text : { *(.text) } = 0
  .rodata : { *(.rodata) *(.rodata.*) }
  _etext = .;

  . = ALIGN(32 / 8);
  .data : { *(.data) }
  . = ALIGN(32 / 8);
  _got = .;
  .got  : { *(.got) _egot = .; *(.got.*) }
  _edata  =  .;

  . = ALIGN(32 / 8);
  __bss_start = .;
  .bss : { *(.bss) *(.sbss) }
  . = ALIGN(32 / 8);
  _ebss = .;
  end = . ;
  _end = . ;

  got_len = (_egot - _got);
}
