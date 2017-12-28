/*
 * tiny serial helper routines for kgdb
 *
 * Based on George G. Davis <gdavis@mvista.com> work.
 */

#include <linux/config.h>
#include <asm/io.h>
#include <asm/hardware/serial_amba.h>
#include <asm/arch/regmap.h>
#include <asm/arch/regs_syscon.h>

#if defined(CONFIG_KGDB_UART0)
#define KGDB_UART_BASE UART1_BASE
#define KGDB_UART_ENABLE 0x00040000
#elif defined(CONFIG_KGDB_UART1)
#define KGDB_UART_BASE UART2_BASE
#define KGDB_UART_ENABLE 0x00100000
#elif defined(CONFIG_KGDB_UART2)
#define KGDB_UART_BASE UART3_BASE
#define KGDB_UART_ENABLE 0x01000000
#else
#error "No kgdb serial port UART has been selected."
#endif

#if defined(CONFIG_KGDB_9600BAUD)
#define KGDB_SERIAL_BAUD_RATE ARM_BAUD_9600
#elif defined(CONFIG_KGDB_19200BAUD)
#define KGDB_SERIAL_BAUD_RATE ARM_BAUD_19200
#elif defined(CONFIG_KGDB_38400BAUD)
#define KGDB_SERIAL_BAUD_RATE ARM_BAUD_38400
#elif defined(CONFIG_KGDB_57600BAUD)
#define KGDB_SERIAL_BAUD_RATE ARM_BAUD_57600
#elif defined(CONFIG_KGDB_115200BAUD)
#define KGDB_SERIAL_BAUD_RATE ARM_BAUD_115200
#else
#error "kgdb serial baud rate has not been specified."
#endif

void kgdb_serial_init(void)
{
	unsigned int uiTemp;

	uiTemp = inl(SYSCON_DEVCFG) | KGDB_UART_ENABLE;
	outl(0xaa, SYSCON_SWLOCK);
	outl(uiTemp, SYSCON_DEVCFG);
	outl(KGDB_SERIAL_BAUD_RATE, KGDB_UART_BASE + AMBA_UARTLCR_L);
	outl(0, KGDB_UART_BASE + AMBA_UARTLCR_M);
	outl(AMBA_UARTLCR_H_WLEN_8 | AMBA_UARTLCR_H_FEN,
	     KGDB_UART_BASE + AMBA_UARTLCR_H);
	outl(AMBA_UARTCR_UARTEN, KGDB_UART_BASE + AMBA_UARTCR);
}

void kgdb_serial_putchar(unsigned char ch)
{
	while(inl(KGDB_UART_BASE + AMBA_UARTFR) & AMBA_UARTFR_TXFF);
	outl(ch, KGDB_UART_BASE + AMBA_UARTDR);
}

unsigned char kgdb_serial_getchar(void)
{
	while(inl(KGDB_UART_BASE + AMBA_UARTFR) & AMBA_UARTFR_RXFE);
	return(inl(KGDB_UART_BASE + AMBA_UARTDR) & 0xff);
}
