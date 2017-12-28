/****************************************************************************/

/*
 *	ivpnss.c -- PCMCIA socket services for IXP4xx/iVPN board.
 *
 *	(C) Copyright 2004, Greg Ungerer <gerg@snapgear.com>
 */

/****************************************************************************/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <pcmcia/version.h>
#include <pcmcia/bus_ops.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>

#include <asm/arch/ivpnss.h>

/****************************************************************************/

#define	IVPNSS_NAME	"ivpnss"

MODULE_DESCRIPTION("SnapGear/iVPN (IXP4xx) socket service driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.org>");

/****************************************************************************/

static void *ivpnss_map8bit;
static unsigned int ivpnss_8bit;
static void *ivpnss_map16bit;
static unsigned int ivpnss_16bit;

/*
 *	Set the maximum region size to be 1MB, should be plenty.
 */
#define	IVPNSS_MEM_SIZE	0x00100000
#define	IVPNSS_IO_OFFSET 0x10000

/****************************************************************************/

/*
 *	Code to enablde the memory map to access different regions of
 *	the PCMCIA/CF address space. We can only look into one region
 *	at a time from the CPU address space.
 */
#define	MODE_UNKNOWN	0
#define	MODE_ATTRIBUTE	1
#define	MODE_IO		2
#define	MODE_MEM	3

static int ivpnss_mem_mode;

static void __inline__ ivpnss_enable_attribute(void)
{
	gpio_line_set(IXP425_GPIO_PIN_10, 0);	/*REG*/
	gpio_line_set(IXP425_GPIO_PIN_8, 0);	/*IO/MEM*/
}

static void __inline__ ivpnss_enable_io(void)
{
	gpio_line_set(IXP425_GPIO_PIN_10, 0);	/*REG*/
	gpio_line_set(IXP425_GPIO_PIN_8, 1);	/*IO/MEM*/
}

static void __inline__ ivpnss_enable_mem(void)
{
	gpio_line_set(IXP425_GPIO_PIN_10, 1);	/*REG*/
	gpio_line_set(IXP425_GPIO_PIN_8, 0);	/*IO/MEM*/
}

static void ivpnss_enable_mode(int mode)
{
	switch (mode) {
	case MODE_ATTRIBUTE:
		ivpnss_enable_attribute();
		break;
	case MODE_IO:
		ivpnss_enable_io();
		break;
	default:
		ivpnss_enable_mem();
		break;
	}
}

/****************************************************************************/

static int ivpnss_hwinited;

void ivpnss_hwsetup(void)
{
	int i;

	if (ivpnss_hwinited)
		return;

	/* Setup IXP4xx chip selects, CS1 is for 8bit, CS2 is 16bit */
	*IXP425_EXP_CS1 = 0xbfff3c03;
	*IXP425_EXP_CS2 = 0xbfff3c02;

	/* Setup IRQ line, we use GPIO7 which maps tp IRQ 24 */
	gpio_line_config(IXP425_GPIO_PIN_7,
		(IXP425_GPIO_IN | IXP425_GPIO_FALLING_EDGE));
	gpio_line_isr_clear(IXP425_GPIO_PIN_7);

	/* Setup PCMCIA/CF REG line (for accessing attribute space) */
	gpio_line_config(IXP425_GPIO_PIN_10, IXP425_GPIO_OUT);
	gpio_line_set(IXP425_GPIO_PIN_10, 1);

	/* Setup PCMCIA/CF IO/MEM (confusingly called RD/WR) select */
	gpio_line_config(IXP425_GPIO_PIN_8, IXP425_GPIO_OUT);
	gpio_line_set(IXP425_GPIO_PIN_10, 1);

	/* Setup PCMCIA/CF RESET line, and issue a short reset */
	gpio_line_set(IXP425_GPIO_PIN_6, 0);
	gpio_line_config(IXP425_GPIO_PIN_6, IXP425_GPIO_OUT);
	for (i = 0; (i < 100); i++)
		udelay(1000);
	gpio_line_set(IXP425_GPIO_PIN_6, 1);

	/* Leave the region idling in "attribute" mode */
	ivpnss_mem_mode = MODE_ATTRIBUTE;
	ivpnss_enable_attribute();

	ivpnss_hwinited = 1;
}

