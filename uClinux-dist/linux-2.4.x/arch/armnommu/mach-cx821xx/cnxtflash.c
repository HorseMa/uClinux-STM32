/****************************************************************************
 *
 *  Name:		cnxtflash.c
 *
 *  Description:	P52/Cx8211x flash programmer.
 *			Programs the Intel 28F320B3 and related flashes on board
 *
 *  Copyright (c) 1999-2002 Conexant Systems, Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/
#include <asm/arch/bsptypes.h>
#include "cnxtflash.h"
#include <asm/arch/hardware.h>
#include <linux/kernel.h> // for printk
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h> // for memcpy
#include <asm/system.h>
#include <linux/spinlock.h>

#define SUPPORT_SST_FLASH
#define CNXT_FLASH_TOTAL_SIZE		0x4000		// Total amount of reserved flash (16 Kbytes)
#ifdef CONFIG_BD_TIBURON
#define CNXT_FLASH_WRITE_START		0x0404000	// starting address
#else
#define CNXT_FLASH_WRITE_START		0x040C000	// starting address
#endif
#ifdef CONFIG_BD_TIBURON
#define CNXT_FLASH_MAX_SEGMENTS		8		// Number of segments
#else
#define CNXT_FLASH_MAX_SEGMENTS		16		// Number of segments
#endif
#define UNUSED __attribute__((unused))
#define CNXT_FLASH_DRIVER_VERSION	"001"
#ifndef SUPPORT_SST_FLASH
#define CNXT_FLASH_MIN_BLOCK_SIZE	0x2000	 	// Smallest writable area (8 Kbytes)
#else 
static unsigned long CNXT_FLASH_MIN_BLOCK_SIZE=0x2000;	 	// Smallest writable area (8 Kbytes)
#endif /* SUPPORT_SST_FLASH */
#define CNXT_FLASH_BLOCK_OFFSET		(CNXT_FLASH_TOTAL_SIZE / CNXT_FLASH_MIN_BLOCK_SIZE)

#if 0
#define DPRINTK(format, args...) printk("cnxtflash.c: " format, ##args)
#else
#define DPRINTK(format, args...)
#endif

/*---------------------------------------------------------------------------
 *  Module Function Prototypes
 *-------------------------------------------------------------------------*/
BOOL CnxtFlashWrite( UINT16 * pflashStartAddr, UINT16 * psdramStartAddr, UINT32 size );
BOOL CnxtFlashRead( UINT16 * pflashStartAddr, UINT16 * psdramStartAddr, UINT32 size );
BOOL CnxtFlashProg( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes );
BOOL CnxtFlashVerifyDeviceID( UINT16 *uDeviceID );
BOOL CnxtFlashEraseBlock( volatile UINT16* pBlockAddr, UINT16 *uDeviceID );
BOOL CnxtFlashProgramLocation( volatile UINT16* pSrcAddr, volatile UINT16* pDestAddr, UINT16 *uDeviceIDofFLASH );
void CnxtFlashClearStatusReg( void );
BOOL CnxtFlashWaitForDone( BOOL );
BOOL CnxtFlashVerifyMemory( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes );
void CnxtFlashSetupSST ( void );
void CnxtFlashToggleBit ( void ); 
DWORD CnxtFlashCalcCRC32( BYTE *pData, DWORD dataLength );

#if 0
static CNXT_FLASH_SEGMENT_T cnxtFlashSegments[CNXT_FLASH_MAX_SEGMENTS];
static BYTE cnxtFlashBuffer[CNXT_FLASH_MIN_BLOCK_SIZE];
static BOOL cnxtFlashLocked[CNXT_FLASH_MAX_SEGMENTS];
#else
CNXT_FLASH_SEGMENT_T cnxtFlashSegments[CNXT_FLASH_MAX_SEGMENTS];
#ifdef SUPPORT_SST_FLASH
BYTE cnxtFlashBuffer[0x2000];   //0X2000 must be the max in the min block sizes
#else
BYTE cnxtFlashBuffer[CNXT_FLASH_MIN_BLOCK_SIZE];
#endif /* SUPPORT_SST_FLASH */

BOOL cnxtFlashLocked[CNXT_FLASH_MAX_SEGMENTS];
#endif

