/*
 * include/asm-armnommu/arch-isl3893/rtc.h
 */
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

#ifndef __ASM_ARCH_RTC_H
#define __ASM_ARCH_RTC_H

/* Register values */

/* - Data */
#define	RTCData				(0x0)
#define	RTCDataMask			(0xffffffff)
#define	RTCDataTestMask			(0xfffffff0)
#define	RTCDataAccessType		(RW)
#define	RTCDataInitialValue		(0x0)

/* - Match */
#define	RTCMatch			(0x4)
#define	RTCMatchMask			(0xffffffff)
#define	RTCMatchAccessType		(RW)
#define	RTCMatchInitialValue		(0x0)
#define	RTCMatchTestMask		(0xffffffff)

/* - IntControl */
#define	RTCIntControl			(0x8)
#define	RTCIntControlBypassClkDivider	(0x10)
#define	RTCIntControlBypass1Mhz		(0x8)
#define	RTCIntControlSTBARK		(0x4)
#define	RTCIntControlTICK		(0x2)
#define	RTCIntControlEVENT		(0x1)
#define	RTCIntControlMask		(0x1f)
#define	RTCIntControlTestMask		(0x1b)
#define	RTCIntControlInitialValue	(0x0)
#define	RTCIntControlAccessType		(RW)

/* - IntStatus */
#define	RTCIntStatus			(0xc)
#define	RTCIntStatusSTBARK		(0x4)
#define	RTCIntStatusTICK		(0x2)
#define	RTCIntStatusEVENT		(0x1)
#define	RTCIntStatusMask		(0x7)
#define	RTCIntStatusAccessType		(NO_TEST)
#define	RTCIntStatusInitialValue	(0x0)
#define	RTCIntStatusTestMask		(0x7)

/* - IntClear */
#define	RTCIntClear			(0x14)
#define	RTCIntClearSTBARK		(0x4)
#define	RTCIntClearTICK			(0x2)
#define	RTCIntClearEVENT		(0x1)
#define	RTCIntClearMask			(0x7)
#define	RTCIntClearAccessType		(WO)
#define	RTCIntClearInitialValue		(0x0)
#define	RTCIntClearTestMask		(0x7)

/* Instance values */
#define aRTCData			 0x280
#define aRTCMatch			 0x284
#define aRTCIntControl			 0x288
#define aRTCIntStatus			 0x290
#define aRTCIntClear			 0x294

/* C struct view */

#ifndef __ASSEMBLER__

typedef struct s_RTC {
 unsigned Data; /* @0 */
 unsigned Match; /* @4 */
 unsigned IntControl; /* @8 */
 unsigned fill0;
 unsigned IntStatus; /* @16 */
 unsigned IntClear; /* @20 */
} s_RTC;

#endif /* !__ASSEMBLER__ */

#define uRTC ((volatile struct s_RTC *) 0xc0000280)

#endif /* __ASM_ARCH_RTC_H */
