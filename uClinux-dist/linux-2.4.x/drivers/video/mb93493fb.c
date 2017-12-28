/* mb93493fb.c: Fujitsu MB93493 FRV companion chip video driver
 *
 * Copyright (C) 2004, 2005 Red Hat, Inc. All Rights Reserved.
 * Written by Louis Hamilton (hamilton@redhat.com)
 *  - Derived from framework set by David Howells (dhowells@redhat.com)
 *  - Derived from skeletonfb.c, Created 28 Dec 1997 by Geert Uytterhoeven
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
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
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>
#include <video/fbcon.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>

#include <asm/dma.h>
#include <asm/mb93493-regs.h>
#include "../fujitsu/mb93493.h"

#define MB93493_FB_ALLOC_SIZE	(4 * 1024 * 1024)
#define NTSC_RVDC		240
#define MODE_STR_SZ		56

MODULE_AUTHOR("Louis Hamilton <hamilton@redhat.com>; Copyright (C) 2004 Red Hat, Inc.");
MODULE_DESCRIPTION("Framebuffer driver for Fujitsu MB93493");
MODULE_LICENSE("GPL");

#define DEBUG_PRINTS
#undef DEBUG_PRINTS

#ifdef DEBUG_PRINTS
#define kdebug(FMT,...) printk(FMT ,## __VA_ARGS__)
#define ktrace(FMT,...) printk(FMT ,## __VA_ARGS__)
#else
#define kdebug(FMT,...)
#define ktrace(FMT,...)
#endif

#define RECT_TEST
#undef RECT_TEST

typedef enum {
	DISPLAY_NOT_SET=0,
	DTYPE_VGA,
	DTYPE_LCD,
	DTYPE_PAL,
	DTYPE_NTSC
} mb93493fb_display_type;

#ifdef FBCON_HAS_CFB16
static uint16_t fbcon_cmap_YCbCr[16] = {
	[0] = 0x007f,		/* black (Y=  0, Cb/Cr=128) */
	[1 ... 15] = 0xff7f,	/* white (Y=255, Cb/Cr=128) */
};
#endif

/*****************************************************************************/
/*
 * MB93493 frame buffer description
 */
struct mb93493fb_info {
	/* generic frame buffer bits */
	struct fb_info_gen gen;

	/* Here starts the frame buffer device dependent part */
	/* You can use this to store e.g. the board number if you support */
	/* multiple boards */

	uint32_t fbcon_cmap[16];		/* basic palette */
	const struct fb_videomode *modedb;	/* video mode DB for this display type */
	int modedb_size;	/* size of video mode DB */
	char nocur;		/* true if cursor should be suppressed */
	char dma_xfer_btmfld;   /* flag to dma topfield or bottomfield */

	atomic_t open_count;	/* number of opens */
	uint8_t *framebuffer;	/* framebuffer storage or NULL */
	void *regs;		/* register base */
	int dma;		/* DMA channel */
};

/*****************************************************************************/
/*
 * the hardware specific parameters that uniquely define a video mode
 */
struct mb93493fb_par {

	/* register shadows */
	uint32_t rcursor;	/* cursor horiz & vert position */
	uint32_t rct1;		/* cursor colour 1 */
	uint32_t rct2;		/* cursor colour 2 */
	uint32_t rhdc;		/* horiz display period */
	uint32_t rh_margins;	/* horiz front & back porches and sync period */
	uint32_t rvdc;		/* vert display period */
	uint32_t rv_margins;	/* vert front & back porches and sync period */
	uint32_t rc;		/* control word */
	uint32_t rclock;	/* pixel clock divider and DMA req delay */
	uint32_t rblack;	/* horiz & vert black pixel insert length */
	uint32_t rs;		/* status word / interrupt control */
	uint64_t bci[32];	/* cursor pattern */

	/* geometry stuff */
	unsigned bpp;		/* 16, 24 or 32 */
	unsigned pitch;		/* offset in bytes of one line from the next */

	uint32_t vxres;		/* virtual xres */
	uint32_t vyres;		/* virtual yres */
	uint32_t xoffset;	/* virtual xres */
	uint32_t yoffset;	/* virtual yres */
};

/* video video mode bitfields */
struct mb93493_vmode {
	uint32_t pix_x;		/* x pixels */
	uint32_t pix_y;		/* y pixels */
	uint32_t pix_xv;	/* virtual x pixels */
	uint32_t pix_yv;	/* virtual y pixels */
	uint32_t pix_xoff;	/* x offset */
	uint32_t pix_yoff;	/* y offset */
	uint32_t pix_sz;	/* pixel size in bytes */

	/* DMA parameters */
	uint32_t dma_mode;
	uint32_t dma_ats;
	uint32_t dma_rs;

	/* VDC Register bitfields */
	uint32_t hls;		/* set interlace (RC)  */
	uint32_t pal;		/* set PAL output format (RC) */
	uint32_t cscv;		/* set color space conversion (RC) */
	uint32_t dbls;		/* set output format (RC)  */
	uint32_t r601;		/* set YCbCr output format (RC) */
	uint32_t tfop;		/* set polarity of TOPFIELD signal (RC) */
	uint32_t dsm;		/* set TOPFIELD polarity (RC) */
	uint32_t dfp;		/* image data transfer method (RC) */
	uint32_t die;		/* interrupt notification (RC) */
	uint32_t enop;		/* polarity of ENABLE signal (RC) */
	uint32_t vsop;		/* polarity of vertical syncronous signal (RC) */
	uint32_t hsop;		/* polarity of horizontal syncronous signal (RC) */
	uint32_t dsr;		/* reset control (RC) */
	uint32_t csron;		/* cursor display control (RC) */
	uint32_t dpf;		/* set output format RGB/YCbCr (RC) */
	uint32_t dms;		/* set output running state (RC) */

	/* rhtc = rhfp + rhsc + rhbp + rhdc + rhipx2 */
	uint32_t rhtc;		/* total count horiz display period (RHDC) */
	uint32_t rhdc;		/* rhdc display period (RHDC) */

	uint32_t rhfp;		/* horiz front porch (RH_MARGINS) */
	uint32_t rhsc;		/* horiz syncronous period (RH_MARGINS) */
	uint32_t rhbp;		/* horiz back porch (RH_MARGINS) */

	/* rvtc = rvfp + rvsc + rvbp + rvdc + rvipx2 */
	uint32_t rvtc;		/* total count vertical display period (RVDC) */
	uint32_t rvdc;		/* rvdc vertical display period (RVDC) */

	uint32_t rvfp;		/* vertical front porch (RV_MARGINS) */
	uint32_t rvsc;		/* vertical syncronous period (RV_MARGINS) */
	uint32_t rvbp;		/* vertical back porch (RV_MARGINS) */

	uint32_t rhip;		/* horiz black-level (RBLACK)  */
	uint32_t rvip;		/* vertical black-level (RBLACK) */

	uint32_t rck;		/* set freq division rate (RCLOCK) */
	uint32_t rddl;		/* delay DMA request signal (RCLOCK) */
};


/** LCD **/

#ifdef CONFIG_MB93093_PDK
static struct mb93493_vmode lcd_320_242_rgb = {
	320, 242, 320, 242,	// pix_x, pix_y, pix_xv, pix_yv
        0, 0, 4,	        // pix_xoff, pix_yoff, pix_sz
	DMAC_CCFRx_CM_2D,	// dma_mode
	2,			// dma_ats
	DMAC_CCFRx_RS_EXTERN,	// dma_rs 
	0,		// hls
	0,		// pal
	0,		// cscv
	0,		// dbls
	0,		// r601
	0,		// tfop
	1,		// dsm
 	1,		// dfp
	1,		// die
	0,		// enop
	0,		// vsop
	0,		// hsop
	0,		// dsr
	0,		// csron
	1,		// dpf (RGB)
	3,		// dms (frame xfer state)
	0,		// rhtc (calculated)
	320,		// rhdc
	1,		// rhfp
	1,		// rhsc
	1,		// rhbp
	0,		// rvtc (calculated)
	242,		// rvdc
	1,		// rvfp
	1,		// rvsc
	1,		// rvbp
	0,		// rhip
	0,		// rvip
	6,		// rck
	2		// rddl
};
#endif

