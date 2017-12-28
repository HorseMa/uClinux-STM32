/****************************************************************************/
/*
 * Flash memory access on uClinux SIOS III like devices
 * Copyright (C) 2001-2002, David McCullough <davidm@snapgear.com>
 * Copyright (C) 2004, SED Systems <hamilton@sedsystems.ca>
 */
/****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/reboot.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nftl.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/cfi.h>

#include <linux/fs.h>

#include <asm/io.h>
#include <asm/delay.h>
/****************************************************************************/
#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))
#define SIZE_128K	(1  *  128 * 1024)
#define SIZE_1MB	(1  * 1024 * 1024)
#define SIZE_2MB	(2  * 1024 * 1024)
#define SIZE_4MB	(4  * 1024 * 1024)
#define SIZE_8MB	(8  * 1024 * 1024)
#define SIZE_16MB	(16 * 1024 * 1024)

#define FLASH_BASE	0x7fc00000
#define	BUS_WIDTH	2
/****************************************************************************/

static __u8 sedcfc_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 sedcfc_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 sedcfc_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void sedcfc_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void sedcfc_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void sedcfc_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void sedcfc_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void sedcfc_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

/****************************************************************************/

static struct map_info sedcfc_flash_map =
{
	name:		"Flash",
	read8:		sedcfc_read8,
	read16:		sedcfc_read16,
	read32:		sedcfc_read32,
	copy_from:	sedcfc_copy_from,
	write8:		sedcfc_write8,
	write16:	sedcfc_write16,
	write32:	sedcfc_write32,
	copy_to:	sedcfc_copy_to,
};

static struct mtd_info *flash_mtdinfo;

/****************************************************************************/
/*
 *	The layout of our flash,  note the order of the names,  this
 *	means we use the same major/minor for the same purpose on all
 *	layouts (when possible)
 */
static struct mtd_partition sedcfc_4mb[] =
{
	{ name: "Colilo",     offset: 0x00000000, size:   0x00010000 },
	{ name: "Kernel",     offset: 0x00010000, size:   0x00070000 },
	{ name: "RootFS",     offset: 0x00080000, size:   0x00180000 },
	{ name: "SEDFS",      offset: 0x00200000, size:   0x00200000 }
};

/****************************************************************************/
/*
 * Find the MTD device with the given name
 */

static struct mtd_info *get_mtd_named(char *name)
{
	int i;
	struct mtd_info *mtd;

	for (i = 0; i < MAX_MTD_DEVICES; i++)
	{
		mtd = get_mtd_device(NULL, i);
		if (mtd)
		{
			if (strcmp(mtd->name, name) == 0)
				return(mtd);
			put_mtd_device(mtd);
		}
	}
	return(NULL);
}

/****************************************************************************/
static int sedcfc_point(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char **mtdbuf)
{
	struct map_info *map = (struct map_info *) mtd->priv;
	*mtdbuf = (u_char *) (map->map_priv_1 + (int)from);
	*retlen = len;
	return(0);
}
/****************************************************************************/
static void
sedcfc_unpoint(struct mtd_info *mtd, u_char *addr, loff_t from, size_t len)
{
}
/****************************************************************************/
static int __init
sedcfc_probe(int ram, unsigned long addr, int size, int buswidth)
{
	struct mtd_info *mymtd;
	struct map_info *map_ptr;
	struct mtd_info *mtd;

	map_ptr = &sedcfc_flash_map;

	map_ptr->buswidth = buswidth;
	map_ptr->map_priv_2 = addr;
	map_ptr->size = size;

	printk(KERN_NOTICE "SED SIOS III Core flash probe(0x%lx,%d,%d): %lx at %lx\n",
			addr, size, buswidth, map_ptr->size,
			map_ptr->map_priv_2);

	map_ptr->map_priv_1 = (unsigned long)
		ioremap_nocache(map_ptr->map_priv_2, map_ptr->size);

	if (!map_ptr->map_priv_1)
	{
		printk("Failed to ioremap_nocache\n");
		return -EIO;
	}

	mymtd = do_map_probe("amd_flash", map_ptr);

	if(!mymtd)
	{
		printk("Failed to find device doing amd_flash,"
				" trying cfi_probe\n");
		mymtd = do_map_probe("cfi_probe", map_ptr);
	}

	if (!mymtd)
	{
		printk("Failed to find device doing cfi_probe\n");
		iounmap((void *)map_ptr->map_priv_1);
		return -ENXIO;
	}

	mymtd->module  = THIS_MODULE;
	mymtd->point   = sedcfc_point;
	mymtd->unpoint = sedcfc_unpoint;
	mymtd->priv    = map_ptr;

	flash_mtdinfo = mymtd;
	switch (size)
	{
		case SIZE_4MB:
			add_mtd_partitions(mymtd, sedcfc_4mb,
					NB_OF(sedcfc_4mb));
			break;
	}
	mtd = get_mtd_named("RootFS");
	if (mtd)
	{
		ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
		put_mtd_device(mtd);
	}
	else
	{
		printk("Root device not set\n");
	}

	return 0;
}

/****************************************************************************/

int __init sedcfc_mtd_init(void)
{
	int rc = -1;
	struct mtd_info *mtd;

	printk(KERN_NOTICE "Initializing flash devices for filesystems\n");

	rc = sedcfc_probe(0, FLASH_BASE, SIZE_4MB, BUS_WIDTH);
	
	mtd = get_mtd_named("RootFS");
	if (mtd)
	{
		ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
		put_mtd_device(mtd);
	}
	return(rc);
}
/****************************************************************************/
static void __exit sedcfc_mtd_cleanup(void)
{
	if (flash_mtdinfo)
	{
		del_mtd_partitions(flash_mtdinfo);
		map_destroy(flash_mtdinfo);
		flash_mtdinfo = NULL;
	}
	if (sedcfc_flash_map.map_priv_1)
	{
		iounmap((void *)sedcfc_flash_map.map_priv_1);
		sedcfc_flash_map.map_priv_1 = 0;
	}
}
/****************************************************************************/
module_init(sedcfc_mtd_init);
module_exit(sedcfc_mtd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David McCullough <davidm@snapgear.com>");
MODULE_DESCRIPTION("SED SIOS III Flash support for uClinux");
/****************************************************************************/
