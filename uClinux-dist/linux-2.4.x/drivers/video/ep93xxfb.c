/******************************************************************************
 *
 *  File:	linux/drivers/video/ep93xxfb.c
 *
 *  Purpose: Support CRT output on a EP9312/EP9315 evaluation board.
 *
 *  Build:	Define CONFIG_FB, CONFIG_FB_EP93XX
 *		which will bring in:
 *			CONFIG_FONT_8x8
 *			CONFIG_FONT_8x16
 *			CONFIG_FONT_ACORN_8x8
 *
 *		When building as console, define CONFIG_VT, CONFIG_VT_CONSOLE
 *		and make sure CONFIG_CMDLINE does NOT have "console=" set.
 *		CONFIG_EP93XX_KBD_SPI or CONFIG_EP93XX_KBD_SCANNED should also
 *		be specified.
 *
 *  History:	010529	Norman Farquhar at LynuxWorks.
 *
 *		Initial version supports:
 *		- crt only
 *		- fixed frequency monitor timings: 640x480
 *		- single default video mode supported: 640x480x8
 *		- no module support because this is core logic
 *		- never will be SMP
 *
 *		Derived from:	arm-linux-2.4.0-test11-rmk1
 *				linux/drivers/video/sa1100fb.c
 *				Version 2000/08/29
 *				Copyright (C) 1999 Eric A. Thomas
 *				See file for many other contributors.
 *
 * (c) Copyright 2001 LynuxWorks, Inc., San Jose, CA.  All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *=============================================================================
 *	Overview of Frame Buffer driver
 *=============================================================================
 *
 *	The frame buffer driver supports the core logic display controller
 *	of the ep93xx family machines.  These machines are embedded systems
 *	and this driver provides for a few video modes with small code
 *	footprint.  Both CRT and LCD may be supported in the future.
 *
 *	Generic fb driver model (skeletonfb.c, fbgen.c) is a bit whacked
 *	so this driver has been converted back to simpler older style.
 *	See below for list of complaints about generic fb model!
 *
 *
 *	Linux color models supported:
 *
 *		CFB8 is 8 bit packed pixel format using 256 color LUT
 *		Believed to be similar to Microsoft Windows:
 *			16 colors VGA standard
 *			16 grays
 *			224 color loadable palette
 *		Note: Linux Logo uses 6x6x6=216 color space
 *
 *	Linux color models NOT supported:
 *
 *		CFB4 is 4 bit packed pixel format using 16 color LUT
 *			16 colors VGA standard
 *		CFB16 is 16 bit flexible format (ep93xx uses R5:G6:B5)
 *		CFB24 is 24 bit truecolor R8:G8:B8
 *		CFB32 is 32 bit truecolor alpha:R8:G8:B8
 *
 *	Reminder about setting up /dev device nodes:
 *
 *		console	c 5,1		to virtual console driver
 *					so that Linux bootup is visible
 *					when fb is console.
 *
 *		fb	link /dev/fb0	used by applications
 *
 *		fb0	c 29,0		to fbmem device
 *
 *		tty	link /dev/console	recommended by BinZ for
 *		tty0	link /dev/console	older Linux applications
 *
 *
 *=============================================================================
 *	Hardware Details
 *=============================================================================
 *
 *	1.  Display controller uses system memory, not a dedicated frame buffer
 *	RAM.  This means that display controller bandwidth to RAM steals from
 *	CPU bandwidth to RAM.
 *
 *		So if bandwidth to memory is about 80MB/s assuming that
 *		all accesses are 16 byte blocks taking 10 clocks on 50MHz
 *		system bus.  Then 640x480x8 60Hz is about 19MB/s, or
 *		1/4 of bus bandwidth which should not impact us badly.
 *
 *	2.  Dual hw palettes are a way to avoid sparkle and allow updating at
 *	anytime, not just during blank period.  Pending hw palette switch
 *	takes place once per frame during blank.
 *
 *	So we keep the master palette copy in memory and update the active hw
 *	palette by updating the inactive hw palette and then switching.
 *
 *	The only complication is writing palette while switch is pending.
 *	We assume LUTSTAT is updated synchronously with bus clock so that
 *	writes to LUT complete to either RAM0 or RAM1, even if request to
 *	switch RAMs comes in asynchronously, in the middle of write to LUT.
 *
 *	3.  Some timing registers must be unlocked before access.
 *
 *=============================================================================
 *	Send to www.complaints.linux.com
 *=============================================================================
 *
 *	Generic fb model is too messy IMHO and here is my list of
 *	problems with it.
 *
 *	1.  Note ALL fbgen.c functions really take fb_info_gen, instead of
 *	fb_info parameters, by casting info instead of changing prototype.
 *
 *	2.  Messy stuff in doing encode/decode var.
 *
 *	3.  The info structure holds the controlling variables for video.
 *	But data is passed around and variables tweaked inside routines
 *	in which you would not expect it.
 *
 *	4.  It is not clear that all variables of a structure get inited
 *	properly because it is not done in one spot.
 *
 *	5.  In the driver model fbskeleton.c, initially, both the 'var' and
 *	'fix' data are derived from 'par'.  This seems backward, and I think
 *	reflects Linux genesis on a PC with a BIOS which has already set the
 *	initial video mode.
 *
 *	6.  For some reason, there is a local copy of 'par' which is the true
 *	current mode (Why not use the 'par' field in info??).
 *
 *	7.  Consistency checks are device specific, but fb implementation
 *	is messy:  high level video mode requests are pushed down to low
 *	level data 'par' and then pulled back out again to high level
 *	with a consistent, possibly changed mode, changed 'var'
 *
 *		decode_var	really checks var consistency and
 *				translates var to par
 *
 *		encode_var	translates par to var
 *				(one driver just passes back current var
 *				 which assumes that par set from var)
 *
 *****************************************************************************/
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/wrapper.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/proc/pgtable.h>

//TBD some of these may not be needed until full implementation

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include "ep93xxfb.h"

#define RasterSetLocked(registername,value) \
    {                                       \
        outl( 0xAA, REALITI_SWLOCK );       \
        outl( value, registername);         \
    }

/*
 *  Debug macros
 */
#ifdef DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

// Max palette entries over all video modes
#define MAX_PALETTE_NUM_ENTRIES		256

// Max X resolution and Y resolution depend on whether LCD is supported,
// and the monitor type.

#define MAX_CRT_XRES	640
#define MAX_CRT_YRES	480

// Max bpp is fixed for low resolutions, but fundamentally depends on the
// memory bandwidth which the system can allocate to the raster engine,
// and so depends on application.
//
//	TBD can create an array which determines max bpp based on passed
//	TBD in resolutions.
//
#ifdef CONFIG_FB_EP93XX_8BPP
#define MAX_BPP		8
#elif defined(CONFIG_FB_EP93XX_16BPP_565)
#define MAX_BPP		16
#else
#define MAX_BPP		32
#endif

// Framebuffer max mem size in bytes over all video modes
// Note: mypar not valid yet so need hard numbers here.
#define FB_MAX_MEM_SIZE ((MAX_CRT_XRES * MAX_CRT_YRES * MAX_BPP)/8)

// Framebuffer mapped mem size in bytes
//	This will be an integral number of pages.
#define FB_MAPPED_MEM_SIZE (PAGE_ALIGN(FB_MAX_MEM_SIZE + PAGE_SIZE))

/* Minimum X and Y resolutions */
//TBD how realistic is this for CRT??
//#define MIN_XRES	64
//#define MIN_YRES	64

#define EP93XX_NAME	"EP93XX"
#define NR_MONTYPES	1

/* Local LCD controller parameters */
/* These can be reduced by making better use of fb_var_screeninfo parameters.*/
/* Several duplicates exist in the two structures. */
struct ep93xxfb_par
{
	dma_addr_t	p_screen_base;
	unsigned char	*v_screen_base;
	unsigned long	screen_size;
	unsigned int	palette_size;
	unsigned int	xres;
	unsigned int	yres;
	unsigned int	bits_per_pixel;
	int		montype;
	unsigned int	currcon;
	unsigned int	visual;
	u16		palette[16];	// Funky 16 table lookup used by
					// "optional" Parameter.
};

//TBD Linux fbmon.c tries to check monitor support for requested mode.
//TBD But 2.4.0 test11 has removed this code from the build.
/* Fake monspecs to fill in fbinfo structure */
//TBD strict VGA monitor has 60,60 instead of 50,65
static struct fb_monspecs __initdata monspecs = {
	 30000, 70000, 50, 65, 0 	/* Generic, not fixed frequency */
};

static char default_font_storage[40];
static char *default_font = "Acorn8x8";

#if defined (CONFIG_FB_LCD_EP93XX)
#define DEFAULT_MODE 1
#elif defined (CONFIG_FB_CX25871)
#define DEFAULT_MODE 3
#elif defined (CONFIG_FB_CRT_EP93XX)
#define DEFAULT_MODE 0
#else
#error What Display Setting was that!!!
#endif

static int mode = DEFAULT_MODE;

#ifdef CONFIG_FB_CX25871_OVERSCAN
#define DEFAULT_OVERSCAN 1
#else
#define DEFAULT_OVERSCAN 0
#endif

static int overscan = DEFAULT_OVERSCAN;

