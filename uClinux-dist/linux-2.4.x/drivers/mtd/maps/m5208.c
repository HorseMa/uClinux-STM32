/*
 *
 * Normal mappings of chips in physical memory.
 *
 * Copyright (C) 2005,  Intec Automation Inc. (mike@steroidmicros.com)
 *
 */

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

#define WINDOW_ADDR 0x00000000
#define WINDOW_SIZE 0x00200000
#define BUSWIDTH 2


/****************************************************************************/

static struct mtd_info *mymtd;

__u8 m5208_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 m5208_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 m5208_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void m5208_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void m5208_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void m5208_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void m5208_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void m5208_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}





/****************************************************************************/

static struct map_info m5208_flash_map = {
	name: 		"Am29BDD160G 2.5v flash device (2MB)",
	size: 		WINDOW_SIZE,
	buswidth: 	BUSWIDTH,
	read8: 		m5208_read8,
	read16: 	m5208_read16,
	read32: 	m5208_read32,
	copy_from: 	m5208_copy_from,
	write8: 	m5208_write8,
	write16: 	m5208_write16,
	write32: 	m5208_write32,
	copy_to: 	m5208_copy_to
};

static struct map_info m5208_ram_map = {
	name:		"RAM",
	read8:		m5208_read8,
	read16:		m5208_read16,
	read32:		m5208_read32,
	copy_from:	m5208_copy_from,
	write8:		m5208_write8,
	write16:	m5208_write16,
	write32:	m5208_write32,
	copy_to:	m5208_copy_to,
};

static struct mtd_info *ram_mtdinfo;


/*
 * MTD 'PARTITIONING' STUFF 
 */
 
/****************************************************************************/

static struct mtd_partition m5208_romfs[] = {
	{ name: "Romfs", offset: 0 }
};
 
/****************************************************************************/
 
static struct mtd_partition m5208_partitions[] = {
        {
                name: "dBUG (256K)",
                size: 0x40000,
                offset: 0x00000
        },
        {
                name: "User FS (640K)",
                size: 0xA0000,
                offset: 0x40000
        },
		{
                name: "Kernel + RootFS (1152K)",
                size: 0x120000,
                offset: 0xE0000
        }
};




/****************************************************************************/

static int
m5208_point(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char **mtdbuf)
{
	struct map_info *map = (struct map_info *) mtd->priv;
	*mtdbuf = (u_char *) (map->map_priv_1 + (int)from);
	*retlen = len;
	return(0);
}




/****************************************************************************/

static void
m5208_unpoint(struct mtd_info *mtd, u_char *addr, loff_t from, size_t len)
{
}





/****************************************************************************/

static int __init
m5208_ram_probe(unsigned long addr, int size, int buswidth)
{
	struct mtd_info *mymtd;
	struct map_info *map_ptr;

	map_ptr = &m5208_ram_map;

	map_ptr->buswidth = buswidth;
	map_ptr->map_priv_2 = addr;
	map_ptr->size = size;

	printk(KERN_NOTICE "m5208evb %s probe(0x%lx,%d,%d): %lx at %lx\n",
			"ram",
			addr, size, buswidth, map_ptr->size, map_ptr->map_priv_2);

	map_ptr->map_priv_1 = addr;

	if (!map_ptr->map_priv_1) {
		printk("Failed to ioremap_nocache\n");
		return -EIO;
	}

	mymtd = do_map_probe("map_ram", map_ptr);

	if (!mymtd) {
		iounmap((void *)map_ptr->map_priv_1);
		return -ENXIO;
	}
		
	mymtd->module  = THIS_MODULE;
	mymtd->point   = m5208_point;
	mymtd->unpoint = m5208_unpoint;
	mymtd->priv    = map_ptr;

	ram_mtdinfo = mymtd;
	add_mtd_partitions(mymtd, m5208_romfs, NB_OF(m5208_romfs));
	return(0);
}





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
/*
 * Initialize the mtd devices
 */

int __init init_m5208(void)
{
	int rc = -1;
	struct mtd_info *mtd;
	extern char _ebss;

       	printk(KERN_NOTICE "m5208 flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);

	mymtd = do_map_probe("cfi_probe", &m5208_flash_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;

		rc =   add_mtd_partitions(mymtd, m5208_partitions,
					  sizeof(m5208_partitions) /
					  sizeof(struct mtd_partition));
	}
	
	/*
	 * Map in the filesystem from RAM last so that,  if the filesystem
	 * is not in RAM for some reason we do not change the minor/major
	 * for the flash devices
	 */
	if (rc != 0) {
		unsigned long start_area;
		unsigned char *sp, *ep;

		size_t len;

		start_area = (unsigned long)&_ebss;

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
		if (0 != m5208_ram_probe( start_area,
				PAGE_ALIGN(* (unsigned long *)(start_area + 8)), 4))
			printk("Failed to probe RAM filesystem\n");
	}
	
	mtd = get_mtd_named("Romfs");
	if (mtd) {
		ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, mtd->index);
		put_mtd_device(mtd);
	}

	return (rc);
}




/****************************************************************************/

static void __exit cleanup_m5208(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
}

module_init(init_m5208);
module_exit(cleanup_m5208);

