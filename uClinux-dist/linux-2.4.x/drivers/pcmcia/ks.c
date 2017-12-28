/****************************************************************************/

/*
 *	ks.c -- PCMCIA socket services for KS8695/SE4200 board.
 *
 *	(C) Copyright 2004-2005, Greg Ungerer <gerg@snapgear.com>
 */

/****************************************************************************/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/cache.h>
#include <linux/mm.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgalloc.h>

#include <pcmcia/version.h>
#include <pcmcia/bus_ops.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>

/****************************************************************************/

#undef DEBUG

/****************************************************************************/

#define	KS_NAME	"ks"

MODULE_DESCRIPTION("Cyberguard/SE4200 (KS8695) socket service driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.com>");

/****************************************************************************/

#define	KS_ADDRMAP8	0x03000000	/* Map CF window here - 8bit*/
#define	KS_ADDRMAP16	0x03010000	/* Map CF window here - 16bit*/
#define	KS_ADDRSIZE	0x00010000	/* It is 64k in size */

/*
 *	The two memory types we can deal with (IO and ATTR) are mapped
 *	into the same memory region, it being parititoned up for each.
 */
#define	KS_IO_OFFSET	0x800		/* A11 must be logic 1 */
#define	KS_IO_SIZE	0x800

static void *ks_map8bit;
static unsigned int ks_8bit;
static void *ks_map16bit;
static unsigned int ks_16bit;

/*
 *	Keep some of the GPIO address info handy. We need to get at them
 *	from time to time in this code.
 */
#define	VIOADDR(r)	(IO_ADDRESS(KS8695_IO_BASE) + (r))

static volatile unsigned int *ks_mode;
static volatile unsigned int *ks_ctrl;
static volatile unsigned int *ks_data;
static volatile unsigned int *ks_intack;

/*
 *	PCMCIA/CF slot card state info.
 */
#define	KS_CARD_PRESENT		0x1

static unsigned int ks_state;

/****************************************************************************/

static void (*ks_callback)(void *, unsigned int);
static void *ks_callback_info;

/****************************************************************************/

#if 0
void dump(void *addr)
{
	volatile unsigned int *p;
	int i;

	printk("DUMP:");
	for (i = 0, p = addr; (i < 32); i++, p++) {
		if ((i % 4) == 0) printk("\n%08x:  ", (int)p);
		printk("%08x ", *p);
	}
	printk("\n");
}
#endif

/****************************************************************************/

static __inline__ unsigned int MADDR(unsigned int addr)
{
	if (addr & 0x2)
		addr += 0x1000;
	addr &= ~0x3;
	return addr;
}

/****************************************************************************/

/*
 *	Using a scheduled task may seem like overkill here. But the
 *	socket callback handler wants to do scheduled waits, so we
 *	need to be in task context. Otherwise we could have just called
 *	this from the debounce timer handler.
 */
static void ks_notify(void *arg)
{
#ifdef DEBUG
	printk("ks_notify()\n");
#endif
	if (ks_state & SS_DETECT)
		printk("KS: CF card insertion detected\n");
	else
		printk("KS: CF card removal detected\n");
	ks_callback(ks_callback_info, ks_state);
}

static struct tq_struct ks_hotplugtask = {
	.routine = ks_notify,
};

/****************************************************************************/

/*
 *	Wait for the card insertion/removal to settle down before
 *	we check what state it is in. Hopefully this will debounce
 *	all the noise of the insert/remove, and leave us in one
 *	clean state.
 */
static struct timer_list ks_debounce;
static int ks_doingdebounce;

static void ks_hotplug(unsigned long arg)
{
	unsigned int state;

#ifdef DEBUG
	printk("ks_hotplug()\n");
#endif

	ks_doingdebounce = 0;

	state = (*ks_data & 0x2) ? 0 : (SS_DETECT | SS_READY | SS_3VCARD);

	if (state != ks_state) {
		ks_state = state;
		schedule_task(&ks_hotplugtask);
	}
}

/****************************************************************************/

static void ks_hotplugisr(int irq, void *dev_id, struct pt_regs *regs)
{
#ifdef DEBUG
	printk("ks_hotplugisr()\n");
#endif
	/* Acknowledge the interrupt at interrupt conrtoller */
	*ks_intack = 0x8;

	if (ks_doingdebounce == 0) {
		ks_doingdebounce++;
		init_timer(&ks_debounce);
		ks_debounce.function = ks_hotplug;
		ks_debounce.expires = jiffies + HZ/2;
		add_timer(&ks_debounce);
	}
}

