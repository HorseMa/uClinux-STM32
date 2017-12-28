/****************************************************************************/
/*
 * Flash memory access on Feith Sensor to Image GmbH devices
 * Copyright (C) Feith Sensor to Image GmbH, Roman Wagner <rw@feith.de>
 *
 * Mostly hijacked from nettel-uc.c
 * Copyright (C) Lineo, <davidm@moreton.com.au>
 */
/****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <linux/fs.h>

#include <asm/io.h>
#include <asm/delay.h>

/****************************************************************************/

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

#define SIZE_2MB	(2 * 1024 * 1024)
#define SIZE_4MB	(4 * 1024 * 1024)
#define SIZE_6MB	(6 * 1024 * 1024)
#define SIZE_8MB	(8 * 1024 * 1024)

#define FLASH_BASE	0xf0000000
#define	BUS_WIDTH	2

/*
 *	If you set AUTO_PROBE,  then we will just use the biggest device
 *	we can find
 */
#undef AUTO_PROBE

/****************************************************************************/

static __u8 Feith_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 Feith_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 Feith_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void Feith_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void Feith_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void Feith_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void Feith_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void Feith_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

/****************************************************************************/

static struct map_info Feith_flash_map = {
	name:		"Flash",
	read8:		Feith_read8,
	read16:		Feith_read16,
	read32:		Feith_read32,
	copy_from:	Feith_copy_from,
	write8:		Feith_write8,
	write16:	Feith_write16,
	write32:	Feith_write32,
	copy_to:	Feith_copy_to,
};

static struct map_info Feith_ram_map = {
	name:		"RAM",
	read8:		Feith_read8,
	read16:		Feith_read16,
	read32:		Feith_read32,
	copy_from:	Feith_copy_from,
	write8:		Feith_write8,
	write16:	Feith_write16,
	write32:	Feith_write32,
	copy_to:	Feith_copy_to,
};

static struct mtd_info *ram_mtdinfo;
static struct mtd_info *flash_mtdinfo;

/****************************************************************************/

static struct mtd_partition Feith_romfs[] = {
	{ name: "romfs", offset: 0 }
};

/****************************************************************************/
/*
 *	The layout of our flash,  note the order of the names,  this
 *	means we use the same major/minor for the same purpose on all
 *	layouts (when possible)
 */

#if defined(CONFIG_CLEOPATRA) || defined(CONFIG_SCALES)
static struct mtd_partition Feith_2mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00002000 },
	{ name: "MAC",        offset: 0x00006000, size:   0x00002000 },
	{ name: "Image",      offset: 0x00020000, size:   0x001c0000 },
	{ name: "Config",     offset: 0x00010000, size:   0x00010000 },
	{ name: "Flash",      offset: 0x00000000, size:   0x00200000 },
	{ name: "Spare",      offset: 0x00008000, size:   0x00008000 },
	{ name: "Config User",offset: 0x001e0000, size:   0x00020000 }
};

static struct mtd_partition Feith_4mb[] = {
	{ name: "Bootloader",  offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",    offset: 0x00004000, size:   0x00002000 },
	{ name: "MAC",         offset: 0x00006000, size:   0x00002000 },
	{ name: "Image",       offset: 0x00020000, size:   0x001c0000 },
	{ name: "Config",      offset: 0x00010000, size:   0x00010000 },
	{ name: "Flash",       offset: 0x00000000, size:   0x00200000 },
	{ name: "Spare",       offset: 0x00008000, size:   0x00008000 },
	{ name: "Config User", offset: 0x001e0000, size:   0x00020000 },
	{ name: "User Block 1",offset: 0x00200000, size:   0x00180000 },
	{ name: "User Block 2",offset: 0x00380000, size:   0x00080000 }
};

static struct mtd_partition Feith_6mb[] = {
	{ name: "Bootloader",  offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",    offset: 0x00004000, size:   0x00002000 },
	{ name: "MAC",         offset: 0x00006000, size:   0x00002000 },
	{ name: "Image",       offset: 0x00020000, size:   0x001c0000 },
	{ name: "Config",      offset: 0x00010000, size:   0x00010000 },
	{ name: "Flash",       offset: 0x00000000, size:   0x00200000 },
	{ name: "Spare",       offset: 0x00008000, size:   0x00008000 },
	{ name: "Config User", offset: 0x001e0000, size:   0x00020000 },
	{ name: "User Block 1",offset: 0x00200000, size:   0x00180000 },
	{ name: "User Block 2",offset: 0x00380000, size:   0x00080000 },
	{ name: "User Block 3",offset: 0x00400000, size:   0x00200000 }
};
#endif

#if defined(CONFIG_CANCAM)
static struct mtd_partition Feith_8mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00010000 },
	{ name: "Bootargs",   offset: 0x00010000, size:   0x00010000 },
	{ name: "MAC",        offset: 0x00020000, size:   0x00010000 },
	{ name: "Image",      offset: 0x00050000, size:   0x001b0000 },
	{ name: "Config",     offset: 0x00040000, size:   0x00010000 },
	{ name: "Flash",      offset: 0x00000000, size:   0x00800000 },
	{ name: "Spare",      offset: 0x00030000, size:   0x00010000 },
	{ name: "Config User",offset: 0x00200000, size:   0x00600000 }
};
#endif


/****************************************************************************/
/*
 * Find the MTD device with the given name
 */

static struct mtd_info *get_mtd_named(char *name)
{
	int i;
	struct mtd_info *mtd;

	for (i = 0; i < MAX_MTD_DEVICES; i++) {
		mtd = get_mtd_device(NULL, i);
		if (mtd) {
			if (strcmp(mtd->name, name) == 0)
				return(mtd);
			put_mtd_device(mtd);
		}
	}
	return(NULL);
}

