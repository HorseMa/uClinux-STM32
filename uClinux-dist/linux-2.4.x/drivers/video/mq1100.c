/*
 * linux/drivers/video/mq1100fb.c -- Framebuffer driver for MediaQ 1100 controller
 *
 * The driver will work with Sony Clie (Dragonball-based PalmOS handheld) LCD panel
 * screen. Initially no hardware cursor support. No support for changing real screen
 * resolution (no point with LCD). No accelerated features support.
 *
 * Created 08/2004 by Anton Solovyev, anton@solovyev.com
 *
 * based on:
 *
 * linux/drivers/video/skeletonfb.c -- Skeleton for a frame buffer device
 *
 *  Created 28 Dec 1997 by Geert Uytterhoeven
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
 
#if defined(CONFIG_M68EZ328)
#include <asm/MC68EZ328.h>
#elif defined(CONFIG_M68VZ328)
#include <asm/MC68VZ328.h>
#else
#error Wrong architecture!
#endif

/* Make noise on the console */
/* #define MQ1100_DEBUG 1 */
#undef MQ1100_DEBUG
#ifdef MQ1100_DEBUG
#define DPRINTK(fmt, args...) printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif
/* Register addresses */
#define MQ1100_BASE (0x1F000000)
#define MQ1100_REG_BASE (MQ1100_BASE + 0x40000)
#define MQ1100_GC_CONTROL_REG (MQ1100_REG_BASE + 0x180)
#define MQ1100_GC_WINDOW_START_REG (MQ1100_REG_BASE + 0x1B0)
#define MQ1100_GC_WINDOW_STRIDE_REG (MQ1100_REG_BASE + 0x1B8)
#define MQ1100_GC_HOR_WINDOW_CONTROL_REG (MQ1100_REG_BASE + 0x1A0)
#define MQ1100_GC_VER_WINDOW_CONTROL_REG (MQ1100_REG_BASE + 0x1A4)
#define MQ1100_GC_PALETTE_REG(x) (MQ1100_REG_BASE + 0x800 + (x) * 4)
/* The horrible world of bitmasks */
#define MQ1100_GC_ENABLE_IMAGE_WINDOW_MASK (1 << 3)
#define MQ1100_GC_COLOR_DEPTH_MASK (0xF << 4)
#define MQ1100_GC_ENABLE_HW_CURSOR_MASK (1 << 8)
#define MQ1100_GC_DOUBLE_BUFFER_CONTROL_MASK (3 << 10)
#define MQ1100_GC_HOR_DOUBLING_MASK (1 << 14)
#define MQ1100_GC_VER_DOUBLING_MASK (1 << 15)
#define MQ1100_GC_RED_MASK (0x3F << 2)
#define MQ1100_GC_GREEN_MASK (0x3F << 10)
#define MQ1100_GC_BLUE_MASK (0x3F << 18)
#define MQ1100_GC_GRAY_HI_MASK (3 << 14)
#define MQ1100_GC_GRAY_LO_MASK (0x3F << 18)
#define MQ1100_GC_WINDOW_START_MASK (0x3FFFF << 0)
#define MQ1100_GC_WINDOW_STRIDE_MASK (0xFFFF << 0)
#define MQ1100_GC_HOR_WINDOW_START_MASK (0x7FF)
#define MQ1100_GC_HOR_WINDOW_WIDTH_MASK (0x7FF << 16)
#define MQ1100_GC_VER_WINDOW_START_MASK (0x3FF << 0)
#define MQ1100_GC_VER_WINDOW_WIDTH_MASK (0x3FF << 16)
/* Constants */
#define MQ1100_GC_SINGLE_PIX 0
#define MQ1100_GC_MAIN_WIN_ACIVE 0
#define MQ1100_GC_1BPP_COLOR 0
#define MQ1100_GC_2BPP_COLOR 1
#define MQ1100_GC_4BPP_COLOR 2
#define MQ1100_GC_8BPP_COLOR 3
#define MQ1100_GC_16BPP_COLOR 0x0C
#define MQ1100_GC_1BPP_GRAY 0x08
#define MQ1100_GC_2BPP_GRAY 0x09
#define MQ1100_GC_4BPP_GRAY 0x0A
#define MQ1100_GC_8BPP_GRAY 0x0B
/* Misc constants */
#define MQ1100_GC_VIDEOMEM_BASE (MQ1100_BASE)
#define MQ1100_GC_VIDEOMEM_SIZE (256 * 1024)
#define HRES 320
#define VRES 320

