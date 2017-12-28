/******************************************************************************
*
*     Author: Xilinx, Inc.
*     
*     
*     This program is free software; you can redistribute it and/or modify it
*     under the terms of the GNU General Public License as published by the
*     Free Software Foundation; either version 2 of the License, or (at your
*     option) any later version.
*     
*     
*     XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
*     COURTESY TO YOU. BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
*     ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD,
*     XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE
*     FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING
*     ANY THIRD PARTY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
*     XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
*     THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY
*     WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM
*     CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND
*     FITNESS FOR A PARTICULAR PURPOSE.
*     
*     
*     Xilinx hardware products are not intended for use in life support
*     appliances, devices, or systems. Use in such applications is
*     expressly prohibited.
*     
*     
*     (c) Copyright 2002-2004 Xilinx Inc.
*     All rights reserved.
*     
*     
*     You should have received a copy of the GNU General Public License along
*     with this program; if not, write to the Free Software Foundation, Inc.,
*     675 Mass Ave, Cambridge, MA 02139, USA.
*
******************************************************************************/
/*****************************************************************************/
/**
*
* @file xiic.h
*
* XIic is the driver for an IIC master or slave device.

* In order to reduce the memory requirements of the driver it is partitioned
* such that there are optional parts of the driver.  Slave, master, and
* multimaster features are optional such that these files are not required.
* In order to use the slave and multimaster features of the driver, the user
* must call functions (XIic_SlaveInclude and XIic_MultiMasterInclude)
* to dynamically include the code .  These functions may be called at any time.
*
* <b>Bus Throttling</b>
*
* The IIC hardware provides bus throttling which allows either the device, as
* either a master or a slave, to stop the clock on the IIC bus. This feature
* allows the software to perform the appropriate processing for each interrupt
* without an unreasonable response restriction.  With this design, it is
* important for the user to understand the implications of bus throttling.
*
* <b>Repeated Start</b>
*
* An application can send multiple messages, as a master, to a slave device
* and re-acquire the IIC bus each time a message is sent. The repeated start
* option allows the application to send multiple messages without re-acquiring
* the IIC bus for each message. This feature also could cause the application
* to lock up, or monopolize the IIC bus, should repeated start option be
* enabled and sequences of messages never end (periodic data collection).
* Also when repeated start is not disable before the last master message is
* sent or received, will leave the bus captive to the master, but unused.
*
* <b>Addressing</b>
*
* The IIC hardware is parameterized such that it can be built for 7 or 10
* bit addresses. The driver provides the ability to control which address
* size is sent in messages as a master to a slave device.  The address size
* which the hardware responds to as a slave is parameterized as 7 or 10 bits
* but fixed by the hardware build.
*
* Addresses are represented as hex values with no adjustment for the data
* direction bit as the software manages address bit placement. This is
* especially important as the bit placement is not handled the same depending
* on which options are used such as repeated start and 7 vs 10 bit addessing.
*
* <b>Data Rates</b>
*
* The IIC hardware is parameterized such that it can be built to support
* data rates from DC to 400KBit. The frequency of the interrupts which
* occur is proportional to the data rate.
*
* <b>Polled Mode Operation</b>
*
* This driver does not provide a polled mode of operation primarily because
* polled mode which is non-blocking is difficult with the amount of
* interaction with the hardware that is necessary.
*
* <b>Interrupts</b>
*
* The device has many interrupts which allow IIC data transactions as well
* as bus status processing to occur.
*
* The interrupts are divided into two types, data and status. Data interrupts
* indicate data has been received or transmitted while the status interrupts
* indicate the status of the IIC bus. Some of the interrupts, such as Not
* Addressed As Slave and Bus Not Busy, are only used when these specific
* events must be recognized as opposed to being enabled at all times.
*
* Many of the interrupts are not a single event in that they are continuously
* present such that they must be disabled after recognition or when undesired.
* Some of these interrupts, which are data related, may be acknowledged by the
* software by reading or writing data to the appropriate register, or must
* be disabled. The following interrupts can be continuous rather than single
* events.
*   - Data Transmit Register Empty/Transmit FIFO Empty
*   - Data Receive Register Full/Receive FIFO
*   - Transmit FIFO Half Empty
*   - Bus Not Busy
*   - Addressed As Slave
*   - Not Addressed As Slave
*
* The following interrupts are not passed directly to the application thru the
* status callback.  These are only used internally for the driver processing
* and may result in the receive and send handlers being called to indicate
* completion of an operation.  The following interrupts are data related
* rather than status.
*   - Data Transmit Register Empty/Transmit FIFO Empty
*   - Data Receive Register Full/Receive FIFO
*   - Transmit FIFO Half Empty
*   - Slave Transmit Complete
*
* <b>Interrupt To Event Mapping</b>
*
* The following table provides a mapping of the interrupts to the events which
* are passed to the status handler and the intended role (master or slave) for
* the event.  Some interrupts can cause multiple events which are combined
* together into a single status event such as XII_MASTER_WRITE_EVENT and
* XII_GENERAL_CALL_EVENT
* <pre>
* Interrupt                         Event(s)                     Role
*
* Arbitration Lost Interrupt        XII_ARB_LOST_EVENT            Master
* Transmit Error                    XII_SLAVE_NO_ACK_EVENT        Master
* IIC Bus Not Busy                  XII_BUS_NOT_BUSY_EVENT        Master
* Addressed As Slave                XII_MASTER_READ_EVENT,        Slave
*                                   XII_MASTER_WRITE_EVENT,       Slave
*                                   XII_GENERAL_CALL_EVENT        Slave
* </pre>
* <b>Not Addressed As Slave Interrupt</b>
*
* The Not Addressed As Slave interrupt is not passed directly to the
* application thru the status callback.  It is used to determine the end of
* a message being received by a slave when there was no stop condition
* (repeated start).  It will cause the receive handler to be called to
* indicate completion of the operation.
*
* <b>RTOS Independence</b>
*
* This driver is intended to be RTOS and processor independent.  It works
* with physical addresses only.  Any needs for dynamic memory management,
* threads or thread mutual exclusion, virtual memory, or cache control must
* be satisfied by the layer above this driver.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.01a rfp  10/19/01 release
* 1.01c ecm  12/05/02 new rev
* </pre>
*
******************************************************************************/

