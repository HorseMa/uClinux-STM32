#ifndef __ARM_A_OUT_H__
#define __ARM_A_OUT_H__

#include <linux/personality.h>
#include <asm/types.h>

struct exec
{
  __u32 a_info;		/* Use macros N_MAGIC, etc for access */
  __u32 a_text;		/* length of text, in bytes */
  __u32 a_data;		/* length of data, in bytes */
  __u32 a_bss;		/* length of uninitialized data area for file, in bytes */
  __u32 a_syms;		/* length of symbol table data in file, in bytes */
  __u32 a_entry;	/* start address */
  __u32 a_trsize;	/* length of relocation info for text, in bytes */
  __u32 a_drsize;	/* length of relocation info for data, in bytes */
};

/*
 * This is always the same
 */
#define N_TXTADDR(a)	(0x00008000)

#define N_TRSIZE(a)	((a).a_trsize)
#define N_DRSIZE(a)	((a).a_drsize)
#define N_SYMSIZE(a)	((a).a_syms)

#define M_ARM 103

#ifdef __KERNEL__

#ifdef CONFIG_ARM_FASS
/* FASS sets STACK_TOP to top of PID relocated memory, ie 32MB. In all reality
 *   there should be some heuristic for selecting of the old STACK_TOP or
 *   32MB should be set or not. This basically boils down to if a non-zero PID
 *   is going to be used. Unfortunately when the stack is set in exec() that is
 *   too difficult to work out.
 */
#define STACK_TOP ARMPID_TASK_SIZE
#else
#define STACK_TOP	((current->personality == PER_LINUX_32BIT) ? \
			 TASK_SIZE : TASK_SIZE_26)
#endif /* CONFIG_ARM_FASS */

#endif

#ifndef LIBRARY_START_TEXT
#define LIBRARY_START_TEXT	(0x00c00000)
#endif

#endif /* __A_OUT_GNU_H__ */
