/****************************************************************************
*
*	Name:			tesla.c
*
*	Description:	HomePlug support for Emac driver for CX821xx products
*
*	Copyright:		(c) 2001,2002 Conexant Systems Inc.
*					Personal Computing Division
*
*****************************************************************************

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************
*  $Author:   davidsdj  $
*  $Revision:   1.0  $
*  $Modtime:   Mar 19 2003 12:25:22  $
****************************************************************************/

#define TESLA_MODULE

#if defined(VXWORKS_DRIVER) 
#include "protoutils.h"
#include "JxRegSrv.h"
#endif

#include "osutil.h"
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "cnxtEmac.h"
#include <asm/arch/gpio.h>
#include <asm/arch/cnxtflash.h>

#include "tesla.h"

#define TESLA_DEBUG	1

// Tesla setupframe
/****************************************************************************
* int5130SendConfigPacket
*
* Get a transmit buffer, fill it in with configuration data and
* send it to the card
*
****************************************************************************/
#define SIZE_ETHER_ADDRESS       6              // Ethernet/IEEE addresses always 6 bytes long
#define MIN_ETHER_PACKET_SIZE    60             // Minimum size Ethernet packet we can send (including header)
#define MAX_ETHER_PACKET_SIZE    1514           // Maximum size Ethernet packet we can send (including header)
#define INT5130_ENCRYPTION_KEY_LENGTH 8

// Intellon Ethertype
#define INT5130_FRAME_TYPE_OCTET_1 (0x88)
#define INT5130_FRAME_TYPE_OCTET_2 (0x7B)

#define INT5130_INIT_FRAME_ENTRY_COUNT (0x05) //Note - change to 4 if not using LCON
#define INT5130_SET_LOCAL_PARAMETERS                     (0x19)
#define INT5130_SET_NETWORK_ENCRYPTION_KEY (0x04)
#define INT5130_REQUEST_PARAMETERS_AND_STATISTICS 	(0x07)
#define INT5130_SET_TRANSMIT_CHARACTERISTICS (0x1F)
#define INT5130_LOCAL_PARAMETER_LENGTH                   (6)
#define INT5130_SET_TRANSMIT_CHARACTERISTICS_LENGTH (3)

/* Here are the bit masks for the first transmit characteristics octet. */
#define INT5130_LCO_MASK (0x80)
#define INT5130_LCO_ON (0x80)

#define INT5130_ENCF_MASK (0x40)
#define INT5130_ENCF_ON (0x40)

#define INT5130_TXPRIO_MASK (0x30)
#define INT5130_CAP0 (0x00)
#define INT5130_CAP1 (0x10)
#define INT5130_CAP2 (0x20)
#define INT5130_CAP3 (0x30)

#define INT5130_RESPEXP_MASK           (0x08)
#define INT5130_RESPEXP_ON             (0x08)

#define INT5130_TX_CONT_FREE_MASK      (0x04)
#define INT5130_TX_CONT_FREE_ON        (0x04)

#define INT5130_CONT_FREE_TOKEN_MASK   (0x02)
#define INT5130_CONT_FREE_TOKEN_ON		(0x02)

#define INT5130_TX_WITH_TOKEN_MASK     (0x01)
#define INT5130_TX_WITH_TOKEN_ON       (0x01)

/* Here are the bit masks for the second transmit characteristics octet. */
#define INT5130_RETRY_CTL_MASK (0xC0)
#define INT5130_RETRY_CTL_NONE (0x00)

#define INT5130_RETRY_CTL_ONE (0x40)
#define INT5130_RETRY_CTL_ALL (0x80)

#define INT5130_BUFFER_PUT(nValue) *pSendBuf++ = nValue; nSendSize +=1;

#define INT5130_BUFFER_PUT_ADDR(pAddr) MOVE_MEMORY(pSendBuf, pAddr, SIZE_ETHER_ADDRESS); \
	pSendBuf += SIZE_ETHER_ADDRESS; nSendSize += SIZE_ETHER_ADDRESS;

#define INT5130_BUFFER_PUT_KEY(pKey) MOVE_MEMORY( pSendBuf, pKey, INT5130_ENCRYPTION_KEY_LENGTH); \
	pSendBuf += INT5130_ENCRYPTION_KEY_LENGTH; nSendSize += INT5130_ENCRYPTION_KEY_LENGTH;

// This is Tesla Control struct with default values, do not modify it
TeslaControl_Str Default_TeslaControl =
{
	TRUE ,
	{0x4C,0x76,0x4A,0xF8,0xE0,0x12,0xD6,0x47},
	CA1 ,
	TRUE ,
	TX_NORMAL_RETRY ,
	{"HomePlug"} ,
	FALSE ,
	{0,0,0,0,0,0,0,0},
	1 ,
	AFE_ADI ,
	ALG_NONE ,
	120 ,
	{0x00,0x05,0x04,0x0c,0x06,0x80,0x12,0x69,0x13,0x69,0x14,0x69,0x00,0x00,0x00,0x00},
	19
} ;

