/*
 * drivers/video/ep93xxfb.h -- IOCTLs for the EP93xx graphics accelerator
 *
 * Copyright (c) 2004 Cirrus Logic, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 */
#ifndef __EP93XXFB_H__
#define __EP93XXFB_H__

/*
 * The following are the IOCTLs that can be sent to the EP93xx frame buffer
 * device.
 */
#define FBIO_EP93XX_GET_CAPS	0x000046c0
#define FBIO_EP93XX_CURSOR	0x000046c1
#define FBIO_EP93XX_LINE	0x000046c2
#define FBIO_EP93XX_FILL	0x000046c3
#define FBIO_EP93XX_BLIT	0x000046c4

/*
 * ioctl(fd, FBIO_EP93XX_GET_CAPS, unsigned long *)
 *
 * The unsigned long is filled in with these capabilities flags to indicate
 * what the frame buffer driver is capable of performing.
 */
#define EP93XX_CAP_CURSOR	0x00000001
#define EP93XX_CAP_LINE		0x00000002
#define EP93XX_CAP_FILL		0x00000004
#define EP93XX_CAP_BLIT		0x00000008

/*
 * The bits in the flags field of ep93xx_cursor.
 */
#define CURSOR_BLINK		0x00000001
#define CURSOR_MOVE		0x00000002
#define CURSOR_SETSHAPE		0x00000004
#define CURSOR_SETCOLOR		0x00000008
#define CURSOR_ON		0x00000010
#define CURSOR_OFF		0x00000020

/*
 * ioctl(fd, FBIO_EP93XX_CURSOR, ep93xx_cursor *)
 *
 * "data" points to an array of pixels that define the cursor; each row should
 * be a multiple of 32-bit values (i.e. 16 pixels).  Each pixel is two bits,
 * where the values are:
 *
 *     00 => transparent    01 => invert    10 => color1    11 => color2
 *
 * The data is arranged as follows (per word):
 *
 *    bits: |31-30|29-28|27-26|25-24|23-22|21-20|19-18|17-16|
 *   pixel: | 12  | 13  | 14  | 15  |  8  |  9  | 10  | 11  |
 *    bits: |15-14|13-12|11-10| 9-8 | 7-6 | 5-4 | 3-2 | 1-0 |
 *   pixel: |  4  |  5  |  6  |  7  |  0  |  1  |  2  |  3  |
 *
 * Regardless of the frame buffer color depth, "color1", "color2",
 * "blinkcolor1", and "blinkcolor2" are 24-bit colors since the cursor is
 * injected into the data stream right before the video DAC.
 *
 * When "blinkrate" is not zero, pixel value 10 will alternate between "color1"
 * and "blinkcolor1" (similar for pixel value 11 and "color2"/"blinkcolor2").
 *
 * "blinkrate" ranges between 0 and 255.  When 0, blinking is disabled.  255 is
 * the fastest blink rate and 1 is the slowest.
 *
 * Both "width" and "height" must be between 1 and 64; it is preferable to have
 * "width" a multiple of 16.
 */
struct ep93xx_cursor {
    __u32 flags;
    __u32 dx;		// Only used if CURSOR_MOVE is set
    __u32 dy;		// Only used if CURSOR_MOVE is set
    __u32 width;	// Only used if CURSOR_SETSHAPE is set
    __u32 height;	// Only used if CURSOR_SETSHAPE is set
    const char *data;	// Only used if CURSOR_SETSHAPE is set
    __u32 blinkrate;	// Only used if CURSOR_BLINK is set
    __u32 color1;	// Only used if CURSOR_SETCOLOR is set
    __u32 color2;	// Only used if CURSOR_SETCOLOR is set
    __u32 blinkcolor1;	// Only used if CURSOR_SETCOLOR is set
    __u32 blinkcolor2;	// Only used if CURSOR_SETCOLOR is set
};

/*
 * The bits in the flags field of ep93xx_line.
 */
#define LINE_PATTERN		0x00000001
#define LINE_PRECISE		0x00000002
#define LINE_BACKGROUND		0x00000004

/*
 * ioctl(fd, FBIO_EP93XX_LINE, ep93xx_line *)
 *
 * The line starts at ("x1","y1") and ends at ("x2","y2").  This means that
 * when using a pattern, the two coordinates are not transitive (i.e. swapping
 * ("x1","y1") with ("x2","y2") will not necessarily draw the exact same line,
 * pattern-wise).
 *
 * "pattern" is a 2 to 16 bit pattern (since a 1 bit pattern isn't much of a
 * pattern).  The lower 16 bits define the pattern (1 being foreground, 0 being
 * background or transparent), and bits 19-16 define the length of the pattern
 * (as pattern length - 1).  So, for example, "0xf00ff" defines a 16 bit
 * with the first 8 pixels in the foreground color and the next 8 pixels in the
 * background color or transparent.
 *
 * LINE_PRECISE is used to apply angularly corrected patterns to line.  It
 * should only be used when LINE_PATTERN is also set.  The pattern will be
 * applied along the length of the line, instead of along the length of the
 * major axis.  This may result in the loss of fine details in the pattern, and
 * will take more time to draw the line in most cases.
 */
