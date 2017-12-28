/*
 *  linux/drivers/serial/cs93xx_amba.c
 *
 *  Driver for CS9312 AMBA serial ports
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o,
 *  Deep Blue Solutions Ltd.
 *
 *  Copyright 1999 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *  Copyright (c) 2003 Cirrus Logic, Inc.
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
 *  $Id: ep93xx_amba.c,v 1.6 2004/08/13 15:31:28 bkircher Exp $
 *
 * The EP9312 serial ports are AMBA, but at different addresses from the
 * integrator.
 * This is a generic driver for ARM AMBA-type serial ports.  They
 * have a lot of 16550-like features, but are not register compatable.
 * Note that although they do have CTS, DCD and DSR inputs, they do
 * not have an RI input, nor do they have DTR or RTS outputs.  If
 * required, these have to be supplied via some other means (eg, GPIO)
 * and hooked into this driver.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

#if defined(CONFIG_SERIAL_EP93XX_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>

#include <asm/hardware/serial_amba.h>

#if defined(CONFIG_ARCH_EP9301) || defined(CONFIG_ARCH_EP9302)
#ifdef CONFIG_EP93XX_IRDA
#define UART_NR		1
#else
#define UART_NR		2
#endif
#else
#ifdef CONFIG_EP93XX_IRDA
#define UART_NR		2
#else
#define UART_NR		3
#endif
#endif

#define SERIAL_AMBA_MAJOR	204
#define SERIAL_AMBA_MINOR	16
#define SERIAL_AMBA_NR		UART_NR

#define CALLOUT_AMBA_NAME	"cuaam"
#define CALLOUT_AMBA_MAJOR	205
#define CALLOUT_AMBA_MINOR	16
#define CALLOUT_AMBA_NR		UART_NR

static struct tty_driver normal, callout;
static struct tty_struct *cs_amba_table[UART_NR];
static struct termios *cs_amba_termios[UART_NR], *cs_amba_termios_locked[UART_NR];
#ifdef SUPPORT_SYSRQ
static struct console cs_amba_console;
#endif

#define AMBA_ISR_PASS_LIMIT	256

/*
 * Access macros for the AMBA UARTs
 */
#define UART_GET_INT_STATUS(p)	((readl((p)->membase + AMBA_UARTIIR)) & 0xff)
#define UART_PUT_ICR(p, c)		writel((c), (p)->membase + AMBA_UARTICR)
#define UART_GET_FR(p)			((readl((p)->membase + AMBA_UARTFR)) & 0xff)
#define UART_GET_CHAR(p)		((readl((p)->membase + AMBA_UARTDR)) & 0xff)
#define UART_PUT_CHAR(p, c)		writel((c), (p)->membase + AMBA_UARTDR)
#define UART_GET_RSR(p)			((readl((p)->membase + AMBA_UARTRSR)) & 0xff)
#define UART_PUT_RSR(p, c)		writel((c), (p)->membase + AMBA_UARTRSR)
#define UART_GET_CR(p)			((readl((p)->membase + AMBA_UARTCR)) & 0xff)
#define UART_PUT_CR(p,c)		writel((c), (p)->membase + AMBA_UARTCR)
#define UART_GET_LCRL(p)		((readl((p)->membase + AMBA_UARTLCR_L)) & 0xff)
#define UART_PUT_LCRL(p,c)		writel((c), (p)->membase + AMBA_UARTLCR_L)
#define UART_GET_LCRM(p)		((readl((p)->membase + AMBA_UARTLCR_M)) & 0xff)
#define UART_PUT_LCRM(p,c)		writel((c), (p)->membase + AMBA_UARTLCR_M)
#define UART_GET_LCRH(p)		((readl((p)->membase + AMBA_UARTLCR_H)) & 0xff)
#define UART_PUT_LCRH(p,c)		writel((c), (p)->membase + AMBA_UARTLCR_H)
#define UART_RX_DATA(s)			(((s) & AMBA_UARTFR_RXFE) == 0)
#define UART_TX_READY(s)		(((s) & AMBA_UARTFR_TXFF) == 0)
#define UART_TX_EMPTY(p)		((UART_GET_FR(p) & AMBA_UARTFR_TMSK) == 0)

