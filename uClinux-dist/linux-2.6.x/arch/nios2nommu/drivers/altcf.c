/*
 *  linux/drivers/ide/altcf.c
 *  Support for Altera CompactFlash core with Avalon interface.
 *
 *  Copyright (C) 2004 Microtronix Datacom Ltd
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 *  Written by Wentao Xu <wentao@microtronix.com>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/nios.h>

MODULE_AUTHOR("Microtronix Datacom Ltd.");
MODULE_DESCRIPTION("Driver of Altera CompactFlash core with Avalon interface");
MODULE_LICENSE("GPL");

#define PDEBUG printk
/* Altera Avalon Compact Flash core registers */
#define REG_CFCTL		0
#define REG_IDECTL		4

/* CFCTL bits */
#define CFCTL_DET		1		/* detect status	*/
#define CFCTL_PWR		2		/* Power			*/
#define CFCTL_RST		4		/* Reset 			*/
#define CFCTL_IDET		8		/* Detect int enable*/

/* IDECTL bits */
#define IDECTL_IIDE		1		/* IDE int enable */

struct cf_dev {
	int base;
	int irq;
	int ide_base;
	int ide_irq;
	int configured;
	ide_hwif_t *hwif;
	struct delayed_work wcf;
};

static struct cf_dev cf_devices[MAX_HWIFS] = {
#if MAX_HWIFS > 0
	{na_ide_ctl, na_ide_ctl_irq, na_ide_ide, na_ide_ide_irq, 0, NULL},
#endif
#if MAX_HWIFS > 1
	{na_ctl_base1, na_ctl_irq1, na_ide_base1, na_ide_irq1, 0, NULL},
#endif
#if MAX_HWIFS > 2
	{na_ctl_base2, na_ctl_irq2, na_ide_base2, na_ide_irq2, 0, NULL},
#endif
#if MAX_HWIFS > 3
	{na_ctl_base3, na_ctl_irq3, na_ide_base3, na_ide_irq3, 0, NULL},
#endif
};

static inline void cf_init_hwif_ports(hw_regs_t *hw,
				       unsigned long io_addr,
				       unsigned long ctl_addr,
				       int *irq)
{
	unsigned int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++)
		hw->io_ports[i] = io_addr + 4*(i-IDE_DATA_OFFSET);

	hw->io_ports[IDE_CONTROL_OFFSET] = ctl_addr;

	if (irq)
		*irq = 0;

	hw->io_ports[IDE_IRQ_OFFSET] = 0;

}

static int cf_release(struct cf_dev* dev)
{
	if (dev) {
	    if ((dev->configured) && (dev->hwif)) {
	    	/* disable IDE interrupts */
		    outl(0, dev->base + REG_IDECTL);
		    /* power off the card */
			//outl(0, dev->base + REG_CFCTL);

			ide_unregister(dev->hwif->index);
			dev->configured = 0;
			dev->hwif = NULL;
			PDEBUG("CF released\n");
			return 0;
		}
	}
	return -1;
}

static int cf_config(struct cf_dev* dev)
{
    hw_regs_t hw;
    int index;
    ide_hwif_t *hwif;

    if (!dev)
    	return -1;

    if (!dev->configured) {
    	int i;
    	for (i=1; i<=10; i++) {
		    cf_init_hwif_ports(&hw, dev->ide_base, 0, NULL);
		    hw.irq = dev->ide_irq;
		    hw.chipset = ide_generic;
		    outl(IDECTL_IIDE, dev->base + REG_IDECTL);
		    index = ide_register_hw(&hw, 1, &hwif);
		    if (index >=0) {
			    dev->configured = 1;
			    dev->hwif = hwif;
			    return index;
			}

			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(HZ/10);
		}
		/* register fails */
		PDEBUG("CF:fail to register\n");
		/* disable IDE interrupt */
		outl(0, dev->base + REG_IDECTL);
		return -1;
	}
	return -2; /* already configured */
}

