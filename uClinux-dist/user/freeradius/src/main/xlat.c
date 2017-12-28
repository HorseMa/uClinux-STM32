/*
 * xlat.c	Translate strings.  This is the first version of xlat
 * 		incorporated to RADIUS
 *
 * Version:	$Id: xlat.c,v 1.72.2.7.2.1 2005/12/08 12:47:56 nbk Exp $
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

static const char rcsid[] =
"$Id: xlat.c,v 1.72.2.7.2.1 2005/12/08 12:47:56 nbk Exp $";

#include	"autoconf.h"
#include	"libradius.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

#include	"radiusd.h"

#include	"rad_assert.h"

typedef struct xlat_t {
	char		module[MAX_STRING_LEN];
	int		length;
	void		*instance;
	RAD_XLAT_FUNC	do_xlat;
	int		internal;	/* not allowed to re-define these */
} xlat_t;

static rbtree_t *xlat_root = NULL;

/*
 *	Define all xlat's in the structure.
 */
static const char *internal_xlat[] = {"check",
				      "request",
				      "reply",
				      "proxy-request",
				      "proxy-reply",
				      NULL};

#if REQUEST_MAX_REGEX > 8
#error Please fix the following line
#endif
static int xlat_inst[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };	/* up to 8 for regex */


/*
 *	Convert the value on a VALUE_PAIR to string
 */
static int valuepair2str(char * out,int outlen,VALUE_PAIR * pair,
			 int type, RADIUS_ESCAPE_STRING func)
{
	char buffer[MAX_STRING_LEN * 4];

	if (pair != NULL) {
		vp_prints_value(buffer, sizeof(buffer), pair, 0);
		return func(out, outlen, buffer);
	}

	switch (type) {
	case PW_TYPE_STRING :
		strNcpy(out,"_",outlen);
		break;
	case PW_TYPE_INTEGER :
		strNcpy(out,"0",outlen);
		break;
	case PW_TYPE_IPADDR :
		strNcpy(out,"?.?.?.?",outlen);
		break;
	case PW_TYPE_DATE :
		strNcpy(out,"0",outlen);
		break;
	default :
		strNcpy(out,"unknown_type",outlen);
	}
	return strlen(out);
}


/*
 *	Dynamically translate for check:, request:, reply:, etc.
 */
static int xlat_packet(void *instance, REQUEST *request,
		       char *fmt, char *out, size_t outlen,
		       RADIUS_ESCAPE_STRING func)
{
	DICT_ATTR	*da;
	VALUE_PAIR	*vp;
	VALUE_PAIR	*vps = NULL;
	RADIUS_PACKET	*packet = NULL;

	switch (*(int*) instance) {
	case 0:
		vps = request->config_items;
		break;

	case 1:
		vps = request->packet->vps;
		packet = request->packet;
		break;

	case 2:
		vps = request->reply->vps;
		packet = request->reply;
		break;

	case 3:
		if (request->proxy) vps = request->proxy->vps;
		packet = request->proxy;
		break;

	case 4:
		if (request->proxy_reply) vps = request->proxy_reply->vps;
		packet = request->proxy_reply;
		break;

	default:		/* WTF? */
		return 0;
	}

	/*
	 *	The "format" string is the attribute name.
	 */
	da = dict_attrbyname(fmt);
	if (!da) {
		int index;
		const char *p = strchr(fmt, '[');
		char buffer[256];

		if (!p) return 0;
		if (strlen(fmt) > sizeof(buffer)) return 0;

		strNcpy(buffer, fmt, p - fmt + 1);

		index = atoi(p + 1);

		/*
		 *	Check the format of the index before looking
		 *	the attribute up in the dictionary, because
		 *	it's a cheap check.
		 */
		p += 1 + strspn(p + 1, "0123456789");
		if (*p != ']') {
			DEBUG2("xlat: Invalid array reference in string at %s %s",
			       fmt, p);
			return 0;
		}

		da = dict_attrbyname(buffer);
		if (!da) return 0;


		/*
		 *	Find the N'th value.
		 */
		for (vp = pairfind(vps, da->attr);
		     vp != NULL;
		     vp = pairfind(vp->next, da->attr)) {
			if (index == 0) break;
			index--;
		}

		/*
		 *	Non-existent array reference.
		 */
		if (!vp) return 0;

		return valuepair2str(out, outlen, vp, da->type, func);
	}

	vp = pairfind(vps, da->attr);
	if (!vp) {
		/*
		 *	Some "magic" handlers, which are never in VP's, but
		 *	which are in the packet.
		 *
		 *	FIXME: Add SRC/DST IP address!
		 */
		if (packet) {
			switch (da->attr) {
			case PW_PACKET_TYPE:
			{
				DICT_VALUE *dval;
				
				dval = dict_valbyattr(da->attr, packet->code);
				if (dval) {
					snprintf(out, outlen, "%s", dval->name);
				} else {
					snprintf(out, outlen, "%d", packet->code);
				}
				return strlen(out);
			}
			break;
			
			default:
				break;
			}
		}

		/*
		 *	Not found, die.
		 */
		return 0;
	}

	if (!vps) return 0;	/* silently fail */

	/*
	 *	Convert the VP to a string, and return it.
	 */
	return valuepair2str(out, outlen, vp, da->type, func);
}