#ifdef SUPPORT_SST_FLASH
/*
 *  set    CNXT_FLASH_MIN_BLOCK_SIZE according to the flash id
*/
void __init setBootsectorBlocksize( void )
{
	UINT16 uDeviceID;
	UINT16 uMfrId;
    UINT16 DeviceIDofFLASH;
    UINT16 *uDeviceIDofFLASH=&DeviceIDofFLASH;

	/* Go into "read identifer" mode for the Intel type part*/
	*pCUIReg = READ_IDENTIFIER;

	uDeviceID = *pDeviceIDReg;
	uMfrId	  = *pMfrIDReg;

	/*Check to see if it is an Intel or Sharp flash*/
	switch ( uDeviceID )
	{
		case DEVICE_ID_INTELB3_BB_2M: 
		case DEVICE_ID_INTELC3_BB_2M: 
		case DEVICE_ID_INTELB3_BB_4M: 
		case DEVICE_ID_INTELC3_BB_4M:
		case DEVICE_ID_INTELB3_BB_1M:
			*uDeviceIDofFLASH = INTEL_TYPE_FLASH_BB;
			*pCUIReg = READ_ARRAY_INTEL;
			break;
		
		case DEVICE_ID_SHARP_BB_2M:
			*uDeviceIDofFLASH = SHARP_TYPE_FLASH_BB_2M;
			*pCUIReg = READ_ARRAY_INTEL;
			break;

		case DEVICE_ID_INTELB3_TB_2M: case DEVICE_ID_INTELC3_TB_2M:
			*uDeviceIDofFLASH = INTEL_TYPE_FLASH_TB_2M;
			*pCUIReg = READ_ARRAY_INTEL;
			break;
			
		default:
			*uDeviceIDofFLASH = UNSUPPORTED_FLASH;
			*pCUIReg = READ_ARRAY_INTEL;
			break;
	}

	if( *uDeviceIDofFLASH == UNSUPPORTED_FLASH )
	{

    	*pCUIReg = READ_ARRAY_SST;
    	CnxtFlashSetupSST();
    	*p5555Reg = READ_IDENTIFIER;
    	
    	uDeviceID = *pDeviceIDReg;
    	uMfrId	  = *pMfrIDReg;
    		/* Check to see if it is an SST, ST-Micro, AMD or MXIC flash */
    	switch ( uDeviceID )
    	{
    		case DEVICE_ID_SST_2M:
    			DPRINTK("SST 2MB FLASH device!\n");
#ifdef SUPPORT_SST_FLASH
                CNXT_FLASH_MIN_BLOCK_SIZE = 0x1000;
#endif /* SUPPORT_SST_FLASH */
    			*uDeviceIDofFLASH = SST_TYPE_FLASH_BB_2M;
    			*pCUIReg = READ_ARRAY_SST;    	
    			break;
    		
            case DEVICE_ID_STM_TB_2M:
    			*uDeviceIDofFLASH = STM_TYPE_FLASH_TB_2M;
    			*pCUIReg = READ_ARRAY_SST;    	
    			break;
    
    		case DEVICE_ID_STM_BB_2M:
    			if ( uMfrId == MFR_ID_MXIC)
    				*uDeviceIDofFLASH = MXIC_TYPE_FLASH_BB_2M;
    			else	 
    				*uDeviceIDofFLASH = STM_TYPE_FLASH_BB_2M;
    			*pCUIReg = READ_ARRAY_SST;    	
    			break;
    
    		case DEVICE_ID_AMD_BB_4M:
    			*uDeviceIDofFLASH = AMD_TYPE_FLASH_BB_4M;
    			*pCUIReg = READ_ARRAY_SST;    	
    			break;
    
    		case DEVICE_ID_HYNIX_BB_4M:
    			*uDeviceIDofFLASH = HYNIX_TYPE_FLASH_BB_4M;
    			*pCUIReg = READ_ARRAY_SST;    	
    			break;
    
    		case DEVICE_ID_MXIC_BB_4M:
    			*uDeviceIDofFLASH = MXIC_TYPE_FLASH_BB_4M;
    			*pCUIReg = READ_ARRAY_SST;    	
    			break;
    					
    	// Derek 8.20.2002
    		case DEVICE_ID_ATMEL_BB_2M:
    			*uDeviceIDofFLASH = ATMEL_TYPE_FLASH_BB_2M;
    			*pCUIReg = READ_ARRAY_SST;    	
    			break;
    
    		default:
    			*uDeviceIDofFLASH = UNSUPPORTED_FLASH;
    	    	*pCUIReg = READ_ARRAY_SST;    	
    			break;
    	}
	}

	if(*uDeviceIDofFLASH == UNSUPPORTED_FLASH) 
	{
		printk("Unsupported FLASH device!\n");
	}
}
#endif /* SUPPORT_SST_FLASH */

static int __init UNUSED CnxtFlashInit( void )
{
	int Status = 0;
	int i;
	BYTE  * startOfSegmentAddress = (BYTE *)CNXT_FLASH_WRITE_START;
	DWORD	headerCRC;
	DWORD	headerSize = sizeof( CNXT_FLASH_SEGMENT_T );

	// Substract out the dataLength, headerCRC and dataCRC from CRC32 calulations
	DWORD 	headerDataSize = headerSize - sizeof( DWORD ) - sizeof( DWORD ) - sizeof(DWORD);
	
	printk("Conexant Flash Driver version: %s\n", CNXT_FLASH_DRIVER_VERSION );

	for( i = 0; i < CNXT_FLASH_MAX_SEGMENTS; i++ )
	{

		// Mark each segment as locked.
		cnxtFlashLocked[i] = TRUE;
		
		// Load flash segment headers 
		memcpy( 
			&cnxtFlashSegments[i], 
			startOfSegmentAddress,
			headerSize
		);	
		
		headerCRC = CnxtFlashCalcCRC32( (BYTE *) &cnxtFlashSegments[i], headerDataSize );

		if( headerCRC != cnxtFlashSegments[i].headerCRC )
		{
			printk("Initializing Flash Segment %d\n", i + 1);

			cnxtFlashSegments[i].segmentNumber = i;	
			cnxtFlashSegments[i].segmentSize   = CNXT_FLASH_SEGMENT_SIZE;
			cnxtFlashSegments[i].startFlashAddress = (DWORD)startOfSegmentAddress;
			cnxtFlashSegments[i].startDataAddress = (DWORD)startOfSegmentAddress + headerSize;
			cnxtFlashSegments[i].owner = i;					
			printk("Initializing Flash data Segment %d,  %lX  \n", i + 1,cnxtFlashSegments[i].startDataAddress);

			// Calculate the new headerCRC and save it.
			headerCRC = CnxtFlashCalcCRC32( (BYTE *) &cnxtFlashSegments[i], headerDataSize );
			cnxtFlashSegments[i].headerCRC = headerCRC;
			
			// Store new header info. along with headerCRC to the flash segment.
			CnxtFlashWrite( 
				(UINT16 *)startOfSegmentAddress, 
				(UINT16 *)&cnxtFlashSegments[i], 
				headerSize
			); 

		} 

		startOfSegmentAddress += CNXT_FLASH_SEGMENT_SIZE;

	}

	return Status;
}