/** VGA **/

static struct mb93493_vmode vga_640_480_rgb = {
	// fH 35.4 kHz
	// fV 67.5 Hz
	640, 480, 640, 480,	// pix_x, pix_y, pix_xv, pix_yv
        0, 0, 3,		// pix_xoff, pix_yoff, pix_sz
	DMAC_CCFRx_CM_2D,	// dma_mode
	2,			// dma_ats
	DMAC_CCFRx_RS_EXTERN,	// dma_rs 
	0,		// hls
	0,		// pal
	0,		// cscv
	0,		// dbls
	0,		// r601
	0,		// tfop
	0,		// dsm
 	0,		// dfp
	1,		// die
	0,		// enop
	0,		// vsop
	0,		// hsop
	0,		// dsr
	0,		// csron
	1,		// dpf (RGB)
	3,		// dms (frame xfer state)
	0,		// rhtc (calculated)
	640,		// rhdc
	18,		// rhfp
	48,		// rhsc
	56,		// rhbp
	0,		// rvtc (calculated)
	480,		// rvdc
	8,		// rvfp
	4,		// rvsc
	33,		// rvbp
	0,		// rhip
	0,		// rvip
	2,		// rck
	1		// rddl
};

static struct mb93493_vmode vga_800_600_rgb = {
	// fH 30.1 kHz
	// fV 48.8 Hz
	800, 600, 800, 600,	// pix_x, pix_y, pix_xv, pix_yv
	0, 0, 3,		// pix_xoff, pix_yoff, pix_sz
	DMAC_CCFRx_CM_2D,	// dma_mode
	2,			// dma_ats
	DMAC_CCFRx_RS_EXTERN,	// dma_rs 
	0,		// hls
	0,		// pal
	0,		// cscv
	0,		// dbls
	0,		// r601
	0,		// tfop
	0,		// dsm
 	0,		// dfp
	1,		// die
	0,		// enop
	0,		// vsop
	0,		// hsop
	0,		// dsr
	0,		// csron
	1,		// dpf (RGB)
	3,		// dms (frame xfer state)
	0,		// rhtc (calculated)
	800,		// rhdc
	13,		// rhfp
	39,		// rhsc
	44,		// rhbp
	0,		// rvtc (calculated)
	600,		// rvdc
	6,		// rvfp
	3,		// rvsc
	9,		// rvbp
	0,		// rhip
	0,		// rvip
	2,		// rck
	1		// rddl
};

/** NTSC **/

static struct mb93493_vmode ntsc_688_480 = {
	// fH 15.72 kHz
	// fV 59.98 Hz  
	688, 480, 688, 480,	// pix_x, pix_y, pix_xv, pix_yv
	0, 0, 2,		// pix_xoff, pix_yoff, pix_sz
	DMAC_CCFRx_CM_2D,	// dma_mode
	2,			// dma_ats
	DMAC_CCFRx_RS_EXTERN,	// dma_rs 
	0,		// hls
	0,		// pal
	0,		// cscv
	0,		// dbls
	0,		// r601
	0,		// tfop
	1,		// dsm (interlace)
	1,		// dfp (one pixel xfrd as one word)
	1,		// die
	0,		// enop
	1,		// vsop (VSYNC active high)
	1,		// hsop (HSYNC active high)
	0,		// dsr
	0,		// csron
	0,		// dpf YCbCr 4:2:2, 16-bit
	3,		// dms (frame xfer state)
	0,		// rhtc (calculated)
	688,		// rhdc [720]
	3,		// rhfp [1]
	19,		// rhsc [67]
	148,		// rhbp [60]
	0,		// rvtc (calculated)
	NTSC_RVDC,	// rvdc
	2,		// rvfp
	19,		// rvsc
	1,		// rvbp
	0,		// rhip
	0,		// rvip
	4,		// rck  (54 / 4 --> 13.5 MHz)
	1		// rddl
};

static struct mb93493_vmode ntsc_720_480 = {
	// fH 15.73 kHz
	// fV 60.06 Hz  
	720, 480, 720, 480,	// pix_x, pix_y, pix_xv, pix_yv
	0, 0, 2,		// pix_xoff, pix_yoff, pix_sz
	DMAC_CCFRx_CM_2D,	// dma_mode
	2,			// dma_ats
	DMAC_CCFRx_RS_EXTERN,	// dma_rs 
	0,		// hls
	0,		// pal
	0,		// cscv
	1,		// dbls
	1,		// r601
	0,		// tfop
	1,		// dsm (interlace)
	1,		// dfp (one pixel xfrd as one word)
	1,		// die
	0,		// enop
	0,		// vsop
	0,		// hsop
	0,		// dsr
	0,		// csron
	0,		// dpf YCbCr 4:2:2, 16-bit
	3,		// dms (frame xfer state)
	0,		// rhtc (calculated)
	720,		// rhdc
	4,		// rhfp
	4,		// rhsc
	130,		// rhbp
	0,		// rvtc (calculated)
	NTSC_RVDC,	// rvdc
	2,		// rvfp
	19,		// rvsc
	1,		// rvbp
	0,		// rhip
	0,		// rvip
	4,		// rck  (54 / 4 --> 13.5 MHz)
	1		// rddl
};


static struct mb93493_vmode *active_vmode;
static int vdc_dma_channel;
static int vdc_in_blank;
static int vdc_intr_cnt_vsync;
static int vdc_intr_cnt_under;

/*****************************************************************************/
/*
 * video mode tables
 * - clock rate
 *	DOTCLOCK = 1/(PIXCLK*1e-12) [result in Hz]
 *
 * - format
 *	NAME  REFRESH  XRES  YRES  PIXCLK  LEFT-M  RIGHT-M
 *	UPPER-M  LOWER-M  HSYNC  VSYNC  SYNCFLAGS  VMODEFLAGS
 */

/** VGA **/

static struct fb_videomode mb93493fb_vga_modedb[] = {
	{	/* 640x480 @ 60Hz */
		"640x480@60",
		60, 640, 480, 39721, 18, 56, 8, 4, 48, 33, 0,
		FB_VMODE_NONINTERLACED
	},
	{	/* 800x600 @ 48Hz */
		"800x600@48",
		48, 800, 600, 39721, 18, 56, 8, 4, 48, 33, 0,
		FB_VMODE_NONINTERLACED
	}
};

static int mb93493fb_vga_modedb_size =
	(sizeof(mb93493fb_vga_modedb) / sizeof(mb93493fb_vga_modedb[0]));

/** PAL **/

static struct fb_videomode mb93493fb_pal_modedb[] = {
	{	/* PAL 720x576 @ 50Hz [816x625] */
		"720x576@50",
		50, 720, 576, 39214, 24, 16, 44, 1, 56, 4, 0,
		FB_VMODE_INTERLACED
	}
};

static int mb93493fb_pal_modedb_size =
	(sizeof(mb93493fb_pal_modedb) / sizeof(mb93493fb_pal_modedb[0]));

/** NTSC **/

static struct fb_videomode mb93493fb_ntsc_modedb[] = {
	{	/* 688x480 @ 60Hz */
		"688x480@60",
		60, 688, 480, 39721, 56, 16, 33, 10, 88, 2,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_INTERLACED
	},
	{	/* 720x480 @ 60Hz */
		"720x480@60",
		60, 720, 480, 39721, 56, 16, 33, 10, 88, 2,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_INTERLACED
	}
};

