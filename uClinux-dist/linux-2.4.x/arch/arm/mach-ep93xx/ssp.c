/*
 *  FILE:			ssp.c
 *
 *  DESCRIPTION:	SSP Interface Driver Module implementation
 *
 *  Copyright Cirrus Logic Corporation, 2001-2003.  All rights reserved
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

/*
 *  This driver provides a way to read and write devices on the SSP
 *  interface.
 *
 *  For Tx devices, EGPIO7 is used as an address pin:
 *  I2S Codec CS4228        = EGPIO7 == 1
 *  Serial Flash AT25F1024  = EGPIO7 == 0
 */
#include <linux/delay.h>
#include <asm/irq.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/ssp.h>

#undef DEBUG
// #define DEBUG 1
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


#define EP93XX_KEY_TIMER_PERIOD_MSEC 20

static int SSP_Open(SSPDeviceType Device, SSPDataCallback Callback);
static int SSP_Close(int Handle);
static int SSP_Read(int Handle, unsigned int Addr, unsigned int *pValue);
static int SSP_Write(int Handle, unsigned int Addr, unsigned int Value);
static int CheckHandle(int Handle);

static void  SetSSPtoPS2(void);
static void  SetSSPtoI2S(void);
static void  SetSSPtoFLASH(void);
static int  ReadIntoBuffer(void);

static int SSP_Write_I2SCodec(int Handle, unsigned int RegAddr,unsigned int RegValue);

/*
 * Key buffer...
 */
#define KEYBUF_SIZE 256
static unsigned int uiKeyBuffer[KEYBUF_SIZE];
static spinlock_t ssp_spinlock = SPIN_LOCK_UNLOCKED;


typedef enum{
	SSP_MODE_UNKNOWN = 0,
	SSP_MODE_PS2,
	SSP_MODE_I2S,
	SSP_MODE_FLASH,
} SSPmodes_t;

static struct timer_list g_KbdTimer;
static SSPmodes_t gSSPmode = SSP_MODE_UNKNOWN;
static SSPDataCallback gKeyCallback = 0;
static int gHookedInterrupt = 0;

/*
 * Keep the last valid handle for SSP for kbd, i2s, and flash
 */
static int iLastValidHandle = -1;
static int KeyboardHandle = 0;
static int I2SHandle = 0;
static int FlashHandle = 0;

#define SSP_DEVICE_MASK    0xf0000000
#define SSP_DEVICE_SHIFT	 28

SSP_DRIVER_API SSPinstance =
{
	SSP_Open,
	SSP_Read,
	SSP_Write,
	SSP_Close,
};

/*
 * The only instance of this driver.
 */
SSP_DRIVER_API *SSPDriver = &SSPinstance;

//=============================================================================
// SSPIrqHandler
//=============================================================================
// This routine will get all of the keys out of the SPI FIFO.
//=============================================================================
void SSPIrqHandler( int irq, void *dev_id, struct pt_regs *regs)
{
	//
	// Get key codes from SSP and send them to the keyboard callback.
	//
	ReadIntoBuffer();
	
	//
	// Clear the interrupt.
	//
	outl( 0, SSPIIR );
}

//=============================================================================
// TimerRoutine
//=============================================================================
// This function is called periodically to make sure that no keys are stuck in
// the SPI FIFO.  This is necessary because the SPI only interrupts on half
// full FIFO which can leave up to one keyboard event in the FIFO until another
// key is pressed.
//=============================================================================
static void TimerRoutine(unsigned long Data)
{
	int keycount;

	//
	// Get key codes from SSP and send them to the keyboard callback.
	//
	keycount = ReadIntoBuffer();
	
	//
	// If no keys were received, call the Data callback anyway so it can
	// check for stuck keys.
	//
	if( (keycount==0) && gKeyCallback )
	{
		gKeyCallback(-1);
	}
	
	//
	// Reschedule our timer in another 20 mSec.
	//
	g_KbdTimer.expires = jiffies + MSECS_TO_JIFFIES( EP93XX_KEY_TIMER_PERIOD_MSEC );
	add_timer(&g_KbdTimer);
}