/****************************************************************************/

static int ks_hwinited;

void ks_hwsetup(void)
{
	int i;

#ifdef DEBUG
	printk("ks_hwinit()\n");
#endif

	if (ks_hwinited)
		return;

	/*
	 * Enable IO/ext chip select regions to address 0x03000000.
	 */
	*((unsigned int *) VIOADDR(KS8695_IO_CTRL0)) = 0xc0300e4b;
	*((unsigned int *) VIOADDR(KS8695_IO_CTRL1)) = 0xc0701e4b;
	*((unsigned int *) VIOADDR(KS8695_MEM_GENERAL)) |= 0x000f0000;

	/*
	 * The external bus interface to the CF Wifi card just doesn't
	 * work right if the bus FIFO's are enabled. I couldn't find 
	 * any work around that would fix this (certainly not flushing
	 * the cache, walking bus addresses, nothing).
	 */
	*((unsigned int *) VIOADDR(KS8695_SDRAM_BUFFER)) &= ~0x00c00000;

	ks_mode = (volatile unsigned int *) VIOADDR(KS8695_GPIO_MODE);
	ks_ctrl = (volatile unsigned int *) VIOADDR(KS8695_GPIO_CTRL);
	ks_data = (volatile unsigned int *) VIOADDR(KS8695_GPIO_DATA);
	ks_intack = (volatile unsigned int *) VIOADDR(KS8695_INT_STATUS);

	/*
	 * Setup GPIO lines 1 and 2 as interrupt inputs.
	 * GPIO1 is used as the CF card detect (for hot plugging),
	 * and GPIO2 is the device interrupt from the CF slot.
	 */
	*ks_mode &= ~6;
	*ks_ctrl = (*ks_ctrl & ~0xff0) | 0x8e0;

	if (request_irq(3, ks_hotplugisr, SA_INTERRUPT, "pcmcia/cf", (void*)3))
		printk("KS: failed to get hotplug interrupt %d?\n", 3);

	/*
	 * Reset the CF device, and let it start up.
	 */
	*ks_data |= 0x80;
	*ks_mode |= 0x80;

	for (i = 0; (i < 10); i++)
		udelay(1000);
	*ks_data &= ~0x80;
	for (i = 0; (i < 100); i++)
		udelay(1000);
	
	ks_state = (*ks_data & 0x2) ? 0 : (SS_DETECT | SS_READY | SS_3VCARD);

	ks_hwinited = 1;
}

/****************************************************************************/

unsigned int ks_buf[0x10000];

void ks_invalidate_buffer(void)
{
#if 0
        static int z = 0;
        ks_buf[z++ & 0xffff] += 1;
        ks_buf[z++ & 0xffff] += 2;
        ks_buf[z++ & 0xffff] += 3;
        ks_buf[z++ & 0xffff] += 4;
#endif
#if 0
        static int z = 0;
        *((volatile unsigned int *) (0xc0010000 + (z++ & 0xffff)));
        *((volatile unsigned int *) (0xc0010000 + (z++ & 0xffff)));
        *((volatile unsigned int *) (0xc0010000 + (z++ & 0xffff)));
        *((volatile unsigned int *) (0xc0010000 + (z++ & 0xffff)));
        *((volatile unsigned int *) (0xc0010000 + (z++ & 0xffff)));
        *((volatile unsigned int *) (0xc0010000 + (z++ & 0xffff)));
        *((volatile unsigned int *) (0xc0010000 + (z++ & 0xffff)));
        *((volatile unsigned int *) (0xc0010000 + (z++ & 0xffff)));
#endif
#if 0
        static int off = 0;
        *((volatile unsigned char *) (ks_mape2 + (off++ & 0x1ffff)));
#endif
#if 0
	int i;
	for (i = 0; (i < CBSIZE); i++)
		cachebuster[i+1] = cachebuster[i] + 1;
#endif
#if 0
	cpu_dcache_invalidate_range(ks_8bit, KS_ADDRSIZE);
	cpu_dcache_invalidate_range(ks_16bit, KS_ADDRSIZE);
#endif
#if 0
	flush_cache_all();
#endif
}

/****************************************************************************/

