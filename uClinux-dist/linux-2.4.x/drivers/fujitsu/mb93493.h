/* mb93493.h: Fujitsu MB93493 local definitions
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _FUJITSU_MB93493_H
#define _FUJITSU_MB93493_H

/* DMA IDs presented to drivers */
enum mb93493_dma_source {
	DMA_MB93493_VDC			= 0,
	DMA_MB93493_VCC			= 1,
	DMA_MB93493_I2S_OUT		= 2,
	DMA_MB93493_USB_2		= 3,
	DMA_MB93493_USB_1		= 4,
	DMA_MB93493_I2S_IN		= 6,
	DMA_MB93493_PCMCIA		= 7,
};

extern int mb93493_dma_open(const char *devname,
			    enum mb93493_dma_source dmasource,
			    int dmacap,
			    dma_irq_handler_t handler,
			    unsigned long irq_flags,
			    void *data);

extern void mb93493_dma_close(int dma);

extern int mb93493_i2s_open(struct inode *inode, struct file *file);

extern int mb93493_i2s_release(struct inode *inode, struct file *file);

extern int mb93493_i2s_ioctl_mem(struct inode *inode, struct file *file,
				 unsigned int cmd, unsigned long arg,
				 int user_mem);

extern ssize_t mb93493_i2s_read_data(struct file *file, char *buffer,
				     size_t count, loff_t *ppos);

extern ssize_t mb93493_i2s_write(struct file *file, const char *buffer,
				 size_t count, loff_t *ppos);

#endif /* _FUJITSU_MB93493_H */
