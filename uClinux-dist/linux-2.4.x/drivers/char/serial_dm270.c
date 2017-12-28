/***********************************************************************
 * drivers/char/serial_dm270.c
 *
 *   Derived from drivers/char/serial_c5471.c
 *
 *   Copyright (C) 2004 InnoMedia Pte Ltd. All rights reserved.
 *   cheetim_loh@innomedia.com.sg  <www.innomedia.com>
 *
 *  This implementation was leveraged from the standard
 *  serial.c Linux file.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
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
 *
 ***********************************************************************/

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif

/* All of the compatibilty code so we can compile serial.c against
 * older kernels is hidden in serial_compat.h
 */

#if defined(LOCAL_HEADERS) || (LINUX_VERSION_CODE < 0x020317) /* 2.3.23 */
#include <linux/compatmac.h>
#endif

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/serial.h>
#include <asm/arch/irqs.h>

/* Set of debugging defines */
#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
#undef SERIAL_DEBUG_AUTOCONF
#undef SERIAL_DEBUG_THROTTLE

#define SERIAL_DEV_OFFSET	0

/* serial subtype definitions */
#ifndef SERIAL_TYPE_NORMAL
#define SERIAL_TYPE_NORMAL      1
#define SERIAL_TYPE_CALLOUT     2
#endif

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS          256

#define SERIAL_PARANOIA_CHECK
#define SERIAL_DO_RESTART

#ifdef MODULE
#undef CONFIG_SERIAL_DM270_CONSOLE
#endif

#define RS_STROBE_TIME (10*HZ)
#define RS_ISR_PASS_LIMIT 256

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define DEFAULT_CONSOLE_BAUD   38400  /* 38,400 buad */
#define DEFAULT_CONSOLE_BITS   8       /* 8 data bits */
#define DEFAULT_CONSOLE_PARITY 'n'     /* No parity */

/* #define DEBUG_ENABLED 1 */

#if (defined(CONFIG_DEBUG_LL) && defined(DEBUG_ENABLED))

/* printch and printhex8 are declared in
 * linux/arch/armnommu/kernel/debug-armv.S\
 */

extern void printch(unsigned char ch);
extern void printhex8(unsigned long val);

# define CHECKPOINT(ch) printch(ch)
# define SHOWVALUE(val) show_value(val)

static void show_value(unsigned long val)
{
  printch('['); printhex8(val); printch(']');
}

#else
# define CHECKPOINT(ch)
# define SHOWVALUE(val)
#endif

/* IRQ_timeout - How long the timeout should be for each IRQ
 * 		 should be after the IRQ has been active.
 */

static dm270_async_t *IRQ_ports[NR_IRQS];
static int IRQ_timeout[NR_IRQS];

static char *serial_name = "TI TMS320DM270 Serial driver";
static char *serial_version = "0.01";

DECLARE_TASK_QUEUE(tq_serial);
struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

#if defined(CONFIG_SERIAL_DM270_POLL) || defined(CONFIG_SERIAL_DM270_TIMEOUT_POLL)
static struct timer_list serial_timer;
#endif

/* Serial Console Driver */

#ifdef CONFIG_SERIAL_DM270_CONSOLE

/* This block is enabled when the user has used `make xconfig`
 * to enable kernel printk() to come out the serial port.
 * The register_console(serial_console_print) call below
 * is what hooks in our serial output routine here with the
 * kernel's printk output.
 */

#include <linux/console.h>

static dm270_async_t async_sercons;

static void serial_console_write(struct console *co, const char *s,
				 unsigned count);
static kdev_t serial_console_device(struct console *co);
static int serial_console_setup(struct console *co, char *options) __init;

static struct console sercons =
{
  name:		"ttyS",
  write:	serial_console_write,
  device:	serial_console_device,
  setup:	serial_console_setup,
  flags:	CON_PRINTBUFFER,
  index:	-1,
};

#if defined(CONFIG_MAGIC_SYSRQ)
static unsigned long break_pressed; /* break, really ... */
#endif
#endif /* CONFIG_SERIAL_DM270_CONSOLE */

static void autoconfig(dm270_serial_state_t *state);
static void change_speed(dm270_async_t *info, struct termios *old);
static void rs_wait_until_sent(struct tty_struct *tty, int timeout);

#define SERIAL_DRIVER_NAME "DM270 UART"

/* tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */

static unsigned char *tmp_buf = 0;
#ifdef DECLARE_MUTEX
static DECLARE_MUTEX(tmp_buf_sem);
#else
static struct semaphore tmp_buf_sem = MUTEX;
#endif

#ifdef CONFIG_ARCH_DM270
/*-------------------------------------------------------------------------
 *       -- Begin; Platform Specifics --
 *   Note: These definitions and routines will
 *         surely need to change if porting this
 *         driver to another platform.
 *-------------------------------------------------------------------------*/

static dm270_serial_state_t rs_table[RS_TABLE_SIZE] =
{
	SERIAL_PORT_DFNS        /* Defined in asm/serial.h */
};

#define NR_PORTS (sizeof(rs_table)/sizeof(dm270_serial_state_t))

static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

static inline unsigned short serial_in(dm270_async_t *info,
				      unsigned long offset)
{
	return inw(info->iomem_base + offset);
}

static inline void serial_out(dm270_async_t *info,
                              unsigned long offset,
                              unsigned short value)
{
	outw(value, info->iomem_base + offset);
}

static inline void serial_reset_chip(dm270_async_t *info)
{
	unsigned short reg;

	/* Disable clock to UART. */
	reg = inw(DM270_CLKC_MOD2);
	outw((reg & ~(DM270_CLKC_MOD2_CUAT << info->line)), DM270_CLKC_MOD2);

	/* Select CLK_ARM as UART clock source */
	reg = inw(DM270_CLKC_CLKC);
	outw((reg & ~(DM270_CLKC_CLKC_CUAS << info->line)), DM270_CLKC_CLKC);

	/* Enable clock to UART. */
	reg = inw(DM270_CLKC_MOD2);
	outw((reg | (DM270_CLKC_MOD2_CUAT << info->line)), DM270_CLKC_MOD2);

	if (info->line == 1) {
		/* Configure GIO23 & GIO24 as RXD1 & TXD1 respectively */
		reg = inw(DM270_GIO_FSEL0);
		outw(reg | DM270_GIO_FSEL_RXD1, DM270_GIO_FSEL0);
		reg = inw(DM270_GIO_FSEL1);
		outw(reg | DM270_GIO_FSEL_TXD1, DM270_GIO_FSEL1);
	}
}

static inline void serial_disable_tx_int(dm270_async_t *info)
{
	info->msr &= ~DM270_UART_MSR_TFTIE;
	serial_out(info, DM270_UART_MSR, info->msr);
}

static inline void serial_disable_rx_int(dm270_async_t *info)
{
	info->msr &= ~(DM270_UART_MSR_TOIC_MASK | DM270_UART_MSR_REIE |
			DM270_UART_MSR_RFTIE);
	serial_out(info, DM270_UART_MSR, info->msr);
}

static inline void serial_enable_tx_int(dm270_async_t *info)
{
	info->msr |= DM270_UART_MSR_TFTIE;
	serial_out(info, DM270_UART_MSR, info->msr);
}

static inline void serial_enable_rx_int(dm270_async_t *info)
{
	info->msr = (info->msr & ~DM270_UART_MSR_TOIC_MASK) |
			(DM270_UART_MSR_TIMEOUT_7 | DM270_UART_MSR_REIE |
			 DM270_UART_MSR_RFTIE);
	serial_out(info, DM270_UART_MSR, info->msr);
}

static inline void serial_disable_ints(dm270_async_t *info, unsigned short *msr)
{
	*msr = info->msr & (DM270_UART_MSR_TOIC_MASK | DM270_UART_MSR_REIE |
			DM270_UART_MSR_TFTIE | DM270_UART_MSR_RFTIE);
	info->msr &= ~(DM270_UART_MSR_TOIC_MASK | DM270_UART_MSR_REIE |
			DM270_UART_MSR_TFTIE | DM270_UART_MSR_RFTIE);
	serial_out(info, DM270_UART_MSR, info->msr);
}

static inline void serial_restore_ints(dm270_async_t *info, unsigned short msr)
{
	info->msr = (info->msr & ~(DM270_UART_MSR_TOIC_MASK | DM270_UART_MSR_REIE |
			DM270_UART_MSR_TFTIE | DM270_UART_MSR_RFTIE)) |
			(msr & (DM270_UART_MSR_TOIC_MASK | DM270_UART_MSR_REIE |
			DM270_UART_MSR_TFTIE | DM270_UART_MSR_RFTIE));
	serial_out(info, DM270_UART_MSR, info->msr);
}