/*
*    MQ1100 FB Info
*/
struct mq1100fb_info
{
	struct fb_info_gen	gen;
};

/*
 *  The hardware specific data in this structure uniquely defines a video
 *  mode.
 */
struct mq1100fb_par
{
	u32	bpp;
	u32	grayscale;
	u32	xres_virtual;
	u32	yres_virtual;
	u32	xoffset;
	u32	yoffset;
};

static struct mq1100fb_info fb_info;
static struct mq1100fb_par current_par, default_par = {8, 0, HRES, VRES, 0, 0};
static int current_par_valid = 0;
static struct display disp;
static union
{
#ifdef FBCON_HAS_CFB16
	u16 cfb16[16];
#endif
} fbcon_cmap;

/* ------------ Hardware Dependent Functions ------------ */

/*
 * Change byte order
 */
static inline u32 swapbytes(u32 val)
{
	return (((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8) | ((val & 0x0000FF00) << 8) | ((val & 0x000000FF) << 24));
}

/*
 *  Write a GC register. We get a chance to insert a debug printk and swap bytes
 */
static inline void writereg(u32 reg, u32 val)
{
	DPRINTK("reg: %lX, val: %lX\n", (unsigned long) reg, (unsigned long) val);
	*((volatile u32*) reg) = swapbytes(val);
}

/*
 *  Read a GC register. We get a chance to insert a debug printk and swap bytes
 */
static inline u32 readreg(u32 reg)
{
	u32 val;

	val = swapbytes(*((volatile u32*) reg));
	DPRINTK("readreg(): reg: %lX val: %lX\n", (unsigned long) reg, (unsigned long) val);
	return val;
}

/*
 * Count trailing zeros
 */
static inline int countz(u32 val)
{
	int i;
	for(i = 0; i < 32; i++)
                if(val & (1 << i)) break;
        return i;
}

/*
 * Get bits with given mask
 */
static inline u32 getbits(u32 value, u32 mask)
{
	return (value & mask) >> (countz(mask));
}

/*
 * Set bits with given mask
 */
static inline u32 setbits(u32 value, u32 mask, u32 setto)
{
	return (value & ~mask) | (setto << countz(mask));
}

/*
 *  This function should fill in the 'fix' structure based on the values
 *  in the `par' structure.
 */
static int mq1100_encode_fix(struct fb_fix_screeninfo* fix, const void* fb_par, struct fb_info_gen* info)
{
	struct mq1100fb_par *par = (struct mq1100fb_par*) fb_par;
	DPRINTK("enter\n");

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, "MediaQ 1100");
	fix->smem_start = MQ1100_GC_VIDEOMEM_BASE;
	fix->smem_len = MQ1100_GC_VIDEOMEM_SIZE;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	if(par->bpp == 16)
	{
		fix->visual = FB_VISUAL_TRUECOLOR;
	}
	else
	{
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	}
	fix->xpanstep = fix->ypanstep = (par->bpp <= 8) ? (8 / par->bpp) : 1;
	fix->ywrapstep = 0;
	fix->line_length = par->bpp * par->xres_virtual / 8;
	fix->mmio_start = 0;
	fix->mmio_len = 0;
        fix->accel = FB_ACCEL_NONE;
	
	DPRINTK("exit\n");
	return 0;
}

/*
 *  Get the video params out of 'var'. If a value doesn't fit, round it up,
 *  if it's too big, return -EINVAL.
 *
 *  Suggestion: Round up in the following order: bits_per_pixel, xres,
 *  yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *  bitfields, horizontal timing, vertical timing.
 */
