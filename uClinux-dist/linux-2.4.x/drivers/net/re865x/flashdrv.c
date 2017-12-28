/*
* ----------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/Attic/flashdrv.c,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $
*
* Abstract: Flash driver source code.
*
* $Author: davidm $
*
* $Log: flashdrv.c,v $
* Revision 1.1.2.1  2007/09/28 14:42:22  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.10  2006/11/22 15:52:57  shliu
* *: modify tick_pollStart() & tick_pollGet10MS() in user space.
*
* Revision 1.9  2006/11/22 10:25:15  ghhuang
* *: Fix Top/Bottom Sector Detection for CFI
*
* Revision 1.8  2006/10/10 03:23:31  alva_zhang
* +: during flash driver initialing in bootloader, record flash size in bdinfo
*
* Revision 1.7  2006/09/26 12:01:09  fae
* +: support Spansion S29GL064A90TFIR3
*
* Revision 1.6  2006/09/26 10:39:36  yjlou
* *: fixed compile error when _DEBUG_ is defined.
*
* Revision 1.5  2006/08/24 16:33:56  chenyl
* *: Unlock mutex even "flashdrv_isLoaderOverwritten" check FAILED.
*
* Revision 1.4  2006/08/03 13:22:55  fae
* *: fixed the bug when injected JUMP instruction in custom FLASH MAP mode.
*
* Revision 1.3  2006/06/19 11:36:09  ghhuang
* *: Fix compilation warning during kernel compilation for NOR CFI
*
* Revision 1.2  2006/06/15 14:44:07  ghhuang
* *: Add NOR CFI Support
*
* Revision 1.1  2006/05/19 05:59:52  chenyl
* *: modified new SDK framework.
*
* Revision 1.9  2006/02/16 03:18:57  ghhuang
* *: Fix SetROMSize() for RTL865XC
*
* Revision 1.8  2005/12/02 11:00:36  yjlou
* *: fixed the bug of re-entry flashdrv:
*    Since we used mutex lock thru ioctl(), it does not work.
*    Because when ioctl() returns to user-space, the 'IEc' flag will be recovered to 1 due to 'rfe' instruction.
*    Therefore, we must turn off GIRM to ensure no interrupt will be fired.
*    And also, we will kick watchdog when erasing and programming flash.
*
* Revision 1.7  2005/11/24 05:46:14  alva_zhang
* split flashdrv.c into flashdrv.c/flashmtd.c, and which one will be compiled is controlled by CONFIG_MTD in .../rtl865x/Makefile
*
* Revision 1.6  2005/11/21 12:40:55  chenyl
* *: always turn OFF interrupts when R/W flash
*
* Revision 1.5  2005/11/18 06:49:29  alva_zhang
* +: add several flash driver functions based on MTD, so that boa can work with MTD
*
* Revision 1.4  2005/10/18 15:04:03  chenyl
* +: add mutex-lock-protection
*
* Revision 1.3  2005/09/27 08:13:20  yjlou
* *: little fix for spansion (not verified).
*
* Revision 1.2  2005/04/25 03:07:31  yjlou
* *: sync with goahead's flashdrv.
* 	* Revision 1.58  2005/04/24 03:53:32  yjlou
* 	* +: add flashdrv_ignore_largeflash to fix loader raw write bug.
* 	* +: support Spansion model ID.
* 	* Revision 1.57  2005/04/12 12:34:59  yjlou
* 	* +: read bdinfo in flashdrv_init().
* 	* +: support S29GL128M R1,R2,R8,R9
* 	*            S29GL064M90TAIR4
* 	*            S29GL064M90TAIR3
*
* Revision 1.1  2005/04/19 04:58:15  tony
* +: BOA web server initial version.
*
* This file is moved from user/goahead-2.1.4/LINUX/flashdrv.c.
* If you want to know the detailed log, please refer to that file.
*
* ---------------------------------------------------------------
*/


#undef _DEBUG_

#include "flashdrv.h"

/* Command Definitions 
*/
#define AM29LVXXX_COMMAND_ADDR1         ((volatile uint16 *) (FLASH_BASE + 0x555 * 2))
#define AM29LVXXX_COMMAND_ADDR2         ((volatile uint16 *) (FLASH_BASE + 0x2AA * 2))
#define AM29LVXXX_COMMAND1              0xAA
#define AM29LVXXX_COMMAND2              0x55
#define AM29LVXXX_AUTOSELECT_CMD        0x90
#define AM29LVXXX_SECTOR_ERASE_CMD1     0x80
#define AM29LVXXX_SECTOR_ERASE_CMD2     0x30
#define AM29LVXXX_PROGRAM_CMD           0xA0
#define AM29LVXXX_RESET_CMD             0xF0

/* Flash Device Specific Definitions
*/
#define AM29LV800BB_DEVID   0x225B
#define AM29LV800BT_DEVID   0x22DA
#define AM29LV160BB_DEVID   0x2249
#define AM29LV160BT_DEVID   0x22C4
#define M29W320DB_DEVID     0x22CB
#define M29W320DT_DEVID     0x22CA
#define AM29LV320DB_DEVID   0x22F9
#define AM29LV320DT_DEVID   0x22F6
#define MX29LV320AB_DEVID   0x22A8
#define MX29LV320AT_DEVID   0x22A7
#define MX29LV640BB_DEVID   0x22CB
#define MX29LV640BT_DEVID   0x22C9
#define S29GLxxxM_DEVID	0x227E
#define AT49BV322A_DEVID	0X00C8
#define AT49BV322AT_DEVID	0X00C9

/* TO BE IMPLEMENTTED */
/* AMD Devices */
#define AM29F400BT      0x2223    
#define AM29F400BB      0x22AB
#define AM29LV400BT     0x22B9
#define AM29LV400BB     0x22BA
#define AM29LV040B      0x4f
/* ST Devices */
#define M29W040         0xE3
/* INTEL Devices - We have added I before the name defintion.*/
#define I28F320J3A      0x0016
#define I28F640J3A      0x0017
#define I28F128J3A      0x0018
#define I28F320B3B      0x8897
#define I28F320B3T      0x8896 
#define I28F160B3B      0x8891
#define I28F160B3T      0x8890
#define I28F160C3B      0x88C3
#define I28F160C3T      0x88C2


#define TIMEOUT_10MS    300 /*3 sec*/

/*
 *  If it is 1, flashdrv will NOT check the address that may touch the flash jump code.
 *    This is for Loader 'o' raw mode.
 *  If it is 0, flashdrv will check the overwriting of jump code.
 *    This is the default mode.
 */
uint32 flashdrv_ignore_largeflash = 0;


/* ===== Mutex lock / unlock Protection ======== */
static int32 flashdrv_mutexLock(void);
static int32 flashdrv_mutexUnlock(void);
/* ================================= */

uint32 _flashdrv_write(void *dstAddr_P, void *srcAddr_P, uint32 size);
uint32 _flashdrv_eraseBlock( uint32 ChipSeq, uint32 BlockSeq );


#ifndef _LOADER_MODE_FLASHDRV_
/* for linux kernel and goahead */
#ifdef _KERNEL_MODE_FLASHDRV_
/* linux kernel */
typedef struct {} FILE; /* kernel mode has NO FILE struct */
#define assert(a)
#else
/* goahead */
#include <stdio.h>
#include <time.h>
#define assert(a)
#define rtlglue_printf printf
#endif/* goahead */

/* #define MAX_FLASH_CHIPS 2   menuconfig will define this. */
#define UART0_ILEV      3
#define MAX_ILEV        10


/* user-space has no need to setIlev */
uint32 setIlev( int inew )
{
	return inew;
}

#ifdef _KERNEL_MODE_FLASHDRV_
/* kernel mode */
unsigned long keepTick; /* unit: 10ms */

void tick_pollStart(void)
{
	keepTick = jiffies;
}

uint32 tick_pollGet10MS(void)
{
	return jiffies - keepTick;
}
#else
/* user mode */
static uint32 cntDelay;

static void tick_pollStart(void)
{
	cntDelay = 0;
}

static uint32 tick_pollGet10MS(void)
{
	cntDelay++;
	return cntDelay/30000;
}

uint32 flashdrv_filewrite(FILE *fp,int size,void  *dstP)
{
	int i;
	int nWritten; /* completed bytes */
	volatile unsigned char data[4096];
	volatile uint16 *dstAddr;

	nWritten = 0;
	dstAddr = dstP;

	while ( nWritten < size )
	{
		int nRead;

		/* fill buffer with file */
		for( i = 0, nRead = 0; 
		     i < sizeof(data) && ( i < (size-nWritten) );
		     i++, nRead++ )
		{
			data[i] = fgetc(fp);
		}

		if ( 0 != flashdrv_updateImg( (void*)data, (void*)dstAddr, nRead ) )
			return 1;

		dstAddr += nRead >> 1;
		nWritten += nRead;
	}
	return 0;

}
#endif/*_USER_MODE_FLASHDRV_*/

#endif/*LOADER*/


/*
 *  global variables
 */
static uint32 flash_total_size = 0;

/*
 *  bdinfo instance in SDRAM.
 */
bdinfo_t	bdinfo;

#ifndef CONFIG_RTL865X_NOR_CFI
/*
 *  Support multiple flash chip
 */
static const uint32 boAm29LV800DB[] = { /* 8Mb, Bottom Sector */
	0x000000L ,0x004000L ,0x006000L ,0x008000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,
	0x050000L ,0x060000L ,0x070000L ,0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,
	0x0D0000L ,0x0E0000L ,0x0F0000L ,0x100000L };
	
static const uint32 boAm29LV800DT[] = { /* 8Mb, Top Sector */
	0x000000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,
	0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,
	0x0F8000L ,0x0FA000L ,0x0FC000L ,0x100000L };

static const uint32 boAm29LV160DB[] = { /* 16Mb, Bottom Sector */
	0x000000L ,0x004000L ,0x006000L ,0x008000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,
	0x050000L ,0x060000L ,0x070000L ,0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,
	0x0D0000L ,0x0E0000L ,0x0F0000L ,0x100000L ,0x110000L ,0x120000L ,0x130000L ,0x140000L ,
	0x150000L ,0x160000L ,0x170000L ,0x180000L ,0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,
	0x1D0000L ,0x1E0000L ,0x1F0000L ,0x200000L };
	
static const uint32 boAm29LV160DT[] = { /* 16Mb, Top Sector */
	0x000000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,
	0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,
	0x100000L ,0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,
	0x180000L ,0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,0x1D0000L ,0x1E0000L ,0x1F0000L ,
	0x1F8000L ,0x1FA000L ,0x1FC000L ,0x200000L };
	
static const uint32 boAm29LV320DB[] = { /* 32Mb, Bottom Sector */
	0x000000L ,0x002000L ,0x004000L ,0x006000L ,0x008000L ,0x00A000L ,0x00C000L ,0x00E000L ,
	0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,0x080000L ,
	0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,0x100000L ,
	0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,0x180000L ,
	0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,0x1D0000L ,0x1E0000L ,0x1F0000L ,0x200000L ,
	0x210000L ,0x220000L ,0x230000L ,0x240000L ,0x250000L ,0x260000L ,0x270000L ,0x280000L ,
	0x290000L ,0x2A0000L ,0x2B0000L ,0x2C0000L ,0x2D0000L ,0x2E0000L ,0x2F0000L ,0x300000L ,
	0x310000L ,0x320000L ,0x330000L ,0x340000L ,0x350000L ,0x360000L ,0x370000L ,0x380000L ,
	0x390000L ,0x3A0000L ,0x3B0000L ,0x3C0000L ,0x3D0000L ,0x3E0000L ,0x3F0000L ,0x400000L };
	
static const uint32 boAm29LV320DT[] = { /* 32Mb, Top Sector */
	0x000000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,
	0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,
	0x100000L ,0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,
	0x180000L ,0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,0x1D0000L ,0x1E0000L ,0x1F0000L ,
	0x200000L ,0x210000L ,0x220000L ,0x230000L ,0x240000L ,0x250000L ,0x260000L ,0x270000L ,
	0x280000L ,0x290000L ,0x2A0000L ,0x2B0000L ,0x2C0000L ,0x2D0000L ,0x2E0000L ,0x2F0000L ,
	0x300000L ,0x310000L ,0x320000L ,0x330000L ,0x340000L ,0x350000L ,0x360000L ,0x370000L ,
	0x380000L ,0x390000L ,0x3A0000L ,0x3B0000L ,0x3C0000L ,0x3D0000L ,0x3E0000L ,0x3F0000L ,
	0x3F2000L ,0x3F4000L ,0x3F6000L ,0x3F8000L ,0x3FA000L ,0x3FC000L ,0x3FE000L ,0x400000L };