#define UART_PUT_MCR(c, p)  writel((c), (p)->membase + ((unsigned int)UART1MCR & 0xffff))
#define UART_CLEAR_ECR(p)   writel( 0, (p)->membase + AMBA_UARTECR)

#define UART_DUMMY_RSR_RX	256
#define UART_PORT_SIZE		64

/*
 * Our private driver data mappings.
 */
#define drv_old_status	driver_priv

/*
 * We wrap our port structure around the generic uart_port.
 */
struct uart_amba_port {
	struct uart_port	port;
	unsigned int		dtr_mask;
	unsigned int		rts_mask;
	unsigned int		old_status;
};


static void csambauart_enable_clocks(struct uart_port *port)
{
	unsigned int uiSysDevCfg;

	/*
	 * Enable the clocks to this UART in CSC_syscon
	 * - Read DEVCFG
	 * - OR in the correct uart enable bit
	 * - Set the lock register
	 * - Write back to DEVCFG
	 */
	uiSysDevCfg = inl(SYSCON_DEVCFG);

	switch( port->mapbase )
	{
		case UART1_BASE:
		    uiSysDevCfg |= SYSCON_DEVCFG_U1EN;
			break;

		case UART2_BASE:
		    uiSysDevCfg |= SYSCON_DEVCFG_U2EN;
			break;

		case UART3_BASE:
		    uiSysDevCfg |= SYSCON_DEVCFG_U3EN;
			break;
	}

	SysconSetLocked( SYSCON_DEVCFG, uiSysDevCfg );
}

static void csambauart_disable_clocks(struct uart_port *port)
{
	unsigned int uiSysDevCfg;

	/*
	 * Disable the clocks to this UART in CSC_syscon
	 * - Read DEVCFG
	 * - AND to clear the correct uart enable bit
	 * - Set the lock register
	 * - Write back to DEVCFG
	 */
	uiSysDevCfg = inl(SYSCON_DEVCFG);

	switch( port->mapbase ) {
	case UART1_BASE:
		uiSysDevCfg &= ~((unsigned int)SYSCON_DEVCFG_U1EN);
		break;

	case UART2_BASE:
		uiSysDevCfg &= ~((unsigned int)SYSCON_DEVCFG_U2EN);
		break;

	case UART3_BASE:
		uiSysDevCfg &= ~((unsigned int)SYSCON_DEVCFG_U3EN);
		break;
	}

	SysconSetLocked( SYSCON_DEVCFG, uiSysDevCfg );
}

static int csambauart_is_port_enabled(struct uart_port *port)
{
	unsigned int uiSysDevCfg;

	uiSysDevCfg = inl(SYSCON_DEVCFG);

	switch( port->mapbase ) {
	case UART1_BASE:
		uiSysDevCfg &= (unsigned int)SYSCON_DEVCFG_U1EN;
		break;

	case UART2_BASE:
		uiSysDevCfg &= (unsigned int)SYSCON_DEVCFG_U2EN;
		break;

	case UART3_BASE:
		uiSysDevCfg &= (unsigned int)SYSCON_DEVCFG_U3EN;
		break;
	}

	return( uiSysDevCfg != 0 );
}



static void csambauart_stop_tx(struct uart_port *port, unsigned int tty_stop)
{
	unsigned int cr;

	cr = UART_GET_CR(port);
	cr &= ~AMBA_UARTCR_TIE;
	UART_PUT_CR(port, cr);
}

static void csambauart_start_tx(struct uart_port *port, unsigned int tty_start)
{
	unsigned int cr;

	cr = UART_GET_CR(port);
	cr |= AMBA_UARTCR_TIE;
	UART_PUT_CR(port, cr);
}

static void csambauart_stop_rx(struct uart_port *port)
{
	unsigned int cr;

	cr = UART_GET_CR(port);
	cr &= ~(AMBA_UARTCR_RIE | AMBA_UARTCR_RTIE);
	UART_PUT_CR(port, cr);
}

static void csambauart_enable_ms(struct uart_port *port)
{
	unsigned int cr;

	cr = UART_GET_CR(port);
	cr |= AMBA_UARTCR_MSIE;
	UART_PUT_CR(port, cr);
}

