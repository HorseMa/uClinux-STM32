/*
 *  linux/drivers/char/at91_serial.c
 *
 *  Driver for Atmel AT91RM9200 Serial ports
 *
 *  Copyright (c) Rick Bronson
 *
 *  Based on drivers/char/serial_sa1100.c, by Deep Blue Solutions Ltd.
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
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
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>

#include <asm/arch/AT91RM9200_USART.h>
#include <asm/mach/serial_at91rm9200.h>
#include <asm/arch/pio.h>


#if defined(CONFIG_SERIAL_AT91_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>

#define SERIAL_AT91_MAJOR	TTY_MAJOR
#define CALLOUT_AT91_MAJOR	TTYAUX_MAJOR
#define MINOR_START		64

#define AT91C_VA_BASE_DBGU	((unsigned long) &(AT91_SYS->DBGU_CR))
#define AT91_ISR_PASS_LIMIT	256

#define UART_PUT_CR(port,v)	((AT91PS_USART)(port)->membase)->US_CR = v
#define UART_GET_MR(port)	((AT91PS_USART)(port)->membase)->US_MR
#define UART_PUT_MR(port,v)	((AT91PS_USART)(port)->membase)->US_MR = v
#define UART_PUT_IER(port,v)	((AT91PS_USART)(port)->membase)->US_IER = v
#define UART_PUT_IDR(port,v)	((AT91PS_USART)(port)->membase)->US_IDR = v
#define UART_GET_IMR(port)	((AT91PS_USART)(port)->membase)->US_IMR
#define UART_GET_CSR(port)	((AT91PS_USART)(port)->membase)->US_CSR
#define UART_GET_CHAR(port)	((AT91PS_USART)(port)->membase)->US_RHR
#define UART_PUT_CHAR(port,v)	((AT91PS_USART)(port)->membase)->US_THR = v
#define UART_GET_BRGR(port)	((AT91PS_USART)(port)->membase)->US_BRGR
#define UART_PUT_BRGR(port,v)	((AT91PS_USART)(port)->membase)->US_BRGR = v
#define UART_PUT_RTOR(port,v)	((AT91PS_USART)(port)->membase)->US_RTOR = v

// #define UART_GET_CR(port)	((AT91PS_USART)(port)->membase)->US_CR		// is write-only

 /* PDC registers */
#define UART_PUT_PTCR(port,v)	((AT91PS_USART)(port)->membase)->US_PTCR = v
#define UART_PUT_RPR(port,v)	((AT91PS_USART)(port)->membase)->US_RPR = v
#define UART_PUT_RCR(port,v)	((AT91PS_USART)(port)->membase)->US_RCR = v
#define UART_GET_RCR(port)	((AT91PS_USART)(port)->membase)->US_RCR
#define UART_PUT_RNPR(port,v)	((AT91PS_USART)(port)->membase)->US_RNPR = v
#define UART_PUT_RNCR(port,v)	((AT91PS_USART)(port)->membase)->US_RNCR = v

static struct tty_driver normal, callout;
static struct tty_struct *at91_table[AT91C_NR_UART];
static struct termios *at91_termios[AT91C_NR_UART], *at91_termios_locked[AT91C_NR_UART];
static int (*at91_open)(struct uart_port *);
static void (*at91_close)(struct uart_port *);

const int at91_serialmap[AT91C_NR_UART] = AT91C_UART_MAP;

/*
 * We wrap our port structure around the generic uart_port.
 */
struct at91_serial_port {
	struct uart_port	uart;
};

static struct at91_serial_port at91_ports[AT91C_NR_UART];

#ifdef SUPPORT_SYSRQ
static struct console at91_console;
#endif

/*
 * Return TIOCSER_TEMT when transmitter FIFO and Shift register is empty.
 */
static u_int at91_tx_empty(struct uart_port *uart)
{
	return UART_GET_CSR(uart) & AT91C_US_TXEMPTY ? TIOCSER_TEMT : 0;
}

/*
 * Set state of the modem control output lines
 */
static void at91_set_mctrl(struct uart_port *uart, u_int mctrl)
{
	unsigned int control = 0;

	if (mctrl & TIOCM_RTS)
		control |= AT91C_US_RTSEN;
	else
		control |= AT91C_US_RTSDIS;

	if (mctrl & TIOCM_DTR)
		control |= AT91C_US_DTREN;
	else
		control |=  AT91C_US_DTRDIS;

	UART_PUT_CR(uart, control);
}

/*
 * Get state of the modem control input lines
 */
