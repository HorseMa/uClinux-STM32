/*
 * linux/drivers/video/s3c44b0xfb.c
 *
 * Support for the S3C44B0X LCD/CRT controller as framebuffer device
 *
 * Copyright (c) 2003 Stefan Macher <stefan.macher@sympat.de>
 * Copyright (c) 2003 Alexander Assel <alexander.assel@sympat.de>
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
#include <asm/uaccess.h>
#include "linux/config.h"

#include <video/fbcon.h>

#include "s3c44b0xfb_intern.h"
#include "linux/s3c44b0xfb.h" /* ioctl defines */
#include "asm-armnommu/arch-S3C44B0X/s3c44b0x.h"

/*---------------------------------*/
/* global parameters               */
/*---------------------------------*/
static int s3c44b0x_lcd_index=0; /*!< Index within panels; will be set according to panel_name */

#if MODULE /* in case of module the panel name is set as module parameter */
static u8 panel_valid = 0; /*!< Indicates if the s3c44b0x_lcd_index is valid */
static unsigned char *panel_name = NULL; /*!< The name of the panel that is connected */
#else /* FIXME this should be similar to module but as linux boot line parameter */
static u8 panel_valid = 1;
/* static unsigned char *panel_name = "LCDBA7T11M4_320x240x8"; */
/* static unsigned char *panel_name = "WM-G3224Y-1NFWbV1_320x240x2"; */
static unsigned char *panel_name = NULL;
#endif

static u8 bootloader = 1;
struct fbgen_hwswitch s3c44b0xfb_switch;
static struct fb_ops s3c44b0xfb_ops;

/*---------------------------------*/
/* parameter declaration           */
/*---------------------------------*/

#if MODULE
MODULE_PARM(panel_name, "s");
MODULE_PARM_DESC(panel_name, "Name of the connected panel\nknown panels are: LCDBA7T11M4_320x240x8, WM-G3224Y-1NFWbV1_320x240x2");
MODULE_PARM(bootloader, "b");
MODULE_PARM_DESC(bootloader, "Set it to 1 if you want the device driver to use the parameters that has the bootloader configured\n");
MODULE_AUTHOR("Alexander Assel  <assel@sympat.de> Stefan Macher <macher@sympat.de>");
MODULE_DESCRIPTION("S3C44B0X LCD framebuffer device driver");
#endif
/*--------------------------------*/

/**************************************
 * definition of the known displays
 **************************************/
/*! \brief A list of known LCD panles, to which parameters are stored
 *
 * If you add some new panels in the struct change also the
 * module description on the end of this file
 */
struct known_lcd_panels panels[] = { {
	"EMPTY_DISPLAY",
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
},
{
	"LCDBA7T11M4_320x240x8", /*!< holds the lcd panel name */
	15, /*!< clkval */ /* FIXME this is based on a cpu clock of 60,75 MHz */
	16, /*!< Determine the VLINE pulses high level width [clocks] valid values 4, 8, 12 and 16 */
	16, /*!< Determine the delay between VLINE and VCLOCK [clocks] valid values 4, 8, 12 and 16 */
	0, /*!< Determine toggle rate of VM 0 -> each frame 1 -> defined by MVAL */
	2, /*!< display mode is 8 bit single scan */
	0, /*!< controls the polarity of VCLOCK active edge 0 -> falling edge 1 -> rising edge */
	0, /*!< indicate the line pulse polarity 0 -> normal 1 -> inverted */
	0, /*!< indicate the frame pulse polarity 0 -> normal 1 -> inverted */
	0, /*!< indicates the video data polarity 0 -> normal 1 -> inverted */
	10, /*!< indicates the blank time in one horizontal line duration */
	0, /*!< LCD self refrash mode enable */
#ifdef CONFIG_CPU_BIG_ENDIAN
	0, /*!< byte swap control for big endian */
#else
	1, /*!< byte swap control for little endian */
#endif
	0x0D, /*!< defines the rate at which the vm signal will toggle */
	320, /*!< display width in pixels */
	240, /*!< display height in pixels */
	8, /*!< bits per pixel valid is 1 (mono), 2 (gray), 4 (gray) and 8 (color) */
	119, /*!< hozval */
	239, /*!< lineval */
	0, /*(u32*)S3C44B0X_PDATC*/ /*!< port register for enabling the display */
	(1<<8), /*!< mask port c pin 8 */
	(1<<8), /*!< set port c pin 8 to high */
	0,/*(u32*)S3C44B0X_PCONC,*/ /*!< port control register for setting PORTC.8 to output */
	(3<<16), /*!< mask port c pin8 control */
	(1<<16) /*!< set port c pin 8 control to output */
},
{
	"WM-G3224Y-1NFWbV1_320x240x2", /*!< holds the lcd panel name */
	15, /*!< clkval */ /* FIXME this is based on a cpu clock of 60,75 MHz */
	16, /*!< Determine the VLINE pulses high level width [clocks] valid values 4, 8, 12 and 16 */
	16, /*!< Determine the delay between VLINE and VCLOCK [clocks] valid values 4, 8, 12 and 16 WDLY */
	0, /*!< Determine toggle rate of VM 0 -> each frame 1 -> defined by MVAL */
	1, /*!< display mode is 8 bit single scan */
	0, /*!< controls the polarity of VCLOCK active edge 0 -> falling edge 1 -> rising edge */
	0, /*!< indicate the line pulse polarity 0 -> normal 1 -> inverted */
	0, /*!< indicate the frame pulse polarity 0 -> normal 1 -> inverted */
	0, /*!< indicates the video data polarity 0 -> normal 1 -> inverted */
	10, /*!< indicates the blank time in one horizontal line duration */
	0, /*!< LCD self refrash mode enable */
#ifdef CONFIG_CPU_BIG_ENDIAN
	0, /*!< byte swap control for big endian */
#else
	1, /*!< byte swap control for little endian */
#endif
	0x0D, /*!< defines the rate at which the vm signal will toggle */
	320, /*!< display width in pixels */
	240, /*!< display height in pixels */
	2, /*!< bits per pixel valid is 1 (mono), 2 (gray), 4 (gray) and 8 (color) */
	79, /*!< hozval */
	239, /*!< lineval */
	0, /*!< port register for enabling the display */
	(1<<8), /*!< mask port c pin 8 */
	(1<<8), /*!< set port c pin 8 to high */
	0, /*!< port control register for setting PORTC.8 to output */
	(3<<16), /*!< mask port c pin8 control */
	(1<<16) /*!< set port c pin 8 control to output */
}
};