static void
#ifdef SUPPORT_SYSRQ
csambauart_rx_chars(struct uart_port *port, struct pt_regs *regs)
#else
csambauart_rx_chars(struct uart_port *port)
#endif
{
	struct tty_struct *tty = port->info->tty;
	unsigned int status, ch, rsr, max_count = 256;

	status = UART_GET_FR(port);
	while (UART_RX_DATA(status) && max_count--) {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			tty->flip.tqueue.routine((void *)tty);
			if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
				printk(KERN_WARNING "TTY_DONT_FLIP set\n");
				return;
			}
		}

		ch = UART_GET_CHAR(port);

		*tty->flip.char_buf_ptr = ch;
		*tty->flip.flag_buf_ptr = TTY_NORMAL;
		port->icount.rx++;

		/*
		 * Note that the error handling code is
		 * out of the main execution path
		 */
		rsr = UART_GET_RSR(port) | UART_DUMMY_RSR_RX;
		UART_PUT_RSR(port, 0);
		if (rsr & AMBA_UARTRSR_ANY) {
			UART_PUT_RSR(port, 0xff); /* clear error */
			if (rsr & AMBA_UARTRSR_BE) {
				rsr &= ~(AMBA_UARTRSR_FE | AMBA_UARTRSR_PE);
				port->icount.brk++;
				if (uart_handle_break(port))
					goto ignore_char;
			} else if (rsr & AMBA_UARTRSR_PE)
				port->icount.parity++;
			else if (rsr & AMBA_UARTRSR_FE)
				port->icount.frame++;
			if (rsr & AMBA_UARTRSR_OE)
				port->icount.overrun++;

			rsr &= port->read_status_mask;

			/* According to the EP9315 Users Guide, to clear 
			 * any overrun, break, parity or framing error, you 
			 * have to write to the UART RSR register. */
			if (rsr & AMBA_UARTRSR_BE){
				*tty->flip.flag_buf_ptr = TTY_BREAK;
			}
			else if (rsr & AMBA_UARTRSR_PE){
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			}
			else if (rsr & AMBA_UARTRSR_FE){
				*tty->flip.flag_buf_ptr = TTY_FRAME;
			}
		}

		if (uart_handle_sysrq_char(port, ch, regs))
			goto ignore_char;

		if ((rsr & port->ignore_status_mask) == 0) {
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if ((rsr & AMBA_UARTRSR_OE) &&
		    tty->flip.count < TTY_FLIPBUF_SIZE) {
			/*
			 * Overrun is special, since it's reported
			 * immediately, and doesn't affect the current
			 * character
			 */
			*tty->flip.char_buf_ptr++ = 0;
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
			tty->flip.count++;
		}
	ignore_char:
		status = UART_GET_FR(port);
	}
	tty_flip_buffer_push(tty);
	return;
}

