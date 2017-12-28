#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/serial.h>
#include <asm/hardware.h>

#include <asm/arch/irqs.h>
#include <asm/arch/gpio.h>

#define SER_IRQ_MASK		(0xF8200000)
#define SER_LED_STATE		(0xF8800000)
#define SER_XCVR_CTRL		(0xF8C00000)

#define cfg_base (0xfC000000)
#define cfg_irq (IRQ_EXT3)

static int initialised = 0;

static __init int quad_init_one(unsigned long base, unsigned int irq)
{
        struct serial_struct serial_req;

        /*printk("registering serial (%08x,%d)\n", base, irq);*/

        serial_req.flags = ASYNC_AUTOPROBE;
        serial_req.baud_base = oki_hclk / 128; /* /8 for CPLD and then /16 */
        serial_req.irq = irq;
        serial_req.io_type = SERIAL_IO_MEM;
        serial_req.iomem_base = ioremap(base, 0x10);
        serial_req.iomem_reg_shift = 0;
	serial_req.port = 0;

        return register_serial(&serial_req);
}

int __init serial_oki_ml67x_quad_init(void)
{
	int port;
	int ret;

	if (initialised)
	    return 0;

	initialised  = 1;

	writeb(0x0f, SER_IRQ_MASK);
	writeb(0x0f, SER_LED_STATE);

	gpctl_modify(OKI_GPCTL_EXTI3, 0);

	for (port = 0; port < 4; port++) {
		ret = quad_init_one(cfg_base + (port << 21) , cfg_irq);
		if (ret < 0)
			printk(KERN_INFO "%s: port %d return %d\n",
			       __FUNCTION__, port, ret);
	}

	return 0;
}


module_init(serial_oki_ml67x_quad_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ben Dooks, ben@simtec.co.uk");
MODULE_DESCRIPTION("OKI ML67X Onboard Serial setup");