static struct s3c44b0xfb_info fb_info; /*!< Framebuffer info struct */
static struct s3c44b0xfb_par current_par;
static int current_par_valid = 0;

/** Framebuffer initialization function */
int s3c44b0xfb_init(void);

/** Framebuffer setup function */
int s3c44b0xfb_setup(char*);

/* ------------------- chipset specific functions -------------------------- */

/**
 * Detect function, not supported
 */
static void s3c44b0xfb_detect(void)
{
        /*
	 * Yeh, well, we're not going to change any settings so we're
	 * always stuck with the default ...
	 */
}

/**
 * Encodes the fix part of the framebuffer info
 *
 * \param fix The structure to fill
 * \param _par s3c44b0xfb_par structure to use as input to encode fix
 * \param _info s3c44b0xfb_info structure to use as input to encode fix
 * \return 0 in case of success; an error value otherwise
 */
static int s3c44b0xfb_encode_fix(struct fb_fix_screeninfo *fix, const void *_par,
			       struct fb_info_gen *_info)
{
	/*
	 *  This function should fill in the 'fix' structure based on the values
	 *  in the `par' structure.
	 */
	struct s3c44b0xfb_info *info = (struct s3c44b0xfb_info *) _info;
        struct s3c44b0xfb_par *par = (struct s3c44b0xfb_par *) _par;
	struct fb_var_screeninfo *var = &par->var;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	fix->smem_start = info->fb_phys;
	fix->smem_len = info->fb_size;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	if(var->bits_per_pixel == 2)
		fix->visual = FB_VISUAL_TRUECOLOR;
	else if(var->bits_per_pixel == 4)
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fix->visual = (var->bits_per_pixel == 1) ?
			FB_VISUAL_MONO01 : FB_VISUAL_TRUECOLOR;

	fix->ywrapstep = 0;
	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->line_length = current_par.line_length;
	return 0;

}

/*
 * Decodes the variable part of the framebuffer info
 * Actually only the settings from the panels array are used not the
 * input parameters
 *
 * \param var fb_var_screeninfo structure to fill
 * \param par s3c44b0xfb_par structure to use as input to encode var
 * \param info s3c44b0xfb_info structure to use as input to encode var
 * \return 0 in case of success; an error value otherwise
 */
static int s3c44b0xfb_decode_var(const struct fb_var_screeninfo *var, void *par,
				 struct fb_info_gen *info)
{
	/*
	 *  Get the video params out of 'var'. If a value doesn't fit, round up,
	 *  if it's too big, return -EINVAL.
	 *
	 *  Suggestion: Round up in the following order: bits_per_pixel, xres,
	 *  yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
	 *  bitfields, horizontal timing, vertical timing.
	 */
	struct known_lcd_panels *p_lcd = &panels[s3c44b0x_lcd_index];
	if (var->xres != p_lcd->width ||
	    var->yres != p_lcd->height)
	{
		return -EINVAL;
	}

	if (var->bits_per_pixel != p_lcd->bpp) 
	{
		return -EINVAL;
	}
	return 0;

}

