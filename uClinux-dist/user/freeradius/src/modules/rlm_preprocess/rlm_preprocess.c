/*
 * rlm_preprocess.c
 *		Contains the functions for the "huntgroups" and "hints"
 *		files.
 *
 * Version:     $Id: rlm_preprocess.c,v 1.52.2.1.2.1 2006/05/05 17:31:53 aland Exp $
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
 * Copyright 2000  Alan DeKok <aland@ox.org>
 */

static const char rcsid[] = "$Id: rlm_preprocess.c,v 1.52.2.1.2.1 2006/05/05 17:31:53 aland Exp $";

#include	"autoconf.h"
#include	"libradius.h"

#include	<sys/stat.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

#include	"radiusd.h"
#include	"modules.h"

typedef struct rlm_preprocess_t {
	char		*huntgroup_file;
	char		*hints_file;
	PAIR_LIST	*huntgroups;
	PAIR_LIST	*hints;
	int		with_ascend_hack;
	int		ascend_channels_per_line;
	int		with_ntdomain_hack;
	int		with_specialix_jetstream_hack;
	int		with_cisco_vsa_hack;
} rlm_preprocess_t;

static CONF_PARSER module_config[] = {
	{ "huntgroups",			PW_TYPE_STRING_PTR,
	  offsetof(rlm_preprocess_t,huntgroup_file), NULL,
	  "${raddbdir}/huntgroups" },
	{ "hints",			PW_TYPE_STRING_PTR,
	  offsetof(rlm_preprocess_t,hints_file), NULL,
	  "${raddbdir}/hints" },
	{ "with_ascend_hack",		PW_TYPE_BOOLEAN,
	  offsetof(rlm_preprocess_t,with_ascend_hack), NULL, "no" },
	{ "ascend_channels_per_line",   PW_TYPE_INTEGER,
	  offsetof(rlm_preprocess_t,ascend_channels_per_line), NULL, "23" },

	{ "with_ntdomain_hack",		PW_TYPE_BOOLEAN,
	  offsetof(rlm_preprocess_t,with_ntdomain_hack), NULL, "no" },
	{ "with_specialix_jetstream_hack",  PW_TYPE_BOOLEAN,
	  offsetof(rlm_preprocess_t,with_specialix_jetstream_hack), NULL,
	  "no" },
	{ "with_cisco_vsa_hack",        PW_TYPE_BOOLEAN,
	  offsetof(rlm_preprocess_t,with_cisco_vsa_hack), NULL, "no" },

	{ NULL, -1, 0, NULL, NULL }
};


/*
 *	dgreer --
 *	This hack changes Ascend's wierd port numberings
 *	to standard 0-??? port numbers so that the "+" works
 *	for IP address assignments.
 */
static void ascend_nasport_hack(VALUE_PAIR *nas_port, int channels_per_line)
{
	int service;
	int line;
	int channel;

	if (!nas_port) {
		return;
	}

	if (nas_port->lvalue > 9999) {
		service = nas_port->lvalue/10000; /* 1=digital 2=analog */
		line = (nas_port->lvalue - (10000 * service)) / 100;
		channel = nas_port->lvalue-((10000 * service)+(100 * line));
		nas_port->lvalue =
			(channel - 1) + (line - 1) * channels_per_line;
	}
}

/*
 *	ThomasJ --
 *	This hack strips out Cisco's VSA duplicities in lines
 *	(Cisco not implemented VSA's in standard way.
 *
 *	Cisco sends it's VSA attributes with the attribute name *again*
 *	in the string, like:  H323-Attribute = "h323-attribute=value".
 *	This sort of behaviour is nonsense.
 */
static void cisco_vsa_hack(VALUE_PAIR *vp)
{
	int		vendorcode;
	char		*ptr;
	char		newattr[MAX_STRING_LEN];

	for ( ; vp != NULL; vp = vp->next) {
		vendorcode = (vp->attribute >> 16); /* HACK! */
		if (!((vendorcode == 9) || (vendorcode == 6618))) continue; /* not a Cisco or Quintum VSA, continue */

		if (vp->type != PW_TYPE_STRING) continue;

		/*
		 *  No weird packing.  Ignore it.
		 */
		ptr = strchr(vp->strvalue, '='); /* find an '=' */
		if (!ptr) continue;

		/*
		 *	Cisco-AVPair's get packed as:
		 *
		 *	Cisco-AVPair = "h323-foo-bar = baz"
		 *	Cisco-AVPair = "h323-foo-bar=baz"
		 *
		 *	which makes sense only if you're a lunatic.
		 *	This code looks for the attribute named inside
		 *	of the string, and if it exists, adds it as a new
		 *	attribute.
		 */
		if ((vp->attribute & 0xffff) == 1) {
			char *p;
			DICT_ATTR	*dattr;

			p = vp->strvalue;
			gettoken(&p, newattr, sizeof(newattr));

			if (((dattr = dict_attrbyname(newattr)) != NULL) &&
			    (dattr->type == PW_TYPE_STRING)) {
				VALUE_PAIR *newvp;

				/*
				 *  Make a new attribute.
				 */
				newvp = pairmake(newattr, ptr + 1, T_OP_EQ);
				if (newvp) {
					pairadd(&vp, newvp);
				}
			}
		} else {	/* h322-foo-bar = "h323-foo-bar = baz" */
			/*
			 *	We strip out the duplicity from the
			 *	value field, we use only the value on
			 *	the right side of the '=' character.
			 */
			strNcpy(newattr, ptr + 1, sizeof(newattr));
			strNcpy((char *)vp->strvalue, newattr,
				sizeof(vp->strvalue));
			vp->length = strlen((char *)vp->strvalue);
		}
	}
}