#if 0
TeslaControl_Str Current_TeslaControl =
{
	FALSE ,
	{0x01,0xf2,0xe3,0xd4,0xc5,0xb6,0xa7,0x98},
	CA1 ,
	TRUE ,
	TX_NORMAL_RETRY ,
	FALSE ,
	{0,0,0,0,0,0,0,0},
	1 ,
	AFE_ADI ,
	ALG_NONE ,
	120 ,
	{0x00,0x05,0x04,0x0c,0x06,0x80,0x12,0x69,0x13,0x69,0x14,0x69,0x00,0x00,0x00,0x00},
	19
} ;
#else
TeslaControl_Str Current_TeslaControl = {0};
#endif


#define MAX_BUF				256

BYTE teslaflashParams[MAX_BUF];


/*******************************************************************************
FUNCTION NAME:
	teslaflashGetParams

ABSTRACT:	
	Get the home plug parameters from the Tesla flash storage segment.

RETURN:
	Status

DETAILS:
*******************************************************************************/

static void teslaflashGetParams( void )
{

	CNXT_FLASH_STATUS_E flashStatus;
	CNXT_FLASH_SEGMENT_E segment = CNXT_TESLA_FLASH_SEGMENT;
	int		i;
	BYTE	*pteslaParams;

	flashStatus = CnxtFlashOpen( segment );

	switch( flashStatus )
	{
		case CNXT_FLASH_DATA_ERROR:
			printk("Flash Data Error: CNXT_FLASH_SEGMENT_%d\n", segment+1 );

			// Get the default home plug device parameters
			pteslaParams = (BYTE *)&Default_TeslaControl;
	
			for( i = 0; i < sizeof(TeslaControl_Str); i++ )
			{
				teslaflashParams[i] = *pteslaParams;
				pteslaParams++;
			}

			// Update the flash device with the default home plug parameters
			flashStatus = CnxtFlashWriteRequest( 
				segment, 
				(UINT16 *)&teslaflashParams[0],
				sizeof(TeslaControl_Str)
			);
			break;

		case CNXT_FLASH_OWNER_ERROR:
			printk("Wrong owner for requested Flash Segment!\n");
			break;

		default:

			CnxtFlashReadRequest( 
				segment, 
				(UINT16 *)&teslaflashParams,
				sizeof(TeslaControl_Str)
			);

			pteslaParams = (BYTE *)&Current_TeslaControl;

			// Update the home plug device parameter structure
			// with the values saved in the flash device.
			for( i = 0; i < sizeof(TeslaControl_Str); i++ )
			{
				*pteslaParams = teslaflashParams[i];
				pteslaParams++;
			}
			printk("Flash Data Success: CNXT_FLASH_SEGMENT_%d\n", segment+1 );
			break;
	}
}


static const UCHAR BroadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/****************************************************************************
* TransmitPriorities
*
* These bit masks are indexed by the Configured Transmit Priority setting.
*
****************************************************************************/

static const UCHAR TransmitPriorities[] = {INT5130_CAP0, INT5130_CAP1, INT5130_CAP2, INT5130_CAP3 };

/****************************************************************************
* RetryControls
*
* These bit masks are indexed by the Configured Retry Control setting.
*
****************************************************************************/
static UCHAR const RetryControls[]      = {INT5130_RETRY_CTL_NONE, INT5130_RETRY_CTL_ONE, INT5130_RETRY_CTL_ALL};

/****************************************************************************
* int5130SendCharacteristicsInit
*
* Construct the send characteristics from the supplied parameters.
*
* SendCharacteristics0 Bits
*        7     (LC0)                Local Consumption only flag (not configurable)
*        6     (ENCF)               Encryption enable flag
*        5-4   (TxPriority)         Transmit Priority (CAP0=00b CAP1=01b CAP2=10b CAP3=11b)
*        3     (RESP_EXP)           Response Expected Flag
*        2     (TX_CONT_FREE)       Transmit contention free period (not configurable)
*        1     (CONT_FREE_TOKEN)    Contention Token (not configurable)
*        0     (TX_WITH_TOKEN)      Transmit with token (not configurable)
*
* SendCharacteristics1 Bits
*        7-6   (RETRY_CTRL)         Retry Control
*        5-0   (RESERVED)           Reserved Bits
*
* SendCharacteristics1 Bits
*        7-0   (ENCRYPT_KEY_SELECT) Which Encryption Key o use (if SendChar0 Bit 7 is set)
*                                   00000000b - Use Default Key
*                                   00000001b - Use Network Key
*
****************************************************************************/
static EMAC_DRV_CTRL *MACAdapter = NULL ;	// TBD