static void csambauart_tx_chars(struct uart_port *port)
{
	struct circ_buf *xmit = &port->info->xmit;
	int count;

	if (port->x_char) {
		UART_PUT_CHAR(port, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		csambauart_stop_tx(port, 0);
		return;
	}

	count = port->fifosize >> 1;
	do {
		UART_PUT_CHAR(port, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		csambauart_stop_tx(port, 0);
}

static void csambauart_modem_status(struct uart_port *port)
{
	struct uart_amba_port *uap = (struct uart_amba_port *)port;
	unsigned int status, delta;

	UART_PUT_ICR(&uap->port, 0);

	status = UART_GET_FR(&uap->port) & AMBA_UARTFR_MODEM_ANY;

	delta = status ^ uap->old_status;
	uap->old_status = status;

	if (!delta)
		return;

	if (delta & AMBA_UARTFR_DCD)
		uart_handle_dcd_change(&uap->port, status & AMBA_UARTFR_DCD);

	if (delta & AMBA_UARTFR_DSR)
		uap->port.icount.dsr++;

	if (delta & AMBA_UARTFR_CTS)
		uart_handle_cts_change(&uap->port, status & AMBA_UARTFR_CTS);

	wake_up_interruptible(&uap->port.info->delta_msr_wait);
}

static void csambauart_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct uart_port *port = dev_id;
	unsigned int status, pass_counter = AMBA_ISR_PASS_LIMIT;

	status = UART_GET_INT_STATUS(port);
	do {
		if (status & (AMBA_UARTIIR_RTIS | AMBA_UARTIIR_RIS))
#ifdef SUPPORT_SYSRQ
			csambauart_rx_chars(port, regs);
#else
			csambauart_rx_chars(port);
#endif
		if (status & AMBA_UARTIIR_TIS)
			csambauart_tx_chars(port);
		if (status & AMBA_UARTIIR_MIS)
			csambauart_modem_status(port);

		if (pass_counter-- == 0)
			break;

		status = UART_GET_INT_STATUS(port);
	} while (status & (AMBA_UARTIIR_RTIS | AMBA_UARTIIR_RIS |
			   AMBA_UARTIIR_TIS));
}

static unsigned int csambauart_tx_empty(struct uart_port *port)
{
	return UART_GET_FR(port) & AMBA_UARTFR_BUSY ? 0 : TIOCSER_TEMT;
}

static unsigned int csambauart_get_mctrl(struct uart_port *port)
{
	unsigned int result = 0;
	unsigned int status;

	status = UART_GET_FR(port);
	if (status & AMBA_UARTFR_DCD)
		result |= TIOCM_CAR;
	if (status & AMBA_UARTFR_DSR)
		result |= TIOCM_DSR;
	if (status & AMBA_UARTFR_CTS)
		result |= TIOCM_CTS;

	return result;
}

static void csambauart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_amba_port *uap = (struct uart_amba_port *)port;
	unsigned int ctrl = 0;
	
	/* If there's no RTS and DTR for this UART, do nothing here */
	if ((uap->rts_mask == 0) && (uap->dtr_mask == 0) )
		return;

	if (mctrl & TIOCM_RTS)
		ctrl |= uap->rts_mask;

	if (mctrl & TIOCM_DTR)
		ctrl |= uap->dtr_mask;

	UART_PUT_MCR(ctrl, port);
}

static void csambauart_break_ctl(struct uart_port *port, int break_state)
{
	unsigned long flags;
	unsigned int lcr_h;

	spin_lock_irqsave(&port->lock, flags);
	lcr_h = UART_GET_LCRH(port);
	if (break_state == -1)
		lcr_h |= AMBA_UARTLCR_H_BRK;
	else
		lcr_h &= ~AMBA_UARTLCR_H_BRK;
	UART_PUT_LCRH(port, lcr_h);
	spin_unlock_irqrestore(&port->lock, flags);
}

static int csambauart_startup(struct uart_port *port)
{
	struct uart_amba_port *uap = (struct uart_amba_port *)port;
	int retval = -EINVAL;

	csambauart_enable_clocks(port);

	/*
	 * Allocate the IRQ
	 */
	switch (port->mapbase) {
	case UART1_BASE:
		retval = request_irq(port->irq, csambauart_int, 0, "uart1",
				     port);
		break;
	case UART2_BASE:
		retval = request_irq(port->irq, csambauart_int, 0, "uart2",
				     port);
		break;
	case UART3_BASE:
		retval = request_irq(port->irq, csambauart_int, 0, "uart3",
				     port);
		break;
	}
	if (retval)
		return retval;

	/*
	 * initialise the old status of the modem signals
	 */
	uap->old_status = UART_GET_FR(port) & AMBA_UARTFR_MODEM_ANY;

	/*
	 * Finally, enable interrupts
	 */
	UART_PUT_CR(port, AMBA_UARTCR_UARTEN | AMBA_UARTCR_RIE |
			  AMBA_UARTCR_RTIE);

	return 0;
}

static void csambauart_shutdown(struct uart_port *port)
{
	/*
	 * Free the interrupt
	 */
	free_irq(port->irq, port);

	/*
	 * disable all interrupts, disable the port
	 */
	UART_PUT_CR(port, 0);

	/* disable break condition and fifos */
	UART_PUT_LCRH(port, UART_GET_LCRH(port) &
		~(AMBA_UARTLCR_H_BRK | AMBA_UARTLCR_H_FEN));

	if (!(port->cons && port->cons->index == port->line))
		csambauart_disable_clocks( port );
}
static void csambauart_change_speed(struct uart_port *port, unsigned int cflag, unsigned int iflag, unsigned int quot)
{
	unsigned int lcr_h, old_cr;
	unsigned long flags;

	/*
	 * Before we change speed, check to see if this UART is currently in use.
	 * If it is, wait for the fifo to empty out before changing speed.
	 */
	if( csambauart_is_port_enabled( port ) )
	{
		do{
			flags = UART_GET_FR(port);
		} while( flags & UARTFR_BUSY );
	}
	else
	{
		csambauart_enable_clocks( port );
	}

#if DEBUG
	printk("ambauart_set_cflag(0x%x) called\n", cflag);
#endif
	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5:
		lcr_h = AMBA_UARTLCR_H_WLEN_5;
		break;
	case CS6:
		lcr_h = AMBA_UARTLCR_H_WLEN_6;
		break;
	case CS7:
		lcr_h = AMBA_UARTLCR_H_WLEN_7;
		break;
	default: // CS8
		lcr_h = AMBA_UARTLCR_H_WLEN_8;
		break;
	}
	if (cflag & CSTOPB)
		lcr_h |= AMBA_UARTLCR_H_STP2;
	if (cflag & PARENB) {
		lcr_h |= AMBA_UARTLCR_H_PEN;
		if (!(cflag & PARODD))
			lcr_h |= AMBA_UARTLCR_H_EPS;
	}
	if (port->fifosize > 1)
		lcr_h |= AMBA_UARTLCR_H_FEN;

	spin_lock_irqsave(&port->lock, flags);

	port->read_status_mask = AMBA_UARTRSR_OE;
	if (iflag & INPCK)
		port->read_status_mask |= AMBA_UARTRSR_FE | AMBA_UARTRSR_PE;
	if (iflag & (BRKINT | PARMRK))
		port->read_status_mask |= AMBA_UARTRSR_BE;

	/*
	 * Characters to ignore
	 */
	port->ignore_status_mask = 0;
	if (iflag & IGNPAR)
		port->ignore_status_mask |= AMBA_UARTRSR_FE | AMBA_UARTRSR_PE;
	if (iflag & IGNBRK) {
		port->ignore_status_mask |= AMBA_UARTRSR_BE;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (iflag & IGNPAR)
			port->ignore_status_mask |= AMBA_UARTRSR_OE;
	}

	/*
	 * Ignore all characters if CREAD is not set.
	 */
	if ((cflag & CREAD) == 0)
		port->ignore_status_mask |= UART_DUMMY_RSR_RX;

	old_cr = UART_GET_CR(port) & ~AMBA_UARTCR_MSIE;

	if (UART_ENABLE_MS(port, cflag))
		old_cr |= AMBA_UARTCR_MSIE;

	UART_PUT_CR(port, 0);

	UART_PUT_LCRM(port, 0 );
	UART_PUT_LCRL(port, 0 );
	UART_PUT_LCRH(port, 0 );

	UART_CLEAR_ECR(port);


	/* Set baud rate */
	quot -= 1;
	UART_PUT_LCRM(port, ((quot & 0xf00) >> 8));
	UART_PUT_LCRL(port, (quot & 0xff));

	/*
	 * ----------v----------v----------v----------v-----
	 * NOTE: MUST BE WRITTEN AFTER UARTLCR_M & UARTLCR_L
	 * ----------^----------^----------^----------^-----
	 */
	UART_PUT_LCRH(port, lcr_h);
	UART_PUT_CR(port, old_cr);

	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *csambauart_type(struct uart_port *port)
{
	return port->type == PORT_AMBA ? "AMBA" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'
 */
static void csambauart_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, UART_PORT_SIZE);
}

