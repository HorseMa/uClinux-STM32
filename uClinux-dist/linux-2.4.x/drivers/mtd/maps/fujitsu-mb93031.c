/* fujitsu-mb93031.c: flash mapping support for the MB93031 VDK boards
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts,
				    unsigned long fis_origin);

static __u8 mb93091_read8(struct map_info *map, unsigned long ofs);
static __u16 mb93091_read16(struct map_info *map, unsigned long ofs);
static __u32 mb93091_read32(struct map_info *map, unsigned long ofs);
static void mb93091_write8(struct map_info *map, __u8 d, unsigned long adr);
static void mb93091_write16(struct map_info *map, __u16 d, unsigned long adr);
static void mb93091_write32(struct map_info *map, __u32 d, unsigned long adr);
static void mb93091_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len);
static void mb93091_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len);

struct map_info mb93031_boot_flash_map = {
	.name		= "MB93031 boot flash",
#ifdef CONFIG_MB93093_PDK
	.size		= 16 * 1024 * 1024,
#else // MB93091
	.size		= 2 * 1024 * 1024,
#endif
	.buswidth	= 2,
	.read8		= mb93091_read8,
	.read16		= mb93091_read16,
	.read32		= mb93091_read32,
	.write8		= mb93091_write8,
	.write16	= mb93091_write16,
	.write32	= mb93091_write32,
	.copy_from	= mb93091_copy_from,
	.copy_to	= mb93091_copy_to
};

static struct mtd_info *boot_mtd;
static struct mtd_partition *boot_parts;
static int boot_parts_qty;

#ifndef CONFIG_MB93093_PDK
struct map_info mb93031_user_flash_map = {
	.name		= "MB93031 user flash",
	.size		= 2 * 1024 * 1024,
	.buswidth	= 2,
	.read8		= mb93091_read8,
	.read16		= mb93091_read16,
	.read32		= mb93091_read32,
	.write8		= mb93091_write8,
	.write16	= mb93091_write16,
	.write32	= mb93091_write32,
	.copy_from	= mb93091_copy_from,
	.copy_to	= mb93091_copy_to
};

static struct mtd_info *user_mtd;
static struct mtd_partition *user_parts;
static int user_parts_qty;
#endif // PDK403

/*****************************************************************************/
/*
 * flash I/O routines
 */
static __u8 mb93091_read8(struct map_info *map, unsigned long ofs)
{
	return readb((volatile void *) map->map_priv_1 + ofs);
}

static __u16 mb93091_read16(struct map_info *map, unsigned long ofs)
{
	return readw((volatile void *) map->map_priv_1 + ofs);
}

static __u32 mb93091_read32(struct map_info *map, unsigned long ofs)
{
	return readl((volatile void *) map->map_priv_1 + ofs);
}

static void mb93091_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, (volatile void *) map->map_priv_1 + adr);
	mb();
}

static void mb93091_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, (volatile void *) map->map_priv_1 + adr);
	mb();
}

static void mb93091_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, (volatile void *) map->map_priv_1 + adr);
	mb();
}

static void mb93091_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, (volatile void *) map->map_priv_1 + from, len);
}

static void mb93091_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio((volatile void *) map->map_priv_1 + to, from, len);
}

/*****************************************************************************/
/*
 * 
 */
static int __init mb93031_maps_init(void)
{
	int ret = -ENXIO;

       	printk(KERN_NOTICE "Fujitsu flash device mappings\n");

	/* contemplate the boot flash */
	mb93031_boot_flash_map.map_priv_1 =
		(unsigned long) ioremap(0xFF000000,
					mb93031_boot_flash_map.size);

	if (!mb93031_boot_flash_map.map_priv_1) {
		printk("Failed to ioremap\n");
		ret = -EIO;
	}
	else {
		boot_mtd = do_map_probe("cfi_probe", &mb93031_boot_flash_map);
		if (!boot_mtd) {
			iounmap((void *) mb93031_boot_flash_map.map_priv_1);
		}
		else {
			/* okay... we've found a valid flash chip */
			boot_mtd->module = THIS_MODULE;

#if defined(CONFIG_MTD_PARTITIONS) && defined(CONFIG_MTD_REDBOOT_PARTS)
			boot_parts_qty = parse_redboot_partitions(boot_mtd, &boot_parts,
								  ULONG_MAX);
			if (boot_parts_qty > 0) {
				printk(KERN_NOTICE "Using RedBoot partition definition\n");
				add_mtd_partitions(boot_mtd, boot_parts, boot_parts_qty);
			} else
#endif
			add_mtd_device(boot_mtd);

			ret = 0;
		}
	}

#ifndef CONFIG_MB93093_PDK
	/* contemplate the user flash */
	mb93031_user_flash_map.map_priv_1 =
		(unsigned long) ioremap(0xFF200000, 0x00200000);

	if (!mb93031_user_flash_map.map_priv_1) {
		printk("Failed to ioremap\n");
		if (ret)
			ret = -EIO;
	}
	else {
		user_mtd = do_map_probe("cfi_probe", &mb93031_user_flash_map);
		if (!user_mtd) {
			iounmap((void *) mb93031_user_flash_map.map_priv_1);
		}
		else {
			/* okay... we've found a valid flash chip */
			user_mtd->module = THIS_MODULE;

#if defined(CONFIG_MTD_PARTITIONS) && defined(CONFIG_MTD_REDBOOT_PARTS)
			user_parts_qty = parse_redboot_partitions(user_mtd, &user_parts,
								  ULONG_MAX);
			if (user_parts_qty > 0) {
				printk(KERN_NOTICE "Using RedBoot partition definition\n");
				add_mtd_partitions(user_mtd, user_parts, user_parts_qty);
			} else
#endif
			add_mtd_device(user_mtd);

			ret = 0;
		}
	}
#endif // PDK403

	return ret;
} /* end mb93031_maps_init() */

/*****************************************************************************/
/*
 * 
 */
static void __exit mb93031_maps_cleanup(void)
{
	if (boot_parts)
		del_mtd_partitions(boot_mtd);
	map_destroy(boot_mtd);
	iounmap((void *) mb93031_boot_flash_map.map_priv_1);

#ifndef CONFIG_MB93093_PDK
	if (user_parts)
		del_mtd_partitions(user_mtd);
	map_destroy(user_mtd);
	iounmap((void *) mb93031_user_flash_map.map_priv_1);
#endif //PDK403

} /* end mb93031_maps_cleanup() */

module_init(mb93031_maps_init);
module_exit(mb93031_maps_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Red Hat, Inc. - David Howells <dhowells@redhat.com>");
MODULE_DESCRIPTION("MTD map driver for Fujitsu MB93031 VDK motherboard");
