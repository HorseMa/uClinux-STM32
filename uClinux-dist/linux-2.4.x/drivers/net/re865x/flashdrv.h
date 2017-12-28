/*
 * Copyright c                Realtek Semiconductor Corporation, 2002
 * All rights reserved.                                                    
 * 
 * $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/Attic/flashdrv.h,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $
 *
 * $Author: davidm $
 *
 * Abstract:
 *
 *   Flash driver header file for export include.
 *
 * $Log: flashdrv.h,v $
 * Revision 1.1.2.1  2007/09/28 14:42:22  davidm
 * #12420
 *
 * Pull in all the appropriate networking files for the RTL8650,  try to
 * clean it up a best as possible so that the files for the build are not
 * all over the tree.  Still lots of crazy dependencies in there though.
 *
 * Revision 1.4  2006/10/10 03:24:31  alva_zhang
 * +: add element in structure bdinfo to record flash size during boot process
 *
 * Revision 1.3  2006/06/15 14:44:07  ghhuang
 * *: Add NOR CFI Support
 *
 * Revision 1.2  2006/05/19 11:47:59  chenyl
 * *: for original SDK framework, system would include "board.h".
 *    In new SDK framework, "board.h" and "rtl_board.h" would be merged and only
 *    use "rtl_board.h" in the feature.
 *
 * Revision 1.1  2006/05/19 05:59:52  chenyl
 * *: modified new SDK framework.
 *
 * Revision 1.6  2006/01/03 06:47:11  alva_zhang
 * add function statement flashdrv_clearbits
 *
 * Revision 1.5  2005/11/21 12:40:55  chenyl
 * *: always turn OFF interrupts when R/W flash
 *
 * Revision 1.4  2005/10/18 15:04:03  chenyl
 * +: add mutex-lock-protection
 *
 * Revision 1.3  2005/07/04 12:26:16  chenyl
 * +: add RFChipID info BDinfo -- because RFChipID is stored in 8185's EEPROM and
 *    there might no EEPROM in 8185.
 *
 * Revision 1.2  2005/04/26 07:31:23  yjlou
 * *: sync with goahead's flashdrv.
 * 	+: declare flashdrv_ignore_largeflash.
 *
 * Revision 1.1  2005/04/19 04:58:15  tony
 * +: BOA web server initial version.
 *
 * Revision 1.21  2005/04/24 03:53:32  yjlou
 * +: add flashdrv_ignore_largeflash to fix loader raw write bug.
 * +: support Spansion model ID.
 *
 */

#ifndef _FLASHDRV_H_
#define _FLASHDRV_H_



#if defined(__KERNEL__) && defined(__linux__)
	/* kernel mode */
	#include "rtl865x/rtl_types.h"
	#include "rtl_flashdrv.h"
	#include <linux/autoconf.h>
	#include "linux/sched.h" /* for jiffies */
	#ifdef CONFIG_RTL865X_NEW_FRAMEWORK
		#include "rtl_board.h"
	#else
		#include "board.h"
	#endif
	#include "rtl865x/asicRegs.h"
	#define _KERNEL_MODE_FLASHDRV_
#else
	#ifdef LOADER
		/* Loader */
		#include "rtl_types.h"
		#include "rtl_flashdrv.h"
		#include <linux/autoconf.h>
		#include "loader.h"
		#include <asicregs.h>
		#define rtlglue_printf printf
		#define _LOADER_MODE_FLASHDRV_
	#else
		/* runtime code */
		#include "rtl_types.h"
		#include "rtl_flashdrv.h"
		#include <version.h>
		#include <linux/autoconf.h>
		#ifdef CONFIG_RTL865X_NEW_FRAMEWORK
			#include "rtl_board.h"
		#else
			#include "board.h"
		#endif
		#include <asicRegs.h>
		#define _USER_MODE_FLASHDRV_
	#endif
#endif



/* Command Definitions 
*/
#define AM29LVXXX_COMMAND_ADDR1         ((volatile uint16 *) (FLASH_BASE + 0x555 * 2))
#define AM29LVXXX_COMMAND_ADDR2         ((volatile uint16 *) (FLASH_BASE + 0x2AA * 2))
#define AM29LVXXX_COMMAND1              0xAA
#define AM29LVXXX_COMMAND2              0x55
#define AM29LVXXX_SECTOR_ERASE_CMD1     0x80
#define AM29LVXXX_SECTOR_ERASE_CMD2     0x30
#define AM29LVXXX_PROGRAM_CMD           0xA0

