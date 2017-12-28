/*
 * rlm_eap_tls.h
 *
 * Version:     $Id: rlm_eap_tls.h,v 1.5.4.3 2006/04/28 18:18:58 aland Exp $
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Copyright 2001  hereUare Communications, Inc. <raghud@hereuare.com>
 * Copyright 2003  Alan DeKok <aland@freeradius.org>
 */
#ifndef _RLM_EAP_TLS_H
#define _RLM_EAP_TLS_H

#include "eap_tls.h"

#include "radiusd.h"
#include "modules.h"

/* configured values goes right here */
typedef struct eap_tls_conf {
	char		*private_key_password;
	char		*private_key_file;
	char		*certificate_file;
	char		*random_file;
	char		*ca_path;
	char		*ca_file;
	char		*dh_file;
	char		*rsa_file;
	int		rsa_key;
	int		dh_key;
	int		rsa_key_length;
	int		dh_key_length;
	int		verify_depth;
	int		file_type;
	int		include_length;

	/*
	 *	Always < 4096 (due to radius limit), 0 by default = 2048
	 */
	int		fragment_size;
	int		check_crl;
	char		*check_cert_cn;
	char		*cipher_list;
	char		*check_cert_issuer;
} EAP_TLS_CONF;

/* This structure gets stored in arg */
typedef struct _eap_tls_t {
	EAP_TLS_CONF 	*conf;
	SSL_CTX		*ctx;
} eap_tls_t;


#endif /* _RLM_EAP_TLS_H */