/*
 * HookInterrupt
 *
 * Requests SSP interrupt, sets up interrupt handler, sets up keyboard polling
 * timer.
 */
static int HookInterrupt(void)
{
	if (gHookedInterrupt)
	{
		printk( KERN_ERR "SSP driver interrupt already hooked\n");
		return(-1);
	}


	if (request_irq(IRQ_SSPRX, SSPIrqHandler, SA_INTERRUPT, "ep93xxsspd", 0))
	{
		printk( KERN_ERR "SSP driver failed to get IRQ handler\n");
		return(-1);
	}
	
	gHookedInterrupt = 1;

	//
	// Initialize the timer that we will use to poll the SPI.
	//
	init_timer(&g_KbdTimer);
	g_KbdTimer.function = TimerRoutine;
	g_KbdTimer.data = 1;
	g_KbdTimer.expires = jiffies + MSECS_TO_JIFFIES( EP93XX_KEY_TIMER_PERIOD_MSEC );

	add_timer(&g_KbdTimer);
	
	return(0);
}

static int SSP_Open(SSPDeviceType Device, SSPDataCallback Callback)
{
	int Handle;
	
	/*
	 * Generate a handle and pass it back.
	 *
	 * Increment the last valid handle.
	 * Check for wraparound (unlikely, but we like to be complete).
	 */
	iLastValidHandle++;
	
	if((iLastValidHandle & ~SSP_DEVICE_MASK) == 0)
	{
		/*
		 * If we wrapped around start over.  Unlikely.
		 */
		iLastValidHandle = 1;
	}
	
	Handle = iLastValidHandle | (Device << SSP_DEVICE_SHIFT);

	switch (Device)
	{
		case PS2_KEYBOARD:
		{
			DPRINTK("SSP_Open - PS2_KEYBOARD\n");
			if (KeyboardHandle)
			{
				return(-1);
			}
			else
			{
				DPRINTK("Handle:%08x  Callback:%08x  -- Success\n",
					Handle, (unsigned int)Callback);

				KeyboardHandle = Handle;
				//
				// Hook the interrupt if we have not yet.
				//
				HookInterrupt();
				SetSSPtoPS2();
				gKeyCallback = Callback;
			}
			break;
		}
		case I2S_CODEC:
		{
			DPRINTK("SSP_Open - I2S_CODEC\n");
			if (I2SHandle)
			{
				return(-1);
			}
			else
			{
				DPRINTK("Handle:%08x  Callback:%08x  -- Success\n",
					Handle, (unsigned int)Callback);

				I2SHandle = Handle;
			}
			break;
		}
		case SERIAL_FLASH:
		{
			DPRINTK("SSP_Open - SERIAL_FLASH\n");
			if (FlashHandle)
			{
				return(-1);
			}
			else
			{
				DPRINTK("Handle:%08x  Callback:%08x  -- Success\n",
					Handle, (unsigned int)Callback);
				FlashHandle = Handle;
			}
			break;
		}
		default:
		{
			return(-1);
		}
	}
	
	/*
	 * Return the handle.
	 */
	return(Handle );
}

/*
 * Release that Handle!
 */
static int SSP_Close(int Handle)
{
	//
	// Find out which device this API was called for.
	//
	switch( CheckHandle(Handle) )
	{
		case PS2_KEYBOARD:
		{
			DPRINTK("SSP_Open - PS2_KEYBOARD\n");
			del_timer(&g_KbdTimer);
			free_irq(IRQ_SSPRX, 0);
			gKeyCallback = 0;
			KeyboardHandle = 0;
			gHookedInterrupt = 0;
			break;
		}
		case I2S_CODEC:
		{
			DPRINTK("SSP_Open - I2S_CODEC\n");
			I2SHandle = 0;
			break;
		}
		case SERIAL_FLASH:
		{
			DPRINTK("SSP_Open - SERIAL_FLASH\n");
			FlashHandle = 0;
			break;
		}
		default:
		{
			return(-1);
		}
	}
	return 0;
}