/* INTEL command set */
#define IN28FXXX_READ_ARRAY             0xFF
#define IN28FXXX_READ_ID                0x90
#define IN28FXXX_READ_STATUS            0x70
#define IN28FXXX_CLEAR_STATUS           0x50
#define IN28FXXX_PROGRAM                0x40
#define IN28FXXX_ERASE_BLOCK1           0x20
#define IN28FXXX_ERASE_BLOCK2           0xD0
#define IN28FXXX_UNLOCK_BLOCK1          0x60
#define IN28FXXX_UNLOCK_BLOCK2          0xD0



/* for using gdb loader, which assumes different flash map than
 * rome loader
 */
//#define GDB_LOADER 



#ifdef CONFIG_RTL865X_CUSTOM_FLASH_MAP
// User has specified flash map in 'make menuconfig'
#define FLASH_MAP_BOARD_INFO_ADDR         (FLASH_BASE+CONFIG_RTL865X_CUSTOM_BDINFO_ADDRESS)
#define FLASH_MAP_BOARD_INFO_SIZE         (CONFIG_RTL865X_CUSTOM_BDINFO_SIZE)
#define FLASH_MAP_CCFG_IMAGE_ADDR         (FLASH_BASE+CONFIG_RTL865X_CUSTOM_CCFG_ADDRESS)
#define FLASH_MAP_CCFG_IMAGE_SIZE         (CONFIG_RTL865X_CUSTOM_CCFG_SIZE)
#define FLASH_MAP_RUN_IMAGE_ADDR          (FLASH_BASE+CONFIG_RTL865X_CUSTOM_RUNTIME_ADDRESS)
#define FLASH_MAP_RUN_IMAGE_SIZE          (CONFIG_RTL865X_CUSTOM_RUNTIME_SIZE)
#else//CONFIG_RTL865X_CUSTOM_FLASH_MAP
// User does not specify. Flash driver will use default value.
#define FLASH_MAP_BOARD_INFO_ADDR         (flashdrv_getBoardInfoAddr())
#define FLASH_MAP_BOARD_INFO_SIZE         (CONFIG_RTL865X_CUSTOM_BDINFO_SIZE)
#define FLASH_MAP_CCFG_IMAGE_ADDR         (flashdrv_getCcfgImageAddr())
#define FLASH_MAP_CCFG_IMAGE_SIZE         (CONFIG_RTL865X_CUSTOM_CCFG_SIZE)
#define FLASH_MAP_RUN_IMAGE_ADDR          (flashdrv_getRunImageAddr())
#define FLASH_MAP_RUN_IMAGE_SIZE          (CONFIG_RTL865X_CUSTOM_RUNTIME_SIZE)
#endif//CONFIG_RTL865X_CUSTOM_FLASH_MAP

//
//	For compatibility between top and bottom flashes,
//	  we must spare an area at 0x4000~0x8000.
//
#define FLASH_ROMCOPYSTART 0x00000
#define FLASH_ROMCOPYPAUSE 0x04000
#define FLASH_ROMCOPYRESUME 0x08000

#ifndef MAX_FLASH_CHIPS /* menuconfig may config this */
	#define MAX_FLASH_CHIPS 2
#endif

#ifdef CONFIG_RTL865X_NOR_CFI
#define COMMAND_SET_INTEL		0x1
#define COMMAND_SET_AMD		0x2
#endif

struct FLASH_CHIP_INFO
{
	const char* Type;
	uint32 ChipSize;			/* total size of this flash chip, ex: 0x100000 for 1MB ... */
	uint32 BlockBase;			/* the start address of this block, ex: 0xbfc00000, 0xbfe00000 ... */
	const uint32 *BlockOffset;	/* sector addresss table */
	uint32 NumOfBlock;			/* number of sector */
	uint16 devId;				/* ID of this flash chip */
	uint16 vendorId;			/* ID of the manufacture */
	uint32 BoardInfo;			/* offset of board info */
	uint32 CGIConfig;			/* offset of web config */
	uint32 RuntimeCode;			/* offset of runtime code */
	uint32 isBottom:1;			/* bottom flash */
	uint32 isTop:1;				/* top flash */
#ifdef CONFIG_RTL865X_NOR_CFI
	uint32 commandSet;		/* command set */
#endif
};

