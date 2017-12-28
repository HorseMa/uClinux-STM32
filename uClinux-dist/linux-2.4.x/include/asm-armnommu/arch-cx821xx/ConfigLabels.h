/******************************************************************************
*******************************************************************************
**
**	FILE NAME:
**		ConfigLabels.h
**
**	ABSTRACT:
**		This file contains definitions for the labels from the config file.
**
********************************************************************************
**	Copyright (c) 1997, 1998, 1999, 2000, 2001, 2002, 2003 Conexant Systems, Inc.
**	(formerly Rockwell Semiconductor Systems)
********************************************************************************

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

*******************************************************************************
*******************************************************************************
** KEYWORDS:
**	$Archive:   S:/Embedded/archives/Linux/BSP_CX821xx/uClinux2.4.6/linux/include/asm-armnommu/arch-cx821xx/ConfigLabels.h-arc  $
**	$Revision:   1.1  $
**	$Date:   Jun 30 2003 16:10:56  $
*******************************************************************************
******************************************************************************/

#define	RX_MAX_LATENCY            "RxMaxLatency"
#define	RX_MIN_LATENCY            "RxMinLatency"
#define	TX_MAX_LATENCY            "TxMaxLatency"
#define	TX_MIN_LATENCY            "TxMinLatency"
#define	RX_INTERRUPT_RATE         "RxInterruptRate"
#define	TX_INTERRUPT_RATE         "TxInterruptRate"
#define	RX_SPEED                  "RxSpeed"
#define	TX_SPEED                  "TxSpeed"
#define	RX_FIFO_SIZE              "RxFifoSize"
#define	TX_FIFO_SIZE              "TxFifoSize"
#define	RX_CHANNEL_SIZE           "RxChannelSize"
#define	RX_SEGMENTS               "RxSegments"
#define	TX_CHANNEL_SIZE           "TxChannelSize"
#define	TX_SEGMENTS               "TxSegments"
#define	ADSL_PSD_TEST_MODE        "AdslPsdTestMode"
#define	LINE_PERSISTENT_ACT       "LinePersistentAct"
#define	LINE_PERSISTENCE_TMR      "LinePersistenceTmr"
#define	RX_BUF_ALLOC_LIMIT        "RXBufAllocLimit"
#define	RX_BUF_RATIO_LIMIT        "RXBufRatioLimit"
#define	RX_MAX_FRAME_SIZE         "RxMaxFrameSize"
#define	TX_MAX_FRAME_SIZE         "TxMaxFrameSize"
#define	MAX_TRANSMIT              "MaxTransmit"
#define	RFC_ENCAPSULATION_MODE    "RfcEncapsulationMode"
#define	CELL_BURST_SIZE           "CellBurstSize"
#define	PEAK_CELL_RATE            "PeakCellRate"
#define	CONTROLLER_LOG_ENABLE     "ControllerLogEnable"
#define	CAPABILITIES_G922         "CapabilitiesG922"
#define	CAPABILITIES_G922_ANNEX   "CapabilitiesG922Annex"
#define AUTO_SENSE_HANDSHAKE      "AutoSenseHandshake"
#define AUTO_SENSE_WIRES_FIRST    "AutoSenseWiresFirst"
#define AUTO_WIRING_SELECTION     "AutoWiringSelection"
#define AUTO_WIRING_RELAY_DELAY	  "AutoWiringRelayDelay"
#define AUTO_WIRING_RELAY_ENRG    "AutoWiringRelayEnrg"
#define AUTO_WIRING_NUM_COMBOS    "AutoWiringNumCombos"
#define AUTO_WIRING_COMBO_GPIO    "AutoWiringComboGPIO"
#define AUTO_WIRING_ORDER         "AutoWiringOrder"
#define	LINE_AUTO_ACTIVATION      "LineAutoActivation"
#define	ADSL_HEAD_END             "AdslHeadEnd"
#define	ADSL_HEAD_END_ENVIRONMENT "AdslHeadEndEnvironment"
#define	VENDOR_NEAR_ID            "VendorNearId"
#define	DEBUG_FLAG                "DebugFlag"
#define	API_HWIF_MODE             "ApiHwIfMode"
#define	OVERRIDE_MAC_ADDRESS      "OverrideMacAddress"
#define MAC_ADDRESS               "MACAddress"
#define	AUTOLOG_ENABLED           "AutoLogEnabled"
#define AUTOLOG_PATH              "AutoLogPath"
#define	AUTOLOG_MAX_FILE_SIZE     "AutoLogMaxFileSize"
#define	AUTOLOG_UPDATE_FREQUENCY  "AutoLogUpdateFrequency"
#define	AUTOLOG_OVERWRITE         "AutoLogOverwrite"
#define ARM_HW_ID                 "ARMHwId"
#define ADSL_HW_ID                "ADSLHwId"
#define ARM_FUNCTION_IDENTIFIER   "ARMFunctionIdentifier"
#define ADSL_FUNCTION_IDENTIFIER  "ADSLFunctionIdentifier"
#define HW_DEBUG_FLAG             "HwDebugFlag"
#define PHYSICAL_DRIVER_TYPE      "PhysicalDriverType"
#define GPIO_ADSL_FUNCTION_MASK   "GpioADSLFunctionMask"
#define GPIO_ARM_FUNCTION_MASK    "GpioARMFunctionMask"
#define GPIO_DIRECTION_MASK       "GpioDirectionMask"
#define GPIO_EDGE_MASK            "GpioEdgeMask"
#define FIRMWARE_FILENAME         "FirmwareFilename"
#define ATM_VPI                   "ATM_VPI"
#define ATM_VCI                   "ATM_VCI"
#define SEND_OAM_CELL		  "SendOAMCell"
#define ADSL_OPTIONS		  "ADSL Options"