static int SSP_Read_FLASH
(
	int Handle,
	unsigned int RegAddr,
	unsigned int *pValue
)
{
	SSPmodes_t saved_mode;

	DPRINTK("SSP_Read_FLASH\n");

	spin_lock(&ssp_spinlock);

	/*
	 * Save the SSP mode.  Switch to FLASH mode if we're not
	 * already in FLASH mode.
	 */
	saved_mode = gSSPmode;
	SetSSPtoFLASH();

	/*
	 * Let TX fifo clear out.  Poll the Transmit Fifo Empty bit.
	 */
	while( !( inl(SSPSR) & SSPSR_TFE ) )
		barrier();

	/*
	 * Write the SPI read command.
	 */
	outl( 0x03, SSPDR );
	outl( (RegAddr >> 16) & 255, SSPDR );
	outl( (RegAddr >> 8) & 255, SSPDR );
	outl( RegAddr & 255, SSPDR );

	/*
	 * Delay long enough for one byte to be transmitted.  It takes 7.6uS to
	 * write a single byte.
	 */
	udelay(10);

	/*
	 * Read a byte to make sure the FIFO doesn't overrun.
	 */
	while( !( inl(SSPSR) & SSPSR_RNE ) )
		barrier();
	inl( SSPDR );

	/*
	 * Write four more bytes so that we can read four bytes.
	 */
	outl( 0, SSPDR );
	outl( 0, SSPDR );
	outl( 0, SSPDR );
	outl( 0, SSPDR );

	/*
	 * Delay long enough for three bytes to be transmitted.  It takes 7.6uS
	 * to write a single byte.
	 */
	udelay(25);

	/*
	 * Read three and throw away the next tree bytes.
	 */
	while( !( inl(SSPSR) & SSPSR_RNE ) )
		barrier();
	inl( SSPDR );
	while( !( inl(SSPSR) & SSPSR_RNE ) )
		barrier();
	inl( SSPDR );
	while( !( inl(SSPSR) & SSPSR_RNE ) )
		barrier();
	inl( SSPDR );

	/*
	 * Delay long enough for four bytes to be transmitted.  It takes 7.6uS
	 * to write a single byte.
	 */
	udelay(30);

	/*
	 * Read the data word.
	 */
	while( !( inl(SSPSR) & SSPSR_RNE ) )
		barrier();
	*pValue = inl( SSPDR );
	while( !( inl(SSPSR) & SSPSR_RNE ) )
		barrier();
	*pValue |= inl( SSPDR ) << 8;
	while( !( inl(SSPSR) & SSPSR_RNE ) )
		barrier();
	*pValue |= inl( SSPDR ) << 16;
	while( !( inl(SSPSR) & SSPSR_RNE ) )
		barrier();
	*pValue |= inl( SSPDR ) << 24;

	/*
	 * Wait until the transmit buffer is empty (it should be...).
	 */
	while( !( inl(SSPSR) & SSPSR_TFE ) )
		barrier();

	/*
	 * Read any residual bytes in the receive buffer.
	 */
	while( inl(SSPSR) & SSPSR_RNE )
		inl( SSPDR );
	
	/*
	 * If we were in PS2 mode, switch back to PS2 mode.
	 * If we weren't in PS2 mode, that means we didn't compile in
	 * the PS2 keyboard support, so no need to switch to PS2 mode.
	 */
	if( saved_mode == SSP_MODE_PS2 )
		SetSSPtoPS2();

	spin_unlock(&ssp_spinlock);

	/*
	 * Return success.
	 */
	return 0;
}

static int SSP_Read(int Handle, unsigned int Addr, unsigned int *pValue)
{
	DPRINTK("SSP_Read\n");

	/*
	 * Find out which device this API was called for.
	 */
	switch( CheckHandle(Handle) )
	{
		case SERIAL_FLASH:
		{
			return SSP_Read_FLASH(Handle, Addr, pValue);
		}
		default:
		{
			return -1;
		}
	}
}