static void __exit UNUSED CnxtFlashExit( void )
{
}

// Add CnxtFlashInit to the '__initcall' list so it will
// be called at startup
module_init( CnxtFlashInit );
module_exit( CnxtFlashExit );
/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashOpen
 *
 *  Description:	Opens the specified flash segment only if 'owner'
 *			is correct for the segment and data actually
 *			exist.
 *
 *  Inputs:		
 *			
 *  Outputs:		
 *
 ***************************************************************************/
CNXT_FLASH_STATUS_E CnxtFlashOpen( CNXT_FLASH_SEGMENT_E segment )
{
	CNXT_FLASH_STATUS_E Status = CNXT_FLASH_SUCCESS;
	DWORD	headerSize = sizeof( CNXT_FLASH_SEGMENT_T );
	DWORD   dataCRC;
	ULONG  	offset;

	// Check for a valid segment number.
   	if( segment < CNXT_FLASH_SEGMENT_1 || segment > CNXT_FLASH_SEGMENT_16 )
	{
		printk("Incorrect Flash Segment number!\n" );
		return CNXT_FLASH_DATA_ERROR;
	}

	// Check for owner
	if( segment != cnxtFlashSegments[segment].owner )
	{
		printk("Incorrect owner for Flash Segment!\n" );
		return CNXT_FLASH_OWNER_ERROR;
	}
	
	DPRINTK(
		"CnxtFlashOpen: &cnxtFlashSegments[%d] = %x\n", 
		segment, 
		&cnxtFlashSegments[segment]
	);
	DPRINTK(
		"cnxtFlashSegments[%d].startFlashAddress = %x\n", 
		segment,
		cnxtFlashSegments[segment].startFlashAddress
	);

	// Copy the header from flash.
	memcpy( 
		(UINT16*)&cnxtFlashSegments[segment], 
		(UINT16*)cnxtFlashSegments[segment].startFlashAddress,
		headerSize
	);	
	

	if( cnxtFlashSegments[segment].dataLength > CNXT_FLASH_SEGMENT_SIZE )
		cnxtFlashSegments[segment].dataLength = CNXT_FLASH_SEGMENT_SIZE;
#ifdef SUPPORT_SST_FLASH
    offset = ((segment * CNXT_FLASH_SEGMENT_SIZE)/ CNXT_FLASH_MIN_BLOCK_SIZE + 1) * 
        CNXT_FLASH_MIN_BLOCK_SIZE - CNXT_FLASH_SEGMENT_SIZE;

    if( cnxtFlashSegments[segment].startDataAddress > ( CNXT_FLASH_WRITE_START + offset ) ) {
        return CNXT_FLASH_DATA_ERROR;	
    }
    else {
        // Calculate the new segment data CRC.
        dataCRC = CnxtFlashCalcCRC32( 
            (BYTE *) cnxtFlashSegments[segment].startDataAddress, 
            cnxtFlashSegments[segment].dataLength 
        );
    }
#else
	if( segment > CNXT_FLASH_SEGMENT_8 )
	{	
		offset = CNXT_FLASH_BLOCK_OFFSET * CNXT_FLASH_MIN_BLOCK_SIZE - CNXT_FLASH_SEGMENT_SIZE;
		
		DPRINTK("CNXT_FLASH_BLOCK_OFFSET: %x\n", CNXT_FLASH_BLOCK_OFFSET);
		DPRINTK("CNXT_FLASH_MIN_BLOCK_SIZE: %x\n", CNXT_FLASH_MIN_BLOCK_SIZE);

		DPRINTK("offset: %lx\n", offset);
		DPRINTK("cnxtFlashSegments[segment].startDataAddress: %lx\n",cnxtFlashSegments[segment].startDataAddress);
		DPRINTK("cnxtFlashSegments[segment].startFlashAddress: %lx\n", cnxtFlashSegments[segment].startFlashAddress);
		DPRINTK("CNXT_FLASH_WRITE_START + offset: %lx\n", CNXT_FLASH_WRITE_START + offset);

		if( cnxtFlashSegments[segment].startFlashAddress > (CNXT_FLASH_WRITE_START + offset) ) {
			return CNXT_FLASH_DATA_ERROR;		
        }
		else {
			// Calculate the new segment data CRC.
			dataCRC = CnxtFlashCalcCRC32( 
				(BYTE *) cnxtFlashSegments[segment].startDataAddress, 
				cnxtFlashSegments[segment].dataLength 
			);
		}

	} else {
        if( cnxtFlashSegments[segment].startDataAddress > ( CNXT_FLASH_WRITE_START + offset ) ) {
			return CNXT_FLASH_DATA_ERROR;	
        }
		else {
			// Calculate the new segment data CRC.
			dataCRC = CnxtFlashCalcCRC32( 
				(BYTE *) cnxtFlashSegments[segment].startDataAddress, 
				cnxtFlashSegments[segment].dataLength 
			);

		}
	}
#endif /* SUPPORT_SST_FLASH */
	DPRINTK(
		"cnxtFlashSegments[%d].dataCRC = %x        dataCRC = %x\n",
		segment,
		cnxtFlashSegments[segment].dataCRC,
		dataCRC
	);

	// Check the data CRC
	if( cnxtFlashSegments[segment].dataCRC != dataCRC ){
		#ifdef CONFIG_BD_TIBURON
		printk("Flash data crc error\n");
		Status = CNXT_FLASH_DATA_CRC_ERROR;
		#else
		Status = CNXT_FLASH_DATA_ERROR;
		#endif
	}

	// Mark the segment as unmarked.
	cnxtFlashLocked[segment] = FALSE;
	return Status;
}
/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashReadRequest
 *
 *  Description:	Read Flash API.
 *
 *  Inputs:		
 *
 *  Outputs:		
 *
 ***************************************************************************/