static int mb93493fb_ntsc_modedb_size =
	(sizeof(mb93493fb_ntsc_modedb) / sizeof(mb93493fb_ntsc_modedb[0]));

/** LCD **/

#ifdef CONFIG_MB93093_PDK
static struct fb_videomode mb93493fb_lcd_modedb[] = {
	{
		"320x242",
		60, 320, 242, 17777, 1, 1, 1, 1, 1, 1, 0,
		FB_VMODE_NONINTERLACED
	}
};

static int mb93493fb_lcd_modedb_size =
	(sizeof(mb93493fb_lcd_modedb) / sizeof(mb93493fb_lcd_modedb[0]));
#endif

#ifndef NO_MM
static struct page *fb_pg;
static unsigned char *fb_va;
static unsigned long fb_phys;
static unsigned long fb_order;
#endif

static unsigned long fb_size;
static struct mb93493fb_info fb_info;
static struct mb93493fb_par current_par;
static int current_par_valid = 0;
static struct display disp;
static char mode_string[MODE_STR_SZ] = { 0 };
static mb93493fb_display_type display_type = DISPLAY_NOT_SET;
static char display_string[MODE_STR_SZ/2] = { 0 };

#ifdef MODULE
MODULE_PARM(mode_string, "s");
#endif

/* Function prototypes */
static void mb93493fb_detect(void);
static int mb93493fb_encode_fix(struct fb_fix_screeninfo *, const void *,
    struct fb_info_gen *);
static int mb93493fb_decode_var(const struct fb_var_screeninfo *, void *,
    struct fb_info_gen *);
static int mb93493fb_encode_var(struct fb_var_screeninfo *, const void *,
    struct fb_info_gen *);
static void mb93493fb_get_par(void *, struct fb_info_gen *);
static void mb93493fb_set_par(const void *, struct fb_info_gen *);
static int mb93493fb_getcolreg(unsigned, unsigned *, unsigned *, unsigned *,
    unsigned *, struct fb_info *);
static int mb93493fb_setcolreg(unsigned, unsigned, unsigned, unsigned, unsigned,
    struct fb_info *);
static int mb93493fb_pan_display(const struct fb_var_screeninfo *,
    struct fb_info_gen *);
static int mb93493fb_blank(int, struct fb_info_gen *);
static void mb93493fb_set_disp(const void *, struct display *,
    struct fb_info_gen *);
#ifdef MODULE
static int mb93493fb_setup(char *options);
#endif
static int mb93493fb_open(struct fb_info *, int);
static int mb93493fb_release(struct fb_info *, int);

#ifdef NO_MM
static unsigned long
mb93493_get_fb_unmapped_area(struct file *file,
		     unsigned long addr, unsigned long len,
		     unsigned long pgoff, unsigned long flags);
#endif

/* interface to hardware functions */
struct fbgen_hwswitch mb93493fb_switch = {
	mb93493fb_detect,
	mb93493fb_encode_fix,
	mb93493fb_decode_var,
	mb93493fb_encode_var,
	mb93493fb_get_par,
	mb93493fb_set_par,
	mb93493fb_getcolreg,
	mb93493fb_setcolreg,
	mb93493fb_pan_display,
	mb93493fb_blank,
	mb93493fb_set_disp
};

/* frame buffer operations */
static struct fb_ops mb93493fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = mb93493fb_open,
	.fb_release = mb93493fb_release,
	.fb_get_fix = fbgen_get_fix,
	.fb_get_var = fbgen_get_var,
	.fb_set_var = fbgen_set_var,
	.fb_get_cmap = fbgen_get_cmap,
	.fb_set_cmap = fbgen_set_cmap,
	.fb_pan_display = fbgen_pan_display,
#ifdef NO_MM
	.get_fb_unmapped_area = mb93493_get_fb_unmapped_area,
#endif
	//.fb_ioctl     = mb93493fb_ioctl,      /* optional */
};



#ifdef DEBUG_PRINTS
static void print_reg(char *regname, unsigned int v)
{
	printk("%12s  0x%08x\n", regname, v);
}
#endif

/*
 * write VDC registers with given bitfield parameters
 */
static void write_vdc_registers(struct mb93493_vmode *p)
{
	kdebug("%s\n", __FUNCTION__);

	/* calculate rhtc, rvtc total counts */
	p->rhtc = p->rhfp + p->rhsc + p->rhbp + p->rhdc + p->rhip * 2;
	p->rvtc = p->rvfp + p->rvsc + p->rvbp + p->rvdc + p->rvip * 2;
	kdebug("rhdc=%d/rhtc=%d, rvdc=%d/rvtc=%d\n", p->rhdc, p->rhtc,
	       p->rvdc, p->rvtc);

#ifdef DEBUG_PRINTS
	print_reg("RHDC", (p->rhtc << 16) | p->rhdc);
	print_reg("RH_MARGINS", (p->rhfp << 24) | (p->rhsc << 16) | p->rhbp);
	print_reg("RVDC", (p->rvtc << 16) | p->rvdc);
	print_reg("RV_MARGINS", (p->rvfp << 24) | (p->rvsc << 16) | (p->rvbp << 8));
	print_reg("RBLACK", (p->rhip << 24) | (p->rvip << 16));
	print_reg("RCLOCK", (p->rck << 24) | (p->rddl << 16));

	print_reg("RC",
	    (p->hls << 31) | (p->pal << 22) | (p->cscv << 21) |
	    (p->dbls << 20) | (p->r601 << 19) | (p->tfop << 16) |
	    (p->dsm << 14) | (p->dfp << 12) | (p->die << 11) |
	    (p->enop << 10) | (p->vsop << 9) | (p->hsop << 8) |
	    (p->dsr << 7) | (p->csron << 4) | (p->dpf << 2) | p->dms);

#endif
	__set_MB93493_VDC(RHDC, (p->rhtc << 16) | p->rhdc);
	__set_MB93493_VDC(RH_MARGINS, (p->rhfp << 24) | (p->rhsc << 16) | p->rhbp);
	__set_MB93493_VDC(RVDC, (p->rvtc << 16) | p->rvdc);
	__set_MB93493_VDC(RV_MARGINS, (p->rvfp << 24) | (p->rvsc << 16) | (p->rvbp << 8));
	__set_MB93493_VDC(RBLACK, (p->rhip << 24) | (p->rvip << 16));
	__set_MB93493_VDC(RCLOCK, (p->rck << 24) | (p->rddl << 16));

	__set_MB93493_VDC(RC,
	    (p->hls << 31) | (p->pal << 22) | (p->cscv << 21) |
	    (p->dbls << 20) | (p->r601 << 19) | (p->tfop << 16) |
	    (p->dsm << 14) | (p->dfp << 12) | (p->die << 11) |
	    (p->enop << 10) | (p->vsop << 9) | (p->hsop << 8) |
	    (p->dsr << 7) | (p->csron << 4) | (p->dpf << 2) | p->dms);
}

/*
 * initiate DMA transfer
 */