void Emac_TeslaSetup( TESLA_HANDLE Tesla_Adapter, TeslaControl_Str *pTeslaControl )
{

	ADDR_TBL *FreeTxBuffer;

	PUCHAR  pSendBuf ;
	ULONG   nSendSize   = 0;
	UCHAR    cfgSendCharacteristics0 ;    // 0x18,   LCO, EncryptFlag, TxPriority, ResponseExpected, TxToken Stuff
	UCHAR    cfgSendCharacteristics1 ;    // 0x80,   Retry Control 00=NoRetry 01=OneRetry 10=All Retry?
	short	i;

	if( Tesla_Adapter )
	{
		MACAdapter = (EMAC_DRV_CTRL *)Tesla_Adapter ;
	}

	if (pTeslaControl->ChipSelectGpio != -1)
	{
		WriteGPIOData(pTeslaControl->ChipSelectGpio, 1);		// set high for Homeplug MAC select
	}

   // Setup send characteristic 0
	cfgSendCharacteristics0 = TransmitPriorities[pTeslaControl->TransmitPriority] ;
	if ( pTeslaControl->EncryptEnableFlag )
	{
		cfgSendCharacteristics0 |= INT5130_ENCF_MASK;
	}
	if ( pTeslaControl->ResponseExpected )
	{
		cfgSendCharacteristics0 |= INT5130_RESPEXP_ON;
	}

   // Setup send characteristic 1
	cfgSendCharacteristics1 = RetryControls[pTeslaControl->RetryControl] ;

	FreeTxBuffer = Get_Free_Tx_Buffer( MACAdapter) ;
	pSendBuf = (UINT8 *)Packet_Buffer_Virtual_Adr(FreeTxBuffer) ;

//     NdisStallExecution(50);
//     NdisStallExecution(50);

   // Put standard Ethernet header into buffer (dest source type)
   INT5130_BUFFER_PUT_ADDR(BroadcastAddress);

	MAC_Assign_MacAddress( MACAdapter, pSendBuf ) ;
	pSendBuf += SIZE_ETHER_ADDRESS;
	nSendSize += SIZE_ETHER_ADDRESS;

//   INT5130_BUFFER_PUT_ADDR(MACAdapter->node_address);
   
   INT5130_BUFFER_PUT(INT5130_FRAME_TYPE_OCTET_1);
   INT5130_BUFFER_PUT(INT5130_FRAME_TYPE_OCTET_2);
                 
   /* Indicate the number of data entries to follow. */
   INT5130_BUFFER_PUT(INT5130_INIT_FRAME_ENTRY_COUNT);

   /* Construct local parameters frame. */
   INT5130_BUFFER_PUT(INT5130_SET_LOCAL_PARAMETERS);
   INT5130_BUFFER_PUT(INT5130_LOCAL_PARAMETER_LENGTH);

   /* Insert another copy of the MAC address. */
	MAC_Assign_MacAddress( MACAdapter, pSendBuf ) ;
	pSendBuf += SIZE_ETHER_ADDRESS; nSendSize += SIZE_ETHER_ADDRESS;

   /* Construct xmit characteristics frame for local consumption only. */
   INT5130_BUFFER_PUT(INT5130_SET_TRANSMIT_CHARACTERISTICS);
   INT5130_BUFFER_PUT(INT5130_SET_TRANSMIT_CHARACTERISTICS_LENGTH);

   //Tesla - need to construct cfgSendCharacteristics0 from 
   //		RetrievedRegistryValues->EncryptFlag, TxPriority, ResponseExpected, TxToken Stuff
   INT5130_BUFFER_PUT(INT5130_LCO_ON | cfgSendCharacteristics0);
   INT5130_BUFFER_PUT(cfgSendCharacteristics1);
   INT5130_BUFFER_PUT( (UCHAR)pTeslaControl->NetworkKeySelect ) ;

   /* Construct two encryption key frames. */
   /* First, put in the default key. */
   INT5130_BUFFER_PUT(INT5130_SET_NETWORK_ENCRYPTION_KEY);
   INT5130_BUFFER_PUT(INT5130_ENCRYPTION_KEY_LENGTH+1);
   INT5130_BUFFER_PUT(0);  /* Default Key */

	INT5130_BUFFER_PUT_KEY( pTeslaControl->DefaultKeyOverride );
#if 0
	MAC_Assign_MacAddress( MACAdapter, pSendBuf+2 ) ;
	// make DefaultEncryptionKey
	pSendBuf[0] = pSendBuf[6];
	pSendBuf[1] = pSendBuf[7];
	pSendBuf += INT5130_ENCRYPTION_KEY_LENGTH; nSendSize += INT5130_ENCRYPTION_KEY_LENGTH;
#endif

   /* Now the network key. */
   INT5130_BUFFER_PUT(INT5130_SET_NETWORK_ENCRYPTION_KEY);
   INT5130_BUFFER_PUT(INT5130_ENCRYPTION_KEY_LENGTH+1);
   INT5130_BUFFER_PUT( (UCHAR)pTeslaControl->NetworkKeySelect ) ;
	INT5130_BUFFER_PUT_KEY(pTeslaControl->NetworkEncryptionKey);

   /* Construct xmit characteristics frame NOT for local consumption. */
   INT5130_BUFFER_PUT(INT5130_SET_TRANSMIT_CHARACTERISTICS);
   INT5130_BUFFER_PUT(INT5130_SET_TRANSMIT_CHARACTERISTICS_LENGTH);
   INT5130_BUFFER_PUT(cfgSendCharacteristics0);
   INT5130_BUFFER_PUT(cfgSendCharacteristics1);
   INT5130_BUFFER_PUT( (UCHAR)pTeslaControl->NetworkKeySelect ) ;

   // Pad out to minimum packet size
   if (nSendSize < MIN_ETHER_PACKET_SIZE)
   {
      ZERO_MEMORY(pSendBuf, MIN_ETHER_PACKET_SIZE-nSendSize); /* Zero out the padded memory */
      nSendSize = MIN_ETHER_PACKET_SIZE;
	}

	MAC_Send_A_Frame( MACAdapter, nSendSize, FreeTxBuffer, TRUE ) ;

	// Set up AFE if type is defined as ADI in config.reg

	if (pTeslaControl->AfeType == AFE_ADI)
	{
		// configure settings
		for (i = 0; i < pTeslaControl->RegisterConfig[1]; i++)
			Write_ADI_AFE(
				pTeslaControl->RegisterConfig[2*(i+1)],		// reg address
				pTeslaControl->RegisterConfig[2*(i+1)+1],	// value to program
				pTeslaControl->ChipSelectGpio);
	}
}

