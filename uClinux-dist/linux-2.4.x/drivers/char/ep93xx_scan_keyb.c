/******************************************************************************
 * 
 *  File:	linux/drivers/char/ep93xx_scan_keyb.c
 *
 *  Purpose:	Support for Cirrus EP93xx architecture core keyboard scanner.
 *
 *  Build:	Define CONFIG_VT.
 *		Do not define CONFIG_VT_CONSOLE yet.
 *
 *  History:	010406	Norman Farquhar at LynuxWorks
 *
 *		Initial version
 *		- Will scan keyboard and feed keyboard.c, which translates
 *		to input codes and puts them into tty console queue.
 *		- Raw mode tbd
 *		- keymaps other than default not supported
 *		- does not support standby mode yet
 *
 *  Limitations:
 *		1.  The EP93xx is limited to supporting small keyboards
 *		and cannot handle full PC style keyboards and key usage:
 *		a)  64 key limit: 8x8 key matrix
 *		b)  limited to 2 keys down MAXIMUM,
 *		which makes it impossible to support SHIFT+SHIFT+KEY
 *		states like shift-control or control-alt keys.
 *		2.  This means the default keyboard, 83key CherryG84-4001QAU/02,
 *		will have some dead keys.  
 *
 *
 * (c) Copyright 2001 LynuxWorks, Inc., San Jose, CA.  All rights reserved.
 *   
 *=============================================================================
 *	Overview of EP93xx Scan Keyboard driver
 *=============================================================================
 *
 *	The EP93xx scanned keyboard driver is a low-level hardware driver which
 *	supports the core logic scanned keyboard controller block of the
 *	EP93xx.  These machines are embedded systems and this keyboard driver 
 *	can support small keyboards.
 *
 *	The keyboard driver does not have a normal device driver interface
 *	and instead interfaces to the keyboard.c driver through function calls.
 *	Note that not all interface function calls are implemented at this time.
 *	(see /asm/arch/keyboard.h for function definitions):
 *
 *	ep93xx_scan_kbd_hw_init
 *		initializes the scan keyboard hw.
 *
 *	handle_scancode
 *		when scan controller generates interrupt, handler will
 *		pass scan codes to queue in keyboard.c with this function call.
 *	
 *	ep93xx_scan_kbd_translate
 *		called by keyboard.c to translate scan code stream to
 *		key code stream.  The keycode stream is then interpreted
 *		by keyboard.c according to current key shift state
 *		into a code suitable for Linux. 
 *
 *	Note that key scan codes will be delivered by this driver on both
 *	key down and key up events, using a coding similar to PC XT keyboard.
 *
 *	Note however that scan compatibility and key code compatibility
 *	to PC standard are not required by Linux, and are not guaranteed.
 *
 ******************************************************************************/

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/keyboard.h>
#include <asm/hardware.h>

//#define DEBUG

#define KEYREG_KEY1_MASK                        0x0000003F
#define KEYREG_KEY1_SHIFT                       0
#define KEYREG_KEY2_MASK                        0x00000Fc0
#define KEYREG_KEY2_SHIFT                       6

#define KEYREG_KEY1ROW_MASK                     0x00000007
#define KEYREG_KEY1ROW_SHIFT                    0
#define KEYREG_KEY1COL_MASK                     0x00000038
#define KEYREG_KEY1COL_SHIFT                    3

#define KEYREG_KEY2ROW_MASK                     0x000001c0
#define KEYREG_KEY2ROW_SHIFT                    6
#define KEYREG_KEY2COL_MASK                     0x00000E00
#define KEYREG_KEY2COL_SHIFT                    9

#define KEYREG_1KEY                             0x00001000
#define KEYREG_2KEYS                            0x00002000
#define KEYREG_INT                              0x00004000
#define KEYREG_K                                0x00008000

#define SCANINIT_DIS3KY                         0x00008000

//
//	Prototypes
//
int kmi_kbd_init(void)
{
    return(0);
}
void __init ep93xx_scan_kbd_hw_init( void);
int ep93xx_scan_kbd_translate( unsigned char scancode, unsigned char *keycode, char rawmode);
//
//	Code
//
int ep93xx_scan_kbd_translate( unsigned char scancode, unsigned char *keycode, char rawmode)
{
	// Translate scan code stream to key code
	// 
	// Our scan coding is simple: each key up/down event generates
	// a single scan code.
	//
	// TBD We translate scancode to keycode regardless of up/down status

	*keycode = scancode & ~KBUP;	// Mask off KBUP flag
	return 1;			// keycode is valid
}

