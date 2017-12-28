/****************************************************************************/
/*
 *  linux/include/asm-m68knommu/ide.h
 *
 *  Copyright (C) 2001-2003  David McCullough <davidm@snapgear.com>
 */

#ifndef _ASM_IDE_H
#define _ASM_IDE_H
#ifdef __KERNEL__

#include <linux/config.h>

#include <asm/setup.h>
#include <asm/io.h>
#include <asm/irq.h>

/****************************************************************************/

#undef SUPPORT_SLOW_DATA_PORTS
#define SUPPORT_SLOW_DATA_PORTS 0

#undef SUPPORT_VLB_SYNC
#define SUPPORT_VLB_SYNC 0

#ifndef MAX_HWIFS
#define MAX_HWIFS 8
#endif

#define IDE_ARCH_ACK_INTR 1
#define ide_ack_intr(hwif) ((hwif)->hw.ack_intr?(hwif)->hw.ack_intr(hwif):1)

/****************************************************************************/
/*
 * these guys are not used on uClinux,  the uclinux.c driver does it all for us
 */
static inline int ide_default_irq(ide_ioreg_t base)
{
	return 0;
}

static inline ide_ioreg_t ide_default_io_base(int index)
{
	/* we don't have an ISA bus */
	return 0;
}

static inline void ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port,
				       ide_ioreg_t ctrl_port, int *irq)
{
#if 1 //ndef CONFIG_UCLINUX
	ide_ioreg_t reg = data_port;
	int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += 1;
	}
	if (ctrl_port) {
		hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
	} else {
		hw->io_ports[IDE_CONTROL_OFFSET] = hw->io_ports[IDE_DATA_OFFSET] + 0x206;
	}
	if (irq != NULL)
		*irq = 0;
	hw->io_ports[IDE_IRQ_OFFSET] = 0;
#else
	return 0;
#endif
}

static inline void ide_init_default_hwifs(void)
{
#if 1 //ndef CONFIG_BLK_DEV_IDEPCI
	hw_regs_t hw;
	int index;

	for (index = 0; index < MAX_HWIFS; index++) {
		ide_init_hwif_ports(&hw, ide_default_io_base(index), 0, NULL);
		hw.irq = ide_default_irq(ide_default_io_base(index));
		ide_register_hw(&hw, NULL);
	}
#endif /* CONFIG_BLK_DEV_IDEPCI */
}

/****************************************************************************/
/*
 * some bits needed for parts of the IDE subsystem to compile
 */
#define __ide_mm_insw(port, addr, n)	insw((u16 *)port, addr, n)
#define __ide_mm_insl(port, addr, n)	insl((u32 *)port, addr, n)
#define __ide_mm_outsw(port, addr, n)	outsw((u16 *)port, addr, n)
#define __ide_mm_outsl(port, addr, n)	outsl((u32 *)port, addr, n)


#endif /* __KERNEL__ */
#endif /* _ASM_IDE_H */
