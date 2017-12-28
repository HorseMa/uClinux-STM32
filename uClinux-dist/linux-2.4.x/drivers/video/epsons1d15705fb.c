/**
 * linux/drivers/video/epsons1d15705fb.c
 *
 * Copyright (c) 2004 by Stefan Macher <stefan.macher@sympat.de>
 *
 * DESCRIPTION:
 * ------------
 * Linux framebuffer driver for the EPSON S1S15705 (old name SED1575).
 * It has actually no terminal support (may come later). Because of the very slow
 * controller interface, the driver allocates a framebuffer memory within normal
 * memory that is accessed by the applications when mapping, writing or reading from
 * the framebuffer device. So the application has to call the special I/O control
 * FB_S1D15705_FLUSH that is defined in epsons1d15705fb.h whenever it wants to update the
 * screen content. Then the internal driver framebuffer is copied to the controller
 * framebuffer.
 *
 * LICENSE:
 * --------
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
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
#include <asm/uaccess.h>
#include "linux/config.h"

#include <video/fbcon.h>

#include <linux/epsons1d15705fb.h>

/* Internally needed defines */
#define FB_S1D15705_PWR_SAVE_OFF     0
#define FB_S1D15705_PWR_SAVE_STANDBY 1
#define FB_S1D15705_PWR_SAVE_SLEEP   2

#define FB_S1D15705_CONTRAST_MIN     0
#define FB_S1D15705_CONTRAST_MAX   127
#define FB_S1D15705_CONTRAST_DEF    79

/** Specific EPSON S1D15705 framebuffer info */
struct epsons1d15705fb_info {
	struct fb_info_gen gen;
	unsigned long fb_virt_start; /**< Where the driver internal fb starts */
	unsigned long fb_size; /**< Size of the framebuffer in bytes */
	unsigned long fb_phys; /**< Where to find the real controller fb */
};

static void _epsons1d15705fb_init(void);
static void _epsons1d15705_set_power_save_state(int mode);
static void _epsons1d15705_copy_fb(void);
static void _epsons1d15705_set_invertion(int);

/**
 * Controller configuration parameters
 *
 * See datasheet for more infos
 */

/**
 * 1 means perform no LCD initialization at startup
 * 0 means perform complete LCD initialization at startup
 */
u8 no_init = 0;

/**
 * Width of the LCD in pixels
 * Legal values: 1...168
 */
u32 width = 160;

/**
 * Height of the LCD in pixels
 * Legal values: 1..64
 */
u32 height = 64;

/**
 * Display turned on or off
 * Legal values: 0, 1
 */
u8 display_on_off = 1;

/**
 * See command 'Display Start Line Set' in datasheet
 * Legal values: 0..63
 */
u8 start_line = 0;

/**
 * See cmd 'Display Normal Rotation/Reversal' in datasheet
 * Legal values: 0, 1
 */
u8 inverted = 0;

/**
 * See cmd 'Display All Lightning ON/OFF' in datasheet
 * Legal values: 0, 1
 */
u8 all_lightning = 0;

/**
 * See cmd 'LCD Bias Set' in datasheet
 * Legal values: 0 -> 1/9
 *               1 -> 1/7
 */
u8 lcd_bias = 0;

/**
 * See cmd 'Common Output State Selection' in datasheet
 * Legal values: 0 -> Normal rotation
 *               1 -> Reversal
 */
u8 com_rotation = 0;

/**
 * See  cmd 'Power Control Set' in datasheet
 * Legal values: 0 -> off
 *               1 -> on
 */
u8 boosting_on_off = 0;

/**
 * See  cmd 'Power Control Set' in datasheet
 * Legal values: 0 -> off
 *               1 -> on
 */
u8 v_adjusting_on_off = 0;

/**
 * See  cmd 'Power Control Set' in datasheet
 * Legal values: 0 -> off
 *               1 -> on
 */
u8 v_f_circuit_on_off = 0;