static const uint32 boM29W320DB[] = { /* 32Mb, Bottom Sector */
	0x000000L ,0x004000L ,0x006000L ,0x008000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,
	0x050000L ,0x060000L ,0x070000L ,0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,
	0x0D0000L ,0x0E0000L ,0x0F0000L ,0x100000L ,0x110000L ,0x120000L ,0x130000L ,0x140000L ,
	0x150000L ,0x160000L ,0x170000L ,0x180000L ,0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,
	0x1D0000L ,0x1E0000L ,0x1F0000L ,0x200000L ,0x210000L ,0x220000L ,0x230000L ,0x240000L ,
	0x250000L ,0x260000L ,0x270000L ,0x280000L ,0x290000L ,0x2A0000L ,0x2B0000L ,0x2C0000L ,
	0x2D0000L ,0x2E0000L ,0x2F0000L ,0x300000L ,0x310000L ,0x320000L ,0x330000L ,0x340000L ,
	0x350000L ,0x360000L ,0x370000L ,0x380000L ,0x390000L ,0x3A0000L ,0x3B0000L ,0x3C0000L ,
	0x3D0000L ,0x3E0000L ,0x3F0000L ,0x400000L };
	
static const uint32 boM29W320DT[] = { /* 32Mb, Top Sector */
	0x000000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,
	0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,
	0x100000L ,0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,
	0x180000L ,0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,0x1D0000L ,0x1E0000L ,0x1F0000L ,
	0x200000L ,0x210000L ,0x220000L ,0x230000L ,0x240000L ,0x250000L ,0x260000L ,0x270000L ,
	0x280000L ,0x290000L ,0x2A0000L ,0x2B0000L ,0x2C0000L ,0x2D0000L ,0x2E0000L ,0x2F0000L ,
	0x300000L ,0x310000L ,0x320000L ,0x330000L ,0x340000L ,0x350000L ,0x360000L ,0x370000L ,
	0x380000L ,0x390000L ,0x3A0000L ,0x3B0000L ,0x3C0000L ,0x3D0000L ,0x3E0000L ,0x3F0000L ,
	0x3F8000L ,0x3FA000L ,0x3FC000L ,0x400000L };

static const uint32 boMX29LV320AB[] = { /* 32Mb, Bottom Sector: 8-8-8-8-8-8-8-8-64-64-... */
	0x000000L ,0x002000L ,0x004000L ,0x006000L ,0x008000L ,0x00A000L ,0x00C000L ,0x00E000L ,
	0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,0x080000L ,
	0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,0x100000L ,
	0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,0x180000L ,
	0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,0x1D0000L ,0x1E0000L ,0x1F0000L ,0x200000L ,
	0x210000L ,0x220000L ,0x230000L ,0x240000L ,0x250000L ,0x260000L ,0x270000L ,0x280000L ,
	0x290000L ,0x2A0000L ,0x2B0000L ,0x2C0000L ,0x2D0000L ,0x2E0000L ,0x2F0000L ,0x300000L ,
	0x310000L ,0x320000L ,0x330000L ,0x340000L ,0x350000L ,0x360000L ,0x370000L ,0x380000L ,
	0x390000L ,0x3A0000L ,0x3B0000L ,0x3C0000L ,0x3D0000L ,0x3E0000L ,0x3F0000L ,0x400000L };

static const uint32 boMX29LV640BB[] = { /* 64Mb, Bottom Sector: 8-8-8-8-8-8-8-8-64-64-... */
	0x000000L ,0x002000L ,0x004000L ,0x006000L ,0x008000L ,0x00A000L ,0x00C000L ,0x00E000L ,
	0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,0x080000L ,
	0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,0x100000L ,
	0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,0x180000L ,
	0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,0x1D0000L ,0x1E0000L ,0x1F0000L ,0x200000L ,
	0x210000L ,0x220000L ,0x230000L ,0x240000L ,0x250000L ,0x260000L ,0x270000L ,0x280000L ,
	0x290000L ,0x2A0000L ,0x2B0000L ,0x2C0000L ,0x2D0000L ,0x2E0000L ,0x2F0000L ,0x300000L ,
	0x310000L ,0x320000L ,0x330000L ,0x340000L ,0x350000L ,0x360000L ,0x370000L ,0x380000L ,
	0x390000L ,0x3A0000L ,0x3B0000L ,0x3C0000L ,0x3D0000L ,0x3E0000L ,0x3F0000L ,0x400000L ,
	0x410000L ,0x420000L ,0x430000L ,0x440000L ,0x450000L ,0x460000L ,0x470000L ,0x480000L ,
	0x490000L ,0x4A0000L ,0x4B0000L ,0x4C0000L ,0x4D0000L ,0x4E0000L ,0x4F0000L ,0x500000L ,
	0x510000L ,0x520000L ,0x530000L ,0x540000L ,0x550000L ,0x560000L ,0x570000L ,0x580000L ,
	0x590000L ,0x5A0000L ,0x5B0000L ,0x5C0000L ,0x5D0000L ,0x5E0000L ,0x5F0000L ,0x600000L ,
	0x610000L ,0x620000L ,0x630000L ,0x640000L ,0x650000L ,0x660000L ,0x670000L ,0x680000L ,
	0x690000L ,0x6A0000L ,0x6B0000L ,0x6C0000L ,0x6D0000L ,0x6E0000L ,0x6F0000L ,0x700000L ,
	0x710000L ,0x720000L ,0x730000L ,0x740000L ,0x750000L ,0x760000L ,0x770000L ,0x780000L ,
	0x790000L ,0x7A0000L ,0x7B0000L ,0x7C0000L ,0x7D0000L ,0x7E0000L ,0x7F0000L ,0x800000L };

static const uint32 boMX29LV640BT[] = { /* 64Mb, Top Sector: 64-64-...-8-8-8-8-8-8-8-8 */
	0x000000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,
	0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,
	0x100000L ,0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,
	0x180000L ,0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,0x1D0000L ,0x1E0000L ,0x1F0000L ,
	0x200000L ,0x210000L ,0x220000L ,0x230000L ,0x240000L ,0x250000L ,0x260000L ,0x270000L ,
	0x280000L ,0x290000L ,0x2A0000L ,0x2B0000L ,0x2C0000L ,0x2D0000L ,0x2E0000L ,0x2F0000L ,
	0x300000L ,0x310000L ,0x320000L ,0x330000L ,0x340000L ,0x350000L ,0x360000L ,0x370000L ,
	0x380000L ,0x390000L ,0x3A0000L ,0x3B0000L ,0x3C0000L ,0x3D0000L ,0x3E0000L ,0x3F0000L ,
	0x400000L ,0x410000L ,0x420000L ,0x430000L ,0x440000L ,0x450000L ,0x460000L ,0x470000L ,
	0x480000L ,0x490000L ,0x4A0000L ,0x4B0000L ,0x4C0000L ,0x4D0000L ,0x4E0000L ,0x4F0000L ,
	0x500000L ,0x510000L ,0x520000L ,0x530000L ,0x540000L ,0x550000L ,0x560000L ,0x570000L ,
	0x580000L ,0x590000L ,0x5A0000L ,0x5B0000L ,0x5C0000L ,0x5D0000L ,0x5E0000L ,0x5F0000L ,
	0x600000L ,0x610000L ,0x620000L ,0x630000L ,0x640000L ,0x650000L ,0x660000L ,0x670000L ,
	0x680000L ,0x690000L ,0x6A0000L ,0x6B0000L ,0x6C0000L ,0x6D0000L ,0x6E0000L ,0x6F0000L ,
	0x700000L ,0x710000L ,0x720000L ,0x730000L ,0x740000L ,0x750000L ,0x760000L ,0x770000L ,
	0x780000L ,0x790000L ,0x7A0000L ,0x7B0000L ,0x7C0000L ,0x7D0000L ,0x7E0000L ,0x7F0000L ,
	0x7F2000L ,0x7F4000L ,0x7F6000L ,0x7F8000L ,0x7FA000L ,0x7FC000L ,0x7FE000L ,0x800000L };


static const uint32 boMX29LV320AT[] = { /* 32Mb, Top Sector: 64-...-64-8-8-8-8-8-8-8-8 */
	0x000000L ,0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,
	0x080000L ,0x090000L ,0x0A0000L ,0x0B0000L ,0x0C0000L ,0x0D0000L ,0x0E0000L ,0x0F0000L ,
	0x100000L ,0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,
	0x180000L ,0x190000L ,0x1A0000L ,0x1B0000L ,0x1C0000L ,0x1D0000L ,0x1E0000L ,0x1F0000L ,
	0x200000L ,0x210000L ,0x220000L ,0x230000L ,0x240000L ,0x250000L ,0x260000L ,0x270000L ,
	0x280000L ,0x290000L ,0x2A0000L ,0x2B0000L ,0x2C0000L ,0x2D0000L ,0x2E0000L ,0x2F0000L ,
	0x300000L ,0x310000L ,0x320000L ,0x330000L ,0x340000L ,0x350000L ,0x360000L ,0x370000L ,
	0x380000L ,0x390000L ,0x3A0000L ,0x3B0000L ,0x3C0000L ,0x3D0000L ,0x3E0000L ,0x3F0000L ,
	0x3F2000L ,0x3F4000L ,0x3F6000L ,0x3F8000L ,0x3FA000L ,0x3FC000L ,0x3FE000L ,0x400000L };

static const uint32 boINTEL4M[] = { /* INTEL 32Mb */
	0x000000L ,0x020000L ,0x040000L ,0x060000L ,0x080000L ,0x0A0000L ,0x0C0000L ,0x0E0000L ,
	0x100000L ,0x120000L ,0x140000L ,0x160000L ,0x180000L ,0x1A0000L ,0x1C0000L ,0x1E0000L ,
	0x200000L ,0x220000L ,0x240000L ,0x260000L ,0x280000L ,0x2A0000L ,0x2C0000L ,0x2E0000L ,
	0x300000L ,0x320000L ,0x340000L ,0x360000L ,0x380000L ,0x3A0000L ,0x3C0000L ,0x3E0000L ,0x400000L 
	};
	
static const uint32 boINTEL8M[] = { /* INTEL 64Mb */
	0x000000L ,0x020000L ,0x040000L ,0x060000L ,0x080000L ,0x0A0000L ,0x0C0000L ,0x0E0000L ,
	0x100000L ,0x120000L ,0x140000L ,0x160000L ,0x180000L ,0x1A0000L ,0x1C0000L ,0x1E0000L ,
	0x200000L ,0x220000L ,0x240000L ,0x260000L ,0x280000L ,0x2A0000L ,0x2C0000L ,0x2E0000L ,
	0x300000L ,0x320000L ,0x340000L ,0x360000L ,0x380000L ,0x3A0000L ,0x3C0000L ,0x3E0000L ,
	0x400000L ,0x420000L ,0x440000L ,0x460000L ,0x480000L ,0x4A0000L ,0x4C0000L ,0x4E0000L ,
	0x500000L ,0x520000L ,0x540000L ,0x560000L ,0x580000L ,0x5A0000L ,0x5C0000L ,0x5E0000L ,
	0x600000L ,0x620000L ,0x640000L ,0x660000L ,0x680000L ,0x6A0000L ,0x6C0000L ,0x6E0000L ,
	0x700000L ,0x720000L ,0x740000L ,0x760000L ,0x780000L ,0x7A0000L ,0x7C0000L ,0x7E0000L ,0x800000L
	};

static const uint32 boINTEL16M[] = { /* INTEL 128Mb */
	0x000000L ,0x020000L ,0x040000L ,0x060000L ,0x080000L ,0x0A0000L ,0x0C0000L ,0x0E0000L ,
	0x100000L ,0x120000L ,0x140000L ,0x160000L ,0x180000L ,0x1A0000L ,0x1C0000L ,0x1E0000L ,
	0x200000L ,0x220000L ,0x240000L ,0x260000L ,0x280000L ,0x2A0000L ,0x2C0000L ,0x2E0000L ,
	0x300000L ,0x320000L ,0x340000L ,0x360000L ,0x380000L ,0x3A0000L ,0x3C0000L ,0x3E0000L ,
	0x400000L ,0x420000L ,0x440000L ,0x460000L ,0x480000L ,0x4A0000L ,0x4C0000L ,0x4E0000L ,
	0x500000L ,0x520000L ,0x540000L ,0x560000L ,0x580000L ,0x5A0000L ,0x5C0000L ,0x5E0000L ,
	0x600000L ,0x620000L ,0x640000L ,0x660000L ,0x680000L ,0x6A0000L ,0x6C0000L ,0x6E0000L ,
	0x700000L ,0x720000L ,0x740000L ,0x760000L ,0x780000L ,0x7A0000L ,0x7C0000L ,0x7E0000L ,
	0x800000L ,0x820000L ,0x840000L ,0x860000L ,0x880000L ,0x8A0000L ,0x8C0000L ,0x8E0000L ,
	0x900000L ,0x920000L ,0x940000L ,0x960000L ,0x980000L ,0x9A0000L ,0x9C0000L ,0x9E0000L ,
	0xA00000L ,0xA20000L ,0xA40000L ,0xA60000L ,0xA80000L ,0xAA0000L ,0xAC0000L ,0xAE0000L ,
	0xB00000L ,0xB20000L ,0xB40000L ,0xB60000L ,0xB80000L ,0xBA0000L ,0xBC0000L ,0xBE0000L ,
	0xC00000L ,0xC20000L ,0xC40000L ,0xC60000L ,0xC80000L ,0xCA0000L ,0xCC0000L ,0xCE0000L ,
	0xD00000L ,0xD20000L ,0xD40000L ,0xD60000L ,0xD80000L ,0xDA0000L ,0xDC0000L ,0xDE0000L ,
	0xE00000L ,0xE20000L ,0xE40000L ,0xE60000L ,0xE80000L ,0xEA0000L ,0xEC0000L ,0xEE0000L ,
	0xF00000L ,0xF20000L ,0xF40000L ,0xF60000L ,0xF80000L ,0xFA0000L ,0xFC0000L ,0xFE0000L ,0x1000000L
	};