static inline void serial_clear_fifos(dm270_async_t *info)
{
	serial_out(info, DM270_UART_TFCR,
			serial_in(info, DM270_UART_TFCR) | DM270_UART_TFCR_CLEAR);
	serial_out(info, DM270_UART_RFCR,
			serial_in(info, DM270_UART_RFCR) |
			(DM270_UART_RFCR_RESET | DM270_UART_RFCR_CLEAR));
}

static inline void serial_disable_breaks(dm270_async_t *info)
{
	serial_out(info, DM270_UART_LCR,
			serial_in(info, DM270_UART_LCR) & ~DM270_UART_LCR_BOC);
}

static inline void serial_enable_breaks(dm270_async_t *info)
{
	serial_out(info, DM270_UART_LCR,
			serial_in(info, DM270_UART_LCR) | DM270_UART_LCR_BOC);
}

static inline void serial_set_rate(dm270_async_t *info,
				   unsigned int rate)
{
	unsigned short div_bit_rate;

	SHOWVALUE(rate);
	SHOWVALUE(serial_in(info, DM270_UART_BRSR));

	div_bit_rate = DM270_UART_BRSR_VAL(rate);
	SHOWVALUE(div_bit_rate);

	serial_out(info, DM270_UART_BRSR, div_bit_rate);
}

static inline void serial_set_mode(dm270_async_t *info, unsigned short mode)
{
	info->msr = (info->msr & ~(DM270_UART_MSR_CLS | DM270_UART_MSR_SBLS |
			DM270_UART_MSR_PSB | DM270_UART_MSR_PEB)) | mode;
	serial_out(info, DM270_UART_MSR, info->msr);
}

static inline void serial_set_rx_trigger(dm270_async_t *info, unsigned short val)
{
	serial_out(info, DM270_UART_RFCR, (serial_in(info, DM270_UART_RFCR) &
			~(DM270_UART_RFCR_RTL_MASK | DM270_UART_RFCR_RESET |
			DM270_UART_RFCR_CLEAR)) | val);
}

static inline void serial_set_tx_trigger(dm270_async_t *info, unsigned short val)
{
	serial_out(info, DM270_UART_TFCR, (serial_in(info, DM270_UART_TFCR) &
			~(DM270_UART_TFCR_TTL_MASK | DM270_UART_TFCR_CLEAR)) |
			val);
}

static inline void serial_char_out(dm270_async_t *info, u8 val)
{
	serial_out(info, DM270_UART_DTRR, val);
}

static inline unsigned char serial_char_in(dm270_async_t *info,
                                           unsigned short *status)
{
	unsigned short dtrr;

	dtrr = serial_in(info, DM270_UART_DTRR);
	*status = (dtrr & 0xff00);
	return ((unsigned char)(dtrr & 0x00ff));
}

static inline int serial_some_error_condition_present(unsigned short status)
{
	return ((status ^ DM270_UART_DTRR_RVF) &
			(DM270_UART_DTRR_RVF | DM270_UART_DTRR_BF | DM270_UART_DTRR_FE |
			 DM270_UART_DTRR_ORF | DM270_UART_DTRR_PEF));
}

static inline int serial_break_condition_present(unsigned short status)
{
	return (status & DM270_UART_DTRR_BF);
}

static inline int serial_parity_error_condition_present(unsigned short status)
{
	return (status & DM270_UART_DTRR_PEF);
}

static inline int serial_framing_error_condition_present(unsigned short status)
{
	return (status & DM270_UART_DTRR_FE);
}

static inline int serial_overrun_error_condition_present(unsigned short status)
{
	return (status & DM270_UART_DTRR_ORF);
}

static inline int serial_received_word_invalid_condition_present(unsigned short status)
{
	return (status & DM270_UART_DTRR_RVF);
}

static inline unsigned short serial_tx_fifo_is_empty(dm270_async_t *info)
{
	return (serial_in(info, DM270_UART_SR) & DM270_UART_SR_TREF);
}

static inline unsigned short serial_room_in_tx_fifo(dm270_async_t *info)
{
	return ((serial_in(info, DM270_UART_TFCR) & DM270_UART_TFCR_TWC_MASK) < DM270_UART_TXFIFO_BYTESIZE);
}

static inline unsigned short serial_rx_fifo_has_content(dm270_async_t *info)
{
	return (serial_in(info, DM270_UART_SR) & DM270_UART_SR_RFNEF);
}

static inline void serial_save_registers(dm270_async_t *info)
{
	info->msr = serial_in(info, DM270_UART_MSR);
}

/*-------------------------------------------------------------------------
 *       -- End; Platform Specifics --
 *-------------------------------------------------------------------------*/

#endif