/**
 * See  cmd 'V5 Voltage Adjusting Built-in Resistance Ratio Set'
 * in datasheet
 * Legal values: 0..7
 */
u8 v5_ratio = 0;

/**
 * See  cmd 'Electronic Control' in datasheet
 * Legal values: 0..63
 */
u8 elec_volume = 0;

/**
 * See cmd 'Power Save' and 'Power Save Reset'
 * Legal values: 0 (FB_S1D15705_PWR_SAVE_OFF)    -> No power save
 *               1 (FB_S1D15705_PWR_SAVE_STANDBY)-> Standby
 *               2 (FB_S1D15705_PWR_SAVE_SLEEP)  -> Sleep
 */
u8 power_save_state = 0;

/** Switch functions */
struct fbgen_hwswitch epsons1d15705fb_switch;

/** Controller ops */
static struct fb_ops epsons1d15705fb_ops;

/** Framebuffer info struct */
static struct epsons1d15705fb_info fb_info;

#if MODULE
MODULE_PARM(no_init, "i");
MODULE_PARM_DESC(no_init, "Setting to one will disable the LCD initialization at startup");

MODULE_PARM(width, "i");
MODULE_PARM_DESC(width, "Width of the LCD in pixels");

MODULE_PARM(height, "i");
MODULE_PARM_DESC(height, "Height of the LCD in pixels");

MODULE_PARM(display_on_off, "i");
MODULE_PARM_DESC(display_on_off, "Initial state of display");

MODULE_PARM(start_line, "i");
MODULE_PARM_DESC(start_line, "Start line of controller output");

MODULE_PARM(inverted, "i");
MODULE_PARM_DESC(inverted, "Setting to one will invert the display content");

MODULE_PARM(lcd_bias, "i");
MODULE_PARM_DESC(lcd_bias, "Settig to one -> 1/9; setting to zero -> 1/7");

MODULE_PARM(com_rotation, "i");
MODULE_PARM_DESC(com_rotation, "Setting to one will reverse the common output state");

MODULE_PARM(boosting_on_off, "i");
MODULE_PARM_DESC(boosting_on_off, "Setting to one will turn the boosting circuit on");

MODULE_PARM(v_adjusting_on_off, "i");
MODULE_PARM_DESC(v_adjusting_on_off, "Setting to one will turn the V adjusting circuit on");

MODULE_PARM(v_f_circuit_on_off, "i");
MODULE_PARM_DESC(v_f_circuit_on_off, "Setting to one will turn the V/F circuit on");

MODULE_PARM(v5_ratio, "i");
MODULE_PARM_DESC(v5_ratio, "V5 Voltage Adjusting Built-In Resistance Ratio Set");

MODULE_PARM(elec_volume, "i");
MODULE_PARM_DESC(elec_volume, "Setting of Electronic volume register");

MODULE_AUTHOR("Stefan Macher <macher@sympat.de>");
MODULE_DESCRIPTION("EPSON S1D15705 (SED1575) LCD linux framebuffer device driver\n\
                    For parameter descriptions refer to the datasheet \
		    available from EPSON");
#endif

/** Framebuffer initialization function */
int epsons1d15705fb_init(void);

/** Framebuffer setup function */
int epsons1d15705fb_setup(char *);

/**
 * Detect function not supported as the controller provides
 * no way to do this.
 */
static void epsons1d15705fb_detect(void)
{
}

/**
 * Encodes the fix part of the framebuffer info
 *
 * @param fix The structure to fill
 * @param _par epsons1d15705fb_par structure to use as input to encode fix
 * @param _info epsons1d15705fb_info structure to use as input to encode fix
 * @return 0 in case of success; an error value otherwise
 */
static int epsons1d15705fb_encode_fix(struct fb_fix_screeninfo *fix, const void *_par,
                                      struct fb_info_gen *_info)
{
	/*
	 *  This function should fill in the 'fix' structure based on the values
	 *  in the `par' structure.
	 */
	struct epsons1d15705fb_info *info = (struct epsons1d15705fb_info *) _info;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	strncpy(fix->id, "EPSON S1D15705", 16);
	fix->smem_start = info->fb_phys;
	fix->smem_len = info->fb_size;
	fix->type = FB_TYPE_PACKED_PIXELS; /* I am not sure; we have only mono */
	fix->type_aux = 0;
	fix->visual = FB_VISUAL_MONO01;
	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->ywrapstep = 0;
	fix->line_length = width;