#ifndef XIIC_H			/* prevent circular inclusions */
#define XIIC_H			/* by using protection macros */

/***************************** Include Files *********************************/

#include "xbasic_types.h"
#include "xstatus.h"
#include "xipif_v1_23_b.h"
#include "xiic_l.h"

/************************** Constant Definitions *****************************/

/** @name Configuration options
 *
 * The following options may be specified or retrieved for the device and
 * enable/disable additional features of the IIC bus.  Each of the options
 * are bit fields such that more than one may be specified.
 * @{
 */
/**
 * <pre>
 * XII_GENERAL_CALL_OPTION      The general call option allows an IIC slave to
 *                              recognized the general call address. The status
 *                              handler is called as usual indicating the device
 *                              has been addressed as a slave with a general
 *                              call. It is the application's responsibility to
 *                              perform any special processing for the general
 *                              call.
 *
 * XII_REPEATED_START_OPTION    The repeated start option allows multiple
 *                              messages to be sent/received on the IIC bus
 *                              without rearbitrating for the bus.  The messages
 *                              are sent as a series of messages such that the
 *                              option must be enabled before the 1st message of
 *                              the series, to prevent an stop condition from
 *                              being generated on the bus, and disabled before
 *                              the last message of the series, to allow the
 *                              stop condition to be generated.
 *
 * XII_SEND_10_BIT_OPTION       The send 10 bit option allows 10 bit addresses
 *                              to be sent on the bus when the device is a
 *                              master. The device can be configured to respond
 *                              as to 7 bit addresses even though it may be
 *                              communicating with other devices that support 10
 *                              bit addresses.  When this option is not enabled,
 *                              only 7 bit addresses are sent on the bus.
 *
 * </pre>
 */
