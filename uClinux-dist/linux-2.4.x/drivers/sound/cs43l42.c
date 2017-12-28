/*
 * CS43L42 DAC on the EDB7312 Cogent eval board
 *
 * (C) 2003 Heeltoe Consulting, http://www.heeltoe.com
 * (c) 2003 Nucleus Systems, Petko Manolov <petkan@nucleusys.com>
 *
 * linux/drivers/sound/cs43l42.c
 */


#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/hardware/ep7312.h>
#include <asm/hardware/clps7111.h>
#include <asm/delay.h>


#define ADDR_CS43L42	(0x10 << 1)
#define ADDR_CS53L32	(0x11 << 1)
#define CS43L42_PWRCTL	0x01
/* direction and i/o bits have the same offset */
#define	SCL		(1 << 5)
#define SDA		(1 << 4)


static void reset_cs43l42(void)
{
	clps_writel((clps_readl(PEDDR) | (1 << 0)), PEDDR);

	clps_writel((clps_readl(PEDR) & ~(1 << 0)), PEDR);
	udelay(10);
	clps_writel((clps_readl(PEDR) | (1 << 0)), PEDR);
	udelay(10);
}

static void make_scl_sda_outputs(void)
{
	unsigned char tmp;
	
	tmp = clps_readb(PDDDR);
	tmp &= ~(SCL | SDA);
	clps_writeb(tmp, PDDDR);
}

static void make_sda_input(void)
{
	unsigned char tmp;

	tmp = clps_readb(PDDDR);
	tmp |= SDA;
	clps_writeb(tmp, PDDDR);
}

static void set_scl(int val)
{
	unsigned char tmp;
	
	tmp = clps_readb(PDDR);
	if (val)
		tmp |= SCL;
	else
		tmp &= ~SCL;
	clps_writeb(tmp, PDDR);
}

static void set_sda(int val)
{
	unsigned char tmp;
	
	tmp = clps_readb(PDDR);
	if (val)
		tmp |= SDA;
	else
		tmp &= ~SDA;
	clps_writeb(tmp, PDDR);
}

static int get_sda(void)
{
	unsigned char tmp;
	
	tmp = clps_readb(PDDR);
	return (tmp & SDA) ? 1 : 0;
}

static int clock_byte_in(void)
{
	int i, b, ack;

	b = 0;
	make_sda_input();
	for (i = 0; i < 8; i++) {
		udelay(3);
		b = (b << 1) |  get_sda();
		udelay(3);
		set_scl(1);
		udelay(5);
		set_scl(0);
	}
	/* ack bit */
	udelay(5);
	set_scl(1);
	udelay(2);
	ack = get_sda();
	udelay(3);
	set_scl(0);
	make_scl_sda_outputs();

	return b;
}

static void clock_byte_out(int b, int set_output)
{
	int i, ack;

	for (i = 0; i < 8; i++) {
		udelay(3);
		set_sda(b & 0x80);
		udelay(3);
		set_scl(1);
		udelay(5);	/* Tlow == 4.7 uS */
		set_scl(0);
		b <<= 1;
	}
	/* ack bit */
	make_sda_input();
	udelay(5);
	set_scl(1);
	udelay(2);
	ack = get_sda();
	udelay(3);
	set_scl(0);

	if (set_output) {
		make_scl_sda_outputs();
	}
}

static int cs43l42_i2c_read(int addr, int reg)
{
	int b;

	/*
	 * start of frame
	 */
	set_sda(1);
	set_scl(1);
	udelay(5);	/* Tbuf == 4.7 uS */
	set_sda(0);
	udelay(4);	/* Thdst == 4 uS */
	set_scl(0);
	clock_byte_out(addr | 0x01, 1);
	clock_byte_out(reg, 0);
	b = clock_byte_in();
	/*
	 * end of frame
	 */
	set_sda(1);
	set_scl(1);
	udelay(5);

	return b;
}

static int cs43l42_i2c_write(int addr, int reg, int value)
{
	set_sda(1);
	set_scl(1);
	udelay(5);
	set_sda(0);
	udelay(4);
	set_scl(0);
	clock_byte_out(addr, 1);
	clock_byte_out(reg, 1);
	clock_byte_out(value, 1);
	set_sda(1);
	set_scl(1);
	udelay(5);

	return 0;
}

static void cs43l42_dump_regs(void)
{
	int i;

	printk("%s: cs43l42 is at %02x\n", __FUNCTION__, ADDR_CS43L42);
	for (i = 1; i <= 0xb; i++) {
		printk("reg %02x: %02x\n",i,cs43l42_i2c_read(ADDR_CS43L42, i));
	}
}


int setup_cs43l42(void)
{
	volatile long u;
	volatile char port_d;

	printk("cs43l42: init dac\n");
	port_d = clps_readb(PDDR) & ~(SCL | SDA);
	
	/* enable codec_en# */
	reset_cs43l42();

	/* make SDA & SCL outputs */
	make_scl_sda_outputs();

	/*
	 * enable the two wire serial interface on the CS43L42
	 * by setting the CP_EN bit (bit 0) of register 1
	 */
	cs43l42_i2c_write(ADDR_CS43L42, CS43L42_PWRCTL, 0xd2);

	/* set the data format to left justified */
	cs43l42_i2c_write(ADDR_CS43L42, 0x0b, 0x02);

	/* power on the DAC */
	cs43l42_i2c_write(ADDR_CS43L42, CS43L42_PWRCTL, 0xd0);

	/* delay while the DAC initializes */
	for(u = 0; u < 15; u++)
		udelay(5);

	cs43l42_i2c_write(ADDR_CS43L42, 2, 0xf1);
	cs43l42_i2c_write(ADDR_CS43L42, 3, 0xf1);

#if 0
	cs43l42_dump_regs();
	printk("syscon1 %08x\n", clps_readl(SYSCON1));
	printk("syscon2 %08x\n", clps_readl(SYSCON2));
	printk("syscon3 %08x\n", clps_readl(SYSCON3));
#endif
	/* restore original port D value */
	port_d |= clps_readb(PDDR);
	clps_writeb(port_d, PDDR);
	printk("cs43l42: init dac complete\n");

	return 0;
}
