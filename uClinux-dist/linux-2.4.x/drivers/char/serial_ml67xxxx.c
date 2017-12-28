/*
 * linux/drivers/char/serial_ml67xxxx.c
 *
 * Driver for the SIO serial port on the OKI ml67xxxx SOC.
 *
 * (c) 2005 Simtec Electronics
 *
 *
 * Based on drivers/char/serial.c
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/console.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/arch/sio.h>
#include <asm/arch/gpio.h>

#include <asm/hardware.h>

#if 1
#define dbg(x...) printk(KERN_DEBUG ## x)
#else
#define dbg(x...)
#endif

#define dbg_tx(x...)
#define dbg_wr(x...)


#define BAUD_BASE		(oki_cclk)

#define SERIAL_ML67XNAME	"ttySK"
#define SERIAL_ML67XMAJOR	204
#define SERIAL_ML67XMINOR	4

#define SERIAL_ML67XAUXNAME	"cuask"
#define SERIAL_ML67XAUXMAJOR	205
#define SERIAL_ML67XAUXMINOR	4

static struct tty_driver sio_driver, callout_driver;
static struct tty_struct *sio_table[1];

static struct termios sio_termios[1];
static struct termios sio_termios_locked[1];

static char wbuf[80], *putp = wbuf, *getp = wbuf, x_char;
static struct tty_struct *sio_tty;

static int sio_refcount;
static int sio_use_count;
static int sio_tx_inprogress;
static int sio_tx_locked;
static int sio_tx_stopped;

static unsigned long sio_tx_last;

static __inline__ unsigned int rd_reg(unsigned int reg)
{
	return __raw_readl(reg);
}

static __inline__ void wr_reg(unsigned int reg, unsigned int val)
{
	dbg_wr("sio: wr_reg(%08x, %02x)\n", reg, val);
	__raw_writel(val, reg);
}

#define sio_xmit_ready() !sio_tx_inprogress
#define sio_xmit_locked() sio_tx_locked

static __inline__ void sio_xmit_lock(void)
{
	sio_tx_locked = 1;
}

static __inline__ void sio_xmit_unlock(void)
{
	sio_tx_locked = 0;
}

static void sio_xmit_check(void)
{
	unsigned int status;

	/* must be called with irqs disabled */

	status = rd_reg(OKI_SIOSTA);

	dbg_tx("%s: status=%02x\n", __FUNCTION__, status);

	if (status & OKI_SIOSTA_TRIRQ) {
		sio_tx_inprogress = 0;
		wr_reg(OKI_SIOSTA, OKI_SIOSTA_TRIRQ);
	} else if (time_after(jiffies, sio_tx_last + HZ/10)) {
		printk(KERN_ERR "sio: tx timeout\n");
		sio_tx_inprogress = 0;
	}
}

static __inline__ void sio_xmit_do(unsigned int ch)
{
	sio_tx_inprogress = 1;
	sio_tx_last = jiffies;
	wr_reg(OKI_SIOBUF, ch);
}

static __inline__ int sio_write_room(struct tty_struct *tty)
{
	return putp >= getp ? (sizeof(wbuf) - (long) putp + (long) getp) : ((long) getp - (long) putp - 1);
}

static __inline__  int sio_tx_empty(void) {
	return (putp == getp);
}

#define ALL_ERRS (OKI_SIOSTA_OERR | OKI_SIOSTA_PERR | OKI_SIOSTA_FERR)

static void sio_rx_int(void)
{
	unsigned int flag;
	unsigned int ch;
	unsigned int status;

	if (!sio_tty) {
		disable_irq(IRQ_SIO);
		wr_reg(OKI_SIOSTA, OKI_SIOSTA_RVIRQ);
		return;
	}

	do {
		status = rd_reg(OKI_SIOSTA);
		flag = TTY_NORMAL;

		if ((status & OKI_SIOSTA_RVIRQ) == 0)
			break;

		ch = rd_reg(OKI_SIOBUF);

		wr_reg(OKI_SIOSTA, OKI_SIOSTA_RVIRQ | ALL_ERRS);

		if ((status &  OKI_SIOSTA_OERR) && 1)
			tty_insert_flip_char(sio_tty, 0, TTY_OVERRUN);

		if (status & OKI_SIOSTA_PERR)
			flag = TTY_PARITY;
		else if (flag & OKI_SIOSTA_FERR )
			flag = TTY_FRAME;

		if (sio_tty->flip.count >= TTY_FLIPBUF_SIZE) {
			printk("calling %p\n", sio_tty->flip.tqueue.routine);
			sio_tty->flip.tqueue.routine((void *)sio_tty);
		}

		tty_insert_flip_char(sio_tty, ch, flag);
	} while(1);

	tty_flip_buffer_push(sio_tty);
}