#define CONFIGURATION_FILENAME    "cnxadsl.conf"
#define CONFIGURATION_FILE_PATH   "/etc/Conexant/"
#define SCRATCH_CONF_NAME         "scrtch$$.conf"

/*
// G.shdsl related labels
*/
#define DOWNLOAD_SOURCE				"DownloadSource"
#define OPERATIONAL_CODE_FILE 		"OperationalCodeFile"

#define TXSOC_WORKAROUND		 	"TXSOCWorkaround"

#define TERMINAL_TYPE				"TerminalType"
#define DSL_SYSTEM_STATE		 	"DslSystemState"

#define DSL_CONFIGURATION			"DslConfiguration"
#define FRAME_STRUCTURE				"FrameStructure"

#define DSL_LINE_CODE				"DslLineCode"
#define BIT_PUMP_TRAINING_MODE		"BitPumpTrainingMode"
#define PSD_MASK					"PSDMask"

#define PCM_TIME_SLOTS				"PCMTimeSlots"
#define DSL_TIME_SLOTS				"DslTimeSlots"
#define OCUPPIED_PCM_TIME_SLOTS		"OccupiedPCMTimeSlots"
#define PCM_TIME_SLOT_START_LOCATION	"PCMTimeSlotStartLoc"
#define IBITS						"IBits"
#define MAINTAIN_DSL_LINK			"MaintainDslLink"
#define MAPPING_MODE				"MappingMode"
#define INTERLEAVE_RATIO			"InterleaveRatio"

#define ATM_ONE_SECOND_MASTER_SLAVE_MODE	"Atm1SecMasterSlaveMode"
#define ATM_SERIAL_INTERFACE_MODE	"AtmSerialInterfaceMode"
#define ATM_RX_INTERFACE_MODE		"AtmRxInterfaceMode"
#define ATM_TX_INTERFACE_MODE		"AtmTxInterfaceMode"
#define ATM_PHY_TC_MODE				"AtmPhyTcMode"
#define ATM_BYTE_ALIGNMENT			"AtmByteAlignment"

#define ATM_PHY_UTOPIA_BUS_WIDTH	"AtmPhyUtopiaBusWidth"
#define ATM_PHY_UTOPIA_MODE			"AtmPhyUtopiaMode"