BOOL CnxtFlashReadRequest( 
	CNXT_FLASH_SEGMENT_E segment, 
	UINT16 * psdramStartAddr, 
	UINT32 size 
)
{

// Make sure data pointer is not NULL.
	if( !psdramStartAddr )
		return FALSE;

	// Make sure maximum size is not exceeded.
	if( size > CNXT_FLASH_SEGMENT_SIZE - sizeof( CNXT_FLASH_SEGMENT_T ) )
#ifdef CONFIG_BD_TIBURON	
		size=CNXT_FLASH_SEGMENT_SIZE - sizeof( CNXT_FLASH_SEGMENT_T );
#else
		return FALSE;
#endif
	// Check for a valid segment number.
	if( segment < CNXT_FLASH_SEGMENT_1 || segment > CNXT_FLASH_SEGMENT_16 )
	{
		return FALSE;
	} else {
		// Copy the data from flash.
		memcpy( 
			(UINT16*)psdramStartAddr, 
			(UINT16*)cnxtFlashSegments[segment].startDataAddress,
			size
		);	

		return TRUE;		
	}
}
/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashWriteRequest
 *
 *  Description:	Write Flash API.
 *
 *  Inputs:		
 *
 *  Outputs:		
 *
 ***************************************************************************/
BOOL CnxtFlashWriteRequest( 
	CNXT_FLASH_SEGMENT_E segment, 
	UINT16 * psdramStartAddr, 
	UINT32 size 
)
{
	DWORD dataCRC;
	DWORD headerSize = sizeof( CNXT_FLASH_SEGMENT_T );
	DWORD flashBlockOffset;
	DWORD flashBlock;
	BOOL bSuccess = FALSE;

    // Make sure data pointer is not NULL.
	if( !psdramStartAddr ) {
		return bSuccess;
	}

	// Make sure there is enough room for the data.
	if( size > CNXT_FLASH_SEGMENT_SIZE - sizeof( CNXT_FLASH_SEGMENT_T ) )
	{
#ifdef CONFIG_BD_TIBURON	
		size=CNXT_FLASH_SEGMENT_SIZE - sizeof( CNXT_FLASH_SEGMENT_T );
#else
		return bSuccess;
#endif
	}

	// Check for a valid segment number.
	if( segment < CNXT_FLASH_SEGMENT_1 || segment > CNXT_FLASH_SEGMENT_16 )
	{
		return bSuccess;
	} else {

		// Make sure the segment is not locked.
    	if( cnxtFlashLocked[segment] ) {
			printk( "Flash Segment Locked! (Try opening it first)\n" );
			return FALSE;
		}
    
		if( size > CNXT_FLASH_SEGMENT_SIZE - headerSize )
			return FALSE;
		else
			cnxtFlashSegments[segment].dataLength = size;
	
		// Calculate the new segment data CRC.
		dataCRC = CnxtFlashCalcCRC32( 
			(BYTE *) psdramStartAddr, 
			size 
		);

		cnxtFlashSegments[segment].dataCRC = dataCRC;
		
		DPRINTK(
			"cnxtFlashSegments[%d].dataCRC = %x\n", 
			segment,
			cnxtFlashSegments[segment].dataCRC
		);
		DPRINTK(
			"headerSize = %x\n",
			headerSize
		);


		if( segment > CNXT_FLASH_SEGMENT_8 )
		{
			flashBlock = CNXT_FLASH_SEGMENT_9;
			flashBlockOffset = (segment * CNXT_FLASH_SEGMENT_SIZE) - CNXT_FLASH_MIN_BLOCK_SIZE;
		}
		else
		{
			flashBlock = CNXT_FLASH_SEGMENT_1;
			flashBlockOffset = (segment * CNXT_FLASH_SEGMENT_SIZE);
		}

		DPRINTK("flashBlock = %d\n", flashBlock	);
#ifdef SUPPORT_SST_FLASH
        if (CNXT_FLASH_SEGMENT_SIZE == CNXT_FLASH_MIN_BLOCK_SIZE) {
            flashBlock = segment; 
            flashBlockOffset = 0;
        }
        else {
            //flashblock        - the first segment num of the block which this segment is in
            //flashBlockOffset  - the offset of this segment in the block
            int num_block;   //start from 0   segment start from 0
            int segs_per_block;

            segs_per_block = CNXT_FLASH_MIN_BLOCK_SIZE / (CNXT_FLASH_SEGMENT_SIZE);
            num_block = (segment * CNXT_FLASH_SEGMENT_SIZE)/ CNXT_FLASH_MIN_BLOCK_SIZE;
            flashBlock = num_block * segs_per_block;
			flashBlockOffset = segment * (CNXT_FLASH_SEGMENT_SIZE) -
                num_block * CNXT_FLASH_MIN_BLOCK_SIZE;
/*            printk ("flashBlock2 = %ld, %ld seg=%ld, %ld, %ld, m/s=%ld, %ld\n", flashBlock,  flashBlockOffset,
                    segment, CNXT_FLASH_SEGMENT_SIZE, CNXT_FLASH_MIN_BLOCK_SIZE, 
                    num_block, 
                    segs_per_block);*/
        }
#endif /* SUPPORT_SST_FLASH */
		// Read in the minimum flash block size (8kbytes)
		memcpy
		(
			&cnxtFlashBuffer[0],
			(BYTE *)cnxtFlashSegments[flashBlock].startFlashAddress,
			CNXT_FLASH_MIN_BLOCK_SIZE	
		);

		// Copy the flash segment header
		DPRINTK("flashBlockOffset = %x\n", flashBlockOffset	);

		memcpy
		(
			&cnxtFlashBuffer[flashBlockOffset], 
			&cnxtFlashSegments[segment], 
			headerSize
		);

		// Copy the data
		memcpy
		( 
			&cnxtFlashBuffer[flashBlockOffset + headerSize],
			psdramStartAddr,
			size
		);			
		
		DPRINTK(
			"cnxtFlashSegments[%d].startFlashAddress = %x\n",
			flashBlock,
			cnxtFlashSegments[flashBlock].startFlashAddress
		);
		DPRINTK(
			"CNXT_FLASH_MIN_BLOCK_SIZE: %x\n",
			CNXT_FLASH_MIN_BLOCK_SIZE
		);

		bSuccess = CnxtFlashWrite( 
				(UINT16 *)cnxtFlashSegments[flashBlock].startFlashAddress, 
				(UINT16 *)&cnxtFlashBuffer[0], 
				CNXT_FLASH_MIN_BLOCK_SIZE
		); 

		if( !bSuccess )  {

			return FALSE;
		}

	}

	return TRUE;
}