static int mq1100_decode_var(const struct fb_var_screeninfo* var, void* fb_par, struct fb_info_gen* info)
{
	struct mq1100fb_par* par = (struct mq1100fb_par*) fb_par;
	DPRINTK("enter\n");

	/* bpp */
	if(var->bits_per_pixel != 1 && var->bits_per_pixel != 2 && var->bits_per_pixel != 4 &&
		var->bits_per_pixel != 8 && var->bits_per_pixel != 16)
			return -EINVAL;
	par->bpp = var->bits_per_pixel;

	/* xres, yres */
	if(var->xres != HRES || var->yres != VRES)
		return -EINVAL;

	/* xres_virtual, yres_virtual */
	if(var->xres_virtual * var->yres_virtual * var->bits_per_pixel / 8 > MQ1100_GC_VIDEOMEM_SIZE ||
		var->xres_virtual < var->xres || var->yres_virtual < var->yres ||
			var->xres_virtual * var->bits_per_pixel % 8 || var->yres_virtual * var->bits_per_pixel % 8)
				return -EINVAL;
	par->xres_virtual = var->xres_virtual;
	par->yres_virtual = var->yres_virtual;

	/* xoffset, yoffset */
	if(var->xoffset > var->xres_virtual - HRES || var->yoffset > var->yres_virtual - VRES)
		return -EINVAL;
	par->xoffset = var->xoffset;
	par->yoffset = var->yoffset;

	/* timings */
	if(var->pixclock || var->left_margin || var->right_margin || var->upper_margin || var->lower_margin
		|| var->hsync_len || var->vsync_len || var->sync || var->vmode)
			return -EINVAL;

	/* grayscale */
	if(var->bits_per_pixel == 16 && var->grayscale)
		return -EINVAL;
	par->grayscale = var->grayscale;

	DPRINTK("exit\n");
	return 0;
}

/*
 *  Fill the 'var' structure based on the values in 'par' and maybe other
 *  values read out of the hardware.
 */
static int mq1100_encode_var(struct fb_var_screeninfo* var, const void* fb_par, struct fb_info_gen* info)
{
	struct mq1100fb_par* par = (struct mq1100fb_par*) fb_par;
	DPRINTK("enter\n");

	memset(var, 0, sizeof(*var));
	var->xres = HRES;
	var->yres = VRES;
	var->xres_virtual = par->xres_virtual;
	var->yres_virtual = par->yres_virtual;
	var->xoffset = par->xoffset;
	var->yoffset = par->yoffset;
	var->bits_per_pixel = par->bpp;
	var->grayscale = par->grayscale;
	if(par->bpp == 16)
	{
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
	}
	else
	{
		var->red.length = par->bpp;
		var->green.length = par->bpp;
		var->blue.length = par->bpp;
		var->transp.length = 0;
	}
	var->nonstd = 0;
	var->activate = 0;
	var->height = 0;
	var->width = 0;
	var->accel_flags = 0;
	var->pixclock = 0;
	var->left_margin = 0;
	var->right_margin = 0;
	var->upper_margin = 0;
	var->lower_margin = 0;
	var->hsync_len = 0;
	var->vsync_len = 0;
	var->sync = 0;
	var->vmode = FB_VMODE_NONINTERLACED;

	DPRINTK("exit\n");
	return 0;
}

/*
 *   Fill in par structure. If current par is valid, just copy, otherwise it is an init and copy default
 */
static void mq1100_get_par(void* fb_par, struct fb_info_gen* info)
{
	struct mq1100fb_par* par = (struct mq1100fb_par *) fb_par;
	DPRINTK("enter\n");

	if(current_par_valid)
	{
		*par = current_par;
	}
	else
	{
		*par = default_par;
	}
	DPRINTK("exit\n");
}

/*
 *  Set the hardware according to 'par'.
 */