static int SSP_Write(int Handle, unsigned int Addr, unsigned int Value)
{
	int iRet = 0;
	// DPRINTK("SSP_Write - Handle:0x%08x  Addr:0x%08x  Value:0x%08x\n",
	//    Handle, Addr, Value );
	
	//
	// Find out which device this API was called for.
	//
	switch( CheckHandle(Handle) )
	{
		case PS2_KEYBOARD:
		{
			break;
		}
		case I2S_CODEC:
		{
			iRet = SSP_Write_I2SCodec( Handle, Addr, Value );
			break;
		}
		case SERIAL_FLASH:
		{
			break;
		}
		default:
		{
			return(-1);
		}
	}

	return iRet;
}

static void SetSSPtoPS2(void)
{
	unsigned int uiRegTemp;
	
	if( gSSPmode == SSP_MODE_PS2 )
	{
		return;
	}

	/*
	 * Disable the SSP, disable interrupts
	 */
	outl( 0, SSPCR1 );

	/*
	 * It takes almost a millisecond for a key to come in so
	 * make sure we have completed all transactions.
	 */
	mdelay(1);

	//
	// Set EGPIO7 to disable EEPROM device on EDB9301, EDB9312, and EDB9315.
	//
	uiRegTemp = inl(GPIO_PADDR);
	outl( uiRegTemp | 0x80, GPIO_PADDR );

	uiRegTemp = inl(GPIO_PADR);
	outl( uiRegTemp | 0x80, GPIO_PADR );

    /*
     * Disable SFRM1 to I2S codec by setting EGPIO8 (port B, bit 0).
     * The EDB9315 board needs this but is harmless on the EDB9312 board.
     */
#if defined(CONFIG_ARCH_EDB9312) || defined(CONFIG_ARCH_EDB9315)
    uiRegTemp = inl(GPIO_PBDDR) | 0x01;
    outl( uiRegTemp, GPIO_PBDDR );

    uiRegTemp = inl(GPIO_PBDR) | 0x01;
    outl( uiRegTemp, GPIO_PBDR );
	
	uiRegTemp = inl(GPIO_PBDR);
#endif

    /*
     * Disable SFRM1 to I2S codec I2S by setting EGPIO6 (port A, bit 6).
     * The EDB9301 board needs this
     */
#if defined(CONFIG_ARCH_EDB9301) || defined(CONFIG_ARCH_EDB9302)
	uiRegTemp = inl(GPIO_PADDR);
	outl( uiRegTemp | 0x40, GPIO_PADDR );

	uiRegTemp = inl(GPIO_PADR);
	outl( uiRegTemp | 0x40, GPIO_PADR );

	uiRegTemp = inl(GPIO_PADR);
#endif

	/*
	 * Still haven't enabled the keyboard.  So anything in
	 * the rx fifo is garbage.  Time to take out the trash.
	 */
	while( inl(SSPSR) & SSPSR_RNE )
	{
		uiRegTemp = inl(SSPDR);
	}

	/*
	 * SPICR0_SPO - SCLKOUT Polarity
	 * SPICR0_SPH - SCLKOUT Phase
	 * Motorola format, 11 bit, one start, 8 data, one bit for
	 * parity, one stop bit.
	 */
	outl( (SSPCR0_FRF_MOTOROLA | SSPCR0_SPH | SSPCR0_SPO | SSPCR0_DSS_11BIT),
	      SSPCR0 );
	/*
	 * Configure the device as a slave, Clear FIFO overrun interrupts,
	 * enable interrupts and reset the device.
	 */
	outl( (SSPC1_MS | SSPC1_RIE | SSPC1_SOD), SSPCR1 );
	outl( 0, SSPIIR );
	outl( (SSPC1_MS | SSPC1_RIE | SSPC1_SOD | SSPC1_SSE), SSPCR1 );

	/*
	 * Configure EGPIO pins 12 and 14 as outputs because they are used
	 * as buffer enables for the SPI interface to the ps2 keyboard.
	 * Clear EGPIO pins 12 and 14, this will enable the SPI keyboard.
	 */
	uiRegTemp = inl(GPIO_PBDDR);
	outl( uiRegTemp | 0x50, GPIO_PBDDR );

	uiRegTemp = inl(GPIO_PBDR);
	outl( uiRegTemp & ~0x50, GPIO_PBDR );

	gSSPmode = SSP_MODE_PS2;
}

