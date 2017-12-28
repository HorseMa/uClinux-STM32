/*
 *  linux/arch/arm/mach-stm3210eb/stm3210e_eval.c
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>

#include <linux/input.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/leds.h>
#include <asm/mach-types.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/nvic.h>

#include <asm/mach/arch.h>

#include <asm/mach/time.h>

#include <asm/arch/irqs.h>
#include <asm/arch/stm_sd.h>

#include <asm/arch/stm32f10x_conf.h>

#include "time.h"

#if defined(CONFIG_CPU_V7M) && (PAGE_OFFSET != PHYS_OFFSET)
#error "PAGE_OFFSET != PHYS_OFFSET"
#endif

#ifdef	CONFIG_GPIO_STM3210E_EVAL
static struct resource stm3210e_eval_gpios_resources = {
	.start = 0,
	.end   = 15,
	.flags = IORESOURCE_IRQ,
};

static struct platform_device stm3210e_eval_gpios_device = {
	.name = "simple-gpio",
	.id = -1,
	.num_resources = 1,
	.resource = &stm3210e_eval_gpios_resources,
};
#endif /* GPIO_STM3210E_EVAL */


#ifdef CONFIG_RTC_STM3210E_EVAL
static struct platform_device rtc_device = {
	.name	= "rtc-stm32f10x",
	.id	= 0,
};
#endif /* RTC_STM3210E_EVAL */


#ifdef USB_GADGET_STM32F10x

#define RegBase  (0x40005C00L)  /* USB_IP Peripheral Registers base address */
#define	USB_BASE_START	RegBase
#define USB_END		(USB_BASE_START + 0x2000)

static struct resource stm32f10x_usbclient_resources[] = {
	[0] = {
		.start	= USB_BASE_START,
		.end	= USB_END,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device stm32f10x_usbclient = {
	.name		= "stm32f10x_udc",
	.id		= -1,
	.dev		= {
	},
	.num_resources	= 1,
	.resource	= stm32f10x_usbclient_resources,

};

#endif /* USB_GADGET_STM32F10x */

static void __init nvic_init_irq(void)
{
	nvic_init();
}

static void __init stm3210e_eval_timer_init(void)
{	
	stm3210e_eval_timers_init(TIMx_IRQChannel);
}

static struct sys_timer stm3210e_eval_timer = {
	.init		= stm3210e_eval_timer_init,
};


static void __init stm3210e_eval_fixup(struct machine_desc *desc, struct tag *tags, char **cmdline, struct meminfo *mi)
{
#if (CONFIG_DRAM_BASE != 0x20000000) || ((CONFIG_EXTERNAL_RAM_BASE & 0xF0000000) != 0x60000000)
#error Please verify your configuration. DRAM_BASE && EXTERNAL_RAM_BASE.
#endif
	/* give values to mm */
	
	mi->nr_banks      = 2;

	/* Bank 1 */
	mi->bank[0].start = (unsigned int) CONFIG_EXTERNAL_RAM_BASE;/* external RAM base address */
	mi->bank[0].size  = (unsigned int) CONFIG_EXTERNAL_RAM_SIZE;/* the size of the enternal RAM */
	mi->bank[0].node  = 0;
	
	/* Bank 2 */
	mi->bank[1].start = (unsigned int) CONFIG_DRAM_BASE;/* internal RAM base address */
	mi->bank[1].size  = CONFIG_DRAM_SIZE;/* the size of the internal RAM */
	mi->bank[1].node  = 1;
	
	
	
	/* print message */
	printk("SRAM Config: bank[0] @ 0x%08x (size: %dKB) - bank[1] @ 0x%08x (size: %dKB).\n",
		(unsigned int) mi->bank[0].start, ((unsigned int) mi->bank[0].size/SZ_1K), 
		 (unsigned int)mi->bank[1].start, ((unsigned int) (mi->bank[1].size) / SZ_1K));
}

static struct platform_device *stm3210e_eval_devs[] __initdata = {
#ifdef	CONFIG_GPIO_STM3210E_EVAL
	&stm3210e_eval_gpios_device,
#endif /* GPIO_STM3210E_EVAL */
//#ifdef MMC_STM3210E_EVAL
//	&stm32_sd_host_data,
//#endif /* MMC_STM3210E_EVAL */
#ifdef CONFIG_USB_GADGET_STM32F10x
	&stm32f10x_usbclient,
#endif /* USB_GADGET_STM32F10x */
#ifdef CONFIG_RTC_STM3210E_EVAL
	&rtc_device,
#endif /* RTC_STM3210E_EVAL */
};





static void __init stm3210e_eval_init(void) 
{
	platform_add_devices(stm3210e_eval_devs, ARRAY_SIZE(stm3210e_eval_devs));

}

MACHINE_START(STM3210E_EVAL, "STM3210E-EVAL")
	/* Maintainer: MCD Application TEAM */					/* Status */
	.fixup		= stm3210e_eval_fixup,					//OK
	.boot_params	= CONFIG_EXTERNAL_RAM_BASE + 0x100,			//OK
	.init_irq	= nvic_init_irq,					//OK
	.timer		= &stm3210e_eval_timer,					//OK
	.init_machine	= stm3210e_eval_init,					//OK
MACHINE_END