static void sio_send_xchar(struct tty_struct *tty, char ch)
{
	// todo = fix
	//x_char = ch;
	//enable_irq(IRQ_CONTX);
}

static void sio_throttle(struct tty_struct *tty)
{
	if (I_IXOFF(tty))
		sio_send_xchar(tty, STOP_CHAR(tty));
}

static void sio_unthrottle(struct tty_struct *tty)
{
	if (I_IXOFF(tty)) {
		if (x_char)
			x_char = 0;
		else
			sio_send_xchar(tty, START_CHAR(tty));
	}
}

/* sio_tx_frombuff
 *
 * returns 0 if it finds there is nothing more to do, else 1 if there
 * is the oportunity to do something
*/

static int sio_tx_frombuff(void)
{
	if (sio_tx_stopped)
		return 0;

	if (x_char) {
		sio_xmit_do(x_char);
		x_char = 0;
		return 1;
	}

	if (sio_tx_empty())
		return 0;

	sio_xmit_do(*getp);

	if (++getp >= wbuf + sizeof(wbuf))
		getp = wbuf;

	return 1;
}

static void sio_tx_int(void)
{
	unsigned long flags;

	local_irq_save(flags);

	while (1) {
		sio_xmit_check();

		if (sio_xmit_locked() || !sio_xmit_ready())
			break;

		if (!sio_tx_frombuff())
			break;
	}

	local_irq_restore(flags);

	if (sio_tty)
		wake_up_interruptible(&sio_tty->write_wait);
}

static void sio_irq(int irq, void *dev, struct pt_regs *regs)
{
	while (1) {
		unsigned int status = rd_reg(OKI_SIOSTA);

		if (!(status & OKI_SIOSTA_ANYIRQ))
			break;

		if (status & OKI_SIOSTA_TRIRQ)
			sio_tx_int();

		if (status & OKI_SIOSTA_RVIRQ)
			sio_rx_int();
	}
}

static inline int sio_xmit(int ch)
{
	unsigned long flags;

	dbg_tx("%s: ch %02x (STA %02x)\n", __FUNCTION__, ch, rd_reg(OKI_SIOSTA));

	local_irq_save(flags);

	/* decided wether we can write this directly to the uart
	 * or wether we are going to have to buffer it */

	printk(KERN_DEBUG "%s: ready %d, locked %d, xchar %02x\n",
	       __FUNCTION__, sio_xmit_ready(), sio_xmit_locked(), x_char);

	if (sio_xmit_ready() && !sio_xmit_locked() && x_char == 0) {
		sio_xmit_do(ch);
		goto exit;
	}

	if (putp + 1 == getp ||
	    (putp + 1 == wbuf + sizeof(wbuf) && getp == wbuf)) {
		local_irq_restore(flags);
		return 0;
	}

	*putp = ch;

	if (++putp >= wbuf + sizeof(wbuf))
		putp = wbuf;

 exit:
	local_irq_restore(flags);
	return 1;
}

static int sio_write(struct tty_struct *tty, int from_user,
		     const u_char * buf, int count)
{
	unsigned long flags;
	int i;

	dbg("%s(%p,%d,%p,%d)\n", __FUNCTION__,
	       tty, from_user, buf, count);

	if (from_user && verify_area(VERIFY_READ, buf, count))
		return -EINVAL;

	local_irq_save(flags);
	sio_xmit_check();
	local_irq_restore(flags);

	for (i = 0; i < count; i++) {
		char ch;
		if (from_user)
			__get_user(ch, buf + i);
		else
			ch = buf[i];

		if (!sio_xmit(ch))
			break;
	}

	dbg("%s: returning %d\n", __FUNCTION__, i);
	return i;
}

static void sio_put_char(struct tty_struct *tty, u_char ch)
{
	sio_xmit(ch);
}

