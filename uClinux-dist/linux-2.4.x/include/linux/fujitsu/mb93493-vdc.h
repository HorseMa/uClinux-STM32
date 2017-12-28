/* mb93493-vdc.h: Fujitsu MB93493
 *	Fujitsu FR400 Companion Chip VDC
 *
 *	Copyright (C) 2002  AXE,Inc.
 *	COPYRIGHT FUJITSU LIMITED 2002
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef _LINUX_FUJITSU_MB93493_VDC_H
#define _LINUX_FUJITSU_MB93493_VDC_H

#include <asm/ioctl.h>

#define FR400CC_VDC_DEBUG_IOCTL

#ifdef __KERNEL__
#define FR400CC_VDC_MINOR	244
#define FR400CC_VDC_VSYNC_MINOR	245
#endif	/* __KERNEL__ */

struct fr400vdc_read {
	int		type;
#define FR400CC_VDC_RTYPE_FRAME_FIN	0
#define FR400CC_VDC_RTYPE_VSYNC		1
#define FR400CC_VDC_RTYPE_ERR_UNDERFLOW	2

	int		count;
};

struct fr400vdc_config {
	int		pix_x;		/* 720 or 640 */
	int		pix_y;		/* 480 */
	int		pix_sz;		/* 2 */

	int		skipbf;		/* Skip bottom field(0:not skip / 1:skip) */
	int		buf_unit_sz;
	int		buf_num;
	int		stop_immediate;
	int		rd_count_buf_idx;

	/* VDC param ... */
	int		prm[13];
	int		rddl;
	int		hls, pal, cscv, dbls, r601, tfop, dsm;
  	int		dfp, die, enop, vsop, hsop, dsr, csron, dpf, dms;

	/* DMA param ... */
	int		dma_mode, dma_ats, dma_rs;
};

struct fr400cc_vdc_regio {
	unsigned	reg_offset;
	unsigned	value;
};

#define VDCIOCGCFG	_IOR('c', 1, struct fr400vdc_config)
#define VDCIOCSCFG	_IOW('c', 2, struct fr400vdc_config)
#define VDCIOCSTART	_IOW('c', 3, int)
#define VDCIOCSTOP	_IO('c', 4)
#define VDCIOCSDAT	_IOW('c', 5, int)
#define VDCIOCTEST	_IOW('c', 10, int)

#define VDCIOCVSSTART	_IO('c', 0x20)
#define VDCIOCVSSTOP	_IO('c', 0x21)
#define VDCIOCGREGIO	_IOR('c', 0x28, struct fr400cc_vdc_regio)
#define VDCIOCSREGIO	_IOR('c', 0x29, struct fr400cc_vdc_regio)

#ifdef FR400CC_VDC_DEBUG_IOCTL
struct __fr400cc_vdc_regio {
	/* register number:
	 *   VDC_RCH | VDC_RCT1 |
	 *   VDC_RCT2 | VDC_RHTC |
	 *   VDC_RHFP | VDC_RVTC |
	 *   VDC_RVFP | VDC_RC |
	 *   VDC_RCK | VDC_RHIP |
	 *   VDC_RS | VDC_BCI | VDC_TPO
	 */
	unsigned	reg_no;		/* W */
	unsigned	value;		/* R/W */
};
#define __VDCIOCGREGIO	_IOR('c', 0x80, struct __fr400cc_vdc_regio)
#define __VDCIOCSREGIO	_IOR('c', 0x81, struct __fr400cc_vdc_regio)
#endif

#endif /* _LINUX_FUJITSU_MB93493_VDC_H */
