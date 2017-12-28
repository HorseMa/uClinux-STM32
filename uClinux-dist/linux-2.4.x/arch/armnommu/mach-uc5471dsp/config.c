/*
 *  linux/arch/armnommu/$(MACHINE)/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *  Copyright (C) 1999 D. Jeff Dionne
 *  Copyright (c) 2001-2003 Arcturus Networks Inc.
 *  		  by Oleksandr Zhadan <oleks@arcturusnetworks.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <stdarg.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/init.h>
#include <asm/uCbootstrap.h>

#undef CONFIG_DEBUG

/* define starting location of user flash on uC5471DSP: */
char *_image_start = (char *)(0x00020000);

#if defined(CONFIG_UCBOOTSTRAP)
int	ConTTY = 0;
static int errno;
_bsc0(char *, getserialnum);
_bsc1(unsigned char *, gethwaddr, int, a);
_bsc1(char *, getbenv, char *, a);

unsigned char fec_hwaddr[6];
#endif


void config_BSP(void)
{
#if defined(CONFIG_UCBOOTSTRAP)
	unsigned char *p;

	printk("Arcturus Networks Inc. uC5471DSP uCbootstrap system calls\n");
	printk("   Serial number: [%s]\n", getserialnum());

	p = gethwaddr(0);
	if (p == NULL) {
		memcpy (fec_hwaddr, "\0x00\0xde\0xad\0xbe\0xef\0x27", 6);
		p = fec_hwaddr;
		printk ("Warning: HWADDR0 not set in uCbootloader. Using default.\n");
	} else {
		memcpy (fec_hwaddr, p, 6);
	}
	printk("uCbootstrap eth addr 0 %02x:%02x:%02x:%02x:%02x:%02x\n",
		p[0], p[1], p[2], p[3], p[4], p[5]);

#endif
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