/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashWrite
 *
 *  Description:	Writes data to flash.
 *
 *  Inputs:		
 *
 *  Outputs:		
 *
 ***************************************************************************/
BOOL CnxtFlashWrite( UINT16 * pflashStartAddr, UINT16 * psdramStartAddr, UINT32 size )
{

	
	BOOL 		bSuccess;

	bSuccess = CnxtFlashProg(
			psdramStartAddr,	// Location of data SDRAM
			pflashStartAddr,	// Location to place data in flash
			size			// The size in bytes
	);

	DPRINTK( "CnxtFlashProg: bSuccess = %d\n", bSuccess );

	return bSuccess;
}


/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashProg( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes )
 *
 *  Description:	Programs the flash
 *
 *  Inputs:		UINT16* pSrcAddr = where data to program is
 *			UINT16* pDestAddr = where to start programming the flash
 *			UINT32 uBytes = number of bytes to program
 *
 *  Outputs:		Returns TRUE if programming is successful, FALSE if not
 *
 ***************************************************************************/

BOOL CnxtFlashProg( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes )
{
	BOOL bSuccess;
	UINT16* pSrcAddrChk;
	UINT16* pDestAddrChk;
	UINT32 uBytesChk;
	UINT16 uDeviceID;

    printk ("CnxtFlashProg src=%x, dst=%x, size=%d\n", 
            pSrcAddr, pDestAddr, uBytes);
	/* Save the addresses and byte count for later validation */
	pSrcAddrChk = pSrcAddr;
	pDestAddrChk = pDestAddr;
	uBytesChk = uBytes;

	/* Make sure the right device is installed */
	bSuccess = CnxtFlashVerifyDeviceID( &uDeviceID );
   	
	// Erase a 8kbyte block beginning at pDestAddr
	bSuccess &= CnxtFlashEraseBlock( pDestAddr, &uDeviceID );

	/* While we haven't finished all locations and there hasn't been an error... */
	while( uBytes && bSuccess )
	{

		/* Program a location */
		bSuccess = CnxtFlashProgramLocation( pSrcAddr, pDestAddr, &uDeviceID );

		/* Update memory pointers and byte counter */
		++pSrcAddr;
		++pDestAddr;
		uBytes -= 2;
	}
	
	/* Verify the data */
	if(bSuccess) bSuccess = CnxtFlashVerifyMemory( pSrcAddrChk, pDestAddrChk, uBytesChk );
	
	return bSuccess;
}


/****************************************************************************
 *
 *  Name:			BOOL CnxtFlashVerifyDeviceID( void )
 *
 *  Description:	Verifies the device ID
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns TRUE if the correct device is installed, FALSE if not
 *
 ***************************************************************************/