/*
 *	Mangle username if needed, IN PLACE.
 */
static void rad_mangle(rlm_preprocess_t *data, REQUEST *request)
{
	VALUE_PAIR	*namepair;
	VALUE_PAIR	*request_pairs;
	VALUE_PAIR	*tmp;

	/*
	 *	Get the username from the request
	 *	If it isn't there, then we can't mangle the request.
	 */
	request_pairs = request->packet->vps;
	namepair = pairfind(request_pairs, PW_USER_NAME);
	if ((namepair == NULL) ||
	    (namepair->length <= 0)) {
	  return;
	}

	if (data->with_ntdomain_hack) {
		char		*ptr;
		char		newname[MAX_STRING_LEN];

		/*
		 *	Windows NT machines often authenticate themselves as
		 *	NT_DOMAIN\username. Try to be smart about this.
		 *
		 *	FIXME: should we handle this as a REALM ?
		 */
		if ((ptr = strchr(namepair->strvalue, '\\')) != NULL) {
			strNcpy(newname, ptr + 1, sizeof(newname));
			/* Same size */
			strcpy(namepair->strvalue, newname);
			namepair->length = strlen(newname);
		}
	}

	if (data->with_specialix_jetstream_hack) {
		char		*ptr;

		/*
		 *	Specialix Jetstream 8500 24 port access server.
		 *	If the user name is 10 characters or longer, a "/"
		 *	and the excess characters after the 10th are
		 *	appended to the user name.
		 *
		 *	Reported by Lucas Heise <root@laonet.net>
		 */
		if ((strlen((char *)namepair->strvalue) > 10) &&
		    (namepair->strvalue[10] == '/')) {
			for (ptr = (char *)namepair->strvalue + 11; *ptr; ptr++)
				*(ptr - 1) = *ptr;
			*(ptr - 1) = 0;
			namepair->length = strlen((char *)namepair->strvalue);
		}
	}

	/*
	 *	Small check: if Framed-Protocol present but Service-Type
	 *	is missing, add Service-Type = Framed-User.
	 */
	if (pairfind(request_pairs, PW_FRAMED_PROTOCOL) != NULL &&
	    pairfind(request_pairs, PW_SERVICE_TYPE) == NULL) {
		tmp = paircreate(PW_SERVICE_TYPE, PW_TYPE_INTEGER);
		if (tmp) {
			tmp->lvalue = PW_FRAMED_USER;
			pairmove(&request_pairs, &tmp);
		}
	}
}

/*
 *	Compare the request with the "reply" part in the
 *	huntgroup, which normally only contains username or group.
 *	At least one of the "reply" items has to match.
 */
static int hunt_paircmp(REQUEST *req, VALUE_PAIR *request, VALUE_PAIR *check)
{
	VALUE_PAIR	*check_item = check;
	VALUE_PAIR	*tmp;
	int		result = -1;

	if (check == NULL) return 0;

	while (result != 0 && check_item != NULL) {

		tmp = check_item->next;
		check_item->next = NULL;

		result = paircmp(req, request, check_item, NULL);

		check_item->next = tmp;
		check_item = check_item->next;
	}

	return result;
}


/*
 *	Add hints to the info sent by the terminal server
 *	based on the pattern of the username, and other attributes.
 */
