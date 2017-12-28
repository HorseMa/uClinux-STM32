/* current.h: current task pointer
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells <dhowells@redhat.com>
 */

#ifndef _ASM_CURRENT_H
#define _ASM_CURRENT_H

#ifndef __ASSEMBLY__

/*
 * dedicate GR29 to keeping the current task pointer
 */
register struct task_struct *current asm("gr29");

#define get_current() current

#else

#define CURRENT gr29

#endif

#endif /* _ASM_CURRENT_H */
