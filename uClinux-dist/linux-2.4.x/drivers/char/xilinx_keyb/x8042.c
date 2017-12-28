/*
 * x8042.c
 *
 * Expose the Xilinx PS/2 components as a typical PC 8042 keyboard controller
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under the terms
 * of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

/*
 * This code is a bit unusual in a few ways.  The first is that it is
 * emulating a hardware component: the typical PC's 8042 keyboard
 * controller.  With the current implementation of the keyboard code in
 * Linux, it seemed to make the most sense to not have yet another
 * hacked up pc_keyb.c to handle the unique interface that has been
 * exposed by Xilinx.  They basically expose the PS/2 interfaces just
 * like serial ports.
 *
 * This code defines four functions that are called by the typical PC
 * keyboard code when it wants to talk to the 8042.  These functions in
 * turn expose the Xilinx PS/2 interfaces via the typical 8042
 * interface.
 *
 * This brings us to another unusual aspect of this code.  Xilinx
 * provides drivers with their FPGA IP.  Their drivers are composed of
 * two logical parts where one part is the OS independent code and the
 * other part is the OS dependent code.  This file exposes their OS
 * independent functions as an 8042 keyboard controller.  The other
 * files in this directory are the OS independent files as provided by
 * Xilinx with no changes made to them.  The names exported by those
 * files begin with XPs2_.  All functions in this file that are called
 * by Linux have names that begin with x8042_.  The functions in this
 * file that have Handler in their name are registered as callbacks with
 * the underlying Xilinx OS independent layer.  Any other functions are
 * static helper functions.
 *
 * One more thing should be noted.  The words input and output can be
 * confusing in this context.  It depends upon whether you look at it
 * from the perspective of the main CPU or from the perspective of the
 * 8042 keyboard controller.  The KBD_STAT_OBF, KBD_STAT_MOUSE_OBF and
 * KBD_STAT_IBF defines from pc_keyb.h are from the perspective of the
 * 8042: OBF means Output Buffer Full which means that there is a byte
 * available to be read from the keyboard controller.  However,
 * pc_keyb.c also calls the functions kbd_read_input, kbd_read_status,
 * kbd_write_output and kbd_write_command.  The naming of these
 * functions is from the other perspective, that of the main CPU.
 *
 * There are some lurking issues that I have not addressed yet.  I haven't seen
 * them raise their heads, but I believe that the potential for a problem is
 * there.  The first issue is that nothing prevents the mouse from trashing the
 * keyboard and vice-versa.  I've tried to make this happen by holding down a
 * key while mucking with the mouse under X, but didn't see any problem, but it
 * still should probably be addressed.
 *
 * The second issue is that whenever we set an Output Buffer Full flag as the
 * result of a command, we should probably generate an interrupt.  I don't
 * think this is actually a problem because whenever pc_keyb.c sends a command
 * where a response is expected, it appears that it goes into a polling loop.
 * Something to remember though if you run into a problem.
 *
 * The final issue is that KBD_MODE_DISABLE_KBD and KBD_MODE_DISABLE_MOUSE are
 * not yet handled.  DISABLE_KBD is not used and DISABLE_MOUSE is only used
 * during startup and shutdown.  In short, this is another thing that should be
 * addressed.
 *
 * SAATODO: Address these issues.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/pc_keyb.h>

#include "xbasic_types.h"
#include "xps2.h"
#include "xps2_i.h"

#define VERSION 0x00
/* Note that if you change the following PS2_INDEXes, you will also need
 * to change the definition of KEYBOARD_IRQ and AUX_IRQ to match. */
#define KEYBOARD_PS2_INDEX	1	/* Keyboard is PS/2 #1 */
#define MOUSE_PS2_INDEX		0	/* Mouse is PS/2 #0 */

/* Our private per device data. */
struct xkeyb_dev {
	int index;		/* Which interface is this */
	int initialized;	/* Successfully initialized? */
	u32 save_BaseAddress;	/* Saved physical base address */
	/*
	 * The underlying OS independent code needs space as well.  A
	 * pointer to the following XPs2 structure will be passed to
	 * any XPs2_ function that requires it.  However, we treat the
	 * data as an opaque object in this file (meaning that we never
	 * reference any of the fields inside of the structure).
	 */
	XPs2 Ps2;
};
static struct xkeyb_dev keyboard = { index:KEYBOARD_PS2_INDEX };
static struct xkeyb_dev mouse = { index:MOUSE_PS2_INDEX };