BOOL CnxtFlashVerifyDeviceID( UINT16 *uDeviceIDofFLASH )
{
	UINT16 uDeviceID;
	UINT16 uMfrId;
	BOOL   bSuccess;

	/* Go into "read identifer" mode for the Intel type part*/
	*pCUIReg = READ_IDENTIFIER;

	uDeviceID = *pDeviceIDReg;
	uMfrId	  = *pMfrIDReg;

	/*Check to see if it is an Intel or Sharp flash*/
	switch ( uDeviceID )
	{
		case DEVICE_ID_INTELB3_BB_2M: 
		case DEVICE_ID_INTELC3_BB_2M: 
		case DEVICE_ID_INTELB3_BB_4M: 
		case DEVICE_ID_INTELC3_BB_4M:
		case DEVICE_ID_INTELB3_BB_1M:
			*uDeviceIDofFLASH = INTEL_TYPE_FLASH_BB;
			*pCUIReg = READ_ARRAY_INTEL;
			break;
		
		case DEVICE_ID_SHARP_BB_2M:
			*uDeviceIDofFLASH = SHARP_TYPE_FLASH_BB_2M;
			*pCUIReg = READ_ARRAY_INTEL;
			break;

		case DEVICE_ID_INTELB3_TB_2M: case DEVICE_ID_INTELC3_TB_2M:
			*uDeviceIDofFLASH = INTEL_TYPE_FLASH_TB_2M;
			*pCUIReg = READ_ARRAY_INTEL;
			break;
			
		default:
			*uDeviceIDofFLASH = UNSUPPORTED_FLASH;
			*pCUIReg = READ_ARRAY_INTEL;
			break;
	}

	if( *uDeviceIDofFLASH == UNSUPPORTED_FLASH )
	{

	*pCUIReg = READ_ARRAY_SST;
	CnxtFlashSetupSST();
	*p5555Reg = READ_IDENTIFIER;
	
	uDeviceID = *pDeviceIDReg;
	uMfrId	  = *pMfrIDReg;
	
		/* Check to see if it is an SST, ST-Micro, AMD or MXIC flash */
	switch ( uDeviceID )
	{
		case DEVICE_ID_SST_2M:
			DPRINTK("SST 2MB FLASH device!\n");
			*uDeviceIDofFLASH = SST_TYPE_FLASH_BB_2M;
			*pCUIReg = READ_ARRAY_SST;    	
			break;
		
		case DEVICE_ID_STM_TB_2M:
			*uDeviceIDofFLASH = STM_TYPE_FLASH_TB_2M;
			*pCUIReg = READ_ARRAY_SST;    	
			break;

		case DEVICE_ID_STM_BB_2M:
			if ( uMfrId == MFR_ID_MXIC)
				*uDeviceIDofFLASH = MXIC_TYPE_FLASH_BB_2M;
			else	 
				*uDeviceIDofFLASH = STM_TYPE_FLASH_BB_2M;
			*pCUIReg = READ_ARRAY_SST;    	
			break;

		case DEVICE_ID_AMD_BB_4M:
			*uDeviceIDofFLASH = AMD_TYPE_FLASH_BB_4M;
			*pCUIReg = READ_ARRAY_SST;    	
			break;

		case DEVICE_ID_HYNIX_BB_4M:
			*uDeviceIDofFLASH = HYNIX_TYPE_FLASH_BB_4M;
			*pCUIReg = READ_ARRAY_SST;    	
			break;

		case DEVICE_ID_MXIC_BB_4M:
			*uDeviceIDofFLASH = MXIC_TYPE_FLASH_BB_4M;
			*pCUIReg = READ_ARRAY_SST;    	
			break;
					
	// Derek 8.20.2002
		case DEVICE_ID_ATMEL_BB_2M:
			*uDeviceIDofFLASH = ATMEL_TYPE_FLASH_BB_2M;
			*pCUIReg = READ_ARRAY_SST;    	
			break;

		default:
			*uDeviceIDofFLASH = UNSUPPORTED_FLASH;
	    	*pCUIReg = READ_ARRAY_SST;    	
			break;
	}
	}

	if(*uDeviceIDofFLASH == UNSUPPORTED_FLASH) 
	{
	 	bSuccess = FALSE;
		printk("Unsupported FLASH device!\n");
	} else
		bSuccess = TRUE;
	
	return bSuccess;
}


/****************************************************************************
 *
 *  Name:			BOOL CnxtFlashEraseBlock( UINT16* pBlockAddr )
 *
 *  Description:	Erases a block of the flash
 *
 *  Inputs:			UINT16* pBlockAddr = address of the start of the block
 *
 *  Outputs:		none
 *
 ***************************************************************************/

