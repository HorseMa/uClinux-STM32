/*
 * linux/drivers/usbd/usbd-debug.c
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
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "usbd-debug.h"
static int gn(char *cp)
{
    /* Dumb little number scanner to avoid stdlib (atoi). */
    int val = 0;
    char d;
    while ('0' <= (d = *cp++) && d <= '9') {
        val = val * 10 + (d - '0');
    }
    return(val);
}

static char *ss(char *str, char c)
{
    /* Dumb little string scanner to avoid stdlib (strchr). */
    char d;
    do {
        if (c == (d = *str)) {
            return(str);
        }
        str += 1;
    } while (0 != d);
    return(NULL);
}

static char *sc(char *dst, char *src)
{
    /* Dumb little string copier to avoid stdlib (~strcpy). */
    char c;
    while (0 != (c = *src++)) {
        *dst++ = c;
    }
    *dst = c;  // Terminate destination str, but don't point past 0
    return(dst);
}

static int se(char *s1, char *s2)
{
    /* Dumb little string comparer to avoid stdlib (strcmp). */
    char c1,c2;
    do {
        if ((c1 = *s1++) != (c2 = *s2++)) {
            return(c1-c2);
        }
    } while (0 != c1);
    return(0);
}

debug_option *find_debug_option(
        debug_option *options,
        char *opt2find)
{
    debug_option *op;
    char *sl;
    int r;
    for (op = options; NULL != op && NULL != op->name; op++) {
        if (NULL != op->sub_table) {
            /* This option is the name of a sub-table, check
               the prefix of opt2find (using ss, to avoid stdlib) */
            if (NULL != (sl = ss(opt2find,'/'))) {
                *sl = 0;
            }
            r = se(opt2find,op->name);
            if (NULL != sl) {
                *sl = '/';
            }
            if (0 == r) {
                /* Got a match. */
                if (NULL == sl) {
                    /* Match was to name of entire table. */
                    return(op);
                }
                return(find_debug_option(op->sub_table,sl+1));
            }
        } else if (0 == se(opt2find,op->name)) {
            return(op);
        }
    }
    return(NULL);
}

static void print_options(
        debug_option *options,
        char *ps,
        char *pe)
{
    debug_option *op;
    char *npe;
    for (op = options; NULL != op && NULL != op->name; op++) {
        if (NULL == op->sub_table) {
            printk(KERN_ERR "%s%s - %s\n",
                   ps,op->name,op->description);

        } else {
            npe = sc(pe,op->name);
            npe = sc(npe,"/");
            print_options(op->sub_table,ps,npe);
            *pe = 0;
        }
    }
}

static void print_all_options(
        char *caller_name,
        debug_option *options)
{
    char opt_prefix[100],
         *prefix_end;
    prefix_end = sc(opt_prefix,caller_name);
    prefix_end = sc(prefix_end,":    ");
    print_options(options,opt_prefix,prefix_end);
}

static void set_all_options(
        debug_option *options,
        int level)
{
    debug_option *op;
    for (op = options; NULL != op && NULL != op->name; op++) {
        if (NULL == op->sub_table) {
            *(op->level) = level;
        } else {
            set_all_options(op->sub_table,level);
        }
    }
}

static int set_debug_option(
        char *caller_name,
        debug_option *options,
        char *value)
{
    debug_option *op;
    int level;
    char *eq;
    if (NULL != (eq = ss(value,'='))) {
        /* There is an '='. */
        *eq = 0;
        level = gn(eq+1);
    } else if (!('0' <= *value && *value <= '9')) {
        /* name with no '=', default to level 1. */
        level = 1;
    } else {
        /* level with no name, set all options to level. */
        level = gn(value);
        set_all_options(options,level);
        return(0);
    }
    op = find_debug_option(options,value);
    if (NULL != eq) {
        *eq = '=';
    }
    if (NULL != op) {
        /* Got a match. */
        if (NULL != op->level) {
            *(op->level) = level;
        } else {
            /* Value matched an entire sub_table */
            set_all_options(op,level);
        }
        return(0);
    }
    /* Unknown debug option. */
    if (NULL == caller_name) {
        /* Silently ignore it. */
        return(0);
    }
    printk(KERN_ERR "%s: unknown dbg option `%s', valid options are:\n",
           caller_name,value);
    print_all_options(caller_name,options);
    return(1);
}

int scan_debug_options(char *caller_name, debug_option *options, char *values)
{
    char *cp;
    int rc = 0;
    if (NULL == values || NULL == options || NULL == caller_name) {
        return(0);
    }
    if ('~' == *values) {
        /* Ignore unknown options. */
        values += 1;
        caller_name = NULL;
    }
    /* Pick apart a colon separated list of option=value. */
    while (NULL != values && 0 != *values) {
        if (NULL != (cp = ss(values,':'))) {
            *cp = 0;
        }
        rc += set_debug_option(caller_name,options,values);
        if (NULL != cp) {
            *cp++ = ':';
        }
        values = cp;
    }
    return(rc);
}

static char to_hex[] = {"0123456789ABCDEF"};

void dbgPRINT_mem(u8 *mem, u32 len)
{
    /* Display a portion of memory. */
    u8 dbg_xbuff[52];
    u8 dbg_sbuff[52];
    u8 dbg_cbuff[20];
    u8 b,*end,*mark;
    u8 *dp,*cp;
    int byte_count;
    end = (mark = mem) + len;
    dp = dbg_xbuff;
    cp = dbg_cbuff;
    byte_count = 0;
    while (mem < end) {
        if (byte_count > 0) {
            /* Space between bytes. */
            *dp++ = ' ';
            if (!(0x3 & (unsigned)(void*)mem)) {
                /* Add an extra space on 4-byte boundary. */
                *dp++ = ' ';
            }
        }
        b = *mem++;
        *dp++ = to_hex[b>>4];
        *dp++ = to_hex[b&15];
        *cp++ = (b < ' ' || '~' < b) ? '.' : b;
        byte_count += 1;
        if (byte_count >= 16) {
            /* Dump this line. */
            *cp = *dp = 0;
            PRINTK("    #%08x |%s| |%s|\n",
                   (unsigned)(void*)mark,dbg_xbuff,dbg_cbuff);
            byte_count = 0;
            dp = dbg_xbuff;
            cp = dbg_cbuff;
            mark = mem;
        }
    }
    if (byte_count > 0) {
        /* Dump last line. */
        *cp = *dp = 0;
        // Find number of spaces needed for vertical alignment
        // Why am I wasting time on this? :)
        memset(dbg_sbuff,' ',sizeof(dbg_sbuff));
        dbg_sbuff[50 - (byte_count*3 - 1 + ((byte_count-1)/4))] = 0;
        PRINTK("    #%08x |%s|%s |%s|\n",
               (unsigned)(void*)mark,dbg_xbuff,dbg_sbuff,dbg_cbuff);
    }
}