#define XII_GENERAL_CALL_OPTION    0x00000001
#define XII_REPEATED_START_OPTION  0x00000002
#define XII_SEND_10_BIT_OPTION     0x00000004

/*@}*/

/** @name Status events
 *
 * The following status events occur during IIC bus processing and are passed
 * to the status callback. Each event is only valid during the appropriate
 * processing of the IIC bus. Each of these events are bit fields such that
 * more than one may be specified.
 * @{
 */
/**
 * <pre>
 *   XII_BUS_NOT_BUSY_EVENT      bus transitioned to not busy
 *   XII_ARB_LOST_EVENT          arbitration was lost
 *   XII_SLAVE_NO_ACK_EVENT      slave did not acknowledge data (had error)
 *   XII_MASTER_READ_EVENT       master reading from slave
 *   XII_MASTER_WRITE_EVENT      master writing to slave
 *   XII_GENERAL_CALL_EVENT      general call to all slaves
 * </pre>
 */
#define XII_BUS_NOT_BUSY_EVENT   0x00000001
#define XII_ARB_LOST_EVENT       0x00000002
#define XII_SLAVE_NO_ACK_EVENT   0x00000004
#define XII_MASTER_READ_EVENT    0x00000008
#define XII_MASTER_WRITE_EVENT   0x00000010
#define XII_GENERAL_CALL_EVENT   0x00000020
/*@}*/

/* The following address types are used when setting and getting the addresses
 * of the driver. These are mutually exclusive such that only one or the other
 * may be specified.
 */
/** bus address of slave device */
#define XII_ADDR_TO_SEND_TYPE       1
/** this device's bus address when slave */
#define XII_ADDR_TO_RESPOND_TYPE    2

/**************************** Type Definitions *******************************/

/**
 * This typedef contains configuration information for the device.
 */
typedef struct {
	u16 DeviceId;	/**< Unique ID  of device */
	u32 BaseAddress;/**< Device base address */
	u32 Has10BitAddr;
		       /**< does device have 10 bit address decoding */
} XIic_Config;

/**
 * This callback function data type is defined to handle the asynchronous
 * processing of sent and received data of the IIC driver.  The application
 * using this driver is expected to define a handler of this type to support
 * interrupt driven mode. The handlers are called in an interrupt context such
 * that minimal processing should be performed. The handler data type is
 * utilized for both send and receive handlers.
 *
 * @param CallBackRef is a callback reference passed in by the upper layer when
 *        setting the callback functions, and passed back to the upper layer
 *        when the callback is invoked. Its type is unimportant to the driver
 *        component, so it is a void pointer.
 *
 * @param ByteCount indicates the number of bytes remaining to be sent or
 *        received.  A value of zero indicates that the requested number of
 *        bytes were sent or received.
 */
typedef void (*XIic_Handler) (void *CallBackRef, int ByteCount);

/**
 * This callback function data type is defined to handle the asynchronous
 * processing of status events of the IIC driver.  The application using
 * this driver is expected to define a handler of this type to support
 * interrupt driven mode. The handler is called in an interrupt context such
 * that minimal processing should be performed.
 *
 * @param CallBackRef is a callback reference passed in by the upper layer when
 *        setting the callback functions, and passed back to the upper layer
 *        when the callback is invoked. Its type is unimportant to the driver
 *        component, so it is a void pointer.
 *
 * @param StatusEvent indicates one or more status events that occurred.  See
 *        the definition of the status events above.
 */
typedef void (*XIic_StatusHandler) (void *CallBackRef, XStatus StatusEvent);

/**
 * XIic statistics
 */
typedef struct {
	u8 ArbitrationLost;/**< Number of times arbitration was lost */
	u8 RepeatedStarts; /**< Number of repeated starts */
	u8 BusBusy;	   /**< Number of times bus busy status returned */
	u8 RecvBytes;	   /**< Number of bytes received */
	u8 RecvInterrupts; /**< Number of receive interrupts */
	u8 SendBytes;	   /**< Number of transmit bytes received */
	u8 SendInterrupts; /**< Number of transmit interrupts */
	u8 TxErrors;	   /**< Number of transmit errors (no ack) */
	u8 IicInterrupts;  /**< Number of IIC (device) interrupts */
} XIicStats;