/*
 * Request the memory region(s) being used by 'port'
 */
static int csambauart_request_port(struct uart_port *port)
{
	char *name = 0;

	switch(port->mapbase)
	{
		case UART1_BASE:
			name = "uart1";
			break;
		case UART2_BASE:
			name = "uart2";
			break;
		case UART3_BASE:
			name = "uart3";
			break;
	}
	return request_mem_region(port->mapbase, UART_PORT_SIZE,
				  name) != NULL ? 0 : -EBUSY;
}

/*
 * Configure/autoconfigure the port.
 */
static void csambauart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_AMBA;
		csambauart_request_port(port);
	}
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int csambauart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	int ret = 0;
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_AMBA)
		ret = -EINVAL;
	if (ser->irq < 0 || ser->irq >= NR_IRQS)
		ret = -EINVAL;
	if (ser->baud_base < 9600)
		ret = -EINVAL;
	return ret;
}

static struct uart_ops amba_pops = {
	.tx_empty	= csambauart_tx_empty,
	.set_mctrl	= csambauart_set_mctrl,
	.get_mctrl	= csambauart_get_mctrl,
	.stop_tx	= csambauart_stop_tx,
	.start_tx	= csambauart_start_tx,
	.stop_rx	= csambauart_stop_rx,
	.enable_ms	= csambauart_enable_ms,
	.break_ctl	= csambauart_break_ctl,
	.startup	= csambauart_startup,
	.shutdown	= csambauart_shutdown,
	.change_speed	= csambauart_change_speed,
	.type		= csambauart_type,
	.release_port	= csambauart_release_port,
	.request_port	= csambauart_request_port,
	.config_port	= csambauart_config_port,
	.verify_port	= csambauart_verify_port,
};