static int sio_chars_in_buffer(struct tty_struct *tty)
{
	return sizeof(wbuf) - sio_write_room(tty);
}

static void sio_flush_buffer(struct tty_struct *tty)
{
	//disable_irq(IRQ_CONTX);
	putp = getp = wbuf;
	//if (x_char)
	//enable_irq(IRQ_CONTX);
}

static inline void sio_set_cflag(int cflag)
{
	int h_lcr, baud, quot;

	switch (cflag & CSIZE) {
	case CS7:
		h_lcr = OKI_SIOCON_LN7;
		break;
	default: /* CS8 */
		h_lcr = 0x0;
		break;

	}
	if (cflag & PARENB)
		h_lcr |= OKI_SIOCON_EVN | OKI_SIOCON_PEN;

	if (cflag & PARODD)
		h_lcr |= OKI_SIOCON_PEN;

	if (!(cflag & CSTOPB))
		h_lcr |= OKI_SIOCON_TSTB;

	switch (cflag & CBAUD) {
	case B200:	baud = 200;		break;
	case B300:	baud = 300;		break;
	case B1200:	baud = 1200;		break;
	case B1800:	baud = 1800;		break;
	case B2400:	baud = 2400;		break;
	case B4800:	baud = 4800;		break;
	default:
	case B9600:	baud = 9600;		break;
	case B19200:	baud = 19200;		break;
	case B38400:	baud = 38400;		break;
	case B57600:	baud = 57600;		break;
	case B115200:	baud = 115200;		break;
	}

	quot = 256 - (oki_cclk/(16*baud));

	printk("quot = %d\n", quot);

	wr_reg(OKI_SIOBCN, 0x00);
	wr_reg(OKI_SIOCON, 0x00);

	wr_reg(OKI_SIOBT,  quot);
	wr_reg(OKI_SIOCON, h_lcr);

	/* set for loopback test... */
	if (0)
		wr_reg(OKI_SIOTCN, OKI_SIOTCN_LBTST);
	else
		wr_reg(OKI_SIOTCN, 0x00);

	wr_reg(OKI_SIOBCN, OKI_SIOBCN_BGRUN);

	dbg("%s: STA=%02x. CON=%02x, BCN=%02x, TCN=%02x, OBT=%02x\n",
	    __FUNCTION__,  rd_reg(OKI_SIOSTA), rd_reg(OKI_SIOCON),
	    rd_reg(OKI_SIOBCN), rd_reg(OKI_SIOTCN), rd_reg(OKI_SIOBT));
}

static void sio_set_termios(struct tty_struct *tty, struct termios *old)
{
	if (old && tty->termios->c_cflag == old->c_cflag)
		return;
	sio_set_cflag(tty->termios->c_cflag);
}


static void sio_stop(struct tty_struct *tty)
{
	sio_tx_stopped = 1;
}

static void sio_start(struct tty_struct *tty)
{
	unsigned long flags;

	local_irq_save(flags);
	sio_tx_stopped = 0;
	sio_tx_frombuff();
	local_irq_restore(flags);
}

static void sio_wait_until_sent(struct tty_struct *tty, int timeout)
{
	int orig_jiffies = jiffies;

	printk("%s:\n", __FUNCTION__);

	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;

		if (sio_tx_empty())
			break;
	}
	current->state = TASK_RUNNING;
}

static int sio_open(struct tty_struct *tty, struct file *filp)
{
	int line;

	dbg("%s:\n", __FUNCTION__);

	MOD_INC_USE_COUNT;
	line = MINOR(tty->device) - tty->driver.minor_start;
	if (line) {
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}

	tty->driver_data = NULL;

	tty->low_latency = 1;  // simple test

	if (!sio_tty)
		sio_tty = tty;

	dbg("%s: STA=%02x. CON=%02x, BCN=%02x, TCN=%02x, BT=%02x\n",
	    __FUNCTION__,  rd_reg(OKI_SIOSTA), rd_reg(OKI_SIOCON),
	    rd_reg(OKI_SIOBCN), rd_reg(OKI_SIOTCN), rd_reg(OKI_SIOBT));

	enable_irq(IRQ_SIO);
	sio_use_count++;

	return 0;
}