	return 0;
}

/** Decodes the variable part of the framebuffer info
 *
 * @param var fb_var_screeninfo structure to fill
 * @param par epsons1d15705fb_par structure to use as input to encode var
 * @param info epsons1d15705fb_info structure to use as input to encode var
 * @return 0 in case of success; an error value otherwise
 */
static int epsons1d15705fb_decode_var(const struct fb_var_screeninfo *var, void *par,
                                      struct fb_info_gen *info)
{
    /* Here we would normally change resolution, color depth, ...
     * But there are no options here that could be changed with this
     * controller. So we only check if the configuration is valid
     * and return -EINVAL in the case the caller wants to change
     * anything.
     */

    /* It is not allowed to change the resolution at runtime */
    if ( (var->xres != width) || (var->yres != height) ) {
        printk("EPSON S1D15705 fb: Change of resolution at runtime not possible\n");
        return -EINVAL;
    }
    /* We do not support virtual resolutions at the moment
     * but the controller would support for example panning.
     * Maybe we will add this later
     */
    if ( (var->yres_virtual != var->yres) || (var->xres_virtual != var->xres) ) {
        printk("EPSON S1D15705 fb: Virtual screens not supported\n");
        return -EINVAL;
    }
    /* The controller only supports 1 bit per pixel */
    if ( var->bits_per_pixel != 1 ) {
        printk("EPSON S1D15705 fb: Only 1bpp Supported\n");
        return -EINVAL;
    }
    return 0;
}

/** Encodes the variable part of the framebuffer info
 *
 * @param var fb_var_screeninfo structure to fill
 * @param _par epsons1d15705fb_par structure to use as input to encode fix
 * @param info epsons1d15705fb_info structure to use as input to encode fix
 * @return 0 in case of success; an error value otherwise
 */
static int epsons1d15705fb_encode_var(struct fb_var_screeninfo *var, const void *_par,
                                      struct fb_info_gen *info)
{
	/*
	 *  Fill the 'var' structure based on epsons1d15705_config
	 */
	memset(var, 0, sizeof(struct fb_var_screeninfo));
	var->xres = var->xres_virtual = width;
	var->yres = var->yres_virtual = height;
	var->bits_per_pixel = 1;

	return 0;
}

/** Fills the epsons1d15705fb_par structure with current_par
 *
 * As we have no par here is nothing to do
 *
 * @param _par Pointer to a epsons1d15705fb_par structure that will be filled
 * @param info Pointer to a framebuffer generic info
 */
static void epsons1d15705fb_get_par(void *_par, struct fb_info_gen *info)
{
}

/** As we have no par here is nothing to do
 */
static void epsons1d15705fb_set_par(const void *par, struct fb_info_gen *info)
{
}

/**
 * Returns one color pallette entry
 *
 * Actually the hardware palette consists of inverting / not inverting
 * the display content as we only have black/white.
 *
 * @param regno the entry index to get
 * @param red A pointer to write the red value to
 * @param green A pointer to write the green value to
 * @param blue A pointer to write the blue value to
 * @param transp A pointer to write the transparency value to
 * @param _info The framebuffer infos
 * @return 0 on success; an error value otherwise
 */
static int epsons1d15705fb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
                                unsigned *blue, unsigned *transp,
                                struct fb_info *_info)
{
	/*
	 *  Read a single color register and split it into colors/transparent.
	 *  The color values must have a 16 bit magnitude.
	 *  Return != 0 for invalid regno.
	 */
	if (regno > 1)
	{
		printk("EPSON S1D15705: Requested palette entry %d not valid", regno);
		return -EINVAL;
	}

