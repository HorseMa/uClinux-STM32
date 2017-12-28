/*
 * rlm_eap_gtc.c    Handles that are called from eap
 *
 * Version:     $Id: rlm_eap_gtc.c,v 1.3 2004/02/26 19:04:30 aland Exp $
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
 * Copyright 2003  The FreeRADIUS server project
 */

#include "autoconf.h"

#include <stdio.h>
#include <stdlib.h>

#include "eap.h"

#include <rad_assert.h>

/*
 *	EAP-GTC is just ASCII data carried inside of the EAP session.
 *	The length of the data is indicated by the encapsulating EAP
 *	protocol.
 */
typedef struct rlm_eap_gtc_t {
	const char	*challenge;
	const char	*auth_type_name;
	int		auth_type;
} rlm_eap_gtc_t;

static CONF_PARSER module_config[] = {
	{ "challenge", PW_TYPE_STRING_PTR,
	  offsetof(rlm_eap_gtc_t, challenge), NULL, "Password: " },

	{ "auth_type", PW_TYPE_STRING_PTR,
	  offsetof(rlm_eap_gtc_t, auth_type_name), NULL, "PAP" },

 	{ NULL, -1, 0, NULL, NULL }           /* end the list */
};


/*
 *	Detach the module.
 */
static int gtc_detach(void *arg)
{
	rlm_eap_gtc_t *inst = (rlm_eap_gtc_t *) arg;

	if (inst->challenge) free(inst->challenge);
	if (inst->auth_type_name) free(inst->auth_type_name);

	free(inst);

	return 0;
}

/*
 *	Attach the module.
 */
static int gtc_attach(CONF_SECTION *cs, void **instance)
{
	rlm_eap_gtc_t	*inst;
	DICT_VALUE	*dval;

	inst = malloc(sizeof(*inst));
	if (!inst) {
		radlog(L_ERR, "rlm_eap_gtc: out of memory");
		return -1;
	}
	memset(inst, 0, sizeof(*inst));

	/*
	 *	Parse the configuration attributes.
	 */
	if (cf_section_parse(cs, inst, module_config) < 0) {
		gtc_detach(inst);
		return -1;
	}

	dval = dict_valbyname(PW_AUTH_TYPE, inst->auth_type_name);
	if (!dval) {
		radlog(L_ERR, "rlm_eap_gtc: Unknown Auth-Type %s",
		       inst->auth_type_name);
		gtc_detach(inst);
		return -1;
	}

	inst->auth_type = dval->value;

	*instance = inst;

	return 0;
}

/*
 *	Initiate the EAP-GTC session by sending a challenge to the peer.
 */
static int gtc_initiate(void *type_data, EAP_HANDLER *handler)
{
	int length;
	EAP_DS *eap_ds = handler->eap_ds;
	rlm_eap_gtc_t *inst = (rlm_eap_gtc_t *) type_data;

	/*
	 *	FIXME: call radius_xlat on the challenge
	 */
	length = strlen(inst->challenge);

	/*
	 *	We're sending a request...
	 */
	eap_ds->request->code = PW_EAP_REQUEST;

	eap_ds->request->type.data = malloc(length);
	if (eap_ds->request->type.data == NULL) {
		radlog(L_ERR, "rlm_eap_gtc: out of memory");
		return 0;
	}

	memcpy(eap_ds->request->type.data, inst->challenge, length);
	eap_ds->request->type.length = length;

	/*
	 *	We don't need to authorize the user at this point.
	 *
	 *	We also don't need to keep the challenge, as it's
	 *	stored in 'handler->eap_ds', which will be given back
	 *	to us...
	 */
	handler->stage = AUTHENTICATE;

	return 1;
}


/*
 *	Authenticate a previously sent challenge.
 */