/****************************************************************************/

u32 ivpnss_in(void *bus, u32 port, s32 sz)
{
	unsigned long flags;
	u32 val = 0;

	if (port > IVPNSS_IO_OFFSET)
		port -= IVPNSS_IO_OFFSET;

	save_flags(flags); cli();
	ivpnss_enable_io();
	if (sz == 0)
		val = readb(ivpnss_8bit + port);
	else if (sz == 1)
		val = readw(ivpnss_16bit + port);
	else if (sz == 2)
		val = readl(ivpnss_16bit + port);
	else
		printk("%s(%d): bad size, sz=%x\n", __FILE__, __LINE__, sz);
	restore_flags(flags);

#ifdef DEBUG
	printk("ivpnss_in(bus=%x,port=%x,sz=%x)=0x%04x\n", (int)bus, port, sz, val);
#endif
	return val;
}

/****************************************************************************/

void ivpnss_ins(void *bus, u32 port, void *buf, u32 count, s32 sz)
{
	u8 *bbuf = (u8 *) buf;
	u16 *wbuf = (u16 *) buf;
	u32 *lbuf = (u32 *) buf;
	unsigned long flags;

#ifdef DEBUG
	printk("ivpnss_ins(bus=%x,port=%x,buf=%x,count=%d,sz=%x)\n",
		(int)bus, port, (int)buf, count, sz);
#endif

	if (port > IVPNSS_IO_OFFSET)
		port -= IVPNSS_IO_OFFSET;

	if ((sz != 0) && (sz != 1) && (sz != 2)) {
		printk("%s(%d): bad size, sz=%x\n", __FILE__, __LINE__, sz);
		return;
	}

	save_flags(flags); cli();
	ivpnss_enable_io();
	for (; (count); count--) {
		if (sz == 0)
			*bbuf++ = readb(ivpnss_8bit + port);
		else if (sz == 1)
			*wbuf++ = cpu_to_le16(readw(ivpnss_16bit + port));
		else if (sz == 2)
			*lbuf++ = cpu_to_le32(readl(ivpnss_16bit + port));
	}
	restore_flags(flags);
}

/****************************************************************************/

void ivpnss_out(void *bus, u32 val, u32 port, s32 sz)
{
	unsigned long flags;

#ifdef DEBUG
	printk("ivpnss_out(bus=%x,val=%x,port=%x,sz=%x)\n",
		(int)bus, val, port, sz);
#endif

	if (port > IVPNSS_IO_OFFSET)
		port -= IVPNSS_IO_OFFSET;

	save_flags(flags); cli();
	ivpnss_enable_io();
	if (sz == 0)
		writeb(val, ivpnss_8bit + port);
	else if (sz == 1)
		writew(val, ivpnss_16bit + port);
	else if (sz == 2)
		writel(val, ivpnss_16bit + port);
	else
		printk("%s(%d): bad size, sz=%x\n", __FILE__, __LINE__, sz);
	restore_flags(flags);
}

/****************************************************************************/

void ivpnss_outs(void *bus, u32 port, void *buf, u32 count, s32 sz)
{
	unsigned long flags;
	u8 *bbuf = (u8 *) buf;
	u16 *wbuf = (u16 *) buf;
	u32 *lbuf = (u32 *) buf;

#ifdef DEBUG
	printk("ivpnss_outs(bus=%x,port=%x,buf=%x,count=%d,sz=%x)\n",
		(int)bus, port, (int)buf, count, sz);
#endif

	if (port > IVPNSS_IO_OFFSET)
		port -= IVPNSS_IO_OFFSET;

	if ((sz != 0) && (sz != 1) && (sz != 2)) {
		printk("%s(%d): bad size, sz=%x\n", __FILE__, __LINE__, sz);
		return;
	}

	save_flags(flags); cli();
	ivpnss_enable_io();
	for (; (count); count--) {
		if (sz == 0)
			writeb(*bbuf++, ivpnss_8bit + port);
		else if (sz == 1)
			writew(cpu_to_le16(*wbuf++), ivpnss_16bit + port);
		else if (sz == 2)
			writel(cpu_to_le32(*lbuf++), ivpnss_16bit + port);
	}
	restore_flags(flags);
}

