/*
 *  linux/include/asm-arm/arch-stm3210e_eval/dma.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/interrupt.h>
 
 /* we have 7 channels on DMA1 and 5 channels on DMA2*/

 
#define MAX_DMA_CHANNELS	1
//(7 + 5) 

extern int set_dma_callback(int dma_ch, irq_handler_t sdh_dma_irq, void *host);