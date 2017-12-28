/******************************************************************************
 * 
 *  File:	linux/drivers/char/ep93xx_spi_kbd.c
 *
 *  Purpose:	Support for SPI Keyboard for a Cirrus Logic EP93xx
 *
 *  History:	
 *
 *  Limitations:
 *  Break and Print Screen keys not handled yet!
 *
 *
 *  Copyright 2003 Cirrus Logic Inc.
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
 *   
 ******************************************************************************/

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/keyboard.h>
#include <asm/hardware.h>
#include <asm/arch/ssp.h>

#define EP93XX_MAX_KEY_DOWN_COUNT 6


void DataCallback(unsigned int Data);
static int g_SSP_Handle;

//-----------------------------------------------------------------------------
//  Debug stuff...
// For debugging, let's spew messages out serial port 1 or 3 at 57,600 baud.
//-----------------------------------------------------------------------------

#undef UART_HACK_DEBUG
// #define UART_HACK_DEBUG 1

#ifdef UART_HACK_DEBUG
static char szBuf[256];
void UARTWriteString(char * msg);

#define DPRINTK( x... )   \
    sprintf( szBuf, ##x ); \
    UARTWriteString( szBuf );
#else
#define DPRINTK( x... )
#endif


#ifdef UART_HACK_DEBUG

// This "array" is a cheap-n-easy way of getting access to the UART registers.
static unsigned int * const pDebugUART=(unsigned int *)IO_ADDRESS(UART1_BASE);
//static unsigned int * const pDebugUART=(unsigned int *)IO_ADDRESS(UART3_BASE);
static int bUartInitialized = 0; 

void SendChar(char value)
{
    // wait for Tx fifo full flag to clear.
    while (pDebugUART[0x18>>2] & 0x20);

    // send a char to the uart               
    pDebugUART[0] = value;
}

void UARTWriteString(char * msg) 
{
    int index = 0;

    //if((pDebugUART[0x14>>2] & 0x1) == 0)
    if( bUartInitialized == 0 )
    {
        uiTemp = inl(SYSCON_DEVCFG);
		uiTemp |= SYSCON_DEVCFG_U1EN;
		//uiTemp |= SYSCON_DEVCFG_U3EN;
        SysconSetLocked(SYSCON_DEVCFG, uiTemp);  
        pDebugUART[0x10>>2] = 0xf;
        pDebugUART[0xc>>2] = 0;
        pDebugUART[0x8>>2] = 0x70;
        pDebugUART[0x14>>2] = 0x1;
        bUartInitialized = 1;
    }
    while (msg[index] != 0)
    {
        if (msg[index] == '\n')
        {
            SendChar('\r');
            SendChar('\n');
        }
        else 
        {
            SendChar(msg[index]);
        }
        index++;
    }
}
#endif // UART_HACK_DEBUG


static unsigned int ExtendedKscanVirtualKey(unsigned char ucExtendedKey);
static unsigned char SPI2KScan(unsigned int uiSPIValue, int * pValid);
static void InitSniffer(void);
static void KeySniffer(unsigned char scancode, int down);
static void Check4StuckKeys(void);

#define KSCAN_TABLE_SIZE    0x88
#define KeyStateDownFlag        1

#define KC_NONE         0
#define KC_ESC          1   // Escape
#define KC_ONE          2   // 1 !          
#define KC_TWO          3   // 2 @
#define KC_THREE        4   // 3 #      
#define KC_FOUR         5   // 4 $          
#define KC_FIVE         6   // 5 %         
#define KC_SIX          7   // 6 ^     
#define KC_SEVEN        8   // 7 &
#define KC_EIGHT        9   // 8 *
#define KC_NINE         10  // 9 (
#define KC_ZERO         11  // 0 )
#define KC_MINUS        12  // - _
#define KC_EQUAL        13  // = +
#define KC_BACKSPACE    14  // Delete
#define KC_TAB          15  // Tab
#define KC_Q            16  // q               
#define KC_W            17  // w               
#define KC_E            18  // e
#define KC_R            19  // r               
#define KC_T            20 	// t               
#define KC_Y            21 	// y               
#define KC_U            22 	// u               
#define KC_I            23 	// i               
#define KC_O            24 	// o               
#define KC_P            25 	// p               
#define KC_BRACKETLEFT  26  // [ {
#define KC_BRAKCETRIGHT 27  // ] }
#define KC_RETURN       28  // Return          
#define KC_LCONTROL     29  // Control         
#define KC_A            30  // a
#define KC_S            31 	// s               
#define KC_D            32 	// d
#define KC_F            33 	// f
#define KC_G            34 	// g               
#define KC_H            35 	// h               
#define KC_J            36 	// j               
#define KC_K            37 	// k               
#define KC_L            38 	// l               
#define KC_SEMICOLON    39  // ; :
#define KC_APOSTROPHE   40  // ' "
#define KC_GRAVE        41  // ` ~
#define KC_LEFTSHIFT    42  // Left Shift           
#define KC_BACKSLASH    43  // \ |
#define KC_Z            44 	// z               
#define KC_X            45 	// x               
#define KC_C            46 	// c
#define KC_V            47 	// v               
#define KC_B            48 	// b
#define KC_N            49  // n               
#define KC_M            50  // m               
#define KC_COMMA        51  // , <
#define KC_PERIOD       52  // . >
#define KC_SLASH        53  // / ?
#define KC_RIGHTSHIFT   54  // Right Shift           
#define KC_KP_MULTIPLY  55  // KP_*
#define KC_ALT          56  // Alt  
#define KC_SPACE        57  // Space
#define KC_CAPSLOCK     58  // CapsLock       
#define KC_F1           59  // F1
#define KC_F2           60 	// F12
#define KC_F3           61 	// F13
#define KC_F4           62 	// F14
#define KC_F5           63 	// F15
#define KC_F6           64 	// F16
#define KC_F7           65 	// F17
#define KC_F8           66 	// F18
#define KC_F9           67 	// F19
#define KC_F10          68 	// F20
#define KC_NUMLOCK      69  // NumLock
#define KC_SCROLLLOCK   70  // Scroll Lock
#define KC_KP_7         71  // KP 7            
#define KC_KP_8         72  // KP_8            
#define KC_KP_9         73  // KP_9            
#define KC_KP_MINUS     74  // KP_-     
#define KC_KP_4         75  // KP_4            
#define KC_KP_5         76 	// KP_5            
#define KC_KP_6         77 	// KP_6            
#define KC_KP_ADD       78 	// KP_+
#define KC_KP_1         79 	// KP_1            
#define KC_KP_2         80 	// KP_2            
#define KC_KP_3         81 	// KP_3            
#define KC_KP_0         82 	// KP_0            
#define KC_KP_PERIOD    83  // KP_.
#define KC_LAST_CONSOLE 84  // Last_Console    
#define KC_F11          87  // F11
#define KC_F12          88  // F12
#define KC_KP_ENTER     96  // KP_Enter        
#define KC_RCONTROL     97  // Control         
#define KC_KP_DIVIDE    98  // KP_/       
#define KC_RALT         100 // Right Alt
#define KC_BREAK        101 // Break           
#define KC_HOME         102 // Home
#define KC_UP           103 // Up              
#define KC_PGUP         104 // Page Up
#define KC_LEFT         105 // Left            
#define KC_RIGHT        106 // Right           
#define KC_END          107 // End
#define KC_DOWN         108 // Down            
#define KC_PGDOWN       109 // Page Down            
#define KC_INSERT       110 // Insert          
#define KC_DELETE       111 // Delete
#define KC_13           113 // F13             
#define KC_14           114 // F14             
#define KC_PAUSE        119 // Pause

#define EXTENDED_KEY     0x80

//
// This table is used to map the scan code to the Linux default keymap.
//
static unsigned int const KScanCodeToVKeyTable[KSCAN_TABLE_SIZE] =
{
    KC_NONE,            // Scan Code 0x0
    KC_F9,              // Scan Code 0x1
    KC_NONE,            // Scan Code 0x2
    KC_F5,              // Scan Code 0x3
    KC_F3,              // Scan Code 0x4
    KC_F1,              // Scan Code 0x5
    KC_F2,              // Scan Code 0x6
    KC_F12,             // Scan Code 0x7
    KC_NONE,            // Scan Code 0x8
    KC_F10,             // Scan Code 0x9
    KC_F8,              // Scan Code 0xA
    KC_F6,              // Scan Code 0xB
    KC_F4,              // Scan Code 0xC
    KC_TAB,             // Scan Code 0xD Tab
    KC_GRAVE,           // Scan Code 0xE '
    KC_NONE,            // Scan Code 0xF
    KC_NONE,            // Scan Code 0x10
    KC_ALT,             // Scan Code 0x11 Left Menu
    KC_LEFTSHIFT,       // Scan Code 0x12 Left Shift
    KC_NONE,            // Scan Code 0x13
    KC_LCONTROL,        // Scan Code 0x14
    KC_Q,               // Scan Code 0x15
    KC_ONE,             // Scan Code 0x16
    KC_NONE,            // Scan Code 0x17
    KC_NONE,            // Scan Code 0x18
    KC_NONE,            // Scan Code 0x19
    KC_Z,               // Scan Code 0x1A
    KC_S,               // Scan Code 0x1B
    KC_A,               // Scan Code 0x1C
    KC_W,               // Scan Code 0x1D
    KC_TWO,             // Scan Code 0x1E
    KC_NONE,            // Scan Code 0x1F
    KC_NONE,            // Scan Code 0x20
    KC_C,               // Scan Code 0x21
    KC_X,               // Scan Code 0x22
    KC_D,               // Scan Code 0x23
    KC_E,               // Scan Code 0x24
    KC_FOUR,            // Scan Code 0x25
    KC_THREE,           // Scan Code 0x26
    KC_NONE,            // Scan Code 0x27
    KC_NONE,            // Scan Code 0x28
    KC_SPACE,           // Scan Code 0x29  Space
    KC_V,               // Scan Code 0x2A
    KC_F,               // Scan Code 0x2B
    KC_T,               // Scan Code 0x2C
    KC_R,               // Scan Code 0x2D
    KC_FIVE,            // Scan Code 0x2E
    KC_NONE,            // Scan Code 0x2F
    KC_NONE,            // Scan Code 0x30
    KC_N,               // Scan Code 0x31
    KC_B,               // Scan Code 0x32 B
    KC_H,               // Scan Code 0x33
    KC_G,               // Scan Code 0x34
    KC_Y,               // Scan Code 0x35
    KC_SIX,             // Scan Code 0x36
    KC_NONE,            // Scan Code 0x37
    KC_NONE,            // Scan Code 0x38
    KC_NONE,            // Scan Code 0x39
    KC_M,               // Scan Code 0x3A
    KC_J,               // Scan Code 0x3B
    KC_U,               // Scan Code 0x3C
    KC_SEVEN,           // Scan Code 0x3D
    KC_EIGHT,           // Scan Code 0x3E
    KC_NONE,            // Scan Code 0x3F
    KC_NONE,            // Scan Code 0x40
    KC_COMMA,           // Scan Code 0x41
    KC_K,               // Scan Code 0x42
    KC_I,               // Scan Code 0x43	
    KC_O,               // Scan Code 0x44
    KC_ZERO,            // Scan Code 0x45
    KC_NINE,            // Scan Code 0x46
    KC_NONE,            // Scan Code 0x47
    KC_NONE,            // Scan Code 0x48
    KC_PERIOD,          // Scan Code 0x49
    KC_SLASH,           // Scan Code 0x4A
    KC_L,               // Scan Code 0x4B
    KC_SEMICOLON,       // Scan Code 0x4C
    KC_P,               // Scan Code 0x4D
    KC_MINUS,           // Scan Code 0x4E
    KC_NONE,            // Scan Code 0x4F
    KC_NONE,            // Scan Code 0x50
    KC_NONE,            // Scan Code 0x51
    KC_APOSTROPHE,      // Scan Code 0x52
    KC_NONE,            // Scan Code 0x53
    KC_BRACKETLEFT,     // Scan Code 0x54
    KC_EQUAL,           // Scan Code 0x55
    KC_BACKSPACE,       // Scan Code 0x56
    KC_NONE,            // Scan Code 0x57
    KC_CAPSLOCK,        // Scan Code 0x58 Caps Lock
    KC_RIGHTSHIFT,      // Scan Code 0x59 Right Shift
    KC_RETURN,          // Scan Code 0x5A
    KC_BRAKCETRIGHT,    // Scan Code 0x5B
    KC_NONE,            // Scan Code 0x5C
    KC_BACKSLASH,       // Scan Code 0x5D
    KC_NONE,            // Scan Code 0x5E
    KC_NONE,            // Scan Code 0x5F
    KC_NONE,            // Scan Code 0x60
    KC_BACKSLASH,       // Scan Code 0x61 ?? //VK_BSLH,            
    KC_NONE,            // Scan Code 0x62
    KC_NONE,            // Scan Code 0x63
    KC_NONE,            // Scan Code 0x64
    KC_NONE,            // Scan Code 0x65
    KC_BACKSPACE,       // Scan Code 0x66 ?? //VK_BKSP,            
    KC_NONE,            // Scan Code 0x67
    KC_NONE,            // Scan Code 0x68
    KC_KP_1,            // Scan Code 0x69
    KC_NONE,            // Scan Code 0x6A
    KC_KP_4,            // Scan Code 0x6B
    KC_KP_7,            // Scan Code 0x6C
    KC_NONE,            // Scan Code 0x6D
    KC_NONE,            // Scan Code 0x6E
    KC_NONE,            // Scan Code 0x6F
    KC_KP_0,            // Scan Code 0x70
    KC_KP_PERIOD,       // Scan Code 0x71 DECIMAL??
    KC_KP_2,            // Scan Code 0x72
    KC_KP_5,            // Scan Code 0x73
    KC_KP_6,            // Scan Code 0x74
    KC_KP_8,            // Scan Code 0x75
    KC_ESC,             // Scan Code 0x76
    KC_NUMLOCK,         // Scan Code 0x77
    KC_F11 ,            // Scan Code 0x78
    KC_KP_ADD,          // Scan Code 0x79
    KC_KP_3,            // Scan Code 0x7A
    KC_KP_MINUS,        // Scan Code 0x7B
    KC_KP_MULTIPLY,     // Scan Code 0x7C
    KC_KP_9,            // Scan Code 0x7D
    KC_SCROLLLOCK,      // Scan Code 0x7E
    KC_NONE,            // Scan Code 0x7F
    KC_NONE,            // Scan Code 0x80      
    KC_NONE,            // Scan Code 0x81
    KC_NONE,            // Scan Code 0x82
    KC_F7,              // Scan Code 0x83
    KC_NONE,            // Scan Code 0x84
    KC_NONE,            // Scan Code 0x85
    KC_NONE,            // Scan Code 0x86
    KC_NONE             // Scan Code 0x87
};

//=============================================================================
// KeyboardTranslate
//=============================================================================
// Translate the raw scan code to the default keymap for linux keyboards.
//=============================================================================
int KeyboardTranslate(unsigned char scancode, unsigned char * keycode, 
    char rawmode)
{
    //
    // Was this an extended key?
    //
    if(scancode & EXTENDED_KEY)
    {
        *keycode = ExtendedKscanVirtualKey(scancode & (~EXTENDED_KEY));
    }
    else
    {
	    *keycode = KScanCodeToVKeyTable[scancode];
    }

    //
    // Indicate that this is a valid keycode.
    //
	return 1;
}

typedef struct{
	unsigned char scancode;
	unsigned char count;
} key_down_tracker_t;

//
// In the interest of efficiency, let's only allow 5 keys to be down
// at a time, maximum.  So if anybody is having a temper tantrum on
// their keyboard, they may get stuck keys, but that's to be expected.
//
#define MAX_KEYS_DOWN 8
static key_down_tracker_t KeyTracker[MAX_KEYS_DOWN];

//=============================================================================
// InitSniffer
//=============================================================================
static void InitSniffer(void)
{
	int i;
	
	//
	// Clear our struct to indicate that no keys are down now.
	// If somebody boots this thing while holding down keys, then they'll
	// get what they deserve.
	//
	for( i=0 ; i < MAX_KEYS_DOWN ; i++ )
	{
		KeyTracker[i].count = 0;
		KeyTracker[i].scancode = 0;
	}
}

//=============================================================================
// KeySniffer
//=============================================================================
// To prevent stuck keys, keep track of what keys are down.  This information
// is used by Check4StuckKeys().
//=============================================================================
static void KeySniffer(unsigned char scancode, int down)
{
	int i;

	//
	// There are certain keys that will definately get held down
	// and we can't interfere with that.
	//
	switch( scancode )
	{
		case 0x12: /* left  shift */
		case 0x59: /* right shift */
		case 0x14: /* left  ctrl  */
		case 0x94: /* right ctrl  */
		case 0x11: /* left  alt   */
		case 0x91: /* right alt   */
		case 0x58: /* caps lock   */
		case 0x77: /* Num lock    */
			handle_scancode( scancode, down );
			return;
	
		default:
			break;
	}

    //printk("Sniff - %02x, %d\n", scancode, down );

	//
	// Go thru our array, looking for the key.  If it already
	// is recorded, update its count.
	// Also look for empty cells in the array in case we
	// need one.
	//
	for( i=0 ; i < MAX_KEYS_DOWN ; i++ )
	{
		//
		// If this is a key up in our list then we are done.
		//
		if (down == 0)
		{
			if (KeyTracker[i].scancode == scancode)
			{
				KeyTracker[i].count = 0;
				KeyTracker[i].scancode = 0;
				handle_scancode( scancode, down );
				break;
			}
		}
		//
		// Hey here's an unused cell.  Save its index.
		//
		else if(KeyTracker[i].count == 0)
		{
			KeyTracker[i].scancode = scancode;
			KeyTracker[i].count = 1;
			handle_scancode( scancode, down );
			break;
		}
	}
}

//=============================================================================
// Check4StuckKeys
//=============================================================================
// When a key is held down longer than 1/2 sec, it start repeating
// 10 times a second.  What we do is watch how long each key is
// held down.  If longer than X where X is less than 1/2 second
// then we assume it is stuck and issue the key up.  If we were
// wrong and the key really is being held down, no problem because
// the keyboard is about to start sending it to us repeatedly
// anyway.
//=============================================================================
static void Check4StuckKeys(void)
{
	int i;

	for( i=0 ; i < MAX_KEYS_DOWN ; i++ )
	{
		if( KeyTracker[i].count )
		{
			KeyTracker[i].count++;
			if( KeyTracker[i].count >= EP93XX_MAX_KEY_DOWN_COUNT )
			{
				handle_scancode( KeyTracker[i].scancode, 0 );
				KeyTracker[i].count = 0;
				KeyTracker[i].scancode = 0;
			}
		}
	}
}

//=============================================================================
// HandleKeyPress
//=============================================================================
// Checks if there are any keys in the FIFO and processes them if there are.
//=============================================================================
void HandleKeyPress(unsigned int Data)
{
    static unsigned char ucKScan[4] = {0,0,0,0};
    static unsigned int ulNum = 0;
	int bParityValid;
	
	//
    // No keys to decode, but the timer went off and is calling us
    // to check for stuck keys.
    //
    if (Data == -1)
	{
		Check4StuckKeys();
		return;
	}
	
    //
    // Read in the value from the SPI controller.
    //
    ucKScan[ulNum++] = SPI2KScan( Data, &bParityValid );

	//
	// Bad parity?	We should read the rest of the fifo and
	// throw it away, because it will all be bad.  Then the
	// SSP will be reset when we close the SSP driver and 
	// all will be good again.
	//
	if(!bParityValid)
	{
		ulNum = 0;
	}

    //
    // If we have one character in the array, do the following.
    //
    if(ulNum == 1)
    {
        //
        // If it is a simple key without the extended scan code perform 
        // following.
        //
        if(ucKScan[0] < KSCAN_TABLE_SIZE)
        {
            DPRINTK("1:Dn %02x\n", ucKScan[0]);
            KeySniffer(ucKScan[0], 1);
            ulNum  = 0;
        }            

		//
		// I don't know what type of character this is so erase the 
		// keys stored in the buffer and continue.
		//
		else if((ucKScan[0] !=0xF0) && (ucKScan[0] !=0xE0))
		{
           	DPRINTK("1:oops - %02x\n",	ucKScan[0] );
            ulNum = 0;
        }
    }
	else if(ulNum == 2)
	{
		//
		// 0xF0 means that a key has been released.
		//
		if(ucKScan[0] == 0xF0)
		{
			//
			// If it is a simple key without the extended scan code 
			// perform the following.
			//
			if(ucKScan[1] < KSCAN_TABLE_SIZE)
			{
				DPRINTK("2:Up %02x %02x\n", ucKScan[0], ucKScan[1]);
				KeySniffer(ucKScan[1], 0);
				ulNum = 0;
			}            
			//
			// If it a extended kscan continue to get the next byte.
			//
			else if(ucKScan[1] !=0xE0)
			{
				DPRINTK("2:oops - %02x %02x\n",	ucKScan[0], ucKScan[1]);
				ulNum = 0;
			}
		}                                    
		//
		// Find out what extended code it is.
		//
		else if(ucKScan[0] == 0xE0 && ucKScan[1] != 0xF0)
		{
			DPRINTK("2:Dn %02x %02x\n", ucKScan[0], ucKScan[1]);
			KeySniffer(EXTENDED_KEY | ucKScan[1], 1);
			ulNum = 0;
		}
	}
	//
	// This means that an extended code key has been released.
	//
	else if (ulNum == 3)
	{
		//
		// 0xF0 means that a key has been released.
		//
		if(ucKScan[0] == 0xE0 && ucKScan[1] == 0xF0)
		{
			DPRINTK("3:Up %02x %02x %02x", 
				ucKScan[0], ucKScan[1], ucKScan[2] );
			KeySniffer(EXTENDED_KEY | ucKScan[2], 0);
		}
		else
		{
			DPRINTK("3:oops - %02x %02x %02x\n",
					ucKScan[0], ucKScan[1], ucKScan[2]);
		}
		ulNum = 0;
	}
}

//=============================================================================
//=============================================================================
char UnexpectedUp(unsigned char xx)
{
	//printk("UnexpectedUp - %02x\n",xx);
    return 200;
}

//=============================================================================
//=============================================================================
static int SetKeycode(unsigned int xx, unsigned int yy)
{
    return -EINVAL;
}

//=============================================================================
//=============================================================================
static int GetKeycode(unsigned int xx)
{
    return -EINVAL;
}

//=============================================================================
//=============================================================================
static void SetLEDs(unsigned char xx)
{
}

//=============================================================================
// ExtendedVirtualKey
//=============================================================================
// Converts an extended key to its virtual key code.
// 
//=============================================================================
static unsigned int ExtendedKscanVirtualKey(unsigned char ucExtendedKey)
{
    unsigned int uiVKey = 0;
    switch(ucExtendedKey)
    {
        case 0x03: // This is really 0x83
        {
            uiVKey = KC_F7;
            break;
        }
        case 0x11:
        {
            uiVKey = KC_RALT;
            break;
        }
        case 0x14:
        {
            uiVKey = KC_RCONTROL;
            break;
        }
        case 0x4a:
        {
            uiVKey = KC_KP_DIVIDE;
            break;
        }
        case 0x5a:  
        {
            uiVKey = KC_RETURN;
            break;
        }
        case 0x69:
        {
            uiVKey = KC_END;
            break;
        }
        case 0x6b:  
        {
            uiVKey = KC_LEFT;
            break;
        }
        case 0x6c:  
        {
            uiVKey = KC_HOME;
            break;
        }
        case 0x70:
        {
            uiVKey = KC_INSERT;
            break;
        }
        case 0x71:  
        {
            uiVKey = KC_DELETE;
            break;
        }
        case 0x72:
        {
            uiVKey = KC_DOWN;
            break;
        }
        case 0x74:
        {
            uiVKey = KC_RIGHT;
            break;
        }
        case 0x75:
        {
            uiVKey = KC_UP;
            break;
        }
        case 0x7a:
        {
            uiVKey = KC_PGDOWN;
            break;
        }
        case 0x7d:
        {
            uiVKey = KC_PGUP;
            break;
        }
        default:
        {
            DPRINTK("Unknown Extended Key - XX\n");
            uiVKey = 0;
            break;
        }
    }
    return (uiVKey);
}

//=============================================================================
// SPI2KScan
//=============================================================================
// Get a character from the spi port if it is available.
// 
// Below is a picture of the spi signal from the PS2. 
//
//CK HHllllHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHllll
//DA HHHllllll000000111111222222333333444444555555666666777777ppppppssssssLLLLHHH
//        ^                                                                ^
//    start bit                                                   important bit
// 
//where:  l = 8042 driving the line 
//        L = KEYBOARD driving the line
//        1..7 data
//         = Parity 8042 driving
//        s = stop   8042 driving
//         = PARITY KEYBOARD driving the line
//        S = STOP   KEYBOARD driving the line
//
//  In our design the value comes high bit first and is inverted.  So we must
//  convert it to low byte first and then inverted it back.
//
//=============================================================================
static unsigned char SPI2KScan(unsigned int uiSPIValue, int * pValid)
{
    unsigned char ucKScan = 0;
    unsigned int uiParity = 0;
    unsigned int uiCount = 0;

    
    for( uiCount = 1 ; uiCount < 10 ; uiCount++ )
    {
        uiParity += (uiSPIValue >> uiCount) & 0x1;
    }
    

    if( !(uiParity & 0x1)  && (uiSPIValue & 0x401) == 0x400 )
    {
        *pValid = 1;
    
	    //
	    // Invert the pattern.
	    //
	    uiSPIValue = ~uiSPIValue;
	    
	    //
	    // Read in the value from the motorola spi file
	    //
	    ucKScan =  (unsigned char)((uiSPIValue & 0x004)<< 5);
	    ucKScan |= (unsigned char)((uiSPIValue & 0x008)<< 3);
	    ucKScan |= (unsigned char)((uiSPIValue & 0x010)<< 1);
	    ucKScan |= (unsigned char)((uiSPIValue & 0x020)>> 1);
	    ucKScan |= (unsigned char)((uiSPIValue & 0x040)>> 3);
	    ucKScan |= (unsigned char)((uiSPIValue & 0x080)>> 5);
	    ucKScan |= (unsigned char)((uiSPIValue & 0x100)>> 7);
	    ucKScan |= (unsigned char)((uiSPIValue & 0x200)>> 9);
    }
    else
    {
        *pValid = 0;
    }
    
    return (ucKScan);        
}    

//=============================================================================
// EP93XXSpiKbdInit
//=============================================================================
void __init EP93XXSpiKbdInit(void)
{
	//printk( "Entering EP93XXSpiKbdInit()\n");

	//
	// Open SSP driver for Keyboard input.
	//
    g_SSP_Handle = SSPDriver->Open(PS2_KEYBOARD, HandleKeyPress);
	
	InitSniffer();

	//
	// Now keyboard is active and will generate interrupts
	// on key presses.  Driver only needs to handle interrupts.
	// There are NO driver ioctl or deinit functions in lowlevel.
	//
	k_unexpected_up = UnexpectedUp;
	k_setkeycode    = SetKeycode;
	k_getkeycode    = GetKeycode;
	
	k_translate     = KeyboardTranslate;
	k_leds          = SetLEDs;

	DPRINTK( "Leaving EP93XXSpiKbdInit()\n");
}
