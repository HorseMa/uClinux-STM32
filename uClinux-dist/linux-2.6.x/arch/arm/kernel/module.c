/*
 *  linux/arch/arm/kernel/module.c
 *
 *  Copyright (C) 2002 Russell King.
 *  Modified for nommu by Hyok S. Choi
 *  Modified for PLT relocation support by Ken Wilson, using code by
 *  Rusty Russell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Module allocation method suggested by Andi Kleen.
 */
#include <linux/module.h>
#include <linux/moduleloader.h>
#include <linux/kernel.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>

#include <asm/pgtable.h>

#ifdef CONFIG_XIP_KERNEL
/*
 * The XIP kernel text is mapped in the module area for modules and
 * some other stuff to work without any indirect relocations.
 * MODULE_START is redefined here and not in asm/memory.h to avoid
 * recompiling the whole kernel when CONFIG_XIP_KERNEL is turned on/off.
 */
extern void _etext;
#undef MODULE_START
#define MODULE_START	(((unsigned long)&_etext + ~PGDIR_MASK) & PGDIR_MASK)
#endif


#ifdef CONFIG_MMU
void *module_alloc(unsigned long size)
{
#ifdef CONFIG_ARM_MODULE_USE_CONTIGUOUS_MEMORY
	return alloc_exact(size);
#else
	struct vm_struct *area;

	size = PAGE_ALIGN(size);
	if (!size)
		return NULL;

	area = __get_vm_area(size, VM_ALLOC, MODULE_START, MODULE_END);
	if (!area)
		return NULL;

	return __vmalloc_area(area, GFP_KERNEL, PAGE_KERNEL);
#endif /* CONFIG_ARM_MODULE_USE_CONTIGUOUS_MEMORY */
}
#else /* CONFIG_MMU */
void *module_alloc(unsigned long size)
{
	return size == 0 ? NULL : vmalloc(size);
}
#endif /* !CONFIG_MMU */

void module_free(struct module *module, void *region)
{
#ifdef CONFIG_ARM_MODULE_USE_CONTIGUOUS_MEMORY
	if (region == module->module_init)
	{
		free_exact(region, module->init_size);
	}
	else
	{
		free_exact(region, module->core_size);
	}
#else
	vfree(region);
#endif /* CONFIG_ARM_MODULE_USE_CONTIGUOUS_MEMORY */
}

#ifdef CONFIG_ARM_PLT_MODULE_LOAD
/*
 * Count the number of distinct 24 bit relocations in the module
 */
static unsigned int count_relocs(const Elf32_Rel *rel, unsigned int num)
{
	unsigned int i, j, ret = 0;

	/* Sure, this is order(n^2), but it's usually short, and not
           time critical */
	for (i = 0; i < num; i++) {
		if (ELF32_R_TYPE(rel[i].r_info) != R_ARM_PC24)
			continue;

		for (j = 0; j < i; j++) {
			if (ELF32_R_TYPE(rel[i].r_info) != R_ARM_PC24)
				continue;
			/* If this addend appeared before, it's
                           already been counted */
			if (ELF32_R_SYM(rel[i].r_info)
			    == ELF32_R_SYM(rel[j].r_info))
				break;
		}
		if (j == i) ret++;
	}
	return ret;
}


/* 
 * Calculate the required size of the PLT
 */
static unsigned long get_plt_size(const Elf32_Ehdr *hdr,
				  const Elf32_Shdr *sechdrs,
				  const char *secstrings)
{
	unsigned long ret = 0;
	unsigned i;

	/* Everything marked ALLOC (this includes the exported
           symbols) */
	for (i = 1; i < hdr->e_shnum; i++) {

		if (sechdrs[i].sh_type == SHT_REL) {
			ret += count_relocs((void *)hdr
					     + sechdrs[i].sh_offset,
					     sechdrs[i].sh_size
					     / sizeof(Elf32_Rel));
		}
	}

	return ret * sizeof(struct arm_plt_entry);
}
#endif /* CONFIG_ARM_PLT_MODULE_LOAD */ 

/*
 * Reserve space for the PLT's
 */
int module_frob_arch_sections(Elf_Ehdr *hdr,
			      Elf_Shdr *sechdrs,
			      char *secstrings,
			      struct module *mod)
{

#ifdef CONFIG_ARM_PLT_MODULE_LOAD
	unsigned int i;
	char *p;

	/* Find the .plt section */
	for (i = 0; i < hdr->e_shnum; i++) {
		if (strcmp(secstrings + sechdrs[i].sh_name, ".plt") == 0)
			mod->arch.plt_section = i;
		
		while ((p = strstr(secstrings + sechdrs[i].sh_name, ".init")))
			p[0] = '_';
	}
	if (!mod->arch.plt_section) {
		printk("Module doesn't contain .plt section.\n");
		return -ENOEXEC;
	}

	/* Override its size */
	sechdrs[mod->arch.plt_section].sh_size
		= get_plt_size(hdr, sechdrs, secstrings);

