/*
 *  linux/include/asm-arm/arch-lpc28xx/dma.h
 *
 *  lpc28xx DMA header file
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#define DMAID_MMC_SINGLE	1
#define DMAID_MMC_BURST	2
#define DMAID_UART_RX		3
#define DMAID_UART_TX		4
#define DMAID_I2C			5
#define DMAID_SAO1_A		6
#define DMAID_SAO1_B		7
#define DMAID_SAO2_A		8
#define DMAID_SAO3_B		9
#define DMAID_SAI1_A		10
#define DMAID_SAI1_B		11
#define DMAID_SAI4_A		16
#define DMAID_SAI4_B		17
#define DMAID_LCD_OUTPUT	18
#define DMAID_MPMC_A19	19
#define DMAID_MPMC_A17	20

struct lpc28xx_dma_entry{
	u32 src_addr;
	u32 dest_addr;
	u32 length;
	u32 config;
	struct lpc28xx_dma_entry* next;
};


#endif				/* _ASM_ARCH_DMA_H */