typedef struct _DisplayTimingValues
{
	const char	*Name;
	unsigned long	DisplayID;
	int		(* RasterConfigure)(struct _DisplayTimingValues *pTimingValues);
	unsigned short	Refresh;
	unsigned long	VDiv;

	unsigned short	HRes;
	unsigned short	HFrontPorch;
	unsigned short	HBackPorch;
	unsigned short	HSyncWidth;
	unsigned short	HTotalClocks;

	unsigned short	VRes;
	unsigned short	VFrontPorch;
	unsigned short	VBackPorch;
	unsigned short	VSyncWidth;
	unsigned short	VTotalClocks;
} DisplayTimingValues;

typedef int (* fRasterConfigure)(DisplayTimingValues *);

typedef enum
{
	CRT_GENERIC,
	Philips_LB064V02A1,
	CX25871,
	Sharp
} DisplayType;

#define TIMING_VALUES(NAME, DISPID, FUNC, REFRESH, VDIV,                \
                   HRES, HFP, HBP, HSYNC, VRES, VFP, VBP, VSYNC)        \
{                                                                       \
	Name:NAME,                                                      \
	DISPID, FUNC, REFRESH, VDIV,                                    \
	HRES, HFP, HBP, HSYNC, (HRES + HFP + HBP + HSYNC),              \
	VRES, VFP, VBP, VSYNC, (VRES + VFP + VBP + VSYNC)               \
}

static void InitializeCX25871For640x480NTSC(void);
static int Conexant_CX25871(DisplayTimingValues *pTimingValues);
static int PhilipsLCD(DisplayTimingValues *pTimingValues);

DisplayTimingValues static TimingValues[]=
{
	//
	// 640x480 Progressive Scan
	//
	TIMING_VALUES("CRT_GENERIC", CRT_GENERIC,
		      (fRasterConfigure)0,
		      60, 0x0000c108, 640, 16, 48, 96, 480, 11, 31, 2),

	//
	// 640x480 Progressive Scan Philips LB064V02A1 on EDB9312 Board.
	//
	TIMING_VALUES("Philips LB064V02A1", Philips_LB064V02A1,
		      PhilipsLCD,
		      68, 0x0000c107, 640, 16, 48, 96, 480, 11, 31, 2),

	//
	// Sharp LQ64D343 LCD Panel
	//
	TIMING_VALUES("Sharp LQ64d343", CRT_GENERIC,
		      (fRasterConfigure)0,
		      60, 0x0000c205, 640, 32, 32, 96, 480, 34, 34, 4),

	//
	// 640x480 NTSC Support for Conexant CX25871
	//
	TIMING_VALUES("Conexant CX25871", CX25871,
		      Conexant_CX25871,
		      60, 0x0000c317, 640, 0, 0, 0, 480, 0, 0, 0),
};
#define NUM_TIMING_VALUES (sizeof(TimingValues)/sizeof(DisplayTimingValues))

unsigned int master_palette[256];	/* master copy of palette data */

//TBD Before fbcon started, the driver calls set_var with con=-1
//TBD which initializes global_disp and puts ptr to it in info.
//TBD Later on display console array keeps track of display settings.
static struct display global_disp;	/* Initial Display Settings */

static struct fb_info fb_info;		// initialized in init_fbinfo()
static struct ep93xxfb_par mypar;

static int ep93xxfb_get_fix(struct fb_fix_screeninfo *fix,
			    int con, struct fb_info *info);
static int ep93xxfb_get_var(struct fb_var_screeninfo *var,
			    int con, struct fb_info *info);
static int ep93xxfb_set_var(struct fb_var_screeninfo *var,
			    int con, struct fb_info *info);
static int ep93xxfb_get_cmap(struct fb_cmap *cmap, int kspc,
			     int con, struct fb_info *info);
static int ep93xxfb_set_cmap(struct fb_cmap *cmap, int kspc,
			     int con, struct fb_info *info);
static int ep93xxfb_ioctl(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg, int con,
			  struct fb_info *info);

static int  ep93xxfb_switch(int con, struct fb_info *info);
static void ep93xxfb_blank(int blank, struct fb_info *info);
static int  MapFramebuffer(void);
static int  ConfigureRaster(struct fb_var_screeninfo *var,
			    DisplayTimingValues *pTimingValues);

static struct fb_ops ep93xxfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	ep93xxfb_get_fix,
	fb_get_var:	ep93xxfb_get_var,
	fb_set_var:	ep93xxfb_set_var,
	fb_get_cmap:	ep93xxfb_get_cmap,
	fb_set_cmap:	ep93xxfb_set_cmap,
	fb_ioctl:	ep93xxfb_ioctl,
};

//-----------------------------------------------------------------------------
//	Local functions
//-----------------------------------------------------------------------------

/*
 * ep93xxfb_palette_write:
 *	Encode palette data to 24bit palette format.
 *	Write palette data to the master palette and inactive hw palette
 *	switch palettes.  And handle asynchronous palette switches.
 */
static inline void
ep93xxfb_palette_write(u_int regno, u_int red, u_int green,
		       u_int blue, u_int trans)
{
	unsigned int cont, i, pal;

	// Only supports color LUT, not gray LUT
	//
	//	TBD if not in 4 or 8bpp, then does LUT logic operate?
	//	TBD or do we need to do nothing here?

	// LCD:	TBD RGB mapping may match spec p193.
	//
	// CRT: LUT RGB mapping is really R/G/B from high to low bits
	//	because really determined by wiring to video DAC.
	//	(disregard spec p 193 showing color LUT B/G/R order)
	//	Here are the details:
	//
	//	Shift mode 1 directs LUT bits 7-2 onto P5-P0,
	//	LUT bits 15-10 onto P11-P6, and LUT bits 23-16
	//	onto P17-P12.
	//
	//	Board wired P17-12 to video DAC Red inputs,
	//	P11-P6 wired to video DAC Green inputs, and
	//	P5-P0 wired to video DAC Blue inputs.
	//
	pal = ((red & 0xFF00) << 8) | (green & 0xFF00) | ((blue & 0xFF00) >> 8);

	master_palette[regno] = pal;

	// Two cases here:
	// 1.  	If LUT switch is pending, then write inactive LUT and when
	//	switch happens, this new palette entry will be active.
	// 	Race condition here between LUT switch and this write is okay
	// 	since we fix it below.
	//
	// 2.	If LUT switch is not pending, then write here is incomplete
	//	and whole palette will be written below.

	outl( pal, (COLOR_LUT+(regno<<2)) );

	cont = inl(LUTCONT);

	if ((cont&LUTCONT_STAT && cont&LUTCONT_RAM1) ||
	    (!(cont&LUTCONT_STAT) && !(cont&LUTCONT_RAM1))) {
		// LUT switch is no longer pending

		// We do not know if write to LUT above really went
		// to currently active LUT.  So need to make sure that
		// data gets into inactive LUT and switch LUTs.
		//
		// But currently inactive LUT may be out of date
		// in more entries than just last write.
		// Need to update currently inactive LUT for all writes
		// which went to currently active LUT.
		// Fully update the LUT now, which is a simpler policy
		// than trying to track writes and do partial update of LUT.
		// (Worstcase impact: we update palette every frame)

		for (i=0; i< 256; i++)	// Update inactive LUT
			outl( master_palette[i], (COLOR_LUT+(i<<2)) );
		// Switch active LUTs next frame
		outl( cont ^ LUTCONT_RAM1, LUTCONT );
	}
}

static inline void
ep93xxfb_palette_read(u_int regno, u_int *red, u_int *green,
		      u_int *blue, u_int *trans)
{
	// Only supports color LUT, not gray LUT

	unsigned int pal;

	// Read only needs to access master palette, not hw palettes.
	pal = master_palette[regno];

	//TBD LCD mode may change LUT from R/G/B order to B/G/R order
	*red = (pal >> 8) & 0xFF00;
	*green = pal & 0xFF00;
	*blue = (pal << 8) & 0xFF00;
	*trans  = 0;
}

//-----------------------------------------------------------------------------
//	Helper functions for fb driver
//-----------------------------------------------------------------------------

static int
ep93xxfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
		   u_int *trans, struct fb_info *info)
{
	if (regno >= mypar.palette_size)
		return 1;

	ep93xxfb_palette_read(regno, red, green, blue, trans);

	return 0;
}

static inline u_int
chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int
ep93xxfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		   u_int trans, struct fb_info *info)
{
	u_int val;

	if (regno >= mypar.palette_size)
		return 1;

	switch (info->disp->visual) {
	case FB_VISUAL_TRUECOLOR:
		if (regno < 16) {
			u16 *pal = info->pseudo_palette;
			val  = chan_to_field(red, &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue, &info->var.blue);
				pal[regno] = val;
		}
		break;
	case FB_VISUAL_PSEUDOCOLOR:
		ep93xxfb_palette_write(regno, red, green, blue, trans);
		break;
	}

	return 0;
}

//TBD Warning: fbmem.c ioctl could result in any of the helper functions being
//TBD called with con=-1, which happens when info->display_fg == NULL,
//TBD which presumably is only during initialization of fbcon.
//TBD Is there any other condition under which con=-1?
//TBD Maybe we do not even need to support it!

