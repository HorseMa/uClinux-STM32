/*
 * drivers/mtd/maps
 *
 * MTD Map file for IXP425 based systems
 *
 * Original Author: Intel Corporation
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2002 Intel Corporation
 *
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/ioport.h>
#include <linux/reboot.h>
#include <asm/io.h>

#define WINDOW_ADDR 	0x50000000
#define WINDOW_ADDR2 	0x51000000
#define BUSWIDTH 	2
#define WINDOW_SIZE	0x01000000

#ifndef __ARMEB__
#define	BYTE0(h)	((h) & 0xFF)
#define	BYTE1(h)	(((h) >> 8) & 0xFF)
#else
#define	BYTE0(h)	(((h) >> 8) & 0xFF)
#define	BYTE1(h)	((h) & 0xFF)
#endif

static __u16 ixp425_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

/*
 * The IXP425 expansion bus only allows 16-bit wide acceses
 * when attached to a 16-bit wide device (such as the 28F128J3A),
 * so we can't use a memcpy_fromio as it does byte acceses.
 */
static void ixp425_copy_from(struct map_info *map, void *to,
    unsigned long from, ssize_t len)
{
	int i;
	u8 *dest = (u8*)to;
	u16 *src = (u16 *)(map->map_priv_1 + from);
	u16 data;

	for(i = 0; i < (len / 2); i++) {
		data = src[i];
		dest[i * 2] = BYTE0(data);
		dest[i * 2 + 1] = BYTE1(data);
	}

	if(len & 1)
		dest[len - 1] = BYTE0(src[i]);
}

static void ixp425_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static struct map_info ixp425_map = {
	name: 		"IXP425 Flash",
	buswidth: 	BUSWIDTH,
	read16:		ixp425_read16,
	copy_from:	ixp425_copy_from,
	write16:	ixp425_write16,
};


#ifdef CONFIG_MACH_MONTEJADE
/*
 *	The Intel Monte Jade has a second optional flash device on CS1.
 */
static struct map_info ixp425_map1 = {
	name: 		"IXP425 Flash",
	buswidth: 	BUSWIDTH,
	read16:		ixp425_read16,
	copy_from:	ixp425_copy_from,
	write16:	ixp425_write16,
};
static struct mtd_info *ixp425_mtd1;
static struct resource *mtd_resource1;
static struct mtd_partition ixp425_partitions1[] = {
	{
		name:	"spare",
		offset:	0x00000000
    	},
};
#endif /* CONFIG_MACH_MONTEJADE */

/*
 * Put flash back in read mode so RedBoot can boot properly.
 */
int ixp425_mtd_reboot(struct notifier_block *n, unsigned long code, void *p)
{
	if (code != SYS_RESTART)
		return NOTIFY_DONE;

	ixp425_write16(&ixp425_map, 0xff, 0x55 * 0x2);
	return NOTIFY_DONE;
}

static struct notifier_block ixp425_mtd_notifier = {
	notifier_call:ixp425_mtd_reboot,
	next:NULL,
	priority:0
};

#ifdef CONFIG_MTD_REDBOOT_PARTS
static struct mtd_partition *parsed_parts;
#endif

#ifdef CONFIG_MACH_MONTEJADE
static struct mtd_partition ixp425_partitions[] = {
	{
		name:	"RedBoot",
		offset:	0x00000000,
		size:	0x00080000
	},
	{
		name:	"config",
		offset:	0x00080000,
		size:	0x00040000
	},
	{
		name:	"image",
		offset:	0x000c0000,
		size:	0x00f00000
	},
	{
		name:	"zImage",
		offset:	0x000c0000,
		size:	0x00100000
	},
	{
		name:	"ramdisk",
		offset:	0x001c0000,
		size:	0x00e00000
	},
	{
		name:	"FIS directory",
		offset:	0x00fe0000,
		size:	0x00020000
	},
	{
		name:	"RedBoot config",
		offset:	0x00fff000,
		size:	0x00001000
	},
	{
		name:	"all",
		offset:	0
	},
};
#else
static struct mtd_partition ixp425_partitions[] = {
	{
		name:	"image",
		offset:	0x00040000,
		size:	0x00400000, 
    	},
	{
		name:	"user",
		offset:	0x00440000,
		size:	0x00B80000, /* Rest of flash space minus redboot 
				     * configuration sectors (-256k)    
				     */
	},
};
#endif

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *ixp425_mtd;
static struct resource *mtd_resource;

