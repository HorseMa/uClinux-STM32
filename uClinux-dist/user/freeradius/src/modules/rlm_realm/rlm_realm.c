/*
 * rlm_realm.c
 *
 * Version:	$Id: rlm_realm.c,v 1.50 2004/05/17 09:52:05 aland Exp $
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
 * Copyright 2000  The FreeRADIUS server project
 * FIXME add copyrights
 */

#include "autoconf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "libradius.h"
#include "radiusd.h"
#include "modules.h"

static const char rcsid[] = "$Id: rlm_realm.c,v 1.50 2004/05/17 09:52:05 aland Exp $";

#define  REALM_FORMAT_PREFIX   0
#define  REALM_FORMAT_SUFFIX   1

typedef struct realm_config_t {
        int        format;
        char       *formatstring;
        char       *delim;
	int        ignore_default;
	int        ignore_null;
} realm_config_t;

static CONF_PARSER module_config[] = {
  { "format", PW_TYPE_STRING_PTR,
    offsetof(realm_config_t,formatstring), NULL, "suffix" },
  { "delimiter", PW_TYPE_STRING_PTR,
    offsetof(realm_config_t,delim), NULL, "@" },
  { "ignore_default", PW_TYPE_BOOLEAN,
    offsetof(realm_config_t,ignore_default), NULL, "no" },
  { "ignore_null", PW_TYPE_BOOLEAN,
    offsetof(realm_config_t,ignore_null), NULL, "no" },
  { NULL, -1, 0, NULL, NULL }    /* end the list */
};

/*
 *	Internal function to cut down on duplicated code.
 *
 *	Returns -1 on failure, 0 on no failure.  returnrealm
 *	is NULL on don't proxy, realm otherwise.
 */
