/*
 *  linux/drivers/char/ep93xx_ts.c
 *
 *  Copyright (C) 2003-2004 Cirrus Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/interrupt.h>
#include <linux/timer.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>


//
// To customize for a new touchscreen, there are various macros that
// have to be set.  If you allow UART_HACK_DEBUG to be defined, you
// will get real time ts data scrolling up your serial terminal
// screen that will help you empirically determine good values for these.  
//

//
// These are used as trigger levels to know when we have pen up/down
//
// The rules:
// 1.  TS_HEAVY_INV_PRESSURE < TS_LIGHT_INV_PRESSURE because these
//    are Inverse pressure.  
// 2.  Any touch lighter than TS_LIGHT_INV_PRESSURE is a pen up.
// 3.  Any touch heavier than TS_HEAVY_INV_PRESSURE is a pen down.
//
#define   TS_HEAVY_INV_PRESSURE 0xFE0 //C00
#define   TS_LIGHT_INV_PRESSURE 0xFFF //e00

//
// If the x, y, or inverse pressure changes more than these values
// between two succeeding points, the point is not reported.
//
#define   TS_MAX_VALID_XY_CHANGE 0x300
#define   TS_MAX_VALID_PRESSURE_CHANGE  0x100

//
// This is the minimum Z1 Value that is valid.
//
#define     MIN_Z1_VALUE                    0x50

//
// Settling delay for taking each ADC measurement.  Increase this
// if ts is jittery.
//
#define EP93XX_TS_ADC_DELAY_USEC 2000

//
// Delay between TS points.
//
#define EP93XX_TS_PER_POINT_DELAY_USEC 10000

//-----------------------------------------------------------------------------
// Debug messaging thru the UARTs
//-----------------------------------------------------------------------------
/*
 *  Hello there!  Are you trying to get this driver to work with a new
 *  touschscreen?  Turn this on and you will get useful info coming
 *  out of your serial port.
 */
/* #define PRINT_CALIBRATION_FACTORS */
#ifdef PRINT_CALIBRATION_FACTORS
#define UART_HACK_DEBUG 1
int iMaxX=0, iMaxY=0, iMinX = 0xfff, iMinY = 0xfff;
#endif

/*
 * For debugging, let's spew messages out serial port 1 or 3 at 57,600 baud.
 */
/* #define UART_HACK_DEBUG 1 */
#ifdef UART_HACK_DEBUG
static char szBuf[256];
void UARTWriteString(char * msg);
#define DPRINTK( x... )   \
    sprintf( szBuf, ##x ); \
    UARTWriteString( szBuf );
#else
#define DPRINTK( x... )
#endif

//-----------------------------------------------------------------------------
// A few more macros...
//-----------------------------------------------------------------------------
#define TSSETUP_DEFAULT  ( TSSETUP_NSMP_32 | TSSETUP_DEV_64 |  \
                           ((128<<TSSETUP_SDLY_SHIFT) & TSSETUP_SDLY_MASK) | \
                           ((128<<TSSETUP_DLY_SHIFT)  & TSSETUP_DLY_MASK) )

#define TSSETUP2_DEFAULT (TSSETUP2_NSIGND)

//
// For now, we use one of the minor numbers from the local/experimental
// range.
//
#define EP93XX_TS_MINOR 240

//-----------------------------------------------------------------------------
// Static Declarations
//-----------------------------------------------------------------------------
static unsigned int   guiLastX, guiLastY;
static unsigned int   guiLastInvPressure;


static int currentX, currentY, currentButton;
static int bFreshTouchData;
static int bCurrentPenDown;


static DECLARE_WAIT_QUEUE_HEAD(queue);
static DECLARE_MUTEX(open_sem);
static spinlock_t event_buffer_lock = SPIN_LOCK_UNLOCKED;
static struct fasync_struct *fasync;

//-----------------------------------------------------------------------------
// Typedef Declarations
//-----------------------------------------------------------------------------
typedef enum {
	TS_MODE_UN_INITIALIZED,
	TS_MODE_HARDWARE_SCAN,
	TS_MODE_SOFT_SCAN
} ts_mode_t;