void kbd_irq_handler( int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int		keystat, key1, key2;
	static unsigned int	lastkeystat=0, lastkey1=0, lastkey2=0;

	// Note: IRQ_KEY automatically disabled before entry,
	// and reenabled after exit by Linux interrupt code.

	// Reading status clears keyboard interrupt

	keystat = inl(KEY_REG) &
		  (KEYREG_KEY1COL_MASK|KEYREG_KEY1ROW_MASK|
		   KEYREG_KEY2COL_MASK|KEYREG_KEY2ROW_MASK|
		   KEYREG_1KEY|KEYREG_2KEYS);

#ifdef DEBUG	// DEBUGGING	
	if (keystat == lastkeystat)
		printk( "ep93xx_scan_keyb:  spurious interrupt, stat %x\n",
			 keystat);
	else
		printk( "ep93xx_scan_keyb:  interrupt, stat %x\n", keystat);
#endif
	if (keystat & KEYREG_1KEY)
			key1 = KEYCODE( (keystat&KEYREG_KEY1ROW_MASK)>>KEYREG_KEY1ROW_SHIFT,
				(keystat&KEYREG_KEY1COL_MASK)>>KEYREG_KEY1COL_SHIFT);
	else
		key1 = 0;	// invalid

	if (keystat & KEYREG_2KEYS)
			key2 = KEYCODE( (keystat&KEYREG_KEY2ROW_MASK)>>KEYREG_KEY2ROW_SHIFT,
				(keystat&KEYREG_KEY2COL_MASK)>>KEYREG_KEY2COL_SHIFT);
	else
		key2 = 0;	// invalid

	//
	// This 'monster' decision tree is used to decide what to report
	// when last key state has changed to current key state.
	// This may involve up to 4 keys changing state simultaneously:
	// lastkey1, lastkey2 going up and key1, key2 going down.
	//
	// We use keyboard scanner hardware guarantees to simplify the logic:
	//	key1 < key2	if both are valid
	//	key1 is valid 	if key2 is valid
	//
	//	handle_scancode called with down and up scancodes
	//		scancode = keycode when down
	//		scancode = keycode|KBUP when up
	//
	// Note that if more than one keys change state in the same scan period,
	// then we really do NOT know the order in which the key events occurred.
	// Our default behavior is to always report key up events before key down
	// events.  However, multiple key up events or multiple key down events
	// will be reported in no special order. 
	//
	if (!(lastkeystat & (KEYREG_1KEY|KEYREG_2KEYS)))  // No keys down lasttime
	{
		if (key1)
			handle_scancode( key1, 1);
		if (key2)
			handle_scancode( key2, 1);
	}
	else if (lastkey1 == key1)	   // means key still down
	{
		// no change for key1 or lastkey1, both valid
		if (lastkey2 != key2)
		{
			// lastkey2 went up if valid, key2 went down if valid
			if (lastkey2)
				handle_scancode( lastkey2|KBUP, 0);
			if (key2)
				handle_scancode( key2, 1);
		}
		//else no change for all keys
	}
	else if	(key1)		// key1 valid and
				// lastkey1 valid (because NOT no keys lasttime)
	{
		if (lastkey1 == key2)
		{
			// no change for lastkey1 or key2
			// lastkey2 went up if valid, key1 went down
			if (lastkey2)
				handle_scancode( lastkey2|KBUP, 0);
			handle_scancode( key1, 1);

		}
		else
		{
			// we know: lastkey1 valid and went up, key1 valid
			handle_scancode( lastkey1|KBUP, 0);
			
			if (lastkey2 == key1)
			{
				// no change for lastkey2 or key1
				// key2 went down if valid
				if (key2)
					handle_scancode( key2, 1);

			}
			else
			{
				if (lastkey2 != key2)
				{
					// lastkey2 went up if valid
					// key2 went down if valid
					if (lastkey2)
					    	handle_scancode( lastkey2|KBUP, 0);
					if (key2)
					    	handle_scancode( key2, 1);
				}
				//else no change for lastkey2 or key2
				
				// key1 valid and went down
				handle_scancode( key1, 1);
			}
	    	}
	}
	else		// key1 not valid and
			// lastkey1 valid (because NOT no keys lasttime)
	{
		// key1 not valid means both key1, key2 not valid
		// so lastkey1 went up and lastkey2 went up if valid
		handle_scancode( lastkey1|KBUP, 0);
		if (lastkey2)
			handle_scancode( lastkey2|KBUP, 0);
	}
	
	lastkeystat = keystat;
	lastkey1 = key1;
	lastkey2 = key2;
}


