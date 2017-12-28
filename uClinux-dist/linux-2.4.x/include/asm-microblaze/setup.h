/*
 * include/asm-microblaze/setup.h
 *
 *  Copyright (C) 2003  John Williams <jwilliams@itee.uq.edu.au>
 *  Copyright (C) 2001  NEC Corporation
 *  Copyright (C) 2001  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 * Microblaze port by John Williams
 */

#ifndef __MICROBLAZE_SETUP_H__
#define __MICROBLAZE_SETUP_H__

/*
 * Memory map description
 */
#define NR_BANKS 8

struct meminfo {
	int nr_banks;
	unsigned long end;
	struct {
		unsigned long start;
		unsigned long size;
		int           node;
	} bank[NR_BANKS];
};

extern struct meminfo meminfo;


#endif /* __MICROBLAZE_SETUP_H__ */
