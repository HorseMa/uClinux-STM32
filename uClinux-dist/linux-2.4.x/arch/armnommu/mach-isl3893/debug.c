/*  $Header$
 *
 *  linux/arch/arm/mach-isl3893/debug.c
 *
 *  Copyright (C) 2003 Intersil Americas Inc.
 *
 *  Architecture specific debug.
 */
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/arch/blobcalls.h>

/* This is initialized in head-armv.S */
extern struct boot_parms *boot_param_block;

struct printf_entry {
	char *format;
	char *param1;
	char *param2;
	char *param3;
};

#define MAX_PRINTF_ENTRIES	1
#define PRINTF_ENTRY_SIZE	(sizeof(struct printf_entry) / sizeof(long))

static long printf_buffer[2 + (MAX_PRINTF_ENTRIES * PRINTF_ENTRY_SIZE)];

/* We setup a debug printf queue that is printed by the bootrom when we have
 * an uncontrolled reset. We only use 1 printf queue: the kernel printk queue
 */
extern int get_printk_log_buf(char **buf);

static int setup_debug_queues(void)
{
	struct printf_buf *my_printf_buf;
	struct printf_entry *my_printf_entry;
	struct printf_info *printf_info, *my_printf_info;
	char *printk_log_buf;
	unsigned long printf_buf_count;

	if(!boot_param_block || boot_param_block->magic != BOOTMAGIC_PARM) {
		printk("Invalid Boot Parameter Block %p (magic %lx)\n",
			boot_param_block, boot_param_block->magic);
		return 0;
	}

	/* Allocate 1 extra printf info struct after the currently existing ones */
	printf_buf_count = boot_param_block->printf_buf_count;
	/* Paranoia? */
	if(printf_buf_count > 32)
		return 0;

	printf_info = kmalloc((printf_buf_count + 1) * sizeof(struct printf_info), GFP_KERNEL);

	if(printf_buf_count > 0) {
		/* Copy the current printf_info array */
		memcpy(printf_info, boot_param_block->printf_info,
			printf_buf_count * sizeof(struct printf_info));
	}


	/* Fill my printf buffer with 1 entry */
	my_printf_buf = (struct printf_buf *)printf_buffer;
	my_printf_buf->head = 0;
	my_printf_buf->tail = 0;
	my_printf_entry = (struct printf_entry *)(my_printf_buf->queue);

	get_printk_log_buf(&printk_log_buf);
	my_printf_entry->format = printk_log_buf;
	my_printf_entry->param1 = NULL;
	my_printf_entry->param2 = NULL;
	my_printf_entry->param3 = NULL;

	my_printf_info = printf_info + printf_buf_count;
	my_printf_info->size = MAX_PRINTF_ENTRIES * PRINTF_ENTRY_SIZE;
	my_printf_info->buf = my_printf_buf;

	/* Update the boot parameter block */
	boot_param_block->printf_info = printf_info;
	boot_param_block->printf_buf_count = printf_buf_count + 1;

	return 0;
}

/* We need to setup the debug queues after the memory is initialized because
 * we need kmalloc. Therefore, we use an __initcall
 */
__initcall(setup_debug_queues);