/****************************************************************************/

static void *ivpnss_ioremap(void *bus, u_long ofs, u_long sz)
{
#ifdef DEBUG
	printk("ivpnss_ioremap(bus=%x,ofs=%x,sz=%x)\n",
		(int)bus, (int)ofs, (int)sz);
#endif

	return NULL;
}

/****************************************************************************/

static void ivpnss_iounmap(void *bus, void *addr)
{
#ifdef DEBUG
	printk("ivpnss_iounmap(bus=%x,addr=%x)\n", (int)bus, (int)addr);
#endif
}

/****************************************************************************/

static u32 ivpnss_readmem(void *bus, void *addr, s32 sz)
{
	unsigned long flags;
	u32 val = 0;
	u32 ofs = (u32) addr;

	save_flags(flags); cli();
	ivpnss_enable_mode(ivpnss_mem_mode);
	if (sz == 0)
		val = readb(ivpnss_8bit + ofs);
	else if (sz == 1)
		val = readw(ivpnss_16bit + ofs);
	else if (sz == 2)
		val = readl(ivpnss_16bit + ofs);
	else
		printk("%s(%d): bad size, sz=%x\n", __FILE__, __LINE__, sz);
	restore_flags(flags);

#ifdef DEBUG
	printk("ivpnss_readmem(bus=%x,addr=%x,sz=%x)[mode=%d]=0x%04x\n",
		(int)bus, (int)addr, sz, ivpnss_mem_mode, val);
#endif

	return val;
}

/****************************************************************************/

static void ivpnss_copy_from(void *bus, void *d, void *s, u32 count)
{
	unsigned long flags;
	u8 *dbuf = d;
	u32 ofs = (u32) s;

#ifdef DEBUG
	printk("ivpnss_copy_from(bus=%x,d=%x,s=%x,count=%d)\n",
		(int)bus, (int)d, (int)s, count);
#endif

	save_flags(flags); cli();
	ivpnss_enable_mode(ivpnss_mem_mode);
	for (; (count); count--)
		*dbuf++ = readb(ivpnss_8bit + ofs++);
	restore_flags(flags);
}

/****************************************************************************/

static void ivpnss_writemem(void *bus, u32 val, void *addr, s32 sz)
{
	unsigned long flags;
	u32 ofs = (u32) addr;

#ifdef DEBUG
	printk("ivpnss_writemem(bus=%x,val=%x,addr=%x,sz=%x) [mode=%d]\n",
		(int)bus, val, (int)addr, sz, ivpnss_mem_mode);
#endif

	save_flags(flags); cli();
	ivpnss_enable_mode(ivpnss_mem_mode);
	if (sz == 0)
		writeb(val, ivpnss_8bit + ofs);
	else if (sz == 1)
		writew(val, ivpnss_16bit + ofs);
	else if (sz == 2)
		writel(val, ivpnss_16bit + ofs);
	else
		printk("%s(%d): bad size, sz=%x\n", __FILE__, __LINE__, sz);
	restore_flags(flags);
}

/****************************************************************************/

static void ivpnss_copy_to(void *bus, void *d, void *s, u32 count)
{
	unsigned long flags;
	u8 *sbuf = s;
	u32 ofs = (u32) d;

#ifdef DEBUG
	printk("ivpnss_copy_from(bus=%x,d=%x,s=%x,count=%d)\n",
		(int)bus, (int)d, (int)s, count);
#endif

	save_flags(flags); cli();
	ivpnss_enable_mode(ivpnss_mem_mode);
	for (; (count); count--)
		writeb(*sbuf++, ivpnss_8bit + ofs++);
	restore_flags(flags);
}

/****************************************************************************/

/*
 *	Wee need to vector through our local handler first, so that
 *	we can clear the interrupt at the IXP425 GPIO line.
 */
static void (*ivpnss_handler)(int, void *, struct pt_regs *);

