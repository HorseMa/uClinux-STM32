/*
 * rlm_eap.h    Local Header file.
 *
 * Version:     $Id: rlm_eap.h,v 1.13.4.1 2006/02/06 16:23:53 nbk Exp $
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
#ifndef _RLM_EAP_H
#define _RLM_EAP_H

#include <ltdl.h>
#include "eap.h"
#include "eap_types.h"

/*
 * Keep track of which sub modules we've loaded.
 */
typedef struct eap_types_t {
	const char	*typename;
	EAP_TYPE       	*type;
	lt_dlhandle     handle;
	CONF_SECTION	*cs;
	void		*type_data;
} EAP_TYPES;

/*
 * This structure contains eap's persistent data.
 * sessions = remembered sessions, in a tree for speed.
 * types = All supported EAP-Types
 * mutex = ensure only one thread is updating the sessions[] struct
 */
typedef struct rlm_eap_t {
	rbtree_t	*session_tree;
	EAP_HANDLER	*session_head, *session_tail;
	EAP_TYPES 	*types[PW_EAP_MAX_TYPES + 1];

	/*
	 *	Configuration items.
	 */
	int		timer_limit;
	char		*default_eap_type_name;
	int		default_eap_type;
	int		ignore_unknown_eap_types;
	int		cisco_accounting_username_bug;

#ifdef HAVE_PTHREAD_H
	pthread_mutex_t	session_mutex;
#endif
} rlm_eap_t;

/*
 *	For simplicity in the rest of the code.
 */
#ifndef HAVE_PTHREAD_H
/*
 *	This is easier than ifdef's throughout the code.
 */
#define pthread_mutex_init(_x, _y)
#define pthread_mutex_destroy(_x)
#define pthread_mutex_lock(_x)
#define pthread_mutex_unlock(_x)
#endif

/* function definitions */
/* EAP-Type */
int      	eaptype_load(EAP_TYPES **type, int eap_type, CONF_SECTION *cs);
int       	eaptype_select(rlm_eap_t *inst, EAP_HANDLER *h);
void		eaptype_free(EAP_TYPES *tl);

/* EAP */
int  		eap_start(rlm_eap_t *inst, REQUEST *request);
void 		eap_fail(EAP_HANDLER *handler);
void 		eap_success(EAP_HANDLER *handler);
int 		eap_compose(EAP_HANDLER *handler);
EAP_HANDLER 	*eap_handler(rlm_eap_t *inst, eap_packet_t **eap_msg, REQUEST *request);

/* Memory Management */
EAP_PACKET  	*eap_packet_alloc(void);
EAP_DS      	*eap_ds_alloc(void);
EAP_HANDLER 	*eap_handler_alloc(void);
void	    	eap_packet_free(EAP_PACKET **eap_packet);
void	    	eap_ds_free(EAP_DS **eap_ds);
void	    	eap_handler_free(EAP_HANDLER *handler);

int 	    	eaplist_add(rlm_eap_t *inst, EAP_HANDLER *handler);
EAP_HANDLER 	*eaplist_find(rlm_eap_t *inst, REQUEST *request,
			      eap_packet_t *eap_packet);
void		eaplist_free(rlm_eap_t *inst);

/* State */
void	    	generate_key(void);
VALUE_PAIR  	*generate_state(time_t timestamp);
int	    	verify_state(VALUE_PAIR *state, time_t timestamp);

#endif /*_RLM_EAP_H*/
