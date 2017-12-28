/*****************************************************************************
 *  linux/include/asm-arm/arch-ep93xx/dma.h
 *
 *  Copyright (C) 2003 Cirrus Logic
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
#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#define MAX_DMA_ADDRESS     0xffffffff

/*
 *  Not using the regular generic DMA interface for ep93xx.
 */
#define MAX_DMA_CHANNELS   0

/*
 * The ep93xx dma controller has 5 memory to peripheral (TX) channels, 5
 * peripheral to memory (RX) channels and 2 memory to memory channels.
 */
#define MAX_EP93XX_DMA_M2P_CHANNELS 10
#define MAX_EP93XX_DMA_M2M_CHANNELS 2

/*
 * The generic arm linux api does not support the ep93xx dma model, therefore
 * we use a set of dma support functions written specifically for this dma
 * controller.
 */
#define MAX_EP93XX_DMA_CHANNELS (MAX_EP93XX_DMA_M2P_CHANNELS + MAX_EP93XX_DMA_M2M_CHANNELS)

/*****************************************************************************
 *
 * Max DMA buffer size
 *
 ****************************************************************************/
#define DMA_MAX_BUFFER_BYTES    0xFFFF

/*****************************************************************************
 *
 * typedefs
 *
 ****************************************************************************/

/*****************************************************************************
 *
 * All devices which can use a DMA channel
 *
 * NOTE: There exist two types of DMA channels, those that transfer
 *       between an internal peripheral and memory (M2P/P2M), and
 *       those that transfer between an external peripheral and memory (M2M).
 *       This becomes a bit confusing when you take into account the fact
 *       that the M2M channels can also transfer between two specific
 *       internal peripherals and memory.
 *       The first 20 enumerated devices use the first type of channel (M2P/
 *       P2M).  The last six enumerations are specific to the M2M channels.
 *
 ****************************************************************************/
typedef enum
{
    /*
     *  Hardware device options for the 10 M2P/P2M DMA channels.
     */
    DMATx_I2S1 = 0x00000000,    /* TX peripheral ports can be allocated an  */
    DMATx_I2S2 = 0x00000001,    /* even numbered DMA channel.               */
    DMATx_AAC1 = 0x00000002,
    DMATx_AAC2 = 0x00000003,
    DMATx_AAC3 = 0x00000004,
    DMATx_I2S3 = 0x00000005,
    DMATx_UART1 = 0x00000006,
    DMATx_UART2 = 0x00000007,
    DMATx_UART3 = 0x00000008,
    DMATx_IRDA = 0x00000009,
    DMARx_I2S1 = 0x0000000A,    /* RX perhipheral ports can be allocated an */
    DMARx_I2S2 = 0x0000000B,    /* odd numbered DMA channel.                */
    DMARx_AAC1 = 0x0000000C,
    DMARx_AAC2 = 0x0000000D,
    DMARx_AAC3 = 0x0000000E,
    DMARx_I2S3 = 0x0000000F,
    DMARx_UART1 = 0x00000010,
    DMARx_UART2 = 0x00000011,
    DMARx_UART3 = 0x00000012,
    DMARx_IRDA = 0x00000013,

    /*
     *  Device options for the 2 M2M DMA channels
     */
    DMA_MEMORY = 0x00000014,
    DMA_IDE = 0x00000015,
    DMARx_SSP = 0x00000016,
    DMATx_SSP = 0x00000017,
    DMATx_EXT_DREQ = 0x00000018,
    DMARx_EXT_DREQ = 0x00000019,
    UNDEF = 0x0000001A
} ep93xx_dma_dev_t;

/*****************************************************************************
 *
 * Enumerated type used as a parameter for a callback function.
 * Indicates the type of interrupt.
 *
 ****************************************************************************/
typedef enum
{
    /*
     *  Common interrupts
     */
    STALL,
    NFB,

    /*
     *  Specific to M2P channels
     */
    CHERROR,

    /*
     *  Specific to M2M channels
     */
    DONE,
    UNDEF_INT
} ep93xx_dma_int_t;

