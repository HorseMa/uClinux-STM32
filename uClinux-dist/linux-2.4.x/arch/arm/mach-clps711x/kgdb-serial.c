/*
 * tiny serial helper routines for kgdb
 *
 *	(C) Copyright 2003 Nucleus Systems, Ltd. <petkan@nucleusys.com>
 *	
 *	Based on George G. Davis <gdavis@mvista.com> work.
 */

#include <linux/config.h>
#include <asm/io.h>
#include <asm/hardware/clps7111.h>




#if defined(CONFIG_KGDB_UART0)
#define KGDB_UART	UARTDR1
#define KGDB_SYSFLG	SYSFLG1
#define	KGDB_SYSCON	SYSCON1
#define	KGDB_UBRLCR	UBRLCR1
#elif defined(CONFIG_KGDB_UART1)
#define KGDB_UART	UARTDR2
#define	KGDB_SYSFLG	SYSFLG2
#define	KGDB_SYSCON	SYSCON2
#define	KGDB_UBRLCR	UBRLCR2
#else
#error "No kgdb serial port UART has been selected."
#endif


#if defined(CONFIG_KGDB_9600BAUD)
#define  KGDB_SERIAL_BAUD_RATE   23
#elif defined(CONFIG_KGDB_19200BAUD)
#define  KGDB_SERIAL_BAUD_RATE   11
#elif defined(CONFIG_KGDB_38400BAUD)
#define  KGDB_SERIAL_BAUD_RATE   5
#elif defined(CONFIG_KGDB_57600BAUD)
#define  KGDB_SERIAL_BAUD_RATE   3
#elif defined(CONFIG_KGDB_115200BAUD)
#define  KGDB_SERIAL_BAUD_RATE   1
#else
#error "kgdb serial baud rate has not been specified."
#endif

#define	KGDB_UART_CFG	(0x70000 | KGDB_SERIAL_BAUD_RATE)


void kgdb_serial_init(void)
{
	clps_writel(clps_readl(KGDB_SYSCON) | 0x100, KGDB_SYSCON);
	clps_writel(0x70001, KGDB_UBRLCR);
	printk("syscon and ubrlcr are: %08x and %08x\n",
	       clps_readl(KGDB_SYSCON), clps_readl(KGDB_UBRLCR));
}

void kgdb_serial_putchar(unsigned char ch)
{
	while (clps_readl(KGDB_SYSFLG) & 0x00800000);
	clps_writeb(ch, KGDB_UART);
}

unsigned char kgdb_serial_getchar(void)
{
	while (clps_readl(KGDB_SYSFLG) & 0x00400000);
	return clps_readb(KGDB_UART);
}