static void SetSSPtoI2S(void)
{
	unsigned int uiRegTemp;

	if( gSSPmode == SSP_MODE_I2S )
	{
		return;
	}
	
	/*
	 * Disable recieve interrupts.
	 */
	outl( (SSPC1_MS | SSPC1_SSE), SSPCR1 );

	/*
	 * Set GPIO pins 12 and 14, this will bring the clock line low
	 * which signals to the keyboard to buffer keystrokes.
	 * Note that EGPIO 14 is the clock line and EGPIO 12 is data line.
	 */
	uiRegTemp = inl(GPIO_PBDR);
	outl( 0x50 | uiRegTemp, GPIO_PBDR );

	/*
	 * It takes almost a millisecond for an partial keystrokes to come in.
	 * Delay to make sure we have completed all transactions.
	 */
	mdelay(1);

	/*
	 * Anything we just recieved is garbage.  Time to take out the trash.
	 */
	while( inl(SSPSR) & SSPSR_RNE )
	{
		uiRegTemp = inl(SSPDR);
	}
	
	/*
	 * Disable the SSP and disable interrupts
	 */
	outl( 0, SSPCR1 );

	/*
	 * Clock will be 14.7 MHz divided by 4.
	 */
	outl( 2, SSPCPSR );

	/*
	 * Configure EGPIO7 as an output and set it.  This selects
	 * I2S codec as the device on the SSP output instead of
	 * the serial flash on EDB9312.  On EDB9301 and EDB9315 it
	 * disables EEPROM but doesn't select anything.
	 */
	uiRegTemp = inl(GPIO_PADDR);
	outl( uiRegTemp | 0x80, GPIO_PADDR );

	uiRegTemp = inl(GPIO_PADR);
	outl( uiRegTemp | 0x80, GPIO_PADR );

    /*
     * Enable SFRM1 to I2S codec by clearing EGPIO8 (port B, bit 0).
     * The EDB9315 board needs this but is harmless on the EDB9312 board.
     */
#if defined(CONFIG_ARCH_EDB9312) || defined(CONFIG_ARCH_EDB9315)
    uiRegTemp = inl(GPIO_PBDDR) | 0x01;
    outl( uiRegTemp, GPIO_PBDDR );

    uiRegTemp = inl(GPIO_PBDR) & 0xfe;
    outl( uiRegTemp, GPIO_PBDR );
	
	uiRegTemp = inl(GPIO_PBDR);
#endif

    /*
     * Enable SFRM1 to I2S codec I2S by clearing EGPIO6 (port A, bit 6).
     * The EDB9301 board needs this
     */
#if defined(CONFIG_ARCH_EDB9301) || defined(CONFIG_ARCH_EDB9302)
	uiRegTemp = inl(GPIO_PADDR);
	outl( uiRegTemp | 0x40, GPIO_PADDR );

	uiRegTemp = inl(GPIO_PADR);
	outl( uiRegTemp & ~0x40, GPIO_PADR );

	uiRegTemp = inl(GPIO_PADR);
#endif

	/*
	 * Motorola format, 8 bit.
	 */
	outl( (SSPCR0_SPO | SSPCR0_SPH | SSPCR0_FRF_MOTOROLA | SSPCR0_DSS_8BIT),
	      SSPCR0 );

	/*
	 * Configure the device as master, reenable the device.
	 */
	outl( SSPC1_SSE, SSPCR1 );

	gSSPmode = SSP_MODE_I2S;

	udelay(10);
}