// definition for vendorID
#define VENDOR_UNKNOWN	0x0000
#define VENDOR_AMD		0x0001
#define VENDOR_FUJI		0x0004
#define VENDOR_ST		0x0020
#define VENDOR_MXIC		0x00C2	/* Macronix */
#define VENDOR_INTEL	0x0089
#define VENDOR_EON		0x7f1c
#define VENDOR_ATMEL	0x001F


extern struct FLASH_CHIP_INFO flash_chip_info[MAX_FLASH_CHIPS];
void flashdrv_init(void);
uint32 flashdrv_read(void *dstAddr_P, void *srcAddr_P, uint32 size);
uint32 flashdrv_clearbits(void *dstAddr_P, void *srcAddr_P, uint32 size);
uint32 flashdrv_updateImg(void *srcAddr_P, void *dstAddr_P, uint32 size);
uint32 flashdrv_getBoardInfoAddr(void);
uint32 flashdrv_getCcfgImageAddr(void);
uint32 flashdrv_getRunImageAddr(void);
uint32 flashdrv_getFlashBase(void);

void flashdrv_updateBdInfo(void *newBdInfo);
int8 flashdrv_isLoaderOverwritten( uint8* dst, uint32 len );

/* Loader Segment Descriptor Table */
#define LDR_SEG_DESC_TABLE_OFFSET	0x100      /* offset to flash base */
#define LDR_SEG_DESC_MAGIC			0x57888651 /* magic number */
struct LDR_SEG_DESC
{
	int32 start;
	int32 end;
};

extern uint32 flashdrv_ignore_largeflash;



/****************************************************************
 *  From rtl_bdinfo.h.
 */

#define BDINFO_WLAN_CALIBRATE_PARAM
#ifdef BDINFO_WLAN_CALIBRATE_PARAM
	#define BDINFO_24GRF_CHANNELS		14 /*ch 1...14*/
	#define BDINFO_5GRF_CHANNELS		8  /*ch 32...64*/
	#define BDINFO_TYPE_WLAN			0x1

typedef struct bdinfo_wlan_s {
	uint8	type;
	uint8	version;
	uint16	len; /* len = data len + 2 (for type and version) */
	uint8	RFChipID;
	uint8	mac[6];
	uint8	rxChargePump;
	uint8	txChargePump;
	uint8	cckTxPower[BDINFO_24GRF_CHANNELS][4];
	uint8	txPower[BDINFO_24GRF_CHANNELS+BDINFO_5GRF_CHANNELS][4];
} bdinfo_wlan_t;
#endif

typedef struct bdinfo_s {
	uint8  mac[6];				// mac address
	uint16  chipsize;		//flash chip total size
	//uint32 macNbr;            /* mac address number */
	uint32 ramStartAddress;
	uint32 rootStartOffset;
	uint32 rootMaxSize;
	uint32 bootSequence;		// 0:normal, 1:debug, 2:L2 switch mode(50A), 3:L2 switch mode(50B)
								// 4:hub mode
	uint32 BackupInst[2];       // Backup Instructions stored at 0xbe400000 or 0xbe800000.
	bdinfo_wlan_t  wlan[2];
} bdinfo_t; 

extern bdinfo_t bdinfo;

/* exported function prototypes */
int32 cmdBdinfo(uint32 channel, int32 argc, int8 ** argv);

/* function: bdinfo_getMac
 * return  0  when successful
 *         -1 when fail
 * note: caller should allocate buffer for mac_P
 */
int32 bdinfo_getMac(macaddr_t *mac_P);

/* function: bdinfo_getMacNbr
 * return  0  when successful
 *         -1 when fail
 * note: caller should allocate buffer for pMacNbr
 */
int32 bdinfo_getMacNbr(uint32 * pMacNbr);

/* function: bdinfo_setMac
 * return  0  when successful
 *         -1 when fail
 * note: caller should allocate buffer for mac_P
 */
int32 bdinfo_setMac(macaddr_t *mac_P);

/*  end of rtl_bdinfo.h.
 ********************************************************************
 */

#endif  /* _FLASHDRV_H_ */