static void ivpnss_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	gpio_line_isr_clear(IXP425_GPIO_PIN_7);

	if (ivpnss_handler)
		ivpnss_handler(irq, dev_id, regs);
}

static int ivpnss_request_irq(void *bus, u_int irq, void (*handler)(int, void *, struct pt_regs *), u_long flags, const char *device, void *dev_id)
{
#ifdef DEBUG
	printk("ivpnss_request_irq(bus=%x,irq=%d,handler=%x,flags=%x,"
		"device=%x,dev_id=%x)\n", (int)bus, irq, (int)handler,
		(int)flags, (int)device, (int)dev_id);
#endif

	ivpnss_handler = handler;
	if (request_irq(irq, ivpnss_isr, flags, device, dev_id)) {
		printk("IVPNSS: failed to get interrupt %d?\n", irq);
		return -EBUSY;
	}
	return 0;
}

/****************************************************************************/

static void ivpnss_free_irq(void *bus, u_int irq, void *dev_id)
{
#ifdef DEBUG
	printk("ivpnss_free_irq(bus=%x,irq=%d,dev_id=%x)\n",
		(int)bus, irq, (int)dev_id);
#endif
	ivpnss_handler = NULL;
	free_irq(irq, dev_id);
}

/****************************************************************************/

/*
 *	When using other drivers as modules we need to export these
 *	IO access functions.
 */
EXPORT_SYMBOL(ivpnss_out);
EXPORT_SYMBOL(ivpnss_in);
EXPORT_SYMBOL(ivpnss_outs);
EXPORT_SYMBOL(ivpnss_ins);

/****************************************************************************/

static struct bus_operations ivpnss_busops = {
	.b_in = ivpnss_in,
	.b_ins = ivpnss_ins,
	.b_out = ivpnss_out,
	.b_outs = ivpnss_outs,
	.b_ioremap = ivpnss_ioremap,
	.b_iounmap = ivpnss_iounmap,
	.b_read = ivpnss_readmem,
	.b_write = ivpnss_writemem,
	.b_copy_from = ivpnss_copy_from,
	.b_copy_to = ivpnss_copy_to,
	.b_request_irq = ivpnss_request_irq,
	.b_free_irq = ivpnss_free_irq,
};

/****************************************************************************/

static socket_cap_t ivpnss_socket_cap = {
	.features = SS_CAP_PCCARD | SS_CAP_STATIC_MAP | SS_CAP_VIRTUAL_BUS,
	.irq_mask = 0xffff,
	.pci_irq = 24,
	.map_size = 0x1000,
	.io_offset = IVPNSS_IO_OFFSET,
	.bus = &ivpnss_busops,
};

/****************************************************************************/

static int ivpnss_ssinit(unsigned int sock)
{
#ifdef DEBUG
	printk("ivpnss_ssinit(sock=%d)\n", sock);
#endif
	return 0;
}

/****************************************************************************/

static int ivpnss_suspend(unsigned int sock)
{
#ifdef DEBUG
	printk("ivpnss_suspend(sock=%d)\n", sock);
#endif
	return 0;
}

/****************************************************************************/

static int ivpnss_register_callback(unsigned int sock, void (*handler)(void *, unsigned int), void *info)
{
#ifdef DEBUG
	printk("ivpnss_callback(sock=%d,handler=%x,info=%x)\n",
		sock, (int)handler, (int)info);
#endif
	return 0;
}

/****************************************************************************/

static int ivpnss_inquire_socket(unsigned int sock, socket_cap_t *cap)
{
#ifdef DEBUG
	printk("ivpnss_inquire_socket(sock=%d,cap=%x)\n", sock, (int)cap);
#endif
	*cap = ivpnss_socket_cap;
	return 0;
}

/****************************************************************************/

static int ivpnss_get_status(unsigned int sock, unsigned int *value)
{
#ifdef DEBUG
	printk("ivpnss_get_status(sock=%d,value=%x)\n", sock, (int)value);
#endif
	*value = SS_DETECT | SS_READY | SS_3VCARD;
	return 0;
}

/****************************************************************************/