static int
ep93xxfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
		          struct fb_info *info)
{
	int err = 0;

	//TBD does not expect call with con=-1 if currcon!=-1
	if (con == -1)
		DPRINTK("get_cmap called with con=-1\n");

	DPRINTK("mypar.visual=%d\n", mypar.visual);
	if (con == mypar.currcon)
		err = fb_get_cmap(cmap, kspc, ep93xxfb_getcolreg, info);
	else if (fb_display[con].cmap.len)
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(mypar.palette_size),
			     cmap, kspc ? 0 : 2);
	return err;
}

static int
ep93xxfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		  struct fb_info *info)
{
	int err = 0;

	//
	// What kind of request is this???
	//
	if (con == -1) {
		DPRINTK("ERROR set_cmap called with con=-1\n");
		return(-1);
	}

	DPRINTK("mypar.visual=%d\n", mypar.visual);

	if (!fb_display[con].cmap.len)
		err = fb_alloc_cmap(&fb_display[con].cmap, mypar.palette_size,
				    0);
	if (!err) {
		if (con == mypar.currcon)
			err = fb_set_cmap(cmap, kspc, ep93xxfb_setcolreg,
					  info);
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	}
	return err;
}

static unsigned long cursor_data[64][4];

static void
ep93xxfb_cursor(struct ep93xx_cursor *cursor)
{
	unsigned long data[64 * 4];
	long i, x, y, save;

	if (cursor->flags & CURSOR_OFF)
		outl(inl(CURSORXYLOC) & ~0x00008000, CURSORXYLOC);

	if (cursor->flags & CURSOR_SETSHAPE) {
		copy_from_user(data, cursor->data,
			       cursor->width * cursor->height / 4);

		save = inl(CURSORXYLOC);
		outl(save & ~0x00008000, CURSORXYLOC);

		for (y = 0, i = 0; y < cursor->height; y++) {
			for (x = 0; x < cursor->width; x += 16)
				cursor_data[y][x] = data[i++];
		}

		outl(virt_to_phys(cursor_data), CURSOR_ADR_START);
		outl(virt_to_phys(cursor_data), CURSOR_ADR_RESET);
		outl(0x00000300 | ((cursor->height - 1) << 2) |
		     ((cursor->width - 1) >> 4), CURSORSIZE);
		outl(save, CURSORXYLOC);
	}

	if (cursor->flags & CURSOR_SETCOLOR) {
		outl(cursor->color1, CURSORCOLOR1);
		outl(cursor->color2, CURSORCOLOR2);
		outl(cursor->blinkcolor1, CURSORBLINK1);
		outl(cursor->blinkcolor2, CURSORBLINK2);
	}

	if (cursor->flags & CURSOR_BLINK) {
		if (cursor->blinkrate)
			outl(0x00000100 | cursor->blinkrate, CURSORBLINK);
		else
			outl(0x000000ff, CURSORBLINK);
	}

	if (cursor->flags & CURSOR_MOVE) {
		x = (inl(HACTIVESTRTSTOP) & 0x000003ff) - cursor->dx - 2;
		y = (inl(VACTIVESTRTSTOP) & 0x000003ff) - cursor->dy;
		outl((inl(CURSORXYLOC) & 0x8000) | (y << 16) | x, CURSORXYLOC);
	}

	if (cursor->flags & CURSOR_ON)
		outl(inl(CURSORXYLOC) | 0x00008000, CURSORXYLOC);
}

#ifdef CONFIG_EP93XX_GRAPHICS
#ifdef CONFIG_FB_EP93XX_8BPP
#define BITS_PER_PIXEL 8
#define BYTES_PER_PIXEL 1
#define PIXEL_MASK 3
#define PIXEL_SHIFT 2
#define PIXEL_FORMAT 0x00040000
#endif
#ifdef CONFIG_FB_EP93XX_16BPP_565
#define BITS_PER_PIXEL 16
#define BYTES_PER_PIXEL 2
#define PIXEL_MASK 1
#define PIXEL_SHIFT 1
#define PIXEL_FORMAT 0x00080000
#endif
#define BYTES_PER_LINE (fb_info.var.xres * BYTES_PER_PIXEL)

static DECLARE_WAIT_QUEUE_HEAD(ep93xxfb_wait_in);

static void
ep93xxfb_irq_handler(int i, void *blah, struct pt_regs *regs)
{
	outl(0x00000000, BLOCKCTRL);

	wake_up_interruptible(&ep93xxfb_wait_in);
}

static void
ep93xxfb_wait(void)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&ep93xxfb_wait_in, &wait);
	current->state = TASK_INTERRUPTIBLE;
	while (inl(BLOCKCTRL) & 0x00000001) {
		schedule();
		current->state = TASK_INTERRUPTIBLE;
	}
	remove_wait_queue(&ep93xxfb_wait_in, &wait);
	current->state = TASK_RUNNING;
}

static unsigned char pucBlitBuf[4096];

static int
ep93xxfb_blit(struct ep93xx_blit *blit)
{
	unsigned long value = 0, size, dx1, dx2, dy1, sx1, sx2, sy1;

	if ((blit->dx >= fb_info.var.xres) ||
	    (blit->dy >= fb_info.var.yres) ||
	    ((blit->dx + blit->width - 1) >= fb_info.var.xres) ||
	    ((blit->dy + blit->height - 1) >= fb_info.var.yres))
		return -EFAULT;

	value = blit->flags & (BLIT_TRANSPARENT | BLIT_MASK_MASK |
			       BLIT_DEST_MASK | BLIT_1BPP_SOURCE);

	if ((blit->flags & BLIT_SOURCE_MASK) == BLIT_SOURCE_MEMORY) {
		if (blit->flags & BLIT_1BPP_SOURCE) {
			size = blit->swidth * blit->height / 8;
			if (size <= 4096)
				copy_from_user(pucBlitBuf, blit->data, size);
			else {
				for (size = blit->height; size; ) {
					blit->height = 32768 / blit->swidth;
					if (blit->height > size)
						blit->height = size;
					ep93xxfb_blit(blit);
					blit->dy += blit->height;
					blit->data += blit->swidth *
						      blit->height / 8;
					size -= blit->height;
				}
				return 0;
			}
			if (blit->flags & BLIT_TRANSPARENT)
				value ^= 0x00004000;
			outl(0x00000007, SRCPIXELSTRT);
			outl(blit->swidth >> 5, SRCLINELENGTH);
			outl((blit->width - 1) >> 5, BLKSRCWIDTH);
			outl(blit->bgcolor, BACKGROUND);
		} else {
			size = blit->swidth * blit->height * BYTES_PER_PIXEL;
			if (size <= 4096)
				copy_from_user(pucBlitBuf, blit->data, size);
			else {
				for (size = blit->height; size; ) {
					blit->height = 4096 / BYTES_PER_PIXEL /
						       blit->swidth;
					if (blit->height > size)
						blit->height = size;
					ep93xxfb_blit(blit);
					blit->dy += blit->height;
					blit->data += blit->swidth *
						      blit->height *
						      BYTES_PER_PIXEL;
					size -= blit->height;
				}
				return 0;
			}
			outl(0x00000000, SRCPIXELSTRT);
			outl(blit->swidth >> PIXEL_SHIFT, SRCLINELENGTH);
			outl((blit->width - 1) >> PIXEL_SHIFT, BLKSRCWIDTH);
		}
		outl(virt_to_phys(pucBlitBuf), BLKSRCSTRT);
		dx1 = blit->dx;
		dy1 = blit->dy;
		dx2 = blit->dx + blit->width - 1;
	} else {
		if ((blit->sx >= fb_info.var.xres) ||
		    (blit->sy >= fb_info.var.yres) ||
		    ((blit->sx + blit->width - 1) >= fb_info.var.xres) ||
		    ((blit->sy + blit->height - 1) >= fb_info.var.yres))
			return -EFAULT;

		if ((blit->dy == blit->sy) &&
		    (blit->dx < blit->sx) &&
		    ((blit->dx + blit->width - 1) >= blit->sx)) {
			dx1 = blit->dx + blit->width - 1;
			dx2 = blit->dx;
			sx1 = blit->sx + blit->width - 1;
			sx2 = blit->sx;
			value |= 0x000000a0;
		} else {
			dx1 = blit->dx;
			dx2 = blit->dx + blit->width - 1;
			sx1 = blit->sx;
			sx2 = blit->sx + blit->width - 1;
		}

		if (blit->dy <= blit->sy) {
			dy1 = blit->dy;
			sy1 = blit->sy;
		} else {
			dy1 = blit->dy + blit->height - 1;
			sy1 = blit->sy + blit->height - 1;
			value |= 0x00000140;
		}

		outl(((sx1 & PIXEL_MASK) * BITS_PER_PIXEL) |
		     (((sx2 & PIXEL_MASK) * BITS_PER_PIXEL) << 16),
		     SRCPIXELSTRT);
		outl(inl(VIDSCRNPAGE) + (sy1 * BYTES_PER_LINE) +
		     ((sx1 * BYTES_PER_PIXEL) & ~PIXEL_MASK), BLKSRCSTRT);
		outl(BYTES_PER_LINE / 4, SRCLINELENGTH);
		if (sx1 < sx2)
			outl((sx2 >> PIXEL_SHIFT) - (sx1 >> PIXEL_SHIFT),
			     BLKSRCWIDTH);
		else
			outl((sx1 >> PIXEL_SHIFT) - (sx2 >> PIXEL_SHIFT),
			     BLKSRCWIDTH);
	}

	outl(((dx1 & PIXEL_MASK) * BITS_PER_PIXEL) |
	     (((dx2 & PIXEL_MASK) * BITS_PER_PIXEL) << 16), DESTPIXELSTRT);
	outl(inl(VIDSCRNPAGE) + (dy1 * BYTES_PER_LINE) +
	     ((dx1 * BYTES_PER_PIXEL) & ~PIXEL_MASK), BLKDSTSTRT);
	outl(BYTES_PER_LINE / 4, DESTLINELENGTH);
	if (dx1 < dx2)
		outl((dx2 >> PIXEL_SHIFT) - (dx1 >> PIXEL_SHIFT),
		     BLKDESTWIDTH);
	else
		outl((dx1 >> PIXEL_SHIFT) - (dx2 >> PIXEL_SHIFT),
		     BLKDESTWIDTH);
	outl(blit->height - 1, BLKDESTHEIGHT);
	outl(blit->fgcolor, BLOCKMASK);
	outl(blit->transcolor, TRANSPATTRN);
	outl(value | PIXEL_FORMAT | 0x00000003, BLOCKCTRL);
	ep93xxfb_wait();

	return 0;
}