/****************************************************************************
* End Of Tesla int5130SendConfigPacket
****************************************************************************/

#if 0	// TBD , collect code in this module

//Tesla - Initialize Encryption Keys

	UCHAR bNullKey[INT5130_ENCRYPTION_KEY_LENGTH] = {0};
	UCHAR bNetworkKeyDefault[INT5130_ENCRYPTION_KEY_LENGTH] = {0x01, 0xf2, 0xe3, 0xd4, 0xc5, 0xb6, 0xa7, 0x98};
	#define KEY_IS_NULL(key)  NdisEqualMemory(key, bNullKey, INT5130_ENCRYPTION_KEY_LENGTH)


/****************************************************************************
* int5130EncryptKeysInit
*
* Complete the initialization of our encryption keys, based on what was
* originally read from the registry.
*
* This routine assumes that our network address has already been setup.
*
****************************************************************************/
VOID
EncryptKeysInit(
	IN PULONG SourceValue, //RetrievedRegistryValues[OVERRIDE_DEFAULT_KEY_KEY]
	IN UCHAR DefaultKey[INT5130_ENCRYPTION_KEY_LENGTH], //Adapter->DefaultEncryptionKey
	IN UCHAR NetworkKey[INT5130_ENCRYPTION_KEY_LENGTH], //Adapter->NetworkEncryptionKey
	IN PUCHAR RegistryNetworkAddress
	)
{
	// Check to see if we need to initialize our default key
   if (!SourceValue || KEY_IS_NULL(DefaultKey)) //pCard->cfg.cfgDefaultEncryptKey
		{
		//Do the hash for the default encryption key. We use the lower 16 bits of the MAC addr
		//for the upper 16 bits of 64 and then the entire 48 bits of the MAC addr for the rest.
		NdisMoveMemory(DefaultKey, &RegistryNetworkAddress[4], 2);
		NdisMoveMemory(&DefaultKey[2], RegistryNetworkAddress, 6);
		}
DbgPrint("DefaultKey %x \n", DefaultKey);
DbgPrint("RegistryNetAddr %x \n", RegistryNetworkAddress);
   
	// Check to see if we need to initialize our network key
	if (KEY_IS_NULL(NetworkKey))
		{
		NdisMoveMemory(NetworkKey, bNetworkKeyDefault, INT5130_ENCRYPTION_KEY_LENGTH);
		}
}


//Tesla read encryption key from registry-used in following function
/****************************************************************************
* UniCharToNibble
*
* Convert a unicode character  to a hex nibble.  Return 0xFF if its not a legal
* hex nibble.
*
****************************************************************************/
UCHAR UniCharToNibble(SHORT wNibble)
{
   UCHAR nNibble;

   // We only care about the lower byte
   wNibble &= 0xff;
   
   if (wNibble >= '0' && wNibble <= '9')
      nNibble = wNibble - '0';
   else if (wNibble >= 'A' && wNibble <= 'F')
      nNibble = (wNibble - 'A') + 10;
   else if (wNibble >= 'a' && wNibble <= 'f')
      nNibble = (wNibble - 'a') + 10;
   else
      nNibble = 0xff;

   return nNibble;
}

