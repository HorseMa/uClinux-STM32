/*
 * YAFFS: Yet another FFS. A NAND-flash specific file system. 
 * yportenv.h: Portable services used by yaffs. This is done to allow
 * simple migration from kernel space into app space for testing.
 *
 * Copyright (C) 2002 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 *
 * Note: Only YAFFS headers are LGPL, YAFFS C code is covered by GPL.
 *
 * $Id: yportenv.h,v 1.1 2005/08/11 12:53:51 claudio Exp $
 *
 */
 
#ifndef __YPORTENV_H__
#define __YPORTENV_H__


#if defined CONFIG_YAFFS_WINCE

#include "ywinceenv.h"

#elif  defined __KERNEL__



// Linux kernel
#include "linux/kernel.h"
#include "linux/version.h"
#include "linux/mm.h"
#include "linux/string.h"
#include "linux/slab.h"


#define YAFFS_LOSTNFOUND_NAME		"lost+found"
#define YAFFS_LOSTNFOUND_PREFIX		"obj"

//#define YPRINTF(x) printk x
#define YMALLOC(x) kmalloc(x,GFP_KERNEL)
#define YFREE(x)   kfree(x)

#define YAFFS_ROOT_MODE				0666
#define YAFFS_LOSTNFOUND_MODE		0666

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
#define Y_CURRENT_TIME CURRENT_TIME.tv_sec
#define Y_TIME_CONVERT(x) (x).tv_sec
#else
#define Y_CURRENT_TIME CURRENT_TIME
#define Y_TIME_CONVERT(x) (x)
#endif

#define yaffs_SumCompare(x,y) ((x) == (y))
#define yaffs_strcmp(a,b) strcmp(a,b)

#define TENDSTR "\n"
#define TSTR(x) KERN_DEBUG x
#define TOUT(p) printk p


#elif defined CONFIG_YAFFS_DIRECT

// Direct interface
#include "ydirectenv.h"

#elif defined CONFIG_YAFFS_UTIL

// Stuff for YAFFS utilities

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include "devextras.h"

#define YMALLOC(x) malloc(x)
#define YFREE(x)   free(x)


//#define YINFO(s) YPRINTF(( __FILE__ " %d %s\n",__LINE__,s))
//#define YALERT(s) YINFO(s)


#define TENDSTR "\n"
#define TSTR(x) x
#define TOUT(p) printf p


#define YAFFS_LOSTNFOUND_NAME		"lost+found"
#define YAFFS_LOSTNFOUND_PREFIX		"obj"
//#define YPRINTF(x) printf x


#define CURRENT_TIME 0
#define YAFFS_ROOT_MODE				0666
#define YAFFS_LOSTNFOUND_MODE		0666

#define yaffs_SumCompare(x,y) ((x) == (y))
#define yaffs_strcmp(a,b) strcmp(a,b)

#else
// Should have specified a configuration type
#error Unknown configuration

#endif 


extern unsigned yaffs_traceMask;

#define YAFFS_TRACE_ERROR		0x0001
#define YAFFS_TRACE_OS			0x0002
#define YAFFS_TRACE_ALLOCATE	0x0004
#define YAFFS_TRACE_SCAN		0x0008
#define YAFFS_TRACE_BAD_BLOCKS	0x0010
#define YAFFS_TRACE_ERASE		0x0020
#define YAFFS_TRACE_GC			0x0040
#define YAFFS_TRACE_TRACING		0x0100
#define YAFFS_TRACE_ALWAYS		0x0200
#define YAFFS_TRACE_BUG			0x8000

#define T(mask,p) do{ if((mask) & (yaffs_traceMask | YAFFS_TRACE_ERROR)) TOUT(p);} while(0) 


#ifndef CONFIG_YAFFS_WINCE
#define YBUG() T(YAFFS_TRACE_BUG,(TSTR("==>> yaffs bug: " __FILE__ " %d" TENDSTR),__LINE__))
#endif

#endif