static int gtc_authenticate(void *type_data, EAP_HANDLER *handler)
{
	VALUE_PAIR *vp;
	EAP_DS *eap_ds = handler->eap_ds;
	rlm_eap_gtc_t *inst = (rlm_eap_gtc_t *) type_data;

	/*
	 *	Get the User-Password for this user.
	 */
	rad_assert(handler->request != NULL);
	rad_assert(handler->stage == AUTHENTICATE);

	/*
	 *	Sanity check the response.  We need at least one byte
	 *	of data.
	 */
	if (eap_ds->response->length <= 4) {
		radlog(L_ERR, "rlm_eap_gtc: corrupted data");
		eap_ds->request->code = PW_EAP_FAILURE;
		return 0;
	}

#if 0
	if (debug_flag > 2) {
		int i;

		for (i = 0; i < eap_ds->response->length - 4; i++) {
			if ((i & 0x0f) == 0) printf("%d: ", i);

			printf("%02x ", eap_ds->response->type.data[i]);

			if ((i & 0x0f) == 0x0f) printf("\n");
		}
	}
#endif

	/*
	 *	Handle passwords here.
	 */
	if (inst->auth_type == PW_AUTHTYPE_LOCAL) {
		/*
		 *	For now, do clear-text password authentication.
		 */
		vp = pairfind(handler->request->config_items, PW_PASSWORD);
		if (!vp) {
			DEBUG2("  rlm_eap_gtc: ERROR: Clear-test User-Password is required for authentication.");
			eap_ds->request->code = PW_EAP_FAILURE;
			return 0;
		}

		if (eap_ds->response->type.length != vp->length) {
			DEBUG2("  rlm_eap_gtc: ERROR: Passwords are of different length. %d %d", eap_ds->response->type.length, vp->length);
			eap_ds->request->code = PW_EAP_FAILURE;
			return 0;
		}

		if (memcmp(eap_ds->response->type.data,
			   vp->strvalue, vp->length) != 0) {
			DEBUG2("  rlm_eap_gtc: ERROR: Passwords are different");
			eap_ds->request->code = PW_EAP_FAILURE;
			return 0;
		}

		/*
		 *	EAP packets can be ~64k long maximum, and
		 *	we don't like that.
		 */
	} else if (eap_ds->response->type.length <= 128) {
		int rcode;

		/*
		 *	If there was a User-Password in the request,
		 *	why the heck are they using EAP-GTC?
		 */
		rad_assert(handler->request->password == NULL);

		vp = pairmake("User-Password", "", T_OP_EQ);
		if (!vp) {
			radlog(L_ERR, "rlm_eap_gtc: out of memory");
			return 0;
		}
		vp->length = eap_ds->response->type.length;
		memcpy(vp->strvalue, eap_ds->response->type.data, vp->length);
		vp->strvalue[vp->length] = 0;

		/*
		 *	Add the password to the request, and allow
		 *	another module to do the work of authenticating it.
		 */
		pairadd(&handler->request->packet->vps, vp);
		handler->request->password = vp;

		/*
		 *	This is a wild & crazy hack.
		 */
		rcode = module_authenticate(inst->auth_type, handler->request);
		if (rcode != RLM_MODULE_OK) {
			eap_ds->request->code = PW_EAP_FAILURE;
			return 0;
		}

	} else {
		radlog(L_ERR, "rlm_eap_gtc: Response is too large to understand");
		eap_ds->request->code = PW_EAP_FAILURE;
		return 0;

	}

	DEBUG2("  rlm_eap_gtc: Everything is OK.");

	eap_ds->request->code = PW_EAP_SUCCESS;

	return 1;
}

/*
 *	The module name should be the only globally exported symbol.
 *	That is, everything else should be 'static'.
 */
EAP_TYPE rlm_eap_gtc = {
	"eap_gtc",
	gtc_attach,	      		/* attach */
	gtc_initiate,			/* Start the initial request */
	NULL,				/* authorization */
	gtc_authenticate,		/* authentication */
	gtc_detach     			/* detach */
};