// Tesla read encryption key from registry
/****************************************************************************
* CfgReadEncryptKey
*
* Read an encryption key from the registery (as a unicode string), and
* convert it to an 8 byte binary array.
*
* NOTE: NOTE: For Win 95 Gold and Win 95 OSR-1, Registry Strings are ASCII (not Unicode)
* If you really want to support these, check if nKeyLength == INT5130_ENCRYPTION_KEY_LENGTH.
* If it is assume its ASCII and proceed accordingly.
*
* All other Windows versions starting with Win 95 OSR-2 store registery strings
* as unicode.
*
* This routine is good for Win 95 OSR-2 and newer versions of Windows O/S.
*
****************************************************************************/
VOID
ReadEncryptKey(
	IN PUNICODE_STRING SourceString,
	IN UCHAR EncryptKey[INT5130_ENCRYPTION_KEY_LENGTH]
	)
{
//   PNDIS_CONFIGURATION_PARAMETER pCfgParam;
	UCHAR	key[INT5130_ENCRYPTION_KEY_LENGTH];
	PUSHORT	pHexString;
	PUCHAR	pHexByte    = key;
	BOOLEAN	bGoodHex    = TRUE;
	UCHAR	nNibbleHi;
	UCHAR	nNibbleLo;
	int		nKeyUniLen;
	int		nKeySize;
	int		i;

	{

   
	nKeyUniLen	= SourceString->Length;
	pHexString	= (PUSHORT)(&(SourceString->Buffer[0]));

	// Don't overflow our key buffer.
	nKeySize = nKeyUniLen / 2;
	if (nKeySize > INT5130_ENCRYPTION_KEY_LENGTH)
		nKeySize = INT5130_ENCRYPTION_KEY_LENGTH;
      
		for (i=0; i<nKeySize; i++)
		{
			nNibbleHi = UniCharToNibble(*pHexString++);
			nNibbleLo = UniCharToNibble(*pHexString++);

			if (nNibbleHi == 0xff || nNibbleLo == 0xff)
			{
				bGoodHex = FALSE;
				break;
			}

			*pHexByte++ = (nNibbleHi << 4) + nNibbleLo;
		}

	if (bGoodHex == TRUE)
		{
			NdisMoveMemory(EncryptKey, key, INT5130_ENCRYPTION_KEY_LENGTH);
		}
	}  
	return;
}


{
   if (RetrievedRegistryValues[ENCRYPT_ENABLE_FLAG_KEY])  //(Adapter->cfgEncryptEnableFlag)
      Adapter->cfgSendCharacteristics0 |= INT5130_ENCF_MASK;
   if (RetrievedRegistryValues[RESPONSE_EXPECTED_KEY])  //(Adapter->cfgResponseExpected)
      Adapter->cfgSendCharacteristics0 |= INT5130_RESPEXP_ON;

   // Setup send characteristic 1
   Adapter->cfgSendCharacteristics1    = RetryControls[RetrievedRegistryValues[RETRY_CONTROL_KEY]]; //RetryControls[Adapter->cfgRetryControl];
}
#endif


// return FALSE if failed (ex: Emac not started)

// TBD : not worked as specified, Kludge for HomePlug only
// TBD : need to move to emac instead of tesla
BOOLEAN Emac_Phy_Attached( Emac_DevType *pPort1, Emac_DevType *pPort2 )
{

	if( MACAdapter )
		*pPort1 = Emac_HomePlug_Phy ;
	else
		*pPort1 = Emac_Ethernet_Phy ;

	*pPort2 = Emac_Ethernet_Phy ;
	return TRUE ;
}


// Save Tesla control specified by TeslaControl into config.reg
// return FALSE : if fail to write
BOOLEAN Emac_TeslaConfigSave( TeslaControl_Str *TeslaControl ) 
{
	Current_TeslaControl = *TeslaControl ;
	Emac_TeslaCurrentConfigSave( NULL) ;

	return TRUE ;
}