static ts_mode_t      gScanningMode;

typedef enum{
    TS_STATE_STOPPED = 0,
    TS_STATE_Z1,
    TS_STATE_Z2,
    TS_STATE_Y,
    TS_STATE_X,
    TS_STATE_DONE
} ts_states_t;

typedef struct 
{
    unsigned int   uiX;
    unsigned int   uiY;
    unsigned int   uiZ1;
    unsigned int   uiZ2;
    ts_states_t    state;
} ts_struct_t;

static ts_struct_t sTouch;

/*
 * From the spec, here's how to set up the touch screen's switch registers.
 */
typedef struct 
{
    unsigned int uiDetect;
    unsigned int uiDischarge;
    unsigned int uiXSample;
    unsigned int uiYSample;
    unsigned int uiSwitchZ1;
    unsigned int uiSwitchZ2;
}SwitchStructType;

//
// Here's the switch settings for a 4-wire touchscreen.  See the spec
// for how to handle a 4, 7, or 8-wire.
//
const static SwitchStructType sSwitchSettings = 
/*     s28en=0
 *   TSDetect    TSDischarge  TSXSample  TSYSample    SwitchZ1   SwitchZ2
 */
    {0x00403604, 0x0007fe04, 0x00081604, 0x00104601, 0x00101601, 0x00101608};   


//-----------------------------------------------------------------------------
// Function Declarations
//-----------------------------------------------------------------------------
static void ep93xx_ts_set_direct( unsigned int uiADCSwitch );
static void ep93xx_ts_isr(int irq, void *dev_id, struct pt_regs *regs);
static void ep93xx_timer2_isr(int irq, void *dev_id, struct pt_regs *regs);
static void ee93xx_ts_evt_add( int buttons, int dX, int dY );
static ssize_t ep93xx_ts_read(struct file *filp, char *buf, 
        size_t count, loff_t *l);
static unsigned int ep93xx_ts_poll(struct file *filp, poll_table *wait);
static int ep93xx_ts_open(struct inode *inode, struct file *filp);
static int ep93xx_ts_fasync(int fd, struct file *filp, int on);
static int ep93xx_ts_release(struct inode *inode, struct file *filp);
static ssize_t ep93xx_ts_write(struct file *file, const char *buffer, 
                size_t count, loff_t *ppos);
static void ep93xx_hw_setup(void);
static void ep93xx_hw_shutdown(void);
int __init ep93xx_ts_init(void);
void __exit ep93xx_ts_exit(void);
static unsigned int CalculateInvPressure( void );
static unsigned int ADCGetData( unsigned int uiSamples, unsigned int uiMaxDiff);
static void TS_Soft_Scan_Mode(void);
static void TS_Hardware_Scan_Mode(void);
static void ProcessPointData(void);
static void Set_Timer2_uSec( unsigned int Delay_mSec );
static void Stop_Timer2(void);


//-----------------------------------------------------------------------------
//  Debug stuff...
//-----------------------------------------------------------------------------

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
	unsigned int uiTemp;

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

/*
 *  ep93xx_ts_isr
 */
static void ep93xx_ts_isr(int irq, void *dev_id, struct pt_regs *regs)
{
    DPRINTK("isr\n");

    // 
    // Note that we don't clear the interrupt here.  The interrupt
    // gets cleared in TS_Soft_Scan_Mode when the TS ENABLE
    // bit is cleared.
    //

    //
    // Set the ts to manual polling mode and schedule a callback.
    // That way we can return from the isr in a reasonable amount of
    // time and process the touch in the callback after a brief delay.
    //
    TS_Soft_Scan_Mode();
    
}

/*
 * Save the current ts 'event' in an atomic fashion.
 */