static int
ep93xxfb_fill(struct ep93xx_fill *fill)
{
	if ((fill->dx >= fb_info.var.xres) ||
	    (fill->dy >= fb_info.var.yres))
		return -EFAULT;

	if ((fill->dx + fill->width - 1) >= fb_info.var.xres)
		fill->width = fb_info.var.xres - fill->dx;
	if ((fill->dy + fill->height - 1) >= fb_info.var.yres)
		fill->height = fb_info.var.yres - fill->dy;

	outl(mypar.p_screen_base + (fill->dy * BYTES_PER_LINE) +
	     ((fill->dx * BYTES_PER_PIXEL) & ~PIXEL_MASK), BLKDSTSTRT);
	outl(((fill->dx & PIXEL_MASK) * BITS_PER_PIXEL) |
	     ((((fill->dx + fill->width - 1) & PIXEL_MASK) *
	       BITS_PER_PIXEL) << 16), DESTPIXELSTRT);
	outl(BYTES_PER_LINE / 4, DESTLINELENGTH);
	outl(((fill->dx + fill->width - 1) >> PIXEL_SHIFT) -
	     (fill->dx >> PIXEL_SHIFT), BLKDESTWIDTH);
	outl(fill->height - 1, BLKDESTHEIGHT);
	outl(fill->color, BLOCKMASK);
	outl(PIXEL_FORMAT | 0x0000000b, BLOCKCTRL);
	ep93xxfb_wait();

	return 0;
}

static unsigned long
isqrt(unsigned long a)
{
	unsigned long rem = 0;
	unsigned long root = 0;
	int i;

	for (i = 0; i < 16; i++) {
		root <<= 1;
		rem = ((rem << 2) + (a >> 30));
		a <<= 2;
		root++;
		if (root <= rem) {
			rem -= root;
			root++;
		} else
			root--;
	}
	return root >> 1;
}

static int
ep93xxfb_line(struct ep93xx_line *line)
{
	unsigned long value = 0;
	long dx, dy, count, xinc, yinc, xval, yval, incr;

	if ((line->x1 >= fb_info.var.xres) ||
	    (line->x2 >= fb_info.var.xres) ||
	    (line->y1 >= fb_info.var.yres) ||
	    (line->y2 >= fb_info.var.yres))
		return -EFAULT;

	dx = line->x2 - line->x1;
	if (dx < 0) {
		value |= 0x00000020;
		dx *= -1;
	}
	dy = line->y2 - line->y1;
	if (dy < 0) {
		value |= 0x00000040;
		dy *= -1;
	}
	if (line->flags & LINE_PRECISE) {
		count = isqrt(((dy * dy) + (dx * dx)) * 4096);
		xinc = (4095 * 64 * dx) / count;
		yinc = (4095 * 64 * dy) / count;
		xval = 2048;
		yval = 2048;
		count = 0;
		while (dx || dy) {
			incr = 0;
			xval -= xinc;
			if (xval <= 0) {
				xval += 4096;
				dx--;
				incr = 1;
			}
			yval -= yinc;
			if (yval <= 0) {
				yval += 4096;
				dy--;
				incr = 1;
			}
			count += incr;
		}
	} else {
		if (dx == dy) {
			xinc = 4095;
			yinc = 4095;
			count = dx;
		} else if (dx < dy) {
			xinc = (dx * 4095) / dy;
			yinc = 4095;
			count = dy;
		} else {
			xinc = 4095;
			yinc = (dy * 4095) / dx;
			count = dx;
		}
	}

	outl(0x08000800, LINEINIT);
	if (line->flags & LINE_PATTERN)
		outl(line->pattern, LINEPATTRN);
	else
		outl(0x000fffff, LINEPATTRN);
	outl(mypar.p_screen_base + (line->y1 * BYTES_PER_LINE) +
	     ((line->x1 * BYTES_PER_PIXEL) & ~PIXEL_MASK), BLKDSTSTRT);
	outl(((line->x1 & PIXEL_MASK) * BITS_PER_PIXEL) |
	     ((((line->x1 + dx - 1) & PIXEL_MASK) *
	       BITS_PER_PIXEL) << 16), DESTPIXELSTRT);
	outl(BYTES_PER_LINE / 4, DESTLINELENGTH);
	outl(line->fgcolor, BLOCKMASK);
	outl(line->bgcolor, BACKGROUND);
	outl((yinc << 16) | xinc, LINEINC);
	outl(count & 0xfff, BLKDESTWIDTH);
	outl(0, BLKDESTHEIGHT);
	value |= (line->flags & LINE_BACKGROUND) ? 0x00004000 : 0;
	outl(value | PIXEL_FORMAT | 0x00000013, BLOCKCTRL);
	ep93xxfb_wait();

	return 0;
}
#endif

static int
ep93xxfb_ioctl(struct inode *inode, struct file *file,
	       unsigned int cmd, unsigned long arg, int con,
	       struct fb_info *info)
{
	struct ep93xx_cursor cursor;
	struct ep93xx_blit blit;
	struct ep93xx_fill fill;
	struct ep93xx_line line;
	unsigned long caps;

	switch (cmd) {
	case FBIO_EP93XX_GET_CAPS:
		caps = EP93XX_CAP_CURSOR;
#ifdef CONFIG_EP93XX_GRAPHICS
		caps |= EP93XX_CAP_LINE | EP93XX_CAP_FILL | EP93XX_CAP_BLIT;
#endif
		copy_to_user((void *)arg, &caps, sizeof(unsigned long));
		return 0;

	case FBIO_EP93XX_CURSOR:
		copy_from_user(&cursor, (void *)arg,
			       sizeof(struct ep93xx_cursor));
		ep93xxfb_cursor(&cursor);
		return 0;

#ifdef CONFIG_EP93XX_GRAPHICS
	case FBIO_EP93XX_LINE:
		copy_from_user(&line, (void *)arg, sizeof(struct ep93xx_line));
		return ep93xxfb_line(&line);

	case FBIO_EP93XX_FILL:
		copy_from_user(&fill, (void *)arg, sizeof(struct ep93xx_fill));
		return ep93xxfb_fill(&fill);

	case FBIO_EP93XX_BLIT:
		copy_from_user(&blit, (void *)arg, sizeof(struct ep93xx_blit));
		return ep93xxfb_blit(&blit);
#endif

	default:
		return -EFAULT;
	}
}

//=============================================================================
// CheckAdjustVar
//=============================================================================
// Check and adjust the video params in 'var'. If a value doesn't fit, round it
// up, if it's too big, return -EINVAL.
//
// Suggestion: Round up in the following order: bits_per_pixel, xres, yres,
// xres_virtual, yres_virtual, xoffset, yoffset, grayscale, bitfields,
// horizontal timing, vertical timing.
//
// Note: This is similar to generic fb call sequence
//    (decode_var, set_par, decode_var)
// in that var gets fixed if fixable and error if not.  The translating
// to parameter settings is handled inside activate_var below.
//=============================================================================
static int CheckAdjustVar(struct fb_var_screeninfo *var)
{
	DPRINTK("Entering\n");

	//
	// Just set virtual to actual resolution.
	//
	var->xres_virtual = mypar.xres;
	var->yres_virtual = mypar.yres;

	DPRINTK("check: var->bpp=%d\n", var->bits_per_pixel);
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:					//TBD need to adjust var
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:		/* through palette */
		var->red.length	= 4;	//TBD is this used? should be 8?
		//TBD what about offset?
		var->green	= var->red;
		var->blue	= var->red;
		var->transp.length = 0;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:  	/* RGB 565 */
		var->red.length    = 5;
		var->blue.length   = 5;
		var->green.length  = 6;
		var->transp.length = 0;
		var->red.offset    = 11;
		var->green.offset  = 5;
		var->blue.offset   = 0;
		var->transp.offset = 0;
		break;
#endif
	default:
		DPRINTK("Invalid var->bits_per_pixel = %d\n",var->bits_per_pixel);
		return -EINVAL;
	}
	return 0;
}

