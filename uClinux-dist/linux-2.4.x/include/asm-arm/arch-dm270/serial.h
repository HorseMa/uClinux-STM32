/***********************************************************************
 * linux/include/asm-arm/arch-dm270/serial.h
 *
 *   Derived from asm/arch-armnommu/arch-c5471/serial.h
 *
 *   Copyright (C) 2004 InnoMedia Pte Ltd. All rights reserved.
 *   cheetim_loh@innomedia.com.sg  <www.innomedia.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***********************************************************************/

#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <linux/config.h>
#include <linux/termios.h>
#include <linux/tqueue.h>
#include <linux/circ_buf.h>
#include <linux/wait.h>

#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>

#define BASE_BAUD     38400
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

/* This defines the initializer for a struct serial_state describing the
 * TI TMS320DM270 serial port 0.
 */

#define DM270_SERIAL_PORT0_DEFN \
  {  \
    magic:          0, \
    port:           0, \
    flags:          STD_COM_FLAGS, \
    xmit_fifo_size: DM270_UART_TXFIFO_BYTESIZE, \
    baud_base:      BASE_BAUD, \
    irq:            DM270_INTERRUPT_UART0, \
    type:           PORT_UNKNOWN, \
    line:           0, \
    iomem_base:     DM270_UART0_BASE, \
    io_type:        SERIAL_IO_MEM \
  }

/* This defines the initializer for a struct serial_state describing the
 * TI TMS320DM270 serial port 1.
 */

#define DM270_SERIAL_PORT1_DEFN \
  {  \
    magic:          0, \
    port:           0, \
    flags:          STD_COM_FLAGS, \
    xmit_fifo_size: DM270_UART_TXFIFO_BYTESIZE, \
    baud_base:      BASE_BAUD, \
    irq:            DM270_INTERRUPT_UART1, \
    type:           PORT_UNKNOWN, \
    line:           1, \
    iomem_base:     DM270_UART1_BASE, \
    io_type:        SERIAL_IO_MEM \
  }

/* There will always be two serial ports. */

#define RS_TABLE_SIZE 2

/* STD_SERIAL_PORT_DEFNS (plus EXTRA_SERIAL_PORT_DEFNS, see
 * include/asm/serial.h) define an array of struct serial_state
 * instance initializers.  These array initilizers are used
 * in drivers/char/serial_dm270.c to describe the serial
 * ports that will be supported.
 */

# define STD_SERIAL_PORT_DEFNS \
  DM270_SERIAL_PORT0_DEFN,  /* ttyS0 */ \
  DM270_SERIAL_PORT1_DEFN   /* ttyS1 */

#define EXTRA_SERIAL_PORT_DEFNS

/* Events are used to schedule things to happen at timer-interrupt
 * time, instead of at rs interrupt time.
 */

#define RS_EVENT_WRITE_WAKEUP	0

#define CONFIGURED_SERIAL_PORT(info) ((info)->state)

#define SERIAL_MAGIC 0x5301
#define SSTATE_MAGIC 0x5302

/* This is the internal structure for each serial port's state.
 *
 * For definitions of the flags field, see tty.h
 */

struct dm270_serial_state_s
{
	int			magic;
	int			port;
	int			baud_base;
	int			irq;
	int			flags;
	int			type;
	int			line;
	int			xmit_fifo_size;
	int			custom_divisor;
	int			count;
	int			io_type;
	unsigned long		iomem_base;
	unsigned short		close_delay;
	unsigned short		closing_wait;	/* time to wait before closing */
	struct async_icount	icount;	
	struct termios		normal_termios;
	struct termios		callout_termios;
	struct dm270_async_s	*info;
};
typedef struct dm270_serial_state_s dm270_serial_state_t;

struct dm270_async_s
{
	int			magic;
	int			port;
	int			flags;
	int			xmit_fifo_size;
	dm270_serial_state_t	*state;
	struct tty_struct	*tty;
	unsigned short		closing_wait;
	int			line;
	unsigned short		read_status_mask;
	unsigned short		ignore_status_mask;
	int			timeout;
	int			baud;
	int			x_char;		/* xon/xoff character */
	int			close_delay;
	unsigned short		msr;		/* Cached UART Mode Setup Register */
	int			blocked_open;	/* # of blocked opens */
	unsigned long		event;
	unsigned long		last_active;
	long			session;	/* Session of opening process */
	long			pgrp;		/* pgrp of opening process */
	struct circ_buf		xmit;
	unsigned long		iomem_base;
	struct tq_struct	tqueue;
#ifdef DECLARE_WAITQUEUE
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;
	wait_queue_head_t	delta_msr_wait;
#else	
	struct wait_queue	*open_wait;
	struct wait_queue	*close_wait;
	struct wait_queue	*delta_msr_wait;
#endif	
	struct dm270_async_s	*next_port;	/* For the linked list */
	struct dm270_async_s	*prev_port;
};
typedef struct dm270_async_s dm270_async_t;

#endif /* __ASM_ARCH_SERIAL_H */