// Read the current Tesla control into TeslaControl
// Return TRUE :  If Emac read these settings from config.reg successfully else
// Return FALSE : Emac would still load the default settings into TeslaControl
BOOLEAN Emac_TeslaConfigRead( TeslaControl_Str *TeslaControl ) 
{
	BOOLEAN    bResult = FALSE;

#if OS_FILE_SYSTEM_SUPPORTED
	JX_REG_VAL_DESC PortConfigDesc[] =
	{
		{
			"EncryptEnableFlag",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->EncryptEnableFlag),
			&TeslaControl->EncryptEnableFlag
		},
		{
			"NetworkEncryptionKey",
			JX_REG_VAL_BINARY,
			sizeof(TeslaControl->NetworkEncryptionKey),
			&TeslaControl->NetworkEncryptionKey
		},
		{
			"TransmitPriority",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->TransmitPriority),
			&TeslaControl->TransmitPriority
		},
		{
			"ResponseExpected",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->ResponseExpected),
			&TeslaControl->ResponseExpected
		},
		{
			"RetryControl",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->RetryControl),
			&TeslaControl->RetryControl
		},
		{
			"OverrideDefaultKey",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->OverrideDefaultKey),
			&TeslaControl->OverrideDefaultKey
		},
		{
			"DefaultKeyOverride",
			JX_REG_VAL_BINARY,
			sizeof(TeslaControl->DefaultKeyOverride),
			&TeslaControl->DefaultKeyOverride
		},
		{
			"NetworkKeySelect",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->NetworkKeySelect),
			&TeslaControl->NetworkKeySelect
		},
		{
			"AfeType",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->AfeType),
			&TeslaControl->AfeType
		},
		{
			"OptSettingAlgorithm",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->OptSettingAlgorithm),
			&TeslaControl->OptSettingAlgorithm
		},
		{
			"AlgorithmTime",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->AlgorithmTime),
			&TeslaControl->AlgorithmTime
		},
		{
			"RegisterConfig",
			JX_REG_VAL_BINARY,
			sizeof(TeslaControl->RegisterConfig),
			&TeslaControl->RegisterConfig
		},
		{
			"ChipSelectGpio",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->ChipSelectGpio),
			&TeslaControl->ChipSelectGpio
		}
	};

	JX_REG_KEY_HANDLE   hKey    = NULL;

	*TeslaControl = Default_TeslaControl ;	// set default 

	if((hKey = JxRegOpenKey(NULL, HOMEPLUG_ROOT_KEY)) != NULL)
	{
		bResult = JxRegQueryStruct
		(
			hKey,
			NUMBER_OF_ELEMENTS(PortConfigDesc),
			PortConfigDesc
		);

		JxRegCloseKey(hKey);
	}

	Current_TeslaControl = *TeslaControl ;
#else
	
	teslaflashGetParams();

	// Parameters in the Current_TeslaControl structure were
	// loaded from the data stored in the flash device.
	*TeslaControl = Current_TeslaControl;
	bResult = TRUE;
#endif
	return bResult;
}

BOOLEAN static Emac_TeslaConfigSaveToFile( TeslaControl_Str *TeslaControl )
{
	BOOLEAN    bResult = FALSE;

#if OS_FILE_SYSTEM_SUPPORTED
	struct JX_REG_VAL_DESC PortConfigDesc[] =
	{
		{
			"EncryptEnableFlag",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->EncryptEnableFlag),
			&TeslaControl->EncryptEnableFlag
		},
		{
			"NetworkEncryptionKey",
			JX_REG_VAL_BINARY,
			sizeof(TeslaControl->NetworkEncryptionKey),
			&TeslaControl->NetworkEncryptionKey
		},
		{
			"TransmitPriority",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->TransmitPriority),
			&TeslaControl->TransmitPriority
		},
		{
			"ResponseExpected",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->ResponseExpected),
			&TeslaControl->ResponseExpected
		},
		{
			"RetryControl",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->RetryControl),
			&TeslaControl->RetryControl
		},
		{
			"OverrideDefaultKey",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->OverrideDefaultKey),
			&TeslaControl->OverrideDefaultKey
		},
		{
			"DefaultKeyOverride",
			JX_REG_VAL_BINARY,
			sizeof(TeslaControl->DefaultKeyOverride),
			&TeslaControl->DefaultKeyOverride
		},
		{
			"NetworkKeySelect",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->NetworkKeySelect),
			&TeslaControl->NetworkKeySelect
		},
		{
			"AfeType",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->AfeType),
			&TeslaControl->AfeType
		},
		{
			"OptSettingAlgorithm",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->OptSettingAlgorithm),
			&TeslaControl->OptSettingAlgorithm
		},
		{
			"AlgorithmTime",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->AlgorithmTime),
			&TeslaControl->AlgorithmTime
		},
		{
			"RegisterConfig",
			JX_REG_VAL_BINARY,
			sizeof(TeslaControl->RegisterConfig),
			&TeslaControl->RegisterConfig
		},
		{
			"ChipSelectGpio",
			JX_REG_VAL_DWORD,
			sizeof(TeslaControl->ChipSelectGpio),
			&TeslaControl->ChipSelectGpio
		}
	};

	JX_REG_KEY_HANDLE   hKey    = NULL;

	if((hKey = JxRegCreateKey(NULL, HOMEPLUG_ROOT_KEY)) != NULL)
	{
		bResult = JxRegSetStruct
		(
			hKey,
			NUMBER_OF_ELEMENTS(PortConfigDesc),
			PortConfigDesc
		);

		JxRegCloseKey(hKey);
	}
#endif
	return bResult;
}

// Emac to save the current Tesla Controls into config.reg
// 12/03/01, the function currently called by CfgMgr at Appy time to write 
// trigger : Windweb Savesetting ... ????
// if input Tesla_Adapter is NULL, Emac will setup whatever Homeplug Phy it detect 
void Emac_TeslaCurrentConfigSave( TESLA_HANDLE Tesla_Adapter)
{

	// Tesla_Adapter : only one now so no need to check
	Emac_TeslaConfigSaveToFile( &Current_TeslaControl );
}