/* We need to keep track of some global flags.  We use this struct to do so. */
static struct {
	unsigned int uninitialized:1;
	unsigned int mouse_enabled:1;
} gflags = {
1, 0};

/* The emulated 8042's registers */
static unsigned char from_host;	/* Last byte written from the host. */
static unsigned char to_host;	/* Next byte to be read by the host. */
static unsigned char status_reg = 0;	/* Status. */
static unsigned char command_byte;	/* The 8042's command byte. */

/* SAATODO: This function will be moved into the Xilinx code. */
/*****************************************************************************/
/**
*
* Lookup the device configuration based on the PS/2 instance.  The table
* XPs2_ConfigTable contains the configuration info for each device in the system.
*
* @param Instance is the index of the PS/2 interface being looked up.
*
* @return
*
* A pointer to the configuration table entry corresponding to the given
* device ID, or NULL if no match is found.
*
* @note
*
* None.
*
******************************************************************************/
XPs2_Config *XPs2_GetConfig(int Instance)
{
    /*
     * By definition, the Xilinx PS2_DUAL_REF core has two instances
     */
	if (Instance < 0 || Instance >= 2)
	{
		return NULL;
	}

	return &XPs2_ConfigTable[Instance];
}

static void
keybHandler(void *CallbackRef, u32 Event, unsigned int EventData)
{
	struct xkeyb_dev *dev = (struct xkeyb_dev *) CallbackRef;

	switch (Event) {
	case XPS2_EVENT_SENT_DATA:
		status_reg &= ~KBD_STAT_IBF;
		break;
	case XPS2_EVENT_RECV_DATA:
		if (XPs2_Recv(&dev->Ps2, &to_host, 1) != 1) {
			printk(KERN_ERR "Xilinx PS/2 received data missing.\n");
			break;
		}
		status_reg |= KBD_STAT_OBF;
		break;
	case XPS2_EVENT_RECV_ERROR:
	case XPS2_EVENT_RECV_OVF:
		status_reg |= KBD_STAT_PERR;
		break;
	case XPS2_EVENT_SENT_NOACK:
	case XPS2_EVENT_TIMEOUT:
		status_reg |= KBD_STAT_GTO;
		break;
	default:
		printk("PS2 #%d unrecognized event %u\n", dev->index, Event);
		break;
	}
}

static void
mouseHandler(void *CallbackRef, u32 Event, unsigned int EventData)
{
	struct xkeyb_dev *dev = (struct xkeyb_dev *) CallbackRef;

	switch (Event) {
	case XPS2_EVENT_SENT_DATA:
		status_reg &= ~KBD_STAT_IBF;
		break;
	case XPS2_EVENT_RECV_DATA:
		if (XPs2_Recv(&dev->Ps2, &to_host, 1) != 1) {
			printk(KERN_ERR "Xilinx PS/2 received data missing.\n");
			break;
		}
		status_reg |= AUX_STAT_OBF;
		break;
	case XPS2_EVENT_RECV_ERROR:
	case XPS2_EVENT_RECV_OVF:
	case XPS2_EVENT_SENT_NOACK:
	case XPS2_EVENT_TIMEOUT:
		/*
		 * It's not clear to me if the mouse should affect the status
		 * register.  I'm presuming it should not based upon how
		 * pc_keyb.c handles the condition.  If this is wrong, code
		 * should be copied from keybHandler.
		 */
		break;
	default:
		printk("PS2 #%d unrecognized event %u\n", dev->index, Event);
		break;
	}
}