static void ixp425_exit(void)
{
    if(ixp425_mtd){
	del_mtd_partitions(ixp425_mtd);
	map_destroy(ixp425_mtd);
    }
    if (ixp425_map.map_priv_1)
	iounmap((void *)ixp425_map.map_priv_1);
    if (mtd_resource)
	release_mem_region(WINDOW_ADDR, WINDOW_SIZE);
    
#ifdef CONFIG_MTD_REDBOOT_PARTS
    if (parsed_parts)
	kfree(parsed_parts);
#endif

    unregister_reboot_notifier(&ixp425_mtd_notifier);

    /* Disable flash write */
    *IXP425_EXP_CS0 &= ~IXP425_FLASH_WRITABLE;

#ifdef CONFIG_MACH_MONTEJADE
    if(ixp425_mtd1){
	del_mtd_partitions(ixp425_mtd1);
	map_destroy(ixp425_mtd1);
    }
    if (ixp425_map1.map_priv_1)
	iounmap((void *)ixp425_map1.map_priv_1);
    if (mtd_resource1)
	release_mem_region(WINDOW_ADDR2, WINDOW_SIZE);
    
    /* Disable flash write */
    *IXP425_EXP_CS1 &= ~IXP425_FLASH_WRITABLE;
#endif /* CONFIG_MACH_MONTEJADE */
}

static int __init ixp425_init(void)
{
    int res = -1, npart;

    /* Enable flash write */
    *IXP425_EXP_CS0 |= IXP425_FLASH_WRITABLE;

    ixp425_map.map_priv_1 = 0;
    mtd_resource = 
	request_mem_region(WINDOW_ADDR, WINDOW_SIZE, "ixp425 Flash");
    if(!mtd_resource) {
	printk(KERN_ERR "ixp425 flash: Could not request mem region.\n" );
	res = -ENOMEM;
	goto Error;
    }

    ixp425_map.map_priv_1 =
	(unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);
    if (!ixp425_map.map_priv_1) {
	printk("ixp425 Flash: Failed to map IO region. (ioremap)\n");
	res = -EIO;
	goto Error;
    }
    ixp425_map.size = WINDOW_SIZE;


    /* 
     * Probe for the CFI complaint chip
     * suposed to be 28F128J3A
     */
    ixp425_mtd = do_map_probe("cfi_probe", &ixp425_map);
    if (!ixp425_mtd) {
	res = -ENXIO;
	goto Error;
    }
    ixp425_mtd->module = THIS_MODULE;
   
    /* Initialize flash partiotions 
     * Note: Redeboot partition info table can be parsed by MTD, and used
     *       instead of hard-coded partions. TBD
     */

#ifdef CONFIG_MTD_REDBOOT_PARTS
    /* Try to parse RedBoot partitions */
    npart = parse_redboot_partitions(ixp425_mtd, &parsed_parts, 0);
    if (npart > 0) 
	res = add_mtd_partitions(ixp425_mtd, parsed_parts, npart);
    else   
#endif
	res = -EIO;

    if (res) {
	printk("Using static MTD partitions.\n");
	/* RedBoot partitions not found - use hardcoded partition table */
	res = add_mtd_partitions(ixp425_mtd, ixp425_partitions,
	    NB_OF(ixp425_partitions));
    }

    if (res)
	goto Error;

    register_reboot_notifier(&ixp425_mtd_notifier);

#ifdef CONFIG_MACH_MONTEJADE
    /*
     *	Possible second flash device on CS1...
     */
    *IXP425_EXP_CS1 = *IXP425_EXP_CS0;

    ixp425_map1.map_priv_1 = 0;
    mtd_resource1 = 
	request_mem_region(WINDOW_ADDR2, WINDOW_SIZE, "ixp425 Flash2");
    if(!mtd_resource1) {
	printk(KERN_ERR "ixp425 flash2: Could not request mem region.\n" );
	goto Done;
    }

    ixp425_map1.map_priv_1 =
	(unsigned long)ioremap(WINDOW_ADDR2, WINDOW_SIZE);
    if (!ixp425_map1.map_priv_1) {
	printk("ixp425 Flash2: Failed to map IO region. (ioremap)\n");
	goto Done;
    }
    ixp425_map1.size = WINDOW_SIZE;

    /* 
     * Probe for the CFI complaint chip
     * suposed to be 28F128J3A
     */
    ixp425_mtd1 = do_map_probe("cfi_probe", &ixp425_map1);
    if (!ixp425_mtd1)
	goto Done;

    ixp425_mtd1->module = THIS_MODULE;

     printk("Using static MTD partitions (on spare flash).\n");
     add_mtd_partitions(ixp425_mtd1, ixp425_partitions1, NB_OF(ixp425_partitions1));
#endif /* CONFIG_MACH_MONTEJADE */

Done:
    return res;
Error:
    ixp425_exit();
    return res;
}

module_init(ixp425_init);
module_exit(ixp425_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTD map driver for ixp425 evaluation board");

