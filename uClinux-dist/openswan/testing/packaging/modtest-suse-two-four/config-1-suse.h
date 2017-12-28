#ifndef _CONFIG_RH_I586_H_
/*
 * Copyright (C) 2002              Michael Richardson <mcr@freeswan.org>
 * 
 * This kernel module is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/lgpl.txt>.
 * 
 * This kernel module is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * RCSID $Id: config-1-suse.h,v 1.3 2005/08/26 13:55:12 ken Exp $
 */
#define	_CONFIG_RH_I586_H_	/* seen it, no need to see it again */

#define CONFIG_IPSEC 1

#ifndef CONFIG_IPSEC_AH
#define CONFIG_IPSEC_AH 1
#endif

#ifndef CONFIG_IPSEC_DEBUG 
#define CONFIG_IPSEC_DEBUG 1
#endif

#ifndef CONFIG_IPSEC_ESP
#define CONFIG_IPSEC_ESP 1
#endif

#ifndef CONFIG_IPSEC_IPCOMP
#define CONFIG_IPSEC_IPCOMP 1
#endif

#ifndef CONFIG_IPSEC_IPIP
#define CONFIG_IPSEC_IPIP 1
#endif

#ifndef CONFIG_IPSEC_AUTH_HMAC_MD5
#define CONFIG_IPSEC_AUTH_HMAC_MD5 1
#endif

#ifndef CONFIG_IPSEC_AUTH_HMAC_SHA1
#define CONFIG_IPSEC_AUTH_HMAC_SHA1 1
#endif 

#ifndef CONFIG_IPSEC_DYNDEV
#define CONFIG_IPSEC_DYNDEV 1
#endif

#ifndef CONFIG_IPSEC_ENC_3DES
#define CONFIG_IPSEC_ENC_3DES 1
#endif

#define SUSE_LINUX_2_4_19_IS_STUPID 1

#endif /* _CONFIG_RH_I586_H_ */

