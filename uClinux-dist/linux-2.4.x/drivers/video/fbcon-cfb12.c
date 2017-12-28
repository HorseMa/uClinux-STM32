/*
 * Frame buffer operations for 12 bpp packed pixels
 * Copyright (C) 2003 Nucleus Systems, Ltd.
 *
 * based on:
 *
 *  linux/drivers/video/cfb16.c -- Low level frame buffer operations for 16 bpp
 *				   truecolor packed pixels
 *
 *	Created 5 Apr 1997 by Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb12.h>

    /*
     *  12 bpp packed pixels
     *
     * (font is supposed to be with _even_ width...)
     */

void fbcon_cfb12_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : ((p->var.xres_virtual * 3) >> 1);
    p->next_plane = 0;
}

void fbcon_cfb12_bmove(struct display *p, int sy, int sx, int dy, int dx,
		       int height, int width)
{
    u8 *src, *dst;
    int bytes = p->next_line;
    int linesize = bytes * fontheight(p);

    src = p->screen_base + sy * linesize + ((sx * fontwidth(p) * 3) >> 1);
    dst = p->screen_base + dy * linesize + ((dx * fontwidth(p) * 3) >> 1);

    if (src < dst) {
        src += height * linesize - bytes;
        dst += height * linesize - bytes;
        bytes = -bytes;
    } 

    width = (width * fontwidth(p) * 3) >> 1;
    height *= fontheight(p);

    for ( ; height ; height--, src += bytes, dst += bytes) {
        fb_memmove (dst, src, width);
    }
}

static void rectfill(u8 *dst, int bytes, int h, int w, int val)
{
    u8 *d;
    int i, v0, v1, v2;

    val |= val << 12;
    v0 = (val >>  0) & 0xFF;
    v1 = (val >>  8) & 0xFF;
    v2 = (val >> 12) & 0xFF;

    for ( ; h ; h--,  dst += bytes) {
        for (i = w, d = dst ; i ; i--) {
            fb_writeb (v0, d++);
            fb_writeb (v1, d++);
            fb_writeb (v2, d++);
        }
    }
}

void fbcon_cfb12_clear(struct vc_data *conp, struct display *p, int dy, int dx,
		       int height, int width)
{
    u8 *dst;
    int bytes = p->next_line;
    int linesize = bytes * fontheight(p);
    int bgx;

    dst = p->screen_base + dy * linesize + ((dx * fontwidth(p) * 3) >> 1);

    width = (width * fontwidth(p)) >> 1;
    height *= fontheight(p);

    bgx = ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];

    rectfill (dst, bytes,  height, width, bgx);
}

