/*
 * drivers/char/ta7_watchdog.c
 *
 * by Craig Hackney <craig@triscend.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This driver is for the watchdog device on the Triscend-A7 CSoC
 * devices.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/ioport.h>
#include <linux/watchdog.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/arch/triscend_a7.h>

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

#ifdef CONFIG_DEVFS_FS
#undef WATCHDOG_MINOR
#define WATCHDOG_MINOR	MISC_DYNAMIC_MINOR
#endif

/*
 * Default timeout in clock cycles.
 */
#define DEFAULT_TIMEOUT 64000000

static unsigned long timeout;
static int reset = 1;
static int watchdogStarted;
static int watchdogFired;

static wait_queue_head_t watchdog_wait;

static void
watchdog_isr (int irq, void *dev_id, struct pt_regs *regs)
{
  SET_BIT(WATCHDOG_CLEAR_REG, WD_INT_CLR_BIT);

  if (reset)
      while (1);

  watchdogFired = 1;
  wake_up (&watchdog_wait);
}

static int
watchdog_open (struct inode *inode, struct file *file)
{
  MOD_INC_USE_COUNT;
  return (0);
}

static int
watchdog_release (struct inode *inode, struct file *file)
{

  if (!nowayout)
  {
    CLR_BIT(WATCHDOG_CONTROL_REG, WD_ENABLE_BIT);
    watchdogStarted = 0;
  }

  MOD_DEC_USE_COUNT;
  return (0);
}

static int
watchdog_read (struct file *file, char *buffer, size_t len, loff_t * f_pos)
{
  unsigned long wdValue = A7_REG( WATCHDOG_CURRENT_VAL_REG );

  if( len < sizeof( unsigned long ) )
    return( -EINVAL );

  copy_to_user (buffer, &wdValue, sizeof( unsigned long ) );
  return ( sizeof( unsigned long ) );
}

static int
watchdog_write (struct file *file,
		const char *data, size_t len, loff_t * ppos)
{
  if (!watchdogStarted)
    {
      SET_BIT( WATCHDOG_CONTROL_REG, WD_RESET_BIT );
      A7_REG( WATCHDOG_TIMEOUT_VAL_REG ) = timeout;
      SET_BIT( WATCHDOG_CONTROL_REG, WD_ENABLE_BIT );

      if (reset)
      	SET_BIT( WATCHDOG_CONTROL_REG, EN_WD_RST_BIT );
      else
	CLR_BIT( WATCHDOG_CONTROL_REG, EN_WD_RST_BIT );

      watchdogStarted = 1;
    }
  else
    SET_BIT( WATCHDOG_CONTROL_REG, WD_RESET_BIT );

  // Return the number of bytes written
  return (len);
}

static int
watchdog_ioctl (struct inode *inode,
		struct file *file, unsigned int cmd, unsigned long arg)
{
  switch (cmd)
    {
    case WDIOF_KEEPALIVEPING:
      SET_BIT( WATCHDOG_CONTROL_REG, WD_RESET_BIT );
      break;
    case WDIOF_SETTIMEOUT:
      // Set the watchdog timeout value
      timeout = arg;
      A7_REG(WATCHDOG_TIMEOUT_VAL_REG) = timeout;
      break;
    case WDIOS_DISABLECARD:
      CLR_BIT(WATCHDOG_CONTROL_REG, WD_ENABLE_BIT);
      watchdogStarted = 0;
      break;
    case WDIOS_ENABLECARD:
      SET_BIT( WATCHDOG_CONTROL_REG, WD_RESET_BIT );
      A7_REG( WATCHDOG_TIMEOUT_VAL_REG ) = timeout;
      SET_BIT( WATCHDOG_CONTROL_REG, WD_ENABLE_BIT );

      if (reset)
      	SET_BIT( WATCHDOG_CONTROL_REG, EN_WD_RST_BIT );
      else
	CLR_BIT( WATCHDOG_CONTROL_REG, EN_WD_RST_BIT );

      watchdogStarted = 1;
      break;
    default:
      return (-EINVAL);
    }
  return (0);
}

static unsigned int
watchdog_poll (struct file *file, poll_table * wait)
{
  poll_wait (file, &watchdog_wait, wait);

  if (watchdogFired)
    {
      watchdogFired = 0;
      return (POLLOUT | POLLWRNORM);
    }

  return (0);
}

static struct file_operations watchdog_fops = {
  owner:THIS_MODULE,
  read:watchdog_read,
  write:watchdog_write,
  poll:watchdog_poll,
  ioctl:watchdog_ioctl,
  open:watchdog_open,
  release:watchdog_release,
};

static struct miscdevice watchdog_miscdev = {
  WATCHDOG_MINOR,
  "watchdog",
  &watchdog_fops
};

void
ta7_watchdog_init (void)
{
  printk ("Triscend A7 Watchdog Driver: 1.00\n");

  watchdogStarted = 0;
  watchdogFired = 0;
  reset = 1;
  timeout = DEFAULT_TIMEOUT;

  init_waitqueue_head (&watchdog_wait);

  if (!request_region (WD_BASE, 16, "watchdog"))
    printk (KERN_ERR "watchdog: IO %X is not free.\n", (unsigned int ) WD_BASE);

  if (request_irq (IRQ_WATCHDOG_BIT,
		   watchdog_isr, SA_INTERRUPT, "watchdog", NULL))
    printk (KERN_ERR "watchdog: IRQ %d is not free.\n",
	    IRQ_WATCHDOG_BIT);

  misc_register (&watchdog_miscdev);
}

int
watchdog_init (void)
{
  ta7_watchdog_init ();
  return (0);
}

void
watchdog_exit (void)
{
  CLR_BIT(WATCHDOG_CONTROL_REG, WD_ENABLE_BIT);
  free_irq (IRQ_WATCHDOG_BIT, NULL);
  release_region (WD_BASE, 16);
  misc_deregister (&watchdog_miscdev);
}

module_init (watchdog_init);
module_exit (watchdog_exit);

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

MODULE_AUTHOR ("Craig Hackney");
MODULE_DESCRIPTION ("Driver to access the Triscend-A7 watchdog");
MODULE_LICENSE ("GPL");
EXPORT_NO_SYMBOLS;