static void mb93493fb_dma_set_2d(struct mb93493_vmode *p)
{
	unsigned long six = 0, bcl = 0, apr = 0, ccfr, cctr;
	uint32_t addr, pix_xv, xoff, yoff;

#ifdef NO_MM
	addr = (uint32_t)fb_info.framebuffer;
#else
	addr = (uint32_t)fb_phys;
#endif

	if (current_par_valid) {
		pix_xv = current_par.vxres;
		xoff = current_par.xoffset;
		yoff = current_par.yoffset;
	} else {
		pix_xv = p->pix_x;
		xoff = yoff = 0;
	}

	switch (p->dma_mode) {
	case DMAC_CCFRx_CM_2D:
		six = p->pix_y;
		bcl = p->pix_x * p->pix_sz;
		apr = pix_xv * p->pix_sz;
		if (p->dsm) {
			six >>= 1;
			apr = apr * 2;
			if(fb_info.dma_xfer_btmfld)
				addr += bcl;
			fb_info.dma_xfer_btmfld = !fb_info.dma_xfer_btmfld;
		} 
		addr += ((yoff * apr) + (xoff * p->pix_sz));
		break;

	case DMAC_CCFRx_CM_SCA:
		six = 0xffffffff;
		bcl = p->pix_x * p->pix_y * p->pix_sz;
		break;

	case DMAC_CCFRx_CM_DA:
		bcl = p->pix_x * p->pix_y * p->pix_sz;
		break;

	default:
		printk(KERN_ERR "%s: unsupported DMA mode %d\n", __FUNCTION__, p->dma_mode);
		break;
	}

	cctr = DMAC_CCTRx_IE | DMAC_CCTRx_SAU_INC | DMAC_CCTRx_DAU_HOLD |
		DMAC_CCTRx_SSIZ_32 | DMAC_CCTRx_DSIZ_32;

	ccfr = p->dma_mode | (p->dma_ats << DMAC_CCFRx_ATS_SHIFT) | p->dma_rs;

	frv_dma_config(vdc_dma_channel, ccfr, cctr, apr);

	frv_dma_start(vdc_dma_channel, addr,
		      __addr_MB93493_VDC_TPO(0), 0, six, bcl);
}

static int is_digit(char ch)
{
	if (ch >= '0' && ch <= '9')
		return 1;
	return 0;
}

/*****************************************************************************/
/*
 * determine the current video mode settings and store them as the default video mode
 * - can't do this from hardware - almost all control regs are write-only
 */
static void mb93493fb_detect(void)
{
	//struct mb93493fb_par par;

	ktrace("Entering %s\n", __FUNCTION__);

	//mb93493fb_get_par(&par, &fb_info.gen);
	//mb93493fb_encode_var(&mb93493fb_initial_var, &par, &fb_info.gen);

	ktrace("Leaving %s\n", __FUNCTION__);
}				/* end mb93493fb_detect() */

/*****************************************************************************/
/*
 * fill in the fixed-parameter record based on the values in the "par" structure
 */
static int mb93493fb_encode_fix(struct fb_fix_screeninfo *fix,
				const void *_par, struct fb_info_gen *_info)
{
	const struct mb93493fb_par *par = _par;
	struct mb93493fb_info *info = container_of(_info, struct mb93493fb_info, gen);

	ktrace("Entering %s\n", __FUNCTION__);

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, "Fujitsu MB93493");

	fix->smem_start = (unsigned long) virt_to_phys(info->framebuffer);
	fix->smem_len = fb_size;
	fix->mmio_start = (unsigned long) info->regs;
	fix->mmio_len = 0x1000;
	fix->line_length = par->pitch;

	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	fix->visual = FB_VISUAL_TRUECOLOR;

	// xpanstep needs to be multiple of 32 bytes for DMA
	switch (par->bpp) {
	case 16:
	    fix->xpanstep = 16;
	    break;
	case 24:
	    fix->xpanstep = 32;
	    break;
	case 32:
	    fix->xpanstep = 8;
	    break;
	}
	fix->ypanstep = 1;
	fix->ywrapstep = 0;
	fix->accel = 0;

	ktrace("Leaving %s\n", __FUNCTION__);
	return 0;
}	/* end mb93493fb_encode_fix() */

/*****************************************************************************/
/*
 * get the video params out of 'var'
 * - if a value doesn't fit, round it up
 * - if it's too big, return -EINVAL.
 *
 *  Suggestion: Round up in the following order: bits_per_pixel, xres,
 *		yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *		bitfields, horizontal timing, vertical timing.
 */
static int mb93493fb_decode_var(const struct fb_var_screeninfo *var,
				void *_par, struct fb_info_gen *_info)
{
	struct mb93493fb_par *par = _par;
	uint32_t rc = 0;
	uint32_t rhdc, rvdc, rh_margins, rv_margins, pitch;
	int xres, vxres, vyres, pix_sz, xoffset;

	ktrace("Entering %s [var bits_per_pixel set to %d]\n", __FUNCTION__, var->bits_per_pixel);

	/* sanity check */
	switch (var->bits_per_pixel) {
	case 16:
		pix_sz = 2;
		break;
	case 24:
		pix_sz = 3;
		break;
	case 32:
		pix_sz = 4;
		if (active_vmode->pix_sz == 3) {
			kdebug("! var bpp is 32 (should be 24)\n");
		}
		break;
	default: printk("%s: BPP %d not supported\n",
		    __FUNCTION__, var->bits_per_pixel);
		return -EINVAL;
	}

	/* work out the line size
	 * - need to round up for DMA'ing in 32b chunks
	 */
	xres = (((var->xres * pix_sz) + 31) & ~31) / pix_sz;
	xoffset = (((var->xoffset * pix_sz) + 31) & ~31) / pix_sz;

	if (xres < 320 || xres > 1920 || (xres & 7) ||
	    var->yres < 240 || var->yres > 1200) {
		printk("%s: resolution %dx%d not supported\n",
		    __FUNCTION__, var->xres, var->yres);
		return -EINVAL;
	}

	rhdc = xres;
	rvdc = var->yres;

	vxres = var->xres_virtual;
	vyres = var->yres_virtual;

	if (vxres < xres)
		vxres = xres;
	else
		vxres = (((vxres * pix_sz) + 31) & ~31) / pix_sz;

	pitch = vxres * pix_sz;

	if (vyres < var->yres)
		vyres = var->yres;

	if (vyres * pitch > fb_size) {
		printk("%s: res %dx%dx%d too big for allocated fb of %lu bytes\n",
		    __FUNCTION__, vxres, vyres, var->bits_per_pixel, fb_size);
		return -EINVAL;
	}
	if (var->yoffset + var->yres > vyres)
		return -EINVAL;
	if (var->xoffset + xres > vxres)
		return -EINVAL;

	if (var->left_margin < 1 || var->left_margin > 127 ||
	    var->right_margin < 1 || var->right_margin > 312 ||
	    var->hsync_len < 1 || var->hsync_len > 192) {
		printk("%s: Horizontal margins out of range\n", __FUNCTION__);
		return -EINVAL;
	}

	rh_margins = (var->left_margin << 24) | (var->hsync_len << 16) | var->right_margin;

	if (var->upper_margin < 1 || var->upper_margin > 37 ||
	    var->lower_margin < 1 || var->lower_margin > 25 ||
	    var->vsync_len < 1 || var->vsync_len > 60) {
		printk("%s: Vertical margins out of range\n", __FUNCTION__);
		return -EINVAL;
	}

	rv_margins = var->upper_margin << 24 | var->vsync_len << 16 | var->lower_margin << 8;

	if (var->sync & FB_SYNC_HOR_HIGH_ACT)
		rc |= (1 << 8);	/* RC.HSOP */

	if (var->sync & FB_SYNC_VERT_HIGH_ACT)
		rc |= (1 << 9);	/* RC.VSOP */

	/* fill out the hardware parameter form */
	memset(par, 0, sizeof(*par));
	par->bpp = var->bits_per_pixel;

	par->rhdc = rhdc;
	par->rh_margins = rh_margins;
	par->rvdc = rvdc;
	par->rv_margins = rv_margins;

	par->vxres = vxres;
	par->vyres = vyres;

	par->xoffset = xoffset;
	par->yoffset = var->yoffset;

	if (var->bits_per_pixel >= 24)
		rc |= (1 << 2);  /* RC.DPF = 01, RGB (4.4.4) */
	//kdebug("setting rc to 0x%x\n", rc);
	par->rc = rc;

	par->rblack = 0;	/* RHIP and RVIP */
	par->pitch = pitch;
	par->rclock = 0x01010000;
	par->rblack = 0;

#if 0
	/* pixclock in picosecs, htotal in pixels, vtotal in scanlines */
	if (!fbmon_valid_timings(par->pixclock, par->htotal, par->vtotal,
		&info->gen.info)) {
		ktrace("Leaving %s, error EINVAL\n", __FUNCTION__);
		return -EINVAL;
	}
#endif

	ktrace("Leaving %s\n", __FUNCTION__);
	return 0;
}	/* end mb93493fb_decode_var() */