static inline int serial_paranoia_check(dm270_async_t *info,
					kdev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char * const badmagic =
		"Warning: bad magic number for serial struct (%s) in %s\n";
	static const char * const badinfo =
		"Warning: null dm270_async_t for (%s) in %s\n";

	if (!info) {
		printk(badinfo, kdevname(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}

/* rs_stop() and rs_start()
 *
 * These routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 */

static void rs_stop(struct tty_struct *tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop")) {
		return;
	}
	
	save_flags(flags); cli();
	if (info->msr & DM270_UART_MSR_TFTIE) {
		serial_disable_tx_int(info);
	}
	restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_start")) {
		return;
	}
        
	save_flags(flags); cli();

	if ((info->xmit.head != info->xmit.tail) && (info->xmit.buf) &&
			!(info->msr & DM270_UART_MSR_TFTIE)) {
		serial_enable_tx_int(info);
	}

	restore_flags(flags);
}

/* ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * rs_interrupt().  They were separated out for readability's sake.
 *
 * Note: rs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * rs_interrupt() should try to keep the interrupt handler as fast as
 * possible.  After you are done making modifications, it is not a bad
 * idea to do:
 * 
 * gcc -S -DKERNEL -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer serial.c
 *
 * and look at the resulting assemble code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

/* This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */

static inline void rs_sched_event(dm270_async_t *info,
				  int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static inline void receive_chars(dm270_async_t *info)
{
	struct tty_struct *tty = info->tty;
	struct async_icount *icount;
	unsigned short status;
	unsigned char ch;
	int max_count = 256;

	icount = &info->state->icount;
	do {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			break;

		ch = serial_char_in(info, &status);
		*tty->flip.char_buf_ptr = ch;
		icount->rx++;

#ifdef SERIAL_DEBUG_INTR
		printk("DR%02x:%04x...", ch, status);
#endif
		*tty->flip.flag_buf_ptr = TTY_NORMAL;
		if (serial_some_error_condition_present(status)) {
			/* For statistics only */
			if (serial_break_condition_present(status)) {
				status &= ~(DM270_UART_DTRR_FE | DM270_UART_DTRR_PEF);
				icount->brk++;
				/* We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
#if defined(CONFIG_SERIAL_DM270_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
				if (info->line == sercons.index) {
					if (!break_pressed) {
						break_pressed = jiffies;
						goto ignore_char;
					}
					break_pressed = 0;
				}
#endif
				if (info->flags & ASYNC_SAK)
					do_SAK(tty);
			} else if (serial_parity_error_condition_present(status)) {
				icount->parity++;
			} else if (serial_framing_error_condition_present(status)) {
				icount->frame++;
			}
			if (serial_overrun_error_condition_present(status))
				icount->overrun++;

			/* Now check to see if character should be
			 * ignored, and mask off conditions which
			 * should be ignored.
			 */
			status &= info->read_status_mask;

			if (serial_break_condition_present(status)) {
#ifdef SERIAL_DEBUG_INTR
				printk("handling break....");
#endif
				*tty->flip.flag_buf_ptr = TTY_BREAK;
			} else if (serial_parity_error_condition_present(status)) {
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			} else if (serial_framing_error_condition_present(status)) {
				*tty->flip.flag_buf_ptr = TTY_FRAME;
			}
		}
#if defined(CONFIG_SERIAL_DM270_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
		if (break_pressed && info->line == sercons.index) {
			if (ch != 0 &&
					time_before(jiffies, break_pressed + HZ*5)) {
				handle_sysrq(ch, regs, NULL, NULL);
				break_pressed = 0;
				goto ignore_char;
			}
			break_pressed = 0;
		}
#endif
		if ((status & info->ignore_status_mask) == 0) {
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if (serial_overrun_error_condition_present(status) &&
				(tty->flip.count < TTY_FLIPBUF_SIZE)) {
			/* Overrun is special, since it's
			 * reported immediately, and doesn't
			 * affect the current character
			 */
			*tty->flip.flag_buf_ptr = TTY_OVERRUN;
			tty->flip.count++;
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
		}
#if defined(CONFIG_SERIAL_DM270_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
	ignore_char:
#endif
	} while (serial_rx_fifo_has_content(info) && (max_count-- > 0));

#if (LINUX_VERSION_CODE > 131394) /* 2.1.66 */
	tty_flip_buffer_push(tty);
#else
	queue_task(&tty->flip.tqueue, &tq_timer);
#endif	
}

static inline void transmit_chars(dm270_async_t *info, int *intr_done)
{
	int count;

	if (info->x_char) {
		serial_char_out(info, info->x_char);
		info->state->icount.tx++;
		info->x_char = 0;
		if (intr_done)
			*intr_done = 0;
		return;
	}

	if ((info->xmit.head == info->xmit.tail) ||
		(info->tty->stopped) ||
		(info->tty->hw_stopped)) {
		serial_disable_tx_int(info);
		return;
	}

	/* Send while we still have data & room in the fifo */

	count = info->xmit_fifo_size;
	do {
		serial_char_out(info, info->xmit.buf[info->xmit.tail++]);
		info->xmit.tail &= (SERIAL_XMIT_SIZE-1);
		info->state->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	} while (--count > 0);

	if (CIRC_CNT(info->xmit.head,
				info->xmit.tail,
				SERIAL_XMIT_SIZE) < WAKEUP_CHARS) {
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
	}

#ifdef SERIAL_DEBUG_INTR
	printk("THRE...");
#endif
	if (intr_done) {
		*intr_done = 0;
	}

	if (info->xmit.head == info->xmit.tail) {
		serial_disable_tx_int(info);
	}
}

/* This routine is used to handle the "top half" processing for the
 * serial driver.
 */

static void rs_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	dm270_async_t     *info;
	unsigned short    status;
	int               pass_counter = 0;

#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt(%d)...", irq);
#endif

	info = IRQ_ports[irq];
	if (!info || !info->tty)
		return;

	status = serial_in(info, DM270_UART_SR);
#ifdef SERIAL_DEBUG_INTR
	printk("status=%x...", status);
#endif
	while (status  & (DM270_UART_SR_RFNEF | DM270_UART_SR_TFEF)) {
		if (status & DM270_UART_SR_RFNEF)
			receive_chars(info);

		if (status & DM270_UART_SR_TFEF)
			transmit_chars(info, 0);

		if (pass_counter++ > RS_ISR_PASS_LIMIT) {
#ifdef SERIAL_DEBUG_INTR
			printk("rs_interrupt loop break.\n");
#endif
			break;
		}

		status = serial_in(info, DM270_UART_SR);
#ifdef SERIAL_DEBUG_INTR
		printk("status=%x...", status);
#endif
	}

	info->last_active = jiffies;
#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}

/* -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

/* This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * rs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using rs_sched_event(), and they get done here.
 */

static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	dm270_async_t     *info = (dm270_async_t *) private_;
	struct tty_struct *tty;
	
	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
				tty->ldisc.write_wakeup) {
			(tty->ldisc.write_wakeup)(tty);
		}
		wake_up_interruptible(&tty->write_wait);
#ifdef SERIAL_HAVE_POLL_WAIT
		wake_up_interruptible(&tty->poll_wait);
#endif
	}
}

#if defined(CONFIG_SERIAL_DM270_POLL) || defined(CONFIG_SERIAL_DM270_TIMEOUT_POLL)
/* This subroutine is called when the RS_TIMER goes off.  It is used
 * by the serial driver to handle ports that do not have an interrupt
 * (irq=0).  This doesn't work very well for 16450's, but gives barely
 * passable results for a 16550A.  (Although at the expense of much
 * CPU overhead).
 */

static void rs_timer(unsigned long dummy)
{
	dm270_async_t	*info;
	static unsigned long last_strobe;
	unsigned int	i;
	unsigned long flags;

#ifdef CONFIG_SERIAL_DM270_POLL
	/* assumes RS_STROBE_TIME < IRQ_timeout[0] */
	if ((jiffies - last_strobe) >= RS_STROBE_TIME)
	{
		for (i=0; i < NR_IRQS; i++) {
			info = IRQ_ports[i];
			if (!info)
				continue;
			save_flags(flags); cli();
			rs_interrupt(i, NULL, NULL);
			restore_flags(flags);
		}
		last_strobe = jiffies;
		mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);
	}
	else
#endif

	{
#ifdef CONFIG_SERIAL_DM270_TIMEOUT_POLL
		if (IRQ_ports[0]) {
			save_flags(flags); cli();
			rs_interrupt(0, NULL, NULL);
			restore_flags(flags);

			mod_timer(&serial_timer, jiffies + IRQ_timeout[0]);
		}
#endif
	}
}
#endif

/* ---------------------------------------------------------------
 * Low level utility subroutines for the serial driver:  routines to
 * figure out the appropriate timeout for an interrupt chain, routines
 * to initialize and startup a serial port, and routines to shutdown a
 * serial port.  Useful stuff like that.
 * ---------------------------------------------------------------
 */

#ifdef CONFIG_SERIAL_DM270_TIMEOUT_POLL
/* This routine figures out the correct timeout for a particular IRQ.
 * It uses the smallest timeout of all of the serial ports in a
 * particular interrupt chain.  Now only used for IRQ 0....
 */

static void figure_IRQ_timeout(int irq)
{
	dm270_async_t	*info;
	int	         timeout = 60*HZ; /* 60 secs === a long time :-) */

	info = IRQ_ports[irq];
	if (!info) {
		IRQ_timeout[irq] = 60*HZ;
		return;
	}
	while (info) {
		if (info->timeout < timeout)
			timeout = info->timeout;
		info = info->next_port;
	}
	if (!irq)
		meout = timeout / 2;
	IRQ_timeout[irq] = (timeout > 3) ? timeout-2 : 1;
}
#endif

static int startup(dm270_async_t *info)
{
	dm270_serial_state_t *state = info->state;
	unsigned long         flags;
	unsigned long         page;
	unsigned short        junk;
	int                   retval = 0;

	page = get_zeroed_page(GFP_KERNEL);
	if (!page) {
		return -ENOMEM;
	}

	save_flags(flags); cli();

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (!CONFIGURED_SERIAL_PORT(info) || !state->type) {
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		goto errout;
	}

	if (info->xmit.buf) {
		free_page(page);
	} else {
		info->xmit.buf = (unsigned char *) page;
	}

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttyS%d (irq %d)...", info->line, state->irq);
#endif
	serial_save_registers(info);
	serial_disable_ints(info, &junk);
	serial_clear_fifos(info);
	serial_disable_breaks(info);	

	/* Allocate the IRQ if necessary */

	if ((state->irq) &&
			((!IRQ_ports[state->irq]) ||
			 (!IRQ_ports[state->irq]->next_port))) {
		if (IRQ_ports[state->irq]) {
			retval = -EBUSY;
			goto errout;
		}

		retval = request_irq(state->irq, rs_interrupt, SA_INTERRUPT,
				"serial", NULL);
		if (retval) {
			if (capable(CAP_SYS_ADMIN)) {
				if (info->tty)
					set_bit(TTY_IO_ERROR,
							&info->tty->flags);
				retval = 0;
			}
			goto errout;
		}
	}

	/* Insert serial port into IRQ chain. */

	info->prev_port = 0;
	info->next_port = IRQ_ports[state->irq];
	if (info->next_port)
		info->next_port->prev_port = info;
	IRQ_ports[state->irq] = info;
#ifdef CONFIG_SERIAL_DM270_TIMEOUT_POLL
	figure_IRQ_timeout(state->irq);
#endif

	/* Finally, enable interrupts */

	serial_enable_rx_int(info);

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit.head = info->xmit.tail = 0;

	/* Set up serial timers... */

#if defined(CONFIG_SERIAL_DM270_POLL) || defined(CONFIG_SERIAL_DM270_TIMEOUT_POLL)
/*	mod_timer(&serial_timer, jiffies + 2*HZ/100);*/
	mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);
#endif

	/* Set up the tty->alt_speed kludge */

#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
	if (info->tty) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			info->tty->alt_speed = 57600;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			info->tty->alt_speed = 115200;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			info->tty->alt_speed = 230400;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			info->tty->alt_speed = 460800;
	}
#endif

	/* And set the speed of the serial port */

	change_speed(info, 0);

	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;

