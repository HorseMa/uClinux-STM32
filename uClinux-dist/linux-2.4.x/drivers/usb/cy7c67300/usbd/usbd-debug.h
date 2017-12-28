/*
 * linux/drivers/usbd/usbd-debug.h
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *      Bruce Balden <balden@lineo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef USBD_DEBUG_H
#define USBD_DEBUG_H 1
/*
 * Generic, multi-area, multi-level, runtime settable debug printing.
 * It isn't the greatest by any means, but it's relatively simple
 * and it does the job for code collections of < 20K lines.
 */

#define DBG_ENABLE 1
// #undef DBG_ENABLE

typedef struct debug_option {
        int *level;
        struct debug_option *sub_table;  // used instead of level if name starts with /
        char *name;
        char *description;
} debug_option;

extern int scan_debug_options(
        char *caller_name,
        debug_option *options,
        char *values);

extern debug_option *find_debug_option(
        debug_option *options,
        char *opt2find);

#define KLEVEL KERN_INFO
#define PRINTK(fmt,args...) printk(KLEVEL fmt,##args)

#if DBG_ENABLE

/* Debug enabled, macros are real. */

#define dbgENTER(flg,lvl) do { if (flg >= lvl) { PRINTK("Enter %s\n",__FUNCTION__); } } while (0)

#define dbgLEAVE(flg,lvl) do { if (flg >= lvl) { PRINTK("Leave %s\n",__FUNCTION__); } } while (0)

extern void dbgPRINT_mem(u8 *mem, u32 len);
#define dbgPRINTmem(flg,lvl,addr,len) do { if (flg >= lvl) { dbgPRINT_mem((u8*)(void*)(addr),len); } } while (0)

// Since PRINTK adds KLEVEL, we might as well add the newline here,
// because continued lines will look a mess anyway.
#define dbgPRINT(flg,lvl,fmt,args...) do { if (flg >= lvl) { PRINTK("%s: " fmt "\n" , __FUNCTION__ , ##args); } } while (0)

#define dbgLEAVEF(flg,lvl,fmt,args...) do { if (flg >= lvl) { PRINTK("Leave %s: " fmt "\n" ,__FUNCTION__ , ##args); } } while (0)

#define DBGprint(fmt,args...)  printk(KERN_INFO "%s: " fmt "\n",__FUNCTION__,##args)
#define DBGmark                printk(KERN_INFO "%s: line %d\n",__FUNCTION__,__LINE__)
#define DBGmarks(str)          printk(KERN_INFO "%s: %s\n",__FUNCTION__,str)

#else

/*  Debug disabled, define the macros to be empty.  This will leave
    a bunch of single semi-colons in the post-processed source, but
    the C language allows it, so compilers had better be able to
    handle it.  It's also another good reason for compulsory {}'s :). */

#define dbgENTER(flg,lvl)
#define dbgLEAVE(flg,lvl)
#define dbgPRINTmem(flg,lvl,addr,len)
#define dbgPRINT(flg,lvl,fmt,args...)
#define dbgLEAVEF(flg,lvl,fmt,args...)
#endif

#endif