BOOL CnxtFlashEraseBlock( volatile UINT16* pBlockAddr, UINT16 *uDeviceIDofFLASH )
{
	BOOL bSuccess=TRUE;
	eFlashErase EraseProcedure = DONT_ERASE;

	switch ( *uDeviceIDofFLASH )
	{
		case INTEL_TYPE_FLASH_BB:
		case SHARP_TYPE_FLASH_BB_2M:
			if(	(pBlockAddr < BOOT_BLOCK_END) && (((UINT32)pBlockAddr & 0x1fff) == 0x0) )
				EraseProcedure = INTEL;
			else if ( 0x0000 == ( (UINT32)pBlockAddr & 0xffff ) )
				EraseProcedure = INTEL;

			break;	
		
		case MXIC_TYPE_FLASH_BB_4M:
		case AMD_TYPE_FLASH_BB_4M:
			if(	(pBlockAddr < BOOT_BLOCK_END) && (((UINT32)pBlockAddr & 0x1fff) == 0x0) )
				EraseProcedure = STM;		 
			else if ( 0x0000 == ( (UINT32)pBlockAddr & 0xffff ) )
				EraseProcedure = STM;

			break;		

		case SST_TYPE_FLASH_BB_2M:
			DPRINTK("SST_TYPE_FLASH: pBlockAddr = %x\n", pBlockAddr );

			if(	(pBlockAddr < BOOT_BLOCK_END) && (((UINT32)pBlockAddr & 0xfff) == 0x0) )
			{
				DPRINTK("Selecting SST EraseProcedure.\n");
				EraseProcedure = SST;

			} else { 
				if ( 0x0000 == ( (UINT32)pBlockAddr & 0xffff ) )
				{
					DPRINTK("Selecting SST EraseProcedure.\n");
					EraseProcedure = SST;		
				}
			}
			
			break;
		
		case STM_TYPE_FLASH_BB_2M:
		case MXIC_TYPE_FLASH_BB_2M:
		case HYNIX_TYPE_FLASH_BB_4M:
			if( ( ((pBlockAddr < BOOT_BLOCK_END) && ( ( (UINT32)pBlockAddr == (UINT32)0x406000) 
				|| ( ( UINT32 )pBlockAddr == ( UINT32 )0x404000 ) 
				|| ( ( UINT32 )pBlockAddr == ( UINT32 )0x408000 ) )) 
				|| (((UINT32)pBlockAddr & 0xffff) == 0x0000) ) )
				EraseProcedure = STM;

			break;

		case ATMEL_TYPE_FLASH_BB_2M:
			if(	(pBlockAddr < BOOT_BLOCK_END) && (((UINT32)pBlockAddr & 0x1fff) == 0x0) )
				EraseProcedure = STM;
			else if ( 0x418000 == (UINT32)pBlockAddr )
				EraseProcedure = STM;
			else if ( 0x0000 == ( (UINT32)pBlockAddr & 0xffff ) )
				EraseProcedure = STM;		

			break;

		default:
			break;
	}

	switch (EraseProcedure)
	{
		case INTEL:
		{

#if 0
			/*  To unlock the blocks of memory in Intel C3 flash */  
			*pBlockAddr = UNLOCK_SETUP;
			*pBlockAddr = ERASE_CONFIRM_INTEL;
			
			*pCUIReg = READ_ARRAY_INTEL;
			/* Clear the status register */
			CnxtFlashClearStatusReg();

			if( *uDeviceIDofFLASH == SHARP_TYPE_FLASH_BB_2M )
				bSuccess = CnxtFlashWaitForDone(0);
			
			/* Write erase setup, erase confirm, and block address */
			*pBlockAddr = ERASE_SETUP_INTEL;
			*pBlockAddr = ERASE_CONFIRM_INTEL;

			/* Wait for erase complete */
			bSuccess &= CnxtFlashWaitForDone(0);

			/* Return to "read array" mode */
			*pCUIReg = READ_ARRAY_INTEL;
#else
			/* Clear the status register */
			CnxtFlashClearStatusReg();

			/*  To unlock the blocks of memory in Intel C3 flash */  
			*pCUIReg = UNLOCK_SETUP;
			*pBlockAddr = ERASE_CONFIRM_INTEL;
			
			if( *uDeviceIDofFLASH == SHARP_TYPE_FLASH_BB_2M )
				bSuccess = CnxtFlashWaitForDone(0);
			
			/* Write erase setup, erase confirm, and block address */
			*pCUIReg = ERASE_SETUP_INTEL;
			*pBlockAddr = ERASE_CONFIRM_INTEL;
			*pCUIReg = ERASE_CONFIRM_INTEL;

			/* Wait for erase complete */
			bSuccess &= CnxtFlashWaitForDone(0);

			/* Return to "read array" mode */
			*pCUIReg = READ_ARRAY_INTEL;

#endif		
		}
		break;

		case SST:
		{
			CnxtFlashSetupSST();
			*p5555Reg = ERASE_SETUP_SST;
			CnxtFlashSetupSST();

			if(	pBlockAddr >= BOOT_BLOCK_END )
			{
				DPRINTK("SST BLOCK_ERASE_CONFIRM.\n");
				*pBlockAddr = BLOCK_ERASE_CONFIRM;

			} else {
				DPRINTK("SST SECTOR_ERASE_CONFIRM.\n");
				*pBlockAddr = SECTOR_ERASE_CONFIRM;	
				
			}
			while(!(*pBlockAddr & DQ7));
			
			//CnxtFlashToggleBit();

			DPRINTK("SST returning to read array mode.\n");
			/* Return to "read array" mode */
			*pCUIReg = READ_ARRAY_SST;
		}
		break;

		case STM:
		{
			CnxtFlashSetupSST();
			*p5555Reg = ERASE_SETUP_SST;
			CnxtFlashSetupSST();
			
			*pBlockAddr = SECTOR_ERASE_CONFIRM;	
				
			while(!(*pBlockAddr & DQ7));
			//ToggleBit();
			
			/* Return to "read array" mode */
			*pCUIReg = READ_ARRAY_SST;
		}
		break;
		
		default:
			break;	
		
	}

	return bSuccess;
}


/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashProgramLocation( UINT16*, UINT16* pDestAddr )
 *
 *  Description:	Programs a location in the flash
 *
 *  Inputs:		UINT16* pSrcAddr = address of where to get the data
 *			UINT16* pDestAddr = flash address to program
 *
 *  Outputs:		Returns TRUE if programming successful, FALSE if not
 *
 ***************************************************************************/