errout:
	restore_flags(flags);
	return retval;
}

/* This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */

static void shutdown(dm270_async_t * info)
{
	unsigned long         flags;
	dm270_serial_state_t *state;
	int                   retval;
	unsigned short        junk;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	state = info->state;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....", info->line,
			state->irq);
#endif
	save_flags(flags); cli(); /* Disable interrupts */

	/* clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */

	wake_up_interruptible(&info->delta_msr_wait);

	/* First unlink the serial port from the IRQ chain... */

	if (info->next_port)
		info->next_port->prev_port = info->prev_port;
	if (info->prev_port)
		info->prev_port->next_port = info->next_port;
	else
		IRQ_ports[state->irq] = info->next_port;

#ifdef CONFIG_SERIAL_DM270_TIMEOUT_POLL
	figure_IRQ_timeout(state->irq);
#endif
	
	/* Free the IRQ, if necessary */

	if ((state->irq) &&
			((!IRQ_ports[state->irq]) ||
			 (!IRQ_ports[state->irq]->next_port))) {
		if (IRQ_ports[state->irq]) {
			free_irq(state->irq, NULL);
			retval = request_irq(state->irq, rs_interrupt,
			       SA_INTERRUPT, "serial", NULL);
			if (retval) {
				printk(KERN_WARNING "serial shutdown: request_irq: error %d"
						"  Couldn't reacquire IRQ.\n", retval);
			}
		} else {
			free_irq(state->irq, NULL);
		}
	}

	if (info->xmit.buf) {
		unsigned long pg = (unsigned long)info->xmit.buf;
		info->xmit.buf = 0;
		free_page(pg);
	}

	serial_disable_ints(info, &junk);	/* disable all intrs */
	serial_disable_breaks(info);		/* disable break condition */
	serial_clear_fifos(info);		/* reset FIFO's */	

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);
	
	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */

/* This is used to figure out the divisor speeds and the timeouts */

static int baud_table[] =
{
      0,    50,     75,    110,    134,  150,   200, 300,
    600,  1200,   1800,   2400,   4800, 9600, 19200,
  38400, 57600, 115200, 230400, 460800,    0
};

/* This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */

int tty_get_baud_rate(struct tty_struct *tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned int   cflag;
	unsigned int   i;

	cflag = tty->termios->c_cflag;

	i = cflag & CBAUD;
	if (i & CBAUDEX) {
		i &= ~CBAUDEX;
		if (i < 1 || i > 2) 
			tty->termios->c_cflag &= ~CBAUDEX;
		else
			i += 15;
	}
	if (i == 15) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			i += 1;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			i += 2;
	}
	return baud_table[i];
}
#endif

static void change_speed(dm270_async_t *info, 
			 struct termios *old_termios)
{
	unsigned long   flags;
	unsigned int    cflag;
	unsigned short  cval = 0;
	int             baud;
	int             bits;

	if (!info->tty || !info->tty->termios) {
		return;
	}

	cflag = info->tty->termios->c_cflag;

	if (!CONFIGURED_SERIAL_PORT(info)) {
		return;
	}

	/* Byte size and parity */

	switch (cflag & CSIZE) {
		case CS7:
			cval = DM270_UART_MSR_7_DBITS;
			bits = 9;
			break;
		case CS8:
		default:
			cval = DM270_UART_MSR_8_DBITS;
			bits = 10;
			break;
	}

	if (cflag & CSTOPB) {
		cval |= DM270_UART_MSR_2_SBITS;
		bits++;
	} else {
		cval |= DM270_UART_MSR_1_SBITS;
	}

	if (cflag & PARENB) {
		if (cflag & PARODD) {
			cval |= DM270_UART_MSR_ODD_PARITY;
		} else {
			cval |= DM270_UART_MSR_EVEN_PARITY;
		}
		bits++;
	} else if (cflag & PARODD) {
		cval |= DM270_UART_MSR_ODD_PARITY;
		bits++;
	} else {
		cval |= DM270_UART_MSR_NO_PARITY;
	}

	/* Determine divisor based on baud rate */

	baud = tty_get_baud_rate(info->tty);
	if (!baud) {
		baud = 9600;    /* B0 transition handled in rs_set_termios */
	}

	info->baud = baud;
	info->timeout  = ((info->xmit_fifo_size*HZ*bits) / baud);
	info->timeout += HZ/50;         /* Add .02 seconds of slop */

	/* Set up parity check flag */

#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	info->read_status_mask = (DM270_UART_DTRR_ORF | DM270_UART_DTRR_RVF);
	if (I_INPCK(info->tty))
		info->read_status_mask |= (DM270_UART_DTRR_PEF | DM270_UART_DTRR_FE);
	if (I_BRKINT(info->tty) || I_PARMRK(info->tty))
		info->read_status_mask |= DM270_UART_DTRR_BF;

	/* Characters to ignore */
	info->ignore_status_mask = 0;
	if (I_IGNPAR(info->tty))
		info->ignore_status_mask |= (DM270_UART_DTRR_PEF | DM270_UART_DTRR_FE);
	if (I_IGNBRK(info->tty)) {
		info->ignore_status_mask |= DM270_UART_DTRR_BF;
		/* If we're ignore parity and break indicators, ignore 
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->tty))
			info->ignore_status_mask |= DM270_UART_DTRR_ORF;
	}
	/* !!! ignore all characters if CREAD is not set */
	if ((cflag & CREAD) == 0)
		info->ignore_status_mask |= DM270_UART_DTRR_RVF;

	save_flags(flags); cli();

	serial_set_rate(info, baud);
	serial_set_mode(info, cval);

	/* We should always set the receive FIFO trigger to the lowest value
	 * because we don't poll.
	 */

	serial_set_rx_trigger(info, DM270_UART_RFCR_TRG_1);

	restore_flags(flags);
}

static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned long  flags;

	if (serial_paranoia_check(info, tty->device, "rs_put_char"))
		return;

	if (!tty || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	if (CIRC_SPACE(info->xmit.head,
				info->xmit.tail,
				SERIAL_XMIT_SIZE) == 0) {
		restore_flags(flags);
		return;
	}

	info->xmit.buf[info->xmit.head++] = ch;
	info->xmit.head &= (SERIAL_XMIT_SIZE-1);
	restore_flags(flags);
}

static void rs_flush_chars(struct tty_struct *tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned long  flags;
				
	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	if ((info->xmit.head == info->xmit.tail) ||
			(tty->stopped) || (tty->hw_stopped) ||
			(!info->xmit.buf))
		return;

	save_flags(flags); cli();
	serial_enable_tx_int(info);
	restore_flags(flags);
}

static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned long  flags;
	int            c;
	int            ret = 0;

	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if (!tty || !info->xmit.buf || !tmp_buf)
		return 0;

	save_flags(flags);
	if (from_user) {
		  down(&tmp_buf_sem);
		  while (1) {
			int c1;
			c = CIRC_SPACE_TO_END(info->xmit.head,
					info->xmit.tail,
					SERIAL_XMIT_SIZE);
			if (count < c)
				c = count;
			if (c <= 0)
				break;

			c -= copy_from_user(tmp_buf, buf, c);
			if (!c) {
				if (!ret)
					ret = -EFAULT;
				break;
			}

			cli();
			c1 = CIRC_SPACE_TO_END(info->xmit.head,
					info->xmit.tail,
					SERIAL_XMIT_SIZE);
			if (c1 < c)
				c = c1;

			memcpy(info->xmit.buf + info->xmit.head, tmp_buf, c);
			info->xmit.head = ((info->xmit.head + c) &
					(SERIAL_XMIT_SIZE-1));
			restore_flags(flags);
			buf += c;
			count -= c;
			ret += c;
		}
		up(&tmp_buf_sem);
	} else {
		cli();
		while (1) {
			c = CIRC_SPACE_TO_END(info->xmit.head,
				info->xmit.tail,
				SERIAL_XMIT_SIZE);
			if (count < c)
				c = count;
			if (c <= 0) {
				break;
			}
			memcpy(info->xmit.buf + info->xmit.head, buf, c);
			info->xmit.head = ((info->xmit.head + c) &
			     (SERIAL_XMIT_SIZE-1));
			buf += c;
			count -= c;
			ret += c;
		}
		restore_flags(flags);
	}

	if ((info->xmit.head != info->xmit.tail) &&
			(!tty->stopped) && (!tty->hw_stopped) &&
			!(info->msr & DM270_UART_MSR_TFTIE)) {
		serial_enable_tx_int(info);
	}