static void ee93xx_ts_evt_add( int buttons, int iX, int iY )
{
#ifdef PRINT_CALIBRATION_FACTORS
    if( iX > iMaxX ) iMaxX = iX;
    if( iX < iMinX ) iMinX = iX;
    if( iY > iMaxY ) iMaxY = iY;
    if( iY < iMinY ) iMinY = iY;
#endif

    //DPRINTK("cb\n");
    /*
     * Note the event, but use spinlocks to keep it from getting
     * halfway read if we get interrupted.
     */  
    spin_lock(&event_buffer_lock);

    currentButton = buttons;
    currentX = iX;
    currentY = iY;
    bFreshTouchData = 1;
    
    spin_unlock(&event_buffer_lock);

    kill_fasync(&fasync, SIGIO, POLL_IN);
    wake_up_interruptible(&queue);
}


static ssize_t ep93xx_ts_read(struct file *filp, char *buf, size_t count, loff_t *l)
{

    unsigned short data[3];

#ifdef PRINT_CALIBRATION_FACTORS
    static int lala=0;
    if( bFreshTouchData && (lala++ > 9) )
    {
        DPRINTK("%4d, %4d - range [%4d to %4d],[%4d to %4d]\n",
            currentX, currentY, iMinX, iMaxX, iMinY, iMaxY );
        lala = 0;
    }
#endif

    if( bFreshTouchData && (count >= sizeof(data)) )
    {
        spin_lock_irq(&event_buffer_lock);
        bFreshTouchData = 0;
        data[0] = currentX;
        data[1] = currentY;
        data[2] = currentButton;
        spin_unlock_irq(&event_buffer_lock);

        if (copy_to_user(buf, data, sizeof data))
            return -EFAULT;

        count -= sizeof(data);

        /* return the # of bytes that got read */
        return( sizeof(data) );
    }

    return -EINVAL;
}

static unsigned int	ep93xx_ts_poll(struct file *filp, poll_table *wait)
{
    poll_wait(filp, &queue, wait);

    if( bFreshTouchData )
    {
        return POLLIN | POLLRDNORM;
    }
    
    return 0;
}

static int ep93xx_ts_open(struct inode *inode, struct file *filp)
{
    if( down_trylock(&open_sem) )
    {
        return -EBUSY;
    }

    ep93xx_hw_setup();

    return 0;
}

/*
 * Asynchronous I/O support.
 */
static int ep93xx_ts_fasync(int fd, struct file *filp, int on)
{
    int retval;

    retval = fasync_helper(fd, filp, on, &fasync);
    if (retval < 0)
    {
        return retval;
    }
    
    return 0;
}

static int ep93xx_ts_release(struct inode *inode, struct file *filp)
{
    Stop_Timer2();

    /*
     * Call our async I/O support to request that this file 
     * cease to be used for async I/O.
     */
    ep93xx_ts_fasync(-1, filp, 0);

    ep93xx_hw_shutdown();
    
    up(&open_sem);
    
    return 0;
}

static ssize_t ep93xx_ts_write(struct file *file, const char *buffer, size_t count,
               loff_t *ppos)
{
    return -EINVAL;
}

static struct file_operations ep93xx_ts_fops = {
    owner:      THIS_MODULE,
    read:       ep93xx_ts_read,
    write:      ep93xx_ts_write,
    poll:       ep93xx_ts_poll,
    open:       ep93xx_ts_open,
    release:    ep93xx_ts_release,
    fasync:     ep93xx_ts_fasync,
};

static struct miscdevice ep93xx_ts_miscdev = 
{
        EP93XX_TS_MINOR,
        "ep93xx_ts",
        &ep93xx_ts_fops
};