struct ep93xx_line {
    __u32 flags;
    __s32 x1;
    __s32 y1;
    __s32 x2;
    __s32 y2;
    __u32 fgcolor;
    __u32 bgcolor;	// Only used if LINE_BACKGROUND is set
    __u32 pattern;	// Only used if LINE_PATTERN is set
};

/*
 * ioctl(fd, FBIO_EP93XX_FILL, ep93xx_fill *)
 *
 * Fills from dx to (dx + width - 1), and from dy to (dy + height - 1).
 */
struct ep93xx_fill {
    __u32 dx;
    __u32 dy;
    __u32 width;
    __u32 height;
    __u32 color;
};

/*
 * The bits in the flags field of ep93xx_blit.
 */
#define BLIT_SOURCE_MASK	0x00000001
#define BLIT_SOURCE_MEMORY	0x00000000
#define BLIT_SOURCE_SCREEN	0x00000001
#define BLIT_TRANSPARENT	0x00000004
#define BLIT_MASK_MASK		0x00000600
#define BLIT_MASK_DISABLE	0x00000000
#define BLIT_MASK_AND		0x00000200
#define BLIT_MASK_OR		0x00000400
#define BLIT_MASK_XOR		0x00000600
#define BLIT_DEST_MASK		0x00001800
#define BLIT_DEST_DISABLE	0x00000000
#define BLIT_DEST_AND		0x00000800
#define BLIT_DEST_OR		0x00001000
#define BLIT_DEST_XOR		0x00001800
#define BLIT_1BPP_SOURCE	0x00006000

/*
 * ioctl(fd, FBIO_EP93XX_BLIT, ep93xx_blit *)
 *
 * When BLIT_SOURCE_MEMORY is set, "data" points to an array of pixels that are
 * to be blitted to the screen; each row should be a multiple of 32-bit values.
 * When BLIT_1BPP_SOURCE is set, each pixel is one bit and is arranged as
 * follows (per word):
 *
 *     bit: |31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|
 *   pixel: |24|25|26|27|28|29|30|31|16|17|18|19|20|21|22|23|
 *     bit: |15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
 *   pixel: | 8| 9|10|11|12|13|14|15| 0| 1| 2| 3| 4| 5| 6| 7|
 *
 * When the frame buffer is in 8bpp mode, each pixel is one byte and is
 * arranged as follows (per word):
 *
 *    bits: |31-24|23-16|15-8 | 7-0 |
 *   pixel: |  3  |  2  |  1  |  0  |
 *
 * When the frame buffer is in 16bpp mode, each pixel is two bytes and is
 * arranged as follows (per word):
 *
 *    bits: |31-16|15-0 |
 *   pixel: |  1  |  0  |
 *
 * When BLIT_SOURCE_SCREEN is set, BLIT_1BPP_SOURCE is not valid (not for any
 * hardware imposed reason, but because it simply doesn't make any sense).
 * When BLIT_SOURCE_MEMORY and BLIT_1BPP_SOURCE are set, BLIT_MASK_DISABLE must
 * be set.
 *
 * BLIT_MASK_MASK applies a masking operation on the source data as it is read.
 * When not disabled, each pixel is modified by the given operation with the
 * "fgcolor" value.
 *
 * BLIT_DEST_MASK determines how the data is written to the destination
 * location.  When not disabled, each destination pixel is modified by the
 * given operation with the current value.  This operation takes place after
 * the BLIT_MASK_MASK operation.
 *
 * BLIT_TRANSPARENT allows a transparency color to be defined for the blit.  If
 * set, after BLIT_DEST_MASK has been applied, if the resulting pixel matches
 * "transcolor", the destination pixel is not modified.
 */
struct ep93xx_blit {
    __u32 flags;
    __u32 dx;
    __u32 dy;
    __u32 width;
    __u32 height;
    const char *data;	// Only used if BLIT_SOURCE_MEMORY is set
    __u32 swidth;	// Only used if BLIT_SOURCE_MEMORY is set
    __u32 sx;		// Only used if BLIT_SOURCE_SCREEN is set
    __u32 sy;		// Only used if BLIT_SOURCE_SCREEN is set
    __u32 fgcolor;	// Only used if BLIT_1BPP_SOURCE or BLIT_MASK_?? is set
    __u32 bgcolor;	// Only used if BLIT_1BPP_SOURCE is set
    __u32 transcolor;	// Only used if BLIT_TRANSPARENT is set
};

#endif /* __EP93XXFB_H__ */
