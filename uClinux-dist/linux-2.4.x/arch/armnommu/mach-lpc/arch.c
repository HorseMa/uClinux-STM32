#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

extern void genarch_init_irq(void);

void fixup_lpc(struct machine_desc *desc, struct param_struct *params, char ** cmdline, struct meminfo * mi)
{
	mi->bank[0].start = 0x81000000;
	mi->bank[0].size = 8 * 1024 * 1024;
	mi->bank[0].node = 0;
	mi->nr_banks = 1;

#ifdef CONFIG_BLK_DEV_INITRD
	setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
	setup_initrd(__phys_to_virt(0x81700000), 1024 * 1024);
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
#endif
}

MACHINE_START(LPC22XX, "zlg-arm-linux")
	MAINTAINER("tsinghua")
	BOOT_MEM(0x81000000, 0xe0000000, 0xe0000000)
	FIXUP(fixup_lpc)
	INITIRQ(genarch_init_irq)
MACHINE_END