static void mq1100_set_par(const void* fb_par, struct fb_info_gen* info)
{
	struct mq1100fb_par* par = (struct mq1100fb_par *) fb_par;
	u32 window_start, window_stride;
	u32 reg;
	DPRINTK("enter\n");
	
	/* Set visual */
	reg = readreg(MQ1100_GC_CONTROL_REG);
	switch(par->grayscale)
	{
		case 0:
			switch(par->bpp)
			{
			case 1:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_1BPP_COLOR);
				break;
			case 2:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_2BPP_COLOR);
				break;
			case 4:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_4BPP_COLOR);
				break;
			case 8:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_8BPP_COLOR);
				break;
			case 16:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_16BPP_COLOR);
				break;
			}
			break;
		case 1:
			switch(par->bpp)
			{
			case 1:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_1BPP_GRAY);
				break;
			case 2:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_2BPP_GRAY);
				break;
			case 4:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_4BPP_GRAY);
				break;
			case 8:
				reg = setbits(reg, MQ1100_GC_COLOR_DEPTH_MASK, MQ1100_GC_8BPP_GRAY);
				break;
			}
			break;
	}
	writereg(MQ1100_GC_CONTROL_REG, reg);

	/* No pixel doubling */
	reg = readreg(MQ1100_GC_CONTROL_REG);
	reg = setbits(reg, MQ1100_GC_HOR_DOUBLING_MASK, MQ1100_GC_SINGLE_PIX);
	reg = setbits(reg, MQ1100_GC_VER_DOUBLING_MASK, MQ1100_GC_SINGLE_PIX);
	writereg(MQ1100_GC_CONTROL_REG, reg);

	/* Activate main window */
	reg = readreg(MQ1100_GC_CONTROL_REG);
	reg = setbits(reg, MQ1100_GC_DOUBLE_BUFFER_CONTROL_MASK, MQ1100_GC_MAIN_WIN_ACIVE);
	writereg(MQ1100_GC_CONTROL_REG, reg);

	/* Set window stride */
	window_stride = par->bpp * par->xres_virtual / 8;
	reg = readreg(MQ1100_GC_WINDOW_STRIDE_REG);
	reg = setbits(reg, MQ1100_GC_WINDOW_STRIDE_MASK, window_stride);
	writereg(MQ1100_GC_WINDOW_STRIDE_REG, reg);

	/* Set window start */
	window_start = (par->xoffset + (par->yoffset * par->xres_virtual)) * par->bpp / 8;
	reg = readreg(MQ1100_GC_WINDOW_START_REG);
	reg = setbits(reg, MQ1100_GC_WINDOW_START_MASK, window_start);
	writereg(MQ1100_GC_WINDOW_START_REG, reg);

	/* Horizontal and vertical window control */
	reg = readreg(MQ1100_GC_HOR_WINDOW_CONTROL_REG);
	reg = setbits(reg, MQ1100_GC_HOR_WINDOW_START_MASK, 0);
	reg = setbits(reg, MQ1100_GC_HOR_WINDOW_WIDTH_MASK, HRES - 1);
	writereg(MQ1100_GC_HOR_WINDOW_CONTROL_REG, reg);
	reg = readreg(MQ1100_GC_VER_WINDOW_CONTROL_REG);
	reg = setbits(reg, MQ1100_GC_VER_WINDOW_START_MASK, 0);
	reg = setbits(reg, MQ1100_GC_VER_WINDOW_WIDTH_MASK, VRES - 1);
	writereg(MQ1100_GC_VER_WINDOW_CONTROL_REG, reg);

	current_par = *par;
	current_par_valid = 1;

	DPRINTK("exit\n");
}

/*
 *  Read a single color register and split it into colors/transparent.
 *  The return values must have a 16 bit magnitude.
 *  Return != 0 for invalid regno.
 */