/*****************************************************************************/
/*
 * store the video parameters into the variable part of the FB description
 */
static int mb93493fb_encode_var(struct fb_var_screeninfo *var,
				const void *_par, struct fb_info_gen *info)
{
	const struct mb93493fb_par *par = _par;

	ktrace("Entering %s\n", __FUNCTION__);

	memset(var, 0, sizeof(*var));

	var->xres = par->rhdc;
	var->yres = par->rvdc;
	var->xres_virtual = par->vxres;
	var->yres_virtual = par->vyres;
	var->xoffset = par->xoffset;
	var->yoffset = par->yoffset;
	var->bits_per_pixel = par->bpp;

	switch (var->bits_per_pixel) {
	case 16:
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		break;
	case 24:
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		break;
	case 32:
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		break;
	}

	var->height = -1;
	var->width = -1;
	var->accel_flags = 0;

	var->left_margin = (par->rh_margins >> 24) & 0x07f;
	var->right_margin = (par->rh_margins >> 0) & 0x1ff;
	var->hsync_len = (par->rh_margins >> 16) & 0x0ff;
	var->upper_margin = (par->rv_margins >> 24) & 0x03f;
	var->lower_margin = (par->rv_margins >> 8) & 0x03f;
	var->vsync_len = (par->rv_margins >> 16) & 0x01f;

	var->sync = 0;
	if (par->rc & (1 << 8))	/* RC.HSOP */
		var->sync |= FB_SYNC_HOR_HIGH_ACT;

	if (par->rc & (1 << 9))	/* RC.VSOP */
		var->sync |= FB_SYNC_VERT_HIGH_ACT;

	var->vmode = 0;
	if (par->rc & (1 << 14)) /* RC.DSM */
		var->vmode |= FB_VMODE_MASK;

	ktrace("Leaving %s\n", __FUNCTION__);

	return 0;
}				/* end mb93493fb_encode_var() */

/*****************************************************************************/
/*
 * get the current video mode from the hardware and place in the parameter structure
 */
static void mb93493fb_get_par(void *_par, struct fb_info_gen *info)
{
	struct mb93493fb_par *par = _par;

	ktrace("Entering ** %s\n", __FUNCTION__);

	/* don't bother querying the hardware if we've already got a snapshot */
	if (current_par_valid) {
		*par = current_par;
		goto done;
	}

	memset(par, 0, sizeof(struct mb93493fb_par));
	par->rcursor = 0;
	par->rct1 = 0;
	par->rct2 = 0;
	par->rhdc = par->vxres = 0x180;
	par->rh_margins = 0x10300030;
	par->rvdc = par->vyres = 0x1e0;
	par->rv_margins = 0x0a022100;
	par->rc = 0x00000000;
	par->rclock = 0x01010000;
	par->rblack = 0x00000000;
	par->rs = __get_MB93493_VDC(RS);

done:
	ktrace("Leaving %s\n", __FUNCTION__);
}	/* end mb93493fb_get_par() */



#ifdef RECT_TEST

static void RGB_to_YCbCr(uint32_t color, uint8_t *Y, uint8_t *Cb, uint8_t *Cr)
{
	uint32_t red = (color >> 16) & 0xff;
	uint32_t green = (color >> 8) & 0xff;
	uint32_t blue = color & 0xff;

	*Y = (uint8_t) (((8432 * red) / 32768) + ((16425 * green) / 32768) +
		  ((3176 * blue) / 32768)) + 16;

	*Cb = (uint8_t) (((4818 * red) / 32768) - ((9527 * green) / 32768) +
		   ((14345 * blue) / 32768)) + 128;

	*Cr = (uint8_t) (((14345 * red) / 32768) - ((12045 * green) / 32768) -
		   ((2300 * blue) / 32768)) + 128;
}

static void rect(struct mb93493fb_par *par, uint8_t *base, int xlo, int ylo,
		 int xhi, int yhi, uint32_t color)
{
	int w = xhi - xlo + 1;
	int j, h = yhi - ylo + 1;

	kdebug("%s: %dbpp: %d,%d w=%d, h=%d (color=0x%08x)\n",
	       __FUNCTION__, par->bpp, xlo, ylo, w, h, color);

	if (par->bpp == 16) {
		uint32_t cnt, pitch = par->pitch / 2;
		uint16_t *dst, *scan = (uint16_t *) (base + (ylo * par->pitch));
		uint16_t w0, w1;
		uint8_t Y, Cb, Cr;

		RGB_to_YCbCr(color, &Y, &Cb, &Cr);
		kdebug("color=0x%04x  Y=0x%02x Cb=0x%02x Cr=0x%02x\n",
			color, (uint32_t)Y, (uint32_t)Cb, (uint32_t)Cr);
		w0 = ((uint16_t)Y) | ((uint16_t)Cb << 8);
		w1 = ((uint16_t)Y) | ((uint16_t)Cr << 8);

		scan += xlo;
		while(h--) {
			dst = scan;
			j = w;
			cnt = 0;
			while(j--) {
				if ((cnt & 1) == 0)
					*dst++ = w0;
				else
					*dst++ = w1;
				cnt++;
			}
			scan += pitch;
		}
	} else if (par->bpp == 24) {
		uint8_t *dst, *scan = base + (ylo * par->pitch);
		uint8_t red = (uint8_t) color >> 16;
		uint8_t green = (uint8_t) (color >> 8) & 0xff;
		uint8_t blue = (uint8_t) color & 0xff;
		scan += xlo * 3;

		while(h--) {
			dst = scan;
			j = w;
			while(j--) {
				*dst++ = red;
				*dst++ = green;
				*dst++ = blue;
			}
			scan += par->pitch;
		}
	} else if (par->bpp == 32) {
		uint32_t *dst, *scan = (uint32_t *) (base + (ylo * par->pitch));
		uint32_t pitch = par->pitch / 4;
		scan += xlo;

		while(h--) {
			dst = scan;
			j = w;
			while(j--) {
				*dst++ = color;
			}
			scan += pitch;
		}
	} else {
		printk("%s: unsupport bpp=%d\n", __FUNCTION__, par->bpp);
	}

}	/* rect */
#endif


/*****************************************************************************/
/*
 * set the hardware according to the supplied parameters
 */
static void mb93493fb_set_par(const void *_par, struct fb_info_gen *_info)
{
	const struct mb93493fb_par *par = _par;
	struct mb93493fb_info *info = (struct mb93493fb_info *) _info;

	ktrace("Entering %s\n", __FUNCTION__);
	kdebug("setting current par with: pitch=%d rhdc=%d rvdc=%d bpp=%d\n",
	       par->pitch, par->rhdc, par->rvdc, par->bpp);

	current_par = *par;
	current_par_valid = 1;
	write_vdc_registers(active_vmode);

	/* clear the framebuffer */
	memset(info->framebuffer, 0, par->pitch * par->rvdc);

#ifdef RECT_TEST
	rect((struct mb93493fb_par *)par, info->framebuffer,  100, 13,  199,  200, 0xff0000);   /* red */
	rect((struct mb93493fb_par *)par, info->framebuffer,  500,  7,  599,  106, 0x007700);   /* med green */
#endif

	ktrace("Leaving %s\n", __FUNCTION__);

}	/* end mb93493fb_set_par() */