	/* Override the types and flags */
	sechdrs[mod->arch.plt_section].sh_type = SHT_NOBITS;
	sechdrs[mod->arch.plt_section].sh_flags = (SHF_EXECINSTR | SHF_ALLOC);
#endif /* CONFIG_ARM_PLT_MODULE_LOAD */
	return 0;
}


#ifdef CONFIG_ARM_PLT_MODULE_LOAD
/* Set up a trampoline in the PLT to bounce us to the distant function */
static uint32_t do_plt_call(void *location,
			    Elf32_Addr val,
			    Elf32_Shdr *sechdrs,
			    struct module *mod)
{
	struct arm_plt_entry *plt;
	unsigned int i, num_plts;
	(void)location;

	plt = (void *)sechdrs[mod->arch.plt_section].sh_addr;
	num_plts = sechdrs[mod->arch.plt_section].sh_size / sizeof(*plt);

	for (i = 0; i < num_plts; i++) {
		if (!plt[i].jump[0]) {
			/* New one.  Fill in. */
			plt[i].jump[0] = 0xe51ff004;
			plt[i].jump[1] = val;
		}
		if (plt[i].jump[1] == val) {
			/* DEBUGP("Made plt %u for %p at %p\n",
			i, (void *)val, &plt[i]); */
			return (u32)&plt[i];
		}
	}
	BUG();
	return -ENOEXEC;
}
#endif /* CONFIG_ARM_PLT_MODULE_LOAD */

int
apply_relocate(Elf32_Shdr *sechdrs, const char *strtab, unsigned int symindex,
	       unsigned int relindex, struct module *module)
{
	Elf32_Shdr *symsec = sechdrs + symindex;
	Elf32_Shdr *relsec = sechdrs + relindex;
	Elf32_Shdr *dstsec = sechdrs + relsec->sh_info;
	Elf32_Rel *rel = (void *)relsec->sh_addr;
	unsigned int i;


	for (i = 0; i < relsec->sh_size / sizeof(Elf32_Rel); i++, rel++) {
		unsigned long loc;
		Elf32_Sym *sym;
		s32 offset;
		unsigned long addend;

		offset = ELF32_R_SYM(rel->r_info);
		if (offset < 0 || offset > (symsec->sh_size / sizeof(Elf32_Sym))) {
			printk(KERN_ERR "%s: bad relocation, section %d reloc %d\n",
				module->name, relindex, i);
			return -ENOEXEC;
		}

		sym = ((Elf32_Sym *)symsec->sh_addr) + offset;

		if (rel->r_offset < 0 || rel->r_offset > dstsec->sh_size - sizeof(u32)) {
			printk(KERN_ERR "%s: out of bounds relocation, "
				"section %d reloc %d offset %d size %d\n",
				module->name, relindex, i, rel->r_offset,
				dstsec->sh_size);
			return -ENOEXEC;
		}

		loc = dstsec->sh_addr + rel->r_offset;

		switch (ELF32_R_TYPE(rel->r_info)) {
		case R_ARM_ABS32:
			*(u32 *)loc += sym->st_value;
			break;

		case R_ARM_PC24:
		case R_ARM_CALL:
		case R_ARM_JUMP24:
			/* Pull addend from location */
			addend = (*(u32 *)loc & 0x00ffffff) << 2;
			if (addend & 0x02000000)
				addend -= 0x04000000;

			offset = sym->st_value + addend - loc;

			if (offset < -0x02000000 || offset >= 0x02000000 
			  || offset & 1 || offset & 3)
			{
#ifndef CONFIG_ARM_PLT_MODULE_LOAD 
				printk(KERN_ERR
				       "%s: relocation out of range, section "
				       "%d reloc %d sym '%s'\n", module->name,
				       relindex, i, strtab + sym->st_name);
				return -ENOEXEC;
#else
				unsigned long plt_loc;
				/*
 				 * Create Trampoline to symbol
				 */
				plt_loc = do_plt_call((u32 *)loc, sym->st_value,
						    sechdrs, module);

				offset = plt_loc + addend - loc;

				if (offset & 3)
				{
					printk(KERN_ERR
					"%s: relocation unsafe, section "
					"%d reloc %d sym '%s'\n", module->name,
					relindex, i, strtab + sym->st_name);
					return -ENOEXEC;
				}
#endif /* CONFIG_ARM_PLT_MODULE_LOAD */
			}

			*(u32 *)loc &= 0xff000000;
			*(u32 *)loc |= (offset >> 2) & 0x00ffffff;
			break;

		default:
			printk(KERN_ERR "%s: unknown relocation: %u\n",
			       module->name, ELF32_R_TYPE(rel->r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}

int
apply_relocate_add(Elf32_Shdr *sechdrs, const char *strtab,
		   unsigned int symindex, unsigned int relsec, struct module *module)
{
	printk(KERN_ERR "module %s: ADD RELOCATION unsupported\n",
	       module->name);
	return -ENOEXEC;
}

int
module_finalize(const Elf32_Ehdr *hdr, const Elf_Shdr *sechdrs,
		struct module *module)
{
	return 0;
}

void
module_arch_cleanup(struct module *mod)
{
}