u32 ks_in(void *bus, u32 port, s32 sz)
{
	u32 val, base;

	base = (sz) ? ks_16bit : ks_8bit;
	ks_invalidate_buffer();
	val = readl(MADDR(base + port));
	ks_invalidate_buffer();
	if (sz == 0)
		val &= 0xff;
	else if (sz == 1)
		val &= 0xffff;

#ifdef DEBUG_
	printk("ks_in(bus=%x,port=%x,sz=%x)=0x%04x\n", (int)bus, port, sz, val);
#endif
	return val;
}

/****************************************************************************/

void ks_ins(void *bus, u32 port, void *buf, u32 count, s32 sz)
{
	u8 *bbuf = (u8 *) buf;
	u16 *wbuf = (u16 *) buf;
	u32 *lbuf = (u32 *) buf;
	u32 val, base;

#ifdef DEBUG_
	printk("ks_ins(bus=%x,port=%x,buf=%x,count=%d,sz=%x)\n",
		(int)bus, port, (int)buf, count, sz);
#endif

	base = (sz) ? ks_16bit : ks_8bit;

	for (; (count); count--) {
		ks_invalidate_buffer();
		val = readl(MADDR(base + port));
		ks_invalidate_buffer();
		if (sz == 0)
			*bbuf++ = val;
		else if (sz == 1)
			*wbuf++ = cpu_to_le16(val);
		else
			*lbuf++ = cpu_to_le32(val);
	}
}

/****************************************************************************/

void ks_out(void *bus, u32 val, u32 port, s32 sz)
{
	u32 base;

#ifdef DEBUG_
	printk("ks_out(bus=%x,val=%x,port=%x,sz=%x)\n",
		(int)bus, val, port, sz);
#endif

	base = (sz) ? ks_16bit : ks_8bit;
	ks_invalidate_buffer();
	writel(val, MADDR(base + port));
	ks_invalidate_buffer();
}

/****************************************************************************/

void ks_outs(void *bus, u32 port, void *buf, u32 count, s32 sz)
{
	u8 *bbuf = (u8 *) buf;
	u16 *wbuf = (u16 *) buf;
	u32 *lbuf = (u32 *) buf;
	u32 base, val;

#ifdef DEBUG_
	printk("ks_outs(bus=%x,port=%x,buf=%x,count=%d,sz=%x)\n",
		(int)bus, port, (int)buf, count, sz);
#endif

	base = (sz) ? ks_16bit : ks_8bit;

	for (; (count); count--) {
		if (sz == 0)
			val = *bbuf++;
		else if (sz == 1)
			val = cpu_to_le16(*wbuf++);
		else
			val = cpu_to_le32(*lbuf++);
		ks_invalidate_buffer();
		writel(val, MADDR(base + port));
		ks_invalidate_buffer();
	}
}

/****************************************************************************/

static void *ks_ioremap(void *bus, u_long ofs, u_long sz)
{
#ifdef DEBUG
	printk("ks_ioremap(bus=%x,ofs=%x,sz=%x)\n",
		(int)bus, (int)ofs, (int)sz);
#endif

	return NULL;
}

/****************************************************************************/

static void ks_iounmap(void *bus, void *addr)
{
#ifdef DEBUG
	printk("ks_iounmap(bus=%x,addr=%x)\n", (int)bus, (int)addr);
#endif
}

/****************************************************************************/

static u32 ks_readmem(void *bus, void *addr, s32 sz)
{
	u32 val, base;
	u32 ofs = (u32) addr;

	base = (sz) ? ks_16bit : ks_8bit;
	val = readl(MADDR(base + ofs));
	if (sz == 0)
		val &= 0xff;
	else if (sz == 1)
		val &= 0xffff;
	ks_invalidate_buffer();

#ifdef DEBUG
	printk("ks_readmem(bus=%x,addr=%x,sz=%x)=0x%04x\n",
		(int)bus, (int)addr, sz, val);
#endif

	return val;
}

/****************************************************************************/

static void ks_copy_from(void *bus, void *d, void *s, u32 count)
{
	u8 *dbuf = d;
	u32 ofs = (u32) s;

#ifdef DEBUG
	printk("ks_copy_from(bus=%x,d=%x,s=%x,count=%d)\n",
		(int)bus, (int)d, (int)s, count);
#endif

	for (; (count); count--) {
		*dbuf++ = readl(MADDR(ks_8bit + ofs++));
		ks_invalidate_buffer();
	}
}

/****************************************************************************/

