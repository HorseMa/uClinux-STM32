/*
 * The IKE Scanner (ike-scan) is Copyright (C) 2003-2005 Roy Hills,
 * NTA Monitor Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * If this license is unacceptable to you, I may be willing to negotiate
 * alternative licenses (contact ike-scan@nta-monitor.com).
 *
 * You are encouraged to send comments, improvements or suggestions to
 * me at ike-scan@nta-monitor.com.
 *
 * $Id: error.c,v 1.1.1.1 2005/01/13 18:45:14 mcr Exp $
 *
 * error.c -- error routines for IKE Scanner (ike-scan)
 *
 * Author:	Roy Hills
 * Date:	1 December 2001
 *
 */

#include "ike-scan.h"

static char rcsid[] = "$Id: error.c,v 1.1.1.1 2005/01/13 18:45:14 mcr Exp $"; /* RCS ID for ident(1) */

int daemon_proc;	/* Non-zero if process is a daemon */

/*
 *	Function to handle fatal system call errors.
 */
void
err_sys(const char *fmt,...) {
   va_list ap;

   va_start(ap, fmt);
   err_print(1, 0, fmt, ap);
   va_end(ap);
   exit(EXIT_FAILURE);
}

/*
 *	Function to handle non-fatal system call errors.
 */
void
warn_sys(const char *fmt,...) {
   va_list ap;

   va_start(ap, fmt);
   err_print(1, 0, fmt, ap);
   va_end(ap);
}

/*
 *	Function to handle fatal errors not from system calls.
 */
void
err_msg(const char *fmt,...) {
   va_list ap;

   va_start(ap, fmt);
   err_print(0, 0, fmt, ap);
   va_end(ap);
   exit(EXIT_FAILURE);
}

/*
 *	Function to handle non-fatal errors not from system calls.
 */
void
warn_msg(const char *fmt,...) {
   va_list ap;

   va_start(ap, fmt);
   err_print(0, 0, fmt, ap);
   va_end(ap);
}

/*
 *	Function to handle informational syslog messages
 */
void
info_syslog(const char *fmt,...) {
   va_list ap;

   va_start(ap, fmt);
   err_print(0, LOG_INFO, fmt, ap);
   va_end(ap);
}

/*
 *	General error printing function used by all the above
 *	functions.
 */
void
err_print (int errnoflag, int level, const char *fmt, va_list ap) {
   int errno_save;
   size_t n;
   char buf[MAXLINE];

   errno_save=errno;

   vsnprintf(buf, MAXLINE, fmt, ap);
   n=strlen(buf);
   if (errnoflag)
     snprintf(buf+n, MAXLINE-n, ": %s", strerror(errno_save));
   strcat(buf, "\n");

   if (level != 0) {
      syslog(level, "%s", buf);
   } else {
      fflush(stdout);	/* In case stdout and stderr are the same */
      fputs(buf, stderr);
      fflush(stderr);
   }
}

void
error_use_rcsid(void) {
   fprintf(stderr, "%s\n", rcsid);       /* Use rcsid to stop compiler optimising away */
}