static int mq1100_getcolreg(unsigned regno, unsigned* red, unsigned* green, unsigned* blue, unsigned* transp, struct fb_info* info)
{
	u32 reg;
	DPRINTK("enter\n");

	if(regno > 255)
	{
		return 1;
	}

	reg = readreg(MQ1100_GC_PALETTE_REG(regno));
	if(current_par.grayscale)
	{
		*red = *green = *blue = ((getbits(reg, MQ1100_GC_GRAY_HI_MASK) << 6) | getbits(reg, MQ1100_GC_GRAY_LO_MASK)) << 8;
	}
	else
	{
		*red = getbits(reg, MQ1100_GC_RED_MASK) << 10;
		*green = getbits(reg, MQ1100_GC_GREEN_MASK) << 10;
		*blue = getbits(reg, MQ1100_GC_BLUE_MASK) << 10;
	}
	*transp = 0;
	
	DPRINTK("exit\n");
	return 0;
}

/*
 *  Set a single color register. The values supplied have a 16 bit
 *  magnitude.
 *  Return != 0 for invalid regno.
 */
static int mq1100_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct fb_info* info)
{
	u32 reg;
	DPRINTK("enter\n");

	if(regno > 255)
	{
		return -EINVAL;
	}

	reg = 0;
	if(current_par.grayscale)
	{
		reg = setbits(reg, MQ1100_GC_GRAY_HI_MASK, 0xC0 & (red >> 8));
		reg = setbits(reg, MQ1100_GC_GRAY_LO_MASK, 0x3F & (red >> 8));
	}
	else
	{
		reg = setbits(reg, MQ1100_GC_RED_MASK, red >> 10);
		reg = setbits(reg, MQ1100_GC_GREEN_MASK, green >> 10);
		reg = setbits(reg, MQ1100_GC_BLUE_MASK, blue >> 10);
	}
	writereg(MQ1100_GC_PALETTE_REG(regno), reg);

	if (regno < 16)
	{
		/*
  		 *  Make the first 16 colors of the palette available to fbcon
		 */
#ifdef FBCON_HAS_CFB16
		if(current_par.bpp == 16)
		{
			fbcon_cmap.cfb16[regno] = (red & 0xf800) | ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
		}
#endif

	}
	DPRINTK("exit\n");
	return 0;
}

/*
 *  Pan (or wrap, depending on the `vmode' field) the display using the
 *  `xoffset' and `yoffset' fields of the `var' structure.
 *  If the values don't fit, return -EINVAL.
 */
static int mq1100_pan_display(const struct fb_var_screeninfo *var, struct fb_info_gen *info)
{
	u32 window_start;
	u32 reg;
	DPRINTK("enter\n");

	window_start = (var->xoffset + (var->yoffset * var->xres_virtual)) * var->bits_per_pixel / 8;
	reg = readreg(MQ1100_GC_WINDOW_START_REG);
	reg = setbits(reg, MQ1100_GC_WINDOW_START_MASK, window_start);
	writereg(MQ1100_GC_WINDOW_START_REG, reg);

	DPRINTK("exit\n");
	return 0;
}

/*
 *  Blank the screen if blank_mode != 0, else unblank. If blank == NULL
 *  then the caller blanks by setting the CLUT (Color Look Up Table) to all
 *  black. Return 0 if blanking succeeded, != 0 if un-/blanking failed due
 *  to e.g. a video mode which doesn't support it. Implements VESA suspend
 *  and powerdown modes on hardware that supports disabling hsync/vsync:
 *    blank_mode == 2: suspend vsync
 *    blank_mode == 3: suspend hsync
 *    blank_mode == 4: powerdown
 */
static int mq1100_blank(int blank_mode, struct fb_info_gen* info)
{
	DPRINTK("enter\n");
	/* I do not want to mess with flat panel control */
	DPRINTK("exit\n");
	return -EINVAL;
}

/*
 *  Fill in a pointer with the virtual address of the mapped frame buffer.
 *  Fill in a pointer to appropriate low level text console operations (and
 *  optionally a pointer to help data) for the video mode `par' of your
 *  video hardware. These can be generic software routines, or hardware
 *  accelerated routines specifically tailored for your hardware.
 *  If you don't have any appropriate operations, you must fill in a
 *  pointer to dummy operations, and there will be no text output.
 */
