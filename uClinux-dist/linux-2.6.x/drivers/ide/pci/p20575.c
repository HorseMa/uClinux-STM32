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

static void p20575_output_data(ide_drive_t *drive, struct request *rq, void *buf, unsigned int len)
{
	p20575_outsw(drive->hwif->io_ports.data_addr, buf, len/2);
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

static void p20575_input_data(ide_drive_t *drive, struct request *rq, void *buf, unsigned int len)
{
	p20575_insw(drive->hwif->io_ports.data_addr, buf, len/2);
}

static void p20575_tf_load(ide_drive_t *drive, ide_task_t *task)
{
	ide_hwif_t *hwif = drive->hwif;
	struct ide_io_ports *io_ports = &hwif->io_ports;
	struct ide_taskfile *tf = &task->tf;

	u8 HIHI = (task->tf_flags & IDE_TFLAG_LBA48) ? 0xE0 : 0xEF;

	if (task->tf_flags & IDE_TFLAG_FLAGGED)
		HIHI = 0xFF;

	ide_set_irq(drive, 1);

	if (task->tf_flags & IDE_TFLAG_OUT_DATA)
		p20575_outw((tf->hob_data << 8) | tf->data, io_ports->data_addr);

	if (task->tf_flags & IDE_TFLAG_OUT_HOB_FEATURE)
		p20575_outb(tf->hob_feature, io_ports->feature_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_NSECT)
		p20575_outb(tf->hob_nsect, io_ports->nsect_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_LBAL)
		p20575_outb(tf->hob_lbal, io_ports->lbal_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_LBAM)
		p20575_outb(tf->hob_lbam, io_ports->lbam_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_LBAH)
		p20575_outb(tf->hob_lbah, io_ports->lbah_addr);

	if (task->tf_flags & IDE_TFLAG_OUT_FEATURE)
		p20575_outb(tf->feature, io_ports->feature_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_NSECT)
		p20575_outb(tf->nsect, io_ports->nsect_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_LBAL)
		p20575_outb(tf->lbal, io_ports->lbal_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_LBAM)
		p20575_outb(tf->lbam, io_ports->lbam_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_LBAH)
		p20575_outb(tf->lbah, io_ports->lbah_addr);

	if (task->tf_flags & IDE_TFLAG_OUT_DEVICE)
		p20575_outb((tf->device & HIHI) | drive->select.all, io_ports->device_addr);
}

static void p20575_tf_read(ide_drive_t *drive, ide_task_t *task)
{
	struct ide_io_ports *io_ports = &drive->hwif->io_ports;
	struct ide_taskfile *tf = &task->tf;

	if (task->tf_flags & IDE_TFLAG_IN_DATA) {
		u16 data = p20575_inw(io_ports->data_addr);
		tf->data = data & 0xff;
		tf->hob_data = (data >> 8) & 0xff;
	}

	/* be sure we're looking at the low order bits */
	p20575_outb(drive->ctl & ~0x80, io_ports->ctl_addr);

	if (task->tf_flags & IDE_TFLAG_IN_NSECT)
		tf->nsect  = p20575_inb(io_ports->nsect_addr);
	if (task->tf_flags & IDE_TFLAG_IN_LBAL)
		tf->lbal   = p20575_inb(io_ports->lbal_addr);
	if (task->tf_flags & IDE_TFLAG_IN_LBAM)
		tf->lbam   = p20575_inb(io_ports->lbam_addr);
	if (task->tf_flags & IDE_TFLAG_IN_LBAH)
		tf->lbah   = p20575_inb(io_ports->lbah_addr);
	if (task->tf_flags & IDE_TFLAG_IN_DEVICE)
		tf->device = p20575_inb(io_ports->device_addr);

	if (task->tf_flags & IDE_TFLAG_LBA48) {
		p20575_outb(drive->ctl | 0x80, io_ports->ctl_addr);

		if (task->tf_flags & IDE_TFLAG_IN_HOB_FEATURE)
			tf->hob_feature = p20575_inb(io_ports->feature_addr);
		if (task->tf_flags & IDE_TFLAG_IN_HOB_NSECT)
			tf->hob_nsect   = p20575_inb(io_ports->nsect_addr);
		if (task->tf_flags & IDE_TFLAG_IN_HOB_LBAL)
			tf->hob_lbal    = p20575_inb(io_ports->lbal_addr);
		if (task->tf_flags & IDE_TFLAG_IN_HOB_LBAM)
			tf->hob_lbam    = p20575_inb(io_ports->lbam_addr);
		if (task->tf_flags & IDE_TFLAG_IN_HOB_LBAH)
			tf->hob_lbah    = p20575_inb(io_ports->lbah_addr);
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
	hw.io_ports.data_addr = 0x300;
	hw.io_ports.error_addr = 0x304;
	hw.io_ports.nsect_addr = 0x308;
	hw.io_ports.lbal_addr = 0x30c;
	hw.io_ports.lbam_addr = 0x310;
	hw.io_ports.lbah_addr = 0x314;
	hw.io_ports.device_addr = 0x318;
	hw.io_ports.status_addr = 0x31C;
	hw.io_ports.ctl_addr = 0x338;

	hwif->INB = p20575_inb;
	hwif->OUTB = p20575_outb;
	hwif->OUTBSYNC = p20575_outbsync;
	hwif->input_data = p20575_input_data;
	hwif->output_data = p20575_output_data;
	hwif->tf_load = p20575_tf_load;
	hwif->tf_read = p20575_tf_read;
	hwif->config_data = hw.io_ports.ctl_addr;
	hwif->ack_intr = p20575_ack_intr;

	hwif->mmio = 0;

	memcpy(&hwif->io_ports, &hw.io_ports, sizeof(hw.io_ports));
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
	.host_flags	= IDE_HFLAG_NO_DMA | IDE_HFLAG_ISA_PORTS | IDE_HFLAG_SINGLE,
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