//
//	Entry:	con	valid console number
//			or -1 to indicate current console
//			(note: current console maybe default initially)
//
//		var	pointer to var structure already allocated by caller.
//
//	Action: Set pointed at var structure to fb_display[con].var or current.
//
//	Exit:	none
//
//	Note:	This function called by fb_mem.c,
static int
ep93xxfb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	//TBD I do not expect call with con=-1
	//TBD Maybe I do not really need to support this??
	if (con == -1)
		DPRINTK("get_var called with con=-1\n");

	DPRINTK("con=%d\n", con);
	if (con == -1) {

		//TBD need to set var with current settings.
		//TBD why can't we just take copy it from info??
		//ep93xxfb_get_par(&par);
		//ep93xxfb_encode_var(var, &par);

		//TBD for now current settings is always equal to
		//TBD default display settings
		//TBD Is this really complete?
		*var = global_disp.var;		// copies structure
	} else
		*var = fb_display[con].var;	// copies structure

	return 0;
}

/*
 * ep93xxfb_set_var():
 *
 *	Entry:	con 	-1 means set default display
 *			else set fb_display[con]
 *
 *		var	static var structure to define video mode
 *
 *	Action:	Settings are tweaked to get a usable display mode.
 *		Set console display values for specified console,
 *		if console valid.  Else set default display values.
 *
 *	Exit:	Returns error if cannot tweak settings to usable mode.
 */
static int
ep93xxfb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct display *display;
	int err, chgvar = 0;

	if (con == -1)
		DPRINTK("called with con=-1\n");

	//
	// Display is not passed in, but we alter as side-effect
	//
	if (con >= 0)
		//
		// Display settings for console
		//
		display = &fb_display[con];
	else
		//
		// Set default display settings
		//
		display = &global_disp;

	//
	// Check and adjust var if need be to get usable display mode, else return
	// error if impossible to adjust
	//
	if ((err = CheckAdjustVar(var)))
		return err;

	//  Update parameters
	DPRINTK("check: var->bpp=%d\n", var->bits_per_pixel);
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:
		mypar.visual = FB_VISUAL_PSEUDOCOLOR;
		mypar.palette_size = 16;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		mypar.visual = FB_VISUAL_PSEUDOCOLOR;
		mypar.palette_size = 256;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		mypar.visual = FB_VISUAL_TRUECOLOR;
		mypar.palette_size = 16;
		break;
#endif
	default:
		DPRINTK("ERROR! Bad bpp %d\n", var->bits_per_pixel);
		return -EINVAL;
	}

	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_TEST)
		return 0;
	else if (((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW) &&
			 ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NXTOPEN))
		return -EINVAL;

	if (con >= 0)
		if ((display->var.xres != var->xres) ||
		    (display->var.yres != var->yres) ||
		    (display->var.xres_virtual != var->xres_virtual) ||
		    (display->var.yres_virtual != var->yres_virtual) ||
		    (display->var.sync != var->sync) ||
		    (display->var.bits_per_pixel != var->bits_per_pixel) ||
		    (memcmp(&display->var.red, &var->red, sizeof(var->red))) ||
		    (memcmp(&display->var.green, &var->green, sizeof(var->green))) ||
		    (memcmp(&display->var.blue, &var->blue, sizeof(var->blue))))
			chgvar = 1;

	DPRINTK("display->var.xres %d\n",display->var.xres);
	DPRINTK("display->var.yres %d\n",display->var.yres);
	DPRINTK("display->var.xres_virtual %d\n",display->var.xres_virtual);
	DPRINTK("display->var.yres_virtual %d\n",display->var.yres_virtual);

	//TBD is this complete?

	display->var = *var;	// copies structure
	display->screen_base = mypar.v_screen_base;
	display->visual	= mypar.visual;
	display->type = FB_TYPE_PACKED_PIXELS;
	display->type_aux = 0;
	display->ypanstep = 0;
	display->ywrapstep = 0;
	display->line_length = display->next_line =
		(var->xres * var->bits_per_pixel) / 8;

	display->can_soft_blank	= 1;
	display->inverse = 0;

	switch (display->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:
		display->dispsw = &fbcon_cfb4;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		display->dispsw = &fbcon_cfb8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		display->dispsw = &fbcon_cfb16;
		display->dispsw_data = info->pseudo_palette;
		break;
#endif
	default:
		display->dispsw = &fbcon_dummy;
		break;
	}

	/* If the console has changed and the console has defined */
	/* a changevar function, call that function. */
	if (chgvar && info && info->changevar)
		info->changevar(con);

	/* If the current console is selected and it's not truecolor,
	 *  update the palette
	 */
	if ((con == mypar.currcon) && (mypar.visual != FB_VISUAL_TRUECOLOR)) {
		struct fb_cmap *cmap;

		//TBD what is the juggling of par about?
		//mypar = par;
		if (display->cmap.len)
			cmap = &display->cmap;
		else
			cmap = fb_default_cmap(mypar.palette_size);

		//TBD when is cmap.len set?
		fb_set_cmap(cmap, 1, ep93xxfb_setcolreg, info);
	}

	//
	// If the current console is selected, activate the new var.
	//
	if (con == mypar.currcon)
		ConfigureRaster(var, info->par);

	return 0;
}

//TBD why have this if it does nothing?

static int
ep93xxfb_updatevar(int con, struct fb_info *info)
{
	DPRINTK("entered\n");
	return 0;
}

//TBD what is relation between fix data in info and getting fix here?

static int
ep93xxfb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct display *display;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, EP93XX_NAME);

	if (con >= 0) {
		DPRINTK("Using console specific display for con=%d\n",con);
		display = &fb_display[con];  /* Display settings for console */
	}
	else
		display = &global_disp;	  /* Default display settings */
	//TBD global_disp is only valid after set_var called with -1

	fix->smem_start	 = (unsigned long)mypar.p_screen_base;
	fix->smem_len	 = mypar.screen_size;
	fix->type	 = display->type;
	fix->type_aux	 = display->type_aux;
	fix->xpanstep	 = 0;
	fix->ypanstep	 = display->ypanstep;
	fix->ywrapstep	 = display->ywrapstep;
	fix->visual	 = display->visual;
	fix->line_length = display->line_length;
	fix->accel	 = FB_ACCEL_NONE;

	return 0;
}

static void
__init ep93xxfb_init_fbinfo(void)
{
	DPRINTK("Entering.");

	//
	// Set up the display name and default font.
	//
	strcpy(fb_info.modename, TimingValues[mode].Name);
	strcpy(fb_info.fontname, default_font);

	fb_info.node = -1;
	fb_info.flags = FBINFO_FLAG_DEFAULT;
	fb_info.open = 0;

	//
	// Set up initial parameters
	//
	fb_info.var.xres = TimingValues[mode].HRes;
	fb_info.var.yres = TimingValues[mode].VRes;

	//
	// Virtual display not supported
	//
	fb_info.var.xres_virtual = fb_info.var.xres;
	fb_info.var.yres_virtual = fb_info.var.yres;

#ifdef CONFIG_FB_EP93XX_8BPP
	DPRINTK("Default framebuffer is 8bpp.");
	fb_info.var.bits_per_pixel	= 8;
	fb_info.var.red.length		= 8;
	fb_info.var.green.length	= 8;
	fb_info.var.blue.length		= 8;

	fb_info.fix.visual = FB_VISUAL_PSEUDOCOLOR;
	mypar.bits_per_pixel		= 8;
#endif // CONFIG_FB_EP93XX_8BPP
#ifdef CONFIG_FB_EP93XX_16BPP_565
	DPRINTK("Default framebuffer is 16bpp 565.");
	fb_info.var.bits_per_pixel	= 16;
	fb_info.var.red.length		= 5;
	fb_info.var.blue.length		= 5;
	fb_info.var.green.length	= 6;
	fb_info.var.transp.length	= 0;
	fb_info.var.red.offset		= 11;
	fb_info.var.green.offset	= 5;
	fb_info.var.blue.offset		= 0;
	fb_info.var.transp.offset	= 0;
	fb_info.fix.visual		= FB_VISUAL_TRUECOLOR;
	fb_info.pseudo_palette		= mypar.palette;

	mypar.bits_per_pixel = 16;
#endif // CONFIG_FB_EP93XX_16BPP

	//TBD wierd to have a flag which has meaning when call set_var
	//TBD be copied into fb_info? this is more a parameter than
	//TBD some state to keep track of.
	fb_info.var.activate		= FB_ACTIVATE_NOW;
	fb_info.var.height		= 640;			//TBD unknown
	fb_info.var.width		= 480;			//TBD unknown

	//
	// Set the default timing information.
	//
	fb_info.var.left_margin		= TimingValues[mode].HFrontPorch;
	fb_info.var.right_margin	= TimingValues[mode].HBackPorch;
	fb_info.var.upper_margin	= TimingValues[mode].VFrontPorch;
	fb_info.var.lower_margin	= TimingValues[mode].VBackPorch;
	fb_info.var.hsync_len		= TimingValues[mode].HSyncWidth;
	fb_info.var.vsync_len		= TimingValues[mode].VSyncWidth;
	fb_info.var.sync		= 0;
	fb_info.var.vmode		= FB_VMODE_NONINTERLACED;

	fb_info.fix.smem_start		= (__u32) mypar.p_screen_base;	// physical
	fb_info.fix.smem_len		= (__u32) mypar.screen_size;	//TBD in bytes
	fb_info.fix.type		= FB_TYPE_PACKED_PIXELS;
	fb_info.fix.line_length		= fb_info.var.xres_virtual *
					  fb_info.var.bits_per_pixel/8;	// stride in bytes

	fb_info.monspecs		= monspecs; // copies structure
	fb_info.fbops			= &ep93xxfb_ops;
	fb_info.screen_base 		= mypar.v_screen_base;

	fb_info.disp			= &global_disp;
	fb_info.changevar		= NULL;
	fb_info.switch_con		= ep93xxfb_switch;
	fb_info.updatevar		= ep93xxfb_updatevar;	//TBD why not NULL?
	fb_info.blank			= ep93xxfb_blank;

	//
	// Save the current TimingValues for later use in Configuration routine.
	//
	fb_info.par = &TimingValues[mode];

	mypar.screen_size		= FB_MAX_MEM_SIZE; // not a particular mode
	mypar.palette_size		= MAX_PALETTE_NUM_ENTRIES; // not a particular mode
	mypar.montype			= 1;	//TBD why not 0 since single entry?
	mypar.currcon			= 0;		//TBD is this right?

	// these are also set by set_var
	mypar.visual			= FB_VISUAL_PSEUDOCOLOR;
}

