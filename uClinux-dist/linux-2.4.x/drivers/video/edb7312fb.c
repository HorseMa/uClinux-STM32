/*
 * Framebuffer driver for the EDB7312 board
 * Copyright (C) 2003 Vladimir Ivanov, Nucleus Systems, Ltd.
 *
 * based mostly on:
 *
 *  linux/drivers/video/anakinfb.c
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/types.h>
#include <linux/fb.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <asm/memory.h>

#include <video/fbcon.h>
#ifdef CONFIG_FB_EDB7312_16BPP
#include <video/fbcon-cfb16.h>
#else
#include <video/fbcon-cfb12.h>
#endif

#include <asm/hardware/clps7111.h>

#define VID_X		320
#define VID_Y		240
#define	VID_PHYS	PAGE_OFFSET
#define	VID_VIRT	PAGE_OFFSET
#define VID_SIZE	0x1D000

#define FB_PHYS		(fb_p)
#define FB_VIRT		(fb_v)
#define	FB_SIZE		(fb_size)

static unsigned long fb_p, fb_v, fb_size;
static u16 colreg[16];
static int currcon = 0;
static struct fb_info fb_info;
static struct display display;

static char default_font_storage[40];
static char *default_font = "vtx6x8";

#ifdef CONFIG_FB_EDB7312_16BPP
#define TIME_REFRESH	(HZ / 4)
static struct timer_list tmrefr;

#define	FB_LLEN		(VID_X << 1)
#else
#define	FB_LLEN		((VID_X * 3) >> 1)
#endif

static void lcd_init(void)
{
	clps_writel(0x76543210, PALLSW);
	clps_writel(0xFEDCBA98, PALMSW);
	clps_writel((clps_readl(PMPCON) & ~0x0000FF00) | 0x00000800, PMPCON);
	clps_writel(clps_readl(SYSCON1) & ~SYSCON1_LCDEN, SYSCON1);
	clps_writel(0xE60F7C1F, LCDCON);
}

static void lcd_on(void)
{
	clps_writel(clps_readl(SYSCON1) | SYSCON1_LCDEN, SYSCON1);
	clps_writeb(clps_readb(PDDR) | EDB_PD2_LCDEN, PDDR);
	mdelay(1);
	clps_writeb(clps_readb(PDDR) | EDB_PD3_LCDBL, PDDR);
}

static void lcd_off(void)
{
	clps_writeb(clps_readb(PDDR) & ~EDB_PD3_LCDBL, PDDR);
	mdelay(1);
	clps_writeb(clps_readb(PDDR) & ~EDB_PD2_LCDEN, PDDR);
	clps_writel(clps_readl(SYSCON1) & ~SYSCON1_LCDEN, SYSCON1);
}

/*
 * color format is as follows:
 *
 * 12-bpp packed pixel: |BBBB|GGGG|RRRR|
 *
 * 16-bpp packed pixel: |RRRRR|GGGGGG|BBBBB|
 *
 */

static int
edb7312fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			u_int *transp, struct fb_info *info)
{
	if (regno > 15)
		return 1;

#ifdef CONFIG_FB_EDB7312_16BPP
	*red   = (colreg[regno] & 0xF800) <<  0;
	*green = (colreg[regno] & 0x07E0) <<  5;
	*blue  = (colreg[regno] & 0x001F) << 11;
#else
	*red   = (colreg[regno] & 0x00F) << 12;
	*green = (colreg[regno] & 0x0F0) <<  8;
	*blue  = (colreg[regno] & 0xF00) <<  4;
#endif
	*transp = 0;
	return 0;
}

static int
edb7312fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			u_int transp, struct fb_info *info)
{
	if (regno > 15)
		return 1;

#ifdef CONFIG_FB_EDB7312_16BPP
	colreg[regno] = ((red   >>  0) & 0xF800) |
			((green >>  5) & 0x07E0) |
			((blue  >> 11) & 0x001F);
#else
	colreg[regno] = ((red   >> 12) & 0x00F) | 
			((green >>  8) & 0x0F0) |
			((blue  >>  4) & 0xF00);
#endif
	return 0;
}