static void ks_writemem(void *bus, u32 val, void *addr, s32 sz)
{
	u32 ofs = (u32) addr;
	u32 base;

#ifdef DEBUG
	printk("ks_writemem(bus=%x,val=%x,addr=%x,sz=%x)\n",
		(int)bus, val, (int)addr, sz);
#endif

	base = (sz) ? ks_16bit : ks_8bit;
	writel(val, MADDR(base + ofs));
	ks_invalidate_buffer();
}

/****************************************************************************/

static void ks_copy_to(void *bus, void *d, void *s, u32 count)
{
	u8 *sbuf = s;
	u32 ofs = (u32) d;

#ifdef DEBUG
	printk("ks_copy_from(bus=%x,d=%x,s=%x,count=%d)\n",
		(int)bus, (int)d, (int)s, count);
#endif

	for (; (count); count--) {
		writel(*sbuf++, MADDR(ks_8bit + ofs++));
		ks_invalidate_buffer();
	}
}

/****************************************************************************/

/*
 *	Wee need to vector through our local handler first, in case no real
 *	handler has been lodged yet.
 */
static void (*ks_handler)(int, void *, struct pt_regs *);

static void ks_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	if (ks_handler)
		ks_handler(irq, dev_id, regs);
}

static int ks_request_irq(void *bus, u_int irq, void (*handler)(int, void *, struct pt_regs *), u_long flags, const char *device, void *dev_id)
{
#ifdef DEBUG
	printk("ks_request_irq(bus=%x,irq=%d,handler=%x,flags=%x,"
		"device=%x,dev_id=%x)\n", (int)bus, irq, (int)handler,
		(int)flags, (int)device, (int)dev_id);
#endif

	ks_handler = handler;
	if (request_irq(irq, ks_isr, flags, device, dev_id)) {
		printk("KS: failed to get interrupt %d?\n", irq);
		return -EBUSY;
	}
	return 0;
}

/****************************************************************************/

static void ks_free_irq(void *bus, u_int irq, void *dev_id)
{
#ifdef DEBUG
	printk("ks_free_irq(bus=%x,irq=%d,dev_id=%x)\n",
		(int)bus, irq, (int)dev_id);
#endif
	ks_handler = NULL;
	free_irq(irq, dev_id);
}

/****************************************************************************/

/*
 *	When using other drivers as modules we need to export these
 *	IO access functions.
 */
EXPORT_SYMBOL(ks_out);
EXPORT_SYMBOL(ks_in);
EXPORT_SYMBOL(ks_outs);
EXPORT_SYMBOL(ks_ins);

/****************************************************************************/

static struct bus_operations ks_busops = {
	.b_in = ks_in,
	.b_ins = ks_ins,
	.b_out = ks_out,
	.b_outs = ks_outs,
	.b_ioremap = ks_ioremap,
	.b_iounmap = ks_iounmap,
	.b_read = ks_readmem,
	.b_write = ks_writemem,
	.b_copy_from = ks_copy_from,
	.b_copy_to = ks_copy_to,
	.b_request_irq = ks_request_irq,
	.b_free_irq = ks_free_irq,
};

/****************************************************************************/

static socket_cap_t ks_socket_cap = {
	.features = SS_CAP_PCCARD | SS_CAP_STATIC_MAP | SS_CAP_VIRTUAL_BUS,
	.irq_mask = 0xffff,
	.pci_irq = 4,
	.map_size = KS_IO_SIZE,
	.bus = &ks_busops,
};

/****************************************************************************/

static int ks_ssinit(unsigned int sock)
{
#ifdef DEBUG
	printk("ks_ssinit(sock=%d)\n", sock);
#endif
	return 0;
}

/****************************************************************************/

static int ks_suspend(unsigned int sock)
{
#ifdef DEBUG
	printk("ks_suspend(sock=%d)\n", sock);
#endif
	return 0;
}

/****************************************************************************/

static int ks_register_callback(unsigned int sock, void (*handler)(void *, unsigned int), void *info)
{
#ifdef DEBUG
	printk("ks_register_callback(sock=%d,handler=%x,info=%x)\n",
		sock, (int)handler, (int)info);
#endif
	ks_callback = handler;
	ks_callback_info = info;
	return 0;
}

/****************************************************************************/

static int ks_inquire_socket(unsigned int sock, socket_cap_t *cap)
{
#ifdef DEBUG
	printk("ks_inquire_socket(sock=%d,cap=%x)\n", sock, (int)cap);
#endif
	ks_socket_cap.io_offset = KS_IO_OFFSET;
	*cap = ks_socket_cap;
	return 0;
}