#ifdef HAVE_REGEX_H
/*
 *	Pull %{0} to %{8} out of the packet.
 */
static int xlat_regex(void *instance, REQUEST *request,
		      char *fmt, char *out, size_t outlen,
		      RADIUS_ESCAPE_STRING func)
{
	char *regex;

	/*
	 *	We cheat: fmt is "0" to "8", but those numbers
	 *	are already in the "instance".
	 */
	fmt = fmt;		/* -Wunused */
	func = func;		/* -Wunused FIXME: do escaping? */
	
	regex = request_data_get(request, request,
				 REQUEST_DATA_REGEX | *(int *)instance);
	if (!regex) return 0;

	/*
	 *	Copy UP TO "freespace" bytes, including
	 *	a zero byte.
	 */
	strNcpy(out, regex, outlen);
	free(regex); /* was strdup'd */
	return strlen(out);
}
#endif				/* HAVE_REGEX_H */

/*
 *	Compare two xlat_t structs, based ONLY on the module name.
 */
static int xlat_cmp(const void *a, const void *b)
{
	if (((const xlat_t *)a)->length != ((const xlat_t *)b)->length) {
		return ((const xlat_t *)a)->length - ((const xlat_t *)b)->length;
	}

	return memcmp(((const xlat_t *)a)->module,
		      ((const xlat_t *)b)->module,
		      ((const xlat_t *)a)->length);
}


/*
 *	find the appropriate registered xlat function.
 */
static xlat_t *xlat_find(const char *module)
{
	char *p;
	xlat_t my_xlat;

	strNcpy(my_xlat.module, module, sizeof(my_xlat.module));

	/*
	 *	We get passed the WHOLE string, and all we want here
	 *	is the first piece.
	 */
	p = strchr(my_xlat.module, ':');
	if (p) *p = '\0';

	my_xlat.length = strlen(my_xlat.module);

	return rbtree_finddata(xlat_root, &my_xlat);
}


/*
 *      Register an xlat function.
 */
