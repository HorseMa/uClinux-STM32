
/*
 *  linux/drivers/usb/usb-ohci-w90n740.c
 *
 *  The outline of this code was taken from Brad Parkers <brad@heeltoe.com>
 *  original OHCI driver modifications, and reworked into a cleaner form
 *  by Russell King <rmk@arm.linux.org.uk>.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include <linux/pci.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/pci.h>

#include "usb-ohci.h"

int __devinit
hc_found_ohci (struct pci_dev *dev, int irq,
               void *mem_base, const struct pci_device_id *id);

extern void hc_remove_ohci(ohci_t *ohci);

static ohci_t *w90n740_ohci;

static void __init w90n740_ohci_configure(void)
{
        /*
         * Configure the power sense and control lines.  Place the USB
         * host controller in reset.
         */

        /*
         * Now, carefully enable the USB clock, and take
         * the USB host controller out of reset.
         */
}

static int __init w90n740_ohci_init(void)
{
        void *mem_base;
        unsigned long mem_len,mem_resource;
        static struct pci_dev dev = {
                                            .name = "w90n740",
                                            };
        static struct pci_device_id id = {
                                                 .driver_data = OHCI_QUIRK_AMD756,
                                                        };
        int ret;

        mem_resource = 0xFFF05000;
        mem_len = 0x5C;

        mem_base = ioremap_nocache (mem_resource, mem_len);
        if (!mem_base) {
                err("Error mapping OHCI memory");
                release_mem_region (mem_resource, mem_len);
                return -EFAULT;
        }

        w90n740_ohci_configure();

        /*
         * Initialise the generic OHCI driver.
         */
        ret = hc_found_ohci(&dev, INT_USBINT0, (void *)mem_base, &id);
        if (ret < 0) {
                iounmap(mem_base);
                release_mem_region (mem_resource, mem_len);
        }

        return ret;
}

static void __exit w90n740_ohci_exit(void)
{
        hc_remove_ohci(w90n740_ohci);

        /*
         * Put the USB host controller into reset.
         */

        /*
         * Stop the USB clock.
         */
}

module_init(w90n740_ohci_init);
module_exit(w90n740_ohci_exit);