static void sio_close(struct tty_struct *tty, struct file *filp)
{
	dbg("%s:\n", __FUNCTION__);

	dbg("%s: STA=%02x. CON=%02x, BCN=%02x, TCN=%02x, BT=%02x\n",
	    __FUNCTION__,  rd_reg(OKI_SIOSTA), rd_reg(OKI_SIOCON),
	    rd_reg(OKI_SIOBCN), rd_reg(OKI_SIOTCN), rd_reg(OKI_SIOBT));

	if (!--sio_use_count) {
		dbg("%s: closing down\n", __FUNCTION__);
		sio_wait_until_sent(tty, 0);
		disable_irq(IRQ_SIO);
		sio_tty = NULL;
	}

	MOD_DEC_USE_COUNT;
}

static int sio_ioctl_getserialinfo(struct serial_struct *retinfo)
{
	struct serial_struct tmp;

	memset(&tmp, 0, sizeof(tmp));

	tmp.flags		= 0;
	tmp.xmit_fifo_size	= 0;
	tmp.baud_base		= oki_cclk / 16;
	tmp.custom_divisor	= 0;
	tmp.irq			= IRQ_SIO;

	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;

	return 0;
}

static int sio_ioctl_setserialinfo(struct serial_struct *retinfo)
{

	return -EINVAL;
}

static int sio_get_lsr(unsigned int *arg)
{
	unsigned int val = 0;

	copy_to_user(arg, val, sizeof(val));

	return 0;
}


static int sio_ioctl(struct tty_struct *tty, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	if (cmd == TIOCGSERIAL)
		return sio_ioctl_getserialinfo((struct serial_struct *)arg);

	if (cmd == TIOCSSERIAL)
		return sio_ioctl_setserialinfo((struct serial_struct *)arg);

#if 0
	if (cmd == TIOCSERGETLSR)
		return sio_get_lsr((unsigned int *) arg);
#endif

	printk(KERN_ERR "unhandled ioctl(%08x)\n", cmd);

	return n_tty_ioctl(tty, file, cmd, arg);
	//return -EIO;
}

static struct tty_driver sio_driver = {
	magic:		TTY_DRIVER_MAGIC,
	driver_name:	"serial",
	name:		SERIAL_ML67XNAME,
	major:		SERIAL_ML67XMAJOR,
	minor_start:	SERIAL_ML67XMINOR,
	num:		1,
	type:		TTY_DRIVER_TYPE_SERIAL,
	subtype:	SERIAL_TYPE_NORMAL,
	refcount:	&sio_refcount,
	table:		sio_table,

	open:		sio_open,
	close:		sio_close,
	write:		sio_write,
	put_char:	sio_put_char,
	write_room:	sio_write_room,
	chars_in_buffer: sio_chars_in_buffer,
	flush_buffer:	sio_flush_buffer,
	throttle:	sio_throttle,
	unthrottle:	sio_unthrottle,
	send_xchar:	sio_send_xchar,
	set_termios:	sio_set_termios,
	ioctl:		sio_ioctl,
	stop:		sio_stop,
	start:		sio_start,
	flags:		TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS,
	wait_until_sent: sio_wait_until_sent,
};

static struct tty_driver callout_driver;

static __inline__ void sio_init_gpio(void)
{
	unsigned long flags;
	unsigned long gpctl;

	local_irq_save(flags);

	gpctl = __raw_readw(OKI_GPCTL);
	gpctl |= OKI_GPCTL_SIO;
	__raw_writew(gpctl, OKI_GPCTL);

	dbg("%s: new gpctl %04lx\n", __FUNCTION__, gpctl);
	dbg("%s: CLKSTP=%08x\n", __FUNCTION__, __raw_readl(0xB8000004));
	dbg("%s: BCKCTL=%04x\n", __FUNCTION__, __raw_readw(0xB7000004));

	local_irq_restore(flags);
}