//=============================================================================
// MapFramebuffer
//=============================================================================
// Allocates the memory for the frame buffer.  This buffer is remapped into a
// non-cached, non-buffered, memory region to allow pixel writes to occur
// without flushing the cache.  Once this area is remapped, all virtual memory
// access to the video memory should occur at the new region.
//=============================================================================
static int __init MapFramebuffer(void)
{
	//
	// Make sure that we don't allocate the buffer twice.
	//
	if (mypar.p_screen_base != 0)
		return(-EINVAL);

	//
	// Allocate the Frame buffer and map it while we are at it.
	// Save the Physical address of the Frame buffer in p_screen_base.
	//
	mypar.v_screen_base = consistent_alloc(GFP_KERNEL, FB_MAPPED_MEM_SIZE,
		&mypar.p_screen_base);

	if ( mypar.v_screen_base == 0)
		return(-ENOMEM);

	return 0;
}

//=============================================================================
// ConfigureRaster():
//=============================================================================
// Configures LCD Controller based on entries in var parameter.
//
// Note: palette is setup elsewhere.
//=============================================================================
static int
ConfigureRaster(struct fb_var_screeninfo *var,
		DisplayTimingValues *pTimingValues)
{
	u_long	flags, ulVIDEOATTRIBS;
	unsigned int total, uiDevCfg;
	unsigned int uiPADDR, uiPADR;

	DPRINTK("activating\n");

	/* Disable interrupts and save status */
	local_irq_save(flags);		// disable interrupts and save flags

	DPRINTK("Configuring %dx%dx%dbpp\n",var->xres, var->yres,
		var->bits_per_pixel);

	// Setup video mode according to var values.
	// Remember to unlock total, startstop, and linecarry registers.

	// Disable the video and outputs while changing the video mode.
	outl( 0, VIDEOATTRIBS );

	if (pTimingValues->RasterConfigure)
		pTimingValues->RasterConfigure(pTimingValues);
	else {
		total = var->vsync_len +  var->upper_margin + var->yres +
		var->lower_margin - 1;

		RasterSetLocked(VLINESTOTAL, total);

		RasterSetLocked(VSYNCSTRTSTOP, total - var->lower_margin +
				((total - (var->lower_margin + var->vsync_len)) << 16));

		RasterSetLocked(VACTIVESTRTSTOP, var->yres-1 + (total << 16));

		// Reverse start/stop since N_VBLANK output
		// unblanked same as active
		RasterSetLocked(VBLANKSTRTSTOP, var->yres-1 + (total << 16));

		RasterSetLocked(VCLKSTRTSTOP, total + (total << 16));

		//
		// Now configure the Horizontal timings.
		//
		total = var->hsync_len + var->left_margin + var->xres +
			var->right_margin - 1;

		RasterSetLocked(HCLKSTOTAL,total);

		RasterSetLocked(HSYNCSTRTSTOP, total +
				((total - var->hsync_len) << 16));

		RasterSetLocked(HACTIVESTRTSTOP, total - var->hsync_len -
				var->left_margin + ((var->right_margin - 1) << 16));

		RasterSetLocked(HBLANKSTRTSTOP, total - var->hsync_len -
				var->left_margin + ((var->right_margin - 1) << 16));

		RasterSetLocked(HCLKSTRTSTOP, total + (total << 16));

		RasterSetLocked(LINECARRY, 0);

		//
		// Now that everything else is setup, enable video and outputs
		//
		// Invert Clock, enable outputs and VSYNC,HSYNC,BLANK active low
		// Invert means pixel output data changes on falling edge of clock, to
		// provide setup time for the video DAC to latch data on rising edge.
		//
		RasterSetLocked(VIDEOATTRIBS, VIDEOATTRIBS_INVCLK |
				VIDEOATTRIBS_PCLKEN | VIDEOATTRIBS_SYNCEN |
				VIDEOATTRIBS_DATAEN);
	}

	//
	// Configure the Frame Buffer size.
	//
	outl( (unsigned int)mypar.p_screen_base, VIDSCRNPAGE );
	outl( var->yres, SCRNLINES );

	//
	// Set up the Line size.
	//
	total = var->xres * var->bits_per_pixel / 32;
	outl( (total - 1), LINELENGTH );
	outl( total, VLINESTEP );

	if (var->bits_per_pixel == 8)
		//
		// 8bpp framebuffer, color LUT and convert to 18bpp on pixel bus.
		//
		outl( (0x8 | PIXELMODE_P_8BPP | PIXELMODE_C_LUT), PIXELMODE );
	else if (var->bits_per_pixel == 16)
		//
		// 16bpp framebuffer and convert to 18bpp on pixel bus.
		//
		outl( 0x8 | PIXELMODE_P_16BPP | ((PIXELMODE_C_565)<<(PIXELMODE_C_SHIFT)),
			  PIXELMODE );

	//
	// TODO this is more HACKING!
	// TODO add configurable framebuffer location and side-band option.
	//
	uiDevCfg = inl(SYSCON_DEVCFG);
	SysconSetLocked(SYSCON_DEVCFG, (uiDevCfg | SYSCON_DEVCFG_RasOnP3) );

	//
	// TODO add code to calculate this not just slam it in there.
	//
	SysconSetLocked(SYSCON_VIDDIV, pTimingValues->VDiv);

	//
	// Enable Raster and make sure that frame buffer chip select is specfied
	// since this field is not readable.
	//
	ulVIDEOATTRIBS = inl(VIDEOATTRIBS);
	RasterSetLocked(VIDEOATTRIBS, ulVIDEOATTRIBS | VIDEOATTRIBS_EN |
			(3 << VIDEOATTRIBS_SDSEL_SHIFT));

	//
	// Turn on the LCD.
	//
	if (mode == 1) {
		uiPADDR = inl(GPIO_PADDR) | 0x2;
		outl( uiPADDR, GPIO_PADDR );

		uiPADR = inl(GPIO_PADR) | 0x2;
		outl( uiPADR, GPIO_PADR );
	}

	//
	// Restore interrupt status
	//
	local_irq_restore(flags);
	return 0;
}

/*
 * ep93xxfb_blank():
 *	TBD not implemented
 */
static void
ep93xxfb_blank(int blank, struct fb_info *info)
{
}

//=============================================================================
// ep93xxfb_switch():
//=============================================================================
// Change to the specified console.  Palette and video mode are changed to the
// console's stored parameters.
//
//=============================================================================
static int ep93xxfb_switch(int con, struct fb_info *info)
{
	DPRINTK("con=%d info->modename=%s\n", con, info->modename);
	if (mypar.visual != FB_VISUAL_TRUECOLOR) {
		struct fb_cmap *cmap;
		if (mypar.currcon >= 0) {
			// Get the colormap for the selected console
			cmap = &fb_display[mypar.currcon].cmap;

			if (cmap->len)
				fb_get_cmap(cmap, 1, ep93xxfb_getcolreg, info);
		}
	}

	mypar.currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;
	DPRINTK("fb_display[%d].var.activate=%x\n", con,
		fb_display[con].var.activate);

	ep93xxfb_set_var(&fb_display[con].var, con, info);

	return 0;
}