/*****************************************************************************/
/*
 * read a single colour register and split it into colours and transparency
 * - the return values must have a 16 bit magnitude
 * - the bottom 16 entries in the colourmap must be maintained
 *   even in truecolour (console palette)
 * - return != 0 for invalid regno
 */
static int mb93493fb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			       unsigned *blue, unsigned *transp, 
			       struct fb_info *_info)
{
	struct mb93493fb_info *info = (struct mb93493fb_info *) _info;
	uint32_t colour;

	if (regno > 15)
		return 1;

	colour = info->fbcon_cmap[regno];

	switch (current_par.bpp) {
#ifdef FBCON_HAS_CFB16
	/* pretend we have a RGB:565 colour system, when actually it's YCbCr:8888 */
	case 16:
		*red = (regno == 0) ? 0x0000 : 0xffff;
		*green = (regno == 0) ? 0x0000 : 0xffff;
		*blue = (regno == 0) ? 0x0000 : 0xffff;
		*transp = 0;
		return 0;
#endif

#if defined(FBCON_HAS_CFB24) || defined(FBCON_HAS_CFB32)
	case 24:
	case 32:
		*red = (colour >> 8) & 0xff00;
		*green = (colour) & 0xff00;
		*blue = (colour << 8) & 0xff00;
		*transp = 0;
		return 0;
#endif
	}

	return 1;
}	/* end mb93493fb_getcolreg() */

/*****************************************************************************/
/*
 * set a single colour register
 * - the bottom 16 entries in the colourmap must be maintained even
 *   in truecolour (console palette)
 * - the values supplied have a 16 bit magnitude
 * - return != 0 for invalid regno
 */
static int mb93493fb_setcolreg(unsigned regno, unsigned red, unsigned green,
			       unsigned blue, unsigned transp, struct fb_info *_info)
{
#if defined(FBCON_HAS_CFB24) || defined(FBCON_HAS_CFB32)
	struct mb93493fb_info *info = (struct mb93493fb_info *) _info;
#endif

	if (regno > 15)
		return 1;

	switch (current_par.bpp) {
#ifdef FBCON_HAS_CFB16
	/* can't change the YCbCr palette */
	case 16:
		return 0;
#endif

#if defined(FBCON_HAS_CFB24) || defined(FBCON_HAS_CFB32)
	case 24:
	case 32:
		info->fbcon_cmap[regno] = ((red & 0xff00) << 8) |
			(green & 0xff00) | ((blue & 0xff00) >> 8);
		return 0;
#endif
	}

	return 1;
}	/* end mb93493fb_setcolreg() */

/*****************************************************************************/
/*
 *  pan (or wrap, depending on the `vmode' field) the display using the
 *  `xoffset' and `yoffset' fields of the `var' structure
 *  - if the values don't fit, return -EINVAL.
 */
static int mb93493fb_pan_display(const struct fb_var_screeninfo *var,
				 struct fb_info_gen *info)
{
	struct mb93493fb_par *par = &current_par;

	if (!current_par_valid)
		return -EINVAL;

	par->xoffset = var->xoffset;
	par->yoffset = var->yoffset;

	return 0;
}	/* end mb93493fb_pan_display() */


/*****************************************************************************/
/*
 * blank the screen if blank_mode != 0, else unblank
 * - if no blank method, then the caller blanks by setting the CLUT (Color Look Up Table)
 *   to all black
 * - return 0 if blanking succeeded, != 0 if un-/blanking failed due
 *   to e.g. a video mode which doesn't support it
 * - implements VESA suspend and powerdown modes on hardware that supports disabling hsync/vsync:
 *   - blank_mode == 2: suspend vsync
 *   - blank_mode == 3: suspend hsync
 *   - blank_mode == 4: powerdown
 */
static int mb93493fb_blank(int blank_mode, struct fb_info_gen *info)
{
	struct mb93493_vmode *p = active_vmode;

	ktrace("\n%s: ", __FUNCTION__);

	switch (blank_mode) {
	case VESA_VSYNC_SUSPEND:
		p->dms = 1;
		kdebug("blank_mode: VESA_VSYNC_SUSPEND\n");
		break;

	case VESA_HSYNC_SUSPEND:
		p->dms = 1;
		kdebug("blank_mode: VESA_HSYNC_SUSPEND\n");
		break;

	case VESA_POWERDOWN:
		p->dms = 0;
		kdebug("blank_mode: VESA_POWERDOWN\n");
		break;

	case VESA_NO_BLANKING:
		p->dms = 3;
		kdebug("blank_mode: frame transfer state (VESA_NO_BLANKING)\n");
		break;

	default:
		kdebug("blank_mode: default\n");
		return 0;
	}
#if 0
	__set_MB93493_VDC(RC,
	    (p->hls << 31) | (p->pal << 22) | (p->cscv << 21) |
	    (p->dbls << 20) | (p->r601 << 19) | (p->tfop << 16) |
	    (p->dsm << 14) | (p->dfp << 12) | (p->die << 11) |
	    (p->enop << 10) | (p->vsop << 9) | (p->hsop << 8) |
	    (p->dsr << 7) | (p->csron << 4) | (p->dpf << 2) | p->dms);

	current_par.rs = __get_MB93493_VDC(RS);
#endif

	return 0;
}	/* end mb93493fb_blank() */

/*****************************************************************************/
/*
 * fill in a pointer with the virtual address of the mapped frame buffer
 * also fill in a pointer to appropriate low level text console operations
 * (and optionally a pointer to help data) for the video mode `par' of your
 * video hardware
 * - these can be generic software routines, or hardware accelerated routines
 *   specifically tailored for your hardware
 * - if you don't have any appropriate operations, you must fill in a
 *   pointer to dummy operations, and there will be no text output
 */
static void mb93493fb_set_disp(const void *par, struct display *disp,
			       struct fb_info_gen *_info)
{
	struct mb93493fb_info *info = (struct mb93493fb_info *) _info;

	ktrace("Entering %s\n", __FUNCTION__);

	disp->screen_base = info->gen.info.screen_base;

	kdebug("set_disp -- bpp=%d\n", current_par.bpp);

	switch (current_par.bpp) {
#ifdef FBCON_HAS_CFB16
	case 16:
		disp->dispsw = &fbcon_cfb16;
		disp->dispsw_data = &fbcon_cmap_YCbCr;	/* b/w YCbCr console palette */
		kdebug("FBCON_HAS_CFB16\n");
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		disp->dispsw = &fbcon_cfb24;
		disp->dispsw_data = &info->fbcon_cmap;	/* console palette */
		kdebug("FBCON_HAS_CFB24\n");
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		disp->dispsw = &fbcon_cfb32;
		disp->dispsw_data = &info->fbcon_cmap;	/* console palette */
		kdebug("FBCON_HAS_CFB32\n");
		break;
#endif
	default:
		disp->dispsw = &fbcon_dummy;
		printk("unsupported BPP (%d) in MB93493 FB\n", current_par.bpp);
		break;
	}

	ktrace("Leaving %s\n", __FUNCTION__);
}	/* end mb93493fb_set_disp() */

/*****************************************************************************/
/*
 * handle DMA-end interrupts for frame data shovelling
 */

