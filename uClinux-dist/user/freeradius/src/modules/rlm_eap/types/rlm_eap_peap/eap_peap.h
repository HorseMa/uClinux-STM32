/*
 * eap_peap.h
 *
 * Version:     $Id: eap_peap.h,v 1.5.4.1 2006/02/06 16:23:55 nbk Exp $
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
 * Copyright 2003 Alan DeKok <aland@freeradius.org>
 */
#ifndef _EAP_PEAP_H
#define _EAP_PEAP_H

#include "eap_tls.h"

typedef struct peap_tunnel_t {
	VALUE_PAIR	*username;
	VALUE_PAIR	*state;
	VALUE_PAIR	*accept_vps;
	int		status;
	int		home_access_accept;
	int		default_eap_type;
	int		copy_request_to_tunnel;
	int		use_tunneled_reply;
	int		proxy_tunneled_request_as_eap;
} peap_tunnel_t;

#define PEAP_STATUS_START_PART2 0
#define PEAP_STATUS_SENT_TLV_SUCCESS 1
#define PEAP_STATUS_SENT_TLV_FAILURE 2

#define EAP_TLV_SUCCESS (1)
#define EAP_TLV_FAILURE (2)
#define EAP_TLV_ACK_RESULT (3)

#define PW_EAP_TLV 33

/*
 *	Process the PEAP portion of an EAP-PEAP request.
 */
int eappeap_process(EAP_HANDLER *handler, tls_session_t *tls_session);
#endif /* _EAP_PEAP_H */