static int
edb7312fb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, "edb7312fb");
	fix->smem_start = FB_PHYS;
	fix->smem_len = FB_SIZE;
        fix->line_length = FB_LLEN;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->ywrapstep = 0;
	fix->accel = FB_ACCEL_NONE;
	return 0;
}
        
static int
edb7312fb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	memset(var, 0, sizeof(struct fb_var_screeninfo));
	var->xres = VID_X;
	var->yres = VID_Y;
	var->xres_virtual = VID_X;
	var->yres_virtual = VID_Y;
	var->xoffset = 0;
	var->yoffset = 0;
	var->grayscale = 0;
#ifdef CONFIG_FB_EDB7312_16BPP
	var->bits_per_pixel = 16;
	var->red.offset = 11;
	var->red.length = 5;
	var->green.offset = 5;
	var->green.length = 6;
	var->blue.offset = 0;
	var->blue.length = 5;
#else
	var->bits_per_pixel = 12;
	var->red.offset = 0;
	var->red.length = 4;
	var->green.offset = 4;
	var->green.length = 4;
	var->blue.offset = 8;
	var->blue.length = 4;
#endif
	var->transp.offset = 0;
	var->transp.length = 0;
	var->nonstd = 0;
	var->activate = FB_ACTIVATE_NOW;
	var->height = -1;
	var->width = -1;
	var->pixclock = 0;
	var->left_margin = 0;
	var->right_margin = 0;
	var->upper_margin = 0;
	var->lower_margin = 0;
	var->hsync_len = 0;
	var->vsync_len = 0;
	var->sync = 0;
	var->vmode = FB_VMODE_NONINTERLACED;
	return 0;
}

static int
edb7312fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	return -EINVAL;
}

static int
edb7312fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
	if (con == currcon)
		return fb_get_cmap(cmap, kspc, edb7312fb_getcolreg, info);
	else if (fb_display[con].cmap.len)
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(16), cmap, kspc ? 0 : 2);
	return 0;
}

static int
edb7312fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
	int err;

	if (!fb_display[con].cmap.len) {
		if ((err = fb_alloc_cmap(&fb_display[con].cmap, 16, 0)))
			return err;
	}
	if (con == currcon)
		return fb_set_cmap(cmap, kspc, edb7312fb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	return 0;
}

static int
edb7312fb_switch_con(int con, struct fb_info *info)
{ 
	currcon = con;
	return 0;
}

static int
edb7312fb_updatevar(int con, struct fb_info *info)
{
	return 0;
}

static void
edb7312fb_blank(int blank, struct fb_info *info)
{
	if (!blank)
		lcd_on();
	else
		lcd_off();
}

#ifdef CONFIG_FB_EDB7312_16BPP
static void refresh_disp (unsigned long dummy)
{
	int x, y;
	u16 *w;
	u8 *b;
	unsigned int w0, w1;

	w = (u16 *)FB_VIRT;
	b = (u8 *)VID_VIRT;
	for (y = VID_Y ; y ; y--) {
		for (x = VID_X >> 1 ; x ; x--) {
			w0 = *w++;
			w1 = *w++;
			*b++ = (w0 >> 12) |
			       ((w0 & 0x0780) >> 3);
			*b++ = ((w0 & 0x001E) >> 1) |
			       ((w1 & 0xF000) >> 8);
			*b++ = ((w1 & 0x0780) >> 7) |
			       ((w1 & 0x001E) << 3);
		}
	}

	tmrefr.expires = jiffies + TIME_REFRESH;
	add_timer (&tmrefr);
}
#endif