#ifdef HOMEPLUG_PHYMAC_OVERFLOW_WORKAROUND

// TBD , allow one HOMEPLUG instance only
// TBD , how about reset 

#define MAC_ADDRESSES_IDENTICAL(a1,a2) (memcmp((UCHAR *)a1,(UCHAR *)a2, 6 ) == 0)   

static ULONG SendStatsRequests = 0 ;
static ULONG StatsRequests = 0 ;
static ULONG StatsRequestsReqDelay = 0 ;

static BOOLEAN TeslaRequireTxDelay = FALSE ;
static ULONG LastTxAck = 0;
static ULONG LastTxCont = 0;

void Emac_TeslaSendStatsRequest(
	TESLA_HANDLE Tesla_Adapter
)
{
	ADDR_TBL *FreeTxBuffer ;
	PUCHAR  pSendBuf ;
	ULONG   nSendSize   = 0;

#if TESLA_DEBUG
	if( MACAdapter != (PMACAPI)Tesla_Adapter )
	{
		printf( "TBD : support only one HOMEPLUG instance\n" ) ;
	}
#endif

#if 0
	if( SendStatsRequests != StatsRequests )
	{
		TeslaRequireTxDelay  = TRUE ;
		SendStatsRequests = StatsRequests ;
#if TESLA_DEBUG
		StatsRequestsReqDelay++ ;
#endif
		return ;
	}
#endif

	FreeTxBuffer = Get_Free_Tx_Buffer( MACAdapter) ;

	if( !FreeTxBuffer)
		return ;

	SendStatsRequests++ ;

	pSendBuf = (U8 *)Packet_Buffer_Virtual_Adr(FreeTxBuffer) ;

	MAC_Assign_MacAddress( MACAdapter, pSendBuf ) ;

	pSendBuf += SIZE_ETHER_ADDRESS; nSendSize += SIZE_ETHER_ADDRESS;

	MAC_Assign_MacAddress( MACAdapter, pSendBuf ) ;

	pSendBuf += SIZE_ETHER_ADDRESS; nSendSize += SIZE_ETHER_ADDRESS;

   INT5130_BUFFER_PUT(INT5130_FRAME_TYPE_OCTET_1);
   INT5130_BUFFER_PUT(INT5130_FRAME_TYPE_OCTET_2);
                 
   /* Indicate the number of data entries to follow. */
   INT5130_BUFFER_PUT(1);

   /* Construct local parameters frame. */
   INT5130_BUFFER_PUT(INT5130_REQUEST_PARAMETERS_AND_STATISTICS) ;
	// length 0
   INT5130_BUFFER_PUT(0) ;

	MAC_Send_A_Frame( MACAdapter, MIN_ETHER_PACKET_SIZE, FreeTxBuffer, TRUE ) ;
}

BOOLEAN Emac_TeslaRequireTxDelay( TESLA_HANDLE Tesla_Adapter )
{

#if TESLA_DEBUG
	if( MACAdapter != (PMACAPI)Tesla_Adapter )
	{
		printf( "TBD : support only one HOMEPLUG instance\n" ) ;
	}
#endif

	return TeslaRequireTxDelay  ;
}


BOOLEAN  Emac_TeslaIsStatsPacket
(
	TESLA_HANDLE	Tesla_Adapter,
	PUCHAR 			DataPacket
) 
{
	ULONG Index;
	ULONG TxAck;
	ULONG TxCont;

#if TESLA_DEBUG
	if( MACAdapter != (PMACAPI)Tesla_Adapter )
	{
		printf( "TBD : support only one HOMEPLUG instance\n" ) ;
	}
#endif

	if ((DataPacket[12] == INT5130_FRAME_TYPE_OCTET_1) && 
	(DataPacket[13] == INT5130_FRAME_TYPE_OCTET_2) &&
	DataPacket[15] == 0x08 ) 	// PARAMETERS_AND_STATISTICS Response
	{
	char SMacAddr[SIZE_ETHER_ADDRESS] ;

		MAC_Assign_MacAddress( MACAdapter, SMacAddr ) ;

		if( MAC_ADDRESSES_IDENTICAL (DataPacket+6, SMacAddr )) 
		{

			StatsRequests++ ;
			TxAck  = (DataPacket[17] << 8) + DataPacket[18];
			TxCont = (DataPacket[23] << 8) + DataPacket[24];
			TxAck  -= LastTxAck; 
			TxCont -= LastTxCont;
			TxAck  -= (TxAck >> 3); 
			if (TxCont > TxAck) {
				TeslaRequireTxDelay = TRUE ;
#if 0	// TESLA_DEBUG
				StatsRequestsReqDelay++ ;

				printf( "SendStatsRequests=%ld, StatsRequests= %ld, StatsRequestsReqDelay =%ld \n",
				SendStatsRequests, StatsRequests, StatsRequestsReqDelay ) ;
#endif
			}
			else {
				TeslaRequireTxDelay = FALSE ;
			}
			LastTxAck =  (DataPacket[17] << 8) + DataPacket[18];
			LastTxCont = (DataPacket[23] << 8) + DataPacket[24];
			return (TRUE);
		}
	}

	// allow all other HOMEPLUG frames to upper for installation or diagnosis application
	return (FALSE);
}