static int hints_setup(PAIR_LIST *hints, REQUEST *request)
{
	char		*name;
	VALUE_PAIR	*add;
	VALUE_PAIR	*tmp;
	PAIR_LIST	*i;
	VALUE_PAIR *request_pairs;

	request_pairs = request->packet->vps;

	if (hints == NULL || request_pairs == NULL)
		return RLM_MODULE_NOOP;

	/*
	 *	Check for valid input, zero length names not permitted
	 */
	if ((tmp = pairfind(request_pairs, PW_USER_NAME)) == NULL)
		name = NULL;
	else
		name = (char *)tmp->strvalue;

	if (name == NULL || name[0] == 0)
		/*
		 *	No name, nothing to do.
		 */
		return RLM_MODULE_NOOP;

	for (i = hints; i; i = i->next) {
		/*
		 *	Use "paircmp", which is a little more general...
		 */
		if (paircmp(request, request_pairs, i->check, NULL) == 0) {
			DEBUG2("  hints: Matched %s at %d",
			       i->name, i->lineno);
			break;
		}
	}

	if (i == NULL) return RLM_MODULE_NOOP;

	add = paircopy(i->reply);

	/*
	 *	Now add all attributes to the request list,
	 *	except the PW_STRIP_USER_NAME one, and
	 *	xlat them.
	 */
	pairdelete(&add, PW_STRIP_USER_NAME);
	pairxlatmove(request, &request->packet->vps, &add);
	pairfree(&add);

	return RLM_MODULE_UPDATED;
}

/*
 *	See if we have access to the huntgroup.
 */
static int huntgroup_access(REQUEST *request,
			    PAIR_LIST *huntgroups, VALUE_PAIR *request_pairs)
{
	PAIR_LIST	*i;
	int		r = RLM_MODULE_OK;

	/*
	 *	We're not controlling access by huntgroups:
	 *	Allow them in.
	 */
	if (huntgroups == NULL)
		return RLM_MODULE_OK;

	for(i = huntgroups; i; i = i->next) {
		/*
		 *	See if this entry matches.
		 */
		if (paircmp(request, request_pairs, i->check, NULL) != 0)
			continue;

		/*
		 *	Now check for access.
		 */
		r = RLM_MODULE_REJECT;
		if (hunt_paircmp(request, request_pairs, i->reply) == 0) {
			VALUE_PAIR *vp;

			/*
			 *  We've matched the huntgroup, so add it in
			 *  to the list of request pairs.
			 */
			vp = pairfind(request_pairs, PW_HUNTGROUP_NAME);
			if (!vp) {
				vp = paircreate(PW_HUNTGROUP_NAME,
						PW_TYPE_STRING);
				if (!vp) {
					radlog(L_ERR, "No memory");
					r = RLM_MODULE_FAIL;
				}

				strNcpy(vp->strvalue, i->name,
					sizeof(vp->strvalue));
				vp->length = strlen(vp->strvalue);

				pairadd(&request_pairs, vp);
			}
			r = RLM_MODULE_OK;
		}
		break;
	}

	return r;
}

/*
 *	If the NAS wasn't smart enought to add a NAS-IP-Address
 *	to the request, then add it ourselves.
 */
static int add_nas_attr(REQUEST *request)
{
	VALUE_PAIR *nas;

	nas = pairfind(request->packet->vps, PW_NAS_IP_ADDRESS);
	if (!nas) {
		nas = paircreate(PW_NAS_IP_ADDRESS, PW_TYPE_IPADDR);
		if (!nas) {
			radlog(L_ERR, "No memory");
			return -1;
		}
		nas->lvalue = request->packet->src_ipaddr;
		ip_hostname(nas->strvalue, sizeof(nas->strvalue), nas->lvalue);
		pairadd(&request->packet->vps, nas);
	}

	/*
	 *	Add in a Client-IP-Address, to tell the user
	 *	the source IP of the request.  That is, the client,
	 *
	 *	Note that this MAY BE different from the NAS-IP-Address,
	 *	especially if the request is being proxied.
	 *
	 *	Note also that this is a server configuration item,
	 *	and will NOT make it to any packets being sent from
	 *	the server.
	 */
	nas = paircreate(PW_CLIENT_IP_ADDRESS, PW_TYPE_IPADDR);
	if (!nas) {
	  radlog(L_ERR, "No memory");
	  return -1;
	}
	nas->lvalue = request->packet->src_ipaddr;
	ip_hostname(nas->strvalue, sizeof(nas->strvalue), nas->lvalue);
	pairadd(&request->packet->vps, nas);
	return 0;
}


/*
 *	Initialize.
 */
static int preprocess_instantiate(CONF_SECTION *conf, void **instance)
{
	int	rcode;
	rlm_preprocess_t *data;

	/*
	 *	Allocate room to put the module's instantiation data.
	 */
	data = (rlm_preprocess_t *) rad_malloc(sizeof(*data));
	memset(data, 0, sizeof(*data));

	/*
	 *	Read this modules configuration data.
	 */
        if (cf_section_parse(conf, data, module_config) < 0) {
		free(data);
                return -1;
        }

	data->huntgroups = NULL;
	data->hints = NULL;

	/*
	 *	Read the huntgroups file.
	 */
	rcode = pairlist_read(data->huntgroup_file, &(data->huntgroups), 0);
	if (rcode < 0) {
		radlog(L_ERR|L_CONS, "rlm_preprocess: Error reading %s",
		       data->huntgroup_file);
		return -1;
	}

	/*
	 *	Read the hints file.
	 */
	rcode = pairlist_read(data->hints_file, &(data->hints), 0);
	if (rcode < 0) {
		radlog(L_ERR|L_CONS, "rlm_preprocess: Error reading %s",
		       data->hints_file);
		return -1;
	}

	/*
	 *	Save the instantiation data for later.
	 */
	*instance = data;

	return 0;
}