	if(inverted != regno)
		*red = *green = *blue =0xFFFF;
	else
		*red = *green = *blue = 0x0000;
	*transp = 0x00;
	return 0;
}

/**
 * Sets one color pallette entry
 *
 * Actually the hardware palette consists of inverting / not inverting
 * the display content as we only have black/white.
 *
 * @param regno the entry index to get
 * @param red The red value of the new pallette entry
 * @param green The green value of the new pallette entry
 * @param blue The blue value of the new pallette entry
 * @param transp The transparency value of the new pallette entry
 * @param _info The framebuffer infos
 * @return 0 on success; an error value otherwise
 */
static int epsons1d15705fb_setcolreg(unsigned regno, unsigned red, unsigned green,
                                     unsigned blue, unsigned transp,
                                     struct fb_info *_info)
{
	/*
	 *  Set a single color register. The values supplied have a 16 bit
	 *  magnitude.
	 *  Return != 0 for invalid regno.
	 */
	if(regno > 1) {
		printk("EPSON S1D15705: Requested palette entry %d not valid", regno);
		return -EINVAL;
	}

	if((blue == 0x00) && (green == 0x00) && (blue == 0x00) ) {
		if (regno == 0) {
			inverted = 0;
		} else {
			inverted = 1;
		}
	} else if(regno == 1) {
		inverted = 0;
	} else {
		inverted = 1;
	}

	_epsons1d15705_set_invertion(inverted);
	return 0;
}

/**
 * Panning is not yet supported
 */
static int epsons1d15705fb_pan_display(const struct fb_var_screeninfo *var,
                                  struct fb_info_gen *info)
{
	/*
	 *  Pan (or wrap, depending on the `vmode' field) the display using the
	 *  `xoffset' and `yoffset' fields of the `var' structure.
	 *  If the values don't fit, return -EINVAL.
	 */
	return -EINVAL;
}

/**
 * Dis-/enables the video output. Used only internally.
 *
 * @param blank_mode 0 -> video output is enabled
 *                   3 -> standby mode
 *                   4 -> sleep mode
 * @param info Pointer the framebuffer infos
 * @return error value
 */