static int
edb7312fb_mmap(struct fb_info *info, struct file *file, struct vm_area_struct *vma)
{
	unsigned long off, start;
	u32 len;

	off = vma->vm_pgoff << PAGE_SHIFT;

	start = fb_p;
	len = PAGE_ALIGN(start & ~PAGE_MASK) + FB_SIZE;
	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	
	pgprot_val(vma->vm_page_prot) &= ~L_PTE_CACHEABLE;

	if (io_remap_page_range(vma->vm_start, off, vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static struct fb_ops edb7312fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	edb7312fb_get_fix,
	fb_get_var:	edb7312fb_get_var,
	fb_set_var:	edb7312fb_set_var,
	fb_get_cmap:	edb7312fb_get_cmap,
	fb_set_cmap:	edb7312fb_set_cmap,
	fb_mmap:	edb7312fb_mmap
};

#ifdef CONFIG_FB_EDB7312_16BPP
static int __init fbmem_alloc(void)
{
	int order = 0;
	unsigned long page, top;

	fb_size = VID_X * VID_Y * 2;
	fb_size = PAGE_ALIGN(fb_size);

	while (fb_size > (PAGE_SIZE * (1 << order)))
		order++;
	fb_v = __get_free_pages(GFP_KERNEL, order);
	if (!fb_v) {
		printk("edb7312fb: unable to allocate screen memory\n");
		return 1;
	}
	top = fb_v + (PAGE_SIZE * (1 << order));
	for (page = fb_v; page < PAGE_ALIGN(fb_v + fb_size); page += PAGE_SIZE)
		SetPageReserved(virt_to_page(page));
	for (page = fb_v + fb_size; page < top; page += PAGE_SIZE)
		free_page(page);
	fb_p = virt_to_phys((void *)fb_v);

	return 0;
}
#endif

int __init
edb7312fb_init(void)
{
	memset(&fb_info, 0, sizeof(struct fb_info));

#ifdef CONFIG_FB_EDB7312_16BPP
	if (fbmem_alloc())
		return -ENOMEM;
#else
	fb_p = VID_PHYS;
	fb_v = VID_VIRT;
	fb_size = VID_SIZE;
#endif

	strcpy(fb_info.modename, "edb7312fb");
	fb_info.node = -1;
	fb_info.flags = FBINFO_FLAG_DEFAULT;
	fb_info.fbops = &edb7312fb_ops;
	fb_info.disp = &display;
	strcpy(fb_info.fontname, default_font);
	fb_info.changevar = NULL;
	fb_info.switch_con = &edb7312fb_switch_con;
	fb_info.updatevar = &edb7312fb_updatevar;
	fb_info.blank = &edb7312fb_blank;

	memset(&display, 0, sizeof(struct display));
	edb7312fb_get_var(&display.var, 0, &fb_info);
	display.screen_base = (void *)FB_VIRT;
	display.line_length = FB_LLEN;
#ifdef CONFIG_FB_EDB7312_16BPP
	display.dispsw = &fbcon_cfb16;
#else
	display.dispsw = &fbcon_cfb12;
#endif
	display.dispsw_data = colreg;
	display.visual = FB_VISUAL_TRUECOLOR;
	display.type = FB_TYPE_PACKED_PIXELS;
	display.type_aux = 0;
	display.ypanstep = 0;
	display.ywrapstep = 0;
	display.can_soft_blank = 1;
	display.inverse = 0;

	memset((void *)VID_VIRT, 0, VID_SIZE);
#ifdef CONFIG_FB_EDB7312_16BPP
	memset((void *)FB_VIRT, 0, FB_SIZE);
#endif

	if (register_framebuffer(&fb_info) < 0)
		return -EINVAL;

	lcd_init();
	lcd_on();

#ifdef CONFIG_FB_EDB7312_16BPP
	init_timer(&tmrefr);
	tmrefr.function = refresh_disp;
	tmrefr.data = (unsigned long)NULL;
	tmrefr.expires = jiffies + TIME_REFRESH;
	add_timer (&tmrefr);
#endif

	return 0;
}

__init int
edb7312fb_setup(char *options)
{
	char *opt;

	if (!options || !*options)
		return 0;

	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;
		if (strncmp(opt, "font=", 5) == 0) {
			strncpy(default_font_storage, opt + 5, sizeof(default_font_storage));
			default_font = default_font_storage;
			continue;
		}

		printk (KERN_ERR "edb7312fb: unknown parameter: %s\n", opt);
	}

	return 0;
}
