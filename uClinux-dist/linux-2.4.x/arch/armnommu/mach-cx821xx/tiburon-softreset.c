/*
 * (C) 2005, Johann Hanne, GPL
 *
 * The Actiontec Dual PC Modem (aka DPCM and Tiburon) has a
 * reset button which must be software controlled; it is
 * connected to GPIO line 27
 *
 * This module implements an interrupt handler for it which
 * is limited to 1 interrupt per second (this is done by disabling
 * the interrupt as soon as one arrives and launching a timer
 * which enables it again); for each interrupt that occurs, a
 * SIGINT is sent to pid 1 (which should be an init process); note that 
 * it's left to the pid 1 process what to do; it could for example 
 * execute /sbin/reboot; it could also reset the configuration to 
 * factory defaults if the button remeins pressed for some seconds
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/mtd/compatmac.h>
#include <linux/mtd/mtd.h>
#include <asm/arch/bsptypes.h>
#include <asm/arch/cnxtflash.h>
#include <asm/arch/gpio.h>
#include <asm/io.h>

static struct timer_list tiburon_softreset_reenable_GPIOB27_timerlist;

static void tiburon_softreset_reenable_GPIOB27(unsigned long arg) {
  SetGPIOIntEnable(GPIO27, 1);
}

void tiburon_softreset_GPIOB27_Handler(int gpio_num)
{
  /* Send a SIGINT to pid 1 (the init process) */
  kill_proc(1, SIGINT, 1);

  SetGPIOIntEnable(GPIO27, 0);
  /* WriteGPIOData(GPIO27, 1); */
  ClearGPIOIntStatus(GPIO27);

  /* Send a SIGINT to pid 1 every second as long
     as the button is pressed */
  tiburon_softreset_reenable_GPIOB27_timerlist.expires = jiffies + HZ;
  add_timer(&tiburon_softreset_reenable_GPIOB27_timerlist);
}

static void __exit cleanup_tiburon_softreset(void)
{
  if (tiburon_softreset_reenable_GPIOB27_timerlist.expires) {
    del_timer(&tiburon_softreset_reenable_GPIOB27_timerlist);
  }

  SetGPIOIntEnable(GPIO27, 0);
  ClearGPIOIntStatus(GPIO27);
}

int __init init_tiburon_softreset(void)
{
  SetGPIOIntEnable(GPIO27, 0);
  SetGPIODir(GPIO27, GP_INPUT);
  SetGPIOIntPolarity(GPIO27, GP_IRQ_POL_NEGATIVE);
  SetGPIOInputEnable(GPIO27, GP_INPUTON);
  GPIO_SetGPIOIRQRoutine(GPIO27, tiburon_softreset_GPIOB27_Handler);
  SetGPIOIntEnable(GPIO27, 1);

  tiburon_softreset_reenable_GPIOB27_timerlist.function = 
    tiburon_softreset_reenable_GPIOB27;
  tiburon_softreset_reenable_GPIOB27_timerlist.data = 0;
  tiburon_softreset_reenable_GPIOB27_timerlist.expires = 0;

  return 0;
}

module_init(init_tiburon_softreset);
module_exit(cleanup_tiburon_softreset);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johann Hanne");
MODULE_DESCRIPTION("Monitor the soft-reset button on the Tiburon (Actiontec DPCM)");
