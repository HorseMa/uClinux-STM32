/*
 * MTD map driver for flash on the DC21285 (the StrongARM-110 companion chip)
 *
 * (C) 2000  Nicolas Pitre <nico@cam.org>
 *
 * This code is GPL
 * 
 * $Id: dc21285.c,v 1.9 2002/10/14 12:22:10 rmk Exp $
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>


static struct mtd_info *mymtd;

__u8 eb67dip_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8*)(map->map_priv_1 + ofs);
}

__u16 eb67dip_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16*)(map->map_priv_1 + ofs);
}

__u32 eb67dip_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32*)(map->map_priv_1 + ofs);
}

void eb67dip_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void*)(map->map_priv_1 + from), len);
}

void eb67dip_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8*)(map->map_priv_1 + adr) = d;
}

void eb67dip_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16*)(map->map_priv_1 + adr) = d;
}

void eb67dip_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32*)(map->map_priv_1 + adr) = d;
}

void eb67dip_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void*)(map->map_priv_1 + from), to, len);
}

struct map_info eb67dip_map = {
	name: "EB67DIP flash",
	size: 16*1024*1024,
	read8: eb67dip_read8,
	read16: eb67dip_read16,
	read32: eb67dip_read32,
	copy_from: eb67dip_copy_from,
	write8: eb67dip_write8,
	write16: eb67dip_write16,
	write32: eb67dip_write32,
	copy_to: eb67dip_copy_to
};


/* Partition stuff */
static struct mtd_partition *eb67dip_parts;
		      
extern int parse_redboot_partitions(struct mtd_info *, struct mtd_partition **);

int __init init_eb67dip(void)
{
  printk (KERN_NOTICE "EB67DIP flash support \n");

	eb67dip_map.buswidth = 2;

	/* Let's map the flash area */
	eb67dip_map.map_priv_1 = (unsigned long)ioremap_nocache(0xC8000000, 16*1024*1024);
	if (!eb67dip_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}

	mymtd = do_map_probe("jedec_probe", &eb67dip_map);
	if (mymtd) {
		int nrparts = 0;

		mymtd->module = THIS_MODULE;
			
		/* partition fixup */

#ifdef CONFIG_MTD_REDBOOT_PARTS
		nrparts = parse_redboot_partitions(mymtd, &eb67dip_parts);
#endif
		if (nrparts > 0) {
			add_mtd_partitions(mymtd, eb67dip_parts, nrparts);
		} else if (nrparts == 0) {
			printk(KERN_NOTICE "RedBoot partition table failed\n");
			add_mtd_device(mymtd);
		}

		return 0;
	}

	iounmap((void *)eb67dip_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_eb67dip(void)
{
	if (mymtd) {
		del_mtd_device(mymtd);
		map_destroy(mymtd);
		mymtd = NULL;
	}
	if (eb67dip_map.map_priv_1) {
		iounmap((void *)eb67dip_map.map_priv_1);
		eb67dip_map.map_priv_1 = 0;
	}
	if(eb67dip_parts)
		kfree(eb67dip_parts);
}

module_init(init_eb67dip);
module_exit(cleanup_eb67dip);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicolas Pitre <nico@cam.org>, Ben Dooks <ben@simtec.co.uk>");
MODULE_DESCRIPTION("MTD map driver for EB67DIP boards");