int xlat_register(const char *module, RAD_XLAT_FUNC func, void *instance)
{
	xlat_t	*c;
	xlat_t	my_xlat;

	if ((module == NULL) || (strlen(module) == 0)) {
		DEBUG("xlat_register: Invalid module name");
		return -1;
	}

	/*
	 *	First time around, build up the tree...
	 *
	 *	FIXME: This code should be hoisted out of this function,
	 *	and into a global "initialization".  But it isn't critical...
	 */
	if (!xlat_root) {
		int i;
#ifdef HAVE_REGEX_H
		char buffer[2];
#endif

		xlat_root = rbtree_create(xlat_cmp, free, 0);
		if (!xlat_root) {
			DEBUG("xlat_register: Failed to create tree.");
			return -1;
		}

		/*
		 *	Register the internal packet xlat's.
		 */
		for (i = 0; internal_xlat[i] != NULL; i++) {
			xlat_register(internal_xlat[i], xlat_packet, &xlat_inst[i]);
			c = xlat_find(internal_xlat[i]);
			rad_assert(c != NULL);
			c->internal = TRUE;
		}

#ifdef HAVE_REGEX_H
		/*
		 *	Register xlat's for regexes.
		 */
		buffer[1] = '\0';
		for (i = 0; i <= REQUEST_MAX_REGEX; i++) {
			buffer[0] = '0' + i;
			xlat_register(buffer, xlat_regex, &xlat_inst[i]);
			c = xlat_find(buffer);
			rad_assert(c != NULL);
			c->internal = TRUE;
		}
#endif /* HAVE_REGEX_H */
	}

	/*
	 *	If it already exists, replace the instance.
	 */
	strNcpy(my_xlat.module, module, sizeof(my_xlat.module));
	my_xlat.length = strlen(my_xlat.module);
	c = rbtree_finddata(xlat_root, &my_xlat);
	if (c) {
		if (c->internal) {
			DEBUG("xlat_register: Cannot re-define internal xlat");
			return -1;
		}

		c->do_xlat = func;
		c->instance = instance;
		return 0;
	}

	/*
	 *	Doesn't exist.  Create it.
	 */
	c = rad_malloc(sizeof(xlat_t));
	memset(c, 0, sizeof(*c));

	c->do_xlat = func;
	strNcpy(c->module, module, sizeof(c->module));
	c->length = strlen(c->module);
	c->instance = instance;

	rbtree_insert(xlat_root, c);

	return 0;
}

/*
 *      Unregister an xlat function.
 *
 *	We can only have one function to call per name, so the
 *	passing of "func" here is extraneous.
 */
void xlat_unregister(const char *module, RAD_XLAT_FUNC func)
{
	rbnode_t	*node;
	xlat_t		my_xlat;

	func = func;		/* -Wunused */

	strNcpy(my_xlat.module, module, sizeof(my_xlat.module));
	my_xlat.length = strlen(my_xlat.module);

	node = rbtree_find(xlat_root, &my_xlat);
	if (!node) return;

	rbtree_delete(xlat_root, node);
}


/*
 *	Decode an attribute name into a string.
 */