/****************************************************************************/

static int
Feith_point(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char **mtdbuf)
{
	struct map_info *map = (struct map_ptr *) mtd->priv;
	*mtdbuf = map->map_priv_1 + from;
	*retlen = len;
	return(0);
}

/****************************************************************************/

static int __init
Feith_probe(int ram, unsigned long addr, int size, int buswidth)
{
	struct mtd_info *mymtd;
	struct map_info *map_ptr;

	if (ram)
		map_ptr = &Feith_ram_map;
	else
		map_ptr = &Feith_flash_map;

	map_ptr->buswidth = buswidth;
	map_ptr->map_priv_2 = addr;
	map_ptr->size = size;

	printk(KERN_NOTICE "Feith MTD %s probe(0x%lx,%d,%d): %lx at %lx\n",
			ram ? "ram" : "flash",
			addr, size, buswidth, map_ptr->size, map_ptr->map_priv_2);

	map_ptr->map_priv_1 = (unsigned long)
			ioremap_nocache(map_ptr->map_priv_2, map_ptr->size);

	if (!map_ptr->map_priv_1) {
		printk("Failed to ioremap_nocache\n");
		return -EIO;
	}

	if (!ram) {
		mymtd = do_map_probe("cfi_probe", map_ptr);
	} else {
		mymtd = do_map_probe("map_ram", map_ptr);
	}

	if (!mymtd) {
		iounmap((void *)map_ptr->map_priv_1);
		return -ENXIO;
	}

	mymtd->module = THIS_MODULE;
	mymtd->point = Feith_point;
	mymtd->priv = map_ptr;

	if (ram) {
		ram_mtdinfo = mymtd;
		add_mtd_partitions(mymtd, Feith_romfs, NB_OF(Feith_romfs));
		return(0);
	}

	flash_mtdinfo = mymtd;
	switch (size) {
	case SIZE_2MB:
		add_mtd_partitions(mymtd, Feith_2mb, NB_OF(Feith_2mb));
		break;
	case SIZE_4MB:
		add_mtd_partitions(mymtd, Feith_4mb, NB_OF(Feith_4mb));
		break;
	case SIZE_6MB:
		add_mtd_partitions(mymtd, Feith_6mb, NB_OF(Feith_6mb));
		break;
#if defined(CONFIG_CANCAM)
	case SIZE_8MB:
		add_mtd_partitions(mymtd, Feith_8mb, NB_OF(Feith_8mb));
		break;
#endif
	}

	return 0;
}

/****************************************************************************/

int __init Feith_mtd_init(void)
{
	int rc = -1;
	struct mtd_info *mtd;
	extern char _ebss;

	/*
	 * I hate this ifdef stuff,  but our HW doesn't always have
	 * the same chipsize as the map that we use
	 */
#if defined(CONFIG_CANCAM)
#if defined(CONFIG_FLASH8MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = Feith_probe(0, FLASH_BASE, SIZE_8MB, BUS_WIDTH);
#endif
#endif

#if defined(CONFIG_FLASH6MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = Feith_probe(0, FLASH_BASE, SIZE_6MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH4MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = Feith_probe(0, FLASH_BASE, SIZE_4MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH2MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = Feith_probe(0, FLASH_BASE, SIZE_2MB, BUS_WIDTH);
#endif

	/*
	 * Map in the filesystem from RAM last so that,  if the filesystem
	 * is not in RAM for some reason we do not change the minor/major
	 * for the flash devices
	 */
#ifndef CONFIG_ROMFS_FROM_ROM
	if (0 != Feith_probe(1, (unsigned long) &_ebss,
			PAGE_ALIGN(* (unsigned long *)((&_ebss) + 8)), 4))
		printk("Failed to probe RAM filesystem 1\n");
#else
	{
		unsigned long start_area;
		unsigned char *sp, *ep;
		size_t len;

		start_area = (unsigned long) &_ebss;

		if (strncmp((char *) start_area, "-rom1fs-", 8) != 0) {
			mtd = get_mtd_named("Image");
			if (mtd && mtd->point) {
				if ((*mtd->point)(mtd, 0, mtd->size, &len, &sp) == 0) {
					ep = sp + len;
					while (sp < ep && strncmp(sp, "-rom1fs-", 8) != 0)
						sp++;
					if (sp < ep)
						start_area = (unsigned long) sp;
				}
			}
			if (mtd)
				put_mtd_device(mtd);
		}
		if (0 != Feith_probe(1, start_area,
				PAGE_ALIGN(* (unsigned long *)(start_area + 8)), 4))
			printk("Failed to probe RAM filesystem 2\n");
	}
#endif
	
	mtd = get_mtd_named("romfs");
	if (mtd) {
		ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
		put_mtd_device(mtd);
	}

	return(rc);
}

/****************************************************************************/

static void __exit Feith_mtd_cleanup(void)
{
	if (flash_mtdinfo) {
		del_mtd_partitions(flash_mtdinfo);
		map_destroy(flash_mtdinfo);
		flash_mtdinfo = NULL;
	}
	if (ram_mtdinfo) {
		del_mtd_partitions(ram_mtdinfo);
		map_destroy(ram_mtdinfo);
		ram_mtdinfo = NULL;
	}
	if (Feith_ram_map.map_priv_1) {
		iounmap((void *)Feith_ram_map.map_priv_1);
		Feith_ram_map.map_priv_1 = 0;
	}
	if (Feith_flash_map.map_priv_1) {
		iounmap((void *)Feith_flash_map.map_priv_1);
		Feith_flash_map.map_priv_1 = 0;
	}
}

/****************************************************************************/

module_init(Feith_mtd_init);
module_exit(Feith_mtd_cleanup);

/****************************************************************************/
