/* $USAGI: atmipv6.h,v 1.4 2003/05/28 23:27:31 yoshfuji Exp $
 *
 * RFC2492 IPv6 over ATM
 *
 * Copyright (C)2001 USAGI/WIDE Project
 * 
 * Written by Kazuyoshi SERIZAWA based on atmclip.h - Classical IP over ATM
 * Written 1995-1998 by Werner Almesberger, EPFL LRC/ICA
 */

#ifndef LINUX_ATMIPV6_H
#define LINUX_ATMIPV6_H

#include <linux/sockios.h>
#include <linux/atmioc.h>


#define RFC1483LLC_LEN	8		/* LLC+OUI+PID = 8 */
#define RFC1626_MTU	9180		/* RFC1626 default MTU */

#define CLIP_DEFAULT_IDLETIMER 1200	/* 20 minutes, see RFC1755 */
#define CLIP_CHECK_INTERVAL	 10	/* check every ten seconds */

#define	SIOCMKP2PPVC		_IO('a',ATMIOC_IPV6)	/* create Point to Point PVC */

#define ATMIPV6_MKIPV6	_IO('a',ATMIOC_IPV6+1)	/* attach socket to IP */
#define ATMIPV6_SETP2P	_IO('a',ATMIOC_IPV6+2)	/* attach socket to P2P PVC */
#define ATMIPV6_ENCAP	_IO('a',ATMIOC_IPV6+3)	/* change encapsulation */

#ifdef __KERNEL__
extern const unsigned char llc_oui[6];
//TODO: extern const unsigned char llc_oui[6];
#endif

#endif
