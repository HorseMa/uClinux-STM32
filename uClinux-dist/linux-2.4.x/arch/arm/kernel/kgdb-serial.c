/*
 * arch/arm/kernel/kgdb-serial.c
 *
 * Generic serial access routines for use by kgdb.
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 * Based on original KGDB code developed by various people:
 *
 * This is a simple middle layer that allows KGDB to run on
 * top of any serial device.  This layer expects the board
 * port to provide the following functions:
 *
 * 	void kgdb_serial_init(void)
 * 		initialize serial interface if needed
 * 	void kgdb_serial_putchar(unsigned char)
 * 		send a character over the serial connection
 * 	kgdb_serial_getchar()
 * 		get a character from the serial connection
 *
 * Note that this is not meant for debugging over sercons, but for
 * when you have a _dedicated_ serial port for kgdb.  To send debug
 * and console messages over the same port (ICK), you need to
 * turn on CONFIG_KGDB_CONSOLE, which uses kgb-console.c instead and
 * hooks into the console system.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/elf.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include <asm/kgdb.h>

#ifdef CONFIG_KGDB_CONSOLE
#include <linux/console.h>
#endif

static int kgdb_io_initialized = 0;

static const char hexchars[]="0123456789abcdef";

static int
hex(unsigned char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}

/*
 *
 * Scan the serial stream for the sequence $<data>#<checksum>
 *
 */
void
kgdb_get_packet(unsigned char *buffer, int buflen)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	char ch;

	do {
		/* Ignore everything until the start character */
		while ((ch = kgdb_serial_getchar()) != '$') ;

		checksum = 0;
		xmitcsum = -1;
		count = 0;

		/* Now, read until a # or end of buffer is found */
		while (count < (buflen - 1)) {
			ch = kgdb_serial_getchar();

			if (ch == '#')
				break;

			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}

		buffer[count] = 0;

		/* Continue to read checksum following # */
		if (ch == '#') {
			xmitcsum = hex(kgdb_serial_getchar()) << 4;
			xmitcsum += hex(kgdb_serial_getchar());

			/* Checksum */
			if (checksum != xmitcsum)
				kgdb_serial_putchar('-'); /* Failed checksum */
			else {
				/* Ack successful transfer */
				kgdb_serial_putchar('+');

				/* If a sequence char is present, reply
				   the sequence ID */
				if (buffer[2] == ':') {
					kgdb_serial_putchar(buffer[0]);
					kgdb_serial_putchar(buffer[1]);

					/* Remove sequence chars from buffer */
					count = strlen(buffer);
					for (i = 3; i <= count; i++)
						buffer[i - 3] = buffer[i];
				}
			}
		}
	} while (checksum != xmitcsum);	/* Keep trying while we fail */
}


/*
 *
 * Send packet in the buffer with run-length encoding as follows:
 *
 * $<packet info>#<checksum>.
 *
 */
void
kgdb_put_packet(const unsigned char *buffer)
{
	unsigned char checksum;
	const unsigned char *src;
	int runlen;
	int encode;

	do {
		src = buffer;
		kgdb_serial_putchar('$');
		checksum = 0;

		/* Continue while we still have chars left */
		while (*src) {
			/* Check for runs up to 99 chars long */
			for (runlen = 1; runlen < 99; runlen++) {
				if (src[0] != src[runlen])
					break;
			}

			if (runlen > 3) {
				/* Got a useful amount, send encoding */
				encode = runlen + ' ' - 4;
				kgdb_serial_putchar(*src);
				checksum += *src;
				kgdb_serial_putchar('*');
				checksum += '*';
				kgdb_serial_putchar(encode);
				checksum += encode;
				src += runlen;
			} else {
				/* Otherwise just send the current char */
				kgdb_serial_putchar(*src);
				checksum += *src;
				src += 1;
			}
		}

		/* '#' Separator, put high and low components of checksum */
		kgdb_serial_putchar('#');
		kgdb_serial_putchar(hexchars[(checksum >> 4) & 0xf]);
		kgdb_serial_putchar(hexchars[checksum & 0xf]);
	} while ((kgdb_serial_getchar()) != '+'); /* While no ack */
}

int
kgdb_io_init(void)
{
	if (kgdb_io_initialized)
		return 0;

	kgdb_serial_init();

	kgdb_io_initialized = 1;

	return 0;
}


/*
 * If only one serial port is available on the board or if you want
 * to send console messages over KGDB, this is a nice and easy way
 * to accomplish this. Just add "console=ttyKGDB" to the command line
 * and all console output will be piped to the GDB client.
 */
#ifdef CONFIG_KGDB_CONSOLE

static char kgdb_console_buf[4096];

static void
kgdb_console_write(struct console *co, const char *s, unsigned count)
{
	int i = 0;
	static int j = 0;

	if(!kgdb_active())
		return;

	kgdb_console_buf[0] = 'O';
	j = 1;

	for(i = 0; i < count; i++, s++)
	{
		kgdb_console_buf[j++] = hexchars[*s >> 4];
		kgdb_console_buf[j++] = hexchars[*s & 0xf];

		if(*s == '\n')
		{
			kgdb_console_buf[j] = 0;
			kgdb_put_packet(kgdb_console_buf);

			kgdb_console_buf[0]='O';
			j = 1;
		}
	}

	if(j != 1)
	{
		kgdb_console_buf[j] = 0;
		kgdb_put_packet(kgdb_console_buf);
	}
}

static kdev_t
kgdb_console_device(struct console *c)
{
	return 0;
}

static int
kgdb_console_setup(struct console *co, char *options)
{
	kgdb_io_init();

	return 0;
}

static struct console kgdb_console = {
	name:		"ttyKGDB",
	write:		kgdb_console_write,
	device:		kgdb_console_device,
	setup:		kgdb_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1
};

void __init
kgdb_console_init(void)
{
	register_console(&kgdb_console);
}

#endif // CONFIG_KGDB_CONSOLE