static u_int at91_get_mctrl(struct uart_port *uart)
{
	unsigned int status, ret = 0;

	status = UART_GET_CSR(uart);
	if (status & AT91C_US_DCD)
		ret |= TIOCM_CD;
	if (status & AT91C_US_CTS)
		ret |= TIOCM_CTS;
	if (status & AT91C_US_DSR)
		ret |= TIOCM_DSR;
	if (status & AT91C_US_RI)
		ret |= TIOCM_RI;

	return ret;
}

/*
 * Stop transmitting.
 */
static void at91_stop_tx(struct uart_port *uart, u_int from_tty)
{
	UART_PUT_IDR(uart, AT91C_US_TXRDY);
	uart->read_status_mask &= ~AT91C_US_TXRDY;
}

/*
 * Start transmitting.
 */
static void at91_start_tx(struct uart_port *uart, u_int from_tty)
{
	unsigned long flags;

	local_irq_save(flags);
	uart->read_status_mask |= AT91C_US_TXRDY;
	UART_PUT_IER(uart, AT91C_US_TXRDY);
	local_irq_restore(flags);
}

/*
 * Stop receiving - port is in process of being closed.
 */
static void at91_stop_rx(struct uart_port *uart)
{
	UART_PUT_IDR(uart, AT91C_US_RXRDY);
}

/*
 * Enable modem status interrupts
 */
static void at91_enable_ms(struct uart_port *uart)
{
	UART_PUT_IER(uart, AT91C_US_RIIC | AT91C_US_DSRIC | AT91C_US_DCDIC | AT91C_US_CTSIC);
}

/*
 * Control the transmission of a break signal
 */
static void at91_break_ctl(struct uart_port *uart, int break_state)
{
	if (break_state != 0)
		UART_PUT_CR(uart, AT91C_US_STTBRK);	/* start break */
	else
		UART_PUT_CR(uart, AT91C_US_STPBRK);	/* stop break */
}

/*
 * Characters received (called from interrupt handler)
 */
static void at91_rx_chars(struct at91_serial_port *port, struct pt_regs *regs)
{
	struct uart_port *uart = &port->uart;
	struct tty_struct *tty = uart->info->tty;
	unsigned int status, ch, flg, ignored = 0;

	status = UART_GET_CSR(uart);
	while (status & (AT91C_US_RXRDY)) {
		ch = UART_GET_CHAR(uart);

		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		uart->icount.rx++;

		flg = TTY_NORMAL;

		/*
		 * note that the error handling code is
		 * out of the main execution path
		 */
		if (status & (AT91C_US_PARE | AT91C_US_FRAME | AT91C_US_OVRE))
			goto handle_error;

		if (uart_handle_sysrq_char(uart, ch, regs))
			goto ignore_char;

	error_return:
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
	ignore_char:
		status = UART_GET_CSR(uart);
	}
out:
	tty_flip_buffer_push(tty);
	return;

handle_error:
	if (status & (AT91C_US_PARE | AT91C_US_FRAME | AT91C_US_OVRE))
		UART_PUT_CR(uart, AT91C_US_RSTSTA);	/* clear error */
	if (status & (AT91C_US_PARE))
		uart->icount.parity++;
	else if (status & (AT91C_US_FRAME))
		uart->icount.frame++;
	if (status & (AT91C_US_OVRE))
		uart->icount.overrun++;

	if (status & uart->ignore_status_mask) {
		if (++ignored > 100)
			goto out;
		goto ignore_char;
	}

	status &= uart->read_status_mask;

	UART_PUT_CR(uart, AT91C_US_RSTSTA);	/* clear error */
	if (status & AT91C_US_PARE)
		flg = TTY_PARITY;
	else if (status & AT91C_US_FRAME)
		flg = TTY_FRAME;

	if (status & AT91C_US_OVRE) {
		/*
		 * overrun does *not* affect the character
		 * we read from the FIFO
		 */
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		ch = 0;
		flg = TTY_OVERRUN;
	}
#ifdef SUPPORT_SYSRQ
	uart->sysrq = 0;
#endif
	goto error_return;
}

/*
 * Transmit characters (called from interrupt handler)
 */