//=============================================================================
// ep93xxfb_init(void)
//=============================================================================
//=============================================================================
int __init ep93xxfb_init(void)
{
	int ret;

	DPRINTK("Entering: ep93xxfb_init.\n");
	//
	// Allocate and map video buffer
	//
	if ((ret = MapFramebuffer()) != 0) {
		DPRINTK("Leaving: ep93xxfb_init FAILED %08x.", ret);
		return ret;
	}

	//
	// Initialize the Framebuffer Info structure.
	//
	ep93xxfb_init_fbinfo();

	//
	// Check and adjust var and set it active.
	// TBD is this okay to pass in fb_info.var or
	// TBD does it cause some aliasing problem?
	// TBD can set var fail?
	//
	ep93xxfb_set_var(&fb_info.var, -1, &fb_info);

	register_framebuffer(&fb_info);

#ifdef CONFIG_EP93XX_GRAPHICS
	outl(0x00000000, BLOCKCTRL);
	request_irq(IRQ_GRAPHICS, ep93xxfb_irq_handler, SA_INTERRUPT,
		    "graphics",NULL);
#endif

	DPRINTK("Leaving: ep93xxfb_init.");

	return 0;
}

__init int
ep93xxfb_setup(char *options)
{
	char *opt;

	if (!options || !*options)
		return 0;

	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;
		if (strncmp(opt, "font=", 5) == 0) {
			strncpy(default_font_storage, opt + 5,
				sizeof(default_font_storage));
			default_font = default_font_storage;
			continue;
		}
		if (strncmp(opt, "mode=lcd", 8) == 0) {
			mode = 1;
			continue;
		}
		if (strncmp(opt, "mode=crt", 8) == 0) {
			mode = 0;
			continue;
		}
		if (strncmp(opt, "mode=tv", 7) == 0) {
			mode = 3;
			continue;
		}
		if (strncmp(opt, "overscan=on", 11) == 0) {
			overscan = 1;
			continue;
		}
		if (strncmp(opt, "overscan=off", 12) == 0) {
			overscan = 0;
			continue;
		}

		printk (KERN_ERR "ep93xxfb: unknown parameter: %s\n", opt);
	}

	return 0;
}

//=============================================================================
// Conexant_CX25871
//=============================================================================
// Timing and screen configuration for the Conexant CX25871.
//=============================================================================
static int Conexant_CX25871(DisplayTimingValues *pTimingValues)
{
	unsigned int uiTemp;

	//
	// By name it Initializes the CX25871 to 640x480 NTSC.
	//
	InitializeCX25871For640x480NTSC();

	//
	// Disable the Raster Engine.
	//
	RasterSetLocked(VIDEOATTRIBS, 0);

	//
	// Configure the Raster to use external clock for pixel clock.
	//
	uiTemp = inl(SYSCON_DEVCFG);
	SysconSetLocked(SYSCON_DEVCFG, (uiTemp | SYSCON_DEVCFG_EXVC) );

	if (overscan) {
		//
		// Configure the Vertical Timing.
		//
		RasterSetLocked(VLINESTOTAL, 0x20c);
		RasterSetLocked(VSYNCSTRTSTOP, 0x01ff0204);
		RasterSetLocked(VBLANKSTRTSTOP, 0x000001E0);
		RasterSetLocked(VACTIVESTRTSTOP, 0x000001E0);
		RasterSetLocked(VCLKSTRTSTOP,0x07FF01E0);

		//
		// Configure the Horizontal Timing.
		//
		RasterSetLocked(HCLKSTOTAL, 0x307);
		RasterSetLocked(HSYNCSTRTSTOP, 0x02c00307);
		RasterSetLocked(HBLANKSTRTSTOP,0x00100290);
		RasterSetLocked(HACTIVESTRTSTOP,0x00100290);
		RasterSetLocked(HCLKSTRTSTOP, 0x07ff0290);
	} else {
		//
		// Configure the Vertical Timing.
		//
		RasterSetLocked(VLINESTOTAL, 0x0257);
		RasterSetLocked(VSYNCSTRTSTOP, 0x01FF022C);
		RasterSetLocked(VBLANKSTRTSTOP, 0x000001E0);
		RasterSetLocked(VACTIVESTRTSTOP, 0x000001E0);
		RasterSetLocked(VCLKSTRTSTOP,0x07FF01E0);

		//
		// Configure the Horizontal Timing.
		//
		RasterSetLocked(HCLKSTOTAL, 0x30F);
		RasterSetLocked(HSYNCSTRTSTOP, 0x02c0030f);
		RasterSetLocked(HBLANKSTRTSTOP,0x000f028f);
		RasterSetLocked(HACTIVESTRTSTOP,0x000f028f);
		RasterSetLocked(HCLKSTRTSTOP, 0x07ff028f);
	}

	RasterSetLocked(LINECARRY, 0);

	//
	// Enable the Data and Sync outputs only.
	//
	RasterSetLocked(VIDEOATTRIBS,
			VIDEOATTRIBS_DATAEN | VIDEOATTRIBS_SYNCEN);

	return(0);
}

//=============================================================================
// PhilipsLCD
//=============================================================================
// Timing and screen configuration for the Conexant CX25871.
//=============================================================================
static int PhilipsLCD(DisplayTimingValues *pTimingValues)
{
	//
	// Disable the Raster Engine.
	//
	RasterSetLocked(VIDEOATTRIBS, 0);

	//
	// Configure the Vertical Timing.
	//
	RasterSetLocked(VLINESTOTAL,	 0x0000020b);
	RasterSetLocked(VSYNCSTRTSTOP,   0x01ea01ec);
	RasterSetLocked(VBLANKSTRTSTOP,  0x020b01df);
	RasterSetLocked(VACTIVESTRTSTOP, 0x020b01df);
	RasterSetLocked(VCLKSTRTSTOP,	0x020b020b);

	//
	// Configure the Horizontal Timing.
	//
	RasterSetLocked(HCLKSTOTAL,	  0x0000031f);
	RasterSetLocked(HSYNCSTRTSTOP,   0x02bf031f);
	RasterSetLocked(HBLANKSTRTSTOP,  0x002f02af);
	RasterSetLocked(HACTIVESTRTSTOP, 0x002f02af);
	RasterSetLocked(HCLKSTRTSTOP,	0x031f031f);

	RasterSetLocked(LINECARRY, 0);

	//
	// Enable the Data and Sync outputs only.
	// According the the LCD spec we should also be setting INVCLK_EN, but
	// this makes the data on the screen look incorrect.
	//
	RasterSetLocked(VIDEOATTRIBS,
			VIDEOATTRIBS_PCLKEN | VIDEOATTRIBS_SYNCEN |
			VIDEOATTRIBS_DATAEN);

	return(0);
}

//
// Set the EE clock rate to 250kHz.
//
#define EE_DELAY_USEC			2

//
// The number of time we should read the two wire device before giving
// up.
//
#define EE_READ_TIMEOUT			100

//
// CS25871 Device Address.
//
#define CX25871_DEV_ADDRESS		0x88

//
// Register 0x32
//
#define CX25871_REGx32_AUTO_CHK		0x80
#define CX25871_REGx32_DRVS_MASK	0x60
#define CX25871_REGx32_DRVS_SHIFT	5
#define CX25871_REGx32_SETUP_HOLD	0x10
#define CX25871_REGx32_INMODE_		0x08
#define CX25871_REGx32_DATDLY_RE	0x04
#define CX25871_REGx32_OFFSET_RGB	0x02
#define CX25871_REGx32_CSC_SEL		0x01

//
// Register 0xBA
//
#define CX25871_REGxBA_SRESET		0x80
#define CX25871_REGxBA_CHECK_STAT	0x40
#define CX25871_REGxBA_SLAVER		0x20
#define CX25871_REGxBA_DACOFF		0x10
#define CX25871_REGxBA_DACDISD		0x08
#define CX25871_REGxBA_DACDISC		0x04
#define CX25871_REGxBA_DACDISB		0x02
#define CX25871_REGxBA_DACDISA		0x01

//
// Register 0xC4
//
#define CX25871_REGxC4_ESTATUS_MASK	0xC0
#define CX25871_REGxC4_ESTATUS_SHIFT	6
#define CX25871_REGxC4_ECCF2		0x20
#define CX25871_REGxC4_ECCF1		0x10
#define CX25871_REGxC4_ECCGATE		0x08
#define CX25871_REGxC4_ECBAR		0x04
#define CX25871_REGxC4_DCHROMA		0x02
#define CX25871_REGxC4_EN_OUT		0x01

//
// Register 0xC6
//
#define CX25871_REGxC6_EN_BLANKO	0x80
#define CX25871_REGxC6_EN_DOT		0x40
#define CX25871_REGxC6_FIELDI		0x20
#define CX25871_REGxC6_VSYNCI		0x10
#define CX25871_REGxC6_HSYNCI		0x08
#define CX25871_REGxC6_INMODE_MASK	0x07
#define CX25871_REGxC6_INMODE_SHIFT	0

#define GPIOG_EEDAT 2
#define GPIOG_EECLK 1
static int WriteCX25871Reg(unsigned char ucRegAddr, unsigned char ucRegValue);

