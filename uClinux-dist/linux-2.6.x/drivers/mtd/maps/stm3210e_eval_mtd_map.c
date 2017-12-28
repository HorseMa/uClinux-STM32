/* mtd-bsp.c */

/*
 * Simple M29W128F NOR flash mapping driver for Linux
 *
 * based on the Embedded Linux book Mapping driver example.
 *
 * Licensed under the GPL.
 *
 */

//#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/concat.h>

#define WINDOW_ADDR_0 0x64000000
#define WINDOW_SIZE_0 0x01000000

/*The map_info contains the details of the starting address (physical) of
the chip, size, and bus width. These are used by the chip probe routines.*/

static struct map_info m29w128f_map=
{
	.name = "M29W128f flash",
	.phys = WINDOW_ADDR_0,
	.size = WINDOW_SIZE_0,
	.bankwidth = 4,
};

static struct mtd_partition stm3210e_eval_flash_partitions [] = {
{
	.name = "Kernel raw data",
	.offset = 0,
	.size = 0x00100000,
	.mask_flags	= MTD_WRITEABLE, /* force read-only */
	},
	{
	.name = "rootfs",
	.offset = 0x00100000,
	.size = 0x60000,/* MTDPART_SIZ_FULL will expand to the end of the flash */
	},
	{
	.name = "rawdata",
	.offset = 0x00160000,
	.size = 0x30000,/* MTDPART_SIZ_FULL will expand to the end of the flash */
	},
	{
	.name = "cramfs_partition",
	.offset = 0x00190000,
	.size = 0x30000,/* MTDPART_SIZ_FULL will expand to the end of the flash */
	},
};

/*
* Following structure holds the mtd_info structure pointers for
* the flash devices
*/
	static struct mtd_info * stm3210e_eval_mtd;
	
int __init stm3210e_eval_init_m29w128f_mtd (void)
{
	/* Probe for the boot flash */
	m29w128f_map.virt = (void*)(unsigned long)ioremap(m29w128f_map.phys,m29w128f_map.size);
	simple_map_init(&m29w128f_map);
	stm3210e_eval_mtd = do_map_probe("m29w128f_probe", &m29w128f_map);
	if(stm3210e_eval_mtd)
		stm3210e_eval_mtd->owner = THIS_MODULE;

	if (!stm3210e_eval_mtd)
		return -ENXIO;

/*
* Now we will partition the boot flash.
*/
	add_mtd_partitions(stm3210e_eval_mtd, stm3210e_eval_flash_partitions, 4);
return 0;
}	
	
static void __exit stm3210e_eval_cleanup_m29w128f_mtd(void)
{
	del_mtd_partitions(stm3210e_eval_mtd);
	map_destroy(stm3210e_eval_mtd);
}

module_init (stm3210e_eval_init_m29w128f_mtd);
module_exit (stm3210e_eval_cleanup_m29w128f_mtd);

MODULE_AUTHOR("MCD Application Team");
MODULE_DESCRIPTION("STM32 M29W128F mapping driver");
MODULE_LICENSE("GPL");
