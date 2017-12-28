/*
 *  linux/arch/armnommu/$(MACHINE)/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *  Copyright (C) 1999 D. Jeff Dionne
 *  Copyright (c) 2001-2003 Arcturus Networks Inc.
 *                by Oleksandr Zhadan <oleks@arcturusnetworks.com>
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

unsigned char *sn;

#if defined(CONFIG_UCBOOTSTRAP)
    int	ConTTY = 0;
    static int errno;
    _bsc0(char *, getserialnum)
    _bsc1(unsigned char *, gethwaddr, int, a)
    _bsc1(char *, getbenv, char *, a)
#endif

unsigned char *get_MAC_address(char *);

void config_BSP(void)
{
#if defined(CONFIG_UCBOOTSTRAP)
    printk("uCbootloader: Copyright (C) 2001-2005 Arcturus Networks Inc.\n");
    printk("uCbootloader system calls are initialized.\n");
#else
  sn = (unsigned char *)"123456789012345";
# ifdef	CONFIG_DEBUG
    printk("Serial number [%s]\n",sn);
# endif  
#endif

}

#ifdef CONFIG_C5471_NET

unsigned char *get_MAC_address(char *devname)
{
    char *devnum  = devname + (strlen(devname)-1);
    char *varname = "HWADDR0";
    char *varnum  = varname + (strlen(varname)-1);
    static unsigned char tmphwaddr[6] = {0, 0, 0, 0, 0, 0};
    char *ret;
    char *p;
    int	 i;

    strcpy ( varnum, devnum );

#if defined(CONFIG_UCBOOTSTRAP)
    ret = getbenv(varname);
#else
    ret = (char *)C5471_HWADDR;
#endif 
    for (p=ret, i=0; *p && i<6; i++) {
        tmphwaddr[i] = simple_strtoul(p, &p, 16);
        while (*p && (*p == ':' || *p == ' ')) p++;
    }
   
    if (i!=6) memset(tmphwaddr, 0, 6);

    return(tmphwaddr);
}

#endif /* CONFIG_C5471_NET */
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */

