void ep93xx_hw_setup(void)
{
    unsigned int uiKTDIV, uiTSXYMaxMin;
    
    /*
     * Set the TSEN bit in KTDIV so that we are enabling the clock
     * for the touchscreen.
     */    
    uiKTDIV = inl(SYSCON_KTDIV);
    uiKTDIV |= SYSCON_KTDIV_TSEN;
    SysconSetLocked( SYSCON_KTDIV, uiKTDIV );    

    //
    // Program the TSSetup and TSSetup2 registers.
    //
	outl( TSSETUP_DEFAULT, TSSetup );
	outl( TSSETUP2_DEFAULT, TSSetup2 );

    //
    // Set the the touch settings. 
    //
	outl( 0xaa, TSSWLock );
	outl( sSwitchSettings.uiDischarge, TSDirect );

	outl( 0xaa, TSSWLock );
	outl( sSwitchSettings.uiDischarge, TSDischarge );

	outl( 0xaa, TSSWLock );
	outl( sSwitchSettings.uiSwitchZ1, TSXSample );

	outl( 0xaa, TSSWLock );
	outl( sSwitchSettings.uiSwitchZ2, TSYSample );

	outl( 0xaa, TSSWLock );
	outl( sSwitchSettings.uiDetect, TSDetect );

    //
    // X,YMin set to 0x40 = have to drag that many pixels for a new irq.
    // X,YMax set to 0x40 = 1024 pixels is the maximum movement within the
    // time scan limit.
    //
	uiTSXYMaxMin =  (50   << TSMAXMIN_XMIN_SHIFT) & TSMAXMIN_XMIN_MASK;
	uiTSXYMaxMin |=	(50   << TSMAXMIN_YMIN_SHIFT) & TSMAXMIN_YMIN_MASK;
	uiTSXYMaxMin |=	(0xff << TSMAXMIN_XMAX_SHIFT) & TSMAXMIN_XMAX_MASK;
	uiTSXYMaxMin |=	(0xff << TSMAXMIN_YMAX_SHIFT) & TSMAXMIN_YMAX_MASK;
	outl( uiTSXYMaxMin, TSXYMaxMin );
	
	bCurrentPenDown = 0;
    bFreshTouchData = 0;
	guiLastX = 0;
	guiLastY = 0;
	guiLastInvPressure = 0xffffff;
	
    //
    // Enable the touch screen scanning engine.
    //
    TS_Hardware_Scan_Mode();

}

/*
 * ep93xx_hw_shutdown
 *
 */
static void
ep93xx_hw_shutdown(void)
{
    unsigned int uiKTDIV;
    
    DPRINTK("ep93xx_hw_shutdown\n");
	
    sTouch.state = TS_STATE_STOPPED;
	Stop_Timer2();

    /*
     * Disable the scanning engine.
     */
	outl( 0, TSSetup );
	outl( 0, TSSetup2 );

    /*
     * Clear the TSEN bit in KTDIV so that we are disabling the clock
     * for the touchscreen.
     */    
    uiKTDIV = inl(SYSCON_KTDIV);
    uiKTDIV &= ~SYSCON_KTDIV_TSEN;
    SysconSetLocked( SYSCON_KTDIV, uiKTDIV );    

} /* ep93xx_hw_shutdown */

static void ep93xx_timer2_isr(int irq, void *dev_id, struct pt_regs *regs)
{
    DPRINTK("%d", (int)sTouch.state );

    switch( sTouch.state )
    {
        case TS_STATE_STOPPED:
			TS_Hardware_Scan_Mode();
            break;
            
        //
        // Get the Z1 value for pressure measurement and set up
		// the switch register for getting the Z2 measurement.
        //
        case TS_STATE_Z1:
		    Set_Timer2_uSec( EP93XX_TS_ADC_DELAY_USEC );
            sTouch.uiZ1 = ADCGetData( 2, 200 );
            ep93xx_ts_set_direct( sSwitchSettings.uiSwitchZ2 );
            sTouch.state = TS_STATE_Z2;
            break;
        
        //
        // Get the Z2 value for pressure measurement and set up
		// the switch register for getting the Y measurement.
        //
        case TS_STATE_Z2:
            sTouch.uiZ2 = ADCGetData( 2, 200 );
            ep93xx_ts_set_direct( sSwitchSettings.uiYSample );
            sTouch.state = TS_STATE_Y;
			break;
		
		//
        // Get the Y value and set up the switch register for 
		// getting the X measurement.
        //
        case TS_STATE_Y:
            sTouch.uiY = ADCGetData( 4, 20 );
            ep93xx_ts_set_direct( sSwitchSettings.uiXSample );
            sTouch.state = TS_STATE_X;
            break;
        
        //
        // Read the X value.  This is the last of the 4 adc values
        // we need so we continue on to process the data.
        //
        case TS_STATE_X:
            Stop_Timer2();
            
            sTouch.uiX = ADCGetData( 4, 20 );
            
			outl( 0xaa, TSSWLock );
			outl( sSwitchSettings.uiDischarge, TSDirect );
            
            sTouch.state = TS_STATE_DONE;
        
            /*
             * Process this set of ADC readings.
             */
            ProcessPointData();
            
            break;


        //
		// Shouldn't get here.  But if we do, we can recover...
		//
		case TS_STATE_DONE:
			TS_Hardware_Scan_Mode();
            break;
    } 

    //
    // Clear the timer2 interrupt.
    //
	outl( 1, TIMER2CLEAR );

}

