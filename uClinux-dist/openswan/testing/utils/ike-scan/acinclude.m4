dnl $Id: acinclude.m4,v 1.1.1.1 2005/01/13 18:44:53 mcr Exp $
dnl ike-scan autoconf macros

dnl	AC_NTA_CHECK_TYPE -- See if a type exists using reasonable includes
dnl
dnl	Although there is a standard macro AC_CHECK_TYPE, we can't always
dnl	use this because it doesn't include enough header files.
dnl
AC_DEFUN([AC_NTA_CHECK_TYPE],
   [AC_MSG_CHECKING([for $1 using $CC])
   AC_CACHE_VAL(ac_cv_nta_have_$1,
	AC_TRY_COMPILE([
#	include "confdefs.h"
#	include <stdio.h>
#	if HAVE_SYS_TYPES_H
#	 include <sys/types.h>
#	endif
#	if HAVE_SYS_STAT_H
#	 include <sys/stat.h>
#	endif
#	ifdef STDC_HEADERS
#	 include <stdlib.h>
#	 include <stddef.h>
#	endif
#	if HAVE_INTTYPES_H
#	 include <inttypes.h>
#	else
#	 if HAVE_STDINT_H
#	  include <stdint.h>
#	 endif
#	endif
#	if HAVE_UNISTD_H
#	 include <unistd.h>
#	endif
#	ifdef HAVE_ARPA_INET_H
#	 include <arpa/inet.h>
#	endif
#	ifdef HAVE_NETDB_H
#	 include <netdb.h>
#	endif
#	ifdef HAVE_NETINET_IN_H
#	 include <netinet/in.h>
#	endif
#	ifdef SYS_SOCKET_H
#	 include <sys/socket.h>
#	endif
	],
	[$1 i],
	ac_cv_nta_have_$1=yes,
	ac_cv_nta_have_$1=no))
   AC_MSG_RESULT($ac_cv_nta_have_$1)
   if test $ac_cv_nta_have_$1 = no ; then
	   AC_DEFINE($1, $2, [Define to required type if we don't have $1])
   fi])

dnl	AC_NTA_NET_SIZE_T -- Determine type of 3rd argument to accept
dnl
dnl	This type is normally socklen_t but is sometimes size_t or int instead.
dnl	We try, in order: socklen_t, int, size_t until we find one that compiles
dnl
AC_DEFUN([AC_NTA_NET_SIZE_T],
   [AC_MSG_CHECKING([for socklen_t or equivalent using $CC])
   ac_nta_net_size_t=no
   AC_TRY_COMPILE([
#	include "confdefs.h"
#	include <sys/types.h>
#	ifdef HAVE_SYS_SOCKET_H
#	include <sys/socket.h>
#	endif],
	[int s;
	struct sockaddr addr;
	socklen_t addrlen;
	int result;
	result=accept(s, &addr, &addrlen)],
	   ac_nta_net_size_t=socklen_t,ac_nta_net_size_t=no)
   if test $ac_nta_net_size_t = no; then
   AC_TRY_COMPILE([
#	include "confdefs.h"
#	include <sys/types.h>
#	ifdef HAVE_SYS_SOCKET_H
#	include <sys/socket.h>
#	endif],
	[int s;
	struct sockaddr addr;
	int addrlen;
	int result;
	result=accept(s, &addr, &addrlen)],
	ac_nta_net_size_t=int,ac_nta_net_size_t=no)
   fi
   if test $ac_nta_net_size_t = no; then
   AC_TRY_COMPILE([
#	include "confdefs.h"
#	include <sys/types.h>
#	ifdef HAVE_SYS_SOCKET_H
#	include <sys/socket.h>
#	endif],
	[int s;
	struct sockaddr addr;
	size_t addrlen;
	int result;
	result=accept(s, &addr, &addrlen)],
	ac_nta_net_size_t=size_t,ac_nta_net_size_t=no)
   fi
   if test $ac_nta_net_size_t = no; then
      AC_MSG_ERROR([Cannot find acceptable type for 3rd arg to accept()])
   else
      AC_MSG_RESULT($ac_nta_net_size_t)
      AC_DEFINE_UNQUOTED(NET_SIZE_T, $ac_nta_net_size_t, [Define required type for 3rd arg to accept()])
   fi
   ])

dnl PGAC_TYPE_64BIT_INT(TYPE)
dnl -------------------------
dnl Check if TYPE is a working 64 bit integer type. Set HAVE_TYPE_64 to
dnl yes or no respectively, and define HAVE_TYPE_64 if yes.
dnl
dnl This function comes from the Postgresql file:
dnl pgsql/config/c-compiler.m4,v 1.13
dnl
AC_DEFUN([PGAC_TYPE_64BIT_INT],
[define([Ac_define], [translit([have_$1_64], [a-z *], [A-Z_P])])dnl
define([Ac_cachevar], [translit([pgac_cv_type_$1_64], [ *], [_p])])dnl
AC_CACHE_CHECK([whether $1 is 64 bits], [Ac_cachevar],
[AC_TRY_RUN(
[typedef $1 int64;

/*
 * These are globals to discourage the compiler from folding all the
 * arithmetic tests down to compile-time constants.
 */
int64 a = 20000001;
int64 b = 40000005;

int does_int64_work()
{
  int64 c,d;

  if (sizeof(int64) != 8)
    return 0;			/* definitely not the right size */

  /* Do perfunctory checks to see if 64-bit arithmetic seems to work */
  c = a * b;
  d = (c + b) / b;
  if (d != a+1)
    return 0;
  return 1;
}
main() {
  exit(! does_int64_work());
}],
[Ac_cachevar=yes],
[Ac_cachevar=no],
[# If cross-compiling, check the size reported by the compiler and
# trust that the arithmetic works.
AC_COMPILE_IFELSE([AC_LANG_BOOL_COMPILE_TRY([], [sizeof($1) == 8])],
                  Ac_cachevar=yes,
                  Ac_cachevar=no)])])

Ac_define=$Ac_cachevar
if test x"$Ac_cachevar" = xyes ; then
  AC_DEFINE(Ac_define,, [Define to 1 if `]$1[' works and is 64 bits.])
fi
undefine([Ac_define])dnl
undefine([Ac_cachevar])dnl
])# PGAC_TYPE_64BIT_INT

dnl PGAC_FUNC_SNPRINTF_LONG_LONG_INT_FORMAT
dnl ---------------------------------------
dnl Determine which format snprintf uses for long long int.  We handle
dnl %lld, %qd, %I64d.  The result is in shell variable
dnl LONG_LONG_INT_FORMAT.
dnl
dnl MinGW uses '%I64d', though gcc throws an warning with -Wall,
dnl while '%lld' doesn't generate a warning, but doesn't work.
dnl
dnl This function comes from the Postgresql file:
dnl pgsql/config/c-library.m4,v 1.28
dnl
AC_DEFUN([PGAC_FUNC_SNPRINTF_LONG_LONG_INT_FORMAT],
[AC_MSG_CHECKING([snprintf format for long long int])
AC_CACHE_VAL(pgac_cv_snprintf_long_long_int_format,
[for pgac_format in '%lld' '%qd' '%I64d'; do
AC_TRY_RUN([#include <stdio.h>
typedef long long int int64;
#define INT64_FORMAT "$pgac_format"

int64 a = 20000001;
int64 b = 40000005;

int does_int64_snprintf_work()
{
  int64 c;
  char buf[100];

  if (sizeof(int64) != 8)
    return 0;			/* doesn't look like the right size */

  c = a * b;
  snprintf(buf, 100, INT64_FORMAT, c);
  if (strcmp(buf, "800000140000005") != 0)
    return 0;			/* either multiply or snprintf is busted */
  return 1;
}
main() {
  exit(! does_int64_snprintf_work());
}],
[pgac_cv_snprintf_long_long_int_format=$pgac_format; break],
[],
[pgac_cv_snprintf_long_long_int_format=cross; break])
done])dnl AC_CACHE_VAL

LONG_LONG_INT_FORMAT=''

case $pgac_cv_snprintf_long_long_int_format in
  cross) AC_MSG_RESULT([cannot test (not on host machine)]);;
  ?*)    AC_MSG_RESULT([$pgac_cv_snprintf_long_long_int_format])
         LONG_LONG_INT_FORMAT=$pgac_cv_snprintf_long_long_int_format;;
  *)     AC_MSG_RESULT(none);;
esac])# PGAC_FUNC_SNPRINTF_LONG_LONG_INT_FORMAT
