/*
 * drivers/mtd/maps/suzaku.c
 *
 * Copyright (C) 2003, 2004 Atmark Techno, Inc.
 *
 * Yasushi SHOJI <yashi@atmark-techno.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/ioport.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

MODULE_AUTHOR("Atmark Techno, Inc.");
MODULE_DESCRIPTION("Suzaku Flash map driver");
MODULE_LICENSE("GPL v2");

#define FLASH_START 0xff000000
#define FLASH_SIZE  0x00400000
#define FLASH_WIDTH 2

__u16 suzaku_read16(struct map_info *map, unsigned long ofs)
{
        return readw(map->map_priv_1 + ofs);
}

void suzaku_write16(struct map_info *map, __u16 d, unsigned long adr)
{
        writew(d, map->map_priv_1 + adr);
}

void suzaku_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
        memcpy_fromio(to, map->map_priv_1 + from, len);
}

void suzaku_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
        memcpy_toio(map->map_priv_1 + to, from, len);
}

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *flash_mtd;

struct map_info suzaku_flash_map = {
        .name      = "flash",
        .size      = FLASH_SIZE,
        .buswidth  = FLASH_WIDTH,

        .read16    = suzaku_read16,
        .copy_from = suzaku_copy_from,

        .write16   = suzaku_write16,
        .copy_to   = suzaku_copy_to,
};

/*
 *      0x0 +-------------+
 *          |    FPGA     |
 *  0x80000 +-------------+
 *          |    Boot     |
 *  0xa0000 +------+------+
 *          |      | Kern |
 * 0x210000 | IMG  +------+
 *          |      | User |
 * 0x3f0000 +------+------+
 *          |   Config    |
 *          +-------------+
 */
/*
 * Do _NOT_ change the order of first four entries. These entries are
 * fixed and assinged to fixed minor numbers.
 */
#define CONFIG_SUZAKU_FLASH_TOP
#if defined(CONFIG_SUZAKU_FLASH_TOP)
struct mtd_partition suzaku_partitions[] = {
        { .name = "Flash/All",        .size = 0x00400000, .offset = 0, },
        { .name = "Flash/FPGA",       .size = 0x00080000, .offset = 0, },
        { .name = "Flash/Bootloader", .size = 0x00020000, .offset = 0x00080000, },
        { .name = "Flash/Config",     .size = 0x00010000, .offset = 0x003F0000, },
        { .name = "Flash/Image",      .size = 0x00350000, .offset = 0x000A0000, },
        { .name = "Flash/Kernel",     .size = 0x00170000, .offset = 0x000A0000, },
        { .name = "Flash/User",       .size = 0x001E0000, .offset = 0x00210000, },
};
#else
struct mtd_partition suzaku_partitions[] = {
        { .name = "Flash/Bootloader", .size = 0x00008000, .offset = 0,},
        { .name = "Flash/Reserved",   .size = 0x00008000, .offset = MTDPART_OFS_APPEND, },
        { .name = "Flash/User",       .size = 0x00370000, .offset = MTDPART_OFS_APPEND, },
        { .name = "Flash/FPGA",       .size = 0x00080000, .offset = MTDPART_OFS_APPEND, },
};
#endif

struct resource suzaku_flash_resource = {
        .name  = "flash",
        .start = FLASH_START,
        .end   = FLASH_START + FLASH_SIZE - 1,
        .flags = IORESOURCE_IO | IORESOURCE_BUSY,
};

static int __init init_suzaku_flash (void)
{
        struct mtd_partition *parts;
        int nb_parts = 0;
        int err;
        
        parts = suzaku_partitions;
        nb_parts = NB_OF(suzaku_partitions);
        
        if (request_resource (&ioport_resource, &suzaku_flash_resource)) {
                printk(KERN_NOTICE "Failed to reserve Suzaku Flash space\n");
                err = -EBUSY;
                goto out;
        }
        
        suzaku_flash_map.map_priv_1 = (unsigned long)ioremap(FLASH_START, FLASH_SIZE);
        if (!suzaku_flash_map.map_priv_1) {
                printk(KERN_NOTICE "Failed to ioremap Suzaku Flash space\n");
                err = -EIO;
                goto out_resource;
        }

        flash_mtd = do_map_probe("cfi_probe", &suzaku_flash_map);
        if (!flash_mtd) {
                flash_mtd = do_map_probe("map_rom", &suzaku_flash_map);
        }
        if (!flash_mtd) {
                printk("FLASH probe failed\n");
                err = -ENXIO;
                goto out_ioremap;
        }

        flash_mtd->module = THIS_MODULE;
        
        if (add_mtd_partitions(flash_mtd, parts, nb_parts)) {
                printk("FLASH partitions addition failed\n");
                err = -ENOMEM;
                goto out_probe;
        }
                
        return 0;

out_probe:
        map_destroy(flash_mtd);
        flash_mtd = 0;
out_ioremap:
        iounmap((void *)suzaku_flash_map.map_priv_1);
out_resource:
        release_resource (&suzaku_flash_resource);
out:
        return err;
}

static int __init init_suzaku_maps(void)
{
        printk(KERN_INFO "Suzaku MTD mappings:\n  Flash 0x%x at 0x%x\n", 
               FLASH_SIZE, FLASH_START);

        init_suzaku_flash();
        
        return 0;
}

static void __exit cleanup_suzaku_maps(void)
{
        if (flash_mtd) {
                del_mtd_partitions(flash_mtd);
                map_destroy(flash_mtd);
                iounmap((void *)suzaku_flash_map.map_priv_1);
                release_resource (&suzaku_flash_resource);
        }
}

module_init(init_suzaku_maps);
module_exit(cleanup_suzaku_maps);
