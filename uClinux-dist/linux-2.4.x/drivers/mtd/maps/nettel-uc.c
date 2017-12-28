/****************************************************************************/
/*
 * Flash memory access on uClinux SnapGear like devices
 * 
 * Copyright (C) 2001-2002, David McCullough <davidm@snapgear.com>
 *
 * M5206eC3 FLASH support (added by Christian Magnusson <mag@mag.cx>)
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
#define SIZE_32MB	(32 * 1024 * 1024)

#ifdef CONFIG_UCDIMM
#define FLASH_BASE	0x10c00000
#define	BUS_WIDTH	2
#ifdef CONFIG_RAMKERNEL
#define ROMFS_LOC	((&__rom_start) + ((&_edata) - (&__ram_start)))
extern char __rom_start, _edata, __ram_start;
#else
#define ROMFS_LOC	(&_edata)
extern char _edata;
#endif
#endif

#ifdef CONFIG_COLDFIRE
#ifdef CONFIG_M5206eC3
#define FLASH_BASE	0xffe00000
#define	BUS_WIDTH	2
#else
#define FLASH_BASE	0xf0000000
#define	BUS_WIDTH	2
#endif
#endif

#ifdef CONFIG_SUPERH
#define FLASH_BASE	0x00000000
#if defined(CONFIG_FLASH32MB)
# define	FLASH_ADDR	FLASH_BASE		/* Physical flash address */
# define	FLASH_SIZE	0x02000000		/* Maximum flash size */
# define	BUS_WIDTH	2				/* Flash bus width */

# define	BOOT_OFFSET	0x00000000
# define	BOOT_SIZE	0x00020000

# define	RECOVER_OFFSET	0x00020000
# define	RECOVER_SIZE	0x00820000

# define	KERNEL_OFFSET	(BOOT_SIZE + RECOVER_SIZE)
# define	KERNEL_SIZE	0x00180000
# define	CONFIG_SIZE	0x00020000
# define	NG_CONFIG_SIZE	0x00200000
# define	NG_VAR_SIZE	0x00200000

# define	ROOTFS_SIZE	(FLASH_SIZE - BOOT_SIZE - KERNEL_SIZE - CONFIG_SIZE - \
			 NG_CONFIG_SIZE - NG_VAR_SIZE - RECOVER_SIZE)

# define	VENDOR_ROOTFS	"SnapGear filesystem"
#else
# define	BUS_WIDTH	1
#endif
#endif

#ifdef CONFIG_MIPS32
#ifdef CONFIG_RTL865X
#define FLASH_BASE	0xbe000000
#define	BUS_WIDTH	2
#define	FORCE_RAM_MAPPING 1
#else
#define FLASH_BASE	0x1f000000
#define	BUS_WIDTH	4
#endif
#endif

#ifdef CONFIG_ARCH_IXP425
#define	FLASH_BASE	0x50000000
#define	BUS_WIDTH	1
#endif

#ifndef ROMFS_LOC
extern char _ebss;
#define ROMFS_LOC	(&_ebss)
#endif

#ifndef VENDOR_ROOTFS
#define	VENDOR_ROOTFS	"Image"
#endif

/****************************************************************************/

static __u8 nettel_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 nettel_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 nettel_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void nettel_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void nettel_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void nettel_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void nettel_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void nettel_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

/****************************************************************************/

static struct map_info nettel_flash_map = {
	name:		"Flash",
	read8:		nettel_read8,
	read16:		nettel_read16,
	read32:		nettel_read32,
	copy_from:	nettel_copy_from,
	write8:		nettel_write8,
	write16:	nettel_write16,
	write32:	nettel_write32,
	copy_to:	nettel_copy_to,
};

static struct map_info nettel_ram_map = {
	name:		"RAM",
	read8:		nettel_read8,
	read16:		nettel_read16,
	read32:		nettel_read32,
	copy_from:	nettel_copy_from,
	write8:		nettel_write8,
	write16:	nettel_write16,
	write32:	nettel_write32,
	copy_to:	nettel_copy_to,
};

static struct mtd_info *ram_mtdinfo;
static struct mtd_info *flash_mtdinfo;

/****************************************************************************/

static struct mtd_partition nettel_romfs[] = {
	{ name: "Romfs", offset: 0 }
};

/****************************************************************************/
/*
 *	The layout of our flash,  note the order of the names,  this
 *	means we use the same major/minor for the same purpose on all
 *	layouts (when possible)
 */

