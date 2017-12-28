/*  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/tty.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/arch/blobcalls.h>
#ifdef CONFIG_MVC_DEBUG_CONSOLE
#include <linux/console.h>
#endif
#include "islmvc.h"
#include "isl_mgt.h"
extern unsigned int src_seq;

#ifdef BLOB_V2
#include <linux/timer.h>
#include <linux_mvc_oids.h>
static struct timer_list timer; /* For the reset button state polling loop. */
#endif BLOB_V2

/*
 * The trap handling mechanism is different for BLOB V1 and BLOB V2.
 *
 */
#ifdef BLOB_V2
static inline void req_trap_handling(void)
{
    struct msg_conf     trapmsg;
    struct obj_mlme     data_buf;

    long                *new_buf = NULL;

    /* Most traps are the size of an obj_mlme struct or less. If this does not fit the trap size
       requirement, we are told so and will malloc a new buffer for the trap data. */
    trapmsg.data = (long*)&data_buf;
    trapmsg.size = sizeof(data_buf);

    /* Get the trap from the MVC... */
    if (blob_trap (&trapmsg) == BER_OVERFLOW)
    {
        /* The size of the buffer we passed was too small. Malloc a new buffer with correct size. */
        if ((new_buf = kmalloc(trapmsg.size, GFP_ATOMIC)) == NULL)
        {
            printk(KERN_ERR "Couldn't malloc new trap buffer. \n");
            return;
        }

        trapmsg.data = new_buf;
        /* Okay, try again... */
        if (blob_trap (&trapmsg) == BER_OVERFLOW)
        {
            printk(KERN_WARNING "Couldn't get trap message for the second time. Stopped trying... \n");
            return;
        }
    }

    mgt_response( 0, src_seq++, DEV_NETWORK_BLOB, BLOB_IFINDEX,
       PIMFOR_OP_TRAP, trapmsg.oid, trapmsg.data, trapmsg.size);

    /* When we malloced a new buffer, free it here. */
    if (new_buf)
        kfree(new_buf);

}
#else /* BLOB_V2 */
static inline void req_trap_handling(void)
{
    struct msg_conf *trapmsg = blob_trap( );

    if( trapmsg != NULL ) {
        /* Since there is no size parameter in MVC v1 we use 100 for the length
         * of the trap msg. This should be big enough to hold all trap msgs. */
        mgt_response( 0, src_seq++, DEV_NETWORK_BLOB, BLOB_IFINDEX,
            PIMFOR_OP_TRAP, trapmsg->oid, trapmsg->data, 100 );
    }
}
#endif /* BLOB_V2 */

/**********************************************************************
 *  handle_blob_reqs
 *
 *  DESCRIPTION: Handle requests for device blob. These requests are
 *		 handles within the Ethernet device. Therefore, traps
 *		 for the blob device should be added to the Ethernet device.
 *
 *  PARAMETERS:	 NONE
 *
 *  RETURN:	 NONE
 *
 *  NOTE:	 Called from the interrupt service routine.
 *
 **********************************************************************/
void handle_blob_reqs(void)
{
    int requests;

    requests = blob_reqs();

    if (!requests)
    return;

    if (requests & 0xFFFFEFE0) {
        printk (" called with unknown requests: %d!!\n", requests);
        return;
    }

    if (requests & REQTRAP)
    {
        req_trap_handling();
    }

    if (requests & REQWATCHDOG)
    {
        /* Lets trigger the blob watchdog service, for the BLOB device */
        blob_watchdog();
    }
}



/*
 * ------------------------------------------------------------
 * console driver
 * ------------------------------------------------------------
 */
static int debugdev_enabled = 0;

#ifdef CONFIG_MVC_DEBUG_CONSOLE

static struct console mvc_dbg_cons;

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void mvc_debug_console_write(struct console *co, const char *s,
				unsigned count)
{
    struct msg_data debug_msg;

    debug_msg.data = "<printk> ";
    debug_msg.length = strlen(debug_msg.data);
    debug_msg.max = 0;
    dev_write(DEVICE_DEBUG, &debug_msg);

    debug_msg.data = (unsigned char *)s;
    debug_msg.length = count;
    dev_write(DEVICE_DEBUG, &debug_msg);
}

static kdev_t mvc_debug_console_device(struct console *c)
{
    return MKDEV(TTY_MAJOR, 64 + c->index);
}

/*
 *	Setup initial baud/bits/parity/flow control. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init mvc_debug_console_setup(struct console *co, char *options)
{

#ifdef BLOB_V2
    struct msg_start start;
#endif

    int ret = -1;

    if(!debugdev_enabled) {
        /* start the MVC debug interface */

#ifdef BLOB_V2
        start.nr    = DEVICE_DEBUG;
        start.mode  = MODE_NONE;

        if(dev_start(DEVICE_DEBUG, &start) < BER_NONE)
            goto out;
#else
        if(dev_start(DEVICE_DEBUG, 0) < BER_NONE)
            goto out;
#endif /* BLOB_V2 */

        if(dev_run(DEVICE_DEBUG) < BER_NONE) {
            dev_stop(DEVICE_DEBUG);
            goto out;
        }
    }

    co->cflag = CREAD | HUPCL | CLOCAL | B115200;
    debugdev_enabled = 1;
    ret = 0;