static void at91_tx_chars(struct at91_serial_port *port)
{
	struct uart_port *uart = &port->uart;
	struct circ_buf *xmit = &uart->info->xmit;

	if (uart->x_char) {
		UART_PUT_CHAR(uart, uart->x_char);
		uart->icount.tx++;
		uart->x_char = 0;
		return;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(uart)) {
		at91_stop_tx(uart, 0);
		return;
	}

	while (UART_GET_CSR(uart) & AT91C_US_TXRDY) {
		UART_PUT_CHAR(uart, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		uart->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(uart);

	if (uart_circ_empty(xmit))
		at91_stop_tx(uart, 0);
}

/*
 * Interrupt handler
 */
static void at91_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct at91_serial_port *port = dev_id;
	struct uart_port *uart = &port->uart;
	unsigned int status, pending, pass_counter = 0;

	status = UART_GET_CSR(uart);
	pending = status & uart->read_status_mask;
	if (pending) {
		do {
			if (pending & AT91C_US_RXRDY)
				at91_rx_chars(port, regs);

			/* Clear the relevent break bits */
			if (pending & AT91C_US_RXBRK) {
				UART_PUT_CR(uart, AT91C_US_RSTSTA);
				uart->icount.brk++;
#ifdef SUPPORT_SYSRQ
				if (uart->line == at91_console.index && !uart->sysrq) {
					uart->sysrq = jiffies + HZ*5;
				}
#endif
			}

			// TODO: All reads to CSR will clear these interrupts!
			if (pending & AT91C_US_RIIC) uart->icount.rng++;
			if (pending & AT91C_US_DSRIC) uart->icount.dsr++;
			if (pending & AT91C_US_DCDIC) {
				uart->icount.dcd++;
				uart_handle_dcd_change(uart, status & AT91C_US_DCD);
			}
			if (pending & AT91C_US_CTSIC) {
				uart->icount.cts++;
				uart_handle_cts_change(uart, status & AT91C_US_CTS);
			}
			if (pending & (AT91C_US_RIIC | AT91C_US_DSRIC | AT91C_US_DCDIC | AT91C_US_CTSIC))
				wake_up_interruptible(&uart->info->delta_msr_wait);

			if (pending & AT91C_US_TXRDY)
				at91_tx_chars(port);

			if (pass_counter++ > AT91_ISR_PASS_LIMIT)
				break;

			status = UART_GET_CSR(uart);
			pending = status & uart->read_status_mask;
		} while (pending);
	}
}

/*
 * Perform initialization and enable port for reception
 */
static int at91_startup(struct uart_port *uart)
{
	int retval;

	/*
	 * Allocate the IRQ
	 */
	retval = request_irq(uart->irq, at91_interrupt, SA_SHIRQ, "at91_serial", uart);
	if (retval) {
		printk("at91_serial: at91_startup - Can't get irq\n");
		return retval;
	}
	/*
	 * If there is a specific "open" function (to register
	 * control line interrupts)
	 */
	if (at91_open) {
		retval = at91_open(uart);
		if (retval) {
			free_irq(uart->irq, uart);
			return retval;
		}
	}

	/* Enable peripheral clock if required */
	if (uart->irq != AT91C_ID_SYS)
		AT91_SYS->PMC_PCER = 1 << uart->irq;

	uart->read_status_mask = AT91C_US_RXRDY | AT91C_US_TXRDY | AT91C_US_OVRE
			| AT91C_US_FRAME | AT91C_US_PARE | AT91C_US_RXBRK;
	/*
	 * Finally, clear and enable interrupts
	 */
	UART_PUT_IDR(uart, -1);
	UART_PUT_CR(uart, AT91C_US_TXEN | AT91C_US_RXEN);	/* enable xmit & rcvr */
	UART_PUT_IER(uart, AT91C_US_RXRDY);	/* do receive only */
	return 0;
}

/*
 * Disable the port
 */
static void at91_shutdown(struct uart_port *uart)
{
	/*
	 * Free the interrupt
	 */
	free_irq(uart->irq, uart);

	/*
	 * If there is a specific "close" function (to unregister
	 * control line interrupts)
	 */
	if (at91_close)
		at91_close(uart);

	/*
	 * Disable all interrupts, port and break condition.
	 */
	UART_PUT_CR(uart, AT91C_US_RSTSTA);
	UART_PUT_IDR(uart, -1);

	/* Disable peripheral clock if required */
	if (uart->irq != AT91C_ID_SYS)
		AT91_SYS->PMC_PCDR = 1 << uart->irq;
}

static struct uart_ops at91_pops;		/* forward declaration */

/*
 * Change the port parameters
 */
static void at91_change_speed(struct uart_port *uart, u_int cflag, u_int iflag, u_int quot)
{
	unsigned long flags;
	unsigned int mode, imr;

	/* Get current mode register */
	mode = UART_GET_MR(uart) & ~(AT91C_US_CHRL | AT91C_US_NBSTOP | AT91C_US_PAR);

	/* byte size */
	switch (cflag & CSIZE) {
	case CS5:
		mode |= AT91C_US_CHRL_5_BITS;
		break;
	case CS6:
		mode |= AT91C_US_CHRL_6_BITS;
		break;
	case CS7:
		mode |= AT91C_US_CHRL_7_BITS;
		break;
	default:
		mode |= AT91C_US_CHRL_8_BITS;
		break;
	}

	/* stop bits */
	if (cflag & CSTOPB)
		mode |= AT91C_US_NBSTOP_2_BIT;

	/* parity */
	if (cflag & PARENB) {
		if (cflag & CMSPAR) {			/* Mark or Space parity */
			if (cflag & PARODD)
				mode |= AT91C_US_PAR_MARK;
			else
				mode |= AT91C_US_PAR_SPACE;
		}
		else if (cflag & PARODD)
			mode |= AT91C_US_PAR_ODD;
		else
			mode |= AT91C_US_PAR_EVEN;
	}
	else
		mode |= AT91C_US_PAR_NONE;

	uart->read_status_mask |= AT91C_US_OVRE;
	if (iflag & INPCK)
		uart->read_status_mask |= AT91C_US_FRAME | AT91C_US_PARE;
	if (iflag & (BRKINT | PARMRK))
		uart->read_status_mask |= AT91C_US_RXBRK;

	/*
	 * Characters to ignore
	 */
	uart->ignore_status_mask = 0;
	if (iflag & IGNPAR)
		uart->ignore_status_mask |= (AT91C_US_FRAME | AT91C_US_PARE);
	if (iflag & IGNBRK) {
		uart->ignore_status_mask |= AT91C_US_RXBRK;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (iflag & IGNPAR)
			uart->ignore_status_mask |= AT91C_US_OVRE;
	}

	// TODO: Ignore all characters if CREAD is set.

	/* first, disable interrupts and drain transmitter */
	local_irq_save(flags);
	imr = UART_GET_IMR(uart);	/* get interrupt mask */
	UART_PUT_IDR(uart, -1);		/* disable all interrupts */
	local_irq_restore(flags);
	while (!(UART_GET_CSR(uart) & AT91C_US_TXEMPTY)) { barrier(); }

	/* disable receiver and transmitter */
	UART_PUT_CR(uart, AT91C_US_TXDIS | AT91C_US_RXDIS);

	/* set the parity, stop bits and data size */
	UART_PUT_MR(uart, mode);

	/* set the baud rate */
	UART_PUT_BRGR(uart, quot);
	UART_PUT_CR(uart, AT91C_US_TXEN | AT91C_US_RXEN);

	/* restore interrupts */
	UART_PUT_IER(uart, imr);

	/* CTS flow-control and modem-status interrupts */
	if (UART_ENABLE_MS(uart, cflag))
		at91_pops.enable_ms(uart);
}

/*
 * Return string describing the specified port
 */
static const char *at91_type(struct uart_port *uart)
{
	return uart->type == PORT_AT91RM9200 ? "AT91_SERIAL" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'.
 */
static void at91_release_port(struct uart_port *uart)
{
	release_mem_region(uart->mapbase,
		uart->mapbase == AT91C_VA_BASE_DBGU ? 512 : SZ_16K);
}

/*
 * Request the memory region(s) being used by 'port'.
 */
static int at91_request_port(struct uart_port *uart)
{
	return request_mem_region(uart->mapbase,
		uart->mapbase == AT91C_VA_BASE_DBGU ? 512 : SZ_16K,
		"at91_serial") != NULL ? 0 : -EBUSY;

}

/*
 * Configure/autoconfigure the port.
 */
static void at91_config_port(struct uart_port *uart, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		uart->type = PORT_AT91RM9200;
		at91_request_port(uart);
	}
}

/*
 * Verify the new serial_struct (for TIOCSSERIAL).
 */
static int at91_verify_port(struct uart_port *uart, struct serial_struct *ser)
{
	int ret = 0;
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_AT91RM9200)
		ret = -EINVAL;
	if (uart->irq != ser->irq)
		ret = -EINVAL;
	if (ser->io_type != SERIAL_IO_MEM)
		ret = -EINVAL;
	if (uart->uartclk / 16 != ser->baud_base)
		ret = -EINVAL;
	if ((void *)uart->mapbase != ser->iomem_base)
		ret = -EINVAL;
	if (uart->iobase != ser->port)
		ret = -EINVAL;
	if (ser->hub6 != 0)
		ret = -EINVAL;
	return ret;
}

