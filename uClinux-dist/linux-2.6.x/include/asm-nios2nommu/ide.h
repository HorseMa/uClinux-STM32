/*
 *  linux/include/asm-niosnommu2/ide.h
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 *  Copyright (C) 2004	     Microtronix Datacom Ltd.
 *
 * All rights reserved.          
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __ASMNIOS2_IDE_H
#define __ASMNIOS2_IDE_H

#ifdef __KERNEL__
#undef MAX_HWIFS		/* we're going to force it */

#ifndef MAX_HWIFS
#define MAX_HWIFS	1
#endif

#include <asm-generic/ide_iops.h>

#endif /* __KERNEL__ */

#endif /* __ASMNIOS2_IDE_H */