/**
 * Encodes the variable part of the framebuffer info
 *
 * \param var fb_var_screeninfo structure to fill
 * \param _par s3c44b0xfb_par structure to use as input to encode fix
 * \param info s3c44b0xfb_info structure to use as input to encode fix
 * \return 0 in case of success; an error value otherwise
 */
static int s3c44b0xfb_encode_var(struct fb_var_screeninfo *var, const void *_par,
                                 struct fb_info_gen *info)
{
	/*
	 *  Fill the 'var' structure based on the values in 'par' and maybe other
	 *  values read out of the hardware.
	 */
	struct s3c44b0xfb_par *par = (struct s3c44b0xfb_par*)_par;

	memcpy(var, &par->var, sizeof(struct fb_var_screeninfo));

	return 0;
}

/**
 * Fills the sc344b0xfb_par structure with current_par
 *
 * \param _par Pointer to a s3c44b0xfb_par structure that will be filled
 * \param info Pointer to a framebuffer generic info
 */
static void s3c44b0xfb_get_par(void *_par, struct fb_info_gen *info)
{
	/*
	 *  Fill the hardware's 'par' structure.
	 */

	struct s3c44b0xfb_par *par = (struct s3c44b0xfb_par*)_par;
	if (current_par_valid)
		memcpy(par, &current_par, sizeof(struct s3c44b0xfb_par));
}

/** Does nothing - a change is not possible */
static void s3c44b0xfb_set_par(const void *par, struct fb_info_gen *info)
{
    /*
     *  Set the hardware according to 'par'.
     */
    /* nothing to do: we don't change any settings */
}

/**
 * Returns one color pallette entry
 *
 * \param regno the entry index to get
 * \param red A pointer to write the red value to
 * \param green A pointer to write the green value to
 * \param blue A pointer to write the blue value to
 * \param transp A pointer to write the transparency value to
 * \param _info The framebuffer infos
 * \return 0 on success; an error value otherwise
 */
static int s3c44b0xfb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			 unsigned *blue, unsigned *transp,
			 struct fb_info *_info)
{
	/*
	 *  Read a single color register and split it into colors/transparent.
	 *  The return values must have a 16 bit magnitude.
	 *  Return != 0 for invalid regno.
	 */
	struct s3c44b0xfb_info *info = (struct s3c44b0xfb_info *) _info;
	u32 blueval, redval, greenval;
	u8 helpvalue;
	blueval = inw(S3C44B0X_BLUELUT);
	redval = inw(S3C44B0X_REDLUT);
	greenval = inw(S3C44B0X_GREENLUT);
	switch(info->gen.info.var.bits_per_pixel)
	{
	case 1:
		if(regno > 1)
			return 1; /* error */
		if(regno == 0)
			*red = *green = *blue = 0;
		else
			*red = *green = *blue = 0xFFFF;
		break;
	case 2:
		if(regno > 4)
			return 1; /* error */

		blueval = (blueval >> (regno<<2)) & 0xF;
		*red = *green = *blue = 0x1111 * blueval; /* 0xFFFF / 0xF = 0x1111 */
		break;
	case 4:
		if (regno > 16)
			return 1; /* error */
		*red = *green = *blue = 0x1111 * regno; /* 0xFFFF / 0xF = 0x1111 */
		break;
	case 8:
		if (regno > 256)
			return 1;
		helpvalue = regno >> 5;
		redval = (redval >> (helpvalue<<2)) & 0xF;
		*red = redval * 0x1111;
		helpvalue = (regno >> 2) & 0x7;
		greenval = (greenval >> (helpvalue<<2)) & 0xF;
		*green = greenval * 0x1111;
		helpvalue = regno & 0x03;
		blueval = (blueval >> (helpvalue<<2)) & 0xF;
		*blue = blueval * 0x1111;
		break;
	default:
		return 1; 
	}
	return 0;
}

/**
 * Sets one color pallette entry
 *
 * \param regno the entry index to get
 * \param red The red value of the new pallette entry
 * \param green The green value of the new pallette entry
 * \param blue The blue value of the new pallette entry
 * \param transp The transparency value of the new pallette entry
 * \param _info The framebuffer infos
 * \return 0 on success; an error value otherwise
 */
