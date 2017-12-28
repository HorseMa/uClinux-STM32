/* 
 * smart.c -- MTD map driver for SIEMENS smart devices (per commandline)
 *
 * Copyright (C) 2004 Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * Copyright (C) 2001 Mark Langsdorf (mark.langsdorf@amd.com)
 *	based on sc520cdp.c by Sysgo Real-Time Solutions GmbH
 *
 * $Id: smart.c,v 1.2 2004/11/04 16:19:56 the Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/io.h>

#ifndef CONFIG_MTD_CMDLINE_PARTS
#error you have to enable CONFIG_MTD_CMDLINE_PARTS for using this map
#endif

/* ( external, see drivers/mtd/cmdlinepart.c ) */
int parse_cmdline_partitions(struct mtd_info *master,
			     struct mtd_partition **pparts,
			     const char *mtd_id);

static __u8 smart_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 smart_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static __u32 smart_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void smart_copy_from(struct map_info *map, void *to, unsigned long from,
			    ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void smart_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
}

static void smart_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
}

static void smart_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
}

static void smart_copy_to(struct map_info *map, unsigned long to,
			  const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info smart_ro_map = {
	name:		"Cmdline Flash Bank - RO",
	size:		0xFFFFFFFF,
	buswidth:	2,
	read8:		smart_read8,
	read16:		smart_read16,
	read32:		smart_read32,
	copy_from:	smart_copy_from,
	write8:		0,
	write16:	0,
	write32:	0,
	copy_to:	0,
	map_priv_1:	FLASH_MEM_BASE, /* (from config.h) */
	map_priv_2:	FLASH_MEM_BASE,
};

static struct map_info smart_rw_map = {
	name:		"Cmdline Flash Bank - RW",
	size:		0xFFFFFFFF,
	buswidth:	2,
	read8:		smart_read8,
	read16:		smart_read16,
	read32:		smart_read32,
	copy_from:	smart_copy_from,
	write8:		smart_write8,
	write16:	smart_write16,
	write32:	smart_write32,
	copy_to:	smart_copy_to,
	map_priv_1:	FLASH_MEM_BASE,
	map_priv_2:	FLASH_MEM_BASE,
};

static struct mtd_info *mymtd_ro;
static struct mtd_info *mymtd_rw;
static int                   mtd_parts_nb_ro = 0;
static int                   mtd_parts_nb_rw = 0;
static struct mtd_partition *mtd_parts_ro    = 0;
static struct mtd_partition *mtd_parts_rw    = 0;

static int __init init_smart_mtd(void)
{
	/* create read-only devices */
	while (!mymtd_ro) {
		/* parse the rom partitions from cmdline */
		mtd_parts_nb_ro = parse_cmdline_partitions(mymtd_ro,
			&mtd_parts_ro, "rom");
		if (mtd_parts_nb_ro < 1)
			break;

		/* determine size of the "rom" bank */
		smart_ro_map.size = mtd_parts_ro[mtd_parts_nb_ro-1].offset +
		                    mtd_parts_ro[mtd_parts_nb_ro-1].size;

		/* probe and add the "rom" devices */
		mymtd_ro = do_map_probe("map_rom", &smart_ro_map);
		if (!mymtd_ro)
			break;
		mymtd_ro->module = THIS_MODULE;

		add_mtd_partitions(mymtd_ro, mtd_parts_ro, mtd_parts_nb_ro);

		/* adjust the start of the "flash" bank */
		smart_rw_map.map_priv_1 =
			smart_ro_map.map_priv_1 + smart_ro_map.size;
		smart_rw_map.map_priv_2 = smart_rw_map.map_priv_1;

		printk(KERN_NOTICE "SmaRT flash dev (RO): %lukB @ 0x%08lX\n",
			smart_ro_map.size >> 10, smart_ro_map.map_priv_2);
		break;
	}

	/* create read-write devices */
	while (!mymtd_rw) {
		/* parse the flash partitions from cmdline */
		mtd_parts_nb_rw = parse_cmdline_partitions(mymtd_rw,
			&mtd_parts_rw, "flash");
		if (mtd_parts_nb_rw < 1)
			break;

		/* determine size of the "flash" bank */
		smart_rw_map.size = mtd_parts_rw[mtd_parts_nb_rw-1].offset +
		                    mtd_parts_rw[mtd_parts_nb_rw-1].size;

		/* probe and add the "flash" devices */
		mymtd_rw = do_map_probe("cfi_probe", &smart_rw_map);
		if (!mymtd_rw)
			mymtd_rw = do_map_probe("jedec_probe", &smart_rw_map);
		if (!mymtd_rw)
			break;

		mymtd_rw->module = THIS_MODULE;

		add_mtd_partitions(mymtd_rw, mtd_parts_rw, mtd_parts_nb_rw);

		printk(KERN_NOTICE "SmaRT flash dev (RW): %lukB @ 0x%08lX\n",
		       smart_rw_map.size >> 10, smart_rw_map.map_priv_2);
		break;
	}

	if (!mymtd_rw) {
		iounmap((void *)smart_rw_map.map_priv_1);
		return -ENXIO;
	}

	mymtd_rw->module = THIS_MODULE;
	return 0;
}

static void __exit cleanup_smart_mtd(void)
{
	if (mymtd_ro) {
		del_mtd_partitions(mymtd_ro);
		map_destroy(mymtd_ro);
	}
	if (mymtd_rw) {
		del_mtd_partitions(mymtd_rw);
		map_destroy(mymtd_rw);
	}
}

module_init(init_smart_mtd);
module_exit(cleanup_smart_mtd);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thomas Eschenbacher <eschenbacher@sympat.de>");
MODULE_DESCRIPTION("MTD map driver for comdline mappings on SIEMENS SmaRT");
