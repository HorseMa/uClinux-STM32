/* vim:set ts=8 sw=8 et: */
/* ----------------------------------------------------------------------------
 * i2c-adap-netarm_gpio.c i2c-hw access using Netsilicon NetARM GPIO
 *
 * ----------------------------------------------------------------------------
 * Arthur E. Shipkowski, Videon Central, Inc.
 * <art@videon-central.com>
 *
 * Copyright (C) 2004 Videon Central, Inc.
 *
 * ----------------------------------------------------------------------------
 * This driver supports using a pair of GPIO pins on the NetSilicon NetARM
 * series of microcontrollers as an I2C adapter.
 *
 * This driver has only undergone basic testing on NS7520 hardware.  Please be
 * aware that issues may be present when attempting to use this driver with
 * untested processors.
 *
 * This driver was based upon Cory Tusar's GPIO I2C driver for ColdFire chips.
 *
 * ----------------------------------------------------------------------------
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

/*
 * NetSilicon register offsets, etc.
 */
#include <asm/arch/netarm_gen_module.h>

/* Legacy includes */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 0))
#include <linux/string.h>               /* Provides NULL for 2.0 kernels */
#include <asm/errno.h>                  /* Provides ENODEV for 2.0 kernels */
#endif  /* LINUX_VERSION_CODE */


/* ----------------------------------------------------------------------------
 * Compile-time dummy checks to keep things relatively sane
 */
#if !defined(CONFIG_ARCH_NETARM)
#error "Can't build NetSilicon I2C-Bus adapter driver for non-NetARM processor"
#endif  /* !defined(CONFIG_ARCH_NETARM) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0))
#warning "This driver has not been tested with kernel versions < 2.4.0"
#endif  /* LINUX_VERSION_CODE */

/* Determine SCL port values */
#if (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA7))
#define I2C_PORT_SCL_STRING     "PA"
#define I2C_PORT_SCL            *(get_gen_reg_addr(NETARM_GEN_PORTA))
#define I2C_SCL_PORTA

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB7))
        
#if defined(CONFIG_NETARM_NS7520)        
#error "Nonexistent PORTB somehow selected for NS7520 processor SCL line!"
#endif  
        
#define I2C_PORT_SCL_STRING     "PB"
#define I2C_PORT_SCL            *(get_gen_reg_addr(NETARM_GEN_PORTB))
#define I2C_SCL_PORTB

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC7))
#define I2C_PORT_SCL_STRING     "PC"
#define I2C_PORT_SCL            *(get_gen_reg_addr(NETARM_GEN_PORTC))
#define I2C_SCL_PORTC

#else
#error "CONFIG_I2C_NETARM_GPIO_SCL_* is not properly configured."
#endif /* CONFIG_I2C_NETARM_GPIO_SCL_*/

#if (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC0))
#define I2C_BIT_SCL             0

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC1))
#define I2C_BIT_SCL             1

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC2))
#define I2C_BIT_SCL             2

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC3))
#define I2C_BIT_SCL             3

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC4))
#define I2C_BIT_SCL             4

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC5))
#define I2C_BIT_SCL             5

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC6))
#define I2C_BIT_SCL             6

#elif (defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTA7) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTB7) || \
        defined(CONFIG_I2C_NETARM_GPIO_SCL_PORTC7))
#define I2C_BIT_SCL             7

#else
#error "CONFIG_I2C_NETARM_GPIO_SCL_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_NETARM_GPIO_SCL_* */

/* Set up SCL port mode for GPIO */
#if (defined(CONFIG_NETARM_NS7520) && defined(I2C_SCL_PORTC))
#define I2C_SCL_PORT_SETUP(x)       (~(NETARM_GEN_PORT_MODE(x) | NETARM_GEN_PORT_CSF(x))) /* Handle NS7520 PORTC properly */
#else
#define I2C_SCL_PORT_SETUP(x)       (~(NETARM_GEN_PORT_MODE(x)))
#endif                        