static int s3c44b0xfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			      unsigned blue, unsigned transp,
			      struct fb_info *_info)
{
	/*
	 *  Set a single color register. The values supplied have a 16 bit
	 *  magnitude.
	 *  Return != 0 for invalid regno.
	 */
	struct s3c44b0xfb_info *info = (struct s3c44b0xfb_info *) _info;
	unsigned int helpvalue;
	u32 blueval, redval, greenval;
	blueval = inw(S3C44B0X_BLUELUT);
	redval = inw(S3C44B0X_REDLUT);
	greenval = inw(S3C44B0X_GREENLUT);
	switch(info->gen.info.var.bits_per_pixel)
	{
	case 1:
		return -EINVAL;
		break;
	case 2:
		if((regno > 4) || (red != green) || (red != blue)) /* FIXME: maybe wrong */
			return 1;
		helpvalue = (unsigned int)(blue / 0x1111) & 0xF;
		blueval &= ~(0xF << (regno<<2));
		blueval |= (helpvalue << (regno<<2));
		outl(blueval, S3C44B0X_BLUELUT);
		break;
	case 4:
		return -EINVAL;
		break;
	case 8:
		if(regno > 256)
			return 1;
		helpvalue = (unsigned int)(blue / 0x1111) & 0xF;
		blueval &= ~(0xF << ((regno&0x03)<<2));
		blueval |= (helpvalue << ((regno&0x03)<<2));
		outl(blueval, S3C44B0X_BLUELUT);
		helpvalue = (unsigned int)(red / 0x1111) & 0xF;
		redval &= ~(0xF << (((regno>>5)&0x7)<<2));
		redval |= (helpvalue << (((regno>>5)&0x7)<<2));
		outl(redval, S3C44B0X_BLUELUT);
		helpvalue = (unsigned int)(green / 0x1111) & 0xF;
		greenval &= ~(0xF << (((regno>>2)&0x7)<<2));
		greenval |= (helpvalue << (((regno>>2)&0x7)<<2));
		outl(greenval, S3C44B0X_BLUELUT);
		break;
	default:
		return 1;
	}

    return 0;
}