static int __init sio_init(void)
{
	int baud = B4800;

	printk("Oki SIO driver, (c) 2005 Simtec Electronics\n");

	sio_init_gpio();

	sio_driver.termios        = sio_termios;
	sio_driver.termios_locked = sio_termios_locked;
	sio_driver.init_termios   = tty_std_termios;
	sio_driver.init_termios.c_cflag = baud | CS8 | CREAD | HUPCL | CLOCAL;

	/* initialise the port by setting the cflags */
	sio_set_cflag(sio_driver.init_termios.c_cflag);

	callout_driver         = sio_driver;
	callout_driver.name    = SERIAL_ML67XAUXNAME;
	callout_driver.major   = SERIAL_ML67XAUXMAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (request_irq(IRQ_SIO, sio_irq, 0, "sio", NULL)) {
		printk(KERN_ERR "Couldn't get irq for sio");
		return -ENXIO;
	}

	if (tty_register_driver(&sio_driver))
		printk(KERN_ERR "Couldn't register ml67xxxx serial driver\n");

	if (tty_register_driver(&callout_driver))
		printk(KERN_ERR "Couldn't register ml67xxxx callout driver\n");

	return 0;
}

static void __exit sio_fini(void)
{
	unsigned long flags;
	int ret;

	save_flags(flags);
	cli();

	ret = tty_unregister_driver(&callout_driver);
	if (ret)
		printk(KERN_ERR "Unable to unregister ml67xxxx callout driver "
			"(%d)\n", ret);
	ret = tty_unregister_driver(&sio_driver);
	if (ret)
		printk(KERN_ERR "Unable to unregister ml67xxxx driver (%d)\n",
			ret);

	free_irq(IRQ_SIO, NULL);
	restore_flags(flags);
}

module_init(sio_init);
module_exit(sio_fini);

#ifdef CONFIG_SERIAL_ML67XCONSOLE
/************** console driver *****************/

static void sio_console_writech(unsigned int ch)
{
	unsigned long flags;

	while (1) {
		local_irq_save(flags);
		sio_xmit_check();

		if (sio_xmit_ready()) {
			sio_xmit_do(ch);
			local_irq_restore(flags);
			return;
		}

		local_irq_restore(flags);
	}
}

static void sio_console_write(struct console *co, const char *s, u_int count)
{
	unsigned long flags;
	int i;

	local_irq_save(flags);
	sio_tx_lock();
	local_irq_restore(flags);

	for (i = 0; i < count; i++) {
		sio_console_writech(s[i]);

		if (s[i] == '\n')
			sio_console_writech('\r');
	}

	/* unlock the tx queue, and check to see if there is anything
	 * more to process
	 */

	local_irq_save(flags);
	sio_tx_unlock();
	sio_tx_frombuff();
	local_irq_restore(flags);
}

static kdev_t sio_console_device(struct console *c)
{
	return MKDEV(SERIAL_ML67XMAJOR, SERIAL_ML67XMINOR);
}

static int __init sio_console_setup(struct console *co, char *options)
{
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int cflag = CREAD | HUPCL | CLOCAL;

	if (options) {
		char *s = options;
		baud = simple_strtoul(options, NULL, 10);
		while (*s >= '0' && *s <= '9')
			s++;
		if (*s)
			parity = *s++;
		if (*s)
			bits = *s - '0';
	}

	/*
	 *    Now construct a cflag setting.
	 */
	switch (baud) {
	case 1200:
		cflag |= B1200;
		break;
	case 2400:
		cflag |= B2400;
		break;
	case 4800:
		cflag |= B4800;
		break;
	case 9600:
		cflag |= B9600;
		break;
	case 19200:
		cflag |= B19200;
		break;
	case 38400:
		cflag |= B38400;
		break;
	case 57600:
		cflag |= B57600;
		break;
	case 115200:
		cflag |= B115200;
		break;
	default:
		cflag |= B9600;
		break;
	}
	switch (bits) {
	case 7:
		cflag |= CS7;
		break;
	default:
		cflag |= CS8;
		break;
	}
	switch (parity) {
	case 'o':
	case 'O':
		cflag |= PARODD;
		break;
	case 'e':
	case 'E':
		cflag |= PARENB;
		break;
	}

	co->cflag = cflag;
	sio_set_cflag(cflag);

	sio_console_write(NULL, "\e[2J\e[Hboot ", 12);
	if (options)
		sio_console_write(NULL, options, strlen(options));
	else
		sio_console_write(NULL, "no options", 10);
	sio_console_write(NULL, "\n", 1);

	return 0;
}


static struct console sio_cons =
{
	name:		SERIAL_ML67XNAME,
	write:		sio_console_write,
	device:		sio_console_device,
	setup:		sio_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init sio_console_init(void)
{
	register_console(&sio_cons);
}

#endif	/* CONFIG_SERIAL_ML67XCONSOLE */

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