	return ret;
}

static int rs_write_room(struct tty_struct *tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;

	save_flags(flags); cli();
	info->xmit.head = info->xmit.tail = 0;
	restore_flags(flags);
	wake_up_interruptible(&tty->write_wait);
#ifdef SERIAL_HAVE_POLL_WAIT
	wake_up_interruptible(&tty->poll_wait);
#endif
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
			tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

/* This function is used to send a high-priority XON/XOFF character to
 * the device
 */

static void rs_send_xchar(struct tty_struct *tty, char ch)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_send_char"))
		return;

	info->x_char = ch;
	if (ch) {
		serial_enable_tx_int(info);
	}
}

/* rs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 */

static void rs_throttle(struct tty_struct * tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char buf[64];

	printk("throttle %s: %d....\n", tty_name(tty, buf),
			tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));
}

static void rs_unthrottle(struct tty_struct * tty)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char buf[64];
	
	printk("unthrottle %s: %d....\n", tty_name(tty, buf),
			tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}
}

/*-------------------------------------------------------------------------
 * rs_ioctl() and friends
 *-------------------------------------------------------------------------*/

static int get_serial_info(dm270_async_t * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct  tmp;
	dm270_serial_state_t *state = info->state;
   
	if (!retinfo)
		return -EFAULT;

	memset(&tmp, 0, sizeof(tmp));
	tmp.type           = state->type;
	tmp.line           = state->line;
	tmp.port           = state->port;
	tmp.port_high      = 0;
	tmp.iomem_base     = (unsigned char *)state->iomem_base;
	tmp.irq            = state->irq;
	tmp.flags          = state->flags;
	tmp.xmit_fifo_size = state->xmit_fifo_size;
	tmp.baud_base      = state->baud_base;
	tmp.close_delay    = state->close_delay;
	tmp.closing_wait   = state->closing_wait;
	tmp.custom_divisor = state->custom_divisor;
	tmp.hub6           = 0;
	tmp.io_type        = state->io_type;

	if (copy_to_user(retinfo,&tmp,sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_serial_info(dm270_async_t * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	dm270_serial_state_t old_state, *state;
	int 		       retval = 0;

	if (copy_from_user(&new_serial, new_info, sizeof(new_serial)))
		return -EFAULT;
	state = info->state;
	old_state = *state;

	/* NOTE:  Changes to the serial port address, IRQ, and serial type
	 * are not supported by this driver
	 */

	if (new_serial.iomem_base != (unsigned char *)state->iomem_base) {
		printk(KERN_INFO "Changes to serial port address not supported\n");
	}

	if (new_serial.irq != state->irq) {
		printk(KERN_INFO "Changes to serial port address not supported\n");
	}

	if (new_serial.type != state->type) {
		printk(KERN_INFO "Changes to serial port type not supported\n");
	}

	if (!capable(CAP_SYS_ADMIN)) {
		if ((new_serial.baud_base != state->baud_base) ||
				(new_serial.close_delay != state->close_delay) ||
				(new_serial.xmit_fifo_size != state->xmit_fifo_size) ||
				((new_serial.flags & ~ASYNC_USR_MASK) !=
				 (state->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		state->flags = ((state->flags & ~ASYNC_USR_MASK) |
				(new_serial.flags & ASYNC_USR_MASK));
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
				(new_serial.flags & ASYNC_USR_MASK));
		state->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	if (new_serial.baud_base < 9600) {
		printk(KERN_INFO "Baud %d not supported\n", new_serial.baud_base);
		return -EINVAL;
	}

	if (new_serial.xmit_fifo_size <= 0) {
		new_serial.xmit_fifo_size = DM270_UART_TXFIFO_BYTESIZE;
	}

	/* OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */

	state->baud_base       = new_serial.baud_base;
	state->flags           = ((state->flags & ~ASYNC_FLAGS) |
					(new_serial.flags & ASYNC_FLAGS));
	info->flags            = ((state->flags & ~ASYNC_INTERNAL_FLAGS) |
					(info->flags & ASYNC_INTERNAL_FLAGS));
	state->custom_divisor  = new_serial.custom_divisor;
	state->close_delay     = new_serial.close_delay * HZ/100;
	state->closing_wait    = new_serial.closing_wait * HZ/100;
#if (LINUX_VERSION_CODE > 0x20100)
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
#endif
	info->xmit_fifo_size = state->xmit_fifo_size = new_serial.xmit_fifo_size;

check_and_exit:

	if (info->flags & ASYNC_INITIALIZED) {
		if (((old_state.flags & ASYNC_SPD_MASK) !=
				(state->flags & ASYNC_SPD_MASK)) ||
				(old_state.custom_divisor != state->custom_divisor)) {
#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
				info->tty->alt_speed = 57600;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
				info->tty->alt_speed = 115200;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
				info->tty->alt_speed = 230400;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
				info->tty->alt_speed = 460800;
#endif
			change_speed(info, 0);
		}
	} else {
		retval = startup(info);
	}
	return retval;
}

/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space. 
 */

static int get_lsr_info(dm270_async_t *info, unsigned short *value)
{
	unsigned char status;
	unsigned int result;
	unsigned long flags;

	save_flags(flags); cli();
	status = serial_in(info, DM270_UART_SR);
	restore_flags(flags);
	result = ((status & DM270_UART_SR_TREF) ? TIOCSER_TEMT : 0);

	/* If we're about to load something into the transmit
	 * register, we'll pretend the transmitter isn't empty to
	 * avoid a race condition (depending on when the transmit
	 * interrupt happens).
	 */

	if ((info->x_char) ||
			((CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE) > 0) &&
			 (!info->tty->stopped && !info->tty->hw_stopped))) {
		result &= ~TIOCSER_TEMT;
	}

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;

	return 0;
}

static int do_autoconfig(dm270_async_t * info)
{
	int retval;
	
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	
	if (info->state->count > 1)
		return -EBUSY;
	
	shutdown(info);
	autoconfig(info->state);
	retval = startup(info);

	if (retval)
		return retval;

	return 0;
}

/* rs_break() --- routine which turns the break handling on or off */

#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
static void send_break(dm270_async_t *info, int duration)
{
	if (!CONFIGURED_SERIAL_PORT(info))
		return;

	current->state = TASK_INTERRUPTIBLE;
	current->timeout = jiffies + duration;
	cli();
	serial_enable_breaks(info);
	schedule();
	serial_disable_breaks(info);
	sti();
}
#else
static void rs_break(struct tty_struct *tty, int break_state)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned long  flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_break"))
		return;

	if (!CONFIGURED_SERIAL_PORT(info))
		return;

	save_flags(flags); cli();

	if (break_state == -1)
		serial_enable_breaks(info);
	else
		serial_disable_breaks(info);

	restore_flags(flags);
}
#endif

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	dm270_async_t                *info = (dm270_async_t *)tty->driver_data;
	struct async_icount           cnow;	/* kernel counter temps */
	struct serial_icounter_struct icount;

	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
			(cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
			(cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
			return -EIO;
	}
	
	switch (cmd) {
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
		case TCSBRK:	/* SVID version: non-zero arg --> no break */
			{
				int retval = tty_check_change(tty);
				if (retval)
					return retval;
				tty_wait_until_sent(tty, 0);
				if (signal_pending(current))
					return -EINTR;
				if (!arg) {
					send_break(info, HZ/4);	/* 1/4 second */
					if (signal_pending(current))
						return -EINTR;
				}
			}
			return 0;

		case TCSBRKP:	/* support for POSIX tcsendbreak() */
			{
				int retval = tty_check_change(tty);
				if (retval)
					return retval;
				tty_wait_until_sent(tty, 0);
				if (signal_pending(current))
					return -EINTR;
				send_break(info, arg ? arg*(HZ/10) : HZ/4);
				if (signal_pending(current))
					return -EINTR;
			}
			return 0;

		case TIOCGSOFTCAR:
			{
				int tmp = C_CLOCAL(tty) ? 1 : 0;
				if (copy_to_user((void *)arg, &tmp, sizeof(int)))
					return -EFAULT;
			}
			return 0;

		case TIOCSSOFTCAR:
			{
				int tmp;

				if (copy_from_user(&tmp, (void *)arg, sizeof(int)))
					return -EFAULT;

				tty->termios->c_cflag =
					((tty->termios->c_cflag & ~CLOCAL) |
					 (tmp ? CLOCAL : 0));
			}
			return 0;
#endif

		case TIOCGSERIAL:
			return get_serial_info(info, (struct serial_struct *) arg);

		case TIOCSSERIAL:
			return set_serial_info(info, (struct serial_struct *) arg);

		case TIOCSERCONFIG:
			return do_autoconfig(info);

		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned short *) arg);

		case TIOCSERGSTRUCT:
			if (copy_to_user((dm270_async_t *) arg, info, sizeof(dm270_async_t))) {
				return -EFAULT;
			}
			return 0;
				
		/* Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */

		case TIOCGICOUNT:
			{
				int flags;

				save_flags(flags); cli();
				cnow = info->state->icount;
				restore_flags(flags);

				icount.cts         = cnow.cts;
				icount.dsr         = cnow.dsr;
				icount.rng         = cnow.rng;
				icount.dcd         = cnow.dcd;
				icount.rx          = cnow.rx;
				icount.tx          = cnow.tx;
				icount.frame       = cnow.frame;
				icount.overrun     = cnow.overrun;
				icount.parity      = cnow.parity;
				icount.brk         = cnow.brk;
				icount.buf_overrun = cnow.buf_overrun;

				if (copy_to_user((void *)arg, &icount, sizeof(icount)))
					return -EFAULT;
			}
			return 0;

		case TIOCSERGWILD:
		case TIOCSERSWILD:
			{
				/* "setserial -W" is called in Debian boot */

				printk ("TIOCSER?WILD ioctl obsolete, ignored.\n");
			}
			return 0;

		default:
			return -ENOIOCTLCMD;
	}
	return 0;
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned int   cflag = tty->termios->c_cflag;

	if ((cflag == old_termios->c_cflag) &&
			(RELEVANT_IFLAG(tty->termios->c_iflag)
			 == RELEVANT_IFLAG(old_termios->c_iflag))) {
		return;
	}

	change_speed(info, old_termios);
}

/*-------------------------------------------------------------------------
 * rs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 *-------------------------------------------------------------------------*/

static void rs_close(struct tty_struct *tty, struct file * filp)
{
	dm270_async_t        *info = (dm270_async_t *)tty->driver_data;
	dm270_serial_state_t *state;
	unsigned long         flags;

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;

	state = info->state;
	
	save_flags(flags); cli();
	
	if (tty_hung_up_p(filp)) {
		  MOD_DEC_USE_COUNT;
		  restore_flags(flags);
		  return;
	}

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_close ttyS%d, count = %d\n", info->line, state->count);
#endif
	if ((tty->count == 1) && (state->count != 1)) {
		/* Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  state->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */

		printk(KERN_WARNING "rs_close: bad serial port count; tty->count is 1, "
				"state->count is %d\n", state->count);
		state->count = 1;
	}

	if (--state->count < 0) {
		printk(KERN_WARNING "rs_close: bad serial port count for ttys%d: %d\n",
				info->line, state->count);
		state->count = 0;
	}

	if (state->count) {
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}

	info->flags |= ASYNC_CLOSING;
	restore_flags(flags);

	/* Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */

	if (info->flags & ASYNC_NORMAL_ACTIVE)
		info->state->normal_termios = *tty->termios;

	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->state->callout_termios = *tty->termios;

	/* Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */

	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);

	/* At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */

	serial_disable_rx_int(info);	

	if (info->flags & ASYNC_INITIALIZED) {
		/* Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */

		rs_wait_until_sent(tty, info->timeout);
	}

	shutdown(info);

	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);

	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);

	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	
	if (info->blocked_open) {
		if (info->close_delay) {
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}

	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
}