static struct mtd_partition nettel_128k[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00004000 },
	{ name: "MAC",        offset: 0x00008000, size:   0x00004000 },
	{ name: "Config",     offset: 0x00010000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x0000c000, size:   0x00004000 },
	{ name: "Flash",      offset: 0 }
};

#ifdef CONFIG_M5206eC3

static struct mtd_partition nettel_1mb[] = {
	{ name: "Image",      offset: 0x00000000, size:   0x000f8000 },
	{ name: "Config",     offset: 0x000f8000, size:   0x00008000 },
	{ name: "Flash",      offset: 0 }
};
#define nettel_2mb nettel_1mb
#define nettel_4mb nettel_1mb
#define nettel_8mb nettel_1mb
#define nettel_16mb nettel_1mb

#else /* CONFIG_M5206eC3 */

static struct mtd_partition nettel_1mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00002000 },
	{ name: "MAC",        offset: 0x00006000, size:   0x00002000 },
	{ name: "Config",     offset: 0x000f0000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x00008000, size:   0x00008000 },
	{ name: "Image",      offset: 0x00010000, size:   0x000e0000 },
	{ name: "Flash",      offset: 0 }
};

#ifndef CONFIG_UCDIMM

static struct mtd_partition nettel_2mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00002000 },
	{ name: "MAC",        offset: 0x00006000, size:   0x00002000 },
	{ name: "Config",     offset: 0x00010000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x00008000, size:   0x00008000 },
	{ name: "Image",      offset: 0x00020000, size:   0x001e0000 },
	{ name: "Flash",      offset: 0 }
};

#if defined(CONFIG_SH_SECUREEDGE5410) || defined(CONFIG_SG310)

static struct mtd_partition nettel_4mb[] = {
	{ name: "Boot data",  offset: 0x00000000, size:   0x00020000 },
	{ name: "Config",     offset: 0x00020000, size:   0x00040000 },
	{ name: "Image",      offset: 0x00060000, size:   0x00000000 },
	{ name: "Flash",      offset: 0 }
};

static struct mtd_partition nettel_8mb[] = {
	{ name: "Boot data",  offset: 0x00000000, size:   0x00020000 },
	{ name: "Config",     offset: 0x00020000, size:   0x00080000 },
	{ name: "Image",      offset: 0x000a0000, size:   0x00000000 },
	{ name: "Flash",      offset: 0 }
};

#else

static struct mtd_partition nettel_4mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00004000 },
	{ name: "Bootargs",   offset: 0x00004000, size:   0x00002000 },
	{ name: "MAC",        offset: 0x00006000, size:   0x00002000 },
	{ name: "Config",     offset: 0x00010000, size:   0x00010000 },
	{ name: "Spare",      offset: 0x00008000, size:   0x00008000 },
	{ name: "Image",      offset: 0x00020000, size:   0x001e0000 },
	{ name: "Flash",      offset: 0x00000000, size:   0x00200000 },
	{ name: "Image2",     offset: 0x00220000, size:   0x001e0000 },
	{ name: "Flash2",     offset: 0 }
};

static struct mtd_partition nettel_8mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00020000 },
	{ name: "Bootargs",   offset: 0x00020000, size:   0x00020000 },
	{ name: "MAC",        offset: 0x00040000, size:   0x00020000 },
	{ name: "Config",     offset: 0x00080000, size:   0x00080000 },
	{ name: "Spare",      offset: 0x00060000, size:   0x00020000 },
	{ name: "Image",      offset: 0x00100000, size:   0x00700000 },
	{ name: "Flash",      offset: 0 }
};

#endif

#else /* CONFIG_UCDIMM */

static struct mtd_partition nettel_2mb[] = {
	{ name: "Bootloader", offset: 0x00000000, size:   0x00010000,
	                                          mask_flags: MTD_WRITEABLE },
	{ name: "Image",      offset: 0x00010000, size:   0x00190000 },
	{ name: "Spare",      offset: 0x001a0000, size:   0x00060000 },
	{ name: "Flash",      offset: 0 }
};

#define nettel_4mb nettel_2mb
#define nettel_8mb nettel_2mb

#endif

#ifdef CONFIG_MIPS_VEGAS
static struct mtd_partition nettel_16mb[] = {
	{ name: "Bootloader",  offset: 0x00c00000, size:   0x00020000 },
	{ name: "Bootargs",    offset: 0x00c20000, size:   0x00020000 },
	{ name: "MAC",         offset: 0x00c40000, size:   0x00020000 },
	{ name: "Config",      offset: 0x00c60000, size:   0x003a0000 },
	{ name: "Image",       offset: 0x00000000, size:   0x00c00000 },
	{ name: "Flash",       offset: 0 }
};
#else
static struct mtd_partition nettel_16mb[] = {
	{ name: "Boot data",  offset: 0x00000000, size:   0x00020000 },
	{ name: "Config",     offset: 0x00020000, size:   0x00100000 },
	{ name: "Image",      offset: 0x00120000, size:   0x00000000 },
	{ name: "Flash",      offset: 0 }
};
#endif
#endif /* CONFIG_M5206eC3 */

