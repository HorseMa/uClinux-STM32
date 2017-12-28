/*
 * $Id: dbox2-flash.c,v 1.4 2001/10/02 15:05:14 dwmw2 Exp $
 *
 * Nokia / Sagem D-Box 2 flash driver
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>

#define WINDOW_ADDR 0x00000000
#define WINDOW_SIZE 0x00800000

/* partition_info gives details on the logical partitions that the split the
 * single flash device into. If the size if zero we use up to the end of the
 * device. */
static struct mtd_partition partition_info[]= {
							{name: "uboot bootloader",		// raw
						     size: 128 * 1024, 
						     offset: 0,                  
						     mask_flags: MTD_WRITEABLE},
                            {name: "System (JFFS2)",		// jffs
						     size: MTDPART_SIZ_FULL, 
						     offset: MTDPART_OFS_APPEND, 
						     mask_flags: 0}
};

#define NUM_PARTITIONS (sizeof(partition_info) / sizeof(partition_info[0]))


static struct mtd_info *mymtd;

__u8 vc547x_flash_read8(struct map_info *map, unsigned long ofs)
{
	return(*((__u8 *) (map->map_priv_1 + ofs)));
}

__u16 vc547x_flash_read16(struct map_info *map, unsigned long ofs)
{
	return(*((__u16 *) (map->map_priv_1 + ofs)));
}

__u32 vc547x_flash_read32(struct map_info *map, unsigned long ofs)
{
	return(*((__u32 *) (map->map_priv_1 + ofs)));
}

void vc547x_flash_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
//printk("flash_copy_from(%x, %x, %x\n", to, (void *)(map->map_priv_1 + from), len);
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

void vc547x_flash_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*((__u8 *) (map->map_priv_1 + adr)) = d;
}

void vc547x_flash_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*((__u16 *) (map->map_priv_1 + adr)) = d;
}

void vc547x_flash_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*((__u32 *) (map->map_priv_1 + adr)) = d;
}

void vc547x_flash_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
//printk("flash_copy_to(%x, %x, %x\n", to, (void *)(map->map_priv_1 + to), from, len);
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

struct map_info vc547x_flash_map = {
	name: "audio_oip flash memory",
	size: WINDOW_SIZE,
	buswidth: 4,
	read8: vc547x_flash_read8,
	read16: vc547x_flash_read16,
	read32: vc547x_flash_read32,
	copy_from: vc547x_flash_copy_from,
	write8: vc547x_flash_write8,
	write16: vc547x_flash_write16,
	write32: vc547x_flash_write32,
	copy_to: vc547x_flash_copy_to
};

int __init init_vc547x_flash(void)
{
   	printk(KERN_NOTICE "vc547x flash driver (size->0x%X mem->0x%X)\n", WINDOW_SIZE, WINDOW_ADDR);
	vc547x_flash_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	// since our flash is at 0 no problem - but don't check
	/*
	if (!vc547x_flash_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}*/

	// Probe for dual Intel 28F320 or dual AMD
	/* can be { "cfi_probe", "jedec_probe", "map_rom", 0 }; */
	mymtd = do_map_probe("jedec_probe", &vc547x_flash_map);
	if (!mymtd) {
	    // Probe for single Intel 28F640
	    vc547x_flash_map.buswidth = 2;
	
	    mymtd = do_map_probe("cfi_probe", &vc547x_flash_map);
	}
	    
	if (mymtd) {
		mymtd->module = THIS_MODULE;

                /* Create MTD devices for each partition. */
	        add_mtd_partitions(mymtd, partition_info, NUM_PARTITIONS);
		
		return 0;
	}

	iounmap((void *)vc547x_flash_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_vc547x_flash(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (vc547x_flash_map.map_priv_1) {
		iounmap((void *)vc547x_flash_map.map_priv_1);
		vc547x_flash_map.map_priv_1 = 0;
	}
}

module_init(init_vc547x_flash);
module_exit(cleanup_vc547x_flash);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kári Davíðsson <kd@flaga.is>");
MODULE_DESCRIPTION("MTD map driver for Nokia/Sagem D-Box 2 board, used for vc547x");