/****************************************************************************/

static int ks_get_status(unsigned int sock, unsigned int *value)
{
#ifdef DEBUG
	printk("ks_get_status(sock=%d,value=%x)\n", sock, (int)value);
#endif
	*value = ks_state;
	return 0;
}

/****************************************************************************/

static int ks_get_socket(unsigned int sock, socket_state_t *state)
{
#ifdef DEBUG
	printk("ks_get_socket(sock=%d,state=%x)\n", sock, (int)state);
#endif
	return 0;
}

/****************************************************************************/

static int ks_set_socket(unsigned int sock, socket_state_t *state)
{
#ifdef DEBUG
	printk("ks_set_socket(sock=%d,state=%x)\n", sock, (int)state);
#endif
	return 0;
}

/****************************************************************************/

static int ks_get_io_map(unsigned int sock, struct pccard_io_map *io)
{
#ifdef DEBUG
	printk("ks_get_io_map(sock=%d,io=%x)\n", sock, (int)io);
#endif
	return 0;
}

/****************************************************************************/

static int ks_set_io_map(unsigned int sock, struct pccard_io_map *io)
{
#ifdef DEBUG
	printk("ks_set_io_map(sock=%d,io=%x)\n", sock, (int)io);
#endif
	return 0;
}

/****************************************************************************/

static int ks_get_mem_map(unsigned int sock, struct pccard_mem_map *mem)
{
#ifdef DEBUG
	printk("ks_get_mem_map(sock=%d,mem=%x)\n", sock, (int)mem);
#endif
	return 0;
}

/****************************************************************************/

static int ks_set_mem_map(unsigned int sock, struct pccard_mem_map *mem)
{
#ifdef DEBUG
	printk("ks_set_mem_map(sock=%d,mem=%x)\n", sock, (int)mem);
#endif

	return 0;
}

/****************************************************************************/

static void ks_proc_setup(unsigned int sock, struct proc_dir_entry *base)
{
#ifdef DEBUG
	printk("ks_proc_setup(sock=%d,base=%x)\n", sock, (int)base);
#endif
}

/****************************************************************************/

static struct pccard_operations ks_operations = {
	.init = ks_ssinit,
	.suspend = ks_suspend,
	.register_callback = ks_register_callback,
	.inquire_socket = ks_inquire_socket,
	.get_status = ks_get_status,
	.get_socket = ks_get_socket,
	.set_socket = ks_set_socket,
	.get_io_map = ks_get_io_map,
	.set_io_map = ks_set_io_map,
	.get_mem_map = ks_get_mem_map,
	.set_mem_map = ks_set_mem_map,
	.proc_setup = ks_proc_setup,
};

/****************************************************************************/

/*
 *	This driver also has a character device interface. Very useful
 *	for debugging, but we really need it to implement the ioctl
 *	support for enabling and disabling ethernet and Wifi hardware
 *	ports.
 */
#define	KS_MAJOR	120

/****************************************************************************/

static ssize_t ks_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
	unsigned short *wbuf = (unsigned short *) buf;
	unsigned int addr, mindev;
	int i, end, width;
	u32 val;

#ifdef DEBUG_
	printk("ks_read(filp=%x,buf=%x,count=%d,ppos=%x)\n",
		(int)filp, (int)buf, count, (int)ppos);
#endif

	mindev = MINOR(filp->f_dentry->d_inode->i_rdev);

	width = (mindev & 1) + 1;
	addr = (unsigned int) ((width == 1) ? ks_8bit : ks_16bit);

	/* Minor devices: 0 -> I/O,  1 -> ATTR,  2 -> MEM */
	addr += (mindev & 0x6) ? 0 : KS_IO_OFFSET;

	if ((i = *ppos) >= KS_IO_SIZE)
		return 0;
	if ((i + count) > KS_IO_SIZE)
		count = KS_IO_SIZE - i;
	for (end = i + count; (i < end); i += width) {
		val = readl(MADDR(addr + i));
		if (width == 1)
			put_user(((unsigned char) val), buf++);
		else
			put_user(((unsigned short) val), wbuf++);
		ks_invalidate_buffer();
	}

	*ppos = i;
	return count;
}

/****************************************************************************/

static ssize_t ks_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	unsigned short *wbuf = (unsigned short *) buf;
	unsigned int addr, mindev;
	unsigned short wval;
	unsigned char val = 0;
	int i, end, width;
	u32 v;