#if defined(CONFIG_FLASH32MB)
/* currently only for NG7101 targets */
static struct mtd_partition nettel_32mb[] = {
	/*
	 * if you change the names of these,  check the table below
	 * for unlocking the flash as well
	 */
	{
		name: "SnapGear kernel",
		offset: KERNEL_OFFSET,
		size: KERNEL_SIZE,
	},
	{
		name: "SnapGear filesystem",
		offset: KERNEL_OFFSET + KERNEL_SIZE,
		size: ROOTFS_SIZE,
	},
	{
		name: "SnapGear config",
		offset: KERNEL_OFFSET + KERNEL_SIZE + ROOTFS_SIZE,
		size: CONFIG_SIZE
	},
	{
		name: "SnapGear Extra config",
		offset: KERNEL_OFFSET + KERNEL_SIZE + ROOTFS_SIZE + CONFIG_SIZE,
		size: NG_CONFIG_SIZE
	},
	{
		name: "SnapGear Extra var",
		offset: KERNEL_OFFSET + KERNEL_SIZE + ROOTFS_SIZE + CONFIG_SIZE + NG_CONFIG_SIZE,
		size: NG_VAR_SIZE
	},
	{
		name: "SnapGear image partition",
		offset: KERNEL_OFFSET,
		size: KERNEL_SIZE + ROOTFS_SIZE,
	},
	{
		name: "SnapGear BIOS config",
		offset: 0,
		size: BOOT_SIZE,
	},
	{
		name: "SnapGear BIOS",
		offset: 0,
		size: BOOT_SIZE,
	},
	{
		name: "SnapGear Recover",
		offset: RECOVER_OFFSET,
		size: RECOVER_SIZE,
	},
	{
		name: "SnapGear Intel/StrataFlash",
		offset: 0
	},
};
#endif

static struct mtd_partition *parts = NULL;
static int nparts = 0;

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

static int nettel_reboot_notifier(
	struct notifier_block *nb,
	unsigned long val,
	void *v)
{
	struct cfi_private *cfi = nettel_flash_map.fldrv_priv;
	int i;
	
	for (i = 0; cfi && i < cfi->numchips; i++)
		cfi_send_gen_cmd(0xff, 0x55, cfi->chips[i].start, &nettel_flash_map,
				cfi, cfi->device_type, NULL);

	return(NOTIFY_OK);
}

static struct notifier_block nettel_notifier_block = {
	nettel_reboot_notifier, NULL, 0
};

#endif

/****************************************************************************/

static int
nettel_point(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char **mtdbuf)
{
	struct map_info *map = (struct map_info *) mtd->priv;
	*mtdbuf = (u_char *) (map->map_priv_1 + (int)from);
	*retlen = len;
	return(0);
}

static void
nettel_unpoint(struct mtd_info *mtd, u_char *addr, loff_t from, size_t len)
{
}

/****************************************************************************/

