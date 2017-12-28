/*
 * Simple character interface to GPIOs.  Allows you to read/write values and
 * control the direction.  Maybe add wakeup when gpio framework supports it ...
 *
 * based on the work of "Mike Frysinger <vapier@gentoo.org>"
 * 
 * Adapted to drive the STM32 MCU, GPIO Port F PINs
 *
 * Copyright 2008 Analog Devices Inc.
 * Licensed under the GPL-2 or later.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <asm/atomic.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>

#define GPIO_DBG(fmt, args...) pr_debug("%s:%i: " fmt "\n", __func__, __LINE__, ## args)
#define pr_devinit(fmt, args...) ({ static const __devinitdata char __fmt[] = fmt; printk(__fmt, ## args); })
#define pr_init(fmt, args...) ({ static const __initdata char __fmt[] = fmt; printk(__fmt, ## args); })

#define DRIVER_NAME "simple-gpio"
#define PFX DRIVER_NAME ": "

struct gpio_data {
	atomic_t open_count;
};
struct group_data {
	dev_t dev_node;
	struct cdev cdev;
	struct resource *gpio_range;
	struct gpio_data *gpios;
};

/**
 *	simple_gpio_read - sample the value of the specified GPIO
 */
static ssize_t simple_gpio_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	unsigned int gpio = iminor(file->f_path.dentry->d_inode);
	ssize_t ret;

	for (ret = 0; ret < count; ++ret) {
		char byte = '0' + gpio_get_value(gpio);
		if (put_user(byte, buf + ret))
			break;
	}

	return ret;
}

/**
 *	simple_gpio_write - modify the state of the specified GPIO
 *
 *	We allow people to control the direction and value of the specified Port F GPIO Pins.
 *	You can only change these once per write though.  We process the user buf
 *	and only do the last thing specified.  We also ignore newlines.  This way
 *	you can quickly test by doing from the shell:
 *		$ echo 'O1' > /dev/gpioF8
 *	This will set GPIOF8 to ouput mode and set the value to 1.
 */
static ssize_t 
simple_gpio_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	unsigned int gpio = iminor(file->f_path.dentry->d_inode);
	ssize_t ret;
	char dir = '?', uvalue = '?';
	int value;

	ret = 0;
	while (ret < count) {
		char byte;
		int user_ret = get_user(byte, buf + ret++);
		if (user_ret)
			return user_ret;

		switch (byte) {
		case '\r':
		case '\n': continue;
		case 'I':
		case 'O': dir = byte; break;
		case 'T':
		case '0':
		case '1': uvalue = byte; break;
		default:  return -EINVAL;
		}
		GPIO_DBG("processed byte '%c'", byte);
	}

	switch (uvalue) {
	case '?': value = gpio_get_value(gpio); break;
	case 'T': value = !gpio_get_output_value(gpio); break;
	default:  value = uvalue - '0'; break;
	}

	switch (dir) {
	case 'I': gpio_direction_input(gpio,value); break;
	case 'O': gpio_direction_output(gpio, value); break;
	}

	if (uvalue != '?')
		gpio_set_value(gpio, value);

	return ret;
}

/**
 *	simple_gpio_open - claim the specified GPIO
 *
 *	Grab the specified GPIO if it's available and keep track of how many times
 *	we've been opened (see close() below).  We allow multiple people to open
 *	at the same time as there's no real limitation in the hardware for reading
 *	from different processes.  Plus this way you can have one app do the write
 *	and management while quickly monitoring from another by doing:
 *		$ cat /dev/gpioF8
 */
static int simple_gpio_open(struct inode *ino, struct file *file)
{
	struct group_data *group_data = container_of(ino->i_cdev, struct group_data, cdev);
	unsigned int gpio = iminor(ino);
	struct gpio_data *gpio_data = &group_data->gpios[gpio - group_data->gpio_range->start];
	int ret;

	if (gpio < group_data->gpio_range->start || gpio > group_data->gpio_range->end)
		return -ENXIO;

	ret = gpio_request(gpio, DRIVER_NAME);
	if (ret)
		return ret;

	atomic_inc(&gpio_data->open_count);

	return 0;
}

/**
 *	simple_gpio_close - release the specified GPIO
 *
 *	Do not actually free the specified GPIO until the last person has closed.
 *	We claim/release here rather than during probe() so that people can swap
 *	between drivers on the fly during runtime without having to load/unload
 *	kernel modules.
 */
