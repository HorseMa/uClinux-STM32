/****************************************************************************/

/*
 *	ivpnss.h -- ioctl support for the iVPN hardware platform.
 *
 *	(C) Copyright 2004,  Greg Ungerer <gerg@snapgear.com>
 */

/****************************************************************************/
#ifndef IVPNSS_H
#define	IVPNSS_H 1
/****************************************************************************/

#include <linux/ioctl.h>

/*
 *	Ioctl to support enabling ports on the iVPN hardware.
 *	Any of the 2 ethernet ports or the WiFi port can be
 *	enabled or disabled using these.
 *
 *	Note: I wouldn't recommend disabling an interface while it was
 *	still operational, that would be bad :-)
 */
#define	ISET_ETH0	_IO('v',0x1)		/* Enable/Disabe eth0 */
#define	ISET_ETH1	_IO('v',0x2)		/* Enable/Disabe eth1 */
#define	ISET_WIFI0	_IO('v',0x3)		/* Enable/Disabe WiFi */

/****************************************************************************/
#endif /* IVPNSS_H */
/****************************************************************************/
