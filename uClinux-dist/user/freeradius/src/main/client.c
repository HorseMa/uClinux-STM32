/*
 * files.c	Read config files into memory.
 *
 * Version:     $Id: client.c,v 1.27 2004/04/07 16:02:05 aland Exp $
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
 * Copyright 2000  Miquel van Smoorenburg <miquels@cistron.nl>
 * Copyright 2000  Alan DeKok <aland@ox.org>
 */

static const char rcsid[] = "$Id: client.c,v 1.27 2004/04/07 16:02:05 aland Exp $";

#include "autoconf.h"
#include "libradius.h"

#include <sys/stat.h>

#ifdef HAVE_NETINET_IN_H
#	include <netinet/in.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <fcntl.h>

#include "radiusd.h"
#include "conffile.h"

/*
 *	Free a RADCLIENT list.
 */
void clients_free(RADCLIENT *cl)
{
	RADCLIENT *next;

	while(cl) {
		next = cl->next;
		free(cl);
		cl = next;
	}
}


/*
 *	Read the clients file.
 */
int read_clients_file(const char *file)
{
	FILE *fp;
	RADCLIENT *c;
	char buffer[256];
	char hostnm[256];
	char secret[256];
	char shortnm[256];
	uint32_t mask;
	int lineno = 0;
	char *p;
	int got_clients = FALSE;

	clients_free(mainconfig.clients);
	mainconfig.clients = NULL;

	if ((fp = fopen(file, "r")) == NULL) {
		/* The clients file is no longer required.  All configuration
		   information is read from radiusd.conf and friends.  If
		   clients exists it will be used, but if it doesn't no harm
		   done. */
		return 0;
	}

	while(fgets(buffer, 256, fp) != NULL) {
		lineno++;
		if (!feof(fp) && (strchr(buffer, '\n') == NULL)) {
			radlog(L_ERR, "%s[%d]: line too long", file, lineno);
			return -1;
		}

		/*
		 *	Skip whitespace.
		 */
		p = buffer;
		while (*p &&
				((*p == ' ') || (*p == '\t')))
			p++;

		/*
		 *	Skip comments and blank lines.
		 */
		if ((*p == '#') || (*p == '\n') || (*p == '\r'))
			continue;

		if (!getword(&p, hostnm, sizeof(hostnm)) ||
				!getword(&p, secret, sizeof(secret))) {
			radlog(L_ERR, "%s[%d]: unexpected end of line",
					file, lineno);
			return -1;
		}

		(void)getword(&p, shortnm, sizeof(shortnm));

		/*
		 *	Look for a mask in the hostname
		 */
		p = strchr(hostnm, '/');
		mask = ~0;

		if (p) {
			int mask_length;

			*p = '\0';
			p++;

			mask_length = atoi(p);
			if ((mask_length < 0) || (mask_length > 32)) {
				radlog(L_ERR, "%s[%d]: Invalid value '%s' for IP network mask.",
				       file, lineno, p);
				return -1;
			}

			if (mask_length == 0) {
				mask = 0;
			} else {
				mask = ~0 << (32 - mask_length);
			}
		}

		/*
		 *	Double-check lengths to be sure they're sane
		 */
		if (strlen(hostnm) >= sizeof(c->longname)) {
			radlog(L_ERR, "%s[%d]: host name of length %d is greater than the allowed maximum of %d.",
			       file, lineno,
			       (int) strlen(hostnm),
			       (int) sizeof(c->longname) - 1);
			return -1;
		}
		if (strlen(secret) >= sizeof(c->secret)) {
			radlog(L_ERR, "%s[%d]: secret of length %d is greater than the allowed maximum of %d.",
			       file, lineno,
			       (int) strlen(secret),
			       (int) sizeof(c->secret) - 1);
			return -1;
		}
		if (strlen(shortnm) > sizeof(c->shortname)) {
			radlog(L_ERR, "%s[%d]: short name of length %d is greater than the allowed maximum of %d.",
			       file, lineno,
			       (int) strlen(shortnm),
			       (int) sizeof(c->shortname) - 1);
			return -1;
		}

		/*
		 *	It should be OK now, let's create the buffer.
		 */
		got_clients = TRUE;
		c = rad_malloc(sizeof(RADCLIENT));
		memset(c, 0, sizeof(*c));

		c->ipaddr = ip_getaddr(hostnm);
		if (c->ipaddr == INADDR_NONE) {
			radlog(L_CONS|L_ERR, "%s[%d]: Failed to look up hostname %s",
					file, lineno, hostnm);
			return -1;
		}
		c->netmask = htonl(mask);
		c->ipaddr &= c->netmask; /* addr & mask are in network order */

		strcpy((char *)c->secret, secret);
		strcpy(c->shortname, shortnm);

		/*
		 *	Only do DNS lookups for machines.  Just print
		 *	the network as the long name.
		 */
		if ((~mask) == 0) {
			NAS *nas;
			ip_hostname(c->longname, sizeof(c->longname), c->ipaddr);

			/*
			 *	Pull information over from the NAS.
			 */
			nas = nas_find(c->ipaddr);
			if (nas) {
				/*
				 *	No short name in the 'clients' file,
				 *	try copying one over from the
				 *	'naslist' file.
				 */
				if (c->shortname[0] == '\0') {
					strcpy(c->shortname, nas->shortname);
				}

				/*
				 *  Copy the nastype over, too.
				 */
				strcpy(c->nastype, nas->nastype);
			}
		} else {
			hostnm[strlen(hostnm)] = '/';
			strNcpy(c->longname, hostnm, sizeof(c->longname));
		}

		c->next = mainconfig.clients;
		mainconfig.clients = c;
	}
	fclose(fp);

	if (got_clients) {
		radlog(L_INFO, "Using deprecated clients file.  Support for this will go away soon.");
	}

	return 0;
}


/*
 *	Find a client in the RADCLIENTS list.
 */
RADCLIENT *client_find(uint32_t ipaddr)
{
	RADCLIENT *cl;
	RADCLIENT *match = NULL;

	for (cl = mainconfig.clients; cl; cl = cl->next) {
		if ((ipaddr & cl->netmask) == cl->ipaddr) {
			if ((!match) ||
			    (ntohl(cl->netmask) > ntohl(match->netmask))) {
				match = cl;
			}
		}
	}

	return match;
}

/*
 *	Walk the RADCLIENT list displaying the clients.  This function
 *	is for debugging purposes.
 */
void client_walk(void)
{
	RADCLIENT *cl;
	char host_ipaddr[16];

	for (cl = mainconfig.clients; cl != NULL; cl = cl->next)
		radlog(L_ERR, "client: client_walk: %s\n",
				ip_ntoa(host_ipaddr, cl->ipaddr));
}

/*
 *	Find the name of a client (prefer short name).
 */
const char *client_name(uint32_t ipaddr)
{
	/* We don't call this unless we should know about the client. */
	RADCLIENT *cl;
	char host_ipaddr[16];

	if ((cl = client_find(ipaddr)) != NULL) {
		if (cl->shortname[0])
			return cl->shortname;
		else
			return cl->longname;
	}

	/*
	 * this isn't normally reachable, but if a loggable event happens just
	 * after a client list change and a HUP, then we may not know this
	 * information any more.
	 *
	 * If you see lots of these, then there's something wrong.
	 */
	radlog(L_ERR, "Trying to look up name of unknown client %s.\n",
	       ip_ntoa(host_ipaddr, ipaddr));

	return "UNKNOWN-CLIENT";
}