static int check_for_realm(void *instance, REQUEST *request, REALM **returnrealm)
{
	char namebuf[MAX_STRING_LEN];
	char *username;
	char *realmname = NULL;
	char *ptr;
	VALUE_PAIR *vp;
	REALM *realm;

        struct realm_config_t *inst = instance;

	/* initiate returnrealm */
	*returnrealm = NULL;

	/*
	 *	If the request has a proxy entry, then it's a proxy
	 *	reply, and we're walking through the module list again.
	 *
	 *	In that case, don't bother trying to proxy the request
	 *	again.
	 *
	 *	Also, if there's no User-Name attribute, we can't
	 *	proxy it, either.
	 */
	if ((request->proxy != NULL) ||
	    (request->username == NULL)) {
		DEBUG2("    rlm_realm: Proxy reply, or no User-Name.  Ignoring.");
		return 0;
	}

	/*
	 *      Check for 'Realm' attribute.  If it exists, then we've proxied
	 *      it already ( via another rlm_realm instance ) and should return.
	 */

	if ( (vp = pairfind(request->packet->vps, PW_REALM)) != NULL ) {
	        DEBUG2("    rlm_realm: Request already proxied.  Ignoring.");
		return 0;
	}

	/*
	 *	We will be modifing this later, so we want our own copy
	 *	of it.
	 */
	strNcpy(namebuf, (char *)request->username->strvalue, sizeof(namebuf));
	username = namebuf;

	switch(inst->format)
	{

	case REALM_FORMAT_SUFFIX:

	  /* DEBUG2("  rlm_realm: Checking for suffix after \"%c\"", inst->delim[0]); */
		realmname = strrchr(username, inst->delim[0]);
		if (realmname) {
			*realmname = '\0';
			realmname++;
		}
		break;

	case REALM_FORMAT_PREFIX:

		/* DEBUG2("  rlm_realm: Checking for prefix before \"%c\"", inst->delim[0]); */

		ptr = strchr(username, inst->delim[0]);
		if (ptr) {
			*ptr = '\0';
		     ptr++;
		     realmname = username;
		     username = ptr;
		}
		break;

	default:
		realmname = NULL;
		break;
	}

	/*
	 *	Print out excruciatingly descriptive debugging messages
	 *	for the people who find it too difficult to think about
	 *	what's going on.
	 */
	if (realmname) {
		DEBUG2("    rlm_realm: Looking up realm \"%s\" for User-Name = \"%s\"",
		       realmname, request->username->strvalue);
	} else {
		if( inst->ignore_null ) {
			DEBUG2("    rlm_realm: No '%c' in User-Name = \"%s\", skipping NULL due to config.",
			inst->delim[0], request->username->strvalue);
			return 0;
		}
		DEBUG2("    rlm_realm: No '%c' in User-Name = \"%s\", looking up realm NULL",
		       inst->delim[0], request->username->strvalue);
	}

	/*
	 *	Allow DEFAULT realms unless told not to.
	 */
	realm = realm_find(realmname, (request->packet->code == PW_ACCOUNTING_REQUEST));
	if (!realm) {
		DEBUG2("    rlm_realm: No such realm \"%s\"",
		       (realmname == NULL) ? "NULL" : realmname);
		return 0;
	}
	if( inst->ignore_default &&
	    (strcmp(realm->realm, "DEFAULT")) == 0) {
		DEBUG2("    rlm_realm: Found DEFAULT, but skipping due to config.");
		return 0;
	}


	DEBUG2("    rlm_realm: Found realm \"%s\"", realm->realm);

	/*
	 *	If we've been told to strip the realm off, then do so.
	 */
	if (realm->striprealm) {
		/*
		 *	Create the Stripped-User-Name attribute, if it
		 *	doesn't exist.
		 *
		 */
		if (request->username->attribute != PW_STRIPPED_USER_NAME) {
			vp = paircreate(PW_STRIPPED_USER_NAME, PW_TYPE_STRING);
			if (!vp) {
				radlog(L_ERR|L_CONS, "no memory");
				return -1;
			}
			pairadd(&request->packet->vps, vp);
			DEBUG2("    rlm_realm: Adding Stripped-User-Name = \"%s\"", username);
		} else {
			vp = request->username;
			DEBUG2("    rlm_realm: Setting Stripped-User-Name = \"%s\"", username);
		}

		strcpy(vp->strvalue, username);
		vp->length = strlen((char *)vp->strvalue);
		request->username = vp;
	}

	DEBUG2("    rlm_realm: Proxying request from user %s to realm %s",
	       username, realm->realm);

	/*
	 *	Add the realm name to the request.
	 */
	pairadd(&request->packet->vps, pairmake("Realm", realm->realm,
						T_OP_EQ));
	DEBUG2("    rlm_realm: Adding Realm = \"%s\"", realm->realm);

	/*
	 *	Figure out what to do with the request.
	 */
	switch (request->packet->code) {
	default:
		DEBUG2("    rlm_realm: Unknown packet code %d\n",
		       request->packet->code);
		return 0;		/* don't do anything */

		/*
		 *	Perhaps accounting proxying was turned off.
		 */
	case PW_ACCOUNTING_REQUEST:
		if (realm->acct_ipaddr == htonl(INADDR_NONE)) {
			DEBUG2("    rlm_realm: Accounting realm is LOCAL.");
			return 0;
		}

		if (realm->acct_port == 0) {
			DEBUG2("    rlm_realm: acct_port is not set.  Proxy cancelled.");
			return 0;
		}
		break;

		/*
		 *	Perhaps authentication proxying was turned off.
		 */
	case PW_AUTHENTICATION_REQUEST:
		if (realm->ipaddr == htonl(INADDR_NONE)) {
			DEBUG2("    rlm_realm: Authentication realm is LOCAL.");
			return 0;
		}

		if (realm->auth_port == 0) {
			DEBUG2("    rlm_realm: auth_port is not set.  Proxy cancelled.");
			return 0;
		}
		break;
	}

	/*
	 *      If this request has arrived from another freeradius server
	 *      that has already proxied the request, we don't need to do
	 *      it again.
	 */
	for (vp = request->packet->vps; vp; vp = vp->next) {
		if (vp->attribute == PW_FREERADIUS_PROXIED_TO) {
			if (request->packet->code == PW_AUTHENTICATION_REQUEST &&
			    vp->lvalue == realm->ipaddr) {
				DEBUG2("    rlm_realm: Request not proxied due to Freeradius-Proxied-To");
				return 0;
			}
			if (request->packet->code == PW_ACCOUNTING_REQUEST &&
			    vp->lvalue == realm->acct_ipaddr) {
				DEBUG2("    rlm_realm: Request not proxied due to Freeradius-Proxied-To");
				return 0;
			}
		}
        }

	/*
	 *	We got this far, which means we have a realm, set returnrealm
	 */
	*returnrealm = realm;
	return 0;
}

/*
 *	Add a "Proxy-To-Realm" attribute to the request.
 */