static int ivpnss_get_socket(unsigned int sock, socket_state_t *state)
{
#ifdef DEBUG
	printk("ivpnss_get_socket(sock=%d,state=%x)\n", sock, (int)state);
#endif
	return 0;
}

/****************************************************************************/

static int ivpnss_set_socket(unsigned int sock, socket_state_t *state)
{
#ifdef DEBUG
	printk("ivpnss_set_socket(sock=%d,state=%x)\n", sock, (int)state);
#endif
	return 0;
}

/****************************************************************************/

static int ivpnss_get_io_map(unsigned int sock, struct pccard_io_map *io)
{
#ifdef DEBUG
	printk("ivpnss_get_io_map(sock=%d,io=%x)\n", sock, (int)io);
#endif
	return 0;
}

/****************************************************************************/

static int ivpnss_set_io_map(unsigned int sock, struct pccard_io_map *io)
{
#ifdef DEBUG
	printk("ivpnss_set_io_map(sock=%d,io=%x)\n", sock, (int)io);
#endif
	return 0;
}

/****************************************************************************/

static int ivpnss_get_mem_map(unsigned int sock, struct pccard_mem_map *mem)
{
#ifdef DEBUG
	printk("ivpnss_get_mem_map(sock=%d,mem=%x)\n", sock, (int)mem);
#endif
	return 0;
}

/****************************************************************************/

static int ivpnss_set_mem_map(unsigned int sock, struct pccard_mem_map *mem)
{
#ifdef DEBUG
	printk("ivpnss_set_mem_map(sock=%d,mem=%x)\n", sock, (int)mem);
#endif

	ivpnss_mem_mode = (mem->flags & MAP_ATTRIB) ? MODE_ATTRIBUTE : MODE_MEM;
	return 0;
}

/****************************************************************************/

static void ivpnss_proc_setup(unsigned int sock, struct proc_dir_entry *base)
{
#ifdef DEBUG
	printk("ivpnss_proc_setup(sock=%d,base=%x)\n", sock, (int)base);
#endif
}

/****************************************************************************/

static struct pccard_operations ivpnss_operations = {
	.init = ivpnss_ssinit,
	.suspend = ivpnss_suspend,
	.register_callback = ivpnss_register_callback,
	.inquire_socket = ivpnss_inquire_socket,
	.get_status = ivpnss_get_status,
	.get_socket = ivpnss_get_socket,
	.set_socket = ivpnss_set_socket,
	.get_io_map = ivpnss_get_io_map,
	.set_io_map = ivpnss_set_io_map,
	.get_mem_map = ivpnss_get_mem_map,
	.set_mem_map = ivpnss_set_mem_map,
	.proc_setup = ivpnss_proc_setup,
};

/****************************************************************************/

/*
 *	This driver also has a character device interface. Very useful
 *	for debugging, but we really need it to implement the ioctl
 *	support for enabling and disabling ethernet and Wifi hardware
 *	ports.
 */
#define	IVPNSS_MAJOR	120

/****************************************************************************/

static ssize_t ivpnss_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
	unsigned short *wbuf = (unsigned short *) buf;
	unsigned int addr, mindev;
	int i, end, width;

#ifdef DEBUG
	printk("ivpnss_read(filp=%x,buf=%x,count=%d,ppos=%x)\n",
		(int)filp, (int)buf, count, (int)ppos);
#endif

	mindev = MINOR(filp->f_dentry->d_inode->i_rdev);

	switch (mindev & 0x6) {
	case 0x2: ivpnss_enable_mem(); break;
	case 0x4: ivpnss_enable_attribute(); break;
	default:  ivpnss_enable_io(); break;
	}

	width = (mindev & 1) + 1;
	addr = (unsigned int) ((width == 1) ? ivpnss_8bit : ivpnss_16bit);

	if ((i = *ppos) >= IVPNSS_MEM_SIZE)
		return 0;
	if ((i + count) > IVPNSS_MEM_SIZE)
		count = IVPNSS_MEM_SIZE - i;
	for (end = i + count; (i < end); i += width) {
		if (width == 1)
			put_user(readb(addr+i), buf++);
		else
			put_user(readw(addr+i), wbuf++);
	}

	*ppos = i;
	return count;
}

/****************************************************************************/

