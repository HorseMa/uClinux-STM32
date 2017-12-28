/*  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __ASM_ARCH_BLOBCALLS_H__
#define __ASM_ARCH_BLOBCALLS_H__

/* Some MVC header files use this define to specify the MVC version 2 interface */
#define BLOB_V2
#define MVC2_2

#include <linux/kernel.h>
#include <blobv2.h>
#include <bloboid.h>
#include <blobarch.h>

/* For the boot struct */
#define CONF_3893
#include <bootdbg.h>

/*
 * This header file needs to be included for making calls to the MVC. It
 * will automatically include the correct MVC header files.
 *
 * The macros below specify the coprocessor interface of the MVC. There are
 * two types of calls: xx_to_mvc and xx_from_mvc. The xx can be _blobcall
 * for calls to the blobdevice or _devcall for calls to the regular devices
 * (WLAN, HOSTDS, DEBUG, etcetera).
 * There are different macros for functions with no arguments (..mvc0) and
 * one argument (..mvc1). There is also a special case for functions that
 * have no return value (.._noret).
 * The actual interface is defined in /arch/armnommu/mach-<machine>/blobcalls.c
 */

/*
 * These instructions share a common format, with the following assembly syntax:
 * 	MRC/MCR{<cond>} p<cp#>>, <opcode_1>, (C)Rd, CRn, CRm, <opcode_2>
 * The generic usage of the instruction fields is :
 * <cp#>	MVC co-processor ID (11),
 * <opcode_1>	Reserved (0),
 * <Rd>		ARM register with a pointer to a message dependent structure,
 *		the ARM register argument may be any register (r0 - r14), except for r15.
 * <CRn>	Device number (0 - 15),
 * <CRm>	Messager number (0 - 15),
 * <opcode_2>	Reserved (0).
 */


/*
 * Some magic to let the preprocessor stringify the coprocessor registers
 */
#define __str(x)	#x
#define __cat( x, y )	__str(x ## y)
#define __creg( nr )	__cat( c, nr )

#define _blobcall_to_mvc0(type, func, msg)						\
static inline type func(void)								\
{											\
	msg_blob blob;									\
	__asm__ __volatile__ (								\
		"mcr	p11, 0, %0, " __creg(DEVICE_BLOB) ", " __creg(msg) ", 0"	\
		: : "r" (&blob));							\
	return (type)(blob.error);							\
}

#define _blobcall_to_mvc1(type, func, msg, type1)					\
static inline type func(type1 arg1)							\
{											\
	__asm__ __volatile__ (								\
		"mcr	p11, 0, %0, " __creg(DEVICE_BLOB) ", " __creg(msg) ", 0"	\
		: : "r" (arg1));							\
	return (type)(arg1->blob.error);						\
}

#define _blobcall_from_mvc0(type, func, msg)						\
static inline type func(void)								\
{											\
	type __res;									\
	__asm__ __volatile__ (								\
		"mrc	p11, 0, %0, " __creg(DEVICE_BLOB) ", " __creg(msg) ", 0"	\
		: "=r" (__res):);							\
	return __res;									\
}

#define _devcall_to_mvc0(type, func, msg)						\
static inline type func(int device)							\
{											\
	msg_blob blob;									\
											\
	blob.device = device;								\
	__asm__ __volatile__ (								\
		"mcr	p11, 0, %0, " __creg(DEVICE_GENERIC) ", " __creg(msg) ", 0"	\
		: : "r" (&blob));							\
											\
	return (type)(blob.error);							\
}

#define _devcall_to_mvc1(type, func, msg, type1)					\
static inline type func(int device, type1 arg1)						\
{											\
	arg1->blob.device = device;							\
	__asm__ __volatile__ (								\
		"mcr	p11, 0, %0, " __creg(DEVICE_GENERIC) ", " __creg(msg) ", 0"	\
		: : "r" (arg1));							\
											\
	return (type)(arg1->blob.error);						\
}


/*
 * Definition of the MVC interface calls
 */

/*
 * Stubs for the blob
 */
_blobcall_to_mvc1(int, blob_init, MSG_INIT, struct msg_init *)
_blobcall_from_mvc0(int, blob_abort, MSG_ABORT)
_blobcall_from_mvc0(int, blob_reset, MSG_RESET)
_blobcall_from_mvc0(int, blob_sleep, MSG_SLEEP)
_blobcall_to_mvc1(int, blob_conf, MSG_CONF, struct msg_conf *)
_blobcall_from_mvc0(int, blob_watchdog, MSG_WATCHDOG)

_blobcall_to_mvc0(int, blob_reqs, MSG_REQS)
_blobcall_to_mvc0(int, blob_service, MSG_SERVICE)
_blobcall_to_mvc1(int, blob_trap, MSG_TRAP, struct msg_conf *)


/*
 * Stubs for other devices
 */
_devcall_to_mvc1(int, dev_start, MSG_START, struct msg_start*)
_devcall_to_mvc0(int, dev_run, MSG_RUN)
_devcall_to_mvc0(int, dev_halt, MSG_HALT)
_devcall_to_mvc0(int, dev_stop, MSG_STOP)
_devcall_to_mvc1(int, dev_conf, MSG_CONF, struct msg_conf *)
_devcall_to_mvc1(int, dev_setup, MSG_SETUP, struct msg_setup *)

_devcall_to_mvc1(int, dev_frame_add, MSG_FRAME_ADD, struct msg_frame *)
_devcall_to_mvc1(int, dev_frame_tx, MSG_FRAME_TX, struct msg_frame *)

_devcall_to_mvc1(int, dev_write, MSG_WRITE, struct msg_data *)

_devcall_to_mvc0(int, dev_state, MSG_STATE)
_devcall_to_mvc0(int, dev_reqs, MSG_REQS)
_devcall_to_mvc0(int, dev_service, MSG_SERVICE)

_devcall_to_mvc0(int, dev_watchdog, MSG_WATCHDOG)

/* dev_trap changed between MVCv1 and MVCv2 */
_devcall_to_mvc1(int, dev_trap, MSG_TRAP, struct msg_conf *)

_devcall_to_mvc0(struct msg_frame *, dev_frame_return, MSG_FRAME_RETURN)
_devcall_to_mvc0(struct msg_frame *, dev_frame_rx, MSG_FRAME_RX)

_devcall_to_mvc0(struct msg_data *, dev_read, MSG_READ)

extern struct boot_struct *bootstruct;

static inline unsigned char *blobtool_find_element(int id, int skip)
{
        unsigned char *p = (unsigned char *)bootstruct->bis;

        while( *((short*)p) != id || skip > 0 )
        {
                if( (*((short*)p)) == (short)BIS_END )
                        return 0L;

                if( *((short*)p) == id )
                        skip--;

                p += 4 + *((short*)(p+2));
        }

        return p + 4;
}

#endif /* __ASM_ARCH_BLOBCALLS_H__ */