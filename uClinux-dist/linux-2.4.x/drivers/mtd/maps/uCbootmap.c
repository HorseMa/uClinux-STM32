/*
 * uCbootmap.c:
 *
 * Retrieve Arcturus Networks uCbootstrap flash device data
 * to create mtd devices
 *
 * (c) 2004 Michael Leslie [mleslie arcturusnetworks com]
 * Based on solutionengine.c by jsiegel
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>

#include <asm/uCbootstrap.h>

static int errno;
extern char *getbenv(char *varname);
static _bsc1 (int, setpmask, int, mask);

/* pmask values: */
#define FEF_BOOT_READ   0x0002	/* Read permission for bootloader */
#define FEF_BOOT_WRITE  0x0004	/* Write permission for bootloader */
#define FEF_SUPER_READ  0x0020	/* Read permission for supervisor */
#define FEF_SUPER_WRITE 0x0040	/* Write permission for supervisor */
#define FEF_USER_READ   0x0200	/* Read permission for user */
#define FEF_USER_WRITE  0x0400	/* Write permission for user */


#define MAX_FLASH_DEVICES 2 /* for uCbootstrap platforms to date */


extern int parse_ucbootstrap_partitions(struct mtd_info *master,
										struct map_info *map,
										struct mtd_partition **pparts);

static struct mtd_info *flash_mtd[MAX_FLASH_DEVICES];
static struct mtd_partition *parsed_parts;



/****************************************************************************/

static __u8 uC_read8(struct map_info *map, unsigned long ofs) {
	return *(__u8 *)(map->map_priv_1 + ofs); }

static __u16 uC_read16(struct map_info *map, unsigned long ofs) {
	return *(__u16 *)(map->map_priv_1 + ofs); }

static __u32 uC_read32(struct map_info *map, unsigned long ofs) {
	return *(__u32 *)(map->map_priv_1 + ofs); }

static void uC_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len) {
	memcpy(to, (void *)(map->map_priv_1 + from), len); }

static void uC_write8(struct map_info *map, __u8 d, unsigned long adr) {
	*(__u8 *)(map->map_priv_1 + adr) = d; }

static void uC_write16(struct map_info *map, __u16 d, unsigned long adr) {
	*(__u16 *)(map->map_priv_1 + adr) = d; }

static void uC_write32(struct map_info *map, __u32 d, unsigned long adr) {
	*(__u32 *)(map->map_priv_1 + adr) = d; }

static void uC_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len) {
	memcpy((void *)(map->map_priv_1 + to), from, len); }

/****************************************************************************/




struct map_info uCflash_map[MAX_FLASH_DEVICES] =
	{
		{
			name:       "Flash 0",
			read8:		uC_read8,
			read16:		uC_read16,
			read32:		uC_read32,
			copy_from:	uC_copy_from,
			write8:		uC_write8,
			write16:	uC_write16,
			write32:	uC_write32,
			copy_to:	uC_copy_to

		},
		{
			name:       "Flash 1",
			read8:		uC_read8,
			read16:		uC_read16,
			read32:		uC_read32,
			copy_from:	uC_copy_from,
			write8:		uC_write8,
			write16:	uC_write16,
			write32:	uC_write32,
			copy_to:	uC_copy_to
		}
	};

char *env_name[MAX_FLASH_DEVICES] = { "FLASH0", "FLASH1" };
#define ENVARNAME_LENGTH 32 /* local maximum */