static void decode_attribute(const char **from, char **to, int freespace,
			     int *open, REQUEST *request,
			     RADIUS_ESCAPE_STRING func)
{
	char attrname[256];
	const char *p;
	char *q, *pa;
	int stop=0, found=0, retlen=0;
	int openbraces = *open;
	xlat_t *c;
	size_t namelen = sizeof(attrname);

	p = *from;
	q = *to;
	pa = &attrname[0];

	*q = '\0';

	/*
	 * Skip the '{' at the front of 'p'
	 * Increment open braces
	 */
	p++;
	openbraces++;

	/*
	 *  Copy over the rest of the string.
	 */
	while ((*p) && (!stop) && (namelen > 1)) {
		switch(*p) {
			/*
			 *  Allow braces inside things, too.
			 */
			case '\\':
				p++; /* skip it */
				*pa++ = *p++;
				break;

			case '{':
				openbraces++;
				*pa++ = *p++;
				break;

			case '}':
				openbraces--;
				if (openbraces == *open) {
					p++;
					stop=1;
				} else {
					*pa++ = *p++;
				}
				break;

				/*
				 *  Attr-Name1:-Attr-Name2
				 *
				 *  Use Attr-Name1, and if not found,
				 *  use Attr-Name2.
				 */
			case ':':
				if (p[1] == '-') {
					p += 2;
					stop = 1;
					break;
				}
				/* else FALL-THROUGH */

			default:
				*pa++ = *p++;
				break;
		}
		namelen--;
	}
	*pa = '\0';

	/*
	 *	Look up almost everything in the new tree of xlat
	 *	functions.  this makes it a little quicker...
	 */
	if ((c = xlat_find(attrname)) != NULL) {
		if (!c->internal) DEBUG("radius_xlat: Running registered xlat function of module %s for string \'%s\'",
					c->module, attrname+ c->length + 1);
		retlen = c->do_xlat(c->instance, request, attrname+(c->length+1), q, freespace, func);
		/* If retlen is 0, treat it as not found */
		if (retlen == 0) {
			found = 0;
		} else {
			found = 1;
			q += retlen;
		}

		/*
		 *	Not in the default xlat database.  Must be
		 *	a bare attribute number.
		 */
	} else if ((retlen = xlat_packet(&xlat_inst[1], request, attrname,
					 q, freespace, func)) > 0) {
		found = 1;
		q += retlen;

		/*
		 *	Look up the name, in order to get the correct
		 *	debug message.
		 */
#ifndef NDEBUG
	} else if (dict_attrbyname(attrname) == NULL) {
		/*
		 *	No attribute by that name, return an error.
		 */
		DEBUG2("WARNING: Attempt to use unknown xlat function, or non-existent attribute in string %%{%s}", attrname);
#endif
	} /* else the attribute is known, but not in the request */

	/*
	 * Skip to last '}' if attr is found
	 * The rest of the stuff within the braces is
	 * useless if we found what we need
	 */
	if (found) {
		while((*p != '\0') && (openbraces > 0)) {
			/*
			 *	Handle escapes outside of the loop.
			 */
			if (*p == '\\') {
				p++;
				if (!*p) break;
				p++; /* get & ignore next character */
				continue;
			}

			switch (*p) {
			default:
				break;

				/*
				 *  Bare brace
				 */
			case '{':
				openbraces++;
				break;

			case '}':
				openbraces--;
				break;
			}
			p++;	/* skip the character */
		}
	}

	*open = openbraces;
	*from = p;
	*to = q;
}

/*
 *  If the caller doesn't pass xlat an escape function, then
 *  we use this one.  It simplifies the coding, as the check for
 *  func == NULL only happens once.
 */
static int xlat_copy(char *out, int outlen, const char *in)
{
	int freespace = outlen;

	rad_assert(outlen > 0);

	while ((*in) && (freespace > 1)) {
		/*
		 *  Copy data.
		 *
		 *  FIXME: Do escaping of bad stuff!
		 */
		*(out++) = *(in++);

		freespace--;
	}
	*out = '\0';

	return (outlen - freespace); /* count does not include NUL */
}

/*
 *	Replace %<whatever> in a string.
 *
 *	See 'doc/variables.txt' for more information.
 */
