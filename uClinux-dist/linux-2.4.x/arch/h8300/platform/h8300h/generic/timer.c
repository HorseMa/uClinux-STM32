/*
 *  linux/arch/h8300/platform/h8300h/generic/timer.c
 *
 *  Yoshinori Sato <qzb04471@nifty.ne.jp>
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

#if defined(CONFIG_H83007) || defined(CONFIG_H83068)
#include <asm/regs306x.h>

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

#define CNT_PER_USEC (1000000 / (CONFIG_CLK_FREQ * 1000 / 8192))
unsigned long platform_get_usec(void)
{
	return inb(_8TCNT2) * CNT_PER_USEC;
}

#endif

#if defined(H8_3002) || defined(CONFIG_H83048)
#define TSTR 0x00ffff60
#define TSNC 0x00ffff61
#define TMDR 0x00ffff62
#define TFCR 0x00ffff63
#define TOER 0x00ffff90
#define TOCR 0x00ffff91
#define TCR  0x00ffff64
#define TIOR 0x00ffff65
#define TIER 0x00ffff66
#define TSR  0x00ffff67
#define TCNT 0x00ffff68
#define GRA  0x00ffff6a
#define GRB  0x00ffff6c

int platform_timer_setup(void (*timer_int)(int, void *, struct pt_regs *))
{
	*(unsigned short *)GRA=CONFIG_CLK_FREQ*10/8;
	*(unsigned short *)TCNT=0;
	outb(0x23,TCR);
	outb(0x00,TIOR);
	request_irq_boot(26,timer_int,0,"timer",0);
	outb(inb(TIER) | 0x01,TIER);
	outb(inb(TSNC) & ~0x01,TSNC);
	outb(inb(TMDR) & ~0x01,TMDR);
	outb(inb(TSTR) | 0x01,TSTR);
	return 0;
}

void platform_timer_eoi(void)
{
	outb(inb(TSR) & ~0x01,TSR);
}

#define CNT_PER_USEC (1000000 / (CONFIG_CLK_FREQ * 1000 / 8))
unsigned long platform_get_usec(void)
{
	return *(unsigned short *)TCNT * CNT_PER_USEC;
}

#endif

void platform_gettod(int *year, int *mon, int *day, int *hour,
		 int *min, int *sec)
{
	*year = *mon = *day = *hour = *min = *sec = 0;
}