static int __init init_uCbootstrap_maps(void)
{
	int n_partitions = 0;
	char varname[ENVARNAME_LENGTH];
	char valstr[ENVARNAME_LENGTH];
	char *val;
	unsigned int window_size, window_addr, bus_width;
	int i;

	printk("MTD chip mapping driver for uCbootstrap\n");

	/* zero pointers: */
	for (i=0;i<MAX_FLASH_DEVICES;i++)
		flash_mtd[i] = 0;

	setpmask(FEF_SUPER_READ); /* set supervisory env var read permission */

	for (i=0;i<MAX_FLASH_DEVICES;i++) {
		valstr[0] = 0; val = NULL;

		/* retrieve FLASH[]_BASE */
		sprintf (varname, "%s_BASE", env_name[i]);
		/* cli(); */
		val = getbenv (varname);
		if (val == NULL) {
			/* sti(); */
			continue;
		}
		strncpy (valstr, val, ENVARNAME_LENGTH);
		/* sti(); */
		sscanf (valstr, "0x%x", &window_addr);

		/* retrieve FLASH[]_SIZE */
		sprintf (varname, "%s_SIZE", env_name[i]);
		/* cli(); */
		val = getbenv (varname);
		strncpy (valstr, val, ENVARNAME_LENGTH);
		/* sti(); */
		sscanf (valstr, "0x%x", &window_size);

		/* retrieve FLASH[]_BUS_WIDTH */
		sprintf (varname, "%s_BUS_WIDTH", env_name[i]);
		/* cli(); */
		val = getbenv (varname);
		strncpy (valstr, val, ENVARNAME_LENGTH);
		/* sti(); */
		sscanf (valstr, "%d", &bus_width);
		bus_width /= 8; /* bytes from bits */
		
		/* mleslie debug */
		printk("%s window address = 0x%08x; size = 0x%08x, bus width = %d bytes\n",
			   env_name[i], window_addr, window_size, bus_width);

		/* First probe at offset 0 */
		uCflash_map[i].buswidth = bus_width;
		uCflash_map[i].size     = window_size;
		uCflash_map[i].map_priv_1 = (unsigned long)ioremap(window_addr, window_size);

		if (!uCflash_map[i].map_priv_1) {
			printk("Failed to ioremap \n");
			return -EIO;
		}
		
		/* reset supervisory env var read permission */
		setpmask(FEF_USER_READ  | FEF_USER_WRITE  |
				 FEF_BOOT_READ  | FEF_BOOT_WRITE);
		
		printk("Probing for flash chip at 0x%08x:\n", window_addr);
		flash_mtd[i] = do_map_probe("cfi_probe", &(uCflash_map[i]));

#ifdef CONFIG_MTD_UCBOOTSTRAP_PARTS
		n_partitions = parse_ucbootstrap_partitions(flash_mtd[i], &(uCflash_map[i]), &parsed_parts);
		if (n_partitions > 0)
			printk(KERN_NOTICE "Found uCbootstrap partition table.\n");
		else if (n_partitions < 0)
			printk(KERN_NOTICE "Error looking for uCbootstrap partitions.\n");

		if (n_partitions > 0)
			add_mtd_partitions(flash_mtd[i], parsed_parts, n_partitions);
		else
#endif /* CONFIG_MTD_UCBOOTSTRAP_PARTS */
			add_mtd_device(flash_mtd[i]);
	}

	return 0;
}

static void __exit cleanup_uCbootstrap_maps(void)
{
	int i;

#ifdef CONFIG_MTD_UCBOOTSTRAP_PARTS
	if (parsed_parts) {
		for (i=0;i<MAX_FLASH_DEVICES;i++) {
			if (flash_mtd[i])
				del_mtd_partitions(flash_mtd[i]);
			map_destroy(flash_mtd[i]);
		}
	} else
#endif /* CONFIG_MTD_UCBOOTSTRAP_PARTS */
		for (i=0;i<MAX_FLASH_DEVICES;i++) {
			if (flash_mtd[i]) {
				del_mtd_device(flash_mtd[i]);
				map_destroy(flash_mtd[i]);
			}
		}
}

module_init(init_uCbootstrap_maps);
module_exit(cleanup_uCbootstrap_maps);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Leslie, Arcturus Networks Inc. [mleslie arcturusnetworks com]");
MODULE_DESCRIPTION("MTD map driver for platforms running Arcturus Networks uCbootstrap");

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
