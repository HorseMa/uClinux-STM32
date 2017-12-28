
/*
 *  linux/drivers/usb/usb-ohci-ep93xx.c
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

static ohci_t *ep93xx_ohci;

static void __init ep93xx_ohci_configure(void)
{
	unsigned int usb_rst = 0;
	unsigned int uiPBDR, uiPBDDR;
	 __u32                   temp;
     
	/*
	 * Configure the power sense and control lines.  Place the USB
	 * host controller in reset.
	 */
#if defined(CONFIG_ARCH_EDB9312) | defined(CONFIG_ARCH_EDB9315)
	/*
	 * For EDB9315 boards, turn on USB by clearing EGPIO9.
	 */
	uiPBDR = inl(GPIO_PBDR) & 0xfd;
	outl( uiPBDR, GPIO_PBDR );

	uiPBDDR = inl(GPIO_PBDDR) | 0x02;
	outl( uiPBDDR, GPIO_PBDDR );
#endif

	/*
	 * Now, carefully enable the USB clock, and take
	 * the USB host controller out of reset.
	 */
	writel (readl ((void *)SYSCON_PWRCNT) | (1<< 28),(void *)SYSCON_PWRCNT);

}

static int __init ep93xx_ohci_init(void)
{
	static struct pci_dev dev = {
		.name = "ep93xx",
	};
	static struct pci_device_id id = {
		.driver_data = 0,
	};
	int ret;

	/*
	 * Only support USB on rev D1 and later chips.
         */
	if((inl(SYSCON_CHIPID) >> 28) <= 3)
		return -EBUSY;
	/*
	 * Request memory resources.
	 */
//	if (!request_mem_region(_USB_OHCI_OP_BASE, _USB_EXTENT, "usb-ohci"))
//		return -EBUSY;

	ep93xx_ohci_configure();

	/*
	 * Initialise the generic OHCI driver.
	 */
	ret = hc_found_ohci(&dev, IRQ_USH, (void *)USB_BASE, &id);

	return ret;
}

static void __exit ep93xx_ohci_exit(void)
{
	hc_remove_ohci(ep93xx_ohci);

	/*
	 * Put the USB host controller into reset.
	 */

	/*
	 * Stop the USB clock.
	 */
}

module_init(ep93xx_ohci_init);
module_exit(ep93xx_ohci_exit);
