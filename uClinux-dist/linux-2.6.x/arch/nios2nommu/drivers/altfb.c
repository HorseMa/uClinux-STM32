/*
 *  Altera VGA controller
 * 
 *  linux/drivers/video/vfb.c -- Virtual frame buffer device
 *
 *      Copyright (C) 2002 James Simmons
 *
 *	Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>

#define vgabase na_vga_controller_0
#define XRES 640
#define YRES 480
#define BPX  16

    /*
     *  RAM we reserve for the frame buffer. This defines the maximum screen
     *  size
     *
     *  The default can be overridden if the driver is compiled as a module
     */

#define VIDEOMEMSIZE	(XRES * YRES * (BPX>>3))

static struct fb_var_screeninfo altfb_default __initdata = {
	.xres =		XRES,
	.yres =		YRES,
	.xres_virtual =	XRES,
	.yres_virtual =	YRES,
	.bits_per_pixel = BPX,
#if (BPX == 16)
	.red =		{ 11, 5, 0 },
      	.green =	{ 5, 6, 0 },
      	.blue =		{ 0, 5, 0 },
#else  // BPX == 24
	.red =		{ 16, 8, 0 },
      	.green =	{ 8, 8, 0 },
      	.blue =		{ 0, 8, 0 },
#endif
      	.activate =	FB_ACTIVATE_NOW,
      	.height =	-1,
      	.width =	-1,
	// timing useless ?
      	.pixclock =	20000,
      	.left_margin =	64,
      	.right_margin =	64,
      	.upper_margin =	32,
      	.lower_margin =	32,
      	.hsync_len =	64,
      	.vsync_len =	2,
      	.vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo altfb_fix __initdata = {
	.id =		"Altera FB",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.line_length =  (XRES * (BPX>>3)),
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0,
	.accel =	FB_ACCEL_NONE,
};

static int altfb_mmap(struct fb_info *info,
		    struct vm_area_struct *vma);

static struct fb_ops altfb_ops = {
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_mmap	= altfb_mmap,
};


/* We implement our own mmap to set MAY_SHARE and add the correct size */
static int altfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	vma->vm_flags |= VM_MAYSHARE| VM_SHARED;

	vma->vm_start = info->fix.smem_start;
	vma->vm_end   = info->fix.smem_start + info->fix.smem_len;
	return 0;
}

    /*
     *  Initialisation
     */

static void altfb_platform_release(struct device *device)
{
	// This is called when the reference count goes to zero.
	dev_err(device, "This driver is broken, please bug the authors so they will fix it.\n");
}

static int __init altfb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	int retval = -ENOMEM;

	altfb_fix.smem_len = VIDEOMEMSIZE;
	altfb_fix.smem_start = __get_free_pages(GFP_KERNEL, get_order(altfb_fix.smem_len));
	if (!altfb_fix.smem_start) {
		printk("Unable to allocate %d PAGEs(%d Bytes) fb memory\n",
		       get_order(altfb_fix.smem_len), altfb_fix.smem_len);
		return -ENOMEM;
	}

	info = framebuffer_alloc(sizeof(u32) * 256, &dev->dev);
	if (!info)
		goto err;

	info->screen_base = ioremap(altfb_fix.smem_start, altfb_fix.smem_len);
	info->fbops = &altfb_ops;
	info->var = altfb_default;
	info->fix = altfb_fix;
	info->pseudo_palette = info->par;
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;

	retval = fb_alloc_cmap(&info->cmap, 256, 0);
	if (retval < 0)
		goto err1;

	retval = register_framebuffer(info);
	if (retval < 0)
		goto err2;
	platform_set_drvdata(dev, info);

	outl(0x0,vgabase+0);  // Reset the VGA controller
	outl(altfb_fix.smem_start, vgabase+4);  // Where our frame buffer starts
	outl(altfb_fix.smem_len, vgabase+8);  // amount of memory needed
	outl(0x1, vgabase+0);  // Set the go bit

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node, info->fix.id);
	// printk("vga %08x, video %08x+%08x\n",vgabase,videomemory,videomemorysize);
	return 0;
err2:
	fb_dealloc_cmap(&info->cmap);
err1:
	iounmap((void *)altfb_fix.smem_start);
	framebuffer_release(info);
err:
	free_pages(altfb_fix.smem_start, get_order(altfb_fix.smem_len));
	return retval;
}

static int altfb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		unregister_framebuffer(info);
		iounmap((void *)info->fix.smem_start);
		free_pages(info->fix.smem_start, get_order(info->fix.smem_len));
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver altfb_driver = {
	.probe	= altfb_probe,
	.remove = altfb_remove,
	.driver = {
		.name	= "altfb",
	},
};

static struct platform_device altfb_device = {
	.name	= "altfb",
	.id	= 0,
	.dev	= {
		.release = altfb_platform_release,
	}
};

static int __init altfb_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&altfb_driver);

	if (!ret) {
		ret = platform_device_register(&altfb_device);
		if (ret)
			platform_driver_unregister(&altfb_driver);
	}
	return ret;
}

module_init(altfb_init);

#ifdef MODULE
static void __exit altfb_exit(void)
{
	platform_device_unregister(&altfb_device);
	platform_driver_unregister(&altfb_driver);
}

module_exit(altfb_exit);

MODULE_LICENSE("GPL");
#endif				/* MODULE */
