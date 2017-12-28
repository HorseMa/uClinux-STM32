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

#define cfg_port (0xb7b00000)
#define cfg_irq (9)

static int initialised = 0;

int __init serial_oki_ml67x_init(void)
{
        struct serial_struct serial_req;

	if (initialised)
	    return 0;

	initialised  = 1;

        printk("registering serial (%08x,%d)\n", cfg_port, cfg_irq);

        serial_req.flags = ASYNC_AUTOPROBE;
        serial_req.baud_base = oki_cclk / 16;
        serial_req.irq = cfg_irq;
        serial_req.io_type = SERIAL_IO_MEM;
        serial_req.iomem_base = ioremap(cfg_port, 0x10);
        serial_req.iomem_reg_shift = 2;
	serial_req.port = 0;

	printk("serial_req.baud_base  = %d\n",   serial_req.baud_base);
	printk("serial_req.iomem_base = %08x\n", serial_req.iomem_base);

	printk("calling register_serial()\n");

        return register_serial(&serial_req);
}


module_init(serial_oki_ml67x_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ben Dooks, ben@simtec.co.uk");
MODULE_DESCRIPTION("OKI ML67X Onboard Serial setup");