static struct uart_ops at91_pops = {
	tx_empty:	at91_tx_empty,
	set_mctrl:	at91_set_mctrl,
	get_mctrl:	at91_get_mctrl,
	stop_tx:	at91_stop_tx,
	start_tx:	at91_start_tx,
	stop_rx:	at91_stop_rx,
	enable_ms:	at91_enable_ms,
	break_ctl:	at91_break_ctl,
	startup:	at91_startup,
	shutdown:	at91_shutdown,
	change_speed:	at91_change_speed,
	type:		at91_type,
	release_port:	at91_release_port,
	request_port:	at91_request_port,
	config_port:	at91_config_port,
	verify_port:	at91_verify_port,
};

void __init at91_init_ports(void)
{
	static int first = 1;
	int i;

	if (!first)
		return;
	first = 0;

	for (i = 0; i < AT91C_NR_UART; i++) {
		at91_ports[i].uart.iotype	= SERIAL_IO_MEM;
		at91_ports[i].uart.flags	= ASYNC_BOOT_AUTOCONF;
		at91_ports[i].uart.uartclk	= AT91C_MASTER_CLOCK;
		at91_ports[i].uart.ops		= &at91_pops;
		at91_ports[i].uart.fifosize	= 1;
		at91_ports[i].uart.line		= i;
	}
}