/*---------------------------------------------------------------------
 * ProcessPointData
 *
 * This routine processes the ADC data into usable point data and then
 * puts the driver into hw or sw scanning mode before returning.
 *
 * We calculate inverse pressure (lower number = more pressure) then
 * do a hystheresis with the two pressure values 'light' and 'heavy'.
 *
 * If we are above the light, we have pen up.
 * If we are below the heavy we have pen down.
 * As long as the pressure stays below the light, pen stays down.
 * When we get above the light again, pen goes back up.
 *
 */
static void ProcessPointData(void)
{
    int  bValidPoint = 0;
    unsigned int   uiXDiff, uiYDiff, uiInvPressureDiff;
    unsigned int   uiInvPressure;

    //
    // Calculate the current pressure.
    //
    uiInvPressure = CalculateInvPressure();

    DPRINTK(" X=0x%x, Y=0x%x, Z1=0x%x, Z2=0x%x, InvPressure=0x%x",
            sTouch.uiX, sTouch.uiY, sTouch.uiZ1, sTouch.uiZ2, uiInvPressure ); 



	//
	// If pen pressure is so light that it is greater than the 'max' setting
	// then we consider this to be a pen up.
	//
    if( uiInvPressure >= TS_LIGHT_INV_PRESSURE )
	{
		DPRINTK(" -- up \n");
		bCurrentPenDown = 0;
        ee93xx_ts_evt_add( 0, guiLastX, guiLastY );
		TS_Hardware_Scan_Mode();
		return;
	}

    //
	// Hystheresis:
    // If the pen pressure is hard enough to be less than the 'min' OR
	// the pen is already down and is still less than the 'max'...
	//
    if( (uiInvPressure < TS_HEAVY_INV_PRESSURE) ||
        ( bCurrentPenDown && (uiInvPressure < TS_LIGHT_INV_PRESSURE) )  )
    {
        if( bCurrentPenDown )
        {
            //
            // If pen was previously down, check the difference between
            // the last sample and this one... if the difference between 
            // samples is too great, ignore the sample.
            //
            uiXDiff = abs(guiLastX - sTouch.uiX);
            uiYDiff = abs(guiLastY - sTouch.uiY);
			uiInvPressureDiff = abs(guiLastInvPressure - uiInvPressure);
			
            if( (uiXDiff < TS_MAX_VALID_XY_CHANGE) && 
			    (uiYDiff < TS_MAX_VALID_XY_CHANGE) &&
				(uiInvPressureDiff < TS_MAX_VALID_PRESSURE_CHANGE) )
            {
				DPRINTK(" -- valid(two) \n");
                bValidPoint = 1;
            }
			else
			{
				DPRINTK(" -- INvalid(two) \n");
			}
        }
        else
        {
			DPRINTK(" -- valid \n");
            bValidPoint = 1;
        }
        
        /*
         * If either the pen was put down or dragged make a note of it.
         */
        if( bValidPoint )
        {
	        guiLastX = sTouch.uiX;
	        guiLastY = sTouch.uiY;
			guiLastInvPressure = uiInvPressure;
			bCurrentPenDown = 1;
            ee93xx_ts_evt_add( 1, sTouch.uiX, sTouch.uiY );
        }

        TS_Soft_Scan_Mode();
		return;
    }

	DPRINTK(" -- fallout \n");
	TS_Hardware_Scan_Mode();
}