static void mb93493_dma_interrupt(int dmachan, unsigned long cstr,
				  void *data, struct pt_regs *regs)
{
	if(vdc_in_blank)
		return;

	mb93493fb_dma_set_2d(active_vmode);
}	/* end mb93493_dma_interrupt() */

/*****************************************************************************/
/*
 * handle VSYNC and VDC error interrupts
 */
//static int icount = 0;



static void mb93493_vdc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	uint32_t rs;
	int vsync, underflow, field;

	ktrace("%s\n", __FUNCTION__);
	
	rs = __get_MB93493_VDC(RS);

	vsync = (rs >> 17) & 1;
	underflow = (rs >> 18) & 1;
	field = (rs >> 16) & 1;

	if (vsync) {
		vdc_intr_cnt_vsync++;
	}

	if (underflow) {
		vdc_intr_cnt_under++;

		frv_dma_stop(vdc_dma_channel);

		mb93493fb_dma_set_2d(active_vmode);
		if (active_vmode->dpf == 1) {
			vdc_in_blank = 1;
		}
		else if (field == 0) {
			vdc_in_blank = 1;
		}
	}

	if (vdc_in_blank) {
		if (active_vmode->dpf == 1) {
			mb93493fb_dma_set_2d(active_vmode);
			vdc_in_blank = 0;
		}
		else if (vsync && field == 0) {
			mb93493fb_dma_set_2d(active_vmode);
			vdc_in_blank = 0;
		}
	}

	/////
	///// clear interrupt status bits
	/////
	rs &= VDC_RS_DFI | VDC_RS_DCSR | VDC_RS_DCM;
	__set_MB93493_VDC(RS, rs);

#if 0
	if (rs && (1 << 17)) {
		/* VSYNC interrupt */
		BUG();
	}

	if (rs && (1 << 18)) {
		/* underflow interrupt */
		BUG();
	}
#endif

}	/* end mb93493_vdc_interrupt() */



/*****************************************************************************/
/*
 * open the frame buffer
 */
static int mb93493fb_open(struct fb_info *_info, int user)
{
	struct mb93493fb_info *info = container_of(_info,
		   struct mb93493fb_info, gen.info);
	int ret;

	if (atomic_add_return(1, &info->open_count) == 1) {
		/* acquire a DMA channel and interrupt */

		kdebug("%s: calling mb93493_dma_open\n", __FUNCTION__);
		ret = mb93493_dma_open("mb93493 fb dma",
			DMA_MB93493_VDC,
			FRV_DMA_CAP_DREQ,
			mb93493_dma_interrupt, SA_SHIRQ, NULL);
		if (ret < 0)
			goto out2;

		vdc_dma_channel = ret;
		info->dma = ret;

		kdebug("%s: calling request_irq\n", __FUNCTION__);
		ret = request_irq(IRQ_MB93493_VDC,
			mb93493_vdc_interrupt,
			SA_SHIRQ | SA_INTERRUPT,
			"mb93493 fb irq", info);
		if (ret < 0)
			goto out1;
		kdebug("%s: PASSED, returning 0\n", __FUNCTION__);
	}

	return 0;

 out1:
	mb93493_dma_close(info->dma);
	info->dma = -1;

 out2:
	kdebug("FAILED <out> ret=%d\n", ret);
	atomic_dec(&info->open_count);

	return ret;
}	/* end mb93493fb_open() */

/*****************************************************************************/
/*
 * release the frame buffer
 */
static int mb93493fb_release(struct fb_info *_info, int user)
{
	struct mb93493fb_info *info =
	    container_of(_info, struct mb93493fb_info, gen.info);

	ktrace("%s\n", __FUNCTION__);

	if (atomic_dec_and_test(&info->open_count)) {
		mb93493fb_blank(VESA_POWERDOWN, &info->gen);

		//free_irq(IRQ_MB93493_VDC, info);
		mb93493_dma_close(info->dma);
		info->dma = -1;
	}

	return 0;
}				/* end mb93493fb_release() */

static void init_mb93493hw(void)
{
	kdebug("%s\n", __FUNCTION__);
	__set_MB93493_LBSER(__get_MB93493_LBSER() | MB93493_LBSER_VDC | MB93493_LBSER_GPIO);
	__set_MB93493_GPIO_SOR(0, __get_MB93493_GPIO_SOR(0) | 0x00ff0000);
	__set_MB93493_GPIO_SOR(1, __get_MB93493_GPIO_SOR(1) | 0x0000ff00);

	/* reset the VDC */
	__set_MB93493_VDC(RC, VDC_RC_DSR);
}


/*****************************************************************************/
/*
 * initialise the frame buffer driver
 */