static void SetSSPtoFLASH(void)
{
	unsigned int uiRegTemp;

	if( gSSPmode == SSP_MODE_FLASH)
		return;

	/*
	 * Disable recieve interrupts.
	 */
	outl( (SSPC1_MS | SSPC1_SSE), SSPCR1 );

	/*
	 * Set GPIO pins 12 and 14, this will bring the clock line low
	 * which signals to the keyboard to buffer keystrokes.
	 * Note that EGPIO 14 is the clock line and EGPIO 12 is data line.
	 */
	outl( inl(GPIO_PBDR) | 0x50, GPIO_PBDR );

	/*
	 * It takes almost a millisecond for an partial keystrokes to come in.
	 * Delay to make sure we have completed all transactions.
	 */
	mdelay(1);

	/*
	 * Anything we just recieved is garbage.  Time to take out the trash.
	 */
	while( inl(SSPSR) & SSPSR_RNE )
		inl(SSPDR);
	
	/*
	 * Disable the SSP and disable interrupts
	 */
	outl( 0, SSPCR1 );

	/*
	 * Clock will be 14.7 MHz divided by 14.
	 */
	outl( 2, SSPCPSR );

	/*
	 * Configure EGPIO7 as an output and clear it.  This selects
	 * serial flash as the device on the SSP output instead of
	 * the I2S codec and is valid for EDB9301, EDB9312, and EDB9315.
	 */
	outl( inl(GPIO_PADDR) | 0x80, GPIO_PADDR );
	outl( inl(GPIO_PADR) & ~0x80, GPIO_PADR );

    /*
     * Disable SFRM1 to I2S codec by setting EGPIO8 (port B, bit 0).
     * The EDB9315 board needs this but is harmless on the EDB9312 board.
     */
#if defined(CONFIG_ARCH_EDB9312) || defined(CONFIG_ARCH_EDB9315)
    uiRegTemp = inl(GPIO_PBDDR) | 0x01;
    outl( uiRegTemp, GPIO_PBDDR );

    uiRegTemp = inl(GPIO_PBDR) | 0x01;
    outl( uiRegTemp, GPIO_PBDR );
	
	uiRegTemp = inl(GPIO_PBDR);
#endif

    /*
     * Disable SFRM1 to I2S codec I2S by setting EGPIO6 (port A, bit 6).
     * The EDB9301 board needs this
     */
#if defined(CONFIG_ARCH_EDB9301) || defined(CONFIG_ARCH_EDB9302)
	uiRegTemp = inl(GPIO_PADDR);
	outl( uiRegTemp | 0x40, GPIO_PADDR );

	uiRegTemp = inl(GPIO_PADR);
	outl( uiRegTemp | 0x40, GPIO_PADR );

	uiRegTemp = inl(GPIO_PBDR);
#endif

	/*
	 * Motorola format, 8 bit.
	 */
	outl( ((6 << SSPCR0_SCR_SHIFT) | SSPCR0_SPO | SSPCR0_SPH |
	       SSPCR0_FRF_MOTOROLA | SSPCR0_DSS_8BIT),
	      SSPCR0 );

	/*
	 * Configure the device as master, reenable the device.
	 */
	outl( SSPC1_SSE, SSPCR1 );

	gSSPmode = SSP_MODE_FLASH;

	udelay(10);
}

/*
 *  CheckHandle
 *
 *  If Handle is valid, returns 0.  Otherwise it returns -1.
 */
static int CheckHandle(int Handle)
{
	int iRet;

	if ((Handle != KeyboardHandle) &&
	    (Handle != I2SHandle) &&
	    (Handle != FlashHandle))
	{
		DPRINTK("OOPS! Invalid SSP Handle!\n");
		return(-1);
	}

	/*
	 * Get the SSP driver instance number from the handle.
	 */
	iRet = (((int)Handle & SSP_DEVICE_MASK) >> SSP_DEVICE_SHIFT);

	return iRet;
}

/*
 * ReadIntoBuffer
 *
 * Drains the SSP rx fifo into a buffer here.  If we overflow this buffer
 * then something's wrong.
 */