static int
initdev(struct xkeyb_dev *dev, XPs2_Handler handler)
{

    /*
     * Each PS/2 instance in the OPB_PS2_DUAL_REF IP core
     * occupies 0x20 bytes of register space.
     */
	static const unsigned long remap_size = 0x20;
	XPs2_Config *cfg;

	/* Find the config for our device. */
	cfg = XPs2_GetConfig(dev->index);
	if (!cfg)
		return -ENODEV;

	/* Change the addresses to be virtual; save the old ones to restore. */
	dev->save_BaseAddress = cfg->BaseAddress;
	cfg->BaseAddress = (u32) ioremap(dev->save_BaseAddress, remap_size);

	if (XPs2_Initialize(&dev->Ps2, cfg->DeviceId) != XST_SUCCESS) {
		printk(KERN_ERR
		       "Xilinx PS/2 #%d: Could not initialize device.\n",
		       dev->index);
		return -ENODEV;
	}

	/* Set up the interrupt handler. */
	XPs2_SetHandler(&dev->Ps2, handler, dev);

	dev->initialized = 1;
	printk(KERN_INFO "Xilinx PS/2 #%d at 0x%08X mapped to 0x%08X\n",
	       dev->index, dev->save_BaseAddress, cfg->BaseAddress);

	return 0;
}

static void
init_all(void)
{
	(void) initdev(&keyboard, keybHandler);
	(void) initdev(&mouse, mouseHandler);
	gflags.uninitialized = 0;
}

unsigned char
x8042_read_input(void)
{
	if (gflags.uninitialized)
		init_all();

	status_reg &= ~AUX_STAT_OBF;
	return to_host;
}

EXPORT_SYMBOL(x8042_read_input);

unsigned char
x8042_read_status(void)
{
	unsigned char rtn;
	if (gflags.uninitialized)
		init_all();

	/*
	 * This is not very pretty.  Typically, I would have the
	 * interrupts come into this code and then call
	 * XPs2_InterruptHandler to take care of them.  However, in this
	 * case, pc_keyb.c receives the interrupts and then calls its
	 * handle_kbd_event to take care of both mouse and keyboard
	 * interrupts.  This in turn calls kbd_read_status which is
	 * #define'ed to be this function.  Both handle_kbd_event and
	 * XPs2_InterruptHandler handle the case of finding nothing to
	 * do well, so that leads to the following code that just gives
	 * the underlying Xilinx code a chance to handle the interrupts
	 * everytime this function is called.
	 */
	XPs2_InterruptHandler(&keyboard.Ps2);
	XPs2_InterruptHandler(&mouse.Ps2);

	rtn = status_reg;
	status_reg &= ~(KBD_STAT_GTO | KBD_STAT_PERR);
	return rtn;
}

EXPORT_SYMBOL(x8042_read_status);

void
x8042_write_output(unsigned char val)
{
	XPs2 *dest_ps2 = &keyboard.Ps2;
	unsigned char prior_command, old_command_byte, changed;

	if (gflags.uninitialized)
		init_all();

	prior_command = (status_reg & KBD_STAT_CMD) ? from_host : 0;
	from_host = val;
	status_reg &= ~KBD_STAT_CMD;

	/* Check to see if this byte is a parameter for the last command. */
	switch (prior_command) {
	case KBD_CCMD_WRITE_MODE:
		old_command_byte = command_byte;
		/* There are eight different flags in the command byte.  Of
		 * these eight, we force three of them (KBD_MODE_NO_KEYLOCK,
		 * KBD_MODE_KCC, and KBD_MODE_RFU) to always be zero.
		 * NO_KEYLOCK is an older AT thing and we don't have an inhibit
		 * keyboard switch anyway.  KCC tells the keyboard controller
		 * to do scan code conversion, which we don't handle.  RFU is
		 * apparently reserved for future use.
		 *
		 * KBD_MODE_SYS apparently is for the system's use so we just
		 * let it be toggled on and off without any action on our
		 * part.
		 *
		 * KBD_MODE_KBD_INT and KBD_MODE_MOUSE_INT are checked here to
		 * see when they are turned on and off so the underlying Xilinx
		 * routine can be called to turn interrupts on and off.
		 *
		 * This leaves KBD_MODE_DISABLE_KBD and KBD_MODE_DISABLE_MOUSE
		 * which are not handled yet.
		 */

		command_byte = val & (KBD_MODE_KBD_INT
				      | KBD_MODE_MOUSE_INT
				      | KBD_MODE_SYS
				      | KBD_MODE_DISABLE_KBD
				      | KBD_MODE_DISABLE_MOUSE);
		changed = old_command_byte ^ command_byte;
		if (changed & KBD_MODE_KBD_INT) {
			if (command_byte & KBD_MODE_KBD_INT)
				XPs2_EnableInterrupt(&keyboard.Ps2);
			else
				XPs2_DisableInterrupt(&keyboard.Ps2);
		}
		if (changed & KBD_MODE_MOUSE_INT) {
			if (command_byte & KBD_MODE_MOUSE_INT)
				XPs2_EnableInterrupt(&mouse.Ps2);
			else
				XPs2_DisableInterrupt(&mouse.Ps2);
		}

		return;		/* We're done here. */
	case KBD_CCMD_WRITE_AUX_OBUF:
		to_host = val;
		status_reg |= AUX_STAT_OBF;
		return;		/* We're done here. */
	case KBD_CCMD_WRITE_MOUSE:
		/* Send this to the mouse. */
		dest_ps2 = &mouse.Ps2;
		break;
	}

	/* We need to send this byte to one of the PS/2 interfaces. */
	status_reg |= KBD_STAT_IBF;
	XPs2_Send(dest_ps2, &val, 1);
}

