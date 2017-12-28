/*
 * FILE NAME: arch/arm/mach-integrator/kgdb-serial.c
 *
 * BRIEF MODULE DESCRIPTION:
 *  Provides low level kgdb serial support hooks for ARM Integrator.
 *
 * Author: George G. Davis <gdavis@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * Based on the AMBA serial port driver, filename drivers/char/serial_amba.c,
 * Copyright 1999 ARM Limited
 * Copyright (C) 2000 Deep Blue Solutions Ltd.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE	LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/kgdb.h>
#include <asm/hardware/serial_amba.h>



#define UART_GET_INT_STATUS(p)	readb((p) + AMBA_UARTIIR)
#define UART_PUT_ICR(p, c)	writel((c), (p) + AMBA_UARTICR)
#define UART_GET_FR(p)		readb((p) + AMBA_UARTFR)
#define UART_GET_CHAR(p)	readb((p) + AMBA_UARTDR)
#define UART_PUT_CHAR(p, c)	writel((c), (p) + AMBA_UARTDR)
#define UART_GET_RSR(p)		readb((p) + AMBA_UARTRSR)
#define UART_GET_CR(p)		readb((p) + AMBA_UARTCR)
#define UART_PUT_CR(p,c)	writel((c), (p) + AMBA_UARTCR)
#define UART_GET_LCRL(p)	readb((p) + AMBA_UARTLCR_L)
#define UART_PUT_LCRL(p,c)	writel((c), (p) + AMBA_UARTLCR_L)
#define UART_GET_LCRM(p)	readb((p) + AMBA_UARTLCR_M)
#define UART_PUT_LCRM(p,c)	writel((c), (p) + AMBA_UARTLCR_M)
#define UART_GET_LCRH(p)	readb((p) + AMBA_UARTLCR_H)
#define UART_PUT_LCRH(p,c)	writel((c), (p) + AMBA_UARTLCR_H)
#define UART_RX_DATA(s)		(((s) & AMBA_UARTFR_RXFE) == 0)
#define UART_TX_READY(s)	(((s) & AMBA_UARTFR_TXFF) == 0)
#define UART_TX_EMPTY(p)	((UART_GET_FR(p) & AMBA_UARTFR_TMSK) == 0)

#if	defined(CONFIG_KGDB_UART0)
#	define	KGDB_SERIAL_PORT_BASE	INTEGRATOR_UART0_BASE
#elif	defined(CONFIG_KGDB_UART1)
#	define	KGDB_SERIAL_PORT_BASE	INTEGRATOR_UART1_BASE
#else
#	error "No kgdb serial port UART has been selected."
#endif

#if	defined(CONFIG_KGDB_9600BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	ARM_BAUD_9600
#elif	defined(CONFIG_KGDB_19200BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	ARM_BAUD_19200
#elif	defined(CONFIG_KGDB_38400BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	ARM_BAUD_38400
#elif	defined(CONFIG_KGDB_57600BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	ARM_BAUD_57600
#elif	defined(CONFIG_KGDB_115200BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	ARM_BAUD_115200
#else
#	error "kgdb serial baud rate has not been specified."
#endif


static volatile unsigned char *port = NULL;


void
kgdb_serial_init(void)
{
	port = (unsigned char *)(IO_ADDRESS(KGDB_SERIAL_PORT_BASE));

	UART_PUT_CR(port, 0);

	/* Set baud rate */
	UART_PUT_LCRM(port, ((KGDB_SERIAL_BAUD_RATE & 0xf00) >> 8));
	UART_PUT_LCRL(port, (KGDB_SERIAL_BAUD_RATE & 0xff));

	/*
	 * ----------v----------v----------v----------v-----
	 * NOTE: MUST BE WRITTEN AFTER UARTLCR_M & UARTLCR_L
	 * ----------^----------^----------^----------^-----
	 */
	UART_PUT_LCRH(port, AMBA_UARTLCR_H_WLEN_8 | AMBA_UARTLCR_H_FEN);
	UART_PUT_CR(port, AMBA_UARTCR_UARTEN);

	return;
}


void
kgdb_serial_putchar(unsigned char ch)
{
        unsigned int status;

	do {
		status = UART_GET_FR(port);
	} while (!UART_TX_READY(status));

	UART_PUT_CHAR(port, ch);
}


unsigned char
kgdb_serial_getchar(void)
{
        unsigned int status;
	unsigned char ch;

	do {
		status = UART_GET_FR(port);
	} while (!UART_RX_DATA(status));

	ch = UART_GET_CHAR(port);

	return ch;
}