static ssize_t ivpnss_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	unsigned short *wbuf = (unsigned short *) buf;
	unsigned int addr, mindev;
	unsigned short wval;
	unsigned char val = 0;
	int i, end, width;

#ifdef DEBUG
	printk("ivpnss_write(filp=%x,buf=%x,count=%d,ppos=%x)\n",
		(int)filp, (int)buf, count, (int)ppos);
#endif

	mindev = MINOR(filp->f_dentry->d_inode->i_rdev);

	switch (mindev & 0x6) {
	case 0x2: ivpnss_enable_mem(); break;
	case 0x4: ivpnss_enable_attribute(); break;
	default:  ivpnss_enable_io(); break;
	}

	width = (mindev & 1) + 1;
	addr = (unsigned int) ((width == 1) ? ivpnss_8bit : ivpnss_16bit);

	if ((i = *ppos) >= IVPNSS_MEM_SIZE)
		return 0;
	if ((i + count) > IVPNSS_MEM_SIZE)
		count = IVPNSS_MEM_SIZE - i;
	for (end = i + count; (i < end); i += width) {
		if (width == 1) {
			get_user(val, buf++);
			writeb(val, addr+i);
		} else {
			get_user(wval, wbuf++);
			writew(wval, addr+i);
		}
	}

	*ppos = i;
	return count;
}

/****************************************************************************/

static loff_t ivpnss_lseek(struct file *file, loff_t offset, int orig)
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

static int ivpnss_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
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
}

/****************************************************************************/

static struct file_operations ivpnss_fops = {
	.read = ivpnss_read,
	.write = ivpnss_write,
	.llseek = ivpnss_lseek,
	.ioctl = ivpnss_ioctl,
};

/****************************************************************************/

int ivpnss_memmap(void)
{
	/* Check if already initialized */
	if (ivpnss_map8bit)
		return 0;

	ivpnss_map8bit = ioremap(IXP425_EXP_BUS_CS1_BASE_PHYS, IVPNSS_MEM_SIZE);
	if (ivpnss_map8bit == NULL) {
		printk("iVPNSS: failed to ioremap 8bit memory region?\n");
		return -ENOMEM;
	}

	ivpnss_map16bit = ioremap(IXP425_EXP_BUS_CS2_BASE_PHYS, IVPNSS_MEM_SIZE);
	if (ivpnss_map16bit == NULL) {
		iounmap(ivpnss_map8bit);
		ivpnss_map8bit = NULL;
		printk("iVPNSS: failed to ioremap 16bit memory region?\n");
		return -ENOMEM;
	}

	ivpnss_8bit = (unsigned int) ivpnss_map8bit;
	ivpnss_16bit = (unsigned int) ivpnss_map16bit;
	return 0;
}

/****************************************************************************/

static int __init ivpnss_init(void)
{
	servinfo_t serv;
	int rc;

	printk("iVPNSS: IXP4xx/iVPN socket service driver\n" \
		"(C) Copyright 2004, Greg Ungerer <gerg@snapgear.com>\n");

	ivpnss_hwsetup();
	if ((rc = ivpnss_memmap()))
		return rc;

	pcmcia_get_card_services_info(&serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk("iVPNSS: Card Services version mis-match!\n");
		return -ENODEV;
	}

	if (register_ss_entry(1, &ivpnss_operations)) {
		printk("iVPNSS: failed to register socket service?\n");
		return -ENODEV;
	}

	if (register_chrdev(IVPNSS_MAJOR, IVPNSS_NAME, &ivpnss_fops))
		printk("iVPNSS: failed to register debug char device?\n");

	return 0;
}

/****************************************************************************/

static void __exit ivpnss_exit(void)
{
	unregister_ss_entry(&ivpnss_operations);

	if (ivpnss_map8bit)
		iounmap(ivpnss_map8bit);
	if (ivpnss_map16bit)
		iounmap(ivpnss_map16bit);

	unregister_chrdev(IVPNSS_MAJOR, IVPNSS_NAME);
}

/****************************************************************************/

module_init(ivpnss_init);
module_exit(ivpnss_exit);

/****************************************************************************/
