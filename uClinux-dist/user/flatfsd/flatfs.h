/*****************************************************************************/

/*
 *	flatfs.h -- support for flat FLASH file systems.
 *
 *	(C) Copyright 1999, Greg Ungerer (gerg@snapgear.com).
 *	(C) Copyright 2000, Lineo Inc. (www.lineo.com)
 *	(C) Copyright 2001-2002, SnapGear (www.snapgear.com)
 */

/*****************************************************************************/
#ifndef flatfs_h
#define flatfs_h
/*****************************************************************************/

/*
 * Hardwire the source and destination directories :-(
 */
#define	FILEFS		"/dev/flash/config"
#define	DEFAULTDIR	"/etc/default"
#define	SRCDIR		"/etc/config"
#define	DSTDIR		SRCDIR

#define FLATFSD_CONFIG	".flatfsd"

/*
 * Globals for file and byte count.
 */
extern int numfiles;
extern int numbytes;
extern int numdropped;

extern int flat_new(const char *dir);
extern int flat_clean(void);
extern int flat_filecount(char *configdir);
extern int flat_needinit(void);
extern int flat_requestinit(void);

#ifdef LOGGING
extern void vlogd(int bg, const char *cmd, const char *arg);
extern void logd(const char *cmd, const char *format, ...) __attribute__ ((format(printf, 2, 3)));
#else
static inline void vlogd(int bg, const char *cmd, const char *arg)
{
}

static void logd(const char *cmd, const char *format, ...) __attribute__ ((format(printf, 2, 3)));
static inline void logd(const char *cmd, const char *format, ...)
{
}
#endif

#define ERROR_CODE()	(-(__LINE__)) /* unique failure codes :-) */

/*****************************************************************************/
#endif
