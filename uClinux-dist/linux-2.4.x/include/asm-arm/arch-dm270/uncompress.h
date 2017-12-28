/*
 * include/asm/arch/uncompress.h:
 *         Optional routines to aid in debugging the decompression phase
 *         of kernel boot.
 * copyright:
 *         (C) 2001 RidgeRun, Inc. (http://www.ridgerun.com)
 * author: Gordon McNutt <gmcnutt@ridgerun.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
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
 */

/* This is used by arch/armnommu/boot/compressed/misc.c to write progress info
 * out the serial port so that the user can see debug messages up to the point
 * where the kernel is decompressed. The STANDALONE_DEBUG macro chooses between
 * this and the standard printf. Punt.
 * --gmcnutt
 */

#include <asm/hardware.h>
#include <asm/io.h>

#ifdef CONFIG_DEBUG_DM270_UART1
# define DM270_UART_BASE	DM270_UART1_BASE
# define DM270_CLKC_UART_SHIFT	1
#else
# define DM270_UART_BASE	DM270_UART0_BASE
# define DM270_CLKC_UART_SHIFT	0
#endif

static inline void
putc(char c)
{
    while (!(__arch_readw(DM270_UART_BASE+DM270_UART_SR) & DM270_UART_SR_TREF));

    __arch_writew(c, DM270_UART_BASE+DM270_UART_DTRR);
}

/* Hardware-specific routine to put a string to the debug UART, converting
 * "\n" to "\n\r" on the way */

static void
puts(const char *s)
{
	while (*s) {
		putc(*s);
		if (*s == '\n')
			putc('\r');
		s++;
	}
}

/* If we need to do some setup prior to decompression (like initializing the
 * UART if we want to use puts() above) then we define it here. Punt.
 */

static inline void
arch_decomp_setup(void)
{
	/* Disable clock to UART. */
	__arch_writew((__arch_readw(DM270_CLKC_MOD2) &
			~(DM270_CLKC_MOD2_CUAT << DM270_CLKC_UART_SHIFT)),
			DM270_CLKC_MOD2);

	/* Select CLK_ARM as UART clock source */
	__arch_writew((__arch_readw(DM270_CLKC_CLKC) &
			~(DM270_CLKC_CLKC_CUAS << DM270_CLKC_UART_SHIFT)),
			DM270_CLKC_CLKC);

	/* Enable clock to UART. */
	__arch_writew((__arch_readw(DM270_CLKC_MOD2) |
			(DM270_CLKC_MOD2_CUAT << DM270_CLKC_UART_SHIFT)),
			DM270_CLKC_MOD2);

#ifdef CONFIG_DEBUG_DM270_UART1
	/* Configure GIO23 & GIO24 as RXD1 & TXD1 respectively */
	__arch_writew((__arch_readw(DM270_GIO_FSEL0) | DM270_GIO_FSEL_RXD1),
			DM270_GIO_FSEL0);
	__arch_writew((__arch_readw(DM270_GIO_FSEL1) | DM270_GIO_FSEL_TXD1),
			DM270_GIO_FSEL1);
#endif

	/* 8-1-no parity. Enable receive interrupts. */
	__arch_writew((DM270_UART_MSR_8_DBITS | DM270_UART_MSR_1_SBITS |
			DM270_UART_MSR_NO_PARITY | DM270_UART_MSR_TIMEOUT_DISABLED |
			DM270_UART_MSR_REIE | DM270_UART_MSR_RFTIE),
			DM270_UART_BASE+DM270_UART_MSR);

	/* Set bit rate. */
	__arch_writew(DM270_UART_BRSR_VAL(_DEFAULT_BAUD),
			DM270_UART_BASE+DM270_UART_BRSR);

	/* Clear & enable FIFO. */
	/* Set interrupt trigger level to be 1 byte. */
	__arch_writew((DM270_UART_TFCR_TRG_1 | DM270_UART_TFCR_CLEAR),
			DM270_UART_BASE+DM270_UART_TFCR);
	__arch_writew((DM270_UART_RFCR_TRG_1 | DM270_UART_RFCR_RESET |
			DM270_UART_RFCR_CLEAR), DM270_UART_BASE+DM270_UART_RFCR);
}

/* Not sure what this is for. Probably an optional watchdog to check if the
 * decompress got hung so we can warn the user. Punt.
 */
#define arch_decomp_wdog()
