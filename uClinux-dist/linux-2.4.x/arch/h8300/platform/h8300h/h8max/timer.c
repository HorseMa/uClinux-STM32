/*
 *  linux/arch/h8300/platform/h8300h/h8max/timer.c
 *
 *  Yoshinori Sato <ysato@users.sourcefoge.jp>
 *
 *  Platform depend Timer Handler
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>

#define CMFA 6

int platform_timer_setup(void (*timer_int)(int, void *, struct pt_regs *))
{
	outb(CONFIG_CLK_FREQ*10/8192,TCORA2);
	outb(0x00,_8TCSR2);
	request_irq(40,timer_int,0,"timer",0);
	outb(0x40|0x08|0x03,_8TCR2);
	return 0;
}

void platform_timer_eoi(void)
{
	*(unsigned char *)_8TCSR2 &= ~(1 << CMFA);
}

void platform_gettod(int *year, int *mon, int *day, int *hour,
		 int *min, int *sec)
{
	*year = *mon = *day = *hour = *min = *sec = 0;
}

#define CNT_PER_USEC (1000000 / (CONFIG_CLK_FREQ * 1000 / 8192))
unsigned long platform_get_usec(void)
{
	return inb(_8TCNT2) * CNT_PER_USEC;
}
