/*
 * xilinxfb.c
 *
 * Xilinx TFT LCD frame buffer driver
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under the terms
 * of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

/*
 * This driver was based off of au1100fb.c by MontaVista, which in turn
 * was based off of skeletonfb.c, Skeleton for a frame buffer device by
 * Geert Uytterhoeven.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/wrapper.h> /* mem_map_(un)reserve */
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>

#include <asm/io.h>
#include <video/fbcon.h>
#include <video/fbcon-cfb32.h>

MODULE_AUTHOR("MontaVista Software, Inc. <source@mvista.com>");
MODULE_DESCRIPTION("Xilinx TFT LCD frame buffer driver");
MODULE_LICENSE("GPL");

/*
 * The interface to the framebuffer is nice and simple.  There are two
 * control registers.  The first tells the LCD interface where in memory
 * the frame buffer is (only the 11 most significant bits are used, so
 * don't start thinking about scrolling).  The second allows the LCD to
 * be turned on or off as well as rotated 180 degrees.
 */
#define NUM_REGS	2
#define REG_FB_ADDR	0
#define REG_CTRL	1
#define REG_CTRL_ENABLE	 0x0001
#define REG_CTRL_ROTATE	 0x0002
#if defined(CONFIG_FB_XILINX_ROTATE)
#define REG_CTRL_DEFAULT (REG_CTRL_ENABLE | REG_CTRL_ROTATE)
#else
#define REG_CTRL_DEFAULT (REG_CTRL_ENABLE)
#endif				/* CONFIG_FB_XILINX_ROTATE */

/*
 * The hardware only handles a single mode: 640x480 24 bit true
 * color. Each pixel gets a word (32 bits) of memory.  Within each word,
 * the 8 most significant bits are ignored, the next 8 bits are the red
 * level, the next 8 bits are the green level and the 8 least
 * significant bits are the blue level.  Each row of the LCD uses 1024
 * words, but only the first 640 pixels are displayed with the other 384
 * words being ignored.  There are 480 rows.
 */
#define BYTES_PER_PIXEL	4
#define BITS_PER_PIXEL	(BYTES_PER_PIXEL * 8)
#define XRES		640
#define YRES		480
#define XRES_VIRTUAL	1024
#define YRES_VIRTUAL	YRES
#define LINE_LENGTH	(XRES_VIRTUAL * BYTES_PER_PIXEL)
#define FB_SIZE		(YRES_VIRTUAL * LINE_LENGTH)

/*
 * Here's the definition of the information needed per frame buffer.  We
 * start off with the generic structure and then follow that with our private
 * data.
 */
struct xilinxfb_info {
	struct fb_info_gen gen;
	struct xilinxfb_info *next;	/* The next node in info_list */
	u32 *regs;		/* Virtual address of our control registers */
	unsigned long fb_virt_start;	/* Virtual address of our frame buffer */
	dma_addr_t fb_phys;	/* Physical address of our frame buffer */
	u32 fbcon_cmap32[16];	/* Colormap */
	struct display disp;
};

struct xilinxfb_par {
	struct fb_var_screeninfo var;
};

/*
 * So we can handle multiple frame buffers with this one driver, we need
 * a list of xilinxfb_info's.
 */
static struct xilinxfb_info *info_list = NULL;
static spinlock_t info_lock = SPIN_LOCK_UNLOCKED;

/* Our parameters are unchangeable, so we only need one parameters struct. */
static struct xilinxfb_par current_par;

/*
 * Free the head of our info_list and remove it from the list.  Note
 * that this function does *not* unregister_framebuffer().  That is up
 * to the caller to do if it is appropriate.
 */
static void
remove_head_info(void)
{
	struct xilinxfb_info *i;

	/* Pull the head off of info_list. */
	spin_lock(&info_lock);
	i = info_list;
	info_list = i->next;
	spin_unlock(&info_lock);

	if (i->regs) {
		/* Turn off the display; the frame buffer is going away. */
		out_be32(i->regs + REG_CTRL, 0);
		iounmap(i->regs);
	}

	if (i->fb_virt_start)
		consistent_free((void *)i->fb_virt_start);

	kfree(i);
}