static void ep93xx_ts_set_direct( unsigned int uiADCSwitch )
{
    unsigned int uiResult;
    
    //
    // Set the switch settings in the direct register.
    //
	outl( 0xaa, TSSWLock );
	outl( uiADCSwitch, TSDirect );

    //
    // Read and throw away the first sample.
    //
    do {
        uiResult = inl(TSXYResult);
    } while( !(uiResult & TSXYRESULT_SDR) );
    
}

static unsigned int ADCGetData
( 
    unsigned int uiSamples, 
    unsigned int uiMaxDiff 
)
{
    unsigned int   uiResult, uiValue, uiCount, uiLowest, uiHighest, uiSum, uiAve;

    do
    {
        //
        //Initialize our values.
        //
        uiLowest        = 0xfffffff;
        uiHighest       = 0;
        uiSum           = 0;
        
        for( uiCount = 0 ; uiCount < uiSamples ; uiCount++ )
        {
            //
            // Read the touch screen four more times and average.
            //
            do {
		        uiResult = inl(TSXYResult);
            } while( !(uiResult & TSXYRESULT_SDR) );
            
            uiValue = (uiResult & TSXYRESULT_AD_MASK) >> TSXYRESULT_AD_SHIFT;
            uiValue = ((uiValue >> 4) + ((1 + TSXYRESULT_X_MASK)>>1))  & TSXYRESULT_X_MASK; 

            //
            // Add up the values.
            //
            uiSum += uiValue;

            //
            // Get the lowest and highest values.
            //
            if( uiValue < uiLowest )
            {
                uiLowest = uiValue;
            }
            if( uiValue > uiHighest )
            {
                uiHighest = uiValue;
            }
        }

    } while( (uiHighest - uiLowest) > uiMaxDiff );

    //
    // Calculate the Average value.
    //
    uiAve = uiSum / uiSamples;

    return uiAve;    
}

//****************************************************************************
// CalculateInvPressure
//****************************************************************************
// Is the Touch Valid.  Touch is not valid if the X or Y value is not 
// in range and the pressure is not  enough.
// 
// Touch resistance can be measured by the following formula:
//
//          Rx * X *     Z2
// Rtouch = --------- * (-- - 1)
//           4096        Z1
//
// This is simplified in the ration of Rtouch to Rx.  The lower the value, the
// higher the pressure.
//
//                     Z2
// InvPressure =  X * (-- - 1)
//                     Z1
//
static unsigned int CalculateInvPressure(void)
{
    unsigned int   uiInvPressure;

    //
    // Check to see if the point is valid.
    //
    if( sTouch.uiZ1 < MIN_Z1_VALUE )
    {
        uiInvPressure = 0x10000;
    }

    //
    // Can omit the pressure calculation if you need to get rid of the division.
    //
    else
    {
        uiInvPressure = ((sTouch.uiX * sTouch.uiZ2) / sTouch.uiZ1) - sTouch.uiX;
    }    

    return uiInvPressure;
}



//****************************************************************************
// TS_Hardware_Scan_Mode
//****************************************************************************
// Enables the ep93xx ts scanning engine so that when the pen goes down
// we will get an interrupt.
// 
//
static void TS_Hardware_Scan_Mode(void)
{
    unsigned int   uiDevCfg;

    DPRINTK("S\n");

    //
	// Disable the soft scanning engine.
	//
	sTouch.state = TS_STATE_STOPPED;
    Stop_Timer2();
    
    //
    // Clear the TIN (Touchscreen INactive) bit so we can go to
    // automatic scanning mode.
    //
    uiDevCfg = inl( SYSCON_DEVCFG );
    SysconSetLocked( SYSCON_DEVCFG, (uiDevCfg & ~SYSCON_DEVCFG_TIN) );    

    //
    // Enable the touch screen scanning state machine by setting
    // the ENABLE bit.
    //
	outl( (TSSETUP_DEFAULT | TSSETUP_ENABLE), TSSetup );

    //
    // Set the flag to show that we are in interrupt mode.
    //
    gScanningMode = TS_MODE_HARDWARE_SCAN;

    //
    // Initialize TSSetup2 register.
    //
	outl( TSSETUP2_DEFAULT, TSSetup2 );

}

