 
/*
 * Normal mappings of chips in physical memory
 * based on Avnet5282
 * adapted by Daniel Alomar (daniel.alomar@gmail.com)
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>


#define WINDOW_ADDR 0xff800000   // Flash Start addr.
#define WINDOW_SIZE 0x00800000   // Flash Size: 8 MB
#define BUSWIDTH 2

extern char* ppcboot_getenv(char* v);

static struct mtd_info *mymtd;

__u8 mfc3000_read8(struct map_info *map, unsigned long ofs)
{
        return __raw_readb(map->map_priv_1 + ofs);
}

__u16 mfc3000_read16(struct map_info *map, unsigned long ofs)
{
        return __raw_readw(map->map_priv_1 + ofs);
}

__u32 mfc3000_read32(struct map_info *map, unsigned long ofs)
{
        return __raw_readl(map->map_priv_1 + ofs);
}

void mfc3000_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
        memcpy_fromio(to, map->map_priv_1 + from, len);
}

void mfc3000_write8(struct map_info *map, __u8 d, unsigned long adr)
{
        __raw_writeb(d, map->map_priv_1 + adr);
        mb();
}

void mfc3000_write16(struct map_info *map, __u16 d, unsigned long adr)
{
        __raw_writew(d, map->map_priv_1 + adr);
        mb();
}

void mfc3000_write32(struct map_info *map, __u32 d, unsigned long adr)
{
        __raw_writel(d, map->map_priv_1 + adr);
        mb();
}

void mfc3000_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
        memcpy_toio(map->map_priv_1 + to, from, len);
}

struct map_info mfc3000_map = {
        name: "mfc3000 flash device",
        size: WINDOW_SIZE,
        buswidth: BUSWIDTH,
        read8: mfc3000_read8,
        read16: mfc3000_read16,
        read32: mfc3000_read32,
        copy_from: mfc3000_copy_from,
        write8: mfc3000_write8,
        write16: mfc3000_write16,
        write32: mfc3000_write32,
        copy_to: mfc3000_copy_to
};

/*
 * MTD 'PARTITIONING' STUFF
 */


static struct mtd_partition mfc3000_partitions[] = {

        {
                name: "uboot (256 KB)",
                size: 0x40000,
                offset: 0x0
        },
        {
                name: "kernel (2 MB)",
                size: 0x200000,
                offset: 0x40000
        },
        {
                name: "rootfs (5,75 MB)",
                size: 0x5C0000,
                offset: 0x240000
        }
};


#if LINUX_VERSION_CODE < 0x20212 && defined(MODULE)
#define init_mfc3000 init_module
#define cleanup_mfc3000 cleanup_module
#endif

int __init init_mfc3000(void)
{
        printk(KERN_NOTICE "mfc3000 flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);
        mfc3000_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

        if (!mfc3000_map.map_priv_1) {
                printk("Failed to ioremap\n");
                return -EIO;
        }
        mymtd = do_map_probe("cfi_probe", &mfc3000_map);
        if (mymtd) {
                mymtd->module = THIS_MODULE;

                mymtd->erasesize = 0x40000;

                return add_mtd_partitions(mymtd, mfc3000_partitions,
                                          sizeof(mfc3000_partitions) /
                                          sizeof(struct mtd_partition));
        }

        iounmap((void *)mfc3000_map.map_priv_1);
        return -ENXIO;
}

static void __exit cleanup_mfc3000(void)
{
        if (mymtd) {
                del_mtd_partitions(mymtd);
                map_destroy(mymtd);
        }
        if (mfc3000_map.map_priv_1) {
                iounmap((void *)mfc3000_map.map_priv_1);
                mfc3000_map.map_priv_1 = 0;
        }
}

module_init(init_mfc3000);