static int
xilinxfb_ioctl(struct inode *inode, struct file *file, u_int cmd,
	       u_long arg, int con, struct fb_info *info)
{
	/* nothing to do yet */
	return -EINVAL;
}

static struct fb_ops xilinxfb_ops = {
	owner:THIS_MODULE,
	fb_get_fix:fbgen_get_fix,
	fb_get_var:fbgen_get_var,
	fb_set_var:fbgen_set_var,
	fb_get_cmap:fbgen_get_cmap,
	fb_set_cmap:fbgen_set_cmap,
	fb_pan_display:fbgen_pan_display,
	fb_ioctl:xilinxfb_ioctl,
};

static void
xilinx_detect(void)
{
	/*
	 *  This function should detect the current video mode settings 
	 *  and store it as the default video mode
	 */

	/*
	 * Yeh, well, we're not going to change any settings so we're
	 * always stuck with the default ...
	 */

}

static int
xilinx_encode_fix(struct fb_fix_screeninfo *fix, const void *par,
		  struct fb_info_gen *info)
{
	struct xilinxfb_info *i = (struct xilinxfb_info *) info;

	memset(fix, 0, sizeof (struct fb_fix_screeninfo));

	strcpy(fix->id, "Xilinx");
	fix->smem_start = i->fb_phys;
	fix->smem_len = FB_SIZE;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->line_length = LINE_LENGTH;
	return 0;
}

static int
xilinx_decode_var(const struct fb_var_screeninfo *var, void *par,
		  struct fb_info_gen *info)
{
	struct xilinxfb_par *p = (struct xilinxfb_par *) par;

	if (var->xres != XRES
	    || var->yres != YRES
	    || var->xres_virtual != XRES_VIRTUAL
	    || var->yres_virtual != YRES_VIRTUAL
	    || var->bits_per_pixel != BITS_PER_PIXEL
	    || var->xoffset != 0
	    || var->yoffset != 0
	    || var->grayscale != 0
	    || var->nonstd != 0
	    || var->pixclock != 0
	    || var->left_margin != 0
	    || var->right_margin != 0
	    || var->hsync_len != 0
	    || var->vsync_len != 0
	    || var->sync != 0 || var->vmode != FB_VMODE_NONINTERLACED) {
		return -EINVAL;
	}

	memset(p, 0, sizeof (struct xilinxfb_par));
	p->var = *var;
	p->var.red.offset = 16;
	p->var.red.length = 8;
	p->var.green.offset = 8;
	p->var.green.length = 8;
	p->var.blue.offset = 0;
	p->var.blue.length = 8;
	p->var.transp.offset = 0;
	p->var.transp.length = 0;
	p->var.red.msb_right = 0;
	p->var.green.msb_right = 0;
	p->var.blue.msb_right = 0;
	p->var.transp.msb_right = 0;
	p->var.height = 99;	/* in mm of NEC NL6448BC20-08 on ML300 */
	p->var.width = 132;	/* in mm of NEC NL6448BC20-08 on ML300 */

	return 0;
}

static int
xilinx_encode_var(struct fb_var_screeninfo *var, const void *par,
		  struct fb_info_gen *info)
{
	struct xilinxfb_par *p = (struct xilinxfb_par *) par;

	*var = p->var;
	return 0;
}

static void
xilinx_get_par(void *par, struct fb_info_gen *info)
{
	struct xilinxfb_par *p = (struct xilinxfb_par *) par;

	*p = current_par;
}

static void
xilinx_set_par(const void *par, struct fb_info_gen *info)
{
	/* nothing to do: we don't change any settings */
}

static int
xilinx_getcolreg(unsigned regno, unsigned *red, unsigned *green,
		 unsigned *blue, unsigned *transp, struct fb_info *info)
{
	struct xilinxfb_info *i = (struct xilinxfb_info *) info;
	unsigned r, g, b;

	if (regno >= 16)
		return 1;

	r = (i->fbcon_cmap32[regno] & 0xFF0000) >> 16;
	g = (i->fbcon_cmap32[regno] & 0x00FF00) >> 8;
	b = (i->fbcon_cmap32[regno] & 0x0000FF);

	*red = r << 8;
	*green = g << 8;
	*blue = b << 8;
	*transp = 0;

	return 0;
}

