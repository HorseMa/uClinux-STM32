/*
 *  linux/arch/arm/mach-lpc28xx/arch.c
 *
 *
 *  Copyright (C) 2007 NXP
 *
 *
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

extern void __init lpc28xx_init_irq(void);
extern struct sys_timer lpc28xx_timer;

#if defined(CONFIG_RTC_DRV_LPC28XX)
static struct resource lpc28xx_rtc_resources[] = {
        {
                .name   = "lpc28xx_rtc",
                .start  = RTC_BASE_ADDR,
                .end    = RTC_BASE_ADDR+0x80-1,
                .flags  = IORESOURCE_MEM,
        },
	{
                .name   = "lpc28xx-rtc-irq",
                .start  = LPC28XX_IRQ_RTCC,
		.end	= LPC28XX_IRQ_RTCA,
                .flags  = IORESOURCE_IRQ,
        },
};
static struct platform_device lpc28xx_rtc_device = {
        .name           = "lpc28xx_rtc",
        .id             = -1,
        .num_resources  = ARRAY_SIZE(lpc28xx_rtc_resources),
        .resource       = lpc28xx_rtc_resources,
};

static void __init lpc28xx_add_rtc_device(void)
{
        platform_device_register(&lpc28xx_rtc_device);
}
#endif

#if defined(CONFIG_USB_LPC28XX)
static struct resource lpc28xx_udc_resources[] = {
        {
                .name   = "lpc28xx_udc",
                .start  = USB_BASE_ADDR,
                .end    = USB_BASE_ADDR+0x510-1,
                .flags  = IORESOURCE_MEM,
        },
        {
                .name   = "lpc28xx-rtc-irq",
                .start  = LPC28XX_IRQ_USB0,
                .end    = LPC28XX_IRQ_USB1,
                .flags  = IORESOURCE_IRQ,
        },
};
static struct platform_device lpc28xx_udc_device = {
        .name           = "lpc28xx_udc",
        .id             = -1,
        .num_resources  = ARRAY_SIZE(lpc28xx_udc_resources),
        .resource       = lpc28xx_udc_resources,
};

static void __init lpc28xx_add_udc_device(void)
{
        platform_device_register(&lpc28xx_udc_device);
}
#endif

#if defined(CONFIG_MMC_LPC28XX)
static struct resource lpc28xx_mci_resources[] = {
        {
                .name   = "lpc28xx_mci",
                .start  = MCI_BASE_ADDR,
                .end    = MCI_BASE_ADDR+0x84-1,
                .flags  = IORESOURCE_MEM,
        },
        {
                .name   = "lpc28xx-rtc-irq",
                .start  = LPC28XX_IRQ_MCI0,
                .end    = LPC28XX_IRQ_MCI1,
                .flags  = IORESOURCE_IRQ,
        },
};
static struct platform_device lpc28xx_mci_device = {
        .name           = "lpc28xx_mci",
        .id             = -1,
        .num_resources  = ARRAY_SIZE(lpc28xx_mci_resources),
        .resource       = lpc28xx_mci_resources,
};

static void __init lpc28xx_add_mci_device(void)
{
        platform_device_register(&lpc28xx_mci_device);
}
#endif

void __init lpc28xx_init_machine(void) 
{
#if defined(CONFIG_RTC_DRV_LPC28XX)
        lpc28xx_add_rtc_device();
#endif
#if defined(CONFIG_USB_LPC28XX)
	lpc28xx_add_udc_device();
#endif
#if defined(CONFIG_MMC_LPC28XX)
        lpc28xx_add_mci_device();
#endif
}

MACHINE_START(LPC22XX, "LPC28XX")
	.init_irq	= lpc28xx_init_irq,
	.init_machine = lpc28xx_init_machine,
	.boot_params	= 0x30000800,
	.timer = &lpc28xx_timer,
MACHINE_END

