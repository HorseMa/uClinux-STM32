/*
 *  linux/arch/arm/mach-W90N740/arch.c
 *
 *  Architecture specific fixups.  This is where any
 *  parameters in the params struct are fixed up, or
 *  any additional architecture specific information
 *  is pulled from the params struct.
 */
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/bootheader.h>

extern void genarch_init_irq(void);


static void __init w90n740_cache_init(void)
{
        printk("Enable W90N740 cache\n");

        CSR_WRITE(AIC_MDCR, 0x7FFFE); /* disable all interrupts */

        // disable cache
        CSR_WRITE(CAHCNF,0x0);

        // drain write buffer (DRWB, DCAH)
        CSR_WRITE(CAHCON,0x82);
        while(CSR_READ(CAHCON)!=0)
                ;

        // Flush I+D cache (FLHA,DCAH,ICAH)
        CSR_WRITE(CAHCON,0x7);
        while(CSR_READ(CAHCON)!=0)
                ;

        // enable WR buffer, I-cache, D-cache
        CSR_WRITE(CAHCNF,0x7);
        //XXX CSR_WRITE(CAHCNF,0x1);	// ICACHE ONLY !!!
}

MACHINE_START(W90N740, "W90N740")
MAINTAINER("Shirley yu")
INITIRQ(genarch_init_irq)
FIXUP(w90n740_cache_init)
MACHINE_END