static irqreturn_t cf_intr(int irq, void *dev_id)
{
	unsigned int cfctl;
	struct cf_dev* dev = (struct cf_dev *)dev_id;

	if (!dev)
		return IRQ_NONE;

	cfctl=inl(dev->base + REG_CFCTL);
	/* unpower the card */
	outl((cfctl & ~(CFCTL_PWR)), dev->base + REG_CFCTL);

	if ((cfctl & CFCTL_DET))
		schedule_delayed_work(&dev->wcf, HZ/2);
	else
		schedule_work(&dev->wcf.work);
	return IRQ_HANDLED;
}

static void cf_event(struct work_struct *work)
{
	struct cf_dev* dev = container_of(work, struct cf_dev, wcf.work);

	if (dev) {
		unsigned int cfctl;

		cfctl=inl(dev->base + REG_CFCTL);
		if ((cfctl & CFCTL_DET)) {
			/* a CF card is inserted, power on the card */
			outl(((cfctl | CFCTL_PWR) & ~(CFCTL_RST) ), dev->base + REG_CFCTL);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(HZ);
			cf_config(dev);
		}
		else {
			/* a CF card is removed */
			cf_release(dev);
		}
	}
}

int __init altcf_init(void)
{
	unsigned int cfctl;
	int i;
    ide_hwif_t *hwif;
	hw_regs_t hw;
	extern ide_hwif_t ide_hwifs[];

	for (i=0; i<MAX_HWIFS; i++) {
		cfctl=inl(cf_devices[i].base + REG_CFCTL);
		PDEBUG("CF: ctl=%d\n", cfctl);
		if (cfctl & CFCTL_DET)
		{
			/* power off the card */
			outl(CFCTL_RST, cf_devices[i].base + REG_CFCTL);
			mdelay(500);
			cfctl=inl(cf_devices[i].base + REG_CFCTL);

			/* power on the card */
			outl(((cfctl | CFCTL_PWR) & ~(CFCTL_RST) ), cf_devices[i].base + REG_CFCTL);
			mdelay(2000);
			inl(cf_devices[i].base + REG_CFCTL);

			/* check if card is in right mode */
			outb(0xa0, cf_devices[i].ide_base+IDE_SELECT_OFFSET*4);
			mdelay(50);
			if (inb(cf_devices[i].ide_base+IDE_SELECT_OFFSET*4) == 0xa0) {
				/* enable IDE interrupt */
				outl(IDECTL_IIDE, cf_devices[i].base + REG_IDECTL);
				ide_hwifs[i].chipset  = ide_generic;
				cf_devices[i].hwif = &ide_hwifs[i];

				memset(&hw, 0, sizeof hw);
				cf_init_hwif_ports(&hw, cf_devices[i].ide_base, 0, NULL);
				hw.chipset = ide_generic;
				hw.irq = cf_devices[i].ide_irq;
				if (ide_register_hw(&hw, 1, &hwif)>=0) {
					cf_devices[i].configured = 1;
					cf_devices[i].hwif = hwif;
				}
				else
					printk("CF register fails\n");
			}
			else printk("Unable to initialize compact flash card. Please re-insert\n");
		}

		/* register the detection interrupt */
		if (request_irq(cf_devices[i].irq, cf_intr, IRQF_DISABLED, "cf", &cf_devices[i])) {
			PDEBUG("CF: unable to get interrupt %d for detecting inf %d\n",
					cf_devices[i].irq, i );
		} else {
			INIT_DELAYED_WORK(&cf_devices[i].wcf, cf_event);
			/* enable the detection interrupt */
			cfctl=inl(cf_devices[i].base + REG_CFCTL);
			outl(cfctl | CFCTL_IDET, cf_devices[i].base + REG_CFCTL);
		}
	}

	return 0;
}

#ifdef MODULE
static void __exit altcf_exit(void)
{
	unsigned int cfctl;
	for (i=0; i<MAX_HWIFS; i++) {
		/* disable detection irq */
		cfctl=inl(cf_devices[i].base + REG_CFCTL);
		outl(cfctl & ~CFCTL_IDET, cf_devices[i].base + REG_CFCTL);

		/* free the detection irq */
		free_irq(cf_devices[i].irq, &cf_devices[i]);

		/* release the device */
		cf_release(&cf_devices[i]);
	}
}

module_init(altcf_init);
module_exit(altcf_exit);
#endif