static int
xilinx_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
		 unsigned transp, struct fb_info *info)
{
	struct xilinxfb_info *i = (struct xilinxfb_info *) info;

	if (regno >= 16)
		return 1;

	/* We only handle 8 bits of each color. */
	red >>= 8;
	green >>= 8;
	blue >>= 8;
	i->fbcon_cmap32[regno] = ((unsigned long) red << 16
				  | (unsigned long) green << 8
				  | (unsigned long) blue);
	return 0;
}

static int
xilinx_pan_display(const struct fb_var_screeninfo *var,
		   struct fb_info_gen *info)
{
	if (var->xoffset != 0 || var->yoffset != 0)
		return -EINVAL;

	return 0;
}

static int
xilinx_blank(int blank_mode, struct fb_info_gen *info)
{
	struct xilinxfb_info *i = (struct xilinxfb_info *) info;

	switch (blank_mode) {
	case VESA_NO_BLANKING:
		/* turn on panel */
		out_be32(i->regs + REG_CTRL, REG_CTRL_DEFAULT);
		break;

	case VESA_VSYNC_SUSPEND:
	case VESA_HSYNC_SUSPEND:
	case VESA_POWERDOWN:
		/* turn off panel */
		out_be32(i->regs + REG_CTRL, 0);
	default:
		break;

	}
	return 0;
}

static void
xilinx_set_disp(const void *unused, struct display *disp,
		struct fb_info_gen *info)
{
	struct xilinxfb_info *i = (struct xilinxfb_info *) info;

	disp->screen_base = (char *) i->fb_virt_start;

	disp->dispsw = &fbcon_cfb32;
	disp->dispsw_data = i->fbcon_cmap32;
}

static struct fbgen_hwswitch xilinx_switch = {
	xilinx_detect,
	xilinx_encode_fix,
	xilinx_decode_var,
	xilinx_encode_var,
	xilinx_get_par,
	xilinx_set_par,
	xilinx_getcolreg,
	xilinx_setcolreg,
	xilinx_pan_display,
	xilinx_blank,
	xilinx_set_disp
};