static int __init
nettel_probe(int ram, unsigned long addr, int size, int buswidth)
{
	struct mtd_info *mymtd;
	struct map_info *map_ptr;

	if (ram)
		map_ptr = &nettel_ram_map;
	else
		map_ptr = &nettel_flash_map;

	map_ptr->buswidth = buswidth;
	map_ptr->map_priv_2 = addr;
	map_ptr->size = size;

	printk(KERN_NOTICE "SnapGear %s probe(0x%lx,%d,%d): %lx at %lx\n",
			ram ? "ram" : "flash",
			addr, size, buswidth, map_ptr->size, map_ptr->map_priv_2);

#ifdef FORCE_RAM_MAPPING
	map_ptr->map_priv_1 = addr;
#else
	if (ram)
		map_ptr->map_priv_1 = addr;
	else
		map_ptr->map_priv_1 = (unsigned long) ioremap_nocache(map_ptr->map_priv_2, map_ptr->size);
#endif

	if (!map_ptr->map_priv_1) {
		printk("Failed to ioremap_nocache\n");
		return -EIO;
	}

	if (!ram) {
		mymtd = do_map_probe("cfi_probe", map_ptr);
		if (!mymtd)
			mymtd = do_map_probe("jedec_probe", map_ptr);
		if (mymtd == NULL || mymtd->size < size)
			return -ENODEV;
	} else
		mymtd = do_map_probe("map_ram", map_ptr);

	if (!mymtd) {
		iounmap((void *)map_ptr->map_priv_1);
		return -ENXIO;
	}
		
	mymtd->module  = THIS_MODULE;
	mymtd->point   = nettel_point;
	mymtd->unpoint = nettel_unpoint;
	mymtd->priv    = map_ptr;

	if (ram) {
		ram_mtdinfo = mymtd;
		add_mtd_partitions(mymtd, nettel_romfs, NB_OF(nettel_romfs));
		return(0);
	}

	flash_mtdinfo = mymtd;
	switch (size) {
	case SIZE_128K:
		add_mtd_partitions(mymtd, parts=nettel_128k, nparts=NB_OF(nettel_128k));
		break;
	case SIZE_1MB:
		add_mtd_partitions(mymtd, parts=nettel_1mb,  nparts=NB_OF(nettel_1mb));
		break;
	case SIZE_2MB:
		add_mtd_partitions(mymtd, parts=nettel_2mb,  nparts=NB_OF(nettel_2mb));
		break;
	case SIZE_4MB:
		add_mtd_partitions(mymtd, parts=nettel_4mb,  nparts=NB_OF(nettel_4mb));
		break;
	case SIZE_8MB:
		add_mtd_partitions(mymtd, parts=nettel_8mb,  nparts=NB_OF(nettel_8mb));
		break;
	case SIZE_16MB:
		add_mtd_partitions(mymtd, parts=nettel_16mb, nparts=NB_OF(nettel_16mb));
		break;
#if defined(CONFIG_FLASH32MB)
	case SIZE_32MB:
		add_mtd_partitions(mymtd, parts=nettel_32mb, nparts=NB_OF(nettel_32mb));
		break;
#endif
	}

	return 0;
}

/****************************************************************************/