/* Determine SDA port values */
#if (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA7))
#define I2C_PORT_SDA_STRING     "PA"
#define I2C_PORT_SDA            *(get_gen_reg_addr(NETARM_GEN_PORTA))
#define I2C_SDA_PORTA

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB7))
        
#if defined(CONFIG_NETARM_NS7520)        
#error "Nonexistent PORTB somehow selected for NS7520 processor SDA line!"
#endif  
        
#define I2C_PORT_SDA_STRING     "PB"
#define I2C_PORT_SDA            *(get_gen_reg_addr(NETARM_GEN_PORTB))
#define I2C_SDA_PORTB

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC7))
#define I2C_PORT_SDA_STRING     "PC"
#define I2C_PORT_SDA            *(get_gen_reg_addr(NETARM_GEN_PORTC))
#define I2C_SDA_PORTC

#else
#error "CONFIG_I2C_NETARM_GPIO_SDA_* is not properly configured."
#endif /* CONFIG_I2C_NETARM_GPIO_SDA_*/

#if (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB0) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC0))
#define I2C_BIT_SDA             0

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB1) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC1))
#define I2C_BIT_SDA             1

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB2) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC2))
#define I2C_BIT_SDA             2

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB3) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC3))
#define I2C_BIT_SDA             3

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB4) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC4))
#define I2C_BIT_SDA             4

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB5) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC5))
#define I2C_BIT_SDA             5

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB6) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC6))
#define I2C_BIT_SDA             6

#elif (defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTA7) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTB7) || \
        defined(CONFIG_I2C_NETARM_GPIO_SDA_PORTC7))
#define I2C_BIT_SDA             7

#else
#error "CONFIG_I2C_NETARM_GPIO_SDA_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_NETARM_GPIO_SDA_* */

/* Set up SDA port mode for GPIO */
#if (defined(CONFIG_NETARM_NS7520) && defined(I2C_SDA_PORTC))
#define I2C_SDA_PORT_SETUP(x)       (~(NETARM_GEN_PORT_MODE(x) | NETARM_GEN_PORT_CSF(x))) /* Handle NS7520 PORTC properly */
#else
#define I2C_SDA_PORT_SETUP(x)       (~(NETARM_GEN_PORT_MODE(x)))
#endif                        

/* Bitmasks for SCL and SDA lines */
#define I2C_MASK_SCL            (1 << I2C_BIT_SCL)
#define I2C_MASK_SDA            (1 << I2C_BIT_SDA)




/* ----------------------------------------------------------------------------
 * Local prototypes
 */
static void bit_netarm_gpio_inc_use(struct i2c_adapter *adap);
static void bit_netarm_gpio_dec_use(struct i2c_adapter *adap);
static int bit_netarm_gpio_reg(struct i2c_client *client);
static int bit_netarm_gpio_unreg(struct i2c_client *client);
static void bit_netarm_gpio_init(void);
static void bit_netarm_gpio_setscl(void *data, int state);
static void bit_netarm_gpio_setsda(void *data, int state);
static int bit_netarm_gpio_getscl(void *data);
static int bit_netarm_gpio_getsda(void *data);


/* ----------------------------------------------------------------------------
 * Data structures
 */
static struct i2c_algo_bit_data bit_netarm_gpio_data = {
        NULL,
        bit_netarm_gpio_setsda,
        bit_netarm_gpio_setscl,
        bit_netarm_gpio_getsda,
        bit_netarm_gpio_getscl,
        10,
        10,
        100
};

static struct i2c_adapter bit_netarm_gpio_ops = {
        "NetSilicon NetARM GPIO",
        I2C_HW_B_NETARM,
        NULL,
        &bit_netarm_gpio_data,
        bit_netarm_gpio_inc_use,
        bit_netarm_gpio_dec_use,
        bit_netarm_gpio_reg,
        bit_netarm_gpio_unreg,
};