/*
 *	Preprocess a request.
 */
static int preprocess_authorize(void *instance, REQUEST *request)
{
	char buf[1024];
	int r;
	rlm_preprocess_t *data = (rlm_preprocess_t *) instance;

	/*
	 *	Mangle the username, to get rid of stupid implementation
	 *	bugs.
	 */
	rad_mangle(data, request);

	if (data->with_ascend_hack) {
		/*
		 *	If we're using Ascend systems, hack the NAS-Port-Id
		 *	in place, to go from Ascend's weird values to something
		 *	approaching rationality.
		 */
		ascend_nasport_hack(pairfind(request->packet->vps,
					     PW_NAS_PORT),
				    data->ascend_channels_per_line);
	}

	if (data->with_cisco_vsa_hack) {
	 	/*
		 *	We need to run this hack because the h323-conf-id
		 *	attribute should be used.
		 */
		cisco_vsa_hack(request->packet->vps);
	}

	/*
	 *	Note that we add the Request-Src-IP-Address to the request
	 *	structure BEFORE checking huntgroup access.  This allows
	 *	the Request-Src-IP-Address to be used for huntgroup
	 *	comparisons.
	 */
	if (add_nas_attr(request) < 0) {
		return RLM_MODULE_FAIL;
	}

	hints_setup(data->hints, request);

	/*
	 *      If there is a PW_CHAP_PASSWORD attribute but there
	 *      is PW_CHAP_CHALLENGE we need to add it so that other
	 *	modules can use it as a normal attribute.
	 */
	if (pairfind(request->packet->vps, PW_CHAP_PASSWORD) &&
	    pairfind(request->packet->vps, PW_CHAP_CHALLENGE) == NULL) {
		VALUE_PAIR *vp;
		vp = paircreate(PW_CHAP_CHALLENGE, PW_TYPE_OCTETS);
		if (!vp) {
			radlog(L_ERR|L_CONS, "no memory");
			return RLM_MODULE_FAIL;
		}
		vp->length = AUTH_VECTOR_LEN;
		memcpy(vp->strvalue, request->packet->vector, AUTH_VECTOR_LEN);
		pairadd(&request->packet->vps, vp);
	}

	if ((r = huntgroup_access(request, data->huntgroups,
			     request->packet->vps)) != RLM_MODULE_OK) {
		radlog(L_AUTH, "No huntgroup access: [%s] (%s)",
		       request->username ? request->username->strvalue : "<No User-Name>",
		       auth_name(buf, sizeof(buf), request, 1));
		return r;
	}

	return RLM_MODULE_OK; /* Meaning: try next authorization module */
}

/*
 *	Preprocess a request before accounting
 */
static int preprocess_preaccounting(void *instance, REQUEST *request)
{
	int r;
	rlm_preprocess_t *data = (rlm_preprocess_t *) instance;

	/*
	 *  Ensure that we have the SAME user name for both
	 *  authentication && accounting.
	 */
	rad_mangle(data, request);

	if (data->with_cisco_vsa_hack) {
	 	/*
		 *	We need to run this hack because the h323-conf-id
		 *	attribute should be used.
		 */
		cisco_vsa_hack(request->packet->vps);
	}

	/*
	 *  Ensure that we log the NAS IP Address in the packet.
	 */
	if (add_nas_attr(request) < 0) {
		return RLM_MODULE_FAIL;
	}

	r = hints_setup(data->hints, request);

	return r;
}

/*
 *      Clean up the module's instance.
 */
static int preprocess_detach(void *instance)
{
	rlm_preprocess_t *data = (rlm_preprocess_t *) instance;

	pairlist_free(&(data->huntgroups));
	pairlist_free(&(data->hints));

	free(data->huntgroup_file);
	free(data->hints_file);
	free(data);

	return 0;
}

/* globally exported name */
module_t rlm_preprocess = {
	"preprocess",
	0,			/* type: reserved */
	NULL,			/* initialization */
	preprocess_instantiate,	/* instantiation */
	{
		NULL,			/* authentication */
		preprocess_authorize,	/* authorization */
		preprocess_preaccounting, /* pre-accounting */
		NULL,			/* accounting */
		NULL,			/* checksimul */
		NULL,			/* pre-proxy */
		NULL,			/* post-proxy */
		NULL			/* post-auth */
	},
	preprocess_detach,	/* detach */
	NULL,			/* destroy */
};

