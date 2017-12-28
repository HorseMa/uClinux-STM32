 /*********************************************************************
 *                
 * Filename:      ep93xx_irda.h
 * Version:       0.2
 * Description:   Header for the EP93xx SOC IrDA driver.
 * Status:        Experimental.
 *
 * Copyright 2003 Cirrus Logic, Inc.
 *
 * Based on the ali-ircc.c implementation:
 *      Author:        Benjamin Kong <benjamin_kong@ali.com.tw>
 *      Created at:    2000/10/16 03:46PM
 *      Modified at:   2001/1/3 02:55PM
 *      Modified by:   Benjamin Kong <benjamin_kong@ali.com.tw>
 * 
 *      Copyright (c) 2000 Benjamin Kong <benjamin_kong@ali.com.tw>
 *      All Rights Reserved
 *      
 *      This program is free software; you can redistribute it and/or 
 *      modify it under the terms of the GNU General Public License as 
 *      published by the Free Software Foundation; either version 2 of 
 *      the License, or (at your option) any later version.
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
 * along with this program; if not, write to:
 *      Free Software Foundation, Inc.
 *      59 Temple Place, Suite 330 
 *      Boston, MA  02111-1307  USA
 ********************************************************************/


#ifndef EP93XX_IRDA_H
#define EP93XX_IRDA_H

#include <linux/time.h>

#include <linux/spinlock.h>
#include <linux/pm.h>
#include <asm/io.h>
//#include <regmap.h>
//#include <regs_syscon.h>


/*
 * Number of DMA channels used
 */
#define DMA_COUNT 3

/*
 * TX/RX window queues
 */
#define MAX_TX_WINDOW 7
#define MAX_RX_WINDOW 7


struct ep93xx_chip 
{
    char *name;
    int iSIR_IRQ, iMFIR_IRQ;
    ep93xx_dma_dev_t eSIR_DMATx;
    ep93xx_dma_dev_t eMFIR_DMATx;
    ep93xx_dma_dev_t eMFIR_DMARx;
	int (*init)(struct ep93xx_chip *chip); 
};

typedef struct ep93xx_chip ep93xx_chip_t;

/* For storing entries in the status FIFO */

struct st_fifo_entry 
{
	int status;
	int len;
};

struct st_fifo 
{
	struct st_fifo_entry entries[MAX_RX_WINDOW];
	int pending_bytes;
	int head;
	int tail;
	int len;
};

struct frame_cb 
{
	void *start; /* Start of frame in DMA mem */
	int len;     /* Lenght of frame in DMA mem */
};

struct tx_fifo 
{
	struct frame_cb queue[MAX_TX_WINDOW]; /* Info about frames in queue */
	int             ptr;                  /* Currently being sent */
	int             len;                  /* Length of queue */
	int             free;                 /* Next free slot */
	void           *tail;                 /* Next free start in DMA mem */
};

/* Private data for each instance */
struct ep93xx_irda_cb 
{
	struct st_fifo st_fifo;         /* Info about received frames */
	struct tx_fifo tx_fifo;         /* Info about frames to be transmitted */

	struct net_device *netdev;      /* Net device pointer */
	struct net_device_stats stats;  /* Net device stats */
	
	struct irlap_cb *irlap;         /* The link layer we are binded with */
	struct qos_info qos;            /* QoS capabilities for this device */

    int iSIR_irq, iMFIR_irq;        /* Interrupts used */

    ep93xx_dma_dev_t ePorts[DMA_COUNT];     /* DMA port ids */

    int iDMAh[DMA_COUNT];          /* handles for DMA channels */
    dma_addr_t rx_dmaphysh, tx_dmaphysh;

    int fifo_size;                  /* FIFO size */
    iobuff_t tx_buff;               /* Transmit buffer */
    iobuff_t rx_buff;               /* Receive buffer */

    char *conv_buf;                 /* Buffer used for SIR conversion */

    struct timeval stamp;           /* ? */
	struct timeval now;             /* ? */

	spinlock_t lock;                /* Used for serializing operations */
	
	__u32 flags;                    /* Interface flags */

    __u8 direction;                 /* TX/RX flag */

    __u32 speed;                    /* Currently used speed */
    __u32 new_speed;                /* New target tx/rx speed */

	int index;                      /* Instance index */
	
  	int suspended;                  /* APM suspend flag */
    struct pm_dev *dev;             /* Power Management device */

};