//****************************************************************************
// InitializeCX25871640x480
//****************************************************************************
// Initialize the CX25871 for 640x480 NTSC output.
//
//
void InitializeCX25871For640x480NTSC(void)
{
	//
	// Perform auto-configuration
	//
	WriteCX25871Reg(0xB8, 0);

	//
	// After auto-configuration, setup pseudo-master mode BUT with
	// EN_BLANKO bit cleared
	//
	WriteCX25871Reg(0xBA, CX25871_REGxBA_SLAVER | CX25871_REGxBA_DACOFF);

	//
	// See if overscan compenstation (the default for this mode) should be
	// disabled.
	//
	if (overscan) {
		//
		// Adaptive flicker filter adjustment.
		//
		WriteCX25871Reg(0x34, 0x9B);
		WriteCX25871Reg(0x36, 0xC0);

		//
		// Standard flicker filter value.
		//
		WriteCX25871Reg(0xC8, 0x92);

		//
		// Brightness and coring for luma.
		//
		WriteCX25871Reg(0xCA, 0xd3);

		//
		// Saturation and coring for chroma.
		//
		WriteCX25871Reg(0xCC, 0xd3);

		//
		// Set luma peaking filter to 2dB gain.
		//
		WriteCX25871Reg(0xD8, 0x60);

		//
		// Set the timing.
		//
		WriteCX25871Reg(0x38, 0x00);
		WriteCX25871Reg(0x76, 0x10);
		WriteCX25871Reg(0x78, 0x80);
		WriteCX25871Reg(0x7a, 0x72);
		WriteCX25871Reg(0x7c, 0x82);
		WriteCX25871Reg(0x7e, 0x42);
		WriteCX25871Reg(0x80, 0xF5);
		WriteCX25871Reg(0x82, 0x13);
		WriteCX25871Reg(0x84, 0xF2);
		WriteCX25871Reg(0x86, 0x26);
		WriteCX25871Reg(0x88, 0x00);
		WriteCX25871Reg(0x8a, 0x08);
		WriteCX25871Reg(0x8c, 0x77);
		WriteCX25871Reg(0x8e, 0x03);
		WriteCX25871Reg(0x90, 0x0D);
		WriteCX25871Reg(0x92, 0x24);
		WriteCX25871Reg(0x94, 0xE0);
		WriteCX25871Reg(0x96, 0x06);
		WriteCX25871Reg(0x98, 0x00);
		WriteCX25871Reg(0x9a, 0x10);
		WriteCX25871Reg(0x9c, 0x68);
		WriteCX25871Reg(0x9e, 0xDA);
		WriteCX25871Reg(0xa0, 0x0A);
		WriteCX25871Reg(0xa2, 0x0A);
		WriteCX25871Reg(0xa4, 0xE5);
		WriteCX25871Reg(0xa6, 0x77);
		WriteCX25871Reg(0xa8, 0xC2);
		WriteCX25871Reg(0xaa, 0x8A);
		WriteCX25871Reg(0xac, 0x9A);
		WriteCX25871Reg(0xae, 0x12);
		WriteCX25871Reg(0xb0, 0x99);
		WriteCX25871Reg(0xb2, 0x86);
		WriteCX25871Reg(0xb4, 0x25);
		WriteCX25871Reg(0xb6, 0x00);
		WriteCX25871Reg(0xbc, 0x00);
		WriteCX25871Reg(0xbe, 0x00);
		WriteCX25871Reg(0xc0, 0x00);
		WriteCX25871Reg(0xc2, 0x00);

		//
		// Reset the timing.
		//
		mdelay(1);
		WriteCX25871Reg(0x6c, 0xc4);
	}

	//
	// Finish pseudo-master mode configuration.
	//
	WriteCX25871Reg(0xC6, (CX25871_REGxC6_INMODE_MASK & 0x3));
	WriteCX25871Reg(0xC4, CX25871_REGxC4_EN_OUT);
	WriteCX25871Reg(0x32, 0);
	WriteCX25871Reg(0xBA, CX25871_REGxBA_SLAVER );
}

//****************************************************************************
// WriteCX25871Reg
//****************************************************************************
// ucRegAddr	- CS4228 Register Address.
// usRegValue   - CS4228 Register Value.
//
// Return	 0 - Success
//			1 - Failure
//

static int WriteCX25871Reg(unsigned char ucRegAddr, unsigned char ucRegValue)
{
	unsigned long uiVal, uiDDR;
	unsigned char ucData, ucIdx, ucBit;
	unsigned long ulTimeout;

	//
	// Read the current value of the GPIO data and data direction registers.
	//
	uiVal = inl(GPIO_PGDR);
	uiDDR = inl(GPIO_PGDDR);

	//
	// If the GPIO pins have not been configured since reset, the data
	// and clock lines will be set as inputs and with data value of 0.
	// External pullup resisters are pulling them high.
	// Set them both high before configuring them as outputs.
	//
	uiVal |= (GPIOG_EEDAT | GPIOG_EECLK);
	outl( uiVal, GPIO_PGDR );

	//
	// Delay to meet the EE Interface timing specification.
	//
	udelay( EE_DELAY_USEC );

	//
	// Configure the EE data and clock lines as outputs.
	//
	uiDDR |= (GPIOG_EEDAT | GPIOG_EECLK);
	outl( uiDDR, GPIO_PGDDR );

	//
	// Delay to meet the EE Interface timing specification.
	//
	udelay( EE_DELAY_USEC );

	//
	// Drive the EE data line low.  Since the EE clock line is currently
	// high, this is the start condition.
	//
	uiVal &= ~GPIOG_EEDAT;
	outl( uiVal, GPIO_PGDR );

	//
	// Delay to meet the EE Interface timing specification.
	//
	udelay( EE_DELAY_USEC );

	//
	// Drive the EE clock line low.
	//
	uiVal &= ~GPIOG_EECLK;
	outl( uiVal, GPIO_PGDR );

	//
	// Delay to meet the EE Interface timing specification.
	//
	udelay( EE_DELAY_USEC );

	//
	// Loop through the three bytes which we will send.
	//
	for (ucIdx = 0; ucIdx < 3; ucIdx++) {
		//
		// Get the appropriate byte based on the current loop
		// iteration.
		//
		if (ucIdx == 0)
			ucData = (unsigned char)CX25871_DEV_ADDRESS;
		else if (ucIdx == 1)
			ucData = ucRegAddr;
		else
			ucData = ucRegValue;

		//
		// Loop through the 8 bits in this byte.
		//
		for (ucBit = 0; ucBit < 8; ucBit++) {
			//
			// Set the EE data line to correspond to the most
			// significant bit of the data byte.
			//
			if (ucData & 0x80)
				uiVal |= GPIOG_EEDAT;
			else
				uiVal &= ~GPIOG_EEDAT;
			outl( uiVal, GPIO_PGDR );

			//
			// Delay to meet the EE Interface timing specification.
			//
			udelay( EE_DELAY_USEC );

			//
			// Drive the EE clock line high.
			//
			outl( (uiVal | GPIOG_EECLK), GPIO_PGDR );

			//
			// Delay to meet the EE Interface timing specification.
			//
			udelay( EE_DELAY_USEC );

			//
			// Drive the EE clock line low.
			//
			outl( uiVal, GPIO_PGDR );

			//
			// Delay to meet the EE Interface timing specification.
			//
			udelay( EE_DELAY_USEC );

			//
			// Shift the data byte to the left by one bit.
			//
			ucData <<= 1;
		}

		//
		// We've sent the eight bits in this data byte, so we need to
		// wait for the acknowledge from the target.  Reconfigure the
		// EE data line as an input so we can read the acknowledge from
		// the device.
		//
		uiDDR &= ~GPIOG_EEDAT;
		outl( uiDDR, GPIO_PGDDR );

		//
		// Delay to meet the EE Interface timing specification.
		//
		udelay( EE_DELAY_USEC );

		//
		// Drive the EE clock line high.
		//
		outl( (uiVal | GPIOG_EECLK), GPIO_PGDR );

		//
		// Delay to meet the EE Interface timing specification.
		//
		udelay( EE_DELAY_USEC );

		//
		// Wait until the EE data line is pulled low by the target
		// device.
		//
		ulTimeout = 0;
		while ( inl(GPIO_PGDR) & GPIOG_EEDAT ) {
			udelay( EE_DELAY_USEC );
			ulTimeout++;
			if (ulTimeout > EE_READ_TIMEOUT )
				return 1;
		}

		//
		// Drive the EE clock line low.
		//
		outl( uiVal, GPIO_PGDR );

		//
		// Delay to meet the EE Interface timing specification.
		//
		udelay( EE_DELAY_USEC );

		//
		// Reconfigure the EE data line as an output.
		//
		uiDDR |= GPIOG_EEDAT;
		outl( uiDDR, GPIO_PGDDR );

		//
		// Delay to meet the EE Interface timing specification.
		//
		udelay( EE_DELAY_USEC );
	}

	//
	// Drive the EE data line low.
	//
	uiVal &= ~GPIOG_EEDAT;
	outl( uiVal, GPIO_PGDR );

	//
	// Delay to meet the EE Interface timing specification.
	//
	udelay( EE_DELAY_USEC );

	//
	// Drive the EE clock line high.
	//
	uiVal |= GPIOG_EECLK;
	outl( uiVal, GPIO_PGDR );

	//
	// Delay to meet the EE Interface timing specification.
	//
	udelay( EE_DELAY_USEC );

	//
	// Drive the EE data line high.  Since the EE clock line is currently
	// high, this is the stop condition.
	//
	uiVal |= GPIOG_EEDAT;
	outl( uiVal, GPIO_PGDR );

	//
	// Delay to meet the EE Interface timing specification.
	//
	udelay( EE_DELAY_USEC );

	return 0;
}
