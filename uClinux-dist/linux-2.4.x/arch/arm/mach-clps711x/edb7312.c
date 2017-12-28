/*
 *  linux/arch/arm/mach-clps711x/edb7312.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>

#include <asm/setup.h>
#include <asm/hardware.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/hardware/clps7111.h>
#include <asm/io.h>
 
extern void clps711x_init_irq(void);
extern void clps711x_map_io(void); 

#define MB1 1048576	/* one megabyte == size of an MMU section */

/*
 * The on-chip registers are given a size of 1MB so that a section can
 * be used to map them; this saves a page table.  This is the place to
 * add mappings for ROM, expansion memory, PCMCIA, etc.  (if static
 * mappings are chosen for those areas).
 *
 * Here is a physical memory map (to be fleshed out later):
 *
 * Physical Address  Size  Description
 * ----------------- ----- ---------------------------------
 * c0000000-c001ffff 128KB reserved for video RAM [1]
 * c0020000-c0023fff  16KB parameters (see Documentation/arm/Setup)
 * c0024000-c0027fff  16KB swapper_pg_dir (task 0 page directory)
 * c0028000-...            kernel image (TEXTADDR)
 *
 * [1] Unused pages should be given back to the VM; they are not yet.
 *     The parameter block should also be released (not sure if this
 *     happens).
 */
static struct map_desc edb7312_io_desc[] __initdata = {
 /* virtual, physical, length, domain, r, w, c, b */

 /* FLASH is 16 MB */
 { EP7312_VIRT_CS0, CS0_PHYS_BASE, MB1 * 16, DOMAIN_IO, 0, 1, 0, 0 }, 
 { EP7312_VIRT_CS1, CS1_PHYS_BASE, MB1, DOMAIN_IO, 0, 1, 0, 0 },
 { EP7312_VIRT_CS2, CS2_PHYS_BASE, MB1, DOMAIN_IO, 0, 1, 0, 0 },
 { EP7312_VIRT_CS3, CS3_PHYS_BASE, MB1, DOMAIN_IO, 0, 1, 0, 0 },
 { EP7312_VIRT_CS4, CS4_PHYS_BASE, MB1, DOMAIN_IO, 0, 1, 0, 0 },
 { EP7312_VIRT_CS5, CS5_PHYS_BASE, MB1, DOMAIN_IO, 0, 1, 0, 0 },
 { EP7312_VIRT_CS6, CS6_PHYS_BASE, MB1, DOMAIN_IO, 0, 1, 0, 0 },
 { EP7312_VIRT_CS7, CS7_PHYS_BASE, MB1, DOMAIN_IO, 0, 1, 0, 0 },

 LAST_DESC
};


void __init edb7312_map_io(void)
{
        clps711x_map_io();
        iotable_init(edb7312_io_desc);

	/* set wait states and nCSx widths (IDE is default 16-bit) */
	clps_writel(0x1F101710, MEMCFG1);
	clps_writel(0x00001C03, MEMCFG2);

	/* set EXCKEN = 1 */
	clps_writel(clps_readl(SYSCON1) | 0x00040000, SYSCON1);
#if 1
	/* set PIO mode 3 timing - 180ns per access */
	__raw_writeb (0, EDB7312_VIRT_IDE + 0x00010000);
#endif

	/* clear eventual "suspend change" interrupt */
	__raw_writeb(0xF4, EDB7312_VIRT_PDIUSBD12 + 1);
	__raw_readb(EP7312_VIRT_CS3 + 0x0004);
	__raw_readb(EP7312_VIRT_CS3 + 0x0004);
	__raw_readb(EDB7312_VIRT_PDIUSBD12 + 0);
	__raw_readb(EP7312_VIRT_CS3 + 0x0004);
	__raw_readb(EP7312_VIRT_CS3 + 0x0004);
	__raw_readb(EDB7312_VIRT_PDIUSBD12 + 0);
}

MACHINE_START(EDB7312, "EDB7312")
	MAINTAINER("Nucleus Systems, Ltd.")
        BOOT_MEM(0xc0000000, 0x80000000, 0xff000000)
	BOOT_PARAMS(0xc0030000)
	MAPIO(edb7312_map_io)
	INITIRQ(clps711x_init_irq)
MACHINE_END
