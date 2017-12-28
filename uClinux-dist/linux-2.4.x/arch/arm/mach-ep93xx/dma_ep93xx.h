/*****************************************************************************
 *
 * arch/arm/mach-ep93xx/dma_ep93xx.h
 *
 * DESCRIPTION:    93XX DMA controller API private defintions.
 *
 * Copyright Cirrus Logic Corporation, 2003.  All rights reserved
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
 *
 ****************************************************************************/
#ifndef _EP93XX_DMA_H_
#define _EP93XX_DMA_H_

#define MAX_EP93XX_DMA_BUFFERS      128

#ifndef TRUE
#define TRUE                        1
#endif

#ifndef FALSE
#define FALSE                       0
#endif

#ifndef NULL
#define NULL                        0
#endif

/*****************************************************************************
 *
 * DMA buffer structure type.
 *
 ****************************************************************************/
typedef struct ep93xx_dma_buffer_s
{
    unsigned int    source;     /* buffer physical source address.          */
    unsigned int    dest;       /* buffer physical destination address,     */
                                /* only used with the 2 M2M channels.       */
    unsigned int    size;       /* buffer size in bytes                     */
    unsigned int    last;       /* 1 if this is the last buffer             */
                                /* in this transaction.  If 1,              */
                                /* disable the NFBint so we aren't          */
                                /* interrupted for another buffer           */
                                /* when we know there won't be another.     */
    unsigned int    used;       /* This field is set to 1 by the DMA        */
                                /* interface after the buffer is transferred*/
    int    buf_id;              /* unique identifyer specified by the       */
                                /* the driver which requested the dma       */
} ep93xx_dma_buffer_t;

typedef ep93xx_dma_buffer_t * ep93xx_dma_buffer_p;

/*****************************************************************************
 *
 * Instance definition for the DMA interface.
 *
 ****************************************************************************/
typedef struct ep9312_dma_s
{
    /*
     *  This 1 when the instance is in use, and 0 when it's not.
     */
    unsigned int ref_count;

    /*
     * This is the last valid handle for this instance.  When giving out a
     * new handle this will be incremented and given out.
     */
    int last_valid_handle;

    /*
     * device specifies one of the 20 DMA hardware ports this 
     * DMA channel will service.
     */
    ep93xx_dma_dev_t device;

    /*
     * DMABufferQueue is the queue of buffer structure pointers which the
     * dma channel will use to setup transfers.
     */
    ep93xx_dma_buffer_t buffer_queue[MAX_EP93XX_DMA_BUFFERS];

    /*
     * currnt_buffer : This is the buffer currently being transfered on 
     *                 this channel.
     * last_buffer : This is the last buffer for this transfer.
     * Note: current_buffer + 1 is already programmed into the dma
     *       channel as the next buffer to transfer. Don't write
     *       over either entry.
     */
    int current_buffer;
    int last_buffer;

    /*
     * The following 3 fields are buffer counters.
     *
     * iNewBuffers: Buffers in the queue which have not been transfered.
     * iUsedBuffers: Buffers in the queue which have have been tranferred,
     *               and are waiting to be returned.
     * iTotalBuffers: Total number of buffers in the queue.
     */  
    int new_buffers;
    int used_buffers;
    int total_buffers;

    /*
     * uiTotalBytes has the total bytes transfered on the channel since the
     * last flush.  This value does not include the bytes tranfered in the
     * current buffer.  A byte count is only added after a complete buffer
     * is tranfered. 
     */
    unsigned int total_bytes;

    /*
     *  Interrupt number for this channel
     */
    unsigned int irq;

    /*
     * Indicates whether or not the channel is currently enabled to transfer
     * data.
     */
    unsigned int xfer_enable;
    
    /*
     * pause indicates if the dma channel was paused by calling the pause
     * ioctl.
     */
    unsigned int pause;
    
    /*
     *  buffer structure used during a pause to capture the current
     *  address and remaining bytes for the buffer actively being transferred
     *  on the channel. This buffer will be used to reprogram the dma 
     *  channel upon a resume.
     */
    ep93xx_dma_buffer_t pause_buf;
    
    /*
     * DMACallback is a function pointer which the calling application can 
     * use install a function to.  this fuction can be used to notify the 
     * calling application of an interrupt.
     */
    dma_callback callback;

    /*
     * User data used as a parameter for the Callback function.  The user
     * sets up the data and sends it with the callback function.
     */
    unsigned int user_data;
    
    /*
     * A string representation of the device attached to the channel.
     */
    const char * device_id;
    
    /*
     * The register base address for this dma channel.
     */
    unsigned int reg_base;
    
} ep93xx_dma_t;

/*****************************************************************************
 *
 * DMA macros
 *
 ****************************************************************************/
#define DMA_HANDLE_SPECIFIER_MASK   0xF0000000
#define DMA_CH0_HANDLE_SPECIFIER    0x00000000
#define DMA_CH1_HANDLE_SPECIFIER    0x10000000
#define DMA_CH2_HANDLE_SPECIFIER    0x20000000
#define DMA_CH3_HANDLE_SPECIFIER    0x30000000
#define DMA_CH4_HANDLE_SPECIFIER    0x40000000
#define DMA_CH5_HANDLE_SPECIFIER    0x50000000
#define DMA_CH6_HANDLE_SPECIFIER    0x60000000
#define DMA_CH7_HANDLE_SPECIFIER    0x70000000
#define DMA_CH8_HANDLE_SPECIFIER    0x80000000
#define DMA_CH9_HANDLE_SPECIFIER    0x90000000
#define DMA_CH10_HANDLE_SPECIFIER   0xA0000000
#define DMA_CH11_HANDLE_SPECIFIER   0xB0000000

#endif // _DMADRV_H_