static void add_proxy_to_realm(VALUE_PAIR **vps, REALM *realm)
{
	VALUE_PAIR *vp;

	/*
	 *	Tell the server to proxy this request to another
	 *	realm.
	 */
	vp = pairmake("Proxy-To-Realm", realm->realm, T_OP_EQ);
	if (!vp) {
		radlog(L_ERR|L_CONS, "no memory");
		exit(1);
	}

	/*
	 *  Add it, even if it's already present.
	 */
	pairadd(vps, vp);
}

/*
 *  Perform the realm module instantiation.  Configuration info is
 *  stored in *instance for later use.
 */

static int realm_instantiate(CONF_SECTION *conf, void **instance)
{
        struct realm_config_t *inst;

        /* setup a storage area for instance data */
        inst = rad_malloc(sizeof(*inst));
	if (!inst) {
		return -1;
	}
	memset(inst, 0, sizeof(*inst));

	if(cf_section_parse(conf, inst, module_config) < 0) {
	       free(inst);
               return -1;
	}

	if(strcasecmp(inst->formatstring, "suffix") == 0) {
	     inst->format = REALM_FORMAT_SUFFIX;
	} else if(strcasecmp(inst->formatstring, "prefix") == 0) {
	     inst->format = REALM_FORMAT_PREFIX;
        } else {
	     radlog(L_ERR, "Bad value \"%s\" for realm format value", inst->formatstring);
	     free(inst);
	     return -1;
	}
	free(inst->formatstring);
	if(strlen(inst->delim) != 1) {
	     radlog(L_ERR, "Bad value \"%s\" for realm delimiter value", inst->delim);
	     free(inst);
	     return -1;
	}

	*instance = inst;
	return 0;

}





/*
 *  Examine a request for a username with an realm, and if it
 *  corresponds to something in the realms file, set that realm as
 *  Proxy-To.
 *
 *  This should very nearly duplicate the old proxy_send() code
 */
static int realm_authorize(void *instance, REQUEST *request)
{
	REALM *realm;

	/*
	 *	Check if we've got to proxy the request.
	 *	If not, return without adding a Proxy-To-Realm
	 *	attribute.
	 */
	if (check_for_realm(instance, request, &realm) < 0) {
		return RLM_MODULE_FAIL;
	}
	if (!realm) {
		return RLM_MODULE_NOOP;
	}

	/*
	 *	Maybe add a Proxy-To-Realm attribute to the request.
	 */
	DEBUG2("    rlm_realm: Preparing to proxy authentication request to realm \"%s\"\n",
	       realm->realm);
	add_proxy_to_realm(&request->config_items, realm);

	return RLM_MODULE_UPDATED; /* try the next module */
}

/*
 * This does the exact same thing as the realm_authorize, it's just called
 * differently.
 */
static int realm_preacct(void *instance, REQUEST *request)
{
	const char *name = (char *)request->username->strvalue;
	REALM *realm;

	if (!name)
	  return RLM_MODULE_OK;


	/*
	 *	Check if we've got to proxy the request.
	 *	If not, return without adding a Proxy-To-Realm
	 *	attribute.
	 */
	if (check_for_realm(instance, request, &realm) < 0) {
		return RLM_MODULE_FAIL;
	}
	if (!realm) {
		return RLM_MODULE_NOOP;
	}


	/*
	 *	Maybe add a Proxy-To-Realm attribute to the request.
	 */
	DEBUG2("    rlm_realm: Preparing to proxy accounting request to realm \"%s\"\n",
	       realm->realm);
	add_proxy_to_realm(&request->config_items, realm);

	return RLM_MODULE_UPDATED; /* try the next module */
}

static int realm_detach(void *instance)
{
	struct realm_config_t *inst = instance;
	free(inst->delim);
	free(instance);
	return 0;
}

/* globally exported name */
module_t rlm_realm = {
  "realm",
  0,				/* type: reserved */
  NULL,				/* initialization */
  realm_instantiate,	       	/* instantiation */
  {
	  NULL,			/* authentication */
	  realm_authorize,	/* authorization */
	  realm_preacct,	/* preaccounting */
	  NULL,			/* accounting */
	  NULL,			/* checksimul */
	  NULL,			/* pre-proxy */
	  NULL,			/* post-proxy */
	  NULL			/* post-auth */
  },
  realm_detach,			/* detach */
  NULL,				/* destroy */
};