/* rs_wait_until_sent() --- wait until the transmitter is empty */

static void rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
	dm270_async_t *info = (dm270_async_t *)tty->driver_data;
	unsigned long  orig_jiffies;
	unsigned long  char_time;
	
	if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
		return;

	if (info->state->type == PORT_UNKNOWN)
		return;

	if (info->xmit_fifo_size <= 0)
		return; /* Just in case.... */

	orig_jiffies = jiffies;

	/* Set the check interval to be 1/5 of the estimated time to
	 * send a single character, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 * 
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */

	char_time = (info->timeout - HZ/50) / info->xmit_fifo_size;
	char_time = char_time / 5;

	if (char_time == 0)
		char_time = 1;

	if (timeout && timeout < char_time)
		char_time = timeout;

	/* If the transmitter hasn't cleared in twice the approximate
	 * amount of time to send the entire FIFO, it probably won't
	 * ever clear.  This assumes the UART isn't doing flow
	 * control, which is currently the case.  Hence, if it ever
	 * takes longer than info->timeout, this is probably due to a
	 * UART bug of some kind.  So, we clamp the timeout parameter at
	 * 2*info->timeout.
	 */

	if (!timeout || timeout > 2*info->timeout)
		timeout = 2*info->timeout;

#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("In rs_wait_until_sent(%d) check=%lu...", timeout, char_time);
	printk("jiff=%lu...", jiffies);
#endif
	while (!serial_tx_fifo_is_empty(info)) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
	set_current_state(TASK_RUNNING);
}

/* rs_hangup() --- called by tty_hangup() when a hangup is signaled. */

static void rs_hangup(struct tty_struct *tty)
{
	dm270_async_t        *info  = (dm270_async_t *)tty->driver_data;
	dm270_serial_state_t *state = info->state;
	
	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	state = info->state;
	
	rs_flush_buffer(tty);
	if (info->flags & ASYNC_CLOSING)
		return;

	shutdown(info);
	info->event = 0;
	state->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*-------------------------------------------------------------------------
 * rs_open() and friends
 *-------------------------------------------------------------------------*/

static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   dm270_async_t *info)
{
	DECLARE_WAITQUEUE(wait, current);
	dm270_serial_state_t *state = info->state;
	unsigned long	        flags;
	int		        retval;
	int		        do_clocal = 0;
	int		        extra_count = 0;

	/* If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */

	if (tty_hung_up_p(filp) ||
			(info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		return ((info->flags & ASYNC_HUP_NOTIFY) ?
				-EAGAIN : -ERESTARTSYS);
#else
		return -EAGAIN;
#endif
	}

	/* If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */

	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;

		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
				(info->flags & ASYNC_SESSION_LOCKOUT) &&
				(info->session != current->session))
			return -EBUSY;

		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
				(info->flags & ASYNC_PGRP_LOCKOUT) &&
				(info->pgrp != current->pgrp))
			return -EBUSY;

		info->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}
	
	/* If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */

	if ((filp->f_flags & O_NONBLOCK) ||
			(tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ASYNC_CALLOUT_ACTIVE) {
		if (state->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}

	/* Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, state->count is dropped by one, so that
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */

	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttys%d, count = %d\n",
			state->line, state->count);
#endif
	save_flags(flags); cli();
	if (!tty_hung_up_p(filp)) {
		extra_count = 1;
		state->count--;
	}

	restore_flags(flags);
	info->blocked_open++;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (tty_hung_up_p(filp) ||
				!(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
				!(info->flags & ASYNC_CLOSING))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttys%d, count = %d\n",
				info->line, state->count);
#endif
		schedule();
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&info->open_wait, &wait);
	if (extra_count)
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
			info->line, state->count);
#endif
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

static int get_async_struct(int line, dm270_async_t **ret_info)
{
	dm270_async_t        *info;
	dm270_serial_state_t *sstate;

	sstate = rs_table + line;
	sstate->count++;

	if (sstate->info) {
		*ret_info = (dm270_async_t*)sstate->info;
		return 0;
	}

	info = kmalloc(sizeof(dm270_async_t), GFP_KERNEL);
	if (!info) {
		sstate->count--;
		return -ENOMEM;
	}

	memset(info, 0, sizeof(dm270_async_t));

	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	init_waitqueue_head(&info->delta_msr_wait);

	info->magic           = SERIAL_MAGIC;
	info->port            = sstate->port;
	info->iomem_base      = sstate->iomem_base;
	info->line            = line;
	info->tqueue.routine  = do_softint;
	info->tqueue.data     = info;

	info->state           = sstate;
	info->flags           = sstate->flags;
	info->xmit_fifo_size  = sstate->xmit_fifo_size;

	if (sstate->info) {
		kfree(info);
		*ret_info = (dm270_async_t*)sstate->info;
		return 0;
	}

	sstate->info = info;
	*ret_info    = info;
	return 0;
}

/* This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */

static int rs_open(struct tty_struct *tty, struct file * filp)
{
	dm270_async_t	*info;
	unsigned long	 page;
	int 		 retval;
	int 		 line;

	MOD_INC_USE_COUNT;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS)) {
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}

	retval = get_async_struct(line, &info);
	if (retval) {
		MOD_DEC_USE_COUNT;
		return retval;
	}

	tty->driver_data = info;
	info->tty = tty;
	if (serial_paranoia_check(info, tty->device, "rs_open")) {
		MOD_DEC_USE_COUNT;		
		return -ENODEV;
	}

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
			info->state->count);
