/*
 *
 * Normal mappings of chips in physical memory
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>


#define WINDOW_ADDR 0xffe00000
#define WINDOW_SIZE 0x200000
#define BUSWIDTH 2

extern char* ppcboot_getenv(char* v);

static struct mtd_info *mymtd;

__u8 cpu16b_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 cpu16b_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 cpu16b_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void cpu16b_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void cpu16b_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void cpu16b_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void cpu16b_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void cpu16b_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

struct map_info cpu16b_map = {
	name: "SNEHA CPU 16B flash device",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8: cpu16b_read8,
	read16: cpu16b_read16,
	read32: cpu16b_read32,
	copy_from: cpu16b_copy_from,
	write8: cpu16b_write8,
	write16: cpu16b_write16,
	write32: cpu16b_write32,
	copy_to: cpu16b_copy_to
};

/*
 * MTD 'PARTITIONING' STUFF 
 */


static struct mtd_partition cpu16b_partitions[] = {
        {
                name: "boot (128K)",
                size: 0x20000,
                offset: 0x00000
        },
        {
                name: "kernel (896K)",
                size: 0xE0000,
                offset: 0x20000
        },
	{
                name: "jffs2 (1024K)",
                size: 0x100000,
                offset: 0x100000
        }
};


#if LINUX_VERSION_CODE < 0x20212 && defined(MODULE)
#define init_cpu16b init_module
#define cleanup_cpu16b cleanup_module
#endif

int __init init_cpu16b(void)
{
       	printk(KERN_NOTICE "SNEHA CPU 16B flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);
	cpu16b_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!cpu16b_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &cpu16b_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;

#ifdef CONFIG_PPCBOOT
                read_cpu16b_partitiontable();
		if(cpu16b_partitions_cnt) {
		  return add_mtd_partitions(mymtd, cpu16b_partitions, cpu16b_partitions_cnt);
		}
		else {
		  return  -ENXIO;
		};
#else
		return add_mtd_partitions(mymtd, cpu16b_partitions,
					  sizeof(cpu16b_partitions) /
					  sizeof(struct mtd_partition));
#endif
	}

	iounmap((void *)cpu16b_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_cpu16b(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (cpu16b_map.map_priv_1) {
		iounmap((void *)cpu16b_map.map_priv_1);
		cpu16b_map.map_priv_1 = 0;
	}
}

module_init(init_cpu16b);
module_exit(cleanup_cpu16b);