/* ----------------------------------------------------------------------------
 * Public methods
 */
#if defined(MODULE)
int __init init_module(void)
{
        return(i2c_bit_netarm_gpio_init());
}


void __exit cleanup_module(void)
{
        i2c_bit_del_bus(&bit_netarm_gpio_ops);
}
#endif  /* MODULE */


int __init i2c_bit_netarm_gpio_init(void)
{
        printk(KERN_INFO "%s: NetSilicon NetARM GPIO I2C adapter\n",
                        __FILE__);
        printk(KERN_INFO "%s: SCL on %s%d, SDA on %s%d\n",
                        __FILE__,
                        I2C_PORT_SCL_STRING, I2C_BIT_SCL,
                        I2C_PORT_SDA_STRING, I2C_BIT_SDA);

        /* Set SCL and SDA lines for use as GPIO */
        bit_netarm_gpio_init();

        /* Set SDA and SCL as output initially */
        bit_netarm_gpio_setsda(NULL, 1);
        bit_netarm_gpio_setscl(NULL, 1);

        if (i2c_bit_add_bus(&bit_netarm_gpio_ops) < 0) {
                printk(KERN_ERR "%s: i2c_bit_add_bus() failed - adapter not "
                                "installed!\n", __FILE__);
                return(-ENODEV);
        }

        return(0);
}


/* ----------------------------------------------------------------------------
 * Private methods and helper functions
 */
static void bit_netarm_gpio_inc_use(struct i2c_adapter *adap)
{
#if defined(MODULE)
        MOD_INC_USE_COUNT;
#endif  /* MODULE */
}


static void bit_netarm_gpio_dec_use(struct i2c_adapter *adap)
{
#if defined(MODULE)
        MOD_DEC_USE_COUNT;
#endif  /* MODULE */
}


static int bit_netarm_gpio_reg(struct i2c_client *client)
{
        return(0);
}


static int bit_netarm_gpio_unreg(struct i2c_client *client)
{
        return(0);
}


static void bit_netarm_gpio_init(void)
{
        I2C_PORT_SCL &= I2C_SCL_PORT_SETUP(I2C_MASK_SCL);
        I2C_PORT_SDA &= I2C_SDA_PORT_SETUP(I2C_MASK_SDA);
}


static void bit_netarm_gpio_setscl(void *data, int state)
{
        I2C_PORT_SCL |= NETARM_GEN_PORT_DIR(I2C_MASK_SCL);

        if (state)
                I2C_PORT_SCL |= I2C_MASK_SCL;
        else
                I2C_PORT_SCL &= ~I2C_MASK_SCL;
}


static void bit_netarm_gpio_setsda(void *data, int state)
{
        I2C_PORT_SDA |= NETARM_GEN_PORT_DIR(I2C_MASK_SDA);


        if (state)
                I2C_PORT_SDA |= I2C_MASK_SDA;
        else
                I2C_PORT_SDA &= ~I2C_MASK_SDA;
}


static int bit_netarm_gpio_getscl(void *data)
{
        I2C_PORT_SCL &= ~NETARM_GEN_PORT_DIR(I2C_MASK_SCL);
        return((I2C_PORT_SCL & I2C_MASK_SCL) != 0);
}


static int bit_netarm_gpio_getsda(void *data)
{
        I2C_PORT_SDA &= ~NETARM_GEN_PORT_DIR(I2C_MASK_SDA);
        return((I2C_PORT_SDA & I2C_MASK_SDA) != 0);
}


/* ----------------------------------------------------------------------------
 * Driver public symbols
 */
EXPORT_NO_SYMBOLS;

#if defined(MODULE)
MODULE_AUTHOR("Arthur E. Shipkowski <art@videon-central.com>");
MODULE_DESCRIPTION("I2C-Bus GPIO driver for NetSilicon NetARM processors");
MODULE_LICENSE("GPL");
#endif  /* MODULE */