#endif	// HOMEPLUG_PHYMAC_OVERFLOW_WORKAROUND

/******************************************************************************
|
|  Function:    TeslaState
|
|  Description: Sets and Gets whether Tesla exists
|
|  Parameters:  SetState: 1 = turns Tesla state on, 0 = do nothing
|				Tesla state off by default
|
|  Returns:     TRUE (Tesla exists), FALSE (otherwise)
*******************************************************************************/

BOOLEAN TeslaState(short SetState)
{
	static BOOLEAN state = FALSE;

	if (SetState == 1)
	{
		state = TRUE;
	}

	return state;
}

unsigned int *EMAC1_MDIO_REG = (unsigned int *)0x310018;

#define	MDIO_MASK		0x1FL
#define	MDIO_DEFAULT	0x08
#define MDIO_CLK_LO		0x00
#define MDIO_CLK_HI		0x01
#define MDIO_READ_MODE	0x18		// rising edge for ADI + input mode
#define MDIO_READ_BIT	0x02

void Write_ADI_Byte(char RegByte)
{
	short	i;
	DWORD	io_data;
	char	data;
	volatile int	j;

	// Preserve non-MDIO bits in shared MAC_MDIO_REG
	io_data = *EMAC1_MDIO_REG & ~MDIO_MASK;

	for (i = 7; i >= 0; i--)		// sweep across byte (MSB first)
	{
		data = (((RegByte>>i)&1)<<2);
		*EMAC1_MDIO_REG = io_data | MDIO_CLK_LO | data;
		for (j = 10; j; j--);
		*EMAC1_MDIO_REG = io_data | MDIO_CLK_HI | data;
		for (j = 10; j; j--);
		// These 2 for loops generate a 500 kHz clk on MDC with the processor running at 144 MHz
	}
}

char Read_ADI_Byte(void)
{
	short	i;
	DWORD	io_data;
	char	read_data = 0;
	volatile int	j;

	// Preserve non-MDIO bits in shared MAC_MDIO_REG
	io_data = *EMAC1_MDIO_REG & ~MDIO_MASK;

	// Set for read mode
	io_data |= MDIO_READ_MODE;
	*EMAC1_MDIO_REG = io_data;

	for (i = 7; i >= 0; i--)		// sweep across byte (MSB first)
	{
		*EMAC1_MDIO_REG = io_data | MDIO_CLK_LO;
		for (j = 10; j; j--);
		*EMAC1_MDIO_REG = io_data | MDIO_CLK_HI;
		read_data |= ((*EMAC1_MDIO_REG & MDIO_READ_BIT) >> 1) << i;
		for (j = 10; j; j--);
		// These 2 for loops generate a 500 kHz clk on MDC with the processor running at 144 MHz
	}

	return read_data;
}

/******************************************************************************
|
|  Function:    Read_ADI_AFE
|
|  Description: Reads from the ADI AFE control register
|
|  Parameters:  RegisterAddress - register control address
|
|  Returns:     contents of register control at requested address
*******************************************************************************/

char Read_ADI_AFE(char RegisterAddress, short GpioEnable)
{
	char	returnByte;

	if (GpioEnable != -1)
		WriteGPIOData(GpioEnable, 0);

	Write_ADI_Byte(RegisterAddress | 0x80);		// need bit 7 hi to let AFE know this is a read request
	returnByte = Read_ADI_Byte();

	if (GpioEnable != -1)
		WriteGPIOData(GpioEnable, 1);

	*EMAC1_MDIO_REG = (*EMAC1_MDIO_REG & ~MDIO_MASK)|MDIO_DEFAULT;

	return returnByte;
}

/******************************************************************************
|
|  Function:    Write_ADI_AFE
|
|  Description: Writes to the ADI AFE control register
|
|  Parameters:  RegisterAddress - register control address
|				RegisterData - register data
|
|  Returns:     void
*******************************************************************************/

void Write_ADI_AFE(char RegisterAddress, char RegisterData, short GpioEnable)
{
	if (GpioEnable != -1)
		WriteGPIOData(GpioEnable, 0);

	Write_ADI_Byte(RegisterAddress);
	Write_ADI_Byte(RegisterData);

	if (GpioEnable != -1)
		WriteGPIOData(GpioEnable, 1);

	*EMAC1_MDIO_REG = (*EMAC1_MDIO_REG & ~MDIO_MASK)|MDIO_DEFAULT;
}
