/*
*  linux/arch/armnommu/mach-W90N740/irq.c
*  2003 clyu <clyu2@winbond.com.tw>
*/
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

void w90n740_mask_irq(unsigned int irq)
{
        INT_DISABLE(irq);
}

void w90n740_unmask_irq(unsigned int irq)
{
        INT_ENABLE(irq);
}

void w90n740_mask_ack_irq(unsigned int irq)
{
        INT_DISABLE(irq);
}

void w90n740_int_init()
{
        //int i=0;
        //IntPend = 0x1FFFFF;
        CSR_WRITE(AIC_MDCR, 0x7FFFE);
        CSR_WRITE(0xFFF82018,0x41);
        CSR_WRITE(0xFFF8201C,0x41);
        //for(i=6;i<=18;i++)
        //	IntScr(i,0x41);
        //IntMode = INT_MODE_IRQ;
        //INT_ENABLE(INT_GLOBAL);
}
