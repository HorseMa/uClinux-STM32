/*
 * FILE NAME: arch/arm/mach-integrator/kgdb-serial.c
 *
 * BRIEF MODULE DESCRIPTION:
 *  Provides low level kgdb serial support hooks for SA-11x0 targets.
 *
 *  Author: MontaVista Software, Inc.  <source@mvista.com>
 *          George G. Davis <gdavis@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
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


#if	defined(CONFIG_KGDB_UART1)
#	define	KGDB_SERIAL_PORT_BASE	&Ser1UTCR0
#elif	defined(CONFIG_KGDB_UART2)
#	define	KGDB_SERIAL_PORT_BASE	&Ser2UTCR0
#elif	defined(CONFIG_KGDB_UART3)
#	define	KGDB_SERIAL_PORT_BASE	&Ser3UTCR0
#else
#	error "No kgdb serial port UART has been selected."
#endif

#if	defined(CONFIG_KGDB_9600BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	(3686400/(9600 * 16) - 1)
#elif	defined(CONFIG_KGDB_19200BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	(3686400/(19200 * 16) - 1)
#elif	defined(CONFIG_KGDB_38400BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	(3686400/(38400 * 16) - 1)
#elif	defined(CONFIG_KGDB_57600BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	(3686400/(57600 * 16) - 1)
#elif	defined(CONFIG_KGDB_115200BAUD)
#	define	KGDB_SERIAL_BAUD_RATE	(3686400/(115200 * 16) - 1)
#else
#	error "kgdb serial baud rate has not been specified."
#endif


#define UART(x)		(*(volatile unsigned long *)(serial_port + (x)))


static unsigned long serial_port = 0;


void
kgdb_serial_init(void)
{
	serial_port = (unsigned long)(KGDB_SERIAL_PORT_BASE);

	UART(UTCR3) = 0; /* disable */
	UART(UTSR0) = 0xff; /* clear sticky bits */
	UART(UTCR0) = UTCR0_8BitData; /* format: 8n1 */
	UART(UTCR1) = (KGDB_SERIAL_BAUD_RATE >> 8) & 0xf;
	UART(UTCR2) = KGDB_SERIAL_BAUD_RATE & 0xff;
	UART(UTCR3) = UTCR3_RXE | UTCR3_TXE; /* enable */
}


void
kgdb_serial_putchar(unsigned char ch)
{
	while (!(UART(UTSR1) & UTSR1_TNF)) ;

	UART(UTDR) = ch;
}


unsigned char
kgdb_serial_getchar(void)
{
        while (!(UART(UTSR1) & UTSR1_RNE)) ;
        return (unsigned char)UART(UTDR);
}
