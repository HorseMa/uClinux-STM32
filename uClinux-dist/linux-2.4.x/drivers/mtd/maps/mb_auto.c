/****************************************************************************/
/*
 * Flash memory access on default auto-config Microblaze uClinux systems 
 * 
 * Copyright (C) 2003-2005  John Williams <jwilliams@itee.uq.edu.au>
 *  based upon NETTEL_uc support, which is 
 * Copyright (C) 2001-2002, David McCullough <davidm@snapgear.com>
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

/* Handle both romfs and cramfs types, without generating unnecessary
   code (ie no point checking for CRAMFS if it's not even enabled).
   This fn is also used in probing, sinec it does the magic number check, 
   returning zero if there's no match */
extern unsigned get_romfs_len(unsigned long *addr);

/****************************************************************************/

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

#define SIZE_128K	(128 * 1024)
#define SIZE_1MB	(1024 * 1024)
#define SIZE_2MB	(2 * 1024 * 1024)
#define SIZE_4MB	(4 * 1024 * 1024)
#define SIZE_8MB	(8 * 1024 * 1024)

#define FLASH_BASE	CONFIG_XILINX_FLASH_START
#define BUS_WIDTH	4

/****************************************************************************/

static __u8 mb_auto_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 mb_auto_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 mb_auto_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void mb_auto_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void mb_auto_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void mb_auto_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void mb_auto_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void mb_auto_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

/****************************************************************************/

static struct map_info mb_auto_flash_map = {
	name:		"Flash",
	read8:		mb_auto_read8,
	read16:		mb_auto_read16,
	read32:		mb_auto_read32,
	copy_from:	mb_auto_copy_from,
	write8:		mb_auto_write8,
	write16:	mb_auto_write16,
	write32:	mb_auto_write32,
	copy_to:	mb_auto_copy_to,
};

static struct map_info mb_auto_ram_map = {
	name:		"RAM",
	read8:		mb_auto_read8,
	read16:		mb_auto_read16,
	read32:		mb_auto_read32,
	copy_from:	mb_auto_copy_from,
	write8:		mb_auto_write8,
	write16:	mb_auto_write16,
	write32:	mb_auto_write32,
	copy_to:	mb_auto_copy_to,
};

static struct mtd_info *ram_mtdinfo;
static struct mtd_info *flash_mtdinfo;

/****************************************************************************/

static struct mtd_partition mb_auto_romfs[] = {
	{ name: "Romfs", offset: 0 }
};

/****************************************************************************/
/*
 *	The layout of our flash,  note the order of the names,  this
 *	means we use the same major/minor for the same purpose on all
 *	layouts (when possible)
 */

static struct mtd_partition mb_auto_128k[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00004000 },
	{ name: "MAC",        offset: 0x00008000, size:   0x00004000 },
	{ name: "Config",     offset: 0x00010000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x0000c000, size:   0x00004000 },
	{ name: "Flash",      offset: 0 }
};

static struct mtd_partition mb_auto_1mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00002000 },
	{ name: "MAC",        offset: 0x00006000, size:   0x00002000 },
	{ name: "Config",     offset: 0x000f0000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x00008000, size:   0x00008000 },
	{ name: "Image",      offset: 0x00010000, size:   0x000e0000 },
	{ name: "Flash",      offset: 0 }
};

static struct mtd_partition mb_auto_2mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00002000 },
	{ name: "MAC",        offset: 0x00006000, size:   0x00002000 },
	{ name: "Config",     offset: 0x00010000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x00008000, size:   0x00008000 },
	{ name: "Image",      offset: 0x00020000, size:   0x001e0000 },
	{ name: "Flash",      offset: 0 }
};

static struct mtd_partition mb_auto_4mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00004000 },
	{ name: "MAC",        offset: 0x00008000, size:   0x00004000 },
	{ name: "Config",     offset: 0x00010000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x0000C000, size:   0x00004000 },
	{ name: "Image",      offset: 0x00100000, size:   0x00300000 },
	{ name: "JFFS2",      offset: 0x00400000, size:   0x00000000 },
	{ name: "Flash",     offset: 0 }
};

static struct mtd_partition mb_auto_8mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00004000 },
	{ name: "MAC",        offset: 0x00008000, size:   0x00004000 },
	{ name: "Config",     offset: 0x00010000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x0000C000, size:   0x00004000 },
	{ name: "Image",      offset: 0x00100000, size:   0x00300000 },
	{ name: "JFFS2",      offset: 0x00400000, size:   0x00400000 },
	{ name: "Flash",     offset: 0 }
};

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
#ifdef CONFIG_MTD_CFI_INTELEXT
/*
 *	Set the Intel flash back to read mode as MTD may leave it in command mode
 */

static int mb_auto_reboot_notifier(
	struct notifier_block *nb,
	unsigned long val,
	void *v)
{
	struct cfi_private *cfi = mb_auto_flash_map.fldrv_priv;
	int i;
	
	for (i = 0; cfi && i < cfi->numchips; i++)
		cfi_send_gen_cmd(0xff, 0x55, cfi->chips[i].start, &mb_auto_flash_map,
				cfi, cfi->device_type, NULL);

	return(NOTIFY_OK);
}

static struct notifier_block mb_auto_notifier_block = {
	mb_auto_reboot_notifier, NULL, 0
};

#endif

/****************************************************************************/

static int
mb_auto_point(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char **mtdbuf)
{
	struct map_info *map = (struct map_info *) mtd->priv;
	*mtdbuf = (u_char *) (map->map_priv_1 + (int)from);
	*retlen = len;
	return(0);
}

