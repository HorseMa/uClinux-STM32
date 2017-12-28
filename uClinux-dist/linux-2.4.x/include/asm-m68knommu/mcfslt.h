/****************************************************************************/

/*
 *	mcfslt.h -- ColdFire internal Slice (SLT) timer support defines.
 *
 *	(C) Copyright 2004, Greg Ungerer (gerg@snapgear.com)
 */

/****************************************************************************/
#ifndef	mcfslt_h
#define	mcfslt_h
/****************************************************************************/

/*
 *	Get address specific defines for the 547x.
 */
#define	MCFSLT_TIMER0		0x900	/* Base address of TIMER0 */
#define	MCFSLT_TIMER1		0x910	/* Base address of TIMER1 */


/*
 *	Define the SLT timer register set addresses.
 */
struct mcfslt {
	unsigned int	stcnt;			/* Terminal count */
	unsigned int	scr;			/* Control */
	unsigned int	scnt;			/* Current count */
	unsigned int	ssr;			/* Status */
};

/*
 *	Bit definitions for the SCR control register.
 */
#define	MCFSLT_SCR_RUN		0x04000000	/* Run mode (continuous) */
#define	MCFSLT_SCR_IEN		0x02000000	/* Interrupt enable */
#define	MCFSLT_SCR_TEN		0x01000000	/* Timer enable */

/*
 *	Bit definitions for the SSR status register.
 */
#define	MCFSLT_SSR_BE		0x02000000	/* Bus error condition */
#define	MCFSLT_SSR_TE		0x01000000	/* Timeout condition */

/****************************************************************************/
#endif	/* mcfslt_h */