void __init at91_register_uart_fns(struct at91rm9200_port_fns *fns)
{
	if (fns->enable_ms)
		at91_pops.enable_ms = fns->enable_ms;
	if (fns->get_mctrl)
		at91_pops.get_mctrl = fns->get_mctrl;
	if (fns->set_mctrl)
		at91_pops.set_mctrl = fns->set_mctrl;
	at91_open          = fns->open;
	at91_close         = fns->close;
	at91_pops.pm       = fns->pm;
	at91_pops.set_wake = fns->set_wake;
}

/*
 * Setup ports.
 */
void __init at91_register_uart(int idx, int port)
{
	if ((idx < 0) || (idx >= AT91C_NR_UART)) {
		printk(KERN_ERR __FUNCTION__ ": bad index number %d\n", idx);
		return;
	}

	switch (port) {
	case 0:
		at91_ports[idx].uart.membase = (void *) AT91C_VA_BASE_US0;
		at91_ports[idx].uart.mapbase = AT91C_VA_BASE_US0;
		at91_ports[idx].uart.irq     = AT91C_ID_US0;
		AT91_CfgPIO_USART0();
		break;
	case 1:
		at91_ports[idx].uart.membase = (void *) AT91C_VA_BASE_US1;
		at91_ports[idx].uart.mapbase = AT91C_VA_BASE_US1;
		at91_ports[idx].uart.irq     = AT91C_ID_US1;
		AT91_CfgPIO_USART1();
		break;
	case 2:
		at91_ports[idx].uart.membase = (void *) AT91C_VA_BASE_US2;
		at91_ports[idx].uart.mapbase = AT91C_VA_BASE_US2;
		at91_ports[idx].uart.irq     = AT91C_ID_US2;
		AT91_CfgPIO_USART2();
		break;
	case 3:
		at91_ports[idx].uart.membase = (void *) AT91C_VA_BASE_US3;
		at91_ports[idx].uart.mapbase = AT91C_VA_BASE_US3;
		at91_ports[idx].uart.irq     = AT91C_ID_US3;
		AT91_CfgPIO_USART3();
		break;
	case 4:
		at91_ports[idx].uart.membase = (void *) AT91C_VA_BASE_DBGU;
		at91_ports[idx].uart.mapbase = AT91C_VA_BASE_DBGU;
		at91_ports[idx].uart.irq     = AT91C_ID_SYS;
		AT91_CfgPIO_DBGU();
		break;
	default:
		printk(KERN_ERR __FUNCTION__ ": bad port number %d\n", port);
	}
}

#ifdef CONFIG_SERIAL_AT91_CONSOLE

/*
 * Interrupts are disabled on entering
 */