BOOL CnxtFlashProgramLocation( volatile UINT16* pSrcAddr, volatile UINT16* pDestAddr, UINT16 *uDeviceIDofFLASH )
{
	BOOL bSuccess=TRUE;
			
	if( *uDeviceIDofFLASH == INTEL_TYPE_FLASH_BB 
	|| *uDeviceIDofFLASH == SHARP_TYPE_FLASH_BB_2M 
	|| *uDeviceIDofFLASH == INTEL_TYPE_FLASH_TB_2M  )
	{	/* Clear status register */
		CnxtFlashClearStatusReg();

		if( *uDeviceIDofFLASH == SHARP_TYPE_FLASH_BB_2M )
			/* Wait for reading of status reg */
			bSuccess = CnxtFlashWaitForDone(0);
			
		/* Write program setup */
		*pCUIReg = PROGRAM_SETUP_INTEL;

		/* Program the location */
		*pDestAddr = *pSrcAddr;
		
		/* Wait for program complete */
		bSuccess &= CnxtFlashWaitForDone(0);

		/* Return to "read array" mode */
		*pCUIReg = READ_ARRAY_INTEL;
	}
	else
	{
	
		CnxtFlashSetupSST();
		*p5555Reg = PROGRAM_SETUP_SST;
		*pDestAddr = *pSrcAddr;

		while( *pDestAddr != *pSrcAddr );
		//CnxtFlashToggleBit();

		*pCUIReg = READ_ARRAY_SST;
	
		
	}

	return bSuccess;
}


/****************************************************************************
 *
 *  Name:			void CnxtFlashClearStatusReg( void )
 *
 *  Description:	Clears the status register
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void CnxtFlashClearStatusReg( void )
{
	*pCUIReg = CLEAR_STATUS_REG;
	return;
}

/****************************************************************************
 *
 *  Name:			BOOL CnxtFlashWaitForDone( BOOL )
 *
 *  Description:	Waits for a process to complete, and checks results
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns TRUE if process successful
 *
 ***************************************************************************/
BOOL CnxtFlashWaitForDone( BOOL debugON )
{
	BOOL bSuccess=TRUE;
	UINT16 uStatus;

	/* Write the "read status register" command */
	*pCUIReg = READ_STATUS_REG;

	/* Wait for the operation to finish */
	do
	{
		/* Read the status register */
		uStatus = *pCUIReg;

		/* Wait for the "state machine ready" bit */
	} while( !( uStatus & WSMS_READY ) );


	if( debugON ) 
	{

		printk(
			"uStatus = %x\n",
			uStatus
		);

		uStatus = *pCUIReg;
		
		printk(
			"uStatus = %x\n",
			uStatus
		);

	} 

	/* Check for errors */
	if ( !( uStatus & FLASH_ERR ) )
	{
		bSuccess = TRUE;
	}

	else
	{
		bSuccess = FALSE;
	}

	return bSuccess;
}

void CnxtFlashToggleBit(void)
{
	UINT16 uFirst_Read, uSecond_Read;
	do
	{
		/* Read the status register */
		uFirst_Read = *pCUIReg;
		uSecond_Read = *pCUIReg;

		/* Wait for the "state machine ready" bit */
	} while( ( uFirst_Read & TOGGLE_BIT ) != ( uSecond_Read & TOGGLE_BIT ) );

	return;
}

/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashVerifyMemory( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes )
 *
 *  Description:	Compares memory locations
 *
 *  Inputs:		UINT16* pSrcAddr = source address
 *			UINT16* pDestAddr = destination address
 *			UINT32 uBytes = number of bytes to check
 *
 *  Outputs:		Returns BOOL TRUE if memories are the same, FALSE if not
 *
 ***************************************************************************/

BOOL CnxtFlashVerifyMemory( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes )
{
	BOOL bSuccess;
	bSuccess = TRUE;

	while( uBytes && bSuccess )
	{
		if ( *pDestAddr++ != *pSrcAddr++ )
		{
			bSuccess = FALSE;
		}

		uBytes -= 2;
	}

	return bSuccess;
}

/****************************************************************************
 *
 *  Name:		DWORD CnxtFlashCalcCRC32
 *
 *  Description:	Performs a 32 bit CRC on pData
 *
 *  Inputs:		DWORD		 crc32_val,		
 *			BYTE		*pData,
 *			DWORD		dataLength	
 *			
 *
 *  Outputs:		Returns the calculated CRC value.
 *
 ***************************************************************************/
// CRC32 Lookup Table generated from Charles Michael
//  Heard's CRC-32 code
static DWORD crc32_tbl[256] =
{
0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
0xf0a5bd1d, 0xf464a0aa797, 0xf9278673, 0xfde69bc4,
0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

#define INITIAL_CRC_VAL		0xffffffff  // negative one

DWORD CnxtFlashCalcCRC32
(
	BYTE		*pData,
	DWORD		dataLength	
)
{
	DWORD	crc32_val = INITIAL_CRC_VAL; 

    // Calculate a CRC32 value
	while ( dataLength-- )
	{
		crc32_val = ( crc32_val << 8 ) ^ crc32_tbl[(( crc32_val >> 24) ^ *pData++ ) & 0xff];
	}
	
	return (crc32_val);

}

void CnxtFlashSetupSST(void)
{
	*p5555Reg = CMD_AA;
	*p2AAAReg = CMD_55;

	return;
}

EXPORT_SYMBOL( CnxtFlashWriteRequest );
EXPORT_SYMBOL( CnxtFlashReadRequest );
EXPORT_SYMBOL( CnxtFlashOpen );
