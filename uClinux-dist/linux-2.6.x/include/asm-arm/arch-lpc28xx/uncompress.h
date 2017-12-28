/*
 * linux/include/asm-arm/arch-lpc22xx/uncompress.h
 *
 * Created Jan 04, 2007 Brandon Fosdick <brandon@teslamotors.com>
 */

#include <asm/arch/lpc28xx.h>	/* For U0LSR	*/
//#include <linux/serial_reg.h>	/* For UART_LSR_THRE	*/

#ifndef LPC28XX_UNCOMPRESSH
#define LPC28XX_UNCOMPRESSH

//#define	TX_DONE	(U0LSR & UART_LSR_THRE)
#define TEMP (1<<6)

static void putc(char c)
{
	while(!(UART_LSR & TEMP)){}
	UART_THR = c;
}

static void flush(void) {}

#define arch_decomp_setup()
#define arch_decomp_wdog()

#endif	//LPC28XX_UNCOMPRESSH