static int ReadIntoBuffer(void)
{
	unsigned int count, index, saved_count, uiRegTemp;
	
	count = 0;
	index = 0;


	if( gSSPmode != SSP_MODE_PS2 )
	{
		return 0;
	}
	
	/*
	 * This spinlock will prevent I2S from grabbing the SSP to do a
	 * write while we are using the SSP for PS2.
	 *
	 * There is a slight chance that we are in the beginning phase
	 * of doing an I2S write but the mode flag hadn't yet switched
	 * to I2S.  If that happens we will end up waiting on I2S to
	 * finish a write.  Not great.
	 */
	spin_lock(&ssp_spinlock);

	while( inl(SSPSR) & SSPSR_RNE)
	{
		/*
		 * Read in the value from the SPI controller into
		 * the partial key buffer.
		 */
		uiKeyBuffer[count] = inl(SSPDR);
		if (((uiKeyBuffer[count] & 0x3fc) != 0x3e0) &&
			((uiKeyBuffer[count] & 0x3fc) != 0x3c0))
		{
			/*
			 * Set GPIO pins 12 and 14, this will bring the clock line low
			 * which signals to the keyboard to buffer keystrokes.
			 * Note that EGPIO 14 is the clock line and EGPIO 12 is data line.
			 */
			uiRegTemp = inl(GPIO_PBDR);
			outl( 0x50 | uiRegTemp, GPIO_PBDR );

			outl( 0, SSPCR1 );
			outl( (SSPC1_MS | SSPC1_RIE | SSPC1_SSE), SSPCR1 );

			/*
			 * Clear EGPIO pins 12 and 14, this will enable the SPI keyboard.
			 */
			uiRegTemp = inl(GPIO_PBDR);
			outl( uiRegTemp & ~0x50, GPIO_PBDR );

			count++;
			break;
		}
		count++;
	}

	saved_count = count;
	index = 0;
	while (count)
	{
		//
		// No callback, dump data.
		//
		if (gKeyCallback)
		{
			gKeyCallback(uiKeyBuffer[index++]);
		}
		count--;
	}

	spin_unlock(&ssp_spinlock);

	return saved_count;
}

/*
 * SSP_Write_I2SCodec
 *
 */
static int SSP_Write_I2SCodec
(
	int Handle,
	unsigned int RegAddr,
	unsigned int RegValue
)
{
	SSPmodes_t saved_mode;

	DPRINTK("SSP_Write_I2SCodec\n");

	spin_lock(&ssp_spinlock);

	/*
	 * Save the SSP mode.  Switch to I2S mode if we're not
	 * already in I2S mode.
	 */
	saved_mode = gSSPmode;
	SetSSPtoI2S();

	/*
	 * Let TX fifo clear out.  Poll the Transmit Fifo Empty bit.
	 */
	while( !( inl(SSPSR) & SSPSR_TFE ) );
	
	/*
	 * Write the data out to the tx fifo.
	 */
	outl( 0x20, SSPDR ); /* chip address for CS4228 */
	outl( (RegAddr & 0xff), SSPDR );
	outl( (RegValue & 0xff), SSPDR );

	/*
	 * Let TX fifo clear out.  Poll the Transmit Fifo Empty bit.
	 */
	while( !( inl(SSPSR) & SSPSR_TFE ) );

	/*
	 * Delay to let stuff make it out of the SR before doing
	 * anthing else to the SSP.  It takes 6.8 uSec to do a
	 * I2S codec register write.
	 */
	udelay(10);

	/*
	 * If we were in PS2 mode, switch back to PS2 mode.
	 * If we weren't in PS2 mode, that means we didn't compile in
	 * the PS2 keyboard support, so no need to switch to PS2 mode.
	 */
	if( saved_mode == SSP_MODE_PS2 )
	{
		SetSSPtoPS2();
	}

	spin_unlock(&ssp_spinlock);

	/*
	 * Return success.
	 */
	return 0;
}