static struct uart_amba_port amba_ports[UART_NR] = {
	{
		.port	= {
			.membase	= (void *)UART1_BASE,
			.mapbase	= UART1_BASE,
			.iotype		= SERIAL_IO_MEM,
			.irq		= IRQ_UART1,
			.uartclk	= 14745600,
			.fifosize	= 8,
			.ops		= &amba_pops,
			.flags		= ASYNC_BOOT_AUTOCONF,
			.line		= 0,
		},
		.dtr_mask	= 1 << 0,
		.rts_mask	= 1 << 1,
	},
#if !defined(CONFIG_EP93XX_IRDA)
	{
		.port	= {
			.membase	= (void *)UART2_BASE,
			.mapbase	= UART2_BASE,
			.iotype		= SERIAL_IO_MEM,
			.irq		= IRQ_UART2,
			.uartclk	= 14745600,
			.fifosize	= 8,
			.ops		= &amba_pops,
			.flags		= ASYNC_BOOT_AUTOCONF,
			.line		= 1,
		},
		.dtr_mask	= 0,
		.rts_mask	= 0,
	},
#endif
#if !defined(CONFIG_ARCH_EP9301) && !defined(CONFIG_ARCH_EP9302)
	{
		.port	= {
			.membase	= (void *)UART3_BASE,
			.mapbase	= UART3_BASE,
			.iotype		= SERIAL_IO_MEM,
			.irq		= IRQ_UART3,
			.uartclk	= 14745600,
			.fifosize	= 8,
			.ops		= &amba_pops,
			.flags		= ASYNC_BOOT_AUTOCONF,
#if !defined(CONFIG_EP93XX_IRDA)
			.line		= 2,
#else
			.line		= 1,
#endif
		},
		.dtr_mask	= 0,
		.rts_mask	= 0,
	}
#endif
};

#ifdef CONFIG_SERIAL_EP93XX_CONSOLE

static void csambauart_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_port *port = &amba_ports[co->index].port;
	unsigned int status, old_cr;
	int i;

	/*
	 * First save the CR then disable the interrupts
	 */
	old_cr = UART_GET_CR(port);
	UART_PUT_CR(port, AMBA_UARTCR_UARTEN);

	/*
	 * Now, do each character
	 */
	for (i = 0; i < count; i++) {
		do {
			status = UART_GET_FR(port);
		} while (!UART_TX_READY(status));
		UART_PUT_CHAR(port, s[i]);
		if (s[i] == '\n') {
			do {
				status = UART_GET_FR(port);
			} while (!UART_TX_READY(status));
			UART_PUT_CHAR(port, '\r');
		}
	}

	/*
	 * Finally, wait for transmitter to become empty
	 * and restore the TCR
	 */
	do {
		status = UART_GET_FR(port);
	} while (status & AMBA_UARTFR_BUSY);
	UART_PUT_CR(port, old_cr);
}

static kdev_t csambauart_console_device(struct console *co)
{
	return MKDEV(SERIAL_AMBA_MAJOR, SERIAL_AMBA_MINOR + co->index);
}

