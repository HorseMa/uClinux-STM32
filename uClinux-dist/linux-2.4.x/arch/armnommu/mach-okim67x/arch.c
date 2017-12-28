/*
 *  linux/arch/arm/mach-okim67x/arch.c
 *
 *  (c)2005 Simtec Electronics
 *
 *  Architecture specific fixups.  This is where any
 *  parameters in the params struct are fixed up, or
 *  any additional architecture specific information
 *  is pulled from the params struct.
 */
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/config.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/hardware.h>
#include <asm/arch/cache.h>
#include <asm/io.h>

extern void genarch_init_irq(void);

int oki_ml67500x = 0;
int oki_ml67400x = 0;

int oki_clk  = 0;   /* master crystal requency */
int oki_hclk = 0;
int oki_cclk = 0;

static void
oki_ml67500x_init_cache(void)
{
	unsigned long flags;
	unsigned long cache;
	int i;

	local_irq_save(flags);

	if (__raw_readl(OKI_CACHE_EN) != 0) {
		printk("M675001: Cache already enabled\n");

		local_irq_save(flags);

		cache = __raw_readl(OKI_CACHE_EN);
		cache |= CACHE_EN_C0;

		__raw_writel(cache, OKI_CACHE_EN);
	} else {

		printk("M675001: Enabling CPU cache\n");

		/* flush and init cache */

		__raw_writel(0x00, OKI_CACHE_EN);
		__raw_writel(0x00, OKI_CACHE_CON);
		__raw_writel(CACHE_FLUSH_FLUSH, OKI_CACHE_FLUSH);

		for (i = 0; i < 128; i++) {
			__raw_readw(0x00);
		}

		/* enable cache for bank0 */
		__raw_writel(CACHE_EN_C0, OKI_CACHE_EN);
	}

	local_irq_restore(flags);
}

void
oki_fixup(struct machine_desc *mdesc,
	  struct param_struct *param,
	  char **arg,
	  struct meminfo *meminfo)
{
  unsigned int bwc, tmp;
  unsigned int clk;

  /* need to determine which version of the cpu this is */

  bwc = __raw_readl(OKI_BWC);

  tmp = bwc & ~(3<<8);   /* IO23BW */

  if (tmp != bwc) {
    printk("CPU is ML67400x series\n");

    oki_ml67400x = 1;
    oki_ml67500x = 0;
    oki_clk      = 25*1000*1000;
  } else {
    printk("CPU is ML67500x series\n");

    oki_ml67400x = 0;
    oki_ml67500x = 1;
    oki_clk      = 7372800*8;

    oki_ml67500x_init_cache();
  }

  clk = __raw_readl(OKI_CGBCNT0);

#ifdef CONFIG_OKI_CCLK_HAVLED

#define OKI_RFRSH1 (0x7818001c)

  /* halve the cclk value */

  {
	  unsigned int refresh1 = __raw_readl(OKI_RFRSH1);

	  refresh1 /= 2;

	  __raw_writel(refresh1, OKI_RFRSH1);
  }

  {
	  unsigned int div_c = (clk >> 4) & 7;

	  div_c++;

	  clk &= ~(7<<4);
	  clk |= (div_c << 4);

	  __raw_writel(0x03c, OKI_CGBCNT0);
	  __raw_writel(clk, OKI_CGBCNT0);
  }
#endif

  oki_hclk = oki_clk /  OKI_CLKGR_H(clk);
  oki_cclk = oki_clk /  OKI_CLKGR_C(clk);

#define print_mhz(x) ((x) / 1000000), (((x) / 1000) % 1000)

  printk("OKI: Clk %d.%3d MHz, HClk %d.%3d MHz, CCLk %d.%3d MHz\n",
	 print_mhz(oki_clk), print_mhz(oki_hclk), print_mhz(oki_cclk));

}

MACHINE_START(EB67XDIP, "EB67X001DIP")
       MAINTAINER("Vincent Sanders, Ben Dooks")
       INITIRQ(genarch_init_irq)
       FIXUP(oki_fixup)
MACHINE_END
