/****************************************************************************
 *
 *  Name:		cnxtflash.h
 *
 *  Description:	Flash programmer header file
 *
 *  Copyright (c) 1999-2002 Conexant Systems, Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *
 ***************************************************************************/
#ifndef _CNXTFLASH_H_
#define _CNXTFLASH_H_

#define START_SIZE				0x50000
#define MAC_SIZE				0xfc		// 180 bytes
#define PARMS_SIZE				0x2000
#define BSP_SIZE				0x170000
#define FFS_SIZE				0x1b0000
#define	READ_ARRAY_INTEL		( UINT16 )0xff
#define PROGRAM_SETUP_INTEL		( UINT16 )0x40
#define ERASE_SETUP_INTEL		( UINT16 )0x20
#define UNLOCK_SETUP			( UINT16 )0x60
#define ERASE_CONFIRM_INTEL		( UINT16 )0xd0
#define READ_STATUS_REG			( UINT16 )0x70
#define CLEAR_STATUS_REG		( UINT16 )0x50
#define WSMS_READY				( UINT16 )( 1 << 7 )
#define ERASE_ERR				( UINT16 )( 1 << 5 )
#define PROG_ERR				( UINT16 )( 1 << 4 )
#define Vpp_ERR					( UINT16 )( 1 << 3 )
#define BLOCK_LOCK_ERR			( UINT16 )( 1 << 1 )

#define FLASH_START_ADDR		( UINT16* )0x400000
#define FLASH_MAC_ADDR			( UINT16* )0x440000
#define FLASH_PARMS_ADDR		( UINT16* )0x40c000
#define FLASH_BSP_ADDR			( UINT16* )0x410000
#define FLASH_FFS_ADDR			( UINT16* )0x450000
#define FLASH_NVRAM_ADDR		( UINT16* )0x450000
#define NVRAM_BLOCK				(UINT32)	0x0
#define FLASH_CHECKSUM_ADDR		( UINT16* )0x450000
#define CHECKSUM_SIZE			(UINT32)	0x0


//These are used for the SST version of Flash
#define p5555Reg				( volatile UINT16* ) 0x40aaaa
#define p2AAAReg				( volatile UINT16* ) 0x405554
#define READ_ARRAY_SST			( UINT16 ) 0xf0
#define ERASE_SETUP_SST			( UINT16 ) 0x80
#define SECTOR_ERASE_CONFIRM	( UINT16 ) 0x30
#define BLOCK_ERASE_CONFIRM		( UINT16 ) 0x50
#define PROGRAM_SETUP_SST		( UINT16 ) 0xa0
#define CMD_AA					( UINT16 ) 0xaa
#define CMD_55					( UINT16 ) 0x55
#define DQ7						( UINT16 ) 0x80
#define TOGGLE_BIT				( UINT16 )( 1 << 6 )

#define MFR_ID_INTEL				0x0089
#define DEVICE_ID_INTELB3_BB_2M		0x8891
#define DEVICE_ID_INTELB3_BB_4M 	0x8897
#define DEVICE_ID_INTELC3_BB_4M 	0x88c5
#define DEVICE_ID_INTELB3_TB_2M		0x8890
#define DEVICE_ID_INTELC3_BB_2M  	0x88c3
#define DEVICE_ID_INTELC3_TB_2M  	0x88c2
#define DEVICE_ID_INTELB3_BB_1M 	0x8893

#define MFR_ID_SST					0x00bf
#define DEVICE_ID_SST_2M			0x2782

#define MFR_ID_AMD					0x0001
#define DEVICE_ID_AMD_TB_2M			0x22c4
#define DEVICE_ID_AMD_BB_2M			0x2249
#define DEVICE_ID_AMD_BB_4M			0x22F9

#define MFR_ID_MXIC					0x00c2
#define DEVICE_ID_MXIC_BB_2M		0x2249
#define DEVICE_ID_MXIC_BB_4M  		0x22A8

#define MFR_ID_FUJITSU				0x0004
#define DEVICE_ID_FUJITSU_BB_2M		0x2249

#define MFR_ID_STM					0x0020
#define DEVICE_ID_STM_TB_2M			0x22c4
#define DEVICE_ID_STM_BB_2M			0x2249

#define MFR_ID_SHARP				0x00b0
#define DEVICE_ID_SHARP_BB_2M		0x00e9

#define MFR_ID_ATMEL				0x001f
#define DEVICE_ID_ATMEL_BB_2M		0x00c0

#define MFR_ID_HYNIX				0x00AD
#define DEVICE_ID_HYNIX_BB_4M		0x227D

typedef enum
{
	INTEL_TYPE_FLASH_BB,		
	SST_TYPE_FLASH_BB_2M,		
	SHARP_TYPE_FLASH_BB_2M,		
	INTEL_TYPE_FLASH_TB_2M,		
	STM_TYPE_FLASH_BB_2M,
	MXIC_TYPE_FLASH_BB_2M,		
	STM_TYPE_FLASH_TB_2M,				
	ATMEL_TYPE_FLASH_BB_2M,
	AMD_TYPE_FLASH_BB_4M,
	MXIC_TYPE_FLASH_BB_4M,
	HYNIX_TYPE_FLASH_BB_4M,
	UNSUPPORTED_FLASH
	
} eFlash;

typedef enum
{
	INTEL,
	SST,
	STM,
	MXIC,
	DONT_ERASE		
} eFlashErase;

#define READ_IDENTIFIER			( UINT16 )0x90

#define SDRAM_START_ADDR		( UINT16* )0x800000
#define SDRAM_MAC_ADDR			( UINT16* )0x840000
#define SDRAM_PARMS_ADDR		( UINT16* )0x80c000
#define SDRAM_BSP_ADDR			( UINT16* )0x810000
#define SDRAM_FFS_ADDR			( UINT16* )0x850000
#define SDRAM_CHECKSUM_ADDR		( UINT16* )0x820000

#define BOOT_BLOCK_END			( UINT16* )0x410000
#define SECOND_BOOT_BLOCK_END	( UINT16* )0x420000
#define MAIN_BLOCK_END			( UINT16* ) 0x5f0000

#define NUM_BOOT_BLOCKS			8

#define pMfrIDReg				( volatile UINT16* )( FLASH_START_ADDR )
#define pDeviceIDReg			( volatile UINT16* )0x400002
#define pCUIReg					( volatile UINT16* )0x400000

#define WSMS_READY				( UINT16 )( 1 << 7 )
#define ERASE_ERR				( UINT16 )( 1 << 5 )
#define PROG_ERR				( UINT16 )( 1 << 4 )
#define Vpp_ERR					( UINT16 )( 1 << 3 )
#define BLOCK_LOCK_ERR			( UINT16 )( 1 << 1 )

#define FLASH_ERR	( UINT16 )( ERASE_ERR | PROG_ERR | Vpp_ERR | BLOCK_LOCK_ERR )
#ifdef CONFIG_BD_TIBURON
#define CNXT_FLASH_SEGMENT_SIZE		2*1024
#else
#define CNXT_FLASH_SEGMENT_SIZE		1024
#endif

#include <asm/arch/cnxtflash.h>

#endif
