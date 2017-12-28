/* include/asm-armnommu/arch-okim67x/gpio.h
 *
 * (c) 2005 Simtec Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 * 
*/

#define OKI_CACHE_CON	(0x78200004)
#define OKI_CACHE_EN	(0x78200008)
#define OKI_CACHE_FLUSH	(0x7820001C)

#define CACHE_FLUSH_FLUSH	(1<<0)

#define CACHE_CON_LCK_NONE	(0x00 << 25)
#define CACHE_CON_LCK_WAY0	(0x01 << 25)
#define CACHE_CON_LCK_WAY01	(0x02 << 25)
#define CACHE_CON_LCK_WAY012	(0x03 << 25)
#define CACHE_CON_LCK_MASK	(0x03 << 25)

#define CACHE_CON_LOAD		(1<<27)
#define CACHE_CON_BNK0		(0x00 << 28)
#define CACHE_CON_BNK1		(0x01 << 28)
#define CACHE_CON_BNK2		(0x02 << 28)
#define CACHE_CON_BNK3		(0x03 << 28)

#define CACHE_EN_C8		(1<<0)
#define CACHE_EN_C9		(1<<1)
#define CACHE_EN_C10		(1<<2)
#define CACHE_EN_C11		(1<<3)
#define CACHE_EN_C12		(1<<4)
#define CACHE_EN_C13		(1<<5)
#define CACHE_EN_C14		(1<<6)
#define CACHE_EN_C15		(1<<7)
#define CACHE_EN_C24		(1<<8)
#define CACHE_EN_C25		(1<<0)
#define CACHE_EN_C26		(1<<10)
#define CACHE_EN_C27		(1<<11)
#define CACHE_EN_C28		(1<<12)
#define CACHE_EN_C29		(1<<13)
#define CACHE_EN_C30		(1<<14)
#define CACHE_EN_C31		(1<<15)
#define CACHE_EN_C0		(1<<16)