int __init mb93493fb_init(void)
{
	int err;

	ktrace("Entering %s\n", __FUNCTION__);

	init_mb93493hw();

	/* allocate a frame buffer */

#ifdef NO_MM
	fb_info.framebuffer = kmalloc(MB93493_FB_ALLOC_SIZE, GFP_KERNEL);
	fb_size = MB93493_FB_ALLOC_SIZE;
#else
	{
		struct page *page, *pend;

		fb_order = PAGE_SHIFT;
		while ((fb_order < 31) && (MB93493_FB_ALLOC_SIZE > (1 << fb_order)))
			fb_order++;
		fb_order -= PAGE_SHIFT;
		kdebug("order for %d byte framebuffer is %d\n",
		       MB93493_FB_ALLOC_SIZE, (int)fb_order);

		fb_pg = alloc_pages(GFP_HIGHUSER, fb_order);
		if (! fb_pg) {
			printk("%s: couldn't allocate image buffer (order=%d)\n",
			       __FUNCTION__, (int)fb_order);
			return -ENOMEM;
		}

		fb_size = PAGE_SIZE << fb_order;
		kdebug("image buffer size is %d\n", fb_size);

		fb_phys = (unsigned long) page_to_phys(fb_pg);
		kdebug("fb physical address is 0x%08x\n", (unsigned int) fb_phys);

		fb_va = page_address(fb_pg);
		fb_info.framebuffer = (uint8_t *)fb_va;
		kdebug("fb virtual address is  0x%08x\n", (unsigned int) fb_va);

		/* clear the framebuffer */
		kdebug("clearing the frambuffer\n");
		memset(fb_info.framebuffer, 0, MB93493_FB_ALLOC_SIZE);

		kdebug("marking pages as reserved\n");
		/* Mark the pages as reserved, otherwise remap_page_range */
		/* doesn't do what we want */
		pend = fb_pg + (1 << fb_order);
		for (page = fb_pg; page < pend; page++)
			SetPageReserved(page);
	}
#endif
	if (! fb_info.framebuffer) {
		printk("%s: failed to allocate framebuffer (%d bytes)\n",
		       __FUNCTION__, MB93493_FB_ALLOC_SIZE);
		return -ENOMEM;
	}
	__set_MB93493_VDC(RC, 0);

	strcpy(fb_info.gen.info.modename, "MB93493");
	fb_info.gen.fbhw = &mb93493fb_switch;
	fb_info.gen.parsize = sizeof(struct mb93493fb_par);
	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.node = -1;
	fb_info.gen.info.fbops = &mb93493fb_ops;
	fb_info.gen.info.disp = &disp;
	fb_info.gen.info.switch_con = &fbgen_switch;
	fb_info.gen.info.updatevar = &fbgen_update_var;
	fb_info.gen.info.blank = &fbgen_blank;
	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
	fb_info.gen.info.screen_base = fb_info.framebuffer;
#ifdef MODULE
	mb93493fb_setup(mode_string);
#endif
	strcpy(display_string, "<Display not set>");

	switch(display_type) {
#ifdef CONFIG_MB93093_PDK
	case DTYPE_LCD:
		active_vmode = &lcd_320_242_rgb;
		strcpy(display_string, "LCD");
		break;
#endif
	case DTYPE_VGA:
		active_vmode = &vga_640_480_rgb;
		if (! strncmp(mode_string, "800x600", 7))
			active_vmode = &vga_800_600_rgb;
		strcpy(display_string, "VGA");
		break;
	case DTYPE_PAL:
		active_vmode = &ntsc_688_480;
		active_vmode->pix_x = 720;
		active_vmode->pix_y = 576;
		active_vmode->vsop = 0;
		active_vmode->hsop = 0;
		active_vmode->pal = 1;
		active_vmode->rhdc = 720;
		active_vmode->rhfp = 11;
		active_vmode->rhsc = 67;
		active_vmode->rhbp = 66;
		active_vmode->rvdc = 288;
		active_vmode->rvfp = 1;
		active_vmode->rvsc = 22;
		active_vmode->rvbp = 1;
		strcpy(mode_string, "720x576-16");
		strcpy(display_string, "PAL");
		break;
	case DTYPE_NTSC:
		if (! strncmp(mode_string, "688x480", 7)) {
			active_vmode = &ntsc_688_480;
			strcpy(mode_string, "688x480-16");
		} else {
			active_vmode = &ntsc_720_480;
			strcpy(mode_string, "720x480-16");
		}
		strcpy(display_string, "NTSC");
		break;

	case DISPLAY_NOT_SET:
	default:
		kdebug("Display type not set, choosing defaults\n");
#ifdef CONFIG_MB93093_PDK
		strcpy(mode_string, "320x242-32");
		display_type = DTYPE_LCD;
		strcpy(display_string, "LCD");
		fb_info.modedb = mb93493fb_lcd_modedb;
		fb_info.modedb_size = mb93493fb_lcd_modedb_size;
		active_vmode = &lcd_320_242_rgb;
#else
		strcpy(mode_string, "640x480-24");
		display_type = DTYPE_VGA;
		strcpy(display_string, "VGA");
		fb_info.modedb = mb93493fb_vga_modedb;
		fb_info.modedb_size = mb93493fb_vga_modedb_size;
		active_vmode = &vga_640_480_rgb;
#endif
	}
	kdebug("display type = \"%s\"\n", display_string);
	kdebug("mode_string  = \"%s\"\n", mode_string);

	/* Choose the initial video mode */
	disp.var.accel_flags = 0;
	if (! fb_find_mode(&disp.var, &fb_info.gen.info,
			   mode_string, fb_info.modedb,
			   fb_info.modedb_size, &fb_info.modedb[0],
			   active_vmode->pix_sz * 8)) {
		printk("%s: default mode setting rejected\n",
		       __FUNCTION__);
		goto inval;
	}

	printk("Fujitsu MB93493 framebuffer driver [%s mode xres=%d yres=%d bits_pp=%d]\n",
			display_string, disp.var.xres, disp.var.yres, disp.var.bits_per_pixel);

	/* Change the video mode */
	err = fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
	if (err) {
		printk("%s: unable to set initial mode\n", __FUNCTION__);
		goto inval;
	}

	fbgen_set_disp(-1, &fb_info.gen);
	fbgen_install_cmap(0, &fb_info.gen);

	kdebug("%s: register_framebuffer\n", __FUNCTION__);
	if (register_framebuffer(&fb_info.gen.info) < 0) {
		printk("%s: register_framebuffer failed\n", __FUNCTION__);
		return -EINVAL;
	}

	printk(KERN_INFO "fb%d: %s frame buffer device\n",
	    GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);

	mb93493fb_dma_set_2d(active_vmode);

	ktrace("Leaving %s\n", __FUNCTION__);
	return 0;

inval:
	ktrace("%s: Leaving, error EINVAL\n", __FUNCTION__);
	return -EINVAL;
}	/* end mb93493fb_init() */

/*****************************************************************************/
/*
 * cleanup the frame buffer driver
 */
static void __exit mb93493fb_cleanup(struct fb_info *info)
{
	ktrace("Entering %s\n", __FUNCTION__);

	unregister_framebuffer(info);
	current_par_valid = 0;

#ifdef NO_MM
	kfree(fb_info.framebuffer);
#else
	free_pages((unsigned long)fb_pg, fb_order);
#endif
	ktrace("Leaving %s\n", __FUNCTION__);
}	/* end mb93493fb_cleanup() */

/*****************************************************************************/
/*
 * handle module exit
 */
static void __exit mb93493fb_exit(void)
{
	ktrace("Entering %s\n", __FUNCTION__);

	mb93493fb_cleanup(&fb_info.gen.info);

	ktrace("Leaving %s\n", __FUNCTION__);
}	/* end mb93493fb_exit() */



/*****************************************************************************/
/*
 *   set up the frame buffer driver
 *   parse user specified options:
 *
 *   video=<display_type><xres>x<yres>[-<bits_per_pixel>]
 */
#ifdef MODULE
static int 
#else
static int __init 
#endif
mb93493fb_setup(char *options)
{
	char buf[MODE_STR_SZ];
	char *p = &buf[0];
	int offset;

	ktrace("** Entering %s \"%s\"\n", __FUNCTION__, options);
	display_type = DISPLAY_NOT_SET;

	if (!options || !*options)
		return 0;

	strncpy(buf, options, MODE_STR_SZ);
	offset = 0;

	if (! strncmp(p, "vga", 3)) {
		offset = 3;
		display_type = DTYPE_VGA;
		fb_info.modedb = mb93493fb_vga_modedb;
		fb_info.modedb_size = mb93493fb_vga_modedb_size;
	} else if (! strncmp(p, "pal", 3)) {
		offset = 3;
		display_type = DTYPE_PAL;
		fb_info.modedb = mb93493fb_pal_modedb;
		fb_info.modedb_size = mb93493fb_pal_modedb_size;
	} else if (! strncmp(p, "ntsc", 4)) {
		offset = 4;
		display_type = DTYPE_NTSC;
		fb_info.modedb = mb93493fb_ntsc_modedb;
		fb_info.modedb_size = mb93493fb_ntsc_modedb_size;
	} else if (! strncmp(p, "lcd", 3)) {
		offset = 3;
#ifdef CONFIG_MB93093_PDK
		display_type = DTYPE_LCD;
		fb_info.modedb = mb93493fb_lcd_modedb;
		fb_info.modedb_size = mb93493fb_lcd_modedb_size;
#endif
	}

	if (display_type == DISPLAY_NOT_SET) {
#ifdef CONFIG_MB93093_PDK
		display_type = DTYPE_LCD;
		fb_info.modedb = mb93493fb_lcd_modedb;
		fb_info.modedb_size = mb93493fb_lcd_modedb_size;
#else
		display_type = DTYPE_VGA;
		fb_info.modedb = mb93493fb_vga_modedb;
		fb_info.modedb_size = mb93493fb_vga_modedb_size;
#endif
	}

	if (is_digit(p[offset])) {
		strncpy(mode_string, &p[offset], sizeof(mode_string));
	}

	ktrace("** Leaving %s\n", __FUNCTION__);

	return 0;
}	/* end mb93493fb_setup() */

#ifdef NO_MM
static unsigned long
mb93493_get_fb_unmapped_area(struct file *file,
		     unsigned long addr, unsigned long len,
		     unsigned long pgoff, unsigned long flags)
{
  	unsigned long off = pgoff << PAGE_SHIFT;

	if (off < fb_size)
	  	if (len <= fb_size - off)
		  	return virt_to_phys(fb_info.framebuffer + off);

	return -EINVAL;
}
#endif


#ifndef MODULE
__setup("video=", mb93493fb_setup);
#endif


/*****************************************************************************/
/*
 *  Modularisation
 */
module_init(mb93493fb_init)
module_exit(mb93493fb_exit)