static const uint32 boINTEL2MB[] = { /* INTEL 2MB bottom */
	0x000000L ,0x002000L ,0x004000L ,0x006000L ,0x008000L ,0x00A000L ,0x00C000L ,0x00E000L ,
	0x010000L ,0x020000L ,0x030000L ,0x040000L ,0x050000L ,0x060000L ,0x070000L ,0x080000L ,
	0x090000L ,0x0a0000L ,0x0b0000L ,0x0c0000L ,0x0d0000L ,0x0e0000L ,0x0f0000L ,0x100000L ,
	0x110000L ,0x120000L ,0x130000L ,0x140000L ,0x150000L ,0x160000L ,0x170000L ,0x180000L ,
	0x190000L ,0x1a0000L ,0x1b0000L ,0x1c0000L ,0x1d0000L ,0x1e0000L ,0x1f0000L ,0x200000L ,
	};
#endif /* NOR_CFI */	

struct FLASH_CHIP_INFO flash_chip_info[MAX_FLASH_CHIPS];
static uint32 flash_num_of_chips = 0; /* the number of flash chip on board */


#ifdef _SUPPORT_LARGE_FLASH_
/*
 *  flashdrv_isOver5xBBootInstructions(), flashdrv_recoverBootInstructions(),
 *  flashdrv_isInjected() and flashdrv_backupAndModifyBootCode() are used for 865xB's bug.
 *
 *  Before read/write flash memory, program should call flashdrv_isOver5xBBootInstructions() to ensure that
 *    the [startOffset,endOffset) range is not overlapped with the boot J instruction.
 *    The returned pInstructionOffset is used to pass to flashdrv_backupAndModifyBootCode().
 */
int32 flashdrv_isOver5xBBootInstructions( uint32 startOffset, uint32 endOffset, uint32 *pInstructionOffset )
{
	*pInstructionOffset = 0;

	/* Only 865XB's revB needs to check. */
	if ( !IS_865XB() )
	{
		/* Not 865xB, it is ok. */
		return FALSE;
	}
	else if ( ( REG32( CRMR_ADDR ) & 0x000f0000 ) != 0x00010000 )
	{
		/* 865XB, but not revB, it is ok. */
		return FALSE;
	}

	if ( flash_chip_info[0].ChipSize == 0x0800000 ) /* 8MB flash */
	{
		*pInstructionOffset = 0x0400000;
		if ( startOffset < 0x0400008 &&
		     endOffset > 0x0400000 )
		{
			return TRUE;
		}
	}
	else if ( flash_chip_info[0].ChipSize == 0x1000000 ) /* 16MB flash */
	{
		*pInstructionOffset = 0x0C00000;
		if ( startOffset < 0x0C00008 &&
		     endOffset > 0x0C00000 )
		{
			return TRUE;
		}
	}

	return FALSE;
}


/*
 *  flashdrv_recoverBootInstructions() is used to recover code that overwriten by JUMP instructions.
 *  Typically, this function is called by flashdrv_read().
 *
 *  Warning: User must read bdinfo structure from flash before calling this function.
 */
int32 flashdrv_recoverBootInstructions( uint32* pStartAddrToRecover )
{
	pStartAddrToRecover[0] = bdinfo.BackupInst[0];
	pStartAddrToRecover[1] = bdinfo.BackupInst[1];
#ifdef _DEBUG_
	rtlglue_printf("\n* READ Recover Instruction Mechanism Fired *\nbdinfo.BackupInst[0]=0x%08x\nbdinfo.BackupInst[1]=0x%08x\n\n",
	       bdinfo.BackupInst[0] ,bdinfo.BackupInst[1] );
#endif//_DEBUG_
	return SUCCESS;
}


/*
 *  flashdrv_backupAndModifyBootCode() is used to inject JUMP instructions.
 *  Before injection, it will backup original instructions in BDINFO (by calling flashdrv_updateImg()).
 *  pStartAddrToBackup is pointed to the address used to inject JUMP instructions.
 *  Typically, this function is called by flashdrv_updateImg().
 *
 *  Warning: User must read bdinfo structure from flash before calling this function.
 */
int32 flashdrv_backupAndModifyBootInstructions( uint32* pStartAddrToBackup )
{
	// will overwrite the 4/8MB'th byte, backup them.

	bdinfo.BackupInst[0] =  pStartAddrToBackup[0];
	bdinfo.BackupInst[1] =  pStartAddrToBackup[1];
	if( flashdrv_updateImg((void *)&bdinfo, (void*)FLASH_MAP_BOARD_INFO_ADDR, sizeof(bdinfo_t) ) )
	{
		rtlglue_printf("\nupdate bdinfo FAIL when backuping instructions!\n\n");
		return 1;
	}    

	/* modify the source code */
	pStartAddrToBackup[0] = 0x0B800080; /* j  0xbe000200 */
	pStartAddrToBackup[1] = 0x00000000; /* nop           */
	
#ifdef _DEBUG_
	rtlglue_printf("\n* WRITE Backup Instruction Mechanism Fired *\nbdinfo.BackupInst[0]=0x%08x\nbdinfo.BackupInst[1]=0x%08x\n\n",
	       bdinfo.BackupInst[0] ,bdinfo.BackupInst[1] );
#endif//_DEBUG_
	return SUCCESS;
}


/*
 *  flashdrv_isInjected() is used to check if JUMP instructions are injected.
 *  pStartAddrToCheck is pointed to the address to be check.
 *  Typically, this function is called by fileSys_updateBootImage().
 */
