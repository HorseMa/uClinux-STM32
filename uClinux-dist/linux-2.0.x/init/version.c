/*
 *  linux/version.c
 *
 *  Copyright (C) 1992  Theodore Ts'o
 *
 *  May be freely distributed as part of Linux.
 */

#include <linux/config.h>
#include <linux/utsname.h>
#include <linux/version.h>
#include <linux/compile.h>

/* make the "checkconfig" script happy: we really need to include config.h */
#ifdef CONFIG_BOGUS
#endif

#define version(a) Version_ ## a
#define version_string(a) version(a)

int version_string(LINUX_VERSION_CODE) = 0;

struct new_utsname system_utsname = {
	UTS_SYSNAME, UTS_NODENAME, UTS_RELEASE, UTS_VERSION,
	UTS_MACHINE, UTS_DOMAINNAME
};

const char *linux_banner = 
	UTS_SYSNAME " version " UTS_RELEASE " (" LINUX_COMPILE_BY "@"
	LINUX_COMPILE_HOST ") (" LINUX_COMPILER ") " UTS_VERSION "\n";
