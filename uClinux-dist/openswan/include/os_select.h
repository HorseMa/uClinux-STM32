#ifndef _OS_SELECT_H_
#define _OS_SELECT_H_ 1
/*
 * Overlay the system select call to handle many more FD's than
 * an fd_set can hold.
 * David McCullough <david_mccullough@securecomputing.com>
 */

#include <sys/select.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * allow build system to override the limit easily
 */

#ifndef OS_FD_SETSIZE
#define OS_FD_SETSIZE	8192
#endif

#define OS_NFDBITS   (8 * sizeof (long int))
#define OS_FDELT(d)  ((d) / OS_NFDBITS)
#define OS_FDMASK(d) ((long int) 1 << ((d) % OS_NFDBITS))
#define OS_FD_SETCOUNT	((OS_FD_SETSIZE + OS_NFDBITS - 1) / OS_NFDBITS)

typedef struct {
	long int	__osfds_bits[OS_FD_SETCOUNT];
} os_fd_set;

#define OS_FDS_BITS(set) ((set)->__osfds_bits)

#define OS_FD_ZERO(set) \
	do { \
		unsigned int __i; \
		os_fd_set *__arr = (set); \
		for (__i = 0; __i < OS_FD_SETCOUNT; __i++) \
			OS_FDS_BITS (__arr)[__i] = 0; \
	} while(0)

#define OS_FD_SET(d, s)     (OS_FDS_BITS (s)[OS_FDELT(d)] |= OS_FDMASK(d))
#define OS_FD_CLR(d, s)     (OS_FDS_BITS (s)[OS_FDELT(d)] &= ~OS_FDMASK(d))
#define OS_FD_ISSET(d, s)   ((OS_FDS_BITS (s)[OS_FDELT(d)] & OS_FDMASK(d)) != 0)

#define os_select(max, r, f, e, t) \
		select(max, (fd_set *)(r), (fd_set *)(f), (fd_set *)(e), t)

#endif /* _OS_SELECT_H_ */