//****************************************************************************
// TS_Soft_Scan_Mode
//****************************************************************************
// Sets the touch screen to manual polling mode.
// 
//
static void TS_Soft_Scan_Mode(void)
{
    unsigned int   uiDevCfg;

    DPRINTK("M\n");

    if( gScanningMode != TS_MODE_SOFT_SCAN )
    {
        //
        // Disable the touch screen scanning state machine by clearing
        // the ENABLE bit.
        //
		outl( TSSETUP_DEFAULT, TSSetup );

        //
        // Set the TIN bit so we can do manual touchscreen polling.
        //
        uiDevCfg = inl( SYSCON_DEVCFG );
        SysconSetLocked( SYSCON_DEVCFG, (uiDevCfg | SYSCON_DEVCFG_TIN) );    
    }

    //
	// Set the switch register up for the first ADC reading
	//
	ep93xx_ts_set_direct( sSwitchSettings.uiSwitchZ1 );
	
	//
	// Initialize our software state machine to know which ADC
	// reading to take
	//
    sTouch.state = TS_STATE_Z1;
	
	//
	// Set the timer so after a mSec or two settling delay it will 
	// take the first ADC reading.
	// 
    Set_Timer2_uSec( EP93XX_TS_PER_POINT_DELAY_USEC );
    
    //
    // Note that we are in sw scanning mode not hw scanning mode.
    //
    gScanningMode = TS_MODE_SOFT_SCAN;

}

static void Set_Timer2_uSec( unsigned int uiDelay_uSec )
{
    unsigned int uiClockTicks;

    /*
     * Stop timer 2
     */
	outl( 0, TIMER2CONTROL );

    uiClockTicks = ((uiDelay_uSec * 508) + 999) / 1000;
    outl( uiClockTicks, TIMER2LOAD );
    outl( uiClockTicks, TIMER2VALUE );

    /*
     * Set up Timer 2 for 508 kHz clock and periodic mode.
     */ 
    outl( 0xC8, TIMER2CONTROL );

}

static void Stop_Timer2(void)
{
    outl( 0, TIMER2CONTROL );
}

/*
 * Initialization and exit routines
 */
int __init
ep93xx_ts_init(void)
{
    int retval;

    DPRINTK("ep93xx_ts_init\n");

    retval = request_irq( IRQ_TOUCH, ep93xx_ts_isr,	SA_INTERRUPT, "ep93xx_ts", 0);
    if( retval )
    {
        printk(KERN_WARNING "ep93xx_ts: failed to get touchscreen IRQ\n");
        return retval;
    }

    retval = request_irq( IRQ_TIMER2, ep93xx_timer2_isr,
                        SA_INTERRUPT, "ep93xx_timer2", 0);
    if( retval )
    {
        printk(KERN_WARNING "ep93xx_ts: failed to get timer2 IRQ\n");
        return retval;
    }

    misc_register(&ep93xx_ts_miscdev);

    sTouch.state = TS_STATE_STOPPED;
	gScanningMode = TS_MODE_UN_INITIALIZED;
    
    printk(KERN_NOTICE "EP93xx touchscreen driver configured for 4-wire operation\n");

    return 0;
}

void __exit
ep93xx_ts_exit(void)
{
    DPRINTK("ep93xx_ts_exit\n");
    
    Stop_Timer2();

    free_irq(IRQ_TOUCH, 0);
    free_irq(IRQ_TIMER2, 0);
    
    misc_deregister(&ep93xx_ts_miscdev);
}

module_init(ep93xx_ts_init);
module_exit(ep93xx_ts_exit);

MODULE_DESCRIPTION("Cirrus EP93xx touchscreen driver");
MODULE_SUPPORTED_DEVICE("touchscreen/ep93xx");
