/****************************************************************************/

/*
 *	ess710ide.c -- IDE driver for the CF slot on the ESS710.
 *
 *	(C) Copyright 2004, Greg Ungerer <gerg@snapgear.com>
 */

/****************************************************************************/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/ide.h>
#include <linux/interrupt.h>
#include <asm/io.h>

/****************************************************************************/

/*
 *	Regiser offsets for the ESS710 CF/IDE port. It is mapped onto
 *	CS3 of the IXP425. The IDE CS0 and CS1 liens are driven based
 *	on A$ from the CS3/IXP425 address range.
 */
#define	ESS710IDE_IRQ	27
#define	ESS710IDE_INT	IXP425_GPIO_PIN_10

#define	ESS710IDE_BASE	0x53000000
#define	ESS710IDE_CTRL	0x1e

static unsigned char *ess710ide_basep;

/****************************************************************************/

/*
 *	Define the CS3 mapping value. No real header file define support
 *	for the appropriate fields or flags, so this is just the number,
 */
#define	ESS710IDE_CS3	0xbfff3c03
#define	IXP425_CS_BEN	0x00000001

/****************************************************************************/

static void ess710ide_enable16(void)
{
	*IXP425_EXP_CS3 &= ~IXP425_CS_BEN;
}

/****************************************************************************/

static void ess710ide_disable16(void)
{
	*IXP425_EXP_CS3 |= IXP425_CS_BEN;
}

/****************************************************************************/

static u8 ess710ide_inb(unsigned long addr)
{
	return readb(addr);
}

/****************************************************************************/

static u16 ess710ide_inw(unsigned long addr)
{
	u16 val;

	ess710ide_enable16();
	val = swab16(readw(addr));
	ess710ide_disable16();

	return val;
}

/****************************************************************************/

static void ess710ide_insw(unsigned long addr, void *buf, u32 len)
{
	u16 *buf16p;

	ess710ide_enable16();
	for (buf16p = (u16 *) buf; (len > 0); len--)
		*buf16p++ = swab16(readw(addr));
	ess710ide_disable16();
}

/****************************************************************************/

static void ess710ide_outb(u8 val, unsigned long addr)
{
	writeb(val, addr);
}

/****************************************************************************/

static void ess710ide_outbsync(ide_drive_t *drive, u8 val, unsigned long addr)
{
	writeb(val, addr);
}
/****************************************************************************/

static void ess710ide_outw(u16 val, unsigned long addr)
{
	ess710ide_enable16();
	writew(swab16(val), addr);
	ess710ide_disable16();
}

/****************************************************************************/

static void ess710ide_outsw(unsigned long addr, void *buf, u32 len)
{
	u16 *buf16p;

	ess710ide_enable16();
	for (buf16p = (u16 *) buf; (len > 0); len--)
		writew(swab16(*buf16p++), addr);
	ess710ide_disable16();
}

/****************************************************************************/

static int ess710ide_iack(ide_hwif_t *hwif)
{
	gpio_line_isr_clear(ESS710IDE_INT);
	return 1;
}

/****************************************************************************/

static int __init ess710_iomap(void)
{
	/* Enable GPIO line 10 as interrupt source */
	gpio_line_config(ESS710IDE_INT, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	//gpio_line_config(ESS710IDE_INT, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_HIGH);
	gpio_line_isr_clear(ESS710IDE_INT);

	/* Program chip selects to decode for IDE region */
	*IXP425_EXP_CS3 |= 0xbfff3c03;

	ess710ide_basep = ioremap(ESS710IDE_BASE, 0x1000);
	if (ess710ide_basep == NULL)
		return -ENOMEM;

	return 0;
}

/****************************************************************************/

void __init ess710ide_init(void)
{
	hw_regs_t hw;
	ide_hwif_t *hwif;
	int i;

	if (ess710_iomap() < 0)
		return;

	memset(&hw, 0, sizeof(hw));
	hw.irq = ESS710IDE_IRQ;
	hw.dma = NO_DMA;
	hw.ack_intr = ess710ide_iack;
	for (i = 0; (i <= IDE_STATUS_OFFSET); i++)
		hw.io_ports[i] = ess710ide_basep + i;
	hw.io_ports[IDE_CONTROL_OFFSET] = ess710ide_basep + ESS710IDE_CTRL;

	if (ide_register_hw(&hw, &hwif) != -1) {
		hwif->mmio = 1;
		hwif->OUTB = ess710ide_outb;
		hwif->OUTBSYNC = ess710ide_outbsync;
		hwif->OUTW = ess710ide_outw;
		hwif->OUTSW = ess710ide_outsw;
		hwif->INB = ess710ide_inb;
		hwif->INW = ess710ide_inw;
		hwif->INSW = ess710ide_insw;
	}
}

/****************************************************************************/
