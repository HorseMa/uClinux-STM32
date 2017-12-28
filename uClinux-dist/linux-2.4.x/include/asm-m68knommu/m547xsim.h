/****************************************************************************/

/*
 *	m547xsim.h -- ColdFire 547x System Integration Module support.
 *
 *	(C) Copyright 2004, Greg Ungerer <gerg@snapgear.com>
 */

/****************************************************************************/
#ifndef	m547xsim_h
#define	m547xsim_h
/****************************************************************************/

/*
 *	Define the 547x SIM register set addresses.
 */
#define	MCFSIM_IPRH		0x700		/* Interrupt pending 32-63 */
#define	MCFSIM_IPRL		0x704		/* Interrupt pending 1-31 */
#define	MCFSIM_IMRH		0x708		/* Interrupt mask 32-63 */
#define	MCFSIM_IMRL		0x70c		/* Interrupt mask 1-31 */
#define	MCFSIM_INTFRCH		0x710		/* Interrupt force 32-63 */
#define	MCFSIM_INTFRCL		0x714		/* Interrupt force 1-31 */
#define	MCFSIM_IRLR		0x718		/* */
#define	MCFSIM_IACKL		0x719		/* */

#define	MCFSIM_ICR0		0x740		/* Base ICR register */

/****************************************************************************/
#endif	/* m547xsim_h */