static int __init
probe(int index)
{
	u32 *phys_reg_addr;
	struct xilinxfb_info *i;
	struct page *page, *end_page;

	switch (index) {
#if defined(CONFIG_XILINX_TFT_CNTLR_REF_0_INSTANCE)
	case 0:
		phys_reg_addr = 
			(u32 *) CONFIG_XILINX_TFT_CNTLR_REF_0_DCR_BASEADDR;
		break;
#if defined(CONFIG_XILINX_TFT_CNTRLR_REF_1_INSTANCE)
	case 1:
		phys_reg_addr = 
			(u32 *) CONFIG_XILINX_TFT_CNTLR_REF_1_DCR_BASEADDR;
		break;
#if defined(CONFIG_XILINX_TFT_CNTRLR_REF_2_INSTANCE)
	case 2:
		phys_reg_addr = 
			(u32 *) CONFIG_XILINX_TFT_CNTLR_REF_2_DCR_BASEADDR;
		break;
#if defined(CONFIG_XILINX_TFT_CNTLR_REF_3_INSTANCE)
#error Edit this file to add more devices.
#endif				/* 3 */
#endif				/* 2 */
#endif				/* 1 */
#endif				/* 0 */
	default:
		return -ENODEV;
	}

	/* Convert DCR register address to OPB address */
	phys_reg_addr = (unsigned *)(((unsigned)phys_reg_addr*4)+0xd0000000);

	/* Allocate the info and zero it out. */
	i = (struct xilinxfb_info *) kmalloc(sizeof (struct xilinxfb_info),
					     GFP_KERNEL);
	if (!i) {
		printk(KERN_ERR "Could not allocate Xilinx "
		       "frame buffer #%d information.\n", index);
		return -ENOMEM;
	}
	memset(i, 0, sizeof (struct xilinxfb_info));

	/* Make it the head of info_list. */
	spin_lock(&info_lock);
	i->next = info_list;
	info_list = i;
	spin_unlock(&info_lock);

	/*
	 * At this point, things are ok for us to call remove_head_info() to
	 * clean up if we run into any problems; i is on info_list and
	 * all the pointers are zeroed because of the memset above.
	 */

	i->fb_virt_start = (unsigned long) consistent_alloc(GFP_KERNEL|GFP_DMA,
							    FB_SIZE,
							    &i->fb_phys);
	if (!i->fb_virt_start) {
		printk(KERN_ERR "Could not allocate frame buffer memory "
		       "for Xilinx device #%d.\n", index);
		remove_head_info();
		return -ENOMEM;
	}

	/*
	 * The 2.4 PPC version of consistent_alloc does not set the
	 * pages reserved.  The pages need to be reserved so that mmap
	 * will work.  This means that we need the following code.  When
	 * consistent_alloc gets fixed, this will no longer be needed.
	 * Note that in 2.4, consistent_alloc doesn't free up the extra
	 * pages either.  This is already fixed in 2.5.
	 */
	page = virt_to_page(__va(i->fb_phys));
	end_page = page + ((FB_SIZE+PAGE_SIZE-1)/PAGE_SIZE);
	while (page < end_page)
		mem_map_reserve(page++);

	/* Clear the frame buffer. */
	memset((void *) i->fb_virt_start, 0, FB_SIZE);

	/* Map the control registers in. */
	i->regs = (u32 *) ioremap((unsigned long) phys_reg_addr, NUM_REGS);

	/* Tell the hardware where the frame buffer is. */
	out_be32(i->regs + REG_FB_ADDR, i->fb_phys);

	/* Turn on the display. */
	out_be32(i->regs + REG_CTRL, REG_CTRL_DEFAULT);

	current_par.var.xres = XRES;
	current_par.var.xres_virtual = XRES_VIRTUAL;
	current_par.var.yres = YRES;
	current_par.var.yres_virtual = YRES_VIRTUAL;
	current_par.var.bits_per_pixel = BITS_PER_PIXEL;

	i->gen.parsize = sizeof (struct xilinxfb_par);
	i->gen.fbhw = &xilinx_switch;

	strcpy(i->gen.info.modename, "Xilinx LCD");
	i->gen.info.changevar = NULL;
	i->gen.info.node = -1;

	i->gen.info.fbops = &xilinxfb_ops;
	i->gen.info.disp = &i->disp;
	i->gen.info.switch_con = &fbgen_switch;
	i->gen.info.updatevar = &fbgen_update_var;
	i->gen.info.blank = &fbgen_blank;
	i->gen.info.flags = FBINFO_FLAG_DEFAULT;

	/* This should give a reasonable default video mode */
	fbgen_get_var(&i->disp.var, -1, &i->gen.info);
	fbgen_do_set_var(&i->disp.var, 1, &i->gen);
	fbgen_set_disp(-1, &i->gen);
	fbgen_install_cmap(0, &i->gen);
	if (register_framebuffer(&i->gen.info) < 0) {
		printk(KERN_ERR "Could not register frame buffer "
		       "for Xilinx device #%d.\n", index);
		remove_head_info();
		return -EINVAL;
	}
	printk(KERN_INFO "fb%d: %s frame buffer at 0x%08X mapped to 0x%08lX\n",
	       GET_FB_IDX(i->gen.info.node), i->gen.info.modename,
	       i->fb_phys, i->fb_virt_start);

	return 0;
}

static int __init
xilinxfb_init(void)
{
	int index = 0;

	while (probe(index++) == 0) ;

	/* If we found at least one, report success. */
	return (index > 1) ? 0 : -ENODEV;
}

static void __exit
xilinxfb_cleanup(void)
{
	while (info_list) {
		unregister_framebuffer((struct fb_info *) info_list);
		remove_head_info();
	}
}

EXPORT_NO_SYMBOLS;

module_init(xilinxfb_init);
module_exit(xilinxfb_cleanup);
