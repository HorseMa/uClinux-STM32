/****************************************************************************
* $Header:   S:/Embedded/archives/Linux/BSP_CX821xx/uClinux2.4.6/linux/drivers/net/cnxt_emac/tesla.h-arc   1.0   Mar 19 2003 13:35:54   davidsdj  $
*
*  Copyright(c)2001  Conexant Systems
*
*****************************************************************************/

// Bright Shih, 11/12/2001
#if !defined(TESLA_H_)
#define	TESLA_H_

//#define HOMEPLUG_PHYMAC_OVERFLOW_WORKAROUND 1

//#define HOMEPLUG_GPIO8		1
//#define GPIO8_DEBUG			1

typedef enum def_ACCESS_PRIORITY
{
	CA0 = 0,
	CA1,
	CA2,
	CA3,
} ACCESS_PRIORITY ;

typedef enum def_TX_RETRY_CONTROL
{
	TX_NO_RETRY = 0,
	TX_ONE_RETRY ,
	TX_NORMAL_RETRY ,
} TX_RETRY_CONTROL ;

typedef enum def_AFE_TYPE
{
	AFE_ADI = 0,
	AFE_INTELLON ,
	AFE_UNKNOWN ,
} AFE_TYPE ;

typedef enum def_SETTING_ALGORITHM		// optimum setting algorithm
{
	ALG_NONE = 0,
	ALG_HUNT ,
} SETTING_ALGORITHM ;

typedef struct def_TeslaControl_Str
{
BOOLEAN				EncryptEnableFlag ;		// default FALSE
UCHAR				NetworkEncryptionKey[8];// default  0x01,0xf2,0xe3,0xd4,0xc5, 0xb6, 0xa7,0x98 

ACCESS_PRIORITY		TransmitPriority ;		// default CA1
BOOLEAN				ResponseExpected ;		// default TRUE
TX_RETRY_CONTROL	RetryControl;			// default TX_NORMAL_RETRY

UCHAR				PasswordPlainText[32];

// below hidden from end users
BOOLEAN				OverrideDefaultKey	;	// default FALSE ;
UCHAR				DefaultKeyOverride[8] ;	//	default get from device driver - expose to user only if OverrideDefaultKey is TRUE
DWORD				NetworkKeySelect ;		// 1-255, default NETWORK_KEY 1 ( 0 - use DEFAULT_KEY)

AFE_TYPE			AfeType ;				// default AFE_ADI
SETTING_ALGORITHM	OptSettingAlgorithm ;	// default ALG_HUNT
DWORD				AlgorithmTime ;			// default 120 (seconds)

UCHAR				RegisterConfig[16] ;	// default:
											//		in pairs except for 0 and 1
											//		pairs denote register and value to set
											//		0: 0x00, 0x03 - denotes 3 reg to program
											//		2: 0x04, 0x0c (Reg 0x04 = 0x0c)
											//		4: 0x05, 0xff (Reg 0x05 = 0xff)
											//		6: 0x06, 0x80 (Reg 0x06 = 0x80)
											//      0x00 for the rest since only 3 reg set by default

DWORD				ChipSelectGpio ;		// default 19
} TeslaControl_Str ;

#define TESLA_HANDLE void*

// call Emac to get the HomePlug Handle, 
// return NULL if no HOMEPLUG phy detected
// TESLA_HANDLE Emac_HomePlugHandle(void) ;

typedef enum def_Emac_DevType
{
	Emac_No_Phy = 0,
	Emac_Ethernet_Phy ,
	Emac_HomePlug_Phy ,
	Emac_Hpna_Phy ,
	// future Phy support
} Emac_DevType ;

// return FALSE if failed (ex: Emac not started)
BOOLEAN Emac_Phy_Attached( Emac_DevType *pPort1, Emac_DevType *pPort2 ) ;


// Save Tesla control specified by TeslaControl into config.reg
// return FALSE : if fail to write
BOOLEAN Emac_TeslaConfigSave( TeslaControl_Str *TeslaControl ) ;


// Read the current Tesla control into TeslaControl
// Return TRUE :  If Emac read these settings from config.reg successfully else
// Return FALSE : Emac would still load the default settings into TeslaControl
BOOLEAN Emac_TeslaConfigRead( TeslaControl_Str *TeslaControl ) ;


// Ask Emac to setup Homeplug phy with pTeslaControl
// if input Tesla_Adapter is NULL, Emac will setup whatever Homeplug Phy it detect
void Emac_TeslaSetup( TESLA_HANDLE Tesla_Adapter, TeslaControl_Str *pTeslaControl ) ;


// Emac to save the current Tesla Controls into config.reg
// 12/03/01, the function currently called by CfgMgr at Appy time to write 
// trigger : Windweb Savesetting ... ????
// if input Tesla_Adapter is NULL, Emac will setup whatever Homeplug Phy it detect 
void Emac_TeslaCurrentConfigSave( TESLA_HANDLE Tesla_Adapter) ;


void Write_ADI_AFE(char RegisterAddress, char RegisterData, short GpioEnable);
char Read_ADI_AFE(char RegisterAddress, short GpioEnable);

#if 1		// HOMEPLUG_PHYMAC_OVERFLOW_WORKAROUND
void Emac_TeslaSendStatsRequest(
	TESLA_HANDLE Tesla_Adapter
) ;

BOOLEAN Emac_TeslaRequireTxDelay( TESLA_HANDLE Tesla_Adapter ) ;

BOOLEAN  Emac_TeslaIsStatsPacket (
	TESLA_HANDLE Tesla_Adapter,
	unsigned char * DataPacket ) ;


#define HomePlug_TX_Delay	2	// suggest 2 milli second

#endif

#endif
