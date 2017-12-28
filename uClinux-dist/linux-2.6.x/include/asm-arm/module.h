#ifndef _ASM_ARM_MODULE_H
#define _ASM_ARM_MODULE_H
#ifdef __KERNEL__
#include <linux/module.h>
#ifdef CONFIG_ARM_PLT_MODULE_LOAD
/*
 * ARM can only do 24 bit relative calls. In the mainstream kernel this is
 * handled by mapping module memory to the 16 MB below the kernel. However,
 * in the no mmu case, this is not possible. So, to fall back, we create
 * a Procedure Linkage Table (PLT) and attach it to the module during
 * the in-kernel linking stage.
 * This code is adapted from the powerpc and v850 architectures,
 * and from that supplied by Rusty Russel
 */
struct arm_plt_entry {
        /* Jump Instruction */
        unsigned long jump[2];
};

struct mod_arch_specific {
        /* The index of the ELF header in the module in which to store the PLT */
        unsigned int plt_section;
};

#else

struct mod_arch_specific {
	int foo;
};

#endif /* CONFIG_ARM_PLT_MODULE_LOAD */

#define Elf_Shdr	Elf32_Shdr
#define Elf_Sym		Elf32_Sym
#define Elf_Ehdr	Elf32_Ehdr


/*
 * Include the ARM architecture version.
 */
#define MODULE_ARCH_VERMAGIC	"ARMv" __stringify(__LINUX_ARM_ARCH__) " "

#ifdef CONFIG_ARM_PLT_MODULE_LOAD
#ifdef MODULE
/*
 * Force the creation of a .plt section. This will contain the PLT constructed
 * during dynamic link time
 */
asm(".section .plt; .align 3; .previous");
#endif /* MODULE */
#endif /* CONFIG_ARM_PLT_MODULE_LOAD */

#endif /* __KERNEL__ */                
#endif /* _ASM_ARM_MODULE_H */