EXPORT_SYMBOL(x8042_write_output);

void
x8042_write_command(unsigned char val)
{
	if (gflags.uninitialized)
		init_all();

	/* Commands operate in three different ways:
	 * 1) A command byte is written and then another byte is written
	 *    to provide a parameter for the command.
	 * 2) A command byte is written and then a byte is read.
	 * 3) The command is self-contained and no other bytes are read
	 *    or written.
	 * The commands that fall into the last category are handled
	 * immediately in this function.  The other two are handled in
	 * x8042_write_output() and x8042_read_input() when the next
	 * byte comes in.  Those functions look at KBD_STAT_CMD within
	 * status_reg to know that a command is pending.
	 */

	status_reg |= KBD_STAT_CMD | KBD_STAT_IBF;
	from_host = val;

	switch (val) {
	case KBD_CCMD_READ_MODE:
		to_host = command_byte;
		status_reg |= KBD_STAT_OBF;
		break;
	case KBD_CCMD_GET_VERSION:
		to_host = VERSION;
		status_reg |= KBD_STAT_OBF;
		break;
	case KBD_CCMD_MOUSE_DISABLE:
		gflags.mouse_enabled = 0;
		break;
	case KBD_CCMD_MOUSE_ENABLE:
		gflags.mouse_enabled = 1;
		break;
	case KBD_CCMD_TEST_MOUSE:
		/* If we failed initialization, return an error. */
		to_host = mouse.initialized ? 0x00 : 0x04;
		status_reg |= KBD_STAT_OBF;
		break;
	case KBD_CCMD_SELF_TEST:
		/* Say our self test passed. */
		to_host = 0x55;
		status_reg |= (KBD_STAT_OBF | KBD_STAT_SELFTEST);
		break;
	case KBD_CCMD_KBD_TEST:
		/* If we failed initialization, return an error. */
		to_host = keyboard.initialized ? 0x00 : 0x04;
		status_reg |= KBD_STAT_OBF;
		break;
	case KBD_CCMD_KBD_DISABLE:
		if (!(command_byte & KBD_MODE_DISABLE_KBD)) {
			/* Keyboard was enabled and we need to disable it. */
			command_byte |= KBD_MODE_DISABLE_KBD;
		}
		break;
	case KBD_CCMD_KBD_ENABLE:
		if (command_byte & KBD_MODE_DISABLE_KBD) {
			/* Keyboard was disabled and we need to enable it. */
			command_byte &= ~KBD_MODE_DISABLE_KBD;
		}
		break;
	case KBD_CCMD_WRITE_AUX_OBUF:
	case KBD_CCMD_WRITE_MOUSE:
	case KBD_CCMD_WRITE_MODE:
		/* Handled in x8042_write_output() */
		break;
	default:
		printk(KERN_ERR "x8042: unrecognized command 0x%X\n", val);
		break;
	}

	status_reg &= ~KBD_STAT_IBF;
}

EXPORT_SYMBOL(x8042_write_command);
