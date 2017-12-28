/****************************************************************************/

/*
 *	mcfgpt.h -- ColdFire internal GPT timer support defines.
 *
 *	(C) Copyright 2004, Greg Ungerer (gerg@snapgear.com)
 */

/****************************************************************************/
#ifndef	mcfgpt_h
#define	mcfgpt_h
/****************************************************************************/

#include <linux/config.h>

/*
 *	Get address specific defines for the 547x.
 */
#define	MCFPIT_BASE0		0x800	/* Base address of TIMER0 */
#define	MCFPIT_BASE1		0x810	/* Base address of TIMER1 */
#define	MCFPIT_BASE2		0x820	/* Base address of TIMER2 */
#define	MCFPIT_BASE3		0x830	/* Base address of TIMER3 */


/*
 *	Define the GPT timer register set addresses.
 */
struct mcfgpt {
	unsigned int	gms;			/* Enable and mode select */
	unsigned int	gcir;			/* Counter input */
	unsigned int	gpwm;			/* PWM configuration */
	unsigned int	gsr;			/* Status */
};

/*
 *	Bit definitions for the GMS Enable and mode select register.
 */
#define	MCFGPT_GMS_ICTANY	0x0000000	/* Input any transition */
#define	MCFGPT_GMS_ICTUP	0x0000400	/* Input rising edge */
#define	MCFGPT_GMS_ICTDOWN	0x0000800	/* Input falling edge */
#define	MCFGPT_GMS_ICTPULSE	0x0000c00	/* Input on pulse */
#define	MCFGPT_GMS_WDT		0x0001000	/* Watchdog enable */
#define	MCFGPT_GMS_CE		0x0008000	/* Counter enable */
#define	MCFGPT_GMS_SC		0x0020000	/* Stop/Continuous mode */
#define	MCFGPT_GMS_IEN		0x0080000	/* Interrupt enable */
#define	MCFGPT_GMS_MODE_OFF	0x0000000	/* Disable timer */
#define	MCFGPT_GMS_MODE_INPUT	0x2000000	/* Input capture mode */
#define	MCFGPT_GMS_MODE_OUTPUT	0x4000000	/* Output capture mode */
#define	MCFGPT_GMS_MODE_PWM	0x6000000	/* PWM mode */
#define	MCFGPT_GMS_MODE_GPIO	0x8000000	/* GPIO mode */

/****************************************************************************/
#endif	/* mcfgpt_h */