#endif
#if (LINUX_VERSION_CODE > 0x20100)
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
#endif

	if (!tmp_buf) {
		page = get_zeroed_page(GFP_KERNEL);
		if (tmp_buf) {
			free_page(page);
		} else {
			if (!page) {
				MOD_DEC_USE_COUNT;
				return -ENOMEM;
			}
			tmp_buf = (unsigned char*)page;
		}
	}

	/* If the port is the middle of closing, bail out now */

	if (tty_hung_up_p(filp) ||
			(info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
		MOD_DEC_USE_COUNT;
#ifdef SERIAL_DO_RESTART
		return ((info->flags & ASYNC_HUP_NOTIFY) ?
				-EAGAIN : -ERESTARTSYS);
#else
		return -EAGAIN;
#endif
	}

	/* Start up serial port */

	retval = startup(info);
	if (retval) {
		MOD_DEC_USE_COUNT;
		return retval;
	}

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("rs_open returning after block_til_ready with %d\n",
				retval);
#endif
		MOD_DEC_USE_COUNT;
		return retval;
	}

	if ((info->state->count == 1) &&
			(info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->state->normal_termios;
		else 
			*tty->termios = info->state->callout_termios;
		change_speed(info, 0);
	}
#ifdef CONFIG_SERIAL_DM270_CONSOLE
	if (sercons.cflag && sercons.index == line) {
		tty->termios->c_cflag = sercons.cflag;
		sercons.cflag = 0;
		change_speed(info, 0);
	}
#endif
	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open ttyS%d successful...", info->line);
#endif
	return 0;
}

/* /proc fs routines.... */

static inline int line_info(char *buf, dm270_serial_state_t *state)
{
	dm270_async_t *info = state->info;
	dm270_async_t scr_info;
	char	stat_buf[30];
	int	ret;

	ret = sprintf(buf, "%d: uart:%s iomem:%lX irq:%d",
			state->line, SERIAL_DRIVER_NAME,
			state->iomem_base, state->irq);

	if (!state->iomem_base) {
		ret += sprintf(buf+ret, "\n");
		return ret;
	}

	/* Figure out the current RS-232 lines */
	if (!info) {
		info = &scr_info;	/* This is just for serial_{in,out} */

		info->magic = SERIAL_MAGIC;
		info->port = state->port;
		info->flags = state->flags;
		info->baud = 0;
		info->tty = 0;
	}

	stat_buf[0] = 0;
	stat_buf[1] = 0;

	if (info->baud) {
		ret += sprintf(buf+ret, " baud:%d",
				info->baud);
	}

	ret += sprintf(buf+ret, " tx:%d rx:%d",
			state->icount.tx, state->icount.rx);

	if (state->icount.frame)
		ret += sprintf(buf+ret, " fe:%d", state->icount.frame);
	
	if (state->icount.parity)
		ret += sprintf(buf+ret, " pe:%d", state->icount.parity);
	
	if (state->icount.brk)
		ret += sprintf(buf+ret, " brk:%d", state->icount.brk);	

	if (state->icount.overrun)
		ret += sprintf(buf+ret, " oe:%d", state->icount.overrun);

	/* Last thing is the RS-232 status lines */
	ret += sprintf(buf+ret, " %s\n", stat_buf+1);
	return ret;
}

#ifdef CONFIG_PROC_FS
static int rs_read_proc(char *page, char **start, off_t off, int count,
		 int *eof, void *data)
{
	off_t begin = 0;
	int len = 0;
	int i;
	int l;

	len += sprintf(page, "serinfo:1.0 driver:%s\n", serial_version);
	for (i = 0; i < NR_PORTS && len < 4000; i++) {
		l = line_info(page + len, &rs_table[i]);
		len += l;
		if (len+begin > off+count)
			goto done;
		if (len+begin < off) {
			begin += len;
			len = 0;
		}
	}
	*eof = 1;
done:
	if (off >= len+begin)
		return 0;
	*start = page + (begin-off);
	return ((count < begin+len-off) ? count : begin+len-off);
}
#endif

/* Basic UART init */

static void autoconfig(dm270_serial_state_t *state)
{
	dm270_async_t *info;
	dm270_async_t  scr_info;
	unsigned long  flags;
	unsigned short junk;

#ifdef SERIAL_DEBUG_AUTOCONF
	printk("Testing ttyS%d (0x%04lx, 0x%04x)...\n", state->line,
			state->port, (unsigned) state->iomem_base);
#endif
	state->type           = PORT_DM270;

	info                  = &scr_info;
	info->magic           = SERIAL_MAGIC;
	info->state           = state;
	info->flags           = state->flags;
	info->line            = state->line;
	info->port            = state->port;
	info->iomem_base      = state->iomem_base;

	/* Skip re-configuration of the serial console. This way we won't
	 * clobber the settings established for console printk's during boot.
	 */

#ifdef CONFIG_SERIAL_DM270_CONSOLE
	if (state != async_sercons.state)
#endif
	{
		save_flags(flags); cli();

		serial_reset_chip(info);
		serial_disable_ints(info, &junk);
		serial_clear_fifos(info);
		serial_disable_breaks(info);
		serial_set_tx_trigger(info, DM270_UART_TFCR_TRG_1);
		serial_set_rx_trigger(info, DM270_UART_RFCR_TRG_16);

		restore_flags(flags);
	}
}

/*-------------------------------------------------------------------------
 * rs_init() and friends
 *-------------------------------------------------------------------------*/

/*
 * This routine prints out the appropriate serial driver version
 * number, and identifies which options were configured into this
 * driver.
 */

static inline void show_serial_version(void)
{
 	printk(KERN_INFO "%s version %s\n", serial_name, serial_version);
}

/* The serial driver boot-time initialization code! */