out:
    return ret;
}

static struct console mvc_dbg_cons = {
    name:	"mvcdbg",
    write:	mvc_debug_console_write,
    device:	mvc_debug_console_device,
    setup:	mvc_debug_console_setup,
    flags:	CON_PRINTBUFFER,
    index:	-1,
};

/*
 *	Register console.
 */
void __init mvc_debug_console_init(void)
{
    register_console(&mvc_dbg_cons);
}
#endif


/**********************************************************************
 *  reset_button_state_timer
 *
 *  DESCRIPTION: The state of the reset button is checked here. If the
 *               button is pressed, a trap is sent. A second trap is sent
 *               when the button is released again.
 *
 *  PARAMETERS:	 Data, this holds the previous state of the reset button.
 *
 *  NOTE:        This code is ISL3893 specific.
 *
 **********************************************************************/
#ifdef BLOB_V2
static void reset_button_state_timer(unsigned long data)
{
    struct msg_conf msg;

    long bank = 1;

    msg.operation = OPSET;
    msg.oid       = BLOB_OID_GPIOBANK;
    msg.data      = &bank; /* GP Bank 1 */
    msg.size      = sizeof(long);

    /* Set the GPIO bank for the reset button */
    if (blob_conf (&msg) < 0)
        goto schedule;

    msg.operation = OPGET;
    msg.oid       = BLOB_OID_GPIOSTATUS;

    /* Get the current status of the reset button */
    if (blob_conf (&msg) < 0)
        goto schedule;

    if (!(*msg.data & 0x8000)) /* Button pressed */
    {
        if (timer.data) /* Button still pressed */
            goto schedule;
        else /* Button pressed for first time, send trap. */
        {
            timer.data = 1;
            mgt_response( 0, src_seq++, DEV_NETWORK_BLOB, BLOB_IFINDEX,
                          PIMFOR_OP_TRAP, GEN_OID_BUTTONSTATE, &timer.data, sizeof(long) );
            goto schedule;
        }
    }

    if (timer.data) /* Button released */
    {
        timer.data = 0;
        /* Send the trap up... */
        mgt_response( 0, src_seq++, DEV_NETWORK_BLOB, BLOB_IFINDEX,
                      PIMFOR_OP_TRAP, GEN_OID_BUTTONSTATE, &timer.data, sizeof(long) );
    }

    /* Schedule this poll again... */
schedule:
    timer.expires = jiffies + 0,1*HZ; /* Every 100 ms */
    add_timer(&timer);

}
#endif /* BLOB_V2 */


/*
 * Startup and exit functions
 */
static int __init prism_blobdrv_init(void)
{
    int ret = -ENODEV;

#ifdef BLOB_V2
    struct msg_start start;
    struct msg_conf msg;
    long watchdog = 0;
#endif

#ifdef CONFIG_MVC_DEBUG
	/* Configure the MVC debug interface. */
/* Is this needed?
#ifdef BLOB_V2
    struct msg_setup setup;

    setup.rx_buf = NULL; // Use the MVC internal buffers
    setup.tx_buf = NULL;

    setup.rx_buf_size = 100;
    setup.tx_buf_size = 100;

    if (dev_setup(DEVICE_DEBUG, &setup) < BER_NONE)
        goto out;
#endif  // BLOB_V2
*/

    /* start the MVC debug interface */
#ifdef BLOB_V2
    start.nr    = DEVICE_DEBUG;
    start.mode  = MODE_NONE;

    if(dev_start(DEVICE_DEBUG, &start) < BER_NONE)
            goto out;
#else
    if(dev_start(DEVICE_DEBUG, 0) < BER_NONE)
            goto out;
#endif /* BLOB_V2 */

    if(dev_run(DEVICE_DEBUG) < BER_NONE) {
            dev_stop(DEVICE_DEBUG);
            goto out;
    }

    /* Schedule timer for checking the reset button status. */
#ifdef BLOB_V2
    init_timer(&timer);
    timer.expires   = jiffies + 0,1*HZ; /* Every 100 ms */
    timer.data      = 0; /* Initial state of the reset button. */
    timer.function  = &reset_button_state_timer;	/* timer handler */
    add_timer(&timer);
#endif /* BLOB_V2 */

    debugdev_enabled = 1;

#ifdef BLOB_V2
    msg.operation = OPSET;
    msg.oid       = GEN_OID_WATCHDOG;
    msg.data      = &watchdog;
    msg.size      = sizeof(long);
    /* Set the WATCHDOG on for the blob device */
    blob_conf (&msg);
#endif
    ret = 0;
out:
#endif
    return ret;
}

static void __exit prism_blobdrv_exit(void)
{
#ifdef CONFIG_MVC_DEBUG
    debugdev_enabled = 0;

    dev_halt(DEVICE_DEBUG);
    dev_stop(DEVICE_DEBUG);
#ifdef BLOB_V2
    del_timer(&timer);
#endif

#endif
}


EXPORT_NO_SYMBOLS;

module_init(prism_blobdrv_init);
module_exit(prism_blobdrv_exit);