void fbcon_cfb12_putc(struct vc_data *conp, struct display *p, int c, int yy,
		      int xx)
{
    u8 *d, *dst, *fnt;
    int bits, fw = fontwidth(p), fh = fontheight(p), i;
    int fgx, bgx, f0, f1, f2, b0, b1, b2;
    u8 bf[4];
    int bytes = p->next_line;
    int linesize = bytes * fontheight(p);

    dst = p->screen_base + yy * linesize + ((xx * fw * 3) >> 1);

    fgx = ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
    fgx |= fgx << 12;
    f0 = (fgx >>  0) & 0xFF;
    f1 = (fgx >>  8) & 0xFF;
    f2 = (fgx >> 16) & 0xFF;

    bgx = ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
    bgx |= bgx << 12;
    b0 = (bgx >>  0) & 0xFF;
    b1 = (bgx >>  8) & 0xFF;
    b2 = (bgx >> 16) & 0xFF;

    bf[0] = b1;
    bf[1] = (b1 & 0x0F) | (f1 & 0xF0);
    bf[2] = (f1 & 0x0F) | (b1 & 0xF0);
    bf[3] = f1;

    c &= p->charmask;

    switch (fw) {
    case 4:
    case 6:
    case 8:
        fnt = p->fontdata + c * fh;
        for (i = fh ; i ; i--, dst += bytes) {
            d = dst;
            bits = *fnt++;
            fb_writeb ((bits & 0x80) ? f0 : b0, d++);
            fb_writeb (bf[(bits & 0xC0) >> 6], d++);
            fb_writeb ((bits & 0x40) ? f2 : b2, d++);
            fb_writeb ((bits & 0x20) ? f0 : b0, d++);
            fb_writeb (bf[(bits & 0x30) >> 4], d++);
            fb_writeb ((bits & 0x10) ? f2 : b2, d++);
            if (fw >= 6) {
                fb_writeb ((bits & 0x08) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0x0C) >> 2], d++);
                fb_writeb ((bits & 0x04) ? f2 : b2, d++);
                if (fw == 8) {
                    fb_writeb ((bits & 0x02) ? f0 : b0, d++);
                    fb_writeb (bf[(bits & 0x03) >> 0], d++);
                    fb_writeb ((bits & 0x01) ? f2 : b2, d++);
                }
            }
        }
        break;
            
    case 10:
    case 12:
    case 14:
    case 16:
        fnt = p->fontdata + ((c * fh) << 1);
        for (i = fh ; i ; i--, dst += bytes) {
            d = dst;
            bits = *fnt++;
            fb_writeb ((bits & 0x80) ? f0 : b0, d++);
            fb_writeb (bf[(bits & 0xC0) >> 6], d++);
            fb_writeb ((bits & 0x40) ? f2 : b2, d++);
            fb_writeb ((bits & 0x20) ? f0 : b0, d++);
            fb_writeb (bf[(bits & 0x30) >> 4], d++);
            fb_writeb ((bits & 0x10) ? f2 : b2, d++);
            fb_writeb ((bits & 0x08) ? f0 : b0, d++);
            fb_writeb (bf[(bits & 0x0C) >> 2], d++);
            fb_writeb ((bits & 0x04) ? f2 : b2, d++);
            fb_writeb ((bits & 0x02) ? f0 : b0, d++);
            fb_writeb (bf[(bits & 0x03) >> 0], d++);
            fb_writeb ((bits & 0x01) ? f2 : b2, d++);
            bits = *fnt++;
            fb_writeb ((bits & 0x80) ? f0 : b0, d++);
            fb_writeb (bf[(bits & 0xC0) >> 6], d++);
            fb_writeb ((bits & 0x40) ? f2 : b2, d++);
            if (fw >= 12) {
                fb_writeb ((bits & 0x20) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0x30) >> 4], d++);
                fb_writeb ((bits & 0x10) ? f2 : b2, d++);
                if (fw >= 14) {
                    fb_writeb ((bits & 0x08) ? f0 : b0, d++);
                    fb_writeb (bf[(bits & 0x0C) >> 2], d++);
                    fb_writeb ((bits & 0x04) ? f2 : b2, d++);
                    if (fw == 16) {
                        fb_writeb ((bits & 0x02) ? f0 : b0, d++);
                        fb_writeb (bf[(bits & 0x03) >> 0], d++);
                        fb_writeb ((bits & 0x01) ? f2 : b2, d++);
                    }
                }
            }
        }
        break;
    }
}

void fbcon_cfb12_putcs(struct vc_data *conp, struct display *p,
		       const unsigned short *s, int count, int yy, int xx)
{
    u8 *d, *d2, *dst, *fnt;
    int bits, fw = fontwidth(p), fh = fontheight(p), i, c;
    int fgx, bgx, f0, f1, f2, b0, b1, b2;
    u8 bf[4];
    int bytes = p->next_line;
    int linesize = bytes * fontheight(p);

    dst = p->screen_base + yy * linesize + ((xx * fw * 3) >> 1);

    c = scr_readw (s);

    fgx = ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
    fgx |= fgx << 12;
    f0 = (fgx >>  0) & 0xFF;
    f1 = (fgx >>  8) & 0xFF;
    f2 = (fgx >> 16) & 0xFF;

    bgx = ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
    bgx |= bgx << 12;
    b0 = (bgx >>  0) & 0xFF;
    b1 = (bgx >>  8) & 0xFF;
    b2 = (bgx >> 16) & 0xFF;

    bf[0] = b1;
    bf[1] = (b1 & 0x0F) | (f1 & 0xF0);
    bf[2] = (f1 & 0x0F) | (b1 & 0xF0);
    bf[3] = f1;

    switch (fw) {
    case 4:
    case 6:
    case 8:
        while (count-- > 0) {
            c = scr_readw (s++) & p->charmask;
            fnt = p->fontdata + c * fh;
            for (i = fh, d2 = dst ; i ; i--, d2 += bytes) {
                d = d2;
                bits = *fnt++;
                fb_writeb ((bits & 0x80) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0xC0) >> 6], d++);
                fb_writeb ((bits & 0x40) ? f2 : b2, d++);
                fb_writeb ((bits & 0x20) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0x30) >> 4], d++);
                fb_writeb ((bits & 0x10) ? f2 : b2, d++);
                if (fw >= 6) {
                    fb_writeb ((bits & 0x08) ? f0 : b0, d++);
                    fb_writeb (bf[(bits & 0x0C) >> 2], d++);
                    fb_writeb ((bits & 0x04) ? f2 : b2, d++);
                    if (fw == 8) {
                        fb_writeb ((bits & 0x02) ? f0 : b0, d++);
                        fb_writeb (bf[(bits & 0x03) >> 0], d++);
                        fb_writeb ((bits & 0x01) ? f2 : b2, d++);
                    }
                }
            }
            dst += (fw * 3) >> 1;
        }
        break;
            
    case 10:
    case 12:
    case 14:
    case 16:
        while (count-- > 0) {
            c = scr_readw (s++) & p->charmask;
            fnt = p->fontdata + ((c * fh) << 1);
            for (i = fh, d2 = dst ; i ; i--, d2 += bytes) {
                d = d2;
                bits = *fnt++;
                fb_writeb ((bits & 0x80) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0xC0) >> 6], d++);
                fb_writeb ((bits & 0x40) ? f2 : b2, d++);
                fb_writeb ((bits & 0x20) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0x30) >> 4], d++);
                fb_writeb ((bits & 0x10) ? f2 : b2, d++);
                fb_writeb ((bits & 0x08) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0x0C) >> 2], d++);
                fb_writeb ((bits & 0x04) ? f2 : b2, d++);
                fb_writeb ((bits & 0x02) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0x03) >> 0], d++);
                fb_writeb ((bits & 0x01) ? f2 : b2, d++);
                bits = *fnt++;
                fb_writeb ((bits & 0x80) ? f0 : b0, d++);
                fb_writeb (bf[(bits & 0xC0) >> 6], d++);
                fb_writeb ((bits & 0x40) ? f2 : b2, d++);
                if (fw >= 12) {
                    fb_writeb ((bits & 0x20) ? f0 : b0, d++);
                    fb_writeb (bf[(bits & 0x30) >> 4], d++);
                    fb_writeb ((bits & 0x10) ? f2 : b2, d++);
                    if (fw >= 14) {
                        fb_writeb ((bits & 0x08) ? f0 : b0, d++);
                        fb_writeb (bf[(bits & 0x0C) >> 2], d++);
                        fb_writeb ((bits & 0x04) ? f2 : b2, d++);
                        if (fw == 16) {
                            fb_writeb ((bits & 0x02) ? f0 : b0, d++);
                            fb_writeb (bf[(bits & 0x03) >> 0], d++);
                            fb_writeb ((bits & 0x01) ? f2 : b2, d++);
                        }
                    }
                }
            }
            dst += (fw * 3) >> 1;
        }
        break;
    }
}

