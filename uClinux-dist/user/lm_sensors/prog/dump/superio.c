/*
    superio: Handle special I/O operations needed by most Super-I/O chips
   
    Copyright (C) 2005  Jean Delvare <khali@linux-fr.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdlib.h>

#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ >= 0
#include <sys/io.h>
#else
#include <asm/io.h>
#endif

#include "superio.h"

int superio_parse_key(unsigned char *key, const char *s)
{
	char *end;
	int tmp;
	key[0] = 0;
	
	while (*s != '\0') {
		tmp = strtol(s, &end, 0);
		if ((*end != '\0' && *end != ',')
		 || (tmp < 0x00 || tmp > 0xff))
			return -1;

		/* Byte is valid, store it */
		key[++key[0]] = tmp;

		/* Last byte? */
		if (key[0] == SUPERIO_MAX_KEY
		 || *end == '\0')
			return 0;

		/* Skip comma */
		s = end + 1;
	}
	
	/* Unexpected end of string */
	return -1;
}

void superio_write_key(int addrreg, unsigned char *key)
{
	int i;

	for (i = 1; i <= key[0]; i++)
		outb(key[i], addrreg);
}

void superio_reset(int addrreg, int datareg)
{
	outb(0x02, addrreg);
	outb(0x02, datareg);
}
