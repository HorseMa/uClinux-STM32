/*
 * missing.c	Replacements for functions that are or can be
 *		missing on some platforms.
 *
 * Version:	$Id: missing.c,v 1.12.4.1 2006/03/15 15:37:56 nbk Exp $
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Copyright 2000  The FreeRADIUS server project
 */

static const char rcsid[] = "$Id: missing.c,v 1.12.4.1 2006/03/15 15:37:56 nbk Exp $";

#include	"autoconf.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<ctype.h>

#include	"libradius.h"
#include	"missing.h"

#ifndef HAVE_CRYPT
char *crypt(char *key, char *salt)
{
	/*log(L_ERR, "crypt() called but not implemented");*/
	return "____fnord____";
}
#endif

#ifndef HAVE_STRNCASECMP
int strncasecmp(char *s1, char *s2, int n)
{
	int		dif;
	unsigned char	*p1, *p2;
	int		c1, c2;

	p1 = (unsigned char *)s1;
	p2 = (unsigned char *)s2;
	dif = 0;

	while (n != 0) {
		if (*p1 == 0 && *p2 == 0)
			break;
		c1 = *p1;
		c2 = *p2;

		if (islower(c1)) c1 = toupper(c1);
		if (islower(c2)) c2 = toupper(c2);

		if ((dif = c1 - c2) != 0)
			break;
		p1++;
		p2++;
		n--;
	}
	return dif;
}
#endif

#ifndef HAVE_STRCASECMP
int strcasecmp(char *s1, char *s2)
{
	int		l1, l2;

	l1 = strlen(s1);
	l2 = strlen(s2);
	if (l2 > l1) l1 = l2;

	return strncasecmp(s1, s2, l1);
}
#endif

#ifndef HAVE_INET_ATON
int inet_aton(char *cp, struct in_addr *inp)
{
	int	a1, a2, a3, a4;

	if (sscanf(cp, "%d.%d.%d.%d", &a1, &a2, &a3, &a4) != 4)
		return 0;

	inp->s_addr = htonl((a1 << 24) + (a2 << 16) + (a3 << 8) + a4);
	return 1;
}
#endif

#ifndef HAVE_GETHOSTNAME
int gethostname(char *name, int len)
{
	char		*h;

	h = getenv("HOSTNAME");
	if (!h || (strlen(h) + 1 > len))
		return -1;
	strcpy(name, h);
	return 0;
}
#endif

#ifndef HAVE_STRSEP
/*
 *	Get next token from string *stringp, where tokens are
 *	possibly-empty strings separated by characters from delim.
 *
 *	Writes NULs into the string at *stringp to end tokens.
 *	delim need not remain constant from call to call.  On
 *	return, *stringp points past the last NUL written (if there
 *	might be further tokens), or is NULL (if there are
 *	definitely no more tokens).
 *
 *	If *stringp is NULL, strsep returns NULL.
 */
char *
strsep(char **stringp, const char *delim)
{
	char *s;
	const char *spanp;
	int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);

	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return (tok);
			}
		} while (sc != 0);
	}

	return NULL;		/* NOTREACHED, but the compiler complains */
}
#endif

#ifndef HAVE_LOCALTIME_R
/*
 *	We use localtime_r() by default in the server.
 *
 *	For systems which do NOT have localtime_r(), we make the
 *	assumption that localtime() is re-entrant, and returns a
 *	per-thread data structure.
 *
 *	Even if localtime is NOT re-entrant, this function will
 *	lower the possibility of race conditions.
 */
struct tm *localtime_r(const time_t *l_clock, struct tm *result)
{
  memcpy(result, localtime(l_clock), sizeof(*result));

  return result;
}
#endif

#ifndef HAVE_CTIME_R
/*
 *	We use ctime_r() by default in the server.
 *
 *	For systems which do NOT have ctime_r(), we make the
 *	assumption that ctime() is re-entrant, and returns a
 *	per-thread data structure.
 *
 *	Even if ctime is NOT re-entrant, this function will
 *	lower the possibility of race conditions.
 */
char *ctime_r(const time_t *l_clock, char *l_buf)
{
  strcpy(l_buf, ctime(l_clock));

  return l_buf;
}
#endif

#ifndef HAVE_GMTIME_R
/*
 *	We use gmtime_r() by default in the server.
 *
 *	For systems which do NOT have gmtime_r(), we make the
 *	assumption that gmtime() is re-entrant, and returns a
 *	per-thread data structure.
 *
 *	Even if gmtime is NOT re-entrant, this function will
 *	lower the possibility of race conditions.
 */
struct tm *gmtime_r(const time_t *l_clock, struct tm *result)
{
  memcpy(result, gmtime(l_clock), sizeof(*result));

  return result;
}
#endif