void fbcon_cfb12_revc(struct display *p, int xx, int yy)
{
    u8 *d, *dst;
    int w, h, i;
    int bytes = p->next_line;
    int linesize = bytes * fontheight(p);

    dst = p->screen_base + yy * linesize + ((xx * fontwidth(p) * 3) >> 1);

    w = (fontwidth(p) * 3) >> 1;
    h = fontheight(p);

    for ( ; h ; h--,  dst += bytes) {
        for (i = w, d = dst ; i ; i--, d++) {
            fb_writeb (~fb_readb (d), d);
        }
    }
}

void fbcon_cfb12_clear_margins(struct vc_data *conp, struct display *p,
			       int bottom_only)
{
    u8 *dst;
    int rs, bs, rw, bw, bgx;
    int bytes = p->next_line;

    bgx = ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
    rs = conp->vc_cols * fontwidth(p);
    rw = p->var.xres - rs;
    bs = conp->vc_rows * fontheight(p);
    bw = p->var.yres - bs;

    if (!bottom_only && rw) {
        dst = p->screen_base + (rs >> 1);
        rectfill(dst, bytes, p->var.yres_virtual, rw >> 1, bgx);
    }
    if (bw) {
        dst = p->screen_base + (p->var.yoffset + bs) * bytes;
        rectfill(dst, bytes, bw, rs >> 1, bgx);
    }
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_cfb12 = {
    setup:		fbcon_cfb12_setup,
    bmove:		fbcon_cfb12_bmove,
    clear:		fbcon_cfb12_clear,
    putc:		fbcon_cfb12_putc,
    putcs:		fbcon_cfb12_putcs,
    revc:		fbcon_cfb12_revc,
    clear_margins:	fbcon_cfb12_clear_margins,
    fontwidthmask:	FONTWIDTH(4) | FONTWIDTH(6) |
                        FONTWIDTH(8) | FONTWIDTH(10) |
                        FONTWIDTH(12) | FONTWIDTH(14) |
                        FONTWIDTH(16)
};


#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{
}
#endif /* MODULE */


    /*
     *  Visible symbols for modules
     */

EXPORT_SYMBOL(fbcon_cfb12);
EXPORT_SYMBOL(fbcon_cfb12_setup);
EXPORT_SYMBOL(fbcon_cfb12_bmove);
EXPORT_SYMBOL(fbcon_cfb12_clear);
EXPORT_SYMBOL(fbcon_cfb12_putc);
EXPORT_SYMBOL(fbcon_cfb12_putcs);
EXPORT_SYMBOL(fbcon_cfb12_revc);
EXPORT_SYMBOL(fbcon_cfb12_clear_margins);