/** Panning is not supported */
static int s3c44b0xfb_pan_display(const struct fb_var_screeninfo *var,
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
 * \param blank_mode 0 -> video output is enabled; 4 -> video output is disabled, all other values are invalid
 * \param info Pointer the framebuffer infos
 */
static int s3c44b0xfb_blank(int blank_mode, struct fb_info_gen *info)
{
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
	u32 helpvalue;
	switch(blank_mode)
	{
	case 0:
		helpvalue = inw(S3C44B0X_LCDCON1);
		outl(helpvalue|S3C44B0X_LCDCON1_ENVID, S3C44B0X_LCDCON1); /* enable video output */
		break;
	case 2:
		return -EINVAL;
	case 3:
		return -EINVAL;
	case 4:
		helpvalue = inw(S3C44B0X_LCDCON1);
		outl(helpvalue&~S3C44B0X_LCDCON1_ENVID, S3C44B0X_LCDCON1); /* disable video output */
		break;
	default:
		return -EINVAL;
	}

    return 0;
}

/**
 * Dis-/enables the video output.
 *
 * \param blank_mode 0 -> video output is enabled; 4 -> video output is disabled, all other values are invalid
 * \param info Pointer the framebuffer infos
 */
static void s3c44b0xfb_gen_blank(int blank_mode, struct fb_info *info)
{
	s3c44b0xfb_blank(blank_mode, &fb_info.gen);
	return;
}

#if 0
static void s3c44b0xfb_set_disp(const void *par, struct display *disp,
			      struct fb_info_gen *info)
{
	/*
	 *  Fill in a pointer with the virtual address of the mapped frame buffer.
	 *  Fill in a pointer to appropriate low level text console operations (and
	 *  optionally a pointer to help data) for the video mode `par' of your
	 *  video hardware. These can be generic software routines, or hardware
	 *  accelerated routines specifically tailored for your hardware.
	 *  If you don't have any appropriate operations, you must fill in a
	 *  pointer to dummy operations, and there will be no text output.
	 */
	return;
}
#endif

/**
 * Initializes the LCD controller according to settings.
 *
 * \param p_lcd Pointer to the LCD panel settings that is connected.
 */
static void s3c44b0xfb_init_lcd_controller( struct known_lcd_panels *p_lcd)
{
	u32 helpvalue = 0, pagewidth=0, offsize = 0;
	u32 lcdbaseu, lcdbasel;
	u8 modesel = 0;

	/*---------------------------------*/
	/* set LCDSADDR1 to zero to disable the LCD controller */
	/*---------------------------------*/
	helpvalue = 0;
	outl(helpvalue, S3C44B0X_LCDCON1);

	/*---------------------------------*/
	/* set LCDSADDR1                   */
	/*---------------------------------*/
	helpvalue = 0;
	switch(p_lcd->bpp)
	{
	case 1:
		modesel = 0;
		break;
	case 2:
		modesel = 1;
		break;
	case 4:
		modesel = 2;
		break;
	case 8:
		modesel = 3;
		break;
	default:
		modesel = 0;
	}
	helpvalue |=  (modesel & 0x03) << 27; /* MODESEL value */
	helpvalue |= ((fb_info.fb_phys & 0x0FC00000) >> 1); /* LCDBANK addr. */
	lcdbaseu = ((fb_info.fb_phys +
		     fb_info.gen.info.var.xoffset +
		     (fb_info.gen.info.var.yoffset *
		      fb_info.gen.info.var.xres_virtual)) & 0x3FFFFF);
	helpvalue |= lcdbaseu >> 1;
	outl(helpvalue, S3C44B0X_LCDSADDR1);
	/*---------------------------------*/
	/* set LCDSADDR2                   */
	/*---------------------------------*/
	helpvalue = 0;
	helpvalue |= (p_lcd->bswp &0x1) << 29; /* BSWP */
	helpvalue |= (p_lcd->mval) << 21; /* MVAL */
	pagewidth = (p_lcd->width * p_lcd->bpp) >> 4;
	offsize = ((fb_info.gen.info.var.xres_virtual * p_lcd->bpp) >> 4) -
		    pagewidth;

	lcdbasel = ((lcdbaseu>>1) +
	            ((pagewidth + offsize) * (p_lcd->lineval + 1))) & 0x3FFFFF;

	helpvalue |= lcdbasel;
	outl(helpvalue, S3C44B0X_LCDSADDR2);

	/*---------------------------------*/
	/* set LCDSADDR3                   */
	/*---------------------------------*/
	helpvalue = 0;
	helpvalue |= (pagewidth & 0x1FF);
	helpvalue |= (offsize & 0x7FF) << 9;
	outl(helpvalue, S3C44B0X_LCDSADDR3);

	/*---------------------------------*/
	/* set LCDCON2                     */
	/*---------------------------------*/
	helpvalue = 0;
	helpvalue |= (p_lcd->lineblank & 0x3FF) << 21;
	helpvalue |= (p_lcd->hozval & 0x3FF) << 10;
	helpvalue |= (p_lcd->lineval & 0x3FF);
	outl(helpvalue, S3C44B0X_LCDCON2);

	/*---------------------------------*/
	/* set LCDCON3                     */
	/*---------------------------------*/
	outb((p_lcd->selfref & 0x1), S3C44B0X_LCDCON3);

	/*---------------------------------*/
	/* set REDLUT                      */
	/*---------------------------------*/
	outl(0xECA86420, S3C44B0X_REDLUT);
       /*---------------------------------*/
	/* set GREENLUT                    */
	/*---------------------------------*/
	outl(0xECA86420, S3C44B0X_GREENLUT);
	/*---------------------------------*/
	/* set BLUELUT                     */
	/*---------------------------------*/
	outl(0x0000F840, S3C44B0X_BLUELUT);

	/*---------------------------------*/
	/* set LCDCON1 (also enables the LCD controller) */
	/*---------------------------------*/
	helpvalue = ((p_lcd->clkval&0x3FF) << 12);
	helpvalue |= ((((p_lcd->wlh >> 2)-1) & 0x03) << 10);
	helpvalue |= ((((p_lcd->wdly >> 2)-1) & 0x03) << 8);
	helpvalue |= ((p_lcd->mmode & 0x01) << 7);
	helpvalue |= (p_lcd->dismode << 5); /* DISMODE */
	helpvalue |= ((p_lcd->invclk&0x01) << 4);
	helpvalue |= ((p_lcd->invline&0x01) << 3);
	helpvalue |= ((p_lcd->invframe&0x01) << 2);
	helpvalue |= ((p_lcd->invvd&0x01) << 1);
	helpvalue |= 1; /* enables the display */
	outl(helpvalue, S3C44B0X_LCDCON1);

	/* switch display on by setting special port */
	if(p_lcd->disp_on_ctrl_reg) {
		helpvalue = inl(p_lcd->disp_on_ctrl_reg);
		helpvalue &= ~p_lcd->disp_on_ctrl_reg_mask;
		helpvalue |= p_lcd->disp_on_ctrl_reg_value;
		outl(helpvalue, p_lcd->disp_on_ctrl_reg);
	}
	if(p_lcd->disp_on_port) {
		helpvalue = inl(p_lcd->disp_on_port);
		helpvalue &= ~p_lcd->disp_on_mask;
		helpvalue |= p_lcd->disp_on_val;
		outl(helpvalue, p_lcd->disp_on_port); /* enable display */
	}
}

/* ------------ Interfaces to hardware functions ------------ */

/**
 * The framebuffer hardware operations table
 */
struct fbgen_hwswitch s3c44b0xfb_switch = {
	s3c44b0xfb_detect,
	s3c44b0xfb_encode_fix,
	s3c44b0xfb_decode_var,
	s3c44b0xfb_encode_var,
	s3c44b0xfb_get_par,
	s3c44b0xfb_set_par,
	s3c44b0xfb_getcolreg,
	s3c44b0xfb_setcolreg,
	s3c44b0xfb_pan_display,
	s3c44b0xfb_blank,
	NULL
};

/* ------------ Hardware Independent Functions ------------ */
static int fill_structs(void)
{
	struct known_lcd_panels *p_lcd;

	p_lcd = &panels[s3c44b0x_lcd_index];
	memset(&fb_info.gen, 0, sizeof(fb_info.gen));
	fb_info.gen.fbhw = &s3c44b0xfb_switch;
	sprintf(fb_info.gen.info.modename, "%dx%dx%d",p_lcd->width,p_lcd->height,p_lcd->bpp);
	fb_info.gen.parsize = sizeof(struct s3c44b0xfb_par);
	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.node = -1;
	fb_info.gen.info.fbops = &s3c44b0xfb_ops;
	fb_info.gen.info.disp = NULL; /* not needed ?? */
	fb_info.gen.info.switch_con = NULL;
	fb_info.gen.info.updatevar = NULL;
	fb_info.gen.info.blank = &s3c44b0xfb_gen_blank;
	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
	/* This should give a reasonable default video mode */
	/* fbgen_get_var(&disp.var, -1, &fb_info.gen.info); */
	/* fbgen_do_set_var(&disp.var, 1, &fb_info.gen); */
	/* fbgen_set_disp(-1, &fb_info.gen); */
	/* fbgen_install_cmap(0, &fb_info.gen); */

	/**********************************************
	 * file the var struct inside the current_par struct
	 **********************************************/
	memset(&current_par.var, 0, sizeof(current_par.var));
	current_par.var.xres = p_lcd->width;
	current_par.var.yres = p_lcd->height;
	current_par.var.xres_virtual = p_lcd->width;
	current_par.var.yres_virtual = p_lcd->height;
	current_par.var.bits_per_pixel = p_lcd->bpp;
	current_par.var.grayscale = (p_lcd->bpp == 8)? 0 : 1;
	if(p_lcd->bpp == 1)
	{
		current_par.var.blue.length = 1;
	}
	else if(p_lcd->bpp == 2)
	{
		current_par.var.blue.length = 2;
	}
	else if(p_lcd->bpp == 4)
	{
		current_par.var.blue.length = 4;
	}
	else if(p_lcd->bpp == 8)
	{
		current_par.var.red.offset = 5;
		current_par.var.red.length = 3;
		current_par.var.green.offset = 2;
		current_par.var.green.length = 3;
		current_par.var.blue.length = 2;
	}

	current_par_valid = 1;

	/*
	 * Allocate LCD framebuffer from system memory
	 * Allocate one byte more to be able to be 16 bit aligned
	 */
	fb_info.fb_size = (p_lcd->width * p_lcd->height * p_lcd->bpp) >> 3;

	fb_info.fb_phys = (unsigned long)kmalloc(fb_info.fb_size+1, GFP_KERNEL);
	if(!fb_info.fb_phys) {
		printk("S3C44B0X framebuffer driver error: Could not allocate framebuffer memory\n");
		return -ENOMEM;
	}
	if(fb_info.fb_phys&0x1)
		fb_info.fb_phys++;

	fb_info.fb_virt_start = fb_info.fb_phys;
	current_par.line_length = (p_lcd->width * p_lcd->bpp) >> 3;

	/* fill the var element of fb_info.gen.info */
	memcpy(&fb_info.gen.info.var, &current_par.var, sizeof(struct fb_var_screeninfo));

	/* fill the fix element of fb_info.gen.info */
	s3c44b0xfb_encode_fix(&fb_info.gen.info.fix, &current_par, &fb_info.gen);

	if (register_framebuffer(&fb_info.gen.info) < 0)
		return -EINVAL;
	printk(KERN_INFO "fb%d: %s frame buffer device\n", GET_FB_IDX(fb_info.gen.info.node),
	       fb_info.gen.info.modename);

	/*********************************************
         *  set framebuffer memory to zero
	 *********************************************/
	memset((void*)fb_info.fb_phys, 0, fb_info.fb_size);

	/*********************************************
	 * init the lcd controller
	 *********************************************/
	s3c44b0xfb_init_lcd_controller(p_lcd);

	/* uncomment this if your driver cannot be unloaded */
	/* MOD_INC_USE_COUNT; */
	printk("S3C44B0X framebuffer driver: Init ready\n");
	return 0;
}

/**
 * Initialization function. Called at startup / load of module
 */
int __init s3c44b0xfb_init(void)
{
	int num_panels; /* number of known LCD panels */
	int i;

	printk("S3C44B0X framebuffer init\n(c) sympat GmbH Alexander Assel / Stefan Macher\n");

	if ((bootloader == 0) && (panel_name == NULL))
	{
		printk("S3C44B0X framebuffer driver error: No panel name specified and bootloader settings shall not be used\n");
		return -EINVAL;
	}

	if (bootloader)
	{
		/*printk("S3C44B0X framebuffer driver error: Using of bootloader settings not yet implemented\n");*/
		/*fill the EMPTY_DISPLAY struct*/
		s3c44b0x_lcd_index = 0; /*default display struct*/

		if (!panels[s3c44b0x_lcd_index].width ||
		    !panels[s3c44b0x_lcd_index].height ||
		    !panels[s3c44b0x_lcd_index].bpp)
			return -EINVAL;

		panels[s3c44b0x_lcd_index].clkval =
			(inl(S3C44B0X_LCDCON1)>>12) & 0x3FF;
		/*wlh*/
		switch( (inl(S3C44B0X_LCDCON1)>>10) & 0x3)
		{
		case 0:
			panels[s3c44b0x_lcd_index].wlh = 4;
			break;
		case 1:
			panels[s3c44b0x_lcd_index].wlh = 8;
			break;
		case 2:
			panels[s3c44b0x_lcd_index].wlh = 12;
			break;
		case 3:
			panels[s3c44b0x_lcd_index].wlh = 16;
			break;
		default:
			break;
		}
		/*wdly*/
		switch( (inl(S3C44B0X_LCDCON1)>>8)&0x3)
		{
		case 0:
			panels[s3c44b0x_lcd_index].wdly = 4;
			break;
		case 1:
			panels[s3c44b0x_lcd_index].wdly = 8;
			break;
		case 2:
			panels[s3c44b0x_lcd_index].wdly = 12;
			break;
		case 3:
			panels[s3c44b0x_lcd_index].wdly = 16;
			break;
		default:
			break;
		}
		/*mmode*/
		panels[s3c44b0x_lcd_index].mmode = (inl(S3C44B0X_LCDCON1)>>7)&0x1;
		/*dismode*/
		panels[s3c44b0x_lcd_index].dismode = (inl(S3C44B0X_LCDCON1)>>5)&0x3;
		/*invclk*/
		panels[s3c44b0x_lcd_index].invclk = (inl(S3C44B0X_LCDCON1)>>4)&0x1;
		/*invline*/
		panels[s3c44b0x_lcd_index].invline = (inl(S3C44B0X_LCDCON1)>>3)&0x1;
		/*invframe*/
		panels[s3c44b0x_lcd_index].invframe = (inl(S3C44B0X_LCDCON1)>>2)&0x1;
		/*invvd */
		panels[s3c44b0x_lcd_index].invvd = (inl(S3C44B0X_LCDCON1)>>1)&0x1;
		/*lineblank*/
		panels[s3c44b0x_lcd_index].lineblank = (inl(S3C44B0X_LCDCON2)>>21)&0x7FF;
		/*selfref*/
		panels[s3c44b0x_lcd_index].selfref = inl(S3C44B0X_LCDCON3)&0x1;
		/*bswp*/
		panels[s3c44b0x_lcd_index].bswp = (inl(S3C44B0X_LCDSADDR2)>>29)&0x1;
		/*mval*/
		panels[s3c44b0x_lcd_index].mval = (inl(S3C44B0X_LCDSADDR2)>>21)&0xFF;
		/*hozval*/
		panels[s3c44b0x_lcd_index].hozval = (inl(S3C44B0X_LCDCON2)>>10)&0x7FF;
		/*lineval*/
		panels[s3c44b0x_lcd_index].lineval = inl(S3C44B0X_LCDCON2)&0x3FF;
		/*disp_on_port*/
		panels[s3c44b0x_lcd_index].disp_on_port = NULL;
		/*disp_on_ctrl_reg*/
		panels[s3c44b0x_lcd_index].disp_on_ctrl_reg = NULL;
	}
	else
	{
		num_panels = sizeof(panels)/sizeof(struct known_lcd_panels);
		/* Get the panel name, everything else is fixed */
		for (i=0; i < num_panels; i++)
		{
			if (!strcmp(panel_name, panels[i].lcd_panel_name))
			{
				s3c44b0x_lcd_index = i;
				panel_valid = 1;
				break;
			}
		}

		if(!panel_valid)
		{
			printk("S3C44B0X framebuffer driver error: Invalid panel name selected\n");
			return -EINVAL;
		}
	}

	return fill_structs();
}


/**
 * Unregisters the framebuffer
 */
void s3c44b0xfb_cleanup(struct fb_info *info)
{
	/*
	 *  If your driver supports multiple boards, you should unregister and
	 *  clean up all instances.
	 */
	kfree((unsigned long *)fb_info.fb_phys);
	unregister_framebuffer(info);
}

/**
 * Framebuffer driver setup function
 *
 * \param Kernel command line
 */
int __init s3c44b0xfb_setup(char *options)
{
	int temp[4];
	get_options(options, 4, temp);
	if(temp[0] != 3)
		return -EINVAL;
	panels[s3c44b0x_lcd_index].width = temp[1];
	panels[s3c44b0x_lcd_index].height = temp[2];
	panels[s3c44b0x_lcd_index].bpp = temp[3];
	return 0;
}

/* ------------------------------------------------------------------------- */

/*
 *  Frame buffer operations
 */

/** Open function only checks that the driver is only once in use */
static int s3c44b0xfb_open(struct fb_info *info, int user)
{
	return 0;
}

/** Release function reduces only the use count */
static int s3c44b0xfb_release(struct fb_info *info, int user)
{
	return 0;
}

/**
 * S3C44B0X specific ioctl function
 *
 * Provides an IO control for configuring the contrast.
 * One must provide min, max values, register address and
 * register type in the linux kernel config to do this.
 *
 * The given contrast value will be set to the configured value.
 * It may be different compared to the given value if the upper or
 * lower limit is reached.
 */
static int s3c44b0xfb_ioctl(struct inode *inode,
                            struct file *file,
                            unsigned int cmd,
                            unsigned long arg,
                            int con,
                            struct fb_info *info)
{
	int contrast;
	struct lcdcontrast contrastval;

	switch(cmd)
	{
#if (CONFIG_FB_S3C44B0X_CONTRAST != 'n')
	case FBIO_S3C44B0X_SET_CONTRAST:
		copy_from_user(&contrast, (int *)arg, sizeof(int));
		if (contrast <  CONFIG_FB_S3C44B0X_CONTRAST_MIN)
			contrast =  CONFIG_FB_S3C44B0X_CONTRAST_MIN;
		if (contrast > CONFIG_FB_S3C44B0X_CONTRAST_MAX)
		contrast = CONFIG_FB_S3C44B0X_CONTRAST_MAX;
  #if defined(CONFIG_FB_S3C44B0X_CONTRAST_BYTE)
		outb(contrast, CONFIG_FB_S3C44B0X_CONTRAST_ADR);
  #elif defined (CONFIG_FB_S3C44B0X_CONTRAST_HALFWORD)
		outw(contrast, CONFIG_FB_S3C44B0X_CONTRAST_ADR);
  #elif defined (CONFIG_FB_S3C44B0X_CONTRAST_WORD)
		outl(contrast, CONFIG_FB_S3C44B0X_CONTRAST_ADR);
  #endif
		copy_to_user((int *)arg, &contrast, sizeof(int));
		return 0;

	case FBIO_S3C44B0X_GET_CONTRAST:
  #if defined(CONFIG_FB_S3C44B0X_CONTRAST_BYTE)
		contrast = inb(CONFIG_FB_S3C44B0X_CONTRAST_ADR);
  #elif defined (CONFIG_FB_S3C44B0X_CONTRAST_HALFWORD)
		contrast = inw(CONFIG_FB_S3C44B0X_CONTRAST_ADR);
  #elif defined (CONFIG_FB_S3C44B0X_CONTRAST_WORD)
		contrast = inl(CONFIG_FB_S3C44B0X_CONTRAST_ADR);
  #endif
		copy_to_user((int *)arg, &contrast, sizeof(unsigned int));
		return 0;
#endif

	/* contrast limits set in 'Config.in' */
	case FBIO_S3C44B0X_GET_CONTRAST_LIM:
		copy_from_user(&contrastval,
			(struct lcdcontrast *)arg,
			sizeof(struct lcdcontrast));
		contrastval.min = CONFIG_FB_S3C44B0X_CONTRAST_MIN;
		contrastval.max = CONFIG_FB_S3C44B0X_CONTRAST_MAX;
		contrastval.def = CONFIG_FB_S3C44B0X_CONTRAST_DEF;
		copy_to_user((struct lcdcontrast *)arg,
			&contrastval, sizeof(struct lcdcontrast));
		return 0;

	default:
		break;
	}
	return -EINVAL; /* unknown IOCTL */
}

static unsigned long
s3c44b0xfb_get_unmapped_area(struct file *file,
			     unsigned long addr, unsigned long len,
			     unsigned long pgoff, unsigned long flags)
{
	unsigned long off = pgoff << PAGE_SHIFT;

	if (off < fb_info.fb_size)
		if ((len >> 3) <= fb_info.fb_size - off)
			return virt_to_phys(fb_info.fb_virt_start + off);

	return -EINVAL;
}


/**
 * The framebuffer operations table
 * Most just use the generic functions
 */
static struct fb_ops s3c44b0xfb_ops = {
	owner:		THIS_MODULE,
	fb_open:	s3c44b0xfb_open,
	fb_release:	s3c44b0xfb_release,
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	fbgen_set_cmap,
	fb_pan_display:	fbgen_pan_display,
	fb_ioctl:	s3c44b0xfb_ioctl,
        get_fb_unmapped_area: s3c44b0xfb_get_unmapped_area,
};

/* ------------------------------------------------------------------------- */

/*
 *  Modularization
 */

#ifdef MODULE
MODULE_LICENSE("GPL");
int init_module(void)
{
	return s3c44b0xfb_init();
}

void cleanup_module(void)
{
	s3c44b0xfb_cleanup(&fb_info.gen.info);
}

#endif /* MODULE */
