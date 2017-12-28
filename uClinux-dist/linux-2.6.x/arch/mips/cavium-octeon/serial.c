/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Cavium Networks
 */
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <linux/tty.h>

#include <asm/time.h>

#include "hal.h"

#ifdef CONFIG_GDB_CONSOLE
#define DEBUG_UART 0
#else
#define DEBUG_UART 1
#endif

#ifdef CONFIG_KGDB

char getDebugChar(void)
{
	unsigned long lsrval;

	octeon_write_lcd("kgdb");

	/* Spin until data is available */
	do {
		lsrval = cvmx_read_csr(CVMX_MIO_UARTX_LSR(DEBUG_UART));
	} while ((lsrval & 0x1) == 0);

	octeon_write_lcd("");

	/* Read and return the data */
	return cvmx_read_csr(CVMX_MIO_UARTX_RBR(DEBUG_UART));
}

void putDebugChar(char ch)
{
	unsigned long lsrval;

	/* Spin until there is room */
	do {
		lsrval = cvmx_read_csr(CVMX_MIO_UARTX_LSR(DEBUG_UART));
	} while ((lsrval & 0x20) == 0);

	/* Write the byte */
	cvmx_write_csr(CVMX_MIO_UARTX_THR(DEBUG_UART), ch);
}

#endif

#if defined(CONFIG_KGDB) || defined(CONFIG_CAVIUM_GDB)

static irqreturn_t interruptDebugChar(int cpl, void *dev_id)
{
	unsigned long lsrval;
	lsrval = cvmx_read_csr(CVMX_MIO_UARTX_LSR(1));
	if (lsrval & 1) {
#ifdef CONFIG_KGDB
		struct pt_regs *regs = get_irq_regs();

		putDebugChar(getDebugChar());
		set_async_breakpoint(&regs->cp0_epc);
#else
		unsigned long tmp;
		/* Pulse MCD0 signal on Ctrl-C to stop all the cores. Also set
		   the MCD0 to be not masked by this core so we know the signal
		   is received by someone */
		octeon_write_lcd("brk");
		asm volatile ("dmfc0 %0, $22\n"
			      "ori   %0, %0, 0x10\n"
			      "dmtc0 %0, $22\n" : "=r" (tmp));
		octeon_write_lcd("");
#endif
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

#endif

unsigned int octeon_serial_in(struct uart_port *up, int offset)
{
	return cvmx_read_csr((uint64_t)(up->membase + (offset << 3)));
}

void octeon_serial_out(struct uart_port *up, int offset, int value)
{
	/*
	 * If bits 6 or 7 of the OCTEON UART's LCR are set, it quits
	 * working.
	 */
	if (offset == UART_LCR)
		value &= 0x9f;
	cvmx_write_csr((uint64_t)(up->membase + (offset << 3)), (u8)value);
}

static int octeon_serial_init(void)
{
	struct uart_port octeon_port;
	int enable_uart0;
	int enable_uart1;
	int enable_uart2;

#ifdef CONFIG_CAVIUM_OCTEON_2ND_KERNEL
	/* If we are configured to run as the second of two kernels, disable
	   uart0 and enable uart1. Uart0 is owned by the first kernel */
	enable_uart0 = 0;
	enable_uart1 = 1;
#else
	/* We are configured for the first kernel. We'll enable uart0 if the
	   bootloader told us to use 0, otherwise will enable uart 1 */
	enable_uart0 = (octeon_get_boot_uart() == 0);
	enable_uart1 = (octeon_get_boot_uart() == 1);
	/* Uncomment the following line if you'd like uart1 to be enable as
	   well as uart 0 when the bootloader tells us to use uart0 */
	/*
	enable_uart1 = 1;
	*/
#endif

#if defined(CONFIG_KGDB) || defined(CONFIG_CAVIUM_GDB)
	/* As a special case disable uart1 if KGDB is in use */
	enable_uart1 = 0;
#endif

	/* Right now CN52XX is the only chip with a third uart */
	enable_uart2 = OCTEON_IS_MODEL(OCTEON_CN52XX);

	/* These fields are common to all Octeon UARTs */
	memset(&octeon_port, 0, sizeof(octeon_port));
	octeon_port.flags = ASYNC_SKIP_TEST | UPF_SHARE_IRQ | UPF_FIXED_TYPE;
	octeon_port.type = PORT_OCTEON;
	octeon_port.iotype = UPIO_MEM;
	octeon_port.regshift = 3;	/* I/O addresses are every 8 bytes */
	octeon_port.uartclk = mips_hpt_frequency;
	octeon_port.fifosize = 64;
	octeon_port.serial_in = octeon_serial_in;
	octeon_port.serial_out = octeon_serial_out;

	/* Add a ttyS device for hardware uart 0 */
	if (enable_uart0) {
		octeon_port.membase = (void *) CVMX_MIO_UARTX_RBR(0);
		octeon_port.mapbase =
			CVMX_MIO_UARTX_RBR(0) & ((1ull << 49) - 1);
		/* Only CN38XXp{1,2} has errata with uart interrupt */
		if (!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2))
			octeon_port.irq = OCTEON_IRQ_UART0;
		serial8250_register_port(&octeon_port);
	}

	/* Add a ttyS device for hardware uart 1 */
	if (enable_uart1) {
		octeon_port.membase = (void *) CVMX_MIO_UARTX_RBR(1);
		octeon_port.mapbase =
			CVMX_MIO_UARTX_RBR(1) & ((1ull << 49) - 1);
		/* Only CN38XXp{1,2} has errata with uart interrupt */
		if (!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2))
			octeon_port.irq = OCTEON_IRQ_UART1;
		serial8250_register_port(&octeon_port);
	}

	/* Add a ttyS device for hardware uart 2 */
	if (enable_uart2) {
		octeon_port.membase = (void *) CVMX_MIO_UART2_RBR;
		octeon_port.mapbase = CVMX_MIO_UART2_RBR & ((1ull << 49) - 1);
		octeon_port.irq = OCTEON_IRQ_UART2;
		serial8250_register_port(&octeon_port);
	}
#if defined(CONFIG_KGDB) || defined(CONFIG_CAVIUM_GDB)
	request_irq(OCTEON_IRQ_UART0 + DEBUG_UART, interruptDebugChar,
		    IRQF_SHARED, "KGDB", interruptDebugChar);

	/* Enable uart1 interrupts for debugger Control-C processing */
	cvmx_write_csr(CVMX_MIO_UARTX_IER(DEBUG_UART),
		       cvmx_read_csr(CVMX_MIO_UARTX_IER(DEBUG_UART)) | 1);
#endif
	return 0;
}

late_initcall(octeon_serial_init);
