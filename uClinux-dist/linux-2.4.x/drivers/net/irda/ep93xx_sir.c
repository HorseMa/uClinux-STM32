/*
 * SIR IR port driver for the Cirrus Logic EP93xx.
 *
 * SIR on the EP93xx hangs off of UART2 so this is emplemented
 * as a dongle driver.
 *
 * Copyright 2003 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irda_device.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/hardware/serial_amba.h>


#undef DEBUG
//#define DEBUG 1
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


/*
 * We need access to the UART2 control register so we can
 * do a read-modify-write and enable SIR.
 */

static void ep93xx_sir_open(dongle_t *self, struct qos_info *qos);
static void ep93xx_sir_close(dongle_t *self);
static int  ep93xx_sir_change_speed(struct irda_task *task);
static int  ep93xx_sir_reset(struct irda_task *task);

static struct dongle_reg dongle = {
    Q_NULL,
    IRDA_EP93XX_SIR,
    ep93xx_sir_open,
    ep93xx_sir_close,
    ep93xx_sir_reset,
    ep93xx_sir_change_speed
};


static void ep93xx_sir_open(dongle_t *self, struct qos_info *qos)
{
    unsigned int uiTemp;

    DPRINTK("----------------\n ep93xx_sir_open \n-----------------\n");
    
    /* 
     * Set UART2 to be an IrDA interface
     */
    uiTemp = inl(SYSCON_DEVCFG);
    SysconSetLocked(SYSCON_DEVCFG, (uiTemp | SYSCON_DEVCFG_IonU2) );
    
    /*
     * Set the pulse width.
     */
    //uiTemp = inl(UART2ILPR);
	outl( 3, UART2ILPR );

    /*
     * Set SIREN bit in UART2 - this enables the SIR encoder/decoder.
     */
    uiTemp = inl(UART2CR);
    outl( (uiTemp | AMBA_UARTCR_SIREN), UART2CR );

    /*
     * Enable Ir in SIR mode.
     */
    /* Write the reg twice because of the IrDA errata.  */
	outl( IrEnable_EN_SIR, IrEnable );
	outl( IrEnable_EN_SIR, IrEnable );
    

    MOD_INC_USE_COUNT;
}

static void ep93xx_sir_close(dongle_t *self)
{
    unsigned int uiTemp;

    DPRINTK("----------------\n ep93xx_sir_close \n-----------------\n");

    /*
     * Disable Ir.
     */
     /* for now, don't write irda regs due to errata. */
	//outl( IrEnable_EN_NONE, IrEnable );

    /* 
     * Set UART2 to be an UART
     */
    uiTemp = inl( SYSCON_DEVCFG );
    SysconSetLocked(SYSCON_DEVCFG, (uiTemp & ~(SYSCON_DEVCFG_IonU2)) );

    MOD_DEC_USE_COUNT;
}

/*
 * Function ep93xx_sir_change_speed (task)
 *
 *   Change speed of the EP93xx I/R port. We don't have to do anything
 *   here as long as the rate is being changed at the serial port
 *   level.  irtty.c should take care of that.
 */
static int ep93xx_sir_change_speed(struct irda_task *task)
{
    DPRINTK("----------------\n ep93xx_sir_change_speed \n-----------------\n");
    irda_task_next_state(task, IRDA_TASK_DONE);
    return 0;
}

/*
 * Function ep93xx_sir_reset (task)
 *
 *      Reset the EP93xx IrDA. We don't really have to do anything.
 *
 */
static int ep93xx_sir_reset(struct irda_task *task)
{
    DPRINTK("----------------\n ep93xx_sir_reset \n-----------------\n");
    irda_task_next_state(task, IRDA_TASK_DONE);
    return 0;
}

/*
 * Function ep93xx_sir_init(void)
 *
 *    Initialize EP93xx IrDA block in SIR mode.
 *
 */
int __init ep93xx_sir_init(void)
{
    DPRINTK("----------------\n ep93xx_sir_init \n-----------------\n");
	return irda_device_register_dongle(&dongle);
}

/*
 * Function ep93xx_sir_cleanup(void)
 *
 *    Cleanup EP93xx IrDA module
 *
 */
static void __exit ep93xx_sir_cleanup(void)
{
    DPRINTK("----------------\n ep93xx_sir_cleanup \n-----------------\n");
    irda_device_unregister_dongle(&dongle);
}

MODULE_DESCRIPTION("EP93xx SIR IrDA driver");
MODULE_LICENSE("GPL");
        
#ifdef MODULE
module_init(ep93xx_sir_init);
module_exit(ep93xx_sir_cleanup);
#endif
