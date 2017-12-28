/*
 * linux-2.4.x/include/linux/mmc-COBRA5282.h 
 *
 * MMC/SD-Card driver
 *
 * based on mmc.h
 * Copyright (C) 2003 Thomas Fehrenbacher <thomas@blumba.de>
 *
 * Changed to COBRA5282:  Wolfgang Elsner wolfgang.elsner@sentec-elektronik.de
 */

/* QSPI registers */
#define	MCFSIM_QMR		0x340	/* mode */
#define	MCFSIM_QDLYR	0x344	/* delay */
#define	MCFSIM_QWR		0x348	/* wrap */
#define	MCFSIM_QIR		0x34c /* interrupt */
#define	MCFSIM_QAR		0x350	/* address */
#define	MCFSIM_QDR		0x354	/* address */

#define TX_RAM_START		   0x00
#define RX_RAM_START		   0x10
#define COMMAND_RAM_START	0x20

#define QMR		*(volatile unsigned short *)(MCF_IPSBAR + MCFSIM_QMR)
#define QAR		*(volatile unsigned short *)(MCF_IPSBAR + MCFSIM_QAR)
#define QDR		*(volatile unsigned short *)(MCF_IPSBAR + MCFSIM_QDR)
#define QWR		*(volatile unsigned short *)(MCF_IPSBAR + MCFSIM_QWR)
#define QDLYR	*(volatile unsigned short *)(MCF_IPSBAR + MCFSIM_QDLYR)
#define QIR		*(volatile unsigned short *)(MCF_IPSBAR + MCFSIM_QIR)

/* QSPI register bits */
#define QMR_MSTR	0x8000	/* master mode enable: must always be set */
#define QMR_DOHIE	0x4000	/* shut off (hi-z) Dout between transfers */
#define QMR_BITS	0x3c00	/* bits per transfer (size) */
#define QMR_CPOL	0x0200	/* clock state when inactive */
#define QMR_CPHA	0x0100	/* clock phase: 1 = data taken at rising edge */
#define QMR_BAUD	0x00ff	/* clock rate divider */

#define QIR_WCEF	0x0008	/* write collison */
#define QIR_ABRT	0x0004	/* abort */
#define QIR_SPIF	0x0001	/* finished */
#define QIR_SETUP	0x0001   /* setup QIR for polling mode */

#define QWR_CSIV	0x1000	/* 1 = active low chip selects */

#define QDLYR_SPE	0x8000	/* initiates transfer when set */
#define QDLYR_QCD	0x7f00	/* start delay between CS and first clock */
#define QDLYR_DTL	0x00ff	/* delay after CS release */

#define QCR_SETUP	0x7000
#define QCR_CONT	0x8000	/* 1=continuous CS after transfer */
#define QCR_CS		0x0e00	/* QSPI_CS 0 */

#define MCF5282_GPTSCR1    0x1b0006
#define MCF5282_GPTDDR     0x1b001e
#define MCF5282_GPTPORT    0x1b001d