static void at91_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *uart = &at91_ports[co->index].uart;
	unsigned int status, i, imr;

	/*
	 *	First, save IMR and then disable interrupts
	 */
	imr = UART_GET_IMR(uart);	/* get interrupt mask */
	UART_PUT_IDR(uart, AT91C_US_RXRDY | AT91C_US_TXRDY);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++) {
		do {
			status = UART_GET_CSR(uart);
		} while (!(status & AT91C_US_TXRDY));
		UART_PUT_CHAR(uart, s[i]);
		if (s[i] == '\n') {
			do {
				status = UART_GET_CSR(uart);
			} while (!(status & AT91C_US_TXRDY));
			UART_PUT_CHAR(uart, '\r');
		}
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore IMR
	 */
	do {
		status = UART_GET_CSR(uart);
	} while (status & AT91C_US_TXRDY);
	UART_PUT_IER(uart, imr);	/* set interrupts back the way they were */
}

static kdev_t at91_console_device(struct console *co)
{
	return MKDEV(SERIAL_AT91_MAJOR, MINOR_START + co->index);
}

/*
 * If the port was already initialised (eg, by a boot loader), try to determine
 * the current setup.
 */
static void __init at91_console_get_options(struct uart_port *uart, int *baud, int *parity, int *bits)
{
	unsigned int mr, quot;

// TODO: CR is a write-only register
//	unsigned int cr;
//
//	cr = UART_GET_CR(uart) & (AT91C_US_RXEN | AT91C_US_TXEN);
//	if (cr == (AT91C_US_RXEN | AT91C_US_TXEN)) {
//		/* ok, the port was enabled */
//
//		mr = UART_GET_MR(port) & AT91C_US_PAR;
//
//		*parity = 'n';
//		if (mr == AT91C_US_PAR_EVEN)
//			*parity = 'e';
//		else if (mr == AT91C_US_PAR_ODD)
//			*parity = 'o';
//	}

	mr = UART_GET_MR(uart) & AT91C_US_CHRL;
	if (mr == AT91C_US_CHRL_8_BITS)
		*bits = 8;
	else
		*bits = 7;

	quot = UART_GET_BRGR(uart);
	*baud = uart->uartclk / (16 * (quot));
}

static int __init at91_console_setup(struct console *co, char *options)
{
	struct uart_port *uart;
	int baud = AT91C_CONSOLE_DEFAULT_BAUDRATE;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (co->index >= AT91C_NR_UART)
		co->index = 0;
	uart = &at91_ports[co->index].uart;

	// TODO: The console port should be initialized, and clock enabled if
	//  we're not relying on the bootloader to do it.

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		at91_console_get_options(uart, &baud, &parity, &bits);

	return uart_set_options(uart, co, baud, parity, bits, flow);
}

static struct console at91_console = {
	name:		"ttyS",
	write:		at91_console_write,
	device:		at91_console_device,
	setup:		at91_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		AT91C_CONSOLE,
};

#define AT91_CONSOLE_DEVICE	&at91_console

void __init at91_console_init(void)
{
	at91_init_ports();
	register_console(&at91_console);
}

#else
#define AT91_CONSOLE_DEVICE	NULL
#endif

static struct uart_driver at91_uart = {
	owner:			THIS_MODULE,
	normal_major:		SERIAL_AT91_MAJOR,
#ifdef CONFIG_DEVFS_FS
	normal_name:		"ttyS%d",
	callout_name:		"cua%d",
#else
	normal_name:		"ttyS",
	callout_name:		"cua",
#endif
	normal_driver:		&normal,
	callout_major:		CALLOUT_AT91_MAJOR,
	callout_driver:		&callout,
	table:			at91_table,
	termios:		at91_termios,
	termios_locked:		at91_termios_locked,
	minor:			MINOR_START,
	nr:			AT91C_NR_UART,
	cons:			AT91_CONSOLE_DEVICE,
};

static int __init at91_serial_init(void)
{
	int ret, i;

	at91_init_ports();

	ret = uart_register_driver(&at91_uart);
	if (ret)
		return ret;

	for (i = 0; i < AT91C_NR_UART; i++) {
		if (at91_serialmap[i] >= 0)
			uart_add_one_port(&at91_uart, &at91_ports[i].uart);
	}

	return 0;
}

static void __exit at91_serial_exit(void)
{
	int i;

	for (i = 0; i < AT91C_NR_UART; i++) {
		if (at91_serialmap[i] >= 0)
			uart_remove_one_port(&at91_uart, &at91_ports[i].uart);
	}

	uart_unregister_driver(&at91_uart);
}

module_init(at91_serial_init);
module_exit(at91_serial_exit);

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Rick Bronson");
MODULE_DESCRIPTION("AT91 generic serial port driver");
MODULE_LICENSE("GPL");
