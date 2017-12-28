/*
 *
 * Handle mapping of the NOR flash on Cirrus Logic EDB9315 boards
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/config.h>

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#define WINDOW_ADDR 0x60000000      /* physical properties of flash */
#define WINDOW_SIZE 0x02000000
#define BUSWIDTH    4
#define FLASH_BLOCKSIZE_MAIN	0x20000
#define FLASH_NUMBLOCKS_MAIN	128
/* can be "cfi_probe", "jedec_probe", "map_rom", 0 }; */
#define PROBETYPES { "cfi_probe", 0 }

#define MSG_PREFIX "EDB9315-NOR:"   /* prefix for our printk()'s */
#define MTDID      "edb9315-nor"    /* for mtdparts= partitioning */

static struct mtd_info *mymtd;

__u8 edb9315nor_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 edb9315nor_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 edb9315nor_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void edb9315nor_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void edb9315nor_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void edb9315nor_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void edb9315nor_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void edb9315nor_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

struct map_info edb9315nor_map = {
	name: "NOR flash on EDB9315",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8: edb9315nor_read8,
	read16: edb9315nor_read16,
	read32: edb9315nor_read32,
	copy_from: edb9315nor_copy_from,
	write8: edb9315nor_write8,
	write16: edb9315nor_write16,
	write32: edb9315nor_write32,
	copy_to: edb9315nor_copy_to
};

#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
int parse_cmdline_partitions(struct mtd_info *master, 
			     struct mtd_partition **pparts,
			     const char *mtd_id);
#endif
#ifdef CONFIG_MTD_REDBOOT_PARTS
extern int parse_redboot_partitions(struct mtd_info *master,
	struct mtd_partition **pparts);
#endif
#endif

static int                   mtd_parts_nb = 0;
static struct mtd_partition *mtd_parts    = 0;

int __init init_edb9315nor(void)
{
	static const char *rom_probe_types[] = PROBETYPES;
	const char **type;
	const char *part_type = 0;

       	printk(KERN_NOTICE MSG_PREFIX "0x%08x at 0x%08x\n", 
	       WINDOW_SIZE, WINDOW_ADDR);
	edb9315nor_map.map_priv_1 = (unsigned long)
	  ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!edb9315nor_map.map_priv_1) {
		printk(MSG_PREFIX "failed to ioremap\n");
		return -EIO;
	}
	mymtd = 0;
	type = rom_probe_types;
	for(; !mymtd && *type; type++) {
		mymtd = do_map_probe(*type, &edb9315nor_map);
	}
	if (mymtd) {
		mymtd->module = THIS_MODULE;

#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
		mtd_parts_nb = parse_cmdline_partitions(mymtd, &mtd_parts, MTDID);
		if (mtd_parts_nb > 0)
		  part_type = "command line";
#endif
		
#ifdef CONFIG_MTD_REDBOOT_PARTS
		if (mtd_parts_nb <= 0) {
			mtd_parts_nb = parse_redboot_partitions(mymtd, &mtd_parts);
			if (mtd_parts_nb > 0) {
				part_type = "RedBoot";
			}
		}
#endif
#endif
		add_mtd_device(mymtd);
		if (mtd_parts_nb <= 0)
		  printk(KERN_NOTICE MSG_PREFIX "no partition info available\n");
		else
		{
			printk(KERN_NOTICE MSG_PREFIX
			       "using %s partition definition\n", part_type);
			add_mtd_partitions(mymtd, mtd_parts, mtd_parts_nb);
		}
		return 0;
	}

	iounmap((void *)edb9315nor_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_edb9315nor(void)
{
	if (mymtd) {
		del_mtd_device(mymtd);
		map_destroy(mymtd);
	}
	if (edb9315nor_map.map_priv_1) {
		iounmap((void *)edb9315nor_map.map_priv_1);
		edb9315nor_map.map_priv_1 = 0;
	}
}

module_init(init_edb9315nor);
module_exit(cleanup_edb9315nor);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marius Groeger <mag@sysgo.de>");
MODULE_DESCRIPTION("Generic configurable MTD map driver");
