/******************************************************************************
 *
 *  spi_mcf.h - Motorola QSPI device driver header
 *
 * (C) Copyright 2004, Josef Baumgartner (josef.baumgartner@telex.de)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ******************************************************************************/

#if !defined(MCF_QSPI_H)
#define MCF_QSPI_H

#include <linux/types.h>
#include <linux/spi.h>
#include "spi_core.h"

#if defined(CONFIG_M5249)
#define MCFQSPI_IRQ_VECTOR      27
#define QSPIMOD_OFFSET          0x400
#elif defined(CONFIG_M5282) || defined(CONFIG_M5280)
#define MCFQSPI_IRQ_VECTOR      (64 + 18)
#define QSPIMOD_OFFSET          0x340
#else
#define MCFQSPI_IRQ_VECTOR      89
#define QSPIMOD_OFFSET          0xa0
#endif


/* QSPI registers */
#define MCFSIM_QMR              (0x00 + QSPIMOD_OFFSET) /* mode */
#define MCFSIM_QDLYR            (0x04 + QSPIMOD_OFFSET) /* delay */
#define MCFSIM_QWR              (0x08 + QSPIMOD_OFFSET) /* wrap */
#define MCFSIM_QIR              (0x0c + QSPIMOD_OFFSET) /* interrupt */
#define MCFSIM_QAR              (0x10 + QSPIMOD_OFFSET) /* address */
#define MCFSIM_QDR              (0x14 + QSPIMOD_OFFSET) /* address */

#define TX_RAM_START            0x00
#define RX_RAM_START            0x10
#define COMMAND_RAM_START       0x20

#define QMR                     *(volatile u16 *)(MCF_MBAR + MCFSIM_QMR)
#define QAR                     *(volatile u16 *)(MCF_MBAR + MCFSIM_QAR)
#define QDR                     *(volatile u16 *)(MCF_MBAR + MCFSIM_QDR)
#define QWR                     *(volatile u16 *)(MCF_MBAR + MCFSIM_QWR)
#define QDLYR                   *(volatile u16 *)(MCF_MBAR + MCFSIM_QDLYR)
#define QIR                     *(volatile u16 *)(MCF_MBAR + MCFSIM_QIR)


/* bits */
#define QMR_MSTR                0x8000  /* master mode enable: must always be set */
#define QMR_DOHIE               0x4000  /* shut off (hi-z) Dout between transfers */
#define QMR_BITS                0x3c00  /* bits per transfer (size) */
#define QMR_CPOL                0x0200  /* clock state when inactive */
#define QMR_CPHA                0x0100  /* clock phase: 1 = data taken at rising edge */
#define QMR_BAUD                0x00ff  /* clock rate divider */

#define QIR_WCEF                0x0008  /* write collison */
#define QIR_ABRT                0x0004  /* abort */
#define QIR_SPIF                0x0001  /* finished */
#define QIR_SETUP               0xdd0f  /* setup QIR for tranfer start */
#define QIR_SETUP_POLL          0xdc0d  /* setup QIR for tranfer start */

#define QWR_CSIV                0x1000  /* 1 = active low chip selects */

#define QDLYR_SPE               0x8000  /* initiates transfer when set */
#define QDLYR_QCD               0x7f00  /* start delay between CS and first clock */
#define QDLYR_DTL               0x00ff  /* delay after CS release */

/* QCR: chip selects return to inactive, bits set in QMR[BITS],
 * after delay set in QDLYR[DTL], pre-delay set in QDLYR[QCD] */
#define QCR_SETUP               0x7000
#define QCR_CONT                0x8000  /* 1=continuous CS after transfer */
#define QCR_SETUP8              0x3000  /* sets BITSE to 0 => eigth bits per transfer */


typedef struct qspi_dev {
        qspi_read_data read_data;
        u8 num;				/* device number */
        u8 bits;                        /* transfer size, number of bits to transfer for each entry */
        u8 baud;                        /* baud rate */
        u8 qcd;                         /* QSPILCK delay */
        u8 dtl;                         /* delay after transfer */
        u32 qcr_cont   : 1;    		/* keep CS active throughout transfer */
        u32 odd_mod    : 1;    		/* if length of buffer is a odd number, 16-bit transfers
                                           are finalized with a 8-bit transfer */
        u32 dsp_mod    : 1;    		/* transfers are bounded to 15/30 bytes
                                           (= a multiple of 3 bytes = 1 word) */
        u32 poll_mod   : 1;    		/* mode polling or interrupt */
        u32 cpol       : 1;    		/* SPI clock polarity */
        u32 cpha       : 1;    		/* SPI clock phase */
        u32 dohie      : 1;    		/* data output high impedance enable */
} qspi_dev;

#endif  /* MCF_QSPI_H */