int radius_xlat(char *out, int outlen, const char *fmt,
		REQUEST *request, RADIUS_ESCAPE_STRING func)
{
	int c, len, freespace;
	const char *p;
	char *q;
	char *nl;
	VALUE_PAIR *tmp;
	struct tm *TM, s_TM;
	char tmpdt[40]; /* For temporary storing of dates */
	int openbraces=0;

	/*
	 *	Catch bad modules.
	 */
	if (!fmt || !out || !request) return 0;

	/*
	 *  Ensure that we always have an escaping function.
	 */
	if (func == NULL) {
		func = xlat_copy;
	}

	q = out;
	p = fmt;
	while (*p) {
		/* Calculate freespace in output */
		freespace = outlen - (q - out);
		if (freespace <= 1)
			break;
		c = *p;

		if ((c != '%') && (c != '$') && (c != '\\')) {
			/*
			 * We check if we're inside an open brace.  If we are
			 * then we assume this brace is NOT literal, but is
			 * a closing brace and apply it
			 */
			if ((c == '}') && openbraces) {
				openbraces--;
				p++; /* skip it */
				continue;
			}
			*q++ = *p++;
			continue;
		}

		/*
		 *	There's nothing after this character, copy
		 *	the last '%' or "$' or '\\' over to the output
		 *	buffer, and exit.
		 */
		if (*++p == '\0') {
			*q++ = c;
			break;
		}

		if (c == '\\') {
			switch(*p) {
			case '\\':
				*q++ = *p;
				break;
			case 't':
				*q++ = '\t';
				break;
			case 'n':
				*q++ = '\n';
				break;
			default:
				*q++ = c;
				*q++ = *p;
				break;
			}
			p++;

			/*
			 *	Hmmm... ${User-Name} is a synonym for
			 *	%{User-Name}.
			 *
			 *	Why, exactly?
			 */
		} else if (c == '$') switch(*p) {
			case '{': /* Attribute by Name */
				decode_attribute(&p, &q, freespace, &openbraces, request, func);
				break;
			default:
				*q++ = c;
				*q++ = *p++;
				break;

		} else if (c == '%') switch(*p) {
			case '{':
				decode_attribute(&p, &q, freespace, &openbraces, request, func);
				break;

			case '%':
				*q++ = *p++;
				break;
			case 'a': /* Protocol: */
				q += valuepair2str(q,freespace,pairfind(request->reply->vps,PW_FRAMED_PROTOCOL),PW_TYPE_INTEGER, func);
				p++;
				break;
			case 'c': /* Callback-Number */
				q += valuepair2str(q,freespace,pairfind(request->reply->vps,PW_CALLBACK_NUMBER),PW_TYPE_STRING, func);
				p++;
				break;
			case 'd': /* request day */
				TM = localtime_r(&request->timestamp, &s_TM);
				len = strftime(tmpdt, sizeof(tmpdt), "%d", TM);
				if (len > 0) {
					strNcpy(q, tmpdt, freespace);
					q += strlen(q);
				}
				p++;
				break;
			case 'f': /* Framed IP address */
				q += valuepair2str(q,freespace,pairfind(request->reply->vps,PW_FRAMED_IP_ADDRESS),PW_TYPE_IPADDR, func);
				p++;
				break;
			case 'i': /* Calling station ID */
				q += valuepair2str(q,freespace,pairfind(request->packet->vps,PW_CALLING_STATION_ID),PW_TYPE_STRING, func);
				p++;
				break;
			case 'l': /* request timestamp */
				snprintf(tmpdt, sizeof(tmpdt), "%lu",
					 (unsigned long) request->timestamp);
				strNcpy(q,tmpdt,freespace);
				q += strlen(q);
				p++;
				break;
			case 'm': /* request month */
				TM = localtime_r(&request->timestamp, &s_TM);
				len = strftime(tmpdt, sizeof(tmpdt), "%m", TM);
				if (len > 0) {
					strNcpy(q, tmpdt, freespace);
					q += strlen(q);
				}
				p++;
				break;
			case 'n': /* NAS IP address */
				q += valuepair2str(q,freespace,pairfind(request->packet->vps,PW_NAS_IP_ADDRESS),PW_TYPE_IPADDR, func);
				p++;
				break;
			case 'p': /* Port number */
				q += valuepair2str(q,freespace,pairfind(request->packet->vps,PW_NAS_PORT),PW_TYPE_INTEGER, func);
				p++;
				break;
			case 's': /* Speed */
				q += valuepair2str(q,freespace,pairfind(request->packet->vps,PW_CONNECT_INFO),PW_TYPE_STRING, func);
				p++;
				break;
			case 't': /* request timestamp */
				CTIME_R(&request->timestamp, tmpdt, sizeof(tmpdt));
				nl = strchr(tmpdt, '\n');
				if (nl) *nl = '\0';
				strNcpy(q, tmpdt, freespace);
				q += strlen(q);
				p++;
				break;
			case 'u': /* User name */
				q += valuepair2str(q,freespace,pairfind(request->packet->vps,PW_USER_NAME),PW_TYPE_STRING, func);
				p++;
				break;
			case 'A': /* radacct_dir */
				strNcpy(q,radacct_dir,freespace-1);
				q += strlen(q);
				p++;
				break;
			case 'C': /* ClientName */
				strNcpy(q,client_name(request->packet->src_ipaddr),freespace-1);
				q += strlen(q);
				p++;
				break;
			case 'D': /* request date */
				TM = localtime_r(&request->timestamp, &s_TM);
				len = strftime(tmpdt, sizeof(tmpdt), "%Y%m%d", TM);
				if (len > 0) {
					strNcpy(q, tmpdt, freespace);
					q += strlen(q);
				}
				p++;
				break;
			case 'H': /* request hour */
				TM = localtime_r(&request->timestamp, &s_TM);
				len = strftime(tmpdt, sizeof(tmpdt), "%H", TM);
				if (len > 0) {
					strNcpy(q, tmpdt, freespace);
					q += strlen(q);
				}
				p++;
				break;
			case 'L': /* radlog_dir */
				strNcpy(q,radlog_dir,freespace-1);
				q += strlen(q);
				p++;
				break;
			case 'M': /* MTU */
				q += valuepair2str(q,freespace,pairfind(request->reply->vps,PW_FRAMED_MTU),PW_TYPE_INTEGER, func);
				p++;
				break;
			case 'R': /* radius_dir */
				strNcpy(q,radius_dir,freespace-1);
				q += strlen(q);
				p++;
				break;
			case 'S': /* request timestamp in SQL format*/
				TM = localtime_r(&request->timestamp, &s_TM);
				len = strftime(tmpdt, sizeof(tmpdt), "%Y-%m-%d %H:%M:%S", TM);
				if (len > 0) {
					strNcpy(q, tmpdt, freespace);
					q += strlen(q);
				}
				p++;
				break;
			case 'T': /* request timestamp */
				TM = localtime_r(&request->timestamp, &s_TM);
				len = strftime(tmpdt, sizeof(tmpdt), "%Y-%m-%d-%H.%M.%S.000000", TM);
				if (len > 0) {
					strNcpy(q, tmpdt, freespace);
					q += strlen(q);
				}
				p++;
				break;
			case 'U': /* Stripped User name */
				q += valuepair2str(q,freespace,pairfind(request->packet->vps,PW_STRIPPED_USER_NAME),PW_TYPE_STRING, func);
				p++;
				break;
			case 'V': /* Request-Authenticator */
				if (request->packet->verified)
					strNcpy(q,"Verified",freespace-1);
				else
					strNcpy(q,"None",freespace-1);
				q += strlen(q);
				p++;
				break;
			case 'Y': /* request year */
				TM = localtime_r(&request->timestamp, &s_TM);
				len = strftime(tmpdt, sizeof(tmpdt), "%Y", TM);
				if (len > 0) {
					strNcpy(q, tmpdt, freespace);
					q += strlen(q);
				}
				p++;
				break;
			case 'Z': /* Full request pairs except password */
				tmp = request->packet->vps;
				while (tmp && (freespace > 3)) {
					if (tmp->attribute != PW_PASSWORD) {
						*q++ = '\t';
						len = vp_prints(q, freespace - 2, tmp);
						q += len;
						freespace -= (len + 2);
						*q++ = '\n';
					}
					tmp = tmp->next;
				}
				p++;
				break;
			default:
				DEBUG2("WARNING: Unknown variable '%%%c': See 'doc/variables.txt'", *p);
				if (freespace > 2) {
					*q++ = '%';
					*q++ = *p++;
				}
				break;
		}
	}
	*q = '\0';

	DEBUG2("radius_xlat:  '%s'", out);

	return strlen(out);
}