/*
 * DMA array handle index meanings
 */
#define DMA_SIR_TX                          0
#define DMA_MFIR_TX                         1
#define DMA_MFIR_RX                         2

/*
 * Directional defines
 */
#define DIR_TX                              0x01
#define DIR_RX                              0x02
#define DIR_BOTH                            0x03

/*
 * Modes support flag defines.
 */
#define CLK_SIR				    0x01
#define CLK_MIR				    0x02
#define CLK_FIR				    0x04

/* Name friendly bit defines for register accesses */

/* UART2/SIR */
#define U2RSR_OvnErr                        0x08
#define U2RSR_BrkErr                        0x04
#define U2RSR_PtyErr                        0x02
#define U2RSR_FrmErr                        0x01

#define U2CRH_WLen                          0x60
#define U2CRH_FEn                           0x10
#define U2CRH_Stp2                          0x08
#define U2CRH_EPS                           0x04
#define U2CRH_PEn                           0x02
#define U2CRH_Brk                           0x01

#define U2CR_LoopBk                         0x80
#define U2CR_RxTO                           0x40
#define U2CR_TxIrq                          0x20
#define U2CR_RxIrq                          0x10
#define U2CR_MdmStsIrq                      0x08
#define U2CR_SIRLowPwr                      0x04
#define U2CR_SIR                            0x02
#define U2CR_UART                           0x01

#define U2FR_TXFE                           0x80
#define U2FR_RXFF                           0x40
#define U2FR_TXFF                           0x20
#define U2FR_RXFE                           0x10
#define U2FR_BUSY                           0x08
#define U2FR_DCD                            0x04
#define U2FR_DSR                            0x02
#define U2FR_CTS                            0x01

#define U2IICR_RXTOIRQ                      0x08
#define U2IICR_TXIRQ                        0x04
#define U2IICR_RXIRQ                        0x02
#define U2IICR_MDMIRQ                       0x01

#define U2DMACR_DMAErr                      0x04
#define U2DMACR_TXDMA                       0x02
#define U2DMACR_RXDMA                       0x01


#define IrENABLE_SIR                        0x01


/* IrDA block - MIR/FIR */
#define IrENABLE_FIR                        0x03
#define IrENABLE_MIR                        0x02

#define IrCONTROL_RXRP                      0x40
#define IrCONTROL_TXRP                      0x20
#define IrCONTROL_RXON                      0x10
#define IrCONTROL_TXON                      0x08
#define IrCONTROL_TXUNDER                   0x04
#define IrCONTROL_MIRBR                     0x02


#define IrFLAG_TXBUSY                       0x200
#define IrFLAG_RXINFRM                      0x100
#define IrFLAG_RXSYNC                       0x80
#define IrFLAG_EOF                          0x40
#define IrFLAG_WIDTHST                      0x30
#define IrFLAG_FIRFE                        0x08
#define IrFLAG_RXOR                         0x04
#define IrFLAG_CRCERR                       0x02
#define IrFLAG_RXABORT                      0x01

#define IrRIB_ByteCt                        0x7FF0
#define IrRIB_BUFFE                         0x08
#define IrRIB_BUFOR                         0x04
#define IrRIB_BUFCRCERR                     0x02
#define IrRIB_BUFRXABORT                    0x01

#define IrDMACR_DMAERR                      0x04
#define IrDMACR_DMATXE                      0x02
#define IrDMACR_DMARXE                      0x01

#define MFISR_RXFL                          0x40
#define MFISR_RXIL                          0x20
#define MFISR_RXFC                          0x10
#define MFISR_RXFS                          0x08
#define MFISR_TXFABORT                      0x04
#define MFISR_TXFC                          0x02
#define MFISR_TXSR                          0x01

#define MFIMR_RXFL                          0x40
#define MFIMR_RXIL                          0x20
#define MFIMR_RXFC                          0x10
#define MFIMR_RXFS                          0x08
#define MFIMR_TXABORT                       0x04
#define MFIMR_TXFC                          0x02
#define MFIMR_TXFS                          0x01

#define MFIIR_RXFL                          0x40
#define MFIIR_RXIL                          0x20
#define MFIIR_RXFC                          0x10
#define MFIIR_RXFS                          0x08
#define MFIIR_TXABORT                       0x04
#define MFIIR_TXFC                          0x02
#define MFIIR_TXFS                          0x01


#endif /* EP93XX_IRDA_H */
