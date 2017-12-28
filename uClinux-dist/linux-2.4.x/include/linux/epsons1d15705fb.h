/**
 * Copyright (C) sympat GmbH 2004
 * Authors: Stefan Macher
 *
 * linux/drivers/video/epsons1d15705fb.h
 *
 * DESCRIPTION:
 * ------------
 * Linux framebuffer driver for the EPSON S1D15705 (old name SED1575).
 * It has actually no terminal support (may come later). Because of the very slow
 * controller interface, the driver allocates a framebuffer memory within normal 
 * memory that is accessed by the applications when mapping, writing or reading from
 * the framebuffer device. So the application has to call the special I/O control
 * FB_S1D15705_FLUSH, that is defined here, whenever it wants to update the 
 * screen content. Then the internal driver framebuffer is copied to the controller
 * framebuffer.
 *
 * LICENSE:
 * --------
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#ifndef __INCLUDE_EPSONS1D15705FB__
#define __INCLUDE_EPSONS1D15705FB__

#include <linux/ioctl.h>

/** struct to get the contrast-range and the default contrast */
struct lcdcontrast {
    int min; /**< minimum contrast */
    int max; /**< maximum contrast */
    int def; /**< default contrast */
};

#define FB_S1D15705_FLUSH            _IO('F', 0x80)
#define FB_S1D15705_SET_CONTRAST     _IOW('F', 0x81, int)
#define FB_S1D15705_GET_CONTRAST_LIM _IOWR('F', 0x83, struct lcdcontrast)

/** the LCD command address */
#define LCD_COMMAND_ADR 0x07FF0000+0x202C

/** the LCD data address */
#define LCD_DATA_ADR LCD_COMMAND_ADR + 0x40

/** the hight and the width of the controlled display in pixels */
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 64

/** the pagecount is displayhight/8 (or >> 3) */
#define NUM_PAGES (DISPLAY_HEIGHT >> 3)

/** and the start-column */
#define COL_START 0x08

#endif