/*****************************************************************************
 *
 * Init flag bit defintions for M2P/P2M flags.
 *
 ****************************************************************************/

/*
 *  Channel error interrupt enable.
 */
#define CHANNEL_ERROR_INT_ENABLE    0x00000001

/*
 *  Determines how the channel state machine behaves in the NEXT state and
 *  in receipt of a peripheral error.
 *  0 -> NEXT -> ON (ignore the peripheral error.)
 *  1 -> NEXT -> STALL (effectively disable the channel.)
 */
#define CHANNEL_ABORT               0x00000002

/*
 *  Ignore channel error interrupt.
 */
#define IGNORE_CHANNEL_ERROR        0x00000004

/*****************************************************************************
 *
 * Init flag bit defintions for M2M flags.
 *
 ****************************************************************************/

/*
 *  Destination address hold.  This should be set for IDE write transfers
 */
#define DESTINATION_HOLD            0x0000001

/*
 *  Source Address hold. This should be set for IDE read transfers
 */
#define SOURCE_HOLD                 0x0000002

/*
 *  Transfer mode.
 *  00 - s/w initiated M2M transfer
 *  01 - h/w initiated external peripheral transfer - memory to external
 *       peripheral/IDE/SSP.
 *  10 - h/w initiated external peripheral transfer - external
 *        peripheral/IDE/SSP to memory.
 *  11 - not used.
 */
#define TRANSFER_MODE_MASK          0x000000C
#define TRANSFER_MODE_SHIFT         2
#define TRANSFER_MODE_SW            0x0000000
#define TRANSFER_MODE_HW_M2P        0x0000004
#define TRANSFER_MODE_HW_P2M        0x0000008

/*
 *  Peripheral wait states count. Latency in HCLK cycles needed by the
 *  peripheral to de-assert its request line once the transfer is
 *  finished.
 *
 *  IDE Operation           Wait States
 *  --------------          ------------
 *  IDE MDMA read               0
 *  IDE MDMA write              3
 *  IDE UDMA read               1
 *  IDE UDMA write              2
 */
#define WAIT_STATES_MASK            0x00007F0
#define WAIT_STATES_SHIFT           4
#define WS_IDE_MDMA_READ            0
#define WS_IDE_MDMA_WRITE           3
#define WS_IDE_UDMA_READ            1
#define WS_IDE_UDMA_WRITE           2

/*****************************************************************************
 *
 * Type definition for the callback function
 *
 ****************************************************************************/
typedef void (* dma_callback)(ep93xx_dma_int_t dma_int,
                              ep93xx_dma_dev_t device,
                              unsigned int user_data);

/*****************************************************************************
 *
 * API function prototypes
 *
 ****************************************************************************/
extern int ep93xx_dma_request(int * handle, const char *device_id,
                              ep93xx_dma_dev_t device);
extern int ep93xx_dma_free(int handle);
extern int ep93xx_dma_config(int handle, unsigned int flags_m2p,
                             unsigned int flags_m2m,
                             dma_callback callback, unsigned int user_data);
extern int ep93xx_dma_add_buffer(int handle, unsigned int source,
                                 unsigned int dest, unsigned int size,
                                 unsigned int last, unsigned int buf_id);
extern int ep93xx_dma_remove_buffer(int handle, unsigned int * buf_id);
extern int ep93xx_dma_start(int handle, unsigned int channels,
                            unsigned int * handles);
extern int ep93xx_dma_pause(int handle, unsigned int channels,
                            unsigned int * handles);
extern int ep93xx_dma_flush(int handle);
extern int ep93xx_dma_queue_full(int handle);
extern int ep93xx_dma_get_position(int handle, unsigned int * buf_id,
                    unsigned int * total, unsigned int * current_frac);
extern int ep93xx_dma_get_total(int handle);
extern int ep93xx_dma_is_done(int handle);

#endif /* _ASM_ARCH_DMA_H */