static int epsons1d15705fb_blank(int blank_mode, struct fb_info_gen *info)
{
	switch(blank_mode)
	{
		case 0:
			_epsons1d15705_set_power_save_state(FB_S1D15705_PWR_SAVE_OFF);
			break;
		case 3:
			_epsons1d15705_set_power_save_state(FB_S1D15705_PWR_SAVE_STANDBY);
			break;
		case 4:
			_epsons1d15705_set_power_save_state(FB_S1D15705_PWR_SAVE_SLEEP);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

/** Dis-/enables the video output.
 *
 * @param blank_mode 0 -> video output is enabled
 *                   3 -> standby mode
 *                   4 -> sleep mode
 * @param info Pointer the framebuffer infos
 */
static void epsons1d15705fb_gen_blank(int blank_mode, struct fb_info *info)
{
	epsons1d15705fb_blank(blank_mode, (struct fb_info_gen*)info);
}

/* ------------ Interfaces to hardware functions ------------ */

/** The framebuffer hardware operations table
 */
struct fbgen_hwswitch s1d15705fb_switch = {
	epsons1d15705fb_detect,
	epsons1d15705fb_encode_fix,
	epsons1d15705fb_decode_var,
	epsons1d15705fb_encode_var,
	epsons1d15705fb_get_par,
	epsons1d15705fb_set_par,
	epsons1d15705fb_getcolreg,
	epsons1d15705fb_setcolreg,
	epsons1d15705fb_pan_display,
	epsons1d15705fb_blank,
	NULL
};

/* ------------ Hardware Independent Functions ------------ */

/** Initialization function. Called at startup / load of module
 */
int __init epsons1d15705fb_init(void)
{
	printk("EPSON S1D15705 framebuffer init\n(c) sympat GmbH Stefan Macher\n");

	/* reset our epsons1d15705_fb_info */
	memset(&fb_info.gen, 0, sizeof(fb_info.gen));

	fb_info.gen.fbhw = &s1d15705fb_switch;
	fb_info.gen.parsize = 0;  /* we have no par */
	snprintf(fb_info.gen.info.modename, 40, "%dx%dx1", width, height);
	fb_info.gen.info.node = -1;
	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.switch_con = NULL;
	fb_info.gen.info.updatevar = NULL; /* ???? */
	fb_info.gen.info.blank = &epsons1d15705fb_gen_blank;
	fb_info.gen.info.fbops = &epsons1d15705fb_ops;
	fb_info.gen.info.disp = NULL; /* not needed ?? */

	/*
	 * Allocate LCD framebuffer from system memory
	 */

	/* size of framebuffer is ((width*height)/8) */
	fb_info.fb_size = (width * height) >> 3;

	/* allocate the memory */
	fb_info.fb_phys = (unsigned long)kmalloc(fb_info.fb_size, GFP_KERNEL);
	if(!fb_info.fb_phys) {
		printk("EPSON S1D15705 framebuffer driver error: Could not allocate framebuffer memory\n");
		return -ENOMEM;
	}
	else
		printk("EPSON S1D15705: Allocated the needed framebuffer memory\n");
	fb_info.fb_virt_start = fb_info.fb_phys;

	/* fill the var element of fb_info.gen.info */
	epsons1d15705fb_encode_var(&(fb_info.gen.info.var), NULL, NULL);

	/* fill the fix element of fb_info.gen.info */
	epsons1d15705fb_encode_fix(&fb_info.gen.info.fix, NULL, &fb_info.gen);

	if (register_framebuffer(&fb_info.gen.info) < 0)
		return -EINVAL;
	printk(KERN_INFO "fb%d: %s frame buffer device\n", GET_FB_IDX(fb_info.gen.info.node),
		fb_info.gen.info.modename);

	/*  clear framebuffer memory */
	memset((void*)fb_info.fb_phys, 0, fb_info.fb_size);

	/* init the lcd controller */
	_epsons1d15705fb_init();

	printk("EPSON S1D15705 framebuffer driver: Init successful\n");
	return 0;
}

/** Unregisters the framebuffer
 */
void epsons1d15705fb_cleanup(struct fb_info *info)
{
	kfree((void*)fb_info.fb_phys);
	unregister_framebuffer(info);
}

/** Framebuffer setup function in case of no module
 *
 * \param Kernel command line
 */
int __init epsons1d15705fb_setup(char *options)
{
	return 0;
}

/** Open function does actually nothing
 */
static int epsons1d15705fb_open(struct fb_info *info, int user)
{
	return 0;
}

/** Release function does actually nothing
 */
static int epsons1d15705fb_release(struct fb_info *info, int user)
{
	return 0;
}

/** IO control function needed for flushing
  */
static int epsons1d15705_ioctl(struct inode *inode, struct file *file,
                               unsigned int cmd, unsigned long arg,
                               int con, struct fb_info *info)
{
	int contrast;
	struct lcdcontrast contrastval;

	switch(cmd) {
		case FB_S1D15705_FLUSH:
			_epsons1d15705_copy_fb();
			return 0;

		case FB_S1D15705_SET_CONTRAST:
			copy_from_user(&contrast, (int *)arg, sizeof(int));
			if(contrast > FB_S1D15705_CONTRAST_MAX)
				contrast = FB_S1D15705_CONTRAST_MAX;
			if(contrast < FB_S1D15705_CONTRAST_MIN)
				contrast = FB_S1D15705_CONTRAST_MIN;

			/* bit6 = voltage-range */
			outb(0x23 + (contrast >> 6), LCD_COMMAND_ADR);

			/* bit0..5 = contrast voltage */
			outb(0x81, LCD_COMMAND_ADR);
			outb(contrast & 0x3F, LCD_COMMAND_ADR);
			return 0;

		/* contrast limits set in defines above */
		case FB_S1D15705_GET_CONTRAST_LIM:
			copy_from_user(&contrastval,
				(struct lcdcontrast *)arg,
				sizeof(struct lcdcontrast));
			contrastval.min = FB_S1D15705_CONTRAST_MIN;
			contrastval.max = FB_S1D15705_CONTRAST_MAX;
			contrastval.def = FB_S1D15705_CONTRAST_DEF;
			copy_to_user((struct lcdcontrast *)arg,
				&contrastval, sizeof(struct lcdcontrast));
			return 0;

		default:
			return -ENOTTY;
	}
}

static unsigned long
epsons1d15705fb_get_unmapped_area(struct file *file,
		     unsigned long addr, unsigned long len,
		     unsigned long pgoff, unsigned long flags)
{
	unsigned long off = pgoff << PAGE_SHIFT;

	if (off < fb_info.fb_size)
		if ((len >> 3) <= fb_info.fb_size - off)
		  	return virt_to_phys(fb_info.fb_virt_start + off);

	return -EINVAL;
}

/** The framebuffer operations table
 *
 * Most just use the generic functions
 */
static struct fb_ops epsons1d15705fb_ops = {
	owner:	 	      THIS_MODULE,
	fb_open:	      epsons1d15705fb_open,
	fb_release:	      epsons1d15705fb_release,
	fb_get_fix:	      fbgen_get_fix,
	fb_get_var:	      fbgen_get_var,
	fb_set_var:	      fbgen_set_var,
	fb_get_cmap:	      fbgen_get_cmap,
	fb_set_cmap:	      fbgen_set_cmap,
	fb_pan_display:	      fbgen_pan_display,
	fb_ioctl:	      epsons1d15705_ioctl,
	get_fb_unmapped_area: epsons1d15705fb_get_unmapped_area,
};

/***********************************************************
 *
 * Functions that really access the hardware
 *
 ***********************************************************/

/**
 * Initializes the LCD controller according to settings in
 * epsons1d15705_config
 */
static void _epsons1d15705fb_init()
{
}

/**
 * Switch power saving mode
 *
 * @param mode A FB_S1D15705_PWR_SAVE mode
 */
static void _epsons1d15705_set_power_save_state(int mode)
{
}

/**
 * Copies the framebuffer to the controller
 */
static void _epsons1d15705_copy_fb()
{
	volatile unsigned int pageCnt, colCnt;
	volatile unsigned int i = 0;
	volatile unsigned int colstart, startpage, endpage;
	char *framebuffer_base = NULL;

	startpage = 0;
	endpage = NUM_PAGES;
	colstart = COL_START;

	framebuffer_base = (void*)fb_info.fb_phys;

	for (pageCnt = startpage; pageCnt < endpage; pageCnt++) {
		/* set page address */
		outb(0xB0 + pageCnt, LCD_COMMAND_ADR);

		/* set column address set to col */
		/* high nibble of the column adr. */
		outb( 0x10 | (colstart >> 4), LCD_COMMAND_ADR);

		/* low nibble of the column adr. */
		outb( colstart&0xF, LCD_COMMAND_ADR);

		for (colCnt = colstart; colCnt < (COL_START + DISPLAY_WIDTH); colCnt++) {
			/*  write into the display memory */
	        	outb( *(framebuffer_base + i), LCD_DATA_ADR);
			i++;
		}

		/* enable the automatic address count in case of read access */
		outb( 0xEE, LCD_COMMAND_ADR);
	}
}

/**
 * Enable / Disables invertion
 *
 * @param inv 0 -> Disable invertion; 1 -> enable invertion
 */
static void _epsons1d15705_set_invertion(int inv)
{
}

/* ------------------------------------------------------------------------- */

/*
 *  Modularization
 */

#ifdef MODULE
MODULE_LICENSE("GPL");
int init_module(void)
{
	return epsons1d15705fb_init();
}

void cleanup_module(void)
{
	epsons1d15705fb_cleanup(&fb_info.gen.info);
}
#endif /* MODULE */

module_init(epsons1d15705fb_init);
