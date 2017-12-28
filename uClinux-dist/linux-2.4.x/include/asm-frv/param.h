#ifndef _ASM_PARAM_H
#define _ASM_PARAM_H

#include <linux/config.h>


#ifndef HZ
#define HZ 100
#endif

#define EXEC_PAGESIZE	16384

#ifndef NGROUPS
#define NGROUPS		32
#endif

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	/* max length of hostname */

#ifdef __KERNEL__
#define CLOCKS_PER_SEC HZ
#endif /* __KERNEL__ */

#endif /* _ASM_PARAM_H */