static int __init rs_init(void)
{
	dm270_serial_state_t* state;
	int i;

	init_bh(SERIAL_BH, do_serial_bh);
#if defined(CONFIG_SERIAL_DM270_POLL) || defined(CONFIG_SERIAL_DM270_TIMEOUT_POLL)
	init_timer(&serial_timer);
	serial_timer.function = rs_timer;
/*	mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);*/
#endif

	for (i = 0; i < NR_IRQS; i++) {
		IRQ_ports[i] = 0;
		IRQ_timeout[i] = 0;
	}

#ifdef CONFIG_SERIAL_DM270_CONSOLE
	/* The interrupt of the serial console port
	 * can't be shared.
	 */
	if (sercons.flags & CON_CONSDEV) {
		for(i = 0; i < NR_PORTS; i++)
			if (i != sercons.index &&
					rs_table[i].irq == rs_table[sercons.index].irq)
				rs_table[i].irq = 0;
	}
#endif
	show_serial_version();

	/* Initialize the tty_driver structure */
	
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic                = TTY_DRIVER_MAGIC;

#if (LINUX_VERSION_CODE > 0x20100)
	serial_driver.driver_name          = "serial";
#endif

#if (LINUX_VERSION_CODE > 0x2032D && defined(CONFIG_DEVFS_FS))
	serial_driver.name                 = "tts/%d";
#else
	serial_driver.name                 = "ttyS";
#endif

	serial_driver.major                = TTY_MAJOR;
	serial_driver.minor_start          = 64 + SERIAL_DEV_OFFSET;
	serial_driver.num                  = NR_PORTS;
	serial_driver.type                 = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype              = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios         = tty_std_termios;
	serial_driver.init_termios.c_cflag = B9600|CS8|CREAD|HUPCL|CLOCAL;
	serial_driver.flags                = TTY_DRIVER_REAL_RAW|TTY_DRIVER_NO_DEVFS;
	serial_driver.refcount             = &serial_refcount;
	serial_driver.table                = serial_table;
	serial_driver.termios              = serial_termios;
	serial_driver.termios_locked       = serial_termios_locked;

	serial_driver.open                 = rs_open;
	serial_driver.close                = rs_close;
	serial_driver.write                = rs_write;
	serial_driver.put_char             = rs_put_char;
	serial_driver.flush_chars          = rs_flush_chars;
	serial_driver.write_room           = rs_write_room;
	serial_driver.chars_in_buffer      = rs_chars_in_buffer;
	serial_driver.flush_buffer         = rs_flush_buffer;
	serial_driver.ioctl                = rs_ioctl;
	serial_driver.throttle             = rs_throttle;
	serial_driver.unthrottle           = rs_unthrottle;
	serial_driver.set_termios          = rs_set_termios;
	serial_driver.stop                 = rs_stop;
	serial_driver.start                = rs_start;
	serial_driver.hangup               = rs_hangup;

#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
	serial_driver.break_ctl            = rs_break;
#endif

#if (LINUX_VERSION_CODE >= 131343)
	serial_driver.send_xchar           = rs_send_xchar;
	serial_driver.wait_until_sent      = rs_wait_until_sent;
#ifdef CONFIG_PROC_FS
	serial_driver.read_proc            = rs_read_proc;
#else
	serial_driver.read_proc            = 0;
#endif
#endif

	/* The callout device is just like normal device except for
	 * major number and the subtype code.
	 */

	callout_driver                     = serial_driver;
#if (LINUX_VERSION_CODE > 0x2032D && defined(CONFIG_DEVFS_FS))
	callout_driver.name                = "cua/%d";
#else
	callout_driver.name                = "cua";
#endif
	callout_driver.major               = TTYAUX_MAJOR;
	callout_driver.subtype             = SERIAL_TYPE_CALLOUT;
#if (LINUX_VERSION_CODE >= 131343)
	callout_driver.read_proc           = 0;
	callout_driver.proc_entry          = 0;
#endif

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");

	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	
	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
		state->magic                   = SSTATE_MAGIC;
		state->line                    = i;
		state->custom_divisor          = 0;
		state->close_delay             = 5*HZ/10;
		state->closing_wait            = 30*HZ;
		state->callout_termios         = callout_driver.init_termios;
		state->normal_termios          = serial_driver.init_termios;

		state->icount.cts              = 0;
		state->icount.dsr              = 0;
		state->icount.rng              = 0;
		state->icount.dcd              = 0;
		state->icount.rx               = 0;
		state->icount.tx               = 0;
		state->icount.frame            = 0; 
		state->icount.parity           = 0;
		state->icount.overrun          = 0;
		state->icount.brk              = 0;

		state->irq                     = irq_cannonicalize(state->irq);
		state->xmit_fifo_size          = DM270_UART_TXFIFO_BYTESIZE;

		if (state->type == PORT_UNKNOWN) {
			/* Configure the serial port. */
			autoconfig(state);
		}

		printk(KERN_INFO "ttyS%02d at 0x%04lx (irq = %d)\n",
				state->line + SERIAL_DEV_OFFSET,
				state->iomem_base,
				state->irq);

		tty_register_devfs(&serial_driver, 0,
				serial_driver.minor_start + state->line);

		tty_register_devfs(&callout_driver, 0,
				callout_driver.minor_start + state->line);
	}

	return 0;
}

static void __exit rs_fini(void) 
{
	dm270_async_t *info;
	unsigned long flags;
	int e1;
	int e2;
	int i;

#if defined(CONFIG_SERIAL_DM270_POLL) || defined(CONFIG_SERIAL_DM270_TIMEOUT_POLL)
	del_timer_sync(&serial_timer);
#endif
	save_flags(flags); cli();
        remove_bh(SERIAL_BH);
	if ((e1 = tty_unregister_driver(&serial_driver)))
		printk(KERN_WARNING "serial: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&callout_driver)))
		printk(KERN_WARNING "serial: failed to unregister callout driver (%d)\n", 
		       e2);
	restore_flags(flags);

	for (i = 0; i < NR_PORTS; i++) {
		if ((info = rs_table[i].info)) {
			rs_table[i].info = NULL;
			kfree(info);
		}
	}
	if (tmp_buf) {
		unsigned long pg = (unsigned long) tmp_buf;
		tmp_buf = NULL;
		free_page(pg);
	}
}

module_init(rs_init);
module_exit(rs_fini);

MODULE_DESCRIPTION("TI TMS320DM270 serial driver");
MODULE_AUTHOR("Chee Tim Loh <cheetim_loh@innomedia.com.sg>, based on serial_c5471 by Todd Fischer");
MODULE_LICENSE("GPL");

/*-------------------------------------------------------------------------
 * Serial Console Driver
 *-------------------------------------------------------------------------*/

#ifdef CONFIG_SERIAL_DM270_CONSOLE

/* This block is enabled when the user has used `make xconfig`
 * to enable kernel printk() to come out the serial port.
 * The register_console(serial_console_print) call below
 * is what hooks in our serial output routine here with the
 * kernel's printk output.
 */

/* Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 * The console_lock must be held when we get here.
 */

static inline void wait_for_xmitr(dm270_async_t *info)
{
	int tmp;
	for (tmp = 1000000 ; tmp > 0 ; tmp--)
		if (serial_room_in_tx_fifo(info))
			break;
}

static void serial_console_write(struct console *co, const char *s,
				unsigned count)
{
	static dm270_async_t *info = &async_sercons;
	unsigned short        msr;
	unsigned int          i;

	/* First save the MSR then disable the interrupts */
	serial_disable_ints(info, &msr);

	/* Now, do each character */
	for (i = 0; i < count; i++, s++) {
		wait_for_xmitr(info);
		serial_char_out(info, *s);
		if (*s == 10) {
			/* LF? add CR */

			wait_for_xmitr(info);
			serial_char_out(info, 13);
		}
	}

	/* Finally, wait for transmitter & holding register to empty */
	wait_for_xmitr(info);
	serial_restore_ints(info, msr);
}

static kdev_t serial_console_device(struct console *co)
{
	return MKDEV(TTY_MAJOR, 64 + co->index);
}

/* Setup initial baud/bits/parity. We do two things here:
 * - construct a cflag setting for the first rs_open()
 * - initialize the serial port
 * Return non-zero if we didn't find a serial port.
 */

static int __init serial_console_setup(struct console *co, char *options)
{
	static dm270_async_t *info;
	dm270_serial_state_t *state;

	unsigned short  cval   = 0;
	int             baud   = DEFAULT_CONSOLE_BAUD;
	int             bits   = DEFAULT_CONSOLE_BITS;
	int             parity = DEFAULT_CONSOLE_PARITY;
	int             cflag  = CREAD | HUPCL | CLOCAL;
	char           *s;
	unsigned short  junk;

	if (co->index < 0 || co->index >= NR_PORTS) {
		return -1;
	}
	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s - '0';
	}

	SHOWVALUE(baud);

	/*
	 *	Now construct a cflag setting.
	 */
	switch(baud) {
		case 1200:
			cflag |= B1200;
			break;
		case 2400:
			cflag |= B2400;
			break;
		case 4800:
			cflag |= B4800;
			break;
		case 19200:
			cflag |= B19200;
			break;
		case 38400:
			cflag |= B38400;
			break;
		case 57600:
			cflag |= B57600;
			break;
		case 115200:
			cflag |= B115200;
			break;
		case 9600:
		default:
			cflag |= B9600;
			break;
	}

	cval = 0;

	SHOWVALUE(bits);

	switch(bits) {
		case 7:
			cflag |= CS7;
			cval  |= DM270_UART_MSR_7_DBITS;
			break;

		default:
		case 8:
			cflag |= CS8;
			cval  |= DM270_UART_MSR_8_DBITS;
			break;
	}

	SHOWVALUE(parity);

	switch(parity) {
		case 'o': case 'O':
			cflag |= (PARENB | PARODD);
			cval  |= DM270_UART_MSR_ODD_PARITY;
			break;
		case 'e': case 'E':
			cflag |= PARENB;
			cval  |= DM270_UART_MSR_EVEN_PARITY;
			break;
	}

	if (cflag & CSTOPB)
		cval |= DM270_UART_MSR_2_SBITS;
	else
		cval |= DM270_UART_MSR_1_SBITS;

	SHOWVALUE(cval);

	co->cflag             = cflag;

	state                 = rs_table + co->index;
	state->type           = PORT_DM270;
	info                  = &async_sercons;
	info->magic           = SERIAL_MAGIC;
	info->state           = state;
	info->flags           = state->flags;
	info->line            = co->index;
	info->port            = state->port;
	info->iomem_base      = state->iomem_base;
	info->xmit_fifo_size  = state->xmit_fifo_size;

	serial_reset_chip(info);
	serial_save_registers(info);
	serial_disable_ints(info, &junk);
	serial_clear_fifos(info);
	serial_set_rate(info, baud);
	serial_set_mode(info, cval);

	return 0;
}

/* Register console */

void __init dm270_console_init(void)
{
	register_console(&sercons);
}

#endif /* CONFIG_SERIAL_DM270_CONSOLE */
