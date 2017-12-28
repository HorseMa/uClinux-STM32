/*
 * linux/drivers/ide/pci/p20575.c
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/ioport.h>
#include <asm/io.h>

#if 0
#define	PRINTK(x...)	printk(x)
#else
#define	PRINTK(x...)	do { } while (0)
#endif

#if 0
static void hexdump(void *v, unsigned int len)
{
	unsigned int p = (unsigned int) v;
	int i;

	for (i = 0; (i < (len/4)); i++) {
		if ((i % 4) == 0) printk("%08x:  ", (int)p);
		printk("%08x ", readl(p));
		p += 4;
		if (((i+1) % 4) == 0) printk("\n");
	}
	if ((i % 4) != 0) printk("\n");
}
#endif

void *p20575_iomap;

static u8 p20575_inb(unsigned long port)
{
	u8 v;
	PRINTK("p20575_inb(port=%x)", (int)port);
	v = readl(p20575_iomap+port);
	PRINTK("=%x\n", (int)v);
	return v;
}

static u16 p20575_inw(unsigned long port)
{
	u16 v;
	PRINTK("p20575_inw(port=%x)", (int)port);
	v = readl(p20575_iomap+port);
	PRINTK("=%x\n", (int)v);
	return v;
}

#if 0
static u32 p20575_inl(unsigned long port)
{
	u32 v;
	PRINTK("p20575_inl(port=%x)", (int)port);
	v = readl(p20575_iomap+port);
	PRINTK("=%x\n", (int)v);
	return v;
}
#endif

static void p20575_outb(u8 val, unsigned long port)
{
	PRINTK("p20575_outb(val=%x,port=%x)\n", (int)val, (int)port);
	writel(val, p20575_iomap+port);
}

static void p20575_outbsync(ide_drive_t *drive, u8 val, unsigned long port)
{
	PRINTK("p20575_outb(val=%x,port=%x)\n", (int)val, (int)port);
	writel(val, p20575_iomap+port);
}

static void p20575_outw(u16 val, unsigned long port)
{
	PRINTK("p20575_outw(val=%x,port=%x)\n", (int)val, (int)port);
	writel(val, p20575_iomap+port);
}

#if 0
static void p20575_outl(u32 val, unsigned long port)
{
	PRINTK("p20575_outl(val=%x,port=%x)\n", (int)val, (int)port);
	writel(val, p20575_iomap+port);
}
#endif

static void p20575_outsw(unsigned long port, void *buf, u32 len)
{
	u16 w, *wp = buf;
	PRINTK("p20575_outsw(port=%x,buf=%x,len=%x)\n", (int)port, (int)buf, len);
	while (len--) {
		w = *wp++;
		w = (w << 8) | (w >> 8);
		writel(w, p20575_iomap+port);
	}
}

static void p20575_insw(unsigned long port, void *buf, u32 len)
{
	u16 w, *wp = buf;
	PRINTK("p20575_insw(port=%x,buf=%x,len=%x)\n", (int)port, (int)buf, len);
	while (len--) {
		w = readl(p20575_iomap+port);
		*wp++ = (w << 8) | (w >> 8);
	}
}

static int p20575_ack_intr(ide_hwif_t *hwif)
{
	unsigned int v;

	v = readl(p20575_iomap+0x40);
	writel(v, p20575_iomap+0x40);
	writel(1, p20575_iomap+(2*4));
	return 1;
}

static void __devinit p20575_init_iops(ide_hwif_t *hwif)
{
	hw_regs_t hw;

	PRINTK("%s(%d): p20575_init_iops()\n", __FILE__, __LINE__);

	memset(&hw, 0, sizeof(hw));
	hw.io_ports[IDE_DATA_OFFSET] = 0x300;
	hw.io_ports[IDE_ERROR_OFFSET] = 0x304;
	hw.io_ports[IDE_NSECTOR_OFFSET] = 0x308;
	hw.io_ports[IDE_SECTOR_OFFSET] = 0x30c;
	hw.io_ports[IDE_LCYL_OFFSET] = 0x310;
	hw.io_ports[IDE_HCYL_OFFSET] = 0x314;
	hw.io_ports[IDE_SELECT_OFFSET] = 0x318;
	hw.io_ports[IDE_STATUS_OFFSET] = 0x31C;
	hw.io_ports[IDE_CONTROL_OFFSET] = 0x338;

	hwif->INB = p20575_inb;
	hwif->INW = p20575_inw;
	/*hwif->INL = p20575_inl;*/
	hwif->OUTB = p20575_outb;
	hwif->OUTBSYNC = p20575_outbsync;
	hwif->OUTW = p20575_outw;
	/*hwif->OUTL = p20575_outl;*/
	hwif->OUTSW = p20575_outsw;
	hwif->INSW = p20575_insw;
	/*hwif->irq = hwif->pci_dev->irq;*/
	hwif->ack_intr = p20575_ack_intr;

	hwif->mmio = 1;

	memcpy(hwif->io_ports, hw.io_ports, sizeof(hw.io_ports));
}

static unsigned int __devinit p20575_init_chipset(struct pci_dev *dev, const char *name)
{
	PRINTK("%s(%d): p20575_init_chipset(name=%s) -> irq=%d\n", __FILE__, __LINE__, name, dev->irq);

	p20575_iomap = ioremap(0x48060000, 0x1000);
	PRINTK("%s(%d): iomap=%x\n", __FILE__, __LINE__, (int)p20575_iomap);

	writel(2, p20575_iomap+0x360);
	writel(1, p20575_iomap+(2*4));

	return dev->irq;
}

static void __devinit p20575_init_hwif(ide_hwif_t *hwif)
{
	PRINTK("%s(%d): p20575_init_hwif()\n", __FILE__, __LINE__);
}

static const struct ide_port_info p20575_chipset __devinitdata = {
	.name		= "P20575",
	.init_chipset	= p20575_init_chipset,
	.init_iops	= p20575_init_iops,
	.init_hwif	= p20575_init_hwif,
	.host_flags	= IDE_HFLAG_NO_DMA | IDE_HFLAG_BOOTABLE,
};

static int __devinit p20575_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	PRINTK("%s(%d): p20575_init_one()\n", __FILE__, __LINE__);
	return ide_setup_pci_device(dev, &p20575_chipset);
}

static struct pci_device_id p20575_pci_tbl[] = {
	{ PCI_VENDOR_ID_PROMISE, 0x3575, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ 0, },
};

MODULE_DEVICE_TABLE(pci, p20575_pci_tbl);

static struct pci_driver driver = {
	.name		= "P20575-IDE",
	.id_table	= p20575_pci_tbl,
	.probe		= p20575_init_one,
};

static int p20575_ide_init(void)
{
	PRINTK("%s(%d): p20575_ide_init()\n", __FILE__, __LINE__);
	return ide_pci_register_driver(&driver);
}

module_init(p20575_ide_init);

MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.com>");
MODULE_DESCRIPTION("PCI driver module for PATA channel of Promise 20575");
MODULE_LICENSE("GPL");
