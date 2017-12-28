
#ifndef _RTL8186_H
#define _RTL8186_H



/* Register access macro
*/
#define	TIMER_CLK_DIV_BASE	2
#define	DIVISOR	22
//#define TICK_FREQ	100
#define REG32(reg)	(*(volatile unsigned int *)(reg))
#define REG16(reg)	(*(volatile unsigned short *)(reg))
#define REG8(reg)	(*(volatile unsigned char *)(reg))


#define GIMR								0xbd010000UL
#define TCIE								(1 << 0)
#define UART0IE							(1 << 3)
#define GISR								0xbd010004UL
#define IRR								0xbd010008UL	/* Actually 8186 does not have this register */
#define TCCNR							0xbd010050UL
#define TC0EN							(1 << 0)
#define TC0MODE_TIMER					(1 << 1)
#define TC0SRC_BASICTIMER				(1 << 8)
#define TCIR								0xbd010054
#define TC0IE							(1 << 0)
#define TC0IP							(1 << 4)
#define CDBR								0xbd010058UL
#define TC0DATA							0xbd010060UL
#define TCD_OFFSET						0
#define UART0_BASE						0xbd0100c3
#define UART_BASECLOCK					153600000



#endif   /* _RTL8186_H */