static int simple_gpio_release(struct inode *ino, struct file *file)
{
	struct group_data *group_data = container_of(ino->i_cdev, struct group_data, cdev);
	unsigned int gpio = iminor(ino);
	struct gpio_data *gpio_data = &group_data->gpios[gpio - group_data->gpio_range->start];

	/* do not free until last consumer has closed */
	if (atomic_dec_and_test(&gpio_data->open_count))
		gpio_free(gpio);
	else
		GPIO_DBG("gpio still in use -- not freeing");

	return 0;
}

static int simple_gpio_ioctl(struct inode * ino, struct file * file, unsigned int a, unsigned long b)
{
	return 0;
}

static struct class *simple_gpio_class;

static struct file_operations simple_gpio_fops = {
	.owner    = THIS_MODULE,
	.read     = simple_gpio_read,
	.write    = simple_gpio_write,
	.open     = simple_gpio_open,
	.release  = simple_gpio_release,
	.ioctl	  = simple_gpio_ioctl,
};

/**
 *	simple_gpio_probe - setup the range of GPIOs
 *
 *	Create a character device for the range of GPIOs and have the minor be
 *	used to specify the GPIO.
 */
static int __devinit simple_gpio_probe(struct platform_device *pdev)
{
	int ret;
	struct group_data *group_data;
	struct resource *gpio_range = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	int gpio, gpio_max = gpio_range->end - gpio_range->start + 1;

	group_data = kzalloc(sizeof(*group_data) + sizeof(struct gpio_data) * gpio_max, GFP_KERNEL);
	if (!group_data)
		return -ENOMEM;
	group_data->gpio_range = gpio_range;
	group_data->gpios = (void *)group_data + sizeof(*group_data);
	platform_set_drvdata(pdev, group_data);

	ret = alloc_chrdev_region(&group_data->dev_node, gpio_range->start, gpio_max, "gpio");
	if (ret) {
		pr_devinit(KERN_ERR PFX "unable to get a char device\n");
		kfree(group_data);
		return ret;
	}

	cdev_init(&group_data->cdev, &simple_gpio_fops);
	group_data->cdev.owner = THIS_MODULE;

	ret = cdev_add(&group_data->cdev, group_data->dev_node, gpio_max);
	if (ret) {
		pr_devinit(KERN_ERR PFX "unable to register char device\n");
		kfree(group_data);
		unregister_chrdev_region(group_data->dev_node, gpio_max);
		return ret;
	}

	for (gpio = gpio_range->start; gpio <= gpio_range->end; ++gpio)
		device_create(simple_gpio_class, &pdev->dev, group_data->dev_node + gpio,
		              NULL, "gpio%i", (int)gpio);

	device_init_wakeup(&pdev->dev, 1);

	pr_devinit(KERN_INFO PFX "now handling %i GPIOs: %i - %i\n",
		gpio_max, gpio_range->start, gpio_range->end);

	return 0;
}

/**
 *	simple_gpio_remove - break down the range of GPIOs
 *
 *	Release the character device and related pieces for this range of GPIOs.
 */
static int __devexit simple_gpio_remove(struct platform_device *pdev)
{
	struct group_data *group_data = platform_get_drvdata(pdev);
	struct resource *gpio_range = group_data->gpio_range;
	int gpio, gpio_max = gpio_range->end - gpio_range->start + 1;

	for (gpio = gpio_range->start; gpio <= gpio_range->end; ++gpio)
		device_destroy(simple_gpio_class, group_data->dev_node + gpio);

	cdev_del(&group_data->cdev);
	unregister_chrdev_region(group_data->dev_node, gpio_max);

	kfree(group_data);

	return 0;
}

struct platform_driver simple_gpio_device_driver = {
	.probe   = simple_gpio_probe,
	.remove  = __devexit_p(simple_gpio_remove),
	.driver  = {
		.name = DRIVER_NAME,
	}
};

/**
 *	simple_gpio_init - setup our GPIO device driver
 *
 *	Create one GPIO class for the entire driver
 */
static int __init simple_gpio_init(void)
{
	simple_gpio_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(simple_gpio_class)) {
		pr_init(KERN_ERR PFX "unable to create gpio class\n");
		return PTR_ERR(simple_gpio_class);
	}

	return platform_driver_register(&simple_gpio_device_driver);
}
module_init(simple_gpio_init);

/**
 *	simple_gpio_exit - break down our GPIO device driver
 */
static void __exit simple_gpio_exit(void)
{
	class_destroy(simple_gpio_class);

	platform_driver_unregister(&simple_gpio_device_driver);
}
module_exit(simple_gpio_exit);

//MODULE_AUTHOR("MCD Application Team");
MODULE_AUTHOR("Mike Frysinger");
MODULE_DESCRIPTION("STM32 Simple-gpio driver");
MODULE_LICENSE("GPL");