/*
 * csambauart_console_get_options
 *
 * Read the current settings of the serial port so that we can call
 * uart_set_options to set the serial port to be the way it already is.
 * Make sense?
 */
#if 0
static void __init
csambauart_console_get_options(struct uart_port *port, int *baud, int *parity, int *bits)
{
	if (UART_GET_CR(port) & AMBA_UARTCR_UARTEN) {
		unsigned int lcr_h, quot;
		lcr_h = UART_GET_LCRH(port);

		*parity = 'n';
		if (lcr_h & AMBA_UARTLCR_H_PEN) {
			if (lcr_h & AMBA_UARTLCR_H_EPS)
				*parity = 'e';
			else
				*parity = 'o';
		}

		if ((lcr_h & 0x60) == AMBA_UARTLCR_H_WLEN_7)
			*bits = 7;
		else
			*bits = 8;

		quot = UART_GET_LCRL(port) | UART_GET_LCRM(port) << 8;
		*baud = port->uartclk / (16 * (quot + 1));
	}
}
#endif

static int __init csambauart_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 57600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (co->index >= UART_NR)
		co->index = 0;
	port = &amba_ports[co->index].port;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	/*
	 * By #if 0-ing this out, we get to set the console baud rate
	 * right here in csambauart_console_setup.
	 */
#if 0
	else
		csambauart_console_get_options(port, &baud, &parity, &bits);
#endif

	return uart_set_options(port, co, baud, parity, bits, flow);
}

/*
 * The 'index' element of this field selects which UART to use for
 * console.  For ep93xx, valid values are 0, 1, and 2.  If you set
 * it to -1, then uart_get_console will search for the first UART
 * which is the same as setting it to 0.
 */
static struct console cs_amba_console = {
	.name		= "ttyAM",
	.write		= csambauart_console_write,
	.device		= csambauart_console_device,
	.setup		= csambauart_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

void __init ep93xxuart_console_init(void)
{
	register_console(&cs_amba_console);
}

#define AMBA_CONSOLE	&cs_amba_console
#else
#define AMBA_CONSOLE	NULL
#endif

static struct uart_driver amba_reg = {
	.owner			= THIS_MODULE,
	.normal_major		= SERIAL_AMBA_MAJOR,
#ifdef CONFIG_DEVFS_FS
	.normal_name		= "ttyAM%d",
	.callout_name		= "cuaam%d",
#else
	.normal_name		= "ttyAM",
	.callout_name		= "cuaam",
#endif
	.normal_driver		= &normal,
	.callout_major		= CALLOUT_AMBA_MAJOR,
	.callout_driver		= &callout,
	.table			= cs_amba_table,
	.termios		= cs_amba_termios,
	.termios_locked		= cs_amba_termios_locked,
	.minor			= SERIAL_AMBA_MINOR,
	.nr			= UART_NR,
	.cons			= AMBA_CONSOLE,
};

static int __init csambauart_init(void)
{
	int ret;

	ret = uart_register_driver(&amba_reg);
	if (ret == 0) {
		int i;

		for (i = 0; i < UART_NR; i++)
			uart_add_one_port(&amba_reg, &amba_ports[i].port);
	}
	return ret;
}

static void __exit csambauart_exit(void)
{
	int i;

	for (i = 0; i < UART_NR; i++)
		uart_remove_one_port(&amba_reg, &amba_ports[i].port);

	uart_unregister_driver(&amba_reg);
}

module_init(csambauart_init);
module_exit(csambauart_exit);

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("ARM Ltd/Deep Blue Solutions Ltd/Cirrus Logic, Inc.");
MODULE_DESCRIPTION("EP9312 ARM AMBA serial port driver");
MODULE_LICENSE("GPL");


void myputchar(char c)
{
	unsigned int status;
#ifdef CONFIG_MACH_IPD
	static unsigned int membase = UART2_BASE;
#else
	static unsigned int membase = UART1_BASE;
#endif

	do {
		status = readl(membase+AMBA_UARTFR) & 0xff;
	} while (!UART_TX_READY(status));

	writel(c, membase+AMBA_UARTDR);

	do {
		status = readl(membase+AMBA_UARTFR) & 0xff;
	} while (status & AMBA_UARTFR_BUSY);
}