#ifdef DEBUG
	printk("ks_write(filp=%x,buf=%x,count=%d,ppos=%x)\n",
		(int)filp, (int)buf, count, (int)ppos);
#endif

	mindev = MINOR(filp->f_dentry->d_inode->i_rdev);

	width = (mindev & 1) + 1;
	addr = (unsigned int) ((width == 1) ? ks_8bit : ks_16bit);

	/* Minor devices: 0 -> I/O,  1 -> ATTR,  2 -> MEM */
	addr += (mindev & 0x6) ? 0 : KS_IO_OFFSET;

	if ((i = *ppos) >= KS_IO_SIZE)
		return 0;
	if ((i + count) > KS_IO_SIZE)
		count = KS_IO_SIZE - i;
	for (end = i + count; (i < end); i += width) {
		if (width == 1) {
			get_user(val, buf++);
			v = val;
		} else {
			get_user(wval, wbuf++);
			v = wval;
		}
		writel(v, MADDR(addr + i));
		ks_invalidate_buffer();
	}

	*ppos = i;
	return count;
}

/****************************************************************************/

static loff_t ks_lseek(struct file *file, loff_t offset, int orig)
{
	switch (orig) {
	case 0:
		file->f_pos = offset;
		break;
	case 1:
		file->f_pos += offset;
		break;
	default:
		return -EINVAL;
	}
	return file->f_pos;
}

/****************************************************************************/

static int ks_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
#if 0
	switch (cmd) {

	case ISET_ETH0:
		printk("%s(%d): cannot disable eth0!\n", __FILE__, __LINE__);
		break;

	case ISET_ETH1:
		/* Enable/disable second ethernet port */
		if (arg)
			gpio_line_set(IXP425_GPIO_PIN_3, 0);
		else
			gpio_line_set(IXP425_GPIO_PIN_3, 1);
		break;

	case ISET_WIFI0:
		/* Best I can figure is to hold the device in reset */
		if (arg)
			gpio_line_set(IXP425_GPIO_PIN_6, 1);
		else
			gpio_line_set(IXP425_GPIO_PIN_6, 0);
		break;

	default:
		return -EINVAL;
	}

	return 0;
#endif

	return -EINVAL;
}

/****************************************************************************/

static struct file_operations ks_fops = {
	.read = ks_read,
	.write = ks_write,
	.llseek = ks_lseek,
	.ioctl = ks_ioctl,
};

/****************************************************************************/

int ks_memmap(void)
{
	/* Check if already initialized */
	if (ks_map8bit)
		return 0;

	ks_map8bit = ioremap(KS_ADDRMAP8, KS_ADDRSIZE);
	if (ks_map8bit == NULL) {
		printk("KS: failed to ioremap 8bit memory region?\n");
		return -ENOMEM;
	}

	ks_map16bit = ioremap(KS_ADDRMAP16, KS_ADDRSIZE);
	if (ks_map16bit == NULL) {
		iounmap(ks_map8bit);
		ks_map8bit = NULL;
		printk("KS: failed to ioremap 16bit memory region?\n");
		return -ENOMEM;
	}

	ks_8bit = (unsigned int) ks_map8bit;
	ks_16bit = (unsigned int) ks_map16bit;
	return 0;
}

/****************************************************************************/

static int __init ks_init(void)
{
	servinfo_t serv;
	int rc;

	printk("KS: KS8695/4200 socket service driver\n" \
		"(C) Copyright 2005, Greg Ungerer <gerg@snapgear.com>\n");

	ks_hwsetup();
	if ((rc = ks_memmap()))
		return rc;

	pcmcia_get_card_services_info(&serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk("KS: Card Services version mis-match!\n");
		return -ENODEV;
	}

	if (register_ss_entry(1, &ks_operations)) {
		printk("KS: failed to register socket service?\n");
		return -ENODEV;
	}

	if (register_chrdev(KS_MAJOR, KS_NAME, &ks_fops))
		printk("KS: failed to register debug char device?\n");

	return 0;
}

/****************************************************************************/

static void __exit ks_exit(void)
{
	unregister_ss_entry(&ks_operations);

	if (ks_map8bit)
		iounmap(ks_map8bit);
	if (ks_map16bit)
		iounmap(ks_map16bit);

	unregister_chrdev(KS_MAJOR, KS_NAME);
}

/****************************************************************************/

module_init(ks_init);
module_exit(ks_exit);

/****************************************************************************/