static void
mb_auto_unpoint (struct mtd_info *mtd, u_char *addr, loff_t from, size_t len)
{
}

/****************************************************************************/

static int __init
mb_auto_probe(int ram, unsigned long addr, int size, int buswidth)
{
	struct mtd_info *mymtd;
	struct map_info *map_ptr;

	if (ram)
		map_ptr = &mb_auto_ram_map;
	else
		map_ptr = &mb_auto_flash_map;

	map_ptr->buswidth = buswidth;
	map_ptr->map_priv_2 = addr;
	map_ptr->size = size;

	printk(KERN_NOTICE "MicroBlaze auto-config %s probe(0x%lx,%d,%d): %lx at %lx\n",
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
		if (!mymtd)
			mymtd = do_map_probe("jedec_probe", map_ptr);
	} else
		mymtd = do_map_probe("map_ram", map_ptr);

	if (!mymtd) {
		iounmap((void *)map_ptr->map_priv_1);
		return -ENXIO;
	}
		
	mymtd->module = THIS_MODULE;
	mymtd->point = mb_auto_point;
	mymtd->unpoint = mb_auto_unpoint;
	mymtd->priv = map_ptr;

	if (ram) {
		ram_mtdinfo = mymtd;
		add_mtd_partitions(mymtd, mb_auto_romfs, NB_OF(mb_auto_romfs));
		return(0);
	}

	flash_mtdinfo = mymtd;
	switch (size) {
	case SIZE_128K:
		add_mtd_partitions(mymtd, mb_auto_128k, NB_OF(mb_auto_128k));
		break;
	case SIZE_1MB:
		add_mtd_partitions(mymtd, mb_auto_1mb, NB_OF(mb_auto_1mb));
		break;
	case SIZE_2MB:
		add_mtd_partitions(mymtd, mb_auto_2mb, NB_OF(mb_auto_2mb));
		break;
	case SIZE_4MB:
		add_mtd_partitions(mymtd, mb_auto_4mb, NB_OF(mb_auto_4mb));
		break;
	case SIZE_8MB:
		add_mtd_partitions(mymtd, mb_auto_8mb, NB_OF(mb_auto_8mb));
		break;
	}

	return 0;
}

/****************************************************************************/

int __init mb_auto_mtd_init(void)
{
	int rc = -1;
	struct mtd_info *mtd;
	extern char _ebss, __bss_lma;
	void *romfs_loc;
	unsigned int romfs_size;

	/*
	 * I hate this ifdef stuff,  but our HW doesn't always have
	 * the same chipsize as the map that we use
	 */
#if defined(CONFIG_FLASH8MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = mb_auto_probe(0, FLASH_BASE, SIZE_8MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH4MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = mb_auto_probe(0, FLASH_BASE, SIZE_4MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH2MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = mb_auto_probe(0, FLASH_BASE, SIZE_2MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH1MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = mb_auto_probe(0, FLASH_BASE, SIZE_1MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH128K) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = mb_auto_probe(0, FLASH_BASE, SIZE_128K, BUS_WIDTH);
#endif

	/* Determine where the ROMFS/CRAMFS image should be, depending
	   upon memory model */
#ifdef CONFIG_MODEL_RAM
	romfs_loc = &_ebss;
#endif
#ifdef CONFIG_MODEL_ROM
	romfs_loc = &__bss_lma;
#endif

	romfs_size = get_romfs_len(romfs_loc);

	/*
	 * Map in the filesystem from RAM last so that,  if the filesystem
	 * is not in RAM for some reason we do not change the minor/major
	 * for the flash devices
	 */
#ifndef CONFIG_SEARCH_ROMFS
	if (0 != mb_auto_probe(1, (unsigned long) romfs_loc,
			PAGE_ALIGN(romfs_size),4))
		printk("Failed to probe RAM filesystem\n");
#else
	{
		unsigned long start_area;
		unsigned char *sp, *ep;
		size_t len;

		printk("Probing for ROMFS...\n");
		start_area = (unsigned long) romfs_loc;

		if (!get_romfs_len(start_area)) {
			mtd = get_mtd_named("Image");
			if (mtd && mtd->point) {
				if ((*mtd->point)(mtd, 0, mtd->size, &len, &sp) == 0) {
					ep = sp + len;
					while (sp < ep && !get_romfs_len(sp))
						sp++;
					if (sp < ep)
						start_area = (unsigned long) sp;
				}
			}
			if (mtd)
				put_mtd_device(mtd);
		}
		if (0 != mb_auto_probe(1, start_area,
				PAGE_ALIGN(get_romfs_len(start_area)), 4))
			printk("Failed to probe RAM filesystem\n");
	}
#endif	
	mtd = get_mtd_named("Romfs");
	if (mtd) {
		ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
		put_mtd_device(mtd);
	}
	return(rc);
}

/****************************************************************************/

static void __exit mb_auto_mtd_cleanup(void)
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
	if (mb_auto_ram_map.map_priv_1) {
		iounmap((void *)mb_auto_ram_map.map_priv_1);
		mb_auto_ram_map.map_priv_1 = 0;
	}
	if (mb_auto_flash_map.map_priv_1) {
		iounmap((void *)mb_auto_flash_map.map_priv_1);
		mb_auto_flash_map.map_priv_1 = 0;
	}
}

/****************************************************************************/

module_init(mb_auto_mtd_init);
module_exit(mb_auto_mtd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Williams <jwilliams@itee.uq.edu.au>");
MODULE_DESCRIPTION("Microblaze/uClinux-auto flash support");

/****************************************************************************/
