/***********************************************************************
 * drivers/mtd/maps/dm270-flash.c
 *
 *   Flash memory access on TI TMS320DM270 based devices
 *
 *   Derived from drivers/mtd/maps/sa1100-flash.c
 *
 *   Copyright (C) 2004 InnoMedia Pte Ltd. All rights reserved.
 *   cheetim_loh@innomedia.com.sg  <www.innomedia.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***********************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/io.h>

#ifndef CONFIG_ARCH_DM270
#error This is for DM270 architecture only
#endif

#define BUSWIDTH 	2

static __u8 dm270_read8(struct map_info *map, unsigned long ofs)
{
	return(*((__u8 *) (map->map_priv_1 + ofs)));
}

static __u16 dm270_read16(struct map_info *map, unsigned long ofs)
{
	return(*((__u16 *) (map->map_priv_1 + ofs)));
}

static __u32 dm270_read32(struct map_info *map, unsigned long ofs)
{
	return(*((__u32 *) (map->map_priv_1 + ofs)));
}

static void dm270_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void dm270_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*((__u8 *) (map->map_priv_1 + adr)) = d;
}

static void dm270_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*((__u16 *) (map->map_priv_1 + adr)) = d;
}

static void dm270_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*((__u32 *) (map->map_priv_1 + adr)) = d;
}

static void dm270_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info dm270_map = {
	name:		"DM270 flash",
	read8:		dm270_read8,
	read16:		dm270_read16,
	read32:		dm270_read32,
	copy_from:	dm270_copy_from,
	write8:		dm270_write8,
	write16:	dm270_write16,
	write32:	dm270_write32,
	copy_to:	dm270_copy_to,

	map_priv_1:	FLASH_MEM_BASE,
	map_priv_2:	-1,
};


/*
 * Here are partition information for all known DM270-based devices.
 * See include/linux/mtd/partitions.h for definition of the mtd_partition
 * structure.
 *
 * The *_max_flash_size is the maximum possible mapped flash size which
 * is not necessarily the actual flash size.  It must be no more than
 * the value specified in the "struct map_desc *_io_desc" mapping
 * definition for the corresponding machine.
 *
 * Please keep these in alphabetical order, and formatted as per existing
 * entries.  Thanks.
 */

#ifdef CONFIG_ARCH_DM270
/*
 * 1 x Toshiba TC58FVB160AFT-70 16-MBIT (2M x 8 bits / 1M x 16 bits) CMOS FLASH MEMORY
 *   Block erase architecture:
 *   1 X 16 Kbytes / 2 x 8 Kbytes / 1 x 32 Kbytes / 31 x 64 Kbytes
 */
static struct mtd_partition dm270_partitions[] = {
	{
		name:		"bootloader",
		size:		0x20000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"kernel",
		size:		0xc0000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"rootfs",
		size:		0x110000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	0,              /* read-write */
	}, {
		name:		"bootloader params",
		size:		0x10000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}
};
#endif

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts, unsigned long fis_origin);
extern int parse_cmdline_partitions(struct mtd_info *master, struct mtd_partition **pparts, char *);

static struct mtd_partition *parsed_parts;
static struct mtd_info *mymtd;

int __init dm270_mtd_init(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0, ret;
	int parsed_nr_parts = 0;
	const char *part_type;
	unsigned long base = -1UL;

	/* Default flash buswidth */
	dm270_map.buswidth = BUSWIDTH;

	/*
	 * Static partition definition selection
	 */
	part_type = "static";

#ifdef CONFIG_ARCH_DM270
	if (machine_is_dm270()) {
		parts = dm270_partitions;
		nb_parts = ARRAY_SIZE(dm270_partitions);
		dm270_map.size = FLASH_SIZE;
		dm270_map.buswidth = BUSWIDTH;
		base = FLASH_MEM_BASE;
	}
#endif

	/*
	 * For simple flash devices, use ioremap to map the flash.
	 */
	if (base != (unsigned long)-1) {
		if (!request_mem_region(base, dm270_map.size, "flash")) {
			printk(KERN_ERR "dm270 flash: Could not request mem region %lx at %lx.\n",
					dm270_map.size, base);
			return -ENOMEM;
		}
		dm270_map.map_priv_2 = base;
		dm270_map.map_priv_1 = (unsigned long)
				ioremap(base, dm270_map.size);
		if (!dm270_map.map_priv_1) {
			printk(KERN_ERR "dm270 Flash: Failed to map IO region %lx at %lx.\n",
					dm270_map.size, base);
			ret = -EIO;
			goto out_err;
		}
	}

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	printk(KERN_NOTICE "DM270 flash: probing %d-bit flash bus\n", dm270_map.buswidth*8);
	mymtd = do_map_probe("cfi_probe", &dm270_map);
	ret = -ENXIO;
	if (!mymtd)
		goto out_err;
	mymtd->module = THIS_MODULE;

	/*
	 * Dynamic partition selection stuff (might override the static ones)
	 */
#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(mymtd, &parsed_parts, FLASH_MEM_BASE);

		if (ret > 0) {
			part_type = "RedBoot";
			parsed_nr_parts = ret;
		}
	}
#endif
#ifdef CONFIG_MTD_CMDLINE_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_cmdline_partitions(mymtd, &parsed_parts, "dm270");
		if (ret > 0) {
			part_type = "Command Line";
			parsed_nr_parts = ret;
		}
	}
#endif

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	}

	if (nb_parts == 0) {
		printk(KERN_NOTICE "DM270 flash: no partition info available, registering whole flash at once\n");
		add_mtd_device(mymtd);
	} else {
		printk(KERN_NOTICE "Using %s partition definition\n", part_type);
		add_mtd_partitions(mymtd, parts, nb_parts);
	}
	return 0;

 out_err:
	if (dm270_map.map_priv_2 != -1) {
		iounmap((void *)dm270_map.map_priv_1);
		release_mem_region(dm270_map.map_priv_2, dm270_map.size);
	}
	return ret;
}

static void __exit dm270_mtd_cleanup(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (dm270_map.map_priv_2 != -1) {
		iounmap((void *)dm270_map.map_priv_1);
		release_mem_region(dm270_map.map_priv_2, dm270_map.size);
	}
}

module_init(dm270_mtd_init);
module_exit(dm270_mtd_cleanup);

MODULE_AUTHOR("Chee Tim Loh");
MODULE_DESCRIPTION("DM270 CFI map driver");
MODULE_LICENSE("GPL");