int __init nettel_mtd_init(void)
{
	int rc = -1;
	struct mtd_info *mtd;

	/*
	 * I hate this ifdef stuff,  but our HW doesn't always have
	 * the same chipsize as the map that we use
	 */
#if defined(CONFIG_FLASH32MB) /* no auto on this on, ATT 7101 only */
	if (rc != 0)
		rc = nettel_probe(0, FLASH_BASE, SIZE_32MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH16MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = nettel_probe(0, FLASH_BASE, SIZE_16MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH8MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = nettel_probe(0, FLASH_BASE, SIZE_8MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH4MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = nettel_probe(0, FLASH_BASE, SIZE_4MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH2MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = nettel_probe(0, FLASH_BASE, SIZE_2MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH1MB) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = nettel_probe(0, FLASH_BASE, SIZE_1MB, BUS_WIDTH);
#endif

#if defined(CONFIG_FLASH128K) || defined(CONFIG_FLASHAUTO)
	if (rc != 0)
		rc = nettel_probe(0, FLASH_BASE, SIZE_128K, BUS_WIDTH);
#endif

#if defined(CONFIG_COLDFIRE) || defined(CONFIG_UCDIMM)
	/*
	 * Map in the filesystem from RAM last so that,  if the filesystem
	 * is not in RAM for some reason we do not change the minor/major
	 * for the flash devices
	 */
#ifndef CONFIG_ROMFS_FROM_ROM
	if (0 != nettel_probe(1, (unsigned long) ROMFS_LOC,
			PAGE_ALIGN(* (unsigned long *)(ROMFS_LOC + 8)), 4))
		printk("Failed to probe RAM filesystem\n");
#else
	{
		unsigned long start_area;
		unsigned char *sp, *ep;

		size_t len;

		start_area = (unsigned long) ROMFS_LOC;

		if (strncmp((char *) start_area, "-rom1fs-", 8) != 0) {
			mtd = get_mtd_named(VENDOR_ROOTFS);
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
		if (0 != nettel_probe(1, start_area,
				PAGE_ALIGN(* (unsigned long *)(start_area + 8)), 4))
			printk("Failed to probe RAM filesystem\n");
	}
#endif
	
	mtd = get_mtd_named("Romfs");
	if (mtd) {
		ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
		put_mtd_device(mtd);
	}
#endif

#if defined(CONFIG_FLASH32MB)
	/*
	 * By default the P30 flash is locked at boot,  we need to unlock
	 * some of the partitions for them to operate,  which is a little
	 * cumbersome,  though simple in reality.
	 */

	if (rc == 0 && flash_mtdinfo->unlock) {
		int n, p, i, j, printed;
		char *writable_parts[] = {
			/* "SnapGear kernel",      */
			/* "SnapGear filesystem",  */
			"SnapGear config",
			"SnapGear Extra config",
			"SnapGear Extra var",
			"SnapGear image partition",
			/* "SnapGear BIOS config", */
			/* "SnapGear BIOS",        */
			/* "SnapGear Recover",     */
			NULL
		};

		for (n = 0; writable_parts[n]; n++) {
			for (p = 0; p < nparts; p++)
				if (strcmp(parts[p].name, writable_parts[n]) == 0)
					break;
			if (p >= nparts) {
				printk("failed to find %s partition\n",
						writable_parts[n]);
				continue;
			}

			printed = 0;
			for (i = 0; i < flash_mtdinfo->numeraseregions; i++) {
				for (j = 0; j < flash_mtdinfo->eraseregions[i].numblocks; j++) {
					/*
					 * check if this sector is within the current region we
					 * are unlocking
					 */
					if (parts[p].offset > 
							flash_mtdinfo->eraseregions[i].offset +
							j * flash_mtdinfo->eraseregions[i].erasesize
							||
							parts[p].offset + parts[p].size <= 
							flash_mtdinfo->eraseregions[i].offset +
							j * flash_mtdinfo->eraseregions[i].erasesize
							)
						continue;

					if (printed == 0) {
						printed = 1;
						printk("unlocked %s\n", writable_parts[n]);
					}

					flash_mtdinfo->unlock(flash_mtdinfo,
							flash_mtdinfo->eraseregions[i].offset +
								j * flash_mtdinfo->eraseregions[i].erasesize,
								flash_mtdinfo->eraseregions[i].erasesize);
				}
			}
		}
	} else if (rc == 0)
		printk("no unlock routine\n");
#endif

#if defined(CONFIG_SH_SECUREEDGE5410) || defined(CONFIG_MIPS_VEGAS) || defined(CONFIG_MACH_ESS710) || defined(CONFIG_RTL865X)
{
	extern int _end;
	unsigned long magic;
	unsigned char *cp = (char *) &_end;

	memcpy(&magic, cp, sizeof(magic));
	if (memcmp(cp, "-rom1fs-", 8) == 0) {
#ifndef CONFIG_BLK_DEV_INITRD
		/* romfs */ ;
		nettel_probe(1, (unsigned long) cp,
				PAGE_ALIGN(* (unsigned long *)(cp + 8)), 4);
		mtd = get_mtd_named("Romfs");
		if (mtd) {
			ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
			put_mtd_device(mtd);
		} else
			printk("%s: Failed to find & make romfs root filesystem\n",
					__FUNCTION__);
#endif
	} else if (le32_to_cpu(magic) == 0x28cd3d45) {
#ifndef CONFIG_BLK_DEV_INITRD
		/* cramfs */ ;
		nettel_probe(1, (unsigned long) cp,
				PAGE_ALIGN(le32_to_cpu(* (unsigned long *)(cp + 4))), 4);
		mtd = get_mtd_named("Romfs");
		if (mtd) {
			ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
			put_mtd_device(mtd);
		} else
			printk("%s: Failed to find & make cramfs root filesystem\n",
					__FUNCTION__);
#endif
	} else {
		mtd = get_mtd_named(VENDOR_ROOTFS);
		if (mtd) {
			ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
			put_mtd_device(mtd);
		}
#if defined(CONFIG_NFTL) || defined(CONFIG_INFTL)
		else
			ROOT_DEV = MKDEV(NFTL_MAJOR, 1);
#endif
	}
}
#endif

#ifdef CONFIG_MTD_CFI_INTELEXT
	register_reboot_notifier(&nettel_notifier_block);
#endif

	return(rc);
}

/****************************************************************************/

static void __exit nettel_mtd_cleanup(void)
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
	if (nettel_ram_map.map_priv_1) {
		iounmap((void *)nettel_ram_map.map_priv_1);
		nettel_ram_map.map_priv_1 = 0;
	}
	if (nettel_flash_map.map_priv_1) {
		iounmap((void *)nettel_flash_map.map_priv_1);
		nettel_flash_map.map_priv_1 = 0;
	}
}

/****************************************************************************/

module_init(nettel_mtd_init);
module_exit(nettel_mtd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David McCullough <davidm@snapgear.com>");
MODULE_DESCRIPTION("SnapGear/SecureEdge FLASH support for uClinux");

/****************************************************************************/