char 
ep93xx_unexpected_up(unsigned char xx)
{
    return 0x80;
}

static int
ep93xx_setkeycode(unsigned int xx, unsigned int yy)
{
    return -EINVAL;
}

static int
ep93xx_getkeycode(unsigned int xx)
{
    return -EINVAL;
}

static void
ep93xx_leds(unsigned char xx)
{
}

void __init ep93xx_scan_kbd_hw_init( void)
{
	int	i=3;
	unsigned int uiTemp;

	printk( "Entering ep93xx_scan_kbd_hw_init()\n");

	// TBD we initialize the key scanner to start running
	// TBD instead of putting it into standby and waiting
	// TBD for any key down interrupt

	cli();		// Atomic access needed

	// Warning: hardware default has already started scanner!

	// Make sure scanner enabled, active and that
	// Keyboard ROW/COL interface enabled.

	uiTemp = inl(SYSCON_DEVCFG);
	
	uiTemp &= ~(SYSCON_DEVCFG_KEYS | SYSCON_DEVCFG_GONK);
		
	SysconSetLocked(SYSCON_DEVCFG, uiTemp);


	// SYSCON locked automatically now after RSTCR written

	// Enable Keyboard Clock and select clock rate
	//
	//	TBD Boot ROM has already inited KTDIV = 0x20018004
	// 	TBD Measured default 64usec scan period with scope.

	// Assume that GPIO registers do not impact row,col pins since
	// they are assigned to keyboard scanner.
	
	// Setup Keyboard Scanner
	//
	// Note that we change the scanner parameters on the fly
	// while it is running, which is okay.  No need to disable
	// scanner while tweaking these.
	// 
	// TBD Keyboard scan rate will change as master clocks/dividers change
	//
	// For now, this gives us measured rate of about 480Hz row scan rate,
	// which implies 60Hz full kbd scan rate.  Together with debounce
	// count of 3, means debounce period = 3/60Hz = 50ms>30ms recommended,
	// so okay.
	//
	outl( (0x00FC00FA | SCANINIT_DIS3KY), SCANINIT );

	//TBD If too much capacitance on keyboard
	//outl( (0x00FC0020 | SCBACK | SCDIS3KEY), SCANINIT );

	uiTemp = inl(SYSCON_KTDIV) | SYSCON_KTDIV_KEN;
		
	SysconSetLocked(SYSCON_KTDIV, uiTemp);


	if (request_irq( IRQ_KEY, kbd_irq_handler, SA_INTERRUPT,
			 "ep93xx_scan_keyb", 0))
	{
		printk( KERN_ERR "EP93xx scan keyboard driver aborting"
				 " due to IRQ_KEY unavailable.\n");
		sti();
		//return -EBUSY;
	} 
	// Note: request_irq has just enabled IRQ_KEY for us.

	// Warning: We have initialized last key status to indicate
	// all keys up which may not be the current hardware state.
	//
	//	TBD If this is important to detect, to alert user
	//	TBD to a possibly faulty keyboard, then we could
	//	TBD manually scan the keyboard to verify all keys up.
	//
	
	// Three common cases here:
	//	1.  All keys up.  This is normal, expected status of keyboard.
	//	2.  All keys up, although at some time ago during initialization
	//	a key was momentarily pressed, causing the hardware to latch it.
	//	3.  Some key is being held down now.
	//
	// Reading status clears any pending keyboard interrupt.
	i = inl(KEY_REG);
	//
	// We believe this will have the following impact on common cases:
	//
	//	1.  No impact.
	//	2.  Momentary presses will be cleared out so they do not
	//	bother us.  Although we get a spurious key up immediately because
	//	the keyboard hardware will see change from last
	//	latched status and current status, higher level keyboard driver
	//	should ignore.
	//	3.  Key being held will generate a new pending key down
	//	event which is acceptable.

	sti();

	// Now keyboard is active and will generate interrupts
	// on key presses.  Driver only needs to handle interrupts.
	// There are NO driver ioctl or deinit functions in lowlevel.
	k_unexpected_up = ep93xx_unexpected_up;
	k_setkeycode    = ep93xx_setkeycode;
	k_getkeycode    = ep93xx_getkeycode;
	
	k_translate     = ep93xx_scan_kbd_translate;
	k_leds          = ep93xx_leds;

	printk( "Leaving ep93xx_scan_kbd_hw_init()\n");
}