static void mq1100_set_disp(const void* fb_par, struct display* disp, struct fb_info_gen* info)
{
	struct mq1100fb_par* par = (struct mq1100fb_par*) fb_par;
	DPRINTK("enter\n");

	disp->screen_base = (void*) MQ1100_GC_VIDEOMEM_BASE;
	disp->scrollmode = SCROLL_YREDRAW;

	switch(par->bpp)
	{
#ifdef FBCON_HAS_MFB
		case 1:
			disp->dispsw = &fbcon_mfb;
			break;
#endif
#ifdef FBCON_HAS_CFB2
		case 2:
			disp->dispsw = &fbcon_cfb2;
			break;
#endif
#ifdef FBCON_HAS_CFB4
		case 4:
			disp->dispsw = &fbcon_cfb4;
			break;
#endif
#ifdef FBCON_HAS_CFB8
		case 8:
			disp->dispsw = &fbcon_cfb8;
			break;
#endif
#ifdef FBCON_HAS_CFB16
		case 16:
			disp->dispsw = &fbcon_cfb16;
			disp->dispsw_data = fbcon_cmap.cfb16;
			break;
#endif
		default:
			disp->dispsw = &fbcon_dummy;
	}

	DPRINTK("exit\n");
}

struct fbgen_hwswitch mq1100_switch =
{
	NULL,
	mq1100_encode_fix,
	mq1100_decode_var,
	mq1100_encode_var,
	mq1100_get_par,
	mq1100_set_par,
	mq1100_getcolreg,
	mq1100_setcolreg,
	mq1100_pan_display,
	mq1100_blank,
	mq1100_set_disp
};

/* ---------------- Frame buffer operations --------------- */

/*
 *  In most cases the `generic' routines (fbgen_*) should be satisfactory.
 *  However, you're free to fill in your own replacements.
 */
static struct fb_ops mq1100fb_ops =
{
	owner:		THIS_MODULE,
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	fbgen_set_cmap,
	fb_pan_display:	fbgen_pan_display,
};

/* ------------ Hardware Independent Functions ------------ */

/*
 *  Initialization
 */
int __init mq1100fb_init(void)
{
	DPRINTK("enter\n");
	/* Generic code */
	fb_info.gen.parsize = sizeof(struct mq1100fb_par);
	fb_info.gen.fbhw = &mq1100_switch;
	strcpy(fb_info.gen.info.modename, "MediaQ 1100");
	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.node = -1;
	fb_info.gen.info.fbops = &mq1100fb_ops;
	fb_info.gen.info.disp = &disp;
	fb_info.gen.info.switch_con = &fbgen_switch;
	fb_info.gen.info.updatevar = &fbgen_update_var;
	fb_info.gen.info.blank = &fbgen_blank;
	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
	/* This should give a reasonable default video mode */
	fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
	fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
	fbgen_set_disp(-1, &fb_info.gen);
	fbgen_install_cmap(0, &fb_info.gen);
	if (register_framebuffer(&fb_info.gen.info) < 0)
		return -EINVAL;
	printk(KERN_INFO "fb%d: %s frame buffer device\n", GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);

	/* uncomment this if your driver cannot be unloaded */
	/* MOD_INC_USE_COUNT; */
	DPRINTK("exit\n");
	return 0;
}

/*
 *  Cleanup
 */
void mq1100fb_cleanup(struct fb_info* info)
{
	/*
	 *  If your driver supports multiple boards, you should unregister and
	 *  clean up all instances.
	 */
	DPRINTK("enter\n");
	unregister_framebuffer(info);
	DPRINTK("exit\n");
}

/*
 *  Setup
 */
int __init mq1100fb_setup(char* options)
{
	/* Parse user speficied options (`video=xxxfb:') */
	DPRINTK("enter\n");
	DPRINTK("exit\n");
	return 0;
}

/* ---------------------- Modularization ----------------------- */

#ifdef MODULE
MODULE_LICENSE("GPL");
int init_module(void)
{
	return mq1100fb_init();
}

void cleanup_module(void)
{
	mq1100fb_cleanup(void);
}
#endif /* MODULE */