int32 flashdrv_isInjected( uint32* pStartAddrToCheck )
{
	if ( pStartAddrToCheck[0] == 0x0B800080 && /* j  0xbe000200 */
	     pStartAddrToCheck[1] == 0x00000000 )  /* nop           */
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


#ifdef LOADER
/*
 *  flashdrv_InjectJumpInstructions() is used to inject JUMP instructions into flash
 *    if JUMP instructions are NOT injected.
 *  Typically, this function is called by fileSys_updateBootImage().
 */
int32 flashdrv_InjectJumpInstructions()
{
	if ( flash_chip_info[0].ChipSize > 0x0400000 )
	{
		uint32 InstructionOffset;
		uint32 *pInjectAddr;

		/* if the first flash size is larger than 4MB, query InstructionOffset. */
		flashdrv_isOver5xBBootInstructions( 0, 0, &InstructionOffset );
		pInjectAddr = (uint32*) ( FLASH_BASE + InstructionOffset );

#ifdef _DEBUG_
		rtlglue_printf("check if JUMP instructions are injected? pInjectAddr=%p\n",pInjectAddr);
#endif//_DEBUG_

		/* check if injected ? */
		if ( flashdrv_isInjected( pInjectAddr ) )
		{
			/* Injected, do nothing .... */
#ifdef _DEBUG_
			rtlglue_printf("JUMP instructions are injected! Do nothing....\n");
#endif//_DEBUG_
		}
		else
		{
			/* Not injected. */
			uint32 ChipSeq, BlockSeq;
			uint32 blockSize;
			
#ifdef _DEBUG_
			rtlglue_printf("JUMP instructions are NOT injected! Injecting....\n");
#endif//_DEBUG_

			if ( searchBlockSeq( pInjectAddr, &ChipSeq, &BlockSeq ) )
			{
				rtlglue_printf( "[%s:%d] pInjectAddr is larger than total flash size: pInjectAddr=0x%08x\n", __FUNCTION__, __LINE__, (uint32)pInjectAddr );
				return 1;
			}

			blockSize = flash_chip_info[ChipSeq].BlockOffset[BlockSeq+1] - flash_chip_info[ChipSeq].BlockOffset[BlockSeq];
			memcpy( (void*)DRAM_MAP_INJECT_BUF_ADDR, pInjectAddr, blockSize );
			flashdrv_backupAndModifyBootInstructions( (uint32*)DRAM_MAP_INJECT_BUF_ADDR );

			if ( _flashdrv_eraseBlock( ChipSeq, BlockSeq ) )
			{
				rtlglue_printf( "! flashdrv_eraseBlock(Chip:%d,Block:%d) failed.\n", ChipSeq, BlockSeq );
				return 1;
			}

			return _flashdrv_write( pInjectAddr, (void*)DRAM_MAP_INJECT_BUF_ADDR, blockSize );
		}

	}

	return SUCCESS;
}
#endif/*LOADER*/

#endif/*_SUPPORT_LARGE_FLASH_*/


/*
 * We note that: __uClinux__ is defined if nommu.
 */
void SetROMSize( uint32 size )
{
#ifdef LOADER // Only available for Loader: to avoid causing bug in user MMU mode
	uint32 mcr;

#ifdef _DEBUG_
	rtlglue_printf( "SetROMSize(0x%X) ... ", size );
#endif// _DEBUG_

#ifdef CONFIG_RTL865XC
	mcr = REG32(MCR);
	mcr &= ~0xe0000000;
	switch ( size )
	{
	case 0x1000000:
	    mcr |= 0xc0000000;
	    break;
	case 0x0800000:
	    mcr |= 0xa0000000;
	    break;
	case 0x0400000:
	    mcr |= 0x80000000;
	    break;
	case 0x0200000:
	    mcr |= 0x60000000;
	    break;
	case 0x0100000:
	    mcr |= 0x40000000;
	    break;
	}
#else
	mcr = REG32(MCR);
	mcr &= ~0xc0003000;
	switch ( size )
	{
	case 0x1000000:
	    mcr |= 0xc0003000;
	    break;
	case 0x0800000:
	    mcr |= 0xc0002000;
	    break;
	case 0x0400000:
	    mcr |= 0xc0001000;
	    break;
	case 0x0200000:
	    mcr |= 0xc0000000;
	    break;
	case 0x0100000:
	    mcr |= 0x80000000;
	    break;
	}
#endif
	
#if 0
	/*
	 *  5788A(865xB revA) has a bug:
	 *    MCR:ROMSizeExt is reverted.
	 */
	if ( IS_865XB() && ( REG32(CRMR) & 0x000f0000 ) == 0x0 )
	{
		mcr ^= 0x00003000; /* NOT the 2 bits */
	}
#endif/*0*/

	REG32(MCR) = mcr;

#ifdef _DEBUG_
	rtlglue_printf( "SetROMSize(): MCR=0x%08X\n", mcr );
#endif// _DEBUG_   
   
#endif// LOADER // to avoid causing bug in MMU mode
}



/*
 *	To erase flash:
 *		ChipSeq and BlockSeq are MUSTBE.
 *		But, dstAddr is assigned NULL.
 *
 *	To program flash:
 *		ChipSeq, BlockSeq, and dstAddr are MUSTBE.
 *
 */
void flashdrv_reset( int32 ChipSeq, int32 BlockSeq, volatile uint16 *dstAddr )
{
	volatile uint16 *cmdAddr1;
	volatile uint16 *cmdAddr2;
	volatile uint16 *pDstAddr;

	cmdAddr1 = (void*) ( flash_chip_info[ChipSeq].BlockBase + ( 0x555 * 2 ) );
	cmdAddr2 = (void*) ( flash_chip_info[ChipSeq].BlockBase + ( 0x2AA * 2 ) );
	pDstAddr = (void*) ( flash_chip_info[ChipSeq].BlockBase + 
	                     flash_chip_info[ChipSeq].BlockOffset[BlockSeq] );

#ifdef CONFIG_RTL865X_NOR_CFI
	/* Reset Command */
	switch ( flash_chip_info[ChipSeq].commandSet)
	{
		case COMMAND_SET_INTEL:
			/* reset */
			if ( dstAddr ) pDstAddr = dstAddr;
			*pDstAddr = IN28FXXX_CLEAR_STATUS;
			*pDstAddr = IN28FXXX_READ_ARRAY;
			break;

		case COMMAND_SET_AMD:
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
			*cmdAddr1 = AM29LVXXX_RESET_CMD;
			break;

		default:
    		setIlev( UART0_ILEV );
			rtlglue_printf( "unknown vendorID: %04x.  Halted at %s():%d.\n", flash_chip_info[ChipSeq].vendorId, __FUNCTION__, __LINE__ );
			while(1);
			break;		
	}
#else
	/* Reset Command */
	switch ( flash_chip_info[ChipSeq].vendorId )
	{
		case VENDOR_AMD:
		case VENDOR_MXIC:
		case VENDOR_FUJI:
		case VENDOR_ST:
		case VENDOR_ATMEL:
		case VENDOR_EON:
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
			*cmdAddr1 = AM29LVXXX_RESET_CMD;
			break;

		case VENDOR_INTEL:
			/* reset */
			if ( dstAddr ) pDstAddr = dstAddr;
			*pDstAddr = IN28FXXX_CLEAR_STATUS;
			*pDstAddr = IN28FXXX_READ_ARRAY;
			break;

		default:
    		setIlev( UART0_ILEV );
			rtlglue_printf( "unknown vendorID: %04x.  Halted at %s():%d.\n", flash_chip_info[ChipSeq].vendorId, __FUNCTION__, __LINE__ );
			while(1);
			break;
	}
#endif /* NOR_CFI */
}

#ifndef CONFIG_RTL865X_NOR_CFI
uint32 GetVendorID( uint32 flashbase )
{
	uint16 vendorID = 0x0000;
	uint16 vendorID_EON1 = 0x0000; /* for EON */
	uint16 vendorID_EON2 = 0x0000; /* for EON */
	volatile uint16* cmdAddr1 = (void*) ( flashbase + ( 0x555 * 2 ) );
	volatile uint16* cmdAddr2 = (void*) ( flashbase + ( 0x2AA * 2 ) );
	volatile uint16* pVendorId = (void*) ( flashbase + 0 );
	uint32 imask;

	imask = setIlev( MAX_ILEV );
	//
	// Try AMD first
	//
	/* Reset Command */
	*cmdAddr1 = AM29LVXXX_COMMAND1;
	*cmdAddr2 = AM29LVXXX_COMMAND2;
	*cmdAddr1 = AM29LVXXX_RESET_CMD;
	
	/* commands to get device id */
	*cmdAddr1 = AM29LVXXX_COMMAND1;
	*cmdAddr2 = AM29LVXXX_COMMAND2;
	*cmdAddr1 = AM29LVXXX_AUTOSELECT_CMD;

	/* read vendor ID */
	vendorID = *pVendorId;
	vendorID_EON1 = *((volatile uint16*)(flashbase+0x100));
	vendorID_EON2 = *((volatile uint16*)(flashbase+0x200));

	/* Reset Command */
	*cmdAddr1 = AM29LVXXX_COMMAND1;
	*cmdAddr2 = AM29LVXXX_COMMAND2;
	*cmdAddr1 = AM29LVXXX_RESET_CMD;
	
	if ( vendorID == VENDOR_AMD ||
	     vendorID == VENDOR_MXIC || 
	     vendorID == VENDOR_ST ||
	     vendorID == VENDOR_ATMEL ||
	     vendorID == VENDOR_FUJI )
	{
		goto out;
	}

	//
	// try EON
	//
	if ( vendorID_EON1 == 0x7f &&
		 vendorID_EON2 == 0x1c )
	{
		vendorID = VENDOR_EON;

		/* Reset Command */
		*cmdAddr1 = AM29LVXXX_COMMAND1;
		*cmdAddr2 = AM29LVXXX_COMMAND2;
		*cmdAddr1 = AM29LVXXX_RESET_CMD;
		goto out;
	}

	//
	// try INTEL
	//
	/* reset */
	*cmdAddr1 = IN28FXXX_READ_ARRAY;

	/* read devID */
	*cmdAddr1 = IN28FXXX_READ_ID;

	/* read vendor ID */
	vendorID = *pVendorId;

	/* reset */
	*cmdAddr1 = IN28FXXX_READ_ARRAY;
	
	if ( vendorID == VENDOR_INTEL )
	{
		goto out;
	}

	//
	//  unknown vendor
	//
	vendorID = VENDOR_UNKNOWN;
	
out:
	setIlev( imask );
	return vendorID;
}
#endif /* !NOR_CFI */

#ifndef CONFIG_RTL865X_NOR_CFI
uint32 GetDeviceId( uint32 flashbase )
{
	uint16 devId = 0x0000;
	volatile uint16* cmdAddr1 = (void*) ( flashbase + ( 0x555 * 2 ) );
	volatile uint16* cmdAddr2 = (void*) ( flashbase + ( 0x2AA * 2 ) );
	volatile uint16* pDevId = (void*) ( flashbase + 2 ); /* chip under test */
	uint32 imask;
	uint16 vendorID = flash_chip_info[0].vendorId;

	imask = setIlev( MAX_ILEV );

	switch ( vendorID )
	{
		case VENDOR_AMD:
		case VENDOR_MXIC:
		case VENDOR_FUJI:
		case VENDOR_ST:
		case VENDOR_ATMEL:
		case VENDOR_EON:
			/* Reset Command */
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
			*cmdAddr1 = AM29LVXXX_RESET_CMD;
			
			/* commands to get device id */
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
			*cmdAddr1 = AM29LVXXX_AUTOSELECT_CMD;

			/* read device id */
			devId = *pDevId;
			
			/* Reset Command */
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
			*cmdAddr1 = AM29LVXXX_RESET_CMD;
			break;

		case VENDOR_INTEL:
			/* reset */
			*cmdAddr1 = IN28FXXX_READ_ARRAY;

			/* read devID */
			*cmdAddr1 = IN28FXXX_READ_ID;

			/* read device id */
			devId = *pDevId;

			/* reset */
			*cmdAddr1 = IN28FXXX_READ_ARRAY;
			break;
			
		default:
			rtlglue_printf( "unknown vendorID: %04x.  Halted at %s():%d.\n", vendorID, __FUNCTION__, __LINE__ );
			setIlev( imask );
			while(1);
			break;
	}
	setIlev( imask );
	
	return devId;
}
#endif /* !NOR_CFI */

int32 init_flash_info( void )
{
	uint32 flashbase = FLASH_BASE; /* start address to detect */
	int32 flashSeq;
#ifndef CONFIG_RTL865X_NOR_CFI
	uint32 vendorId, devId;
#endif

	flash_total_size = 0;
	flash_num_of_chips = 0;

	for( flashSeq = 0; flashSeq < MAX_FLASH_CHIPS; flashSeq++ )
	{

		if ( flashSeq == 0 )
		{
			/* We assume the first flash chip ALWAYS exists !! */
		}
		else
		{
			/* FIXME: In 8650, the maximum flash size is 4MB.
			 *        It is dangerous to access beyond 4MB (0xc000-0000). 
			 */
 			if ( IS_865XA() && flash_total_size >= 0x400000 )
			{
#ifdef _DEBUG_
				rtlglue_printf( "865x: flash_total_size(0x%08X) is larger than or equal to 4MB ... stop detecting flash ...\n", flash_total_size );
#endif//_DEBUG_
				goto out;
			}
			else if ( IS_865XB() && flash_total_size >= 0x1000000 )
			{
#ifdef _DEBUG_
				rtlglue_printf( "865xB: flash_total_size(0x%08X) is larger than or equal to 16MB ... stop detecting flash ...\n", flash_total_size );
#endif//_DEBUG_
				goto out;
			}

#if 0
			/* detect other flash chips */
			if ( DetectFlashChip( flashbase ) == 1 )
			{
#ifdef _DEBUG_
				rtlglue_printf( "Flash[%d] not found ... stop detecting flash ...\n", flashSeq );
#endif//_DEBUG_
				break;
			}
#endif//0
		}

		// clear it
		memset( &flash_chip_info[flashSeq], 0, sizeof( flash_chip_info[flashSeq] ) );

#ifdef CONFIG_RTL865X_NOR_CFI

#define NOR_READ(addr, data_out)		(data_out) = *(volatile unsigned short *) (flashbase | ((addr) << 1))
#define NOR_WRITE(addr, data_in)		*(volatile unsigned short *) (flashbase | ((addr) << 1)) = (data_in)

		{
			unsigned int i, j;
			unsigned short temp1, temp2, temp3, temp4, temp5;

			static const uint32 CFI_STRUCT1[256], CFI_STRUCT2[256];
			uint32 *CFI_STRUCT;
			uint32 imask;

	        	flash_chip_info[flashSeq].Type = "CFI";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;

			if (flashSeq == 0)
			{
		        	flash_chip_info[flashSeq].BlockOffset = CFI_STRUCT1;
				CFI_STRUCT = (uint32 *) CFI_STRUCT1;
			}
			else
			{
		        	flash_chip_info[flashSeq].BlockOffset = CFI_STRUCT2;
				CFI_STRUCT = (uint32 *) CFI_STRUCT2;
			}

			imask = setIlev( MAX_ILEV );


			/* Reset */
			NOR_WRITE(0x000, 0xF0);
			NOR_WRITE(0x000, 0xFF);

			/* Read Vendor ID */
			NOR_WRITE(0x555, 0xAA);
			NOR_WRITE(0x2AA, 0x55);
			NOR_WRITE(0x555, 0x90);
			NOR_READ(0x000, temp1);
	        	flash_chip_info[flashSeq].vendorId= temp1;
			NOR_READ(0x001, temp1);
	        	flash_chip_info[flashSeq].devId= temp1;

			/* Reset */
			NOR_WRITE(0x000, 0xF0);
			NOR_WRITE(0x000, 0xFF);

			/* Query */
			NOR_WRITE(0x055, 0x98);

			/* Read Command Set */
			NOR_READ(0x013, temp1);
			flash_chip_info[flashSeq].commandSet = temp1;

			// Read Boot Sector Information      // Device Specific??!!
			NOR_READ(0x04F, temp1);

			if (temp1 == 0x2)
				flash_chip_info[flashSeq].isBottom = TRUE;

			if (temp1 == 0x3)
				flash_chip_info[flashSeq].isTop = TRUE;

			/* Read Block Count */
			NOR_READ(0x02C, temp1);

			/* Read Device Size */
			NOR_READ(0x027, temp2);
			flash_chip_info[flashSeq].ChipSize = (temp1 == 0) ? 0 : (1 << temp2);

			/* Set Initial Block Address */
			CFI_STRUCT[0] = 0x0;
			flash_chip_info[flashSeq].NumOfBlock += 1;

			for (i = 0; i < temp1; i++)
			{
				if (flash_chip_info[flashSeq].isTop == TRUE)
					j = temp1 - i - 1;
				else
					j = i;

				/* Read Sector Count */
				NOR_READ(0x02D + j*4, temp2);
				NOR_READ(0x02E + j*4, temp3);

				/* Read Sector Size */
				NOR_READ(0x02F + j*4, temp4);
				NOR_READ(0x030 + j*4, temp5);

				for (j = 0; j < (temp3 << 8) + temp2 + 1; j++)
				{
					CFI_STRUCT[flash_chip_info[flashSeq].NumOfBlock+j] = CFI_STRUCT [flash_chip_info[flashSeq].NumOfBlock+j-1] + (((temp5 << 8) + temp4) << 8);
				}

				flash_chip_info[flashSeq].NumOfBlock += (temp3 << 8) + temp2 + 1;
			}

			/* Reset */
			NOR_WRITE(0x000, 0xF0);
			NOR_WRITE(0x000, 0xFF);

			setIlev( imask );

			if (flash_chip_info[flashSeq].isTop == TRUE)
			{
		        	flash_chip_info[flashSeq].BoardInfo = flash_chip_info[flashSeq].ChipSize - 0x4000;
		        	flash_chip_info[flashSeq].CGIConfig= flash_chip_info[flashSeq].ChipSize - 0x10000; 
		        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
			}
			else
			{
		        	flash_chip_info[flashSeq].BoardInfo = 0x00004000;
		        	flash_chip_info[flashSeq].CGIConfig= 0x00006000; 
		        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
			}
		}
#endif  /* NOR_CFI */

#ifndef CONFIG_RTL865X_NOR_CFI
		vendorId = flash_chip_info[flashSeq].vendorId = GetVendorID( flashbase );

#ifdef _DEBUG_
		rtlglue_printf( "Flash[%d].vendorId    = %04x\n", flashSeq, vendorId );
#endif// _DEBUG_

		if ( vendorId == VENDOR_UNKNOWN )
		{
			// unknown vendor.
			/* The first chip cannot be unknown vendor */
			if ( flashSeq == 0 )
			{
				rtlglue_printf( "Halted! Error get first flash vendor ID (0x%08x).\n", vendorId );
				while ( 1 );
			}
			else
			{
				// if not the firstc chip, it is ok.
				break;
			}
		}

		devId = GetDeviceId( flashbase );
		
#ifdef _DEBUG_
		rtlglue_printf( "Flash[%d].devId       = %04x\n", flashSeq, devId );
#endif// _DEBUG_

		switch ( devId )
		{
	        case AM29LV800BB_DEVID:
	        	flash_chip_info[flashSeq].Type = "AM29LV800BB";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boAm29LV800DB;
	        	flash_chip_info[flashSeq].ChipSize = 0x100000;
	        	flash_chip_info[flashSeq].NumOfBlock = 19;
	        	flash_chip_info[flashSeq].isBottom = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case AM29LV800BT_DEVID:
	        	flash_chip_info[flashSeq].Type = "AM29LV800BT";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boAm29LV800DT;
	        	flash_chip_info[flashSeq].ChipSize = 0x100000;
	        	flash_chip_info[flashSeq].NumOfBlock = 19;
	        	flash_chip_info[flashSeq].isTop = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x000FC000; /* 16KB */
	        	flash_chip_info[flashSeq].CGIConfig   = 0x000F0000; /* 48KB */
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case AM29LV160BB_DEVID: /* also Spansion S29AL016M__B */
	        	flash_chip_info[flashSeq].Type = "AM29LV160BB";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boAm29LV160DB;
	        	flash_chip_info[flashSeq].ChipSize = 0x200000;
	        	flash_chip_info[flashSeq].NumOfBlock = 35;
	        	flash_chip_info[flashSeq].isBottom = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case AM29LV160BT_DEVID: /* also Spansion S29AL016M__T */
	        	flash_chip_info[flashSeq].Type = "AM29LV160BT";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boAm29LV160DT;
	        	flash_chip_info[flashSeq].ChipSize = 0x200000;
	        	flash_chip_info[flashSeq].NumOfBlock = 35;
	        	flash_chip_info[flashSeq].isTop = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x001FC000; /* 16KB */
	        	flash_chip_info[flashSeq].CGIConfig   = 0x001F0000; /* 48KB */
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case AM29LV320DB_DEVID:
			case AT49BV322A_DEVID:
	        	flash_chip_info[flashSeq].Type = "AM29LV320DB";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boAm29LV320DB;
	        	flash_chip_info[flashSeq].ChipSize = 0x400000;
	        	flash_chip_info[flashSeq].NumOfBlock = 71;
	        	flash_chip_info[flashSeq].isBottom = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case AM29LV320DT_DEVID:
			case AT49BV322AT_DEVID:
	        	flash_chip_info[flashSeq].Type = "AM29LV320DT";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boAm29LV320DT;
	        	flash_chip_info[flashSeq].ChipSize = 0x400000;
	        	flash_chip_info[flashSeq].NumOfBlock = 71;
	        	flash_chip_info[flashSeq].isTop = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x003FC000; /* 16KB */
	        	flash_chip_info[flashSeq].CGIConfig   = 0x003F0000; /* 48KB */
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case M29W320DB_DEVID: /* also MX29LV640BB_DEVID */
	        	if ( vendorId == VENDOR_ST )
	        	{
	        		/* ST's flash */
		        	flash_chip_info[flashSeq].Type = "M29W320DB";
		        	flash_chip_info[flashSeq].BlockBase = flashbase;
		        	flash_chip_info[flashSeq].BlockOffset = boM29W320DB;
		        	flash_chip_info[flashSeq].ChipSize = 0x400000;
		        	flash_chip_info[flashSeq].NumOfBlock = 67;
		        	flash_chip_info[flashSeq].isBottom = TRUE;
		        	flash_chip_info[flashSeq].devId = devId;
		        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
		        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
		        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
		        }
		        else
				{
					/* Other AMD-compatible flash */
		        	flash_chip_info[flashSeq].Type = "MX29LV640BB";
		        	flash_chip_info[flashSeq].BlockBase = flashbase;
		        	flash_chip_info[flashSeq].BlockOffset = boMX29LV640BB;
		        	flash_chip_info[flashSeq].ChipSize = 0x800000;
		        	flash_chip_info[flashSeq].NumOfBlock = 135;
		        	flash_chip_info[flashSeq].isBottom = TRUE;
		        	flash_chip_info[flashSeq].devId = devId;
		        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
		        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
		        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
		        }
	        	break;

	        case M29W320DT_DEVID:
	        	flash_chip_info[flashSeq].Type = "M29W320DT";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boM29W320DT;
	        	flash_chip_info[flashSeq].ChipSize = 0x400000;
	        	flash_chip_info[flashSeq].NumOfBlock = 67;
	        	flash_chip_info[flashSeq].isTop = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x003FC000; /* 16KB */
	        	flash_chip_info[flashSeq].CGIConfig   = 0x003F0000; /* 48KB */
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case S29GLxxxM_DEVID:
#if 0
				/* R4, top */
	        	flash_chip_info[flashSeq].Type = "S29GL064M90TAIR4";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boMX29LV640TB;
	        	flash_chip_info[flashSeq].ChipSize = 0x800000;
	        	flash_chip_info[flashSeq].NumOfBlock = 135;
	        	flash_chip_info[flashSeq].isBottom = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;
#else
		        {
					volatile uint16* cmdAddr1 = (void*) ( flashbase + ( 0x555 * 2 ) );
					volatile uint16* cmdAddr2 = (void*) ( flashbase + ( 0x2AA * 2 ) );
					volatile uint16 cycle2;
					volatile uint16 cycle3;

					/* Reset Command */
					*cmdAddr1 = AM29LVXXX_COMMAND1;
					*cmdAddr2 = AM29LVXXX_COMMAND2;
					*cmdAddr1 = AM29LVXXX_RESET_CMD;
					
					/* commands to get device id */
					*cmdAddr1 = AM29LVXXX_COMMAND1;
					*cmdAddr2 = AM29LVXXX_COMMAND2;
					*cmdAddr1 = AM29LVXXX_AUTOSELECT_CMD;

					/* read cycle2 & cycle3 */
					cycle2 = *(uint16*) ( flashbase + ( 0x00E * 2 ) );
					cycle3 = *(uint16*) ( flashbase + ( 0x00F * 2 ) );
					
					/* Reset Command */
					*cmdAddr1 = AM29LVXXX_COMMAND1;
					*cmdAddr2 = AM29LVXXX_COMMAND2;
					*cmdAddr1 = AM29LVXXX_RESET_CMD;
					
		        	if ( vendorId == VENDOR_AMD )
		        	{
		        		switch( cycle2 )
		        		{
		        			case 0x2212:
		        				if ( cycle3 == 0x2200 )
		        				{
#if 1
									goto unsupported; /* beyond IC limit */
#else
						        	flash_chip_info[flashSeq].Type = "S29GL256M";
						        	flash_chip_info[flashSeq].BlockBase = flashbase;
						        	flash_chip_info[flashSeq].BlockOffset = boMX29LV640BB;
						        	flash_chip_info[flashSeq].ChipSize = 0x800000;
						        	flash_chip_info[flashSeq].NumOfBlock = 135;
						        	flash_chip_info[flashSeq].isBottom = TRUE;
						        	flash_chip_info[flashSeq].devId = devId;
						        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
						        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
						        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
#endif
						        }
						        else if ( cycle3 == 0x2201 )
						        {
						        	flash_chip_info[flashSeq].Type = "S29GL128M R1,R2,R8,R9";
						        	flash_chip_info[flashSeq].BlockBase = flashbase;
						        	//flash_chip_info[flashSeq].BlockOffset = boS29GL128M; // FIXME
						        	flash_chip_info[flashSeq].ChipSize = 0x1000000;
						        	flash_chip_info[flashSeq].NumOfBlock = -1;
						        	flash_chip_info[flashSeq].isBottom = TRUE;
						        	flash_chip_info[flashSeq].devId = devId;
						        	flash_chip_info[flashSeq].BoardInfo   = 0x00FE0000;
						        	flash_chip_info[flashSeq].CGIConfig   = 0x00FF0000;
						        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
						        }
						        else
						        	goto unsupported;
						        break;
		        			case 0x2210: /* 064M R3,R4 */
		        				if ( cycle3 == 0x2200 )
		        				{
		        					/* R4, bottom */
						        	flash_chip_info[flashSeq].Type = "S29GL064M90TAIR4";
						        	flash_chip_info[flashSeq].BlockBase = flashbase;
						        	flash_chip_info[flashSeq].BlockOffset = boMX29LV640BB;
						        	flash_chip_info[flashSeq].ChipSize = 0x800000;
						        	flash_chip_info[flashSeq].NumOfBlock = 135;
						        	flash_chip_info[flashSeq].isBottom = TRUE;
						        	flash_chip_info[flashSeq].devId = devId;
						        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
						        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
						        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
						        }
						        else if ( cycle3 == 0x2201 )
						        {
						        	/* R3, top */
						        	flash_chip_info[flashSeq].Type = "S29GL064M90TAIR3";
						        	flash_chip_info[flashSeq].BlockBase = flashbase;
						        	flash_chip_info[flashSeq].BlockOffset = boMX29LV640BT;
						        	flash_chip_info[flashSeq].ChipSize = 0x800000;
						        	flash_chip_info[flashSeq].NumOfBlock = 135;
						        	flash_chip_info[flashSeq].isTop = TRUE;
						        	flash_chip_info[flashSeq].devId = devId;
						        	flash_chip_info[flashSeq].BoardInfo   = 0x007FC000; /* 16KB */
						        	flash_chip_info[flashSeq].CGIConfig   = 0x007F0000; /* 48KB */
						        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
						        }
						        else
						        	goto unsupported;
						        break;

		        			case 0x0000: /* 064A R3,R4 */
		        				if ( cycle3 == 0x2200 )
		        				{
		        					/* R4, bottom */
						        	flash_chip_info[flashSeq].Type = "S29GL064A90T*IR4";
						        	flash_chip_info[flashSeq].BlockBase = flashbase;
						        	flash_chip_info[flashSeq].BlockOffset = boMX29LV640BB;
						        	flash_chip_info[flashSeq].ChipSize = 0x800000;
						        	flash_chip_info[flashSeq].NumOfBlock = 135;
						        	flash_chip_info[flashSeq].isBottom = TRUE;
						        	flash_chip_info[flashSeq].devId = devId;
						        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
						        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
						        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
						        }
						        else if ( cycle3 == 0x2201 )
						        {
						        	/* R3, top */
						        	flash_chip_info[flashSeq].Type = "S29GL064A90T*IR3";
						        	flash_chip_info[flashSeq].BlockBase = flashbase;
						        	flash_chip_info[flashSeq].BlockOffset = boMX29LV640BT;
						        	flash_chip_info[flashSeq].ChipSize = 0x800000;
						        	flash_chip_info[flashSeq].NumOfBlock = 135;
						        	flash_chip_info[flashSeq].isTop = TRUE;
						        	flash_chip_info[flashSeq].devId = devId;
						        	flash_chip_info[flashSeq].BoardInfo   = 0x007FC000; /* 16KB */
						        	flash_chip_info[flashSeq].CGIConfig   = 0x007F0000; /* 48KB */
						        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
						        }
						        else
						        	goto unsupported;
						        break;

		        			case 0x221a: /* 032M R3,R4,R5,R6,R7 */
						        	/* R4, top */
						        	flash_chip_info[flashSeq].Type = "S29GL032M90TAIR4";
						        	flash_chip_info[flashSeq].BlockBase = flashbase;
						        	flash_chip_info[flashSeq].BlockOffset = boMX29LV320AB;
						        	flash_chip_info[flashSeq].ChipSize = 0x400000;
						        	flash_chip_info[flashSeq].NumOfBlock = 71;
						        	flash_chip_info[flashSeq].isBottom = TRUE;
						        	flash_chip_info[flashSeq].devId = devId;
						        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000; /* 8KB */
						        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000; /* 48KB */
						        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
								break;
							/* not supported yet */
		        			case 0x2213: /* 064M R0,R5,R6,R7 */
		        			case 0x220c: /* 064M R1,R2,R8,R9 */
		        			case 0x221c: /* 032M R0 */
		        			case 0x221d: /* 032M R1,R2,R8,R9 */
						    default:
						    	rtlglue_printf( "Spansion: cycle2=0x%04x, cycle3=0x%04x\n", cycle2, cycle3 );
						    	goto unsupported;
						    	break;
					    }
			        }
		        }
	        	break;
#endif
	        case MX29LV320AB_DEVID:
	        	flash_chip_info[flashSeq].Type = "MX29LV320AB";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boMX29LV320AB;
	        	flash_chip_info[flashSeq].ChipSize = 0x400000;
	        	flash_chip_info[flashSeq].NumOfBlock = 71;
	        	flash_chip_info[flashSeq].isBottom = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;
	        	
	        case MX29LV320AT_DEVID:
	        	flash_chip_info[flashSeq].Type = "MX29LV320AT";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boMX29LV320AT;
	        	flash_chip_info[flashSeq].ChipSize = 0x400000;
	        	flash_chip_info[flashSeq].NumOfBlock = 71;
	        	flash_chip_info[flashSeq].isTop = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x003FC000; /* 16KB */
	        	flash_chip_info[flashSeq].CGIConfig   = 0x003F0000; /* 48KB */
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case MX29LV640BT_DEVID:
	        	flash_chip_info[flashSeq].Type = "MX29LV640BT";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boMX29LV640BT;
	        	flash_chip_info[flashSeq].ChipSize = 0x800000;
	        	flash_chip_info[flashSeq].NumOfBlock = 135;
	        	flash_chip_info[flashSeq].isTop = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x007FC000; /* 16KB */
	        	flash_chip_info[flashSeq].CGIConfig   = 0x007F0000; /* 48KB */
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case I28F320J3A:
	        	flash_chip_info[flashSeq].Type = "I28F320J3A";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boINTEL4M;
	        	flash_chip_info[flashSeq].ChipSize = 0x400000;
	        	flash_chip_info[flashSeq].NumOfBlock = 32;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x003C0000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x003E0000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case I28F640J3A:
	        	flash_chip_info[flashSeq].Type = "I28F640J3A";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boINTEL8M;
	        	flash_chip_info[flashSeq].ChipSize = 0x800000;
	        	flash_chip_info[flashSeq].NumOfBlock = 64;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x007C0000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x007E0000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case I28F128J3A:
	        	flash_chip_info[flashSeq].Type = "I28F128J3A";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boINTEL16M;
	        	flash_chip_info[flashSeq].ChipSize = 0x1000000;
	        	flash_chip_info[flashSeq].NumOfBlock = 128;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x00FC0000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x00FE0000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;

	        case I28F160C3B:
	        	flash_chip_info[flashSeq].Type = "I28F160C3B";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;
	        	flash_chip_info[flashSeq].BlockOffset = boINTEL2MB;
	        	flash_chip_info[flashSeq].ChipSize = 0x200000;
	        	flash_chip_info[flashSeq].NumOfBlock = 39;
	        	flash_chip_info[flashSeq].isBottom = TRUE;
	        	flash_chip_info[flashSeq].devId = devId;
	        	flash_chip_info[flashSeq].BoardInfo   = 0x00004000;
	        	flash_chip_info[flashSeq].CGIConfig   = 0x00006000;
	        	flash_chip_info[flashSeq].RuntimeCode = 0x00020000;
	        	break;
	        	
	        default:
	        goto unsupported;
unsupported:
#ifdef _DEBUG_
				rtlglue_printf( "Flash[%d]: unsupported flash Device ID = %04x ....\n", flashSeq, devId );
#endif//_DEBUG_
	        	flash_chip_info[flashSeq].Type = "Unsupported";
	        	flash_chip_info[flashSeq].BlockBase = flashbase;

				/* The first chip cannot be unknown type */
				if ( flashSeq == 0 )
				{
					rtlglue_printf( "Halted! Error get first flash device ID (0x%08x).\n", devId );
					while ( 1 );
				}
				
	        	break;
		}

#endif  /* !NOR_CFI */

		if ( flash_chip_info[flashSeq].ChipSize == 0 ) break; // unsupported device id

		if ( flashSeq == 0 )
		{
			/* The ROM size register in ASIC means the size of 'CS0 flash'. */
			SetROMSize( flash_chip_info[flashSeq].ChipSize );
		}

#ifdef _DEBUG_
		rtlglue_printf( "Flash[%d].Type        = '%s'\n", flashSeq, flash_chip_info[flashSeq].Type );
		rtlglue_printf( "Flash[%d].BlockBase   = 0x%08x\n", flashSeq, flash_chip_info[flashSeq].BlockBase );
		rtlglue_printf( "Flash[%d].ChipSize    = 0x%08x\n", flashSeq, flash_chip_info[flashSeq].ChipSize );
		rtlglue_printf( "Flash[%d].NumOfBlock  = 0x%08x\n", flashSeq, flash_chip_info[flashSeq].NumOfBlock );
#endif//_DEBUG_

		flash_num_of_chips++;
		flashbase += flash_chip_info[flashSeq].ChipSize;
		flash_total_size += flash_chip_info[flashSeq].ChipSize;
	}

out:
	return flash_num_of_chips;
}


/* 
 * Addr is absolute address, such as: 0xbfc04000
 */
int32 searchBlockSeq( uint32 Addr, uint32 *chipNum, uint32 *blockNum )
{
	uint32 ChipSeq;
	uint32 BlockSeq;
	uint32 blockbase;

#ifdef _DEBUG_
	BlockSeq = blockbase = 0;
#endif/*_DEBUG_*/

	for( ChipSeq = 0; ChipSeq < flash_num_of_chips; ChipSeq++ )
	{
		assert( flash_chip_info[ChipSeq].BlockBase != 0 );
		assert( flash_chip_info[ChipSeq].BlockOffset != NULL );
		assert( flash_chip_info[ChipSeq].ChipSize != 0 );
		assert( flash_chip_info[ChipSeq].NumOfBlock > 0 );

		blockbase = flash_chip_info[ChipSeq].BlockBase;
		for( BlockSeq = 0; BlockSeq < flash_chip_info[ChipSeq].NumOfBlock; BlockSeq++ )
		{
			//rtlglue_printf("[%d][%d]: 0x%08x\n", ChipSeq, BlockSeq, blockbase + (uint32)flash_chip_info[ChipSeq].BlockOffset[ BlockSeq + 1 ] );
			if ( blockbase + (uint32)flash_chip_info[ChipSeq].BlockOffset[ BlockSeq + 1 ] > Addr )
			{
				goto found;
			}
		}
	}

#ifdef _DEBUG_
	rtlglue_printf( "[%s:%d] Address 0x%08x is not in flash range(last:0x%08x)\n", __FUNCTION__, __LINE__, Addr-blockbase, flash_chip_info[ChipSeq].BlockOffset[ BlockSeq ] );
#endif/*_DEBUG_*/

	/* Not found */
	return 1;

found:
	*chipNum = ChipSeq;
	*blockNum = BlockSeq;
	return 0;
}


void flashdrv_init(void)
{
	flashdrv_mutexLock();

	init_flash_info();

#ifdef _SUPPORT_LARGE_FLASH_
	/* read bdinfo before any read to flash (to recover flash JUMP code) */
    flashdrv_read( &bdinfo, (void*)(FLASH_MAP_BOARD_INFO_ADDR), sizeof(bdinfo_t) );
/*
	rtlglue_printf( "\n*** read bdinfo ***\n0x%08x\n0x%08x\n\n", bdinfo.BackupInst[0], bdinfo.BackupInst[1] );
*/
#endif

#ifdef LOADER
   flashdrv_read( &bdinfo, (void*)(FLASH_MAP_BOARD_INFO_ADDR), sizeof(bdinfo_t) );	
   if(bdinfo.chipsize !=(flash_total_size>>20))
   {
       rtlglue_printf( "flash size=0x%08X\n", flash_total_size );
       bdinfo.chipsize=flash_total_size>>20;
	flashdrv_updateBdInfo(&bdinfo);
   }
#endif

	flashdrv_mutexUnlock();
}


uint32 flashdrv_getDevSize(void)
{
	return flash_total_size;
}


/* Internal Function */
uint32 _flashdrv_eraseBlock( uint32 ChipSeq, uint32 BlockSeq )
{
	uint32 imask;
	register uint32 pat;
	volatile uint16 *cmdAddr1;
	volatile uint16 *cmdAddr2;
	volatile uint16 *eraseAddr = NULL;
#ifdef _DEBUG_
	uint32 blockSize;
	uint32 i;
	blockSize = 0;
#endif//_DEBUG_
	int32 ret;

	/* Prevent interrupts interference */
	imask = setIlev(MAX_ILEV);

	if ( ChipSeq >= flash_num_of_chips ) { ret = 1; goto out; }
	if ( BlockSeq >= flash_chip_info[ChipSeq].NumOfBlock ) { ret = 1; goto out; }

#ifdef _DEBUG_
	rtlglue_printf( "flashdrv_eraseBlock( ChipSeq=%d, BlockSeq=%d)\n", ChipSeq, BlockSeq );
#endif//_DEBUG_

	cmdAddr1 = (void*) ( flash_chip_info[ChipSeq].BlockBase + ( 0x555 * 2 ) );
	cmdAddr2 = (void*) ( flash_chip_info[ChipSeq].BlockBase + ( 0x2AA * 2 ) );
	eraseAddr = (void*) ( flash_chip_info[ChipSeq].BlockBase + 
	                      flash_chip_info[ChipSeq].BlockOffset[BlockSeq] );

#ifdef _DEBUG_
	rtlglue_printf( "Look-ahead(%p):", eraseAddr );
	for( i=0; i<8; i++ ) rtlglue_printf( "%04x-", *(eraseAddr+i) );
	rtlglue_printf( "\n" );
#endif//_DEBUG_

	flashdrv_reset( ChipSeq, BlockSeq, NULL );

#ifdef CONFIG_RTL865X_NOR_CFI
	switch ( flash_chip_info[ChipSeq].commandSet)
	{
		case COMMAND_SET_INTEL:
			/* unlock block */
			*eraseAddr = IN28FXXX_READ_ARRAY;
			if ( flash_chip_info[ChipSeq].devId==I28F160C3B ) /* C3 series needs to be unlocked. */
			{
				*eraseAddr = IN28FXXX_UNLOCK_BLOCK1;
				*eraseAddr = IN28FXXX_UNLOCK_BLOCK2;

#ifdef _DEBUG_
				/* ensure unlocked !! */
				*eraseAddr = IN28FXXX_READ_ARRAY;
				*eraseAddr = IN28FXXX_READ_ID;
				if ( ( *(eraseAddr+2) & 0x0001 ) == 0x0001 )
				{
					rtlglue_printf( "warning! flashdrv_eraseBlock(): The block is still locked, addr: 0x%08x.\n", (uint32)(eraseAddr+2) );
				}
#endif// _DEBUG_
			}

			/* reset */
			*eraseAddr = IN28FXXX_CLEAR_STATUS;
			if ( flash_chip_info[ChipSeq].devId==I28F160C3B ) /* C3 series needs to be unlocked. */
				*eraseAddr = IN28FXXX_READ_ARRAY;
			
			/* erase */
			*eraseAddr = IN28FXXX_ERASE_BLOCK1;
			*eraseAddr = IN28FXXX_ERASE_BLOCK2;

			/* read status */
			*eraseAddr = IN28FXXX_READ_STATUS;
			break;

		case COMMAND_SET_AMD:
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
			*cmdAddr1 = AM29LVXXX_SECTOR_ERASE_CMD1;
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
			*eraseAddr = AM29LVXXX_SECTOR_ERASE_CMD2;
			break;

		default:
			setIlev( imask );
			rtlglue_printf( "unknown vendorID: %04x.  Halted at %s():%d.\n", flash_chip_info[ChipSeq].vendorId, __FUNCTION__, __LINE__ );
			while(1);
			break;
	}
#else
	switch ( flash_chip_info[ChipSeq].vendorId )
	{
		case VENDOR_AMD:
		case VENDOR_MXIC:
		case VENDOR_FUJI:
		case VENDOR_ST:
		case VENDOR_ATMEL:
		case VENDOR_EON:
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
			*cmdAddr1 = AM29LVXXX_SECTOR_ERASE_CMD1;
			*cmdAddr1 = AM29LVXXX_COMMAND1;
			*cmdAddr2 = AM29LVXXX_COMMAND2;
		    *eraseAddr = AM29LVXXX_SECTOR_ERASE_CMD2;
		    break;
		    
		case VENDOR_INTEL:
			/* unlock block */
			*eraseAddr = IN28FXXX_READ_ARRAY;
			if ( flash_chip_info[ChipSeq].devId==I28F160C3B ) /* C3 series needs to be unlocked. */
			{
				*eraseAddr = IN28FXXX_UNLOCK_BLOCK1;
				*eraseAddr = IN28FXXX_UNLOCK_BLOCK2;

#ifdef _DEBUG_
				/* ensure unlocked !! */
				*eraseAddr = IN28FXXX_READ_ARRAY;
				*eraseAddr = IN28FXXX_READ_ID;
				if ( ( *(eraseAddr+2) & 0x0001 ) == 0x0001 )
				{
					rtlglue_printf( "warning! flashdrv_eraseBlock(): The block is still locked, addr: 0x%08x.\n", (uint32)(eraseAddr+2) );
				}
#endif// _DEBUG_
			}

			/* reset */
			*eraseAddr = IN28FXXX_CLEAR_STATUS;
			if ( flash_chip_info[ChipSeq].devId==I28F160C3B ) /* C3 series needs to be unlocked. */
				*eraseAddr = IN28FXXX_READ_ARRAY;
			
			/* erase */
			*eraseAddr = IN28FXXX_ERASE_BLOCK1;
			*eraseAddr = IN28FXXX_ERASE_BLOCK2;

			/* read status */
			*eraseAddr = IN28FXXX_READ_STATUS;
			break;
			
		default:
			setIlev( imask );
			rtlglue_printf( "unknown vendorID: %04x.  Halted at %s():%d.\n", flash_chip_info[ChipSeq].vendorId, __FUNCTION__, __LINE__ );
			while(1);
			break;
	}
#endif /* NOR_CFI */
  
    /* Check Q7 for completion */
    tick_pollStart();
    while ( ( ( pat = *eraseAddr ) & 0x80 ) == 0 )
    {
    	/* kick watchdog */
    	REG32( WDTCNR_ADDR ) |= WDTCLR;
    	
        /* Check Q5 for timeout */
		/*
        if (pat & 0x20)
        {
            rtlglue_printf("\n! Block number %d erase internal timeout.\n ", BlockSeq );
    
            setIlev( imask );
            ret = 1;
            goto out;
        }
		*/
        
        if (tick_pollGet10MS() > TIMEOUT_10MS )
        {
            rtlglue_printf( "\n! Block number %d (%p) erase timeout.\n", BlockSeq, eraseAddr );
            ret = 1;
            goto out;
        }
    }

#ifdef CONFIG_RTL865X_NOR_CFI
    if ( ( flash_chip_info[ChipSeq].commandSet== COMMAND_SET_INTEL) &&
         ( (pat&0x7f) != 00 ) )
    {
    	rtlglue_printf( "\nWarning! Status Register is NOT clean: 0x%04x.\n", pat&0x7f );
    }

#else
    if ( ( flash_chip_info[ChipSeq].vendorId == VENDOR_INTEL ) &&
         ( (pat&0x7f) != 00 ) )
    {
    	rtlglue_printf( "\nWarning! Status Register is NOT clean: 0x%04x.\n", pat&0x7f );
    }
#endif /* NOR_CFI */

#ifdef _DEBUG_
	/* blank check */
	flashdrv_reset( ChipSeq, BlockSeq, NULL );
	blockSize = flash_chip_info[ChipSeq].BlockOffset[BlockSeq+1] - flash_chip_info[ChipSeq].BlockOffset[BlockSeq];
	for( i = ( blockSize >> 1 );
	     i > 0;
	     i--, eraseAddr++ )
	{
		if ( *eraseAddr != 0xffff )
		{
			rtlglue_printf( "\neraseBlock(): 0x%08x is not 0xffff (0x%04x).\n", (uint32)eraseAddr, *eraseAddr );
			ret = 1;
			goto out;
		}
	}
#endif// _DEBUG_

#ifdef _DEBUG_
	rtlglue_printf( "\neraseBlock(): blank check (0x%08x) OK !\n", ((uint32)eraseAddr)-blockSize );
#endif// _DEBUG_

	ret = 0;

out:
	flashdrv_reset( ChipSeq, BlockSeq, NULL );
	
#ifdef _DEBUG_
	rtlglue_printf( "Look-ahead(0x%08x):", ((uint32)eraseAddr)-(blockSize>>1) );
	for( i=0; i<8; i++ ) rtlglue_printf( "%04x-", *((eraseAddr-(blockSize>>1))+i) );
	rtlglue_printf( "\n" );
#endif//_DEBUG_

    setIlev( imask );
    return ret;
}

/* External Function */
uint32 flashdrv_eraseBlock( uint32 ChipSeq, uint32 BlockSeq )
{
	uint32 ret;

	flashdrv_mutexLock();
	ret = _flashdrv_eraseBlock( ChipSeq, BlockSeq );
	flashdrv_mutexUnlock();

	return ret;
}

uint32 flashdrv_updateImg(void *srcAddr_P, void *dstAddr_P, uint32 size)
{
    uint32 ChipSeq;
    uint32 BlockSeq;
    uint32 erased = 0;
    uint32 retval = 1;
   
    ASSERT_CSP( srcAddr_P );
    ASSERT_CSP( dstAddr_P );

	flashdrv_mutexLock();

#ifdef _DEBUG_
	rtlglue_printf( "flashdrv_updateImg(0x%p,0x%p,0x%08X)\n", srcAddr_P, dstAddr_P, size );
#endif//_DEBUG_

#ifdef _DEBUG_
	{
		volatile uint16 *dstAddr = dstAddr_P;
		int i;
		
		rtlglue_printf( "Look-ahead(0x%08x):", (uint32)dstAddr_P );
		for( i=0; i<8; i++ ) rtlglue_printf( "%04x-", *(dstAddr+i) );
		rtlglue_printf( "\n" );
	}
#endif//_DEBUG_

    /* Check if the size is larger than than total flash size */
	if ( searchBlockSeq( ((uint32)dstAddr_P + size - 1 ), &ChipSeq, &BlockSeq ) != 0 )
	{
		rtlglue_printf( "[%s:%d] Size is larger than total flash size! dstAddr_P=0x%08x size=0x%08x\n", __FUNCTION__, __LINE__, (uint32)dstAddr_P, size );
		goto out;
	}

    while ( erased < size )
    {
		uint32 BlockStart;

		/* Search the #chip and #block */
		if ( searchBlockSeq( ((uint32)dstAddr_P) + erased, &ChipSeq, &BlockSeq ) != 0 )
		{
			rtlglue_printf( "searchBlockSeq(0x%08x) failed.\n", ((uint32)dstAddr_P) + erased );
			goto out;
		}

		//rtlglue_printf( "W:0x%08x B: 0x%08x b:%d\n", erased, flash_chip_info[ChipSeq].BlockBase, BlockSeq );
		BlockStart = flash_chip_info[ChipSeq].BlockBase + flash_chip_info[ChipSeq].BlockOffset[BlockSeq];

		/* If the writing address is NOT at the start address of block,
		   DO NOT erase the block. We assume the block has been erased in the lastest updating. */
		if ( ( ( (uint32)dstAddr_P ) + erased ) != BlockStart )
		{
			// DO NOT erase this block.
#ifdef _DEBUG_
			rtlglue_printf( "flashdrv_eraseBlock(0x%08X!=0x%08X,Chip:%d,Block:%d) is skiped !\n", 
			        ((uint32)dstAddr_P) + erased, 
			        BlockStart,
			        ChipSeq, BlockSeq );
#endif//_DEBUG_

			// skip bytes those need NOT to be erased.
			// move to the next block start address.
			uint32 NextBlockStart = flash_chip_info[ChipSeq].BlockBase + flash_chip_info[ChipSeq].BlockOffset[BlockSeq+1];
			erased = NextBlockStart - (uint32)dstAddr_P;

#ifdef _DEBUG_
			rtlglue_printf( "erased=0x%08X  (dstAddr_P+erased)=0x%08X\n", erased, ( (uint32)dstAddr_P ) + erased );
#endif//_DEBUG_
		}
		else
		{
#ifdef _DEBUG_
			rtlglue_printf( "erasing (Chip:%d,Block:%d)...\n", ChipSeq, BlockSeq );
#endif//_DEBUG_

			// Erase this block.
			if ( _flashdrv_eraseBlock( ChipSeq, BlockSeq ) != 0 )
			{
				rtlglue_printf( "! flashdrv_eraseBlock(Chip:%d,Block:%d) failed.\n", ChipSeq, BlockSeq );
				goto out;
			}

			erased += flash_chip_info[ChipSeq].BlockOffset[BlockSeq+1] - 
			          flash_chip_info[ChipSeq].BlockOffset[BlockSeq];
		}

	}

    /* Write blocks */
#ifdef _SUPPORT_LARGE_FLASH_
    if ( flash_chip_info[0].ChipSize > 0x400000 &&
         flashdrv_ignore_largeflash==0 )
    {
    	/* If the first chip is larger than 4MB (not including 4MB),
    	 * 865xB has bug to boot system.
    	 * We must backup instructions at 4MB or 8MB position into bdinfo, and recover them in flashdrv_read().
    	 */
    	uint32 startOffset; /* start offset to write */
    	uint32 endOffset;   /* end offset to write */
    	uint32 InstructionOffset; /* offset of boot instruction */
    	
		startOffset = ((uint32)dstAddr_P) - FLASH_BASE;
		endOffset = startOffset + size;

		/* 865xB has bug when booting from a single 8MB or 16MB flash. */
		if ( TRUE == flashdrv_isOver5xBBootInstructions( startOffset, endOffset, &InstructionOffset ) )
		{
			uint32 *pStartAddrToBackup;
			void *pAddrToInject;

			pStartAddrToBackup = (uint32*)( ((uint32)srcAddr_P+(InstructionOffset-startOffset)) );
			rtlglue_printf("before backup ...\n%p=0x%08x\n%p=0x%08x\n",&pStartAddrToBackup[0],pStartAddrToBackup[0],&pStartAddrToBackup[1],pStartAddrToBackup[1]);
			flashdrv_backupAndModifyBootInstructions( pStartAddrToBackup );

			/* Inject right now! To reduce risk .... */
			pAddrToInject = (void*)( (uint32)((uint32)dstAddr_P+(InstructionOffset-startOffset)) );
			rtlglue_printf("flashdrv_write(dst=%p,src=%p,size=0x%08x)\n",pAddrToInject, pStartAddrToBackup, sizeof(bdinfo.BackupInst));
			_flashdrv_write(pAddrToInject, pStartAddrToBackup, sizeof(bdinfo.BackupInst) );
		}
    }
 #endif/*_SUPPORT_LARGE_FLASH_*/
   
    retval = _flashdrv_write(dstAddr_P, srcAddr_P, size);
out:
	flashdrv_mutexUnlock();
	return retval;
}


uint32 flashdrv_read(void *dstAddr_P, void *srcAddr_P, uint32 size)
{
#ifdef _SUPPORT_LARGE_FLASH_
	uint32 startOffset; /* start offset to read */
	uint32 endOffset;   /* end offset to read */
	uint32 InstructionOffset; /* offset of boot instruction */
#endif/*_SUPPORT_LARGE_FLASH_*/

    ASSERT_CSP( srcAddr_P );
    ASSERT_CSP( dstAddr_P );
    
	flashdrv_mutexLock();
    memcpy(dstAddr_P, srcAddr_P, size);

#ifdef _SUPPORT_LARGE_FLASH_
	startOffset = ((uint32)srcAddr_P) - FLASH_BASE;
	endOffset = startOffset + size;
	if ( TRUE == flashdrv_isOver5xBBootInstructions( startOffset, endOffset, &InstructionOffset ) )
	{
		uint32* pStartAddrToRecover;

		pStartAddrToRecover = (uint32*)( ((uint32)dstAddr_P+(InstructionOffset-startOffset)) );
		flashdrv_recoverBootInstructions( pStartAddrToRecover );
	}
#endif/*_SUPPORT_LARGE_FLASH_*/
	flashdrv_mutexUnlock();
    
    return 0;
}


/* Internal Function */
uint32 _flashdrv_write(void *dstAddr_P, void *srcAddr_P, uint32 size)
{
    volatile uint16 *cmdAddr1 = NULL;
    volatile uint16 *cmdAddr2 = NULL;
    volatile uint16 *dstAddr = NULL;
    uint16 *srcAddr;
    uint32 len;
    uint16 pat;
    int32  imask;
    uint32 ChipSeq;
    uint32 BlockSeq;
    int32 ret;
#ifdef _DEBUG_
	uint32 i;
#endif//_DEBUG_
    
    ASSERT_CSP( srcAddr_P );
    ASSERT_CSP( dstAddr_P );

    /* Prevent interrupts interference */
    imask = setIlev(MAX_ILEV);

    /* Check if the total size is larger than flash boundary */
	if ( searchBlockSeq( ((uint32)dstAddr_P + size - 1 ), &ChipSeq, &BlockSeq ) != 0 )
	{
		rtlglue_printf( "[%s:%d] Size is larger than total flash size! dstAddr_P=0x%08x size=0x%08x\n", __FUNCTION__, __LINE__, (uint32)dstAddr_P, size );
		ret = 1;
		goto out;
	}

    dstAddr = (volatile uint16 *) dstAddr_P;
    srcAddr = (uint16 *) srcAddr_P;
    len = ( size + 1 ) / 2; /* 2-byte assigned */
    
#ifdef _DEBUG_
	{
		rtlglue_printf( "Look-ahead(0x%08x):", (uint32)dstAddr );
		for( i=0; i<8; i++ ) rtlglue_printf( "%04x-", *(dstAddr+i) );
		rtlglue_printf( "\n" );
	}
#endif//_DEBUG_

#ifdef _DEBUG_
	rtlglue_printf( "\nflashdrv_write(srcAddr:%p dstAddr:%p len=0x%08x)\n", srcAddr, dstAddr, len );
#endif//_DEBUG_

    while ( len )
    {
    	uint32 Expected, Read;
    	
    	/* kick watchdog */
    	REG32( WDTCNR_ADDR ) |= WDTCLR;

		/* Every 4KB, we update the command address. */
    	if ( cmdAddr1 == NULL ||
    	     ( ((uint32)dstAddr) & 0x00000fff ) == 0x00000000 )
    	{
			if ( searchBlockSeq( (uint32)dstAddr , &ChipSeq, &BlockSeq ) != 0 )
			{
				rtlglue_printf( "searchBlockSeq(0x%08x) failed.\n", (uint32)dstAddr );
                ret = 1;
                goto out;
			}
				
			cmdAddr1 = (void*)( flash_chip_info[ChipSeq].BlockBase + ( 0x555 * 2 ) );
			cmdAddr2 = (void*)( flash_chip_info[ChipSeq].BlockBase + ( 0x2AA * 2 ) );
		}

retry:
		flashdrv_reset( ChipSeq, BlockSeq, dstAddr );

		Expected = *srcAddr;
		Read = *dstAddr;

		if ( Read != 0xffff )
		{
			rtlglue_printf( "Warning! Write flash failed: content of address(0x%08x) is NOT 0xffff (0x%04x), go ahead.\n", (uint32)dstAddr, Read );
		}

#ifdef CONFIG_RTL865X_NOR_CFI
		switch ( flash_chip_info[ChipSeq].commandSet)
		{
			case COMMAND_SET_INTEL:
				/* unlock block */
				*dstAddr = IN28FXXX_READ_ARRAY;
				if ( flash_chip_info[ChipSeq].devId==I28F160C3B ) /* C3 series needs to be unlocked. */
				{
					*dstAddr = IN28FXXX_UNLOCK_BLOCK1;
			    	*dstAddr = IN28FXXX_UNLOCK_BLOCK2;
				}

				/* reset */
				*dstAddr = IN28FXXX_CLEAR_STATUS;
				if ( flash_chip_info[ChipSeq].devId==I28F160C3B ) /* C3 series needs to be unlocked. */
					*dstAddr = IN28FXXX_READ_ARRAY;

				/* protgram */
				*dstAddr = IN28FXXX_PROGRAM;
				*dstAddr = *srcAddr;

				/* read status */
				*dstAddr = IN28FXXX_READ_STATUS;
				break;
			case COMMAND_SET_AMD:
				/* Start program sequence */
				*cmdAddr1 = AM29LVXXX_COMMAND1;
				*cmdAddr2 = AM29LVXXX_COMMAND2;
				*cmdAddr1 = AM29LVXXX_PROGRAM_CMD;
				*dstAddr = *srcAddr;
				break;
			default:
				setIlev( UART0_ILEV );
				rtlglue_printf( "unknown vendorID: %04x.  Halted at %s():%d.\n", flash_chip_info[ChipSeq].vendorId, __FUNCTION__, __LINE__ );
				while(1);
				break;
		}
#else
		switch ( flash_chip_info[ChipSeq].vendorId )
		{
			case VENDOR_AMD:
			case VENDOR_MXIC:
			case VENDOR_FUJI:
			case VENDOR_ST:
			case VENDOR_ATMEL:
			case VENDOR_EON:
				/* Start program sequence */
				*cmdAddr1 = AM29LVXXX_COMMAND1;
				*cmdAddr2 = AM29LVXXX_COMMAND2;
				*cmdAddr1 = AM29LVXXX_PROGRAM_CMD;
				*dstAddr = *srcAddr;
				break;

			case VENDOR_INTEL:
				/* unlock block */
				*dstAddr = IN28FXXX_READ_ARRAY;
				if ( flash_chip_info[ChipSeq].devId==I28F160C3B ) /* C3 series needs to be unlocked. */
				{
					*dstAddr = IN28FXXX_UNLOCK_BLOCK1;
			    	*dstAddr = IN28FXXX_UNLOCK_BLOCK2;
				}

				/* reset */
				*dstAddr = IN28FXXX_CLEAR_STATUS;
				if ( flash_chip_info[ChipSeq].devId==I28F160C3B ) /* C3 series needs to be unlocked. */
					*dstAddr = IN28FXXX_READ_ARRAY;

				/* protgram */
				*dstAddr = IN28FXXX_PROGRAM;
			    *dstAddr = *srcAddr;

				/* read status */
			    *dstAddr = IN28FXXX_READ_STATUS;
				break;
			
			default:
				setIlev( UART0_ILEV );
				rtlglue_printf( "unknown vendorID: %04x.  Halted at %s():%d.\n", flash_chip_info[ChipSeq].vendorId, __FUNCTION__, __LINE__ );
				while(1);
				break;
		}
#endif /* NOR_CFI */

        /* Check Q7 for completion */
        tick_pollStart();
        while ( 1 )
        {
#ifdef CONFIG_RTL865X_NOR_CFI
        	if ( (flash_chip_info[ChipSeq].commandSet == COMMAND_SET_AMD) &&
		    ((pat = *dstAddr) & 0x80) == (*srcAddr & 0x80) ) break;		
        	else if ( (flash_chip_info[ChipSeq].commandSet == COMMAND_SET_INTEL) &&
        	          ( (pat=*dstAddr) & 0x80 ) == 0x80 ) break;
#else
        	if ( ( flash_chip_info[ChipSeq].vendorId == VENDOR_AMD ||
        	       flash_chip_info[ChipSeq].vendorId == VENDOR_ST ||
        	       flash_chip_info[ChipSeq].vendorId == VENDOR_MXIC ||
        	       flash_chip_info[ChipSeq].vendorId == VENDOR_FUJI || 
        	       flash_chip_info[ChipSeq].vendorId == VENDOR_ATMEL || 
		           flash_chip_info[ChipSeq].vendorId == VENDOR_EON )&&
        	     ((pat = *dstAddr) & 0x80) == (*srcAddr & 0x80) ) break;
        	else if ( flash_chip_info[ChipSeq].vendorId == VENDOR_INTEL &&
        	          ( (pat=*dstAddr) & 0x80 ) == 0x80 ) break;
#endif /* NOR_CFI */
        	
            /* Check Q5 for timeout */
            /*if (pat & 0x20)
            {
                rtlglue_printf("\n! Address %x write internal timeout ", (uint32) dstAddr);
        
                setIlev(s);
                ret = 1;
                goto out;
            }*/
            
            if (tick_pollGet10MS() > TIMEOUT_10MS)
            {
                rtlglue_printf("\nWarning! Address 0x%08x write timeout, retry again.\n", (uint32) dstAddr);
                ret = 1;
            
	            setIlev(imask);
	            tick_pollStart();
	            while( tick_pollGet10MS()<1);
	    		imask = setIlev(MAX_ILEV);
	            goto retry;
            }
        }
        
		flashdrv_reset( ChipSeq, BlockSeq, dstAddr );	
		
        /* Check if completed */
        Expected = *srcAddr;
        Read = *dstAddr;
		if ( Read == Expected )
		{
			dstAddr++;
			srcAddr++;
		}
		else
		{
          	rtlglue_printf("\nWarning! Address %x write fatal (Expected:0x%04x, Read:0x%04x), retry again.\n",
          	               (uint32) dstAddr, Expected, Read );
            ret = 1;
            
            setIlev(imask);
            tick_pollStart();
            while( tick_pollGet10MS()<1);
    		imask = setIlev(MAX_ILEV);
            goto retry;
		}
	    
        len--;
    }    
    
#ifdef _DEBUG_
	rtlglue_printf( "\nflashdrv_write(srcAddr:%p dstAddr:%p len=0x%08x) OK\n", srcAddr, dstAddr, len );
#endif//_DEBUG_
	ret = 0;

out:
	flashdrv_reset( ChipSeq, BlockSeq, 0x0 );
    setIlev( imask );

#ifdef _DEBUG_
	{
		rtlglue_printf( "Look-ahead(0x%08x):", (uint32)dstAddr );
		for( i=0; i<8; i++ ) rtlglue_printf( "%04x-", *(dstAddr+i) );
		rtlglue_printf( "\n" );
	}
#endif//_DEBUG_

    return ret;
}

/* External Function */
uint32 flashdrv_write(void *dstAddr_P, void *srcAddr_P, uint32 size)
{
	uint32 ret;

	flashdrv_mutexLock();
	ret = _flashdrv_write( dstAddr_P, srcAddr_P, size );
	flashdrv_mutexUnlock();

	return ret;
}


uint32 flashdrv_getBlockBase( uint32 ChipSeq )
{
	if ( ChipSeq >= flash_num_of_chips ) return 0xffffffff;
	
	return flash_chip_info[ChipSeq].BlockBase;
}

uint32 flashdrv_getBlockOffset( uint32 ChipSeq, uint32 BlockSeq )
{
	if ( ChipSeq >= flash_num_of_chips ) return 0xffffffff;
	if ( BlockSeq >= flash_chip_info[ChipSeq].NumOfBlock ) return 0xffffffff;
	
	return flash_chip_info[ChipSeq].BlockOffset[BlockSeq];
}


/* 
 * Offset is related to flashbase, such as: 0x00004567.
 * blockOffset is returned with related to flashbase, for example: 0x00004000.
 */
int32 flashdrv_getBlockSizeAndBlockOffset( uint32 Offset, uint32 ChipSeq, uint32 *blockNum, uint32* blockOffset, uint32 *blockSize )
{
	uint32 BlockSeq;

	assert( flash_chip_info[ChipSeq].BlockOffset != NULL );

	for( BlockSeq = 0; BlockSeq < flash_chip_info[ChipSeq].NumOfBlock; BlockSeq++ )
	{
		//rtlglue_printf("[%d][%d]: 0x%08x\n", ChipSeq, BlockSeq, blockbase + (uint32)flash_chip_info[ChipSeq].BlockOffset[ BlockSeq + 1 ] );
		if ( (uint32)flash_chip_info[ChipSeq].BlockOffset[ BlockSeq + 1 ] > Offset )
		{
			goto found;
		}
	}
	/* Not found */
	return 1;

found:
	if ( blockNum ) *blockNum = BlockSeq;
	if ( blockOffset ) *blockOffset = flash_chip_info[ChipSeq].BlockOffset[BlockSeq];
	if ( blockSize ) *blockSize = flash_chip_info[ChipSeq].BlockOffset[BlockSeq+1] - flash_chip_info[ChipSeq].BlockOffset[BlockSeq];
	return 0;
}

/*
 *  The following functions are used to return the addresses of BDINFO, CCFG, RUNTIME IMAGE.
 *
 *	These addresses are decided by the flash type and size.
 *  By calling these functions, we no more need compile flags.
 *
 */
uint32 flashdrv_getBoardInfoAddr()
{
	uint32 addr;

	if ( flash_num_of_chips == 0 ) return 0;

	if ( flash_chip_info[0].isBottom )
		addr = flash_chip_info[0].BlockBase + flash_chip_info[0].BoardInfo; // first chip
	else
		addr = flash_chip_info[flash_num_of_chips-1].BlockBase + flash_chip_info[flash_num_of_chips-1].BoardInfo; // last chip

	return addr;
}

uint32 flashdrv_getCcfgImageAddr()
{
	uint32 addr;
	
	if ( flash_num_of_chips == 0 ) return 0;

	if ( flash_chip_info[0].isBottom )
		addr = flash_chip_info[0].BlockBase + flash_chip_info[0].CGIConfig; // first chip
	else
		addr = flash_chip_info[flash_num_of_chips-1].BlockBase + flash_chip_info[flash_num_of_chips-1].CGIConfig; // last chip

	return addr;
}

uint32 flashdrv_getRunImageAddr()
{
	uint32 addr;
	
	if ( flash_num_of_chips == 0 ) return 0;

	addr= flash_chip_info[0].BlockBase + flash_chip_info[0].RuntimeCode; // always return first chip

	return addr;
}

uint32 flashdrv_getFlashBase()
{
	return FLASH_BASE;
}


void flashdrv_updateBdInfo(void *newBdInfo)

{
	void *pSrc = (void*)newBdInfo;
	void *pDst = (void*)FLASH_MAP_BOARD_INFO_ADDR;
	
	//ENTER_CRITICAL(cfgmgrSemId,WAIT_FOREVER); /* enter critical section */
	
	if (flashdrv_updateImg((void *)pSrc, /* srcAddr_P */
    	    	           (void *)pDst, /* dstAddr_P */
        	    	       (uint32)sizeof(bdinfo_t))!=0)
	{
		while(1);
		//EXIT_CRITICAL(cfgmgrSemId); /* exit critical section */
		//CFGMGR_HALT;
	}
	//EXIT_CRITICAL(cfgmgrSemId); /* exit critical section */
} /* end cfgmgr_task */


#ifdef _USER_MODE_FLASHDRV_
/*
 * flashdrv_isLoaderOverwritten() will check if the dedicated programming range will damage loader ?
 * return: TRUE -- Loader will be damaged
 *         FALSE - Loader is safe
 */
int8 flashdrv_isLoaderOverwritten( uint8* dst, uint32 len )
{
	struct LDR_SEG_DESC oldFashion[] =
	{
		{ 0x00000000, 0x00004000 },
		{ 0x00008000, 0x00020000 },
		{ 0x00000000, 0x00000000 }
	};
	struct LDR_SEG_DESC *pLdr;
	int8 ret;

	flashdrv_mutexLock();
	
	pLdr = (struct LDR_SEG_DESC *)(flashdrv_getFlashBase()+0x100 /*offset of .c_data section, defined in loader's gnu.ldr.rom.lnk file. */ );

	rtlglue_printf( "[%s():%d] dst=%p len=%d\n", __FUNCTION__, __LINE__, dst, len );

	if ( pLdr->start == LDR_SEG_DESC_MAGIC &&
	     pLdr->end == LDR_SEG_DESC_MAGIC )
	{
		/* The existed loader in flash is new-fashion loader. */
		pLdr++; /* skip magic number */
	}
	else
	{
		/* segment descriptor of old-falshion loader, for compatibility */
		pLdr = oldFashion;
	}

	/* traverse the whole table to check if loader will be damaged ? */
	while ( pLdr->end )
	{
		rtlglue_printf( "[%s():%d] start=0x%08x  end=0x%08x\n", __FUNCTION__, __LINE__, flashdrv_getFlashBase()+pLdr->start, flashdrv_getFlashBase()+pLdr->end );
		if ( ( (uint32)dst>=(flashdrv_getFlashBase()+pLdr->start) && (uint32)dst<(flashdrv_getFlashBase()+pLdr->end) ) ||
		     ( (uint32)(dst+len-1)>=(flashdrv_getFlashBase()+pLdr->start) && (uint32)(dst+len-1)<(flashdrv_getFlashBase()+pLdr->end) ) )
		{
			rtlglue_printf( "[%s():%d] Loader will be damage!!\n", __FUNCTION__, __LINE__ );
			ret = TRUE;
			goto out;
		}
		pLdr++;
	}

 	rtlglue_printf( "[%s():%d] Loader is safe!!\n", __FUNCTION__, __LINE__ );
	ret = FALSE;
out:
	flashdrv_mutexUnlock();
	return ret;
}
#endif

#define FLASHDRV_MUTEX

/* ===============================================================================
										Mutex-lock-protection
    =============================================================================== */

static int32 flashdrv_mutexLock(void)
{
#ifdef _USER_MODE_FLASHDRV_
	rtl8651_kernelMutexLock( 0xffffffff );
#elif defined(_KERNEL_MODE_FLASHDRV_) /* kernel */
	rtlglue_drvMutexLock();
#else /* loader */
	/* don't do anything */
#endif/*_USER_MODE_FLASHDRV_*/
	return 0;
}

static int32 flashdrv_mutexUnlock(void)
{
#ifdef _USER_MODE_FLASHDRV_
	rtl8651_kernelMutexUnlock( 0xffffffff );
#elif defined(_KERNEL_MODE_FLASHDRV_) /* kernel */
	rtlglue_drvMutexUnlock();
#else /* loader */
	/* don't do anything */
#endif/*_USER_MODE_FLASHDRV_*/
	return 0;
}