/**
 * The XIic driver instance data. The user is required to allocate a
 * variable of this type for every IIC device in the system. A pointer
 * to a variable of this type is then passed to the driver API functions.
 */
typedef struct {
	XIicStats Stats;	/* Statistics                              */
	u32 BaseAddress;	/* Device base address                     */
	u32 Has10BitAddr;	/* TRUE when 10 bit addressing in design  */
	u32 IsReady;		/* Device is initialized and ready         */
	u32 IsStarted;		/* Device has been started                 */
	int AddrOfSlave;	/* Slave addr writing to                   */

	u32 Options;		/* current operating options               */
	u8 *SendBufferPtr;	/* Buffer to send (state)                  */
	u8 *RecvBufferPtr;	/* Buffer to receive (state)               */
	u8 TxAddrMode;		/* State of Tx Address transmission        */
	int SendByteCount;	/* Number of data bytes in buffer (state)  */
	int RecvByteCount;	/* Number of empty bytes in buffer (state) */

	u32 BNBOnly;		/* TRUE when BNB interrupt needs to   */
	/* call callback  */

	XIic_StatusHandler StatusHandler;
	void *StatusCallBackRef;	/* Callback reference for status handler */
	XIic_Handler RecvHandler;
	void *RecvCallBackRef;	/* Callback reference for recv handler */
	XIic_Handler SendHandler;
	void *SendCallBackRef;	/* Callback reference for send handler */

} XIic;

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/*
 * Required functions in xiic.c
 */
XStatus XIic_Initialize(XIic * InstancePtr, u16 DeviceId);

XStatus XIic_Start(XIic * InstancePtr);
XStatus XIic_Stop(XIic * InstancePtr);

void XIic_Reset(XIic * InstancePtr);

XStatus XIic_SetAddress(XIic * InstancePtr, int AddressType, int Address);
u16 XIic_GetAddress(XIic * InstancePtr, int AddressType);

XIic_Config *XIic_LookupConfig(u16 DeviceId);

/*
 * Interrupt (currently required) functions in xiic_intr.c
 */
void XIic_InterruptHandler(void *InstancePtr);
void XIic_SetRecvHandler(XIic * InstancePtr, void *CallBackRef,
			 XIic_Handler FuncPtr);
void XIic_SetSendHandler(XIic * InstancePtr, void *CallBackRef,
			 XIic_Handler FuncPtr);
void XIic_SetStatusHandler(XIic * InstancePtr, void *CallBackRef,
			   XIic_StatusHandler FuncPtr);
/*
 * Master send and receive functions in xiic_master.c
 */
XStatus XIic_MasterRecv(XIic * InstancePtr, u8 * RxMsgPtr, int ByteCount);
XStatus XIic_MasterSend(XIic * InstancePtr, u8 * TxMsgPtr, int ByteCount);

/*
 * Slave send and receive functions in xiic_slave.c
 */
void XIic_SlaveInclude(void);
XStatus XIic_SlaveRecv(XIic * InstancePtr, u8 * RxMsgPtr, int ByteCount);
XStatus XIic_SlaveSend(XIic * InstancePtr, u8 * TxMsgPtr, int ByteCount);

/*
 * Statistics functions in xiic_stats.c
 */
void XIic_GetStats(XIic * InstancePtr, XIicStats * StatsPtr);
void XIic_ClearStats(XIic * InstancePtr);

/*
 * Self test functions in xiic_selftest.c
 */
XStatus XIic_SelfTest(XIic * InstancePtr);

/*
 * Options functions in xiic_options.c
 */
void XIic_SetOptions(XIic * InstancePtr, u32 Options);
u32 XIic_GetOptions(XIic * InstancePtr);

/*
 * Multi-master functions in xiic_multi_master.c
 */
void XIic_MultiMasterInclude(void);

#endif				/* end of protection macro */
