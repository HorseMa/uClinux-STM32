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
* @file xiic.c
*
* Contains required functions for the XIic component. See xiic.h for more
* information on the driver.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- --- ------- -----------------------------------------------
* 1.01a rfp  10/19/01 release
* 1.01c ecm  12/05/02 new rev
* 1.01c rmm  05/14/03 Fixed diab compiler warnings relating to asserts.
* </pre>
*
****************************************************************************/

/***************************** Include Files *******************************/

#include "xiic.h"
#include "xiic_i.h"
#include "xio.h"
//#include "xparameters.h"

/************************** Constant Definitions ***************************/

/**************************** Type Definitions *****************************/

/***************** Macros (Inline Functions) Definitions *******************/

/************************** Function Prototypes ****************************/

static void XIic_StubStatusHandler(void *CallBackRef, XStatus ErrorCode);

static void XIic_StubHandler(void *CallBackRef, int ByteCount);

/************************** Variable Definitions **************************/

/*****************************************************************************/
/**
*
* Initializes a specific XIic instance.  The initialization entails:
*
* - Check the device has an entry in the configuration table.
* - Initialize the driver to allow access to the device registers and
*   initialize other subcomponents necessary for the operation of the device.
* - Default options to:
*     - 7-bit slave addressing
*     - Send messages as a slave device
*     - Repeated start off
*     - General call recognition disabled
* - Clear messageing and error statistics
*
* The XIic_Start() function must be called after this function before the device
* is ready to send and receive data on the IIC bus.
*
* Before XIic_Start() is called, the interrupt control must connect the ISR
* routine to the interrupt handler. This is done by the user, and not
* XIic_Start() to allow the user to use an interrupt controller of their choice.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
* @param    DeviceId is the unique id of the device controlled by this XIic
*           instance.  Passing in a device id associates the generic XIic
*           instance to a specific device, as chosen by the caller or
*           application developer.
*
* @return
*
* - XST_SUCCESS when successful
* - XST_DEVICE_IS_STARTED indicates the device is started (i.e. interrupts
*   enabled and messaging is possible). Must stop before re-initialization
*   is allowed.
*
* @note
*
* None.
*
****************************************************************************/
XStatus
XIic_Initialize(XIic * InstancePtr, u16 DeviceId)
{
	XIic_Config *IicConfigPtr;	/* Pointer to configuration data */

	/*
	 * Asserts test the validity of selected input arguments.
	 */
	XASSERT_NONVOID(InstancePtr != NULL);

	InstancePtr->IsReady = 0;

	/*
	 * If the device is started, disallow the initialize and return a Status
	 * indicating it is started.  This allows the user to stop the device
	 * and reinitialize, but prevents a user from inadvertently initializing
	 */
	if (InstancePtr->IsStarted == XCOMPONENT_IS_STARTED) {
		return XST_DEVICE_IS_STARTED;
	}

	/*
	 * Lookup the device configuration in the temporary CROM table. Use this
	 * configuration info down below when initializing this component.
	 */
	IicConfigPtr = XIic_LookupConfig(DeviceId);
	if (IicConfigPtr == NULL) {
		return XST_DEVICE_NOT_FOUND;
	}
	/*
	 * Set default values, including setting the callback
	 * handlers to stubs so the system will not crash should the
	 * application not assign its own callbacks.
	 */
	InstancePtr->IsStarted = 0;
	InstancePtr->BaseAddress = IicConfigPtr->BaseAddress;
	InstancePtr->RecvHandler = XIic_StubHandler;
	InstancePtr->SendHandler = XIic_StubHandler;
	InstancePtr->StatusHandler = XIic_StubStatusHandler;
	InstancePtr->Has10BitAddr = IicConfigPtr->Has10BitAddr;
	InstancePtr->IsReady = XCOMPONENT_IS_READY;
	InstancePtr->Options = 0;
	InstancePtr->BNBOnly = FALSE;

	/*
	 * Reset the device so it's in the reset state, this must be after the
	 * IPIF is initialized since it resets thru the IPIF and clear the stats
	 */
	XIic_Reset(InstancePtr);

	XIIC_CLEAR_STATS(InstancePtr);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function starts the IIC device and driver by enabling the proper
* interrupts such that data may be sent and received on the IIC bus.
* This function must be called before the functions to send and receive data.
*
* Before XIic_Start() is called, the interrupt control must connect the ISR
* routine to the interrupt handler. This is done by the user, and not
* XIic_Start() to allow the user to use an interrupt controller of their choice.
*
* Start enables:
*  - IIC device
*  - Interrupts:
*     - Addressed as slave to allow messages from another master
*     - Arbitration Lost to detect Tx arbitration errors
*     - Global IIC interrupt within the IPIF interface
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
*
* @return
*
* XST_SUCCESS always
*
* @note
*
* The device interrupt is connected to the interrupt controller, but no
* "messaging" interrupts are enabled. Addressed as Slave is enabled to
* reception of messages when this devices address is written to the bus.
* The correct messaging interrupts are enabled when sending or receiving
* via the IicSend() and IicRecv() functions. No action is required
* by the user to control any IIC interrupts as the driver completely
* manages all 8 interrupts. Start and Stop control the ability
* to use the device. Stopping the device completely stops all device
* interrupts from the processor.
*
****************************************************************************/
XStatus
XIic_Start(XIic * InstancePtr)
{
	XASSERT_NONVOID(InstancePtr != NULL);
	XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

	/*
	 * Mask off all interrupts, each is enabled when needed.
	 */
	XIIF_V123B_WRITE_IIER(InstancePtr->BaseAddress, 0);

	/*
	 * Clear all interrupts by reading and rewriting exact value back.
	 * Only those bits set will get written as 1 (writing 1 clears intr)
	 */
	XIic_mClearIntr(InstancePtr->BaseAddress, 0xFFFFFFFF);

	/*
	 * Enable the device
	 */
	XIo_Out8(InstancePtr->BaseAddress + XIIC_CR_REG_OFFSET,
		 XIIC_CR_ENABLE_DEVICE_MASK);
	/*
	 * Set Rx FIFO Occupancy depth to throttle at first byte(after reset = 0)
	 */
	XIo_Out8(InstancePtr->BaseAddress + XIIC_RFD_REG_OFFSET, 0);

	/*
	 * Clear and enable the interrupts needed
	 */
	XIic_mClearEnableIntr(InstancePtr->BaseAddress,
			      XIIC_INTR_AAS_MASK | XIIC_INTR_ARB_LOST_MASK);

	InstancePtr->IsStarted = XCOMPONENT_IS_STARTED;

	/* Enable all interrupts by the global enable in the IPIF */

	XIIF_V123B_GINTR_ENABLE(InstancePtr->BaseAddress);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function stops the IIC device and driver such that data is no longer
* sent or received on the IIC bus. This function stops the device by
* disabling interrupts. This function only disables interrupts within the
* device such that the caller is responsible for disconnecting the interrupt
* handler of the device from the interrupt source and disabling interrupts
* at other levels.
*
* Due to bus throttling that could hold the bus between messages when using
* repeated start option, stop will not occur when the device is actively
* sending or receiving data from the IIC bus or the bus is being throttled
* by this device, but instead return XST_IIC_BUS_BUSY.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
*
* @return
*
* - XST_SUCCESS indicates all IIC interrupts are disabled. No messages can
*   be received or transmitted until XIic_Start() is called.
* - XST_IIC_BUS_BUSY indicates this device is currently engaged in message
*   traffic and cannot be stopped.
*
* @note
*
* None.
*
****************************************************************************/
XStatus
XIic_Stop(XIic * InstancePtr)
{
	u8 Status;
	u8 CntlReg;

	XASSERT_NONVOID(InstancePtr != NULL);

	/*
	 * Disable all interrupts globally using the IPIF
	 */
	XIIF_V123B_GINTR_DISABLE(InstancePtr->BaseAddress);

	CntlReg = XIo_In8(InstancePtr->BaseAddress + XIIC_CR_REG_OFFSET);
	Status = XIo_In8(InstancePtr->BaseAddress + XIIC_SR_REG_OFFSET);

	if ((CntlReg & XIIC_CR_MSMS_MASK)
	    || (Status & XIIC_SR_ADDR_AS_SLAVE_MASK)) {
		/* when this device is using the bus
		 * - re-enable interrupts to finish current messaging
		 * - return bus busy
		 */
		XIIF_V123B_GINTR_ENABLE(InstancePtr->BaseAddress);

		return XST_IIC_BUS_BUSY;
	}

	InstancePtr->IsStarted = 0;

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* Resets the IIC device. Reset must only be called after the driver has been
* initialized. The configuration after this reset is as follows:
*   - Repeated start is disabled
*   - General call is disabled
*
* The upper layer software is responsible for initializing and re-configuring
* (if necessary) and restarting the IIC device after the reset.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
*
* @return
*
* None.
*
* @note
*
* None.
*
* @internal
*
* The reset is accomplished by setting the IPIF reset register.  This takes
* care of resetting all IPIF hardware blocks, including the IIC device.
*
****************************************************************************/
void
XIic_Reset(XIic * InstancePtr)
{
	XASSERT_VOID(InstancePtr != NULL);
	XASSERT_VOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

	XIIF_V123B_RESET(InstancePtr->BaseAddress);
}

/*****************************************************************************/
/**
*
* This function sets the bus addresses. The addresses include the device
* address that the device responds to as a slave, or the slave address
* to communicate with on the bus.  The IIC device hardware is built to
* allow either 7 or 10 bit slave addressing only at build time rather
* than at run time. When this device is a master, slave addressing can
* be selected at run time to match addressing modes for other bus devices.
*
* Addresses are represented as hex values with no adjustment for the data
* direction bit as the software manages address bit placement.
* Example: For a 7 address written to the device of 1010 011X where X is
* the transfer direction (send/recv), the address parameter for this function
* needs to be 01010011 or 0x53 where the correct bit alllignment will be
* handled for 7 as well as 10 bit devices. This is especially important as
* the bit placement is not handled the same depending on which options are
* used such as repeated start.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
* @param    AddressType indicates which address is being modified; the address
*           which this device responds to on the IIC bus as a slave, or the
*           slave address to communicate with when this device is a master. One
*           of the following values must be contained in this argument.
* <pre>
*   XII_ADDRESS_TO_SEND         Slave being addressed by a this master
*   XII_ADDRESS_TO_RESPOND      Address to respond to as a slave device
* </pre>
* @param    Address contains the address to be set; 7 bit or 10 bit address.
*           A ten bit address must be within the range: 0 - 1023 and a 7 bit
*           address must be within the range 0 - 127.
*
* @return
*
* XST_SUCCESS is returned if the address was successfully set, otherwise one
* of the following errors is returned.
* - XST_IIC_NO_10_BIT_ADDRESSING indicates only 7 bit addressing supported.
* - XST_INVALID_PARAM indicates an invalid parameter was specified.
*
* @note
*
* Upper bits of 10-bit address is written only when current device is built
* as a ten bit device.
*
****************************************************************************/
XStatus
XIic_SetAddress(XIic * InstancePtr, int AddressType, int Address)
{
	u8 SendAddr;

	XASSERT_NONVOID(InstancePtr != NULL);
	XASSERT_NONVOID(Address < 1023);

	/* Set address to respond to for this device into address registers */

	if (AddressType == XII_ADDR_TO_RESPOND_TYPE) {
		SendAddr = (u8) ((Address & 0x007F) << 1);	/* Addr in upper 7 bits */
		XIo_Out8(InstancePtr->BaseAddress + XIIC_ADR_REG_OFFSET,
			 SendAddr);

		if (InstancePtr->Has10BitAddr == TRUE) {
			/* Write upper 3 bits of addr to DTR only when 10 bit option
			 * included in design i.e. register exists
			 */
			SendAddr = (u8) ((Address & 0x0380) >> 7);
			XIo_Out8(InstancePtr->BaseAddress + XIIC_TBA_REG_OFFSET,
				 SendAddr);
		}

		return XST_SUCCESS;
	}

	/* Store address of slave device being read from */

	if (AddressType == XII_ADDR_TO_SEND_TYPE) {
		InstancePtr->AddrOfSlave = Address;
		return XST_SUCCESS;
	}

	return XST_INVALID_PARAM;
}

/*****************************************************************************/
/**
*
* This function gets the addresses for the IIC device driver. The addresses
* include the device address that the device responds to as a slave, or the
* slave address to communicate with on the bus. The address returned has the
* same format whether 7 or 10 bits.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
* @param    AddressType indicates which address, the address which this
*           responds to on the IIC bus as a slave, or the slave address to
*           communicate with when this device is a master. One of the following
*           values must be contained in this argument.
* <pre>
*   XII_ADDRESS_TO_SEND_TYPE         slave being addressed as a master
*   XII_ADDRESS_TO_RESPOND_TYPE      slave address to respond to as a slave
* </pre>
*  If neither of the two valid arguments are used, the function returns
*  the address of the slave device
*
* @return
*
* The address retrieved.
*
* @note
*
* None.
*
****************************************************************************/
u16
XIic_GetAddress(XIic * InstancePtr, int AddressType)
{
	u8 LowAddr;
	u16 HighAddr = 0;

	XASSERT_NONVOID(InstancePtr != NULL);

	/* return this devices address */

	if (AddressType == XII_ADDR_TO_RESPOND_TYPE) {

		LowAddr =
		    XIo_In8(InstancePtr->BaseAddress + XIIC_ADR_REG_OFFSET);

		if (InstancePtr->Has10BitAddr == TRUE) {
			HighAddr = (u16) XIo_In8(InstancePtr->BaseAddress +
						 XIIC_TBA_REG_OFFSET);
		}
		return ((HighAddr << 8) & (u16) LowAddr);
	}

	/* Otherwise return address of slave device on the IIC bus */

	return InstancePtr->AddrOfSlave;
}

/*****************************************************************************/
/**
*
* A function to determine if the device is currently addressed as a slave
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
*
* @return
*
* TRUE if the device is addressed as slave, and FALSE otherwise.
*
* @note
*
* None.
*
****************************************************************************/
u32
XIic_IsSlave(XIic * InstancePtr)
{
	XASSERT_NONVOID(InstancePtr != NULL);

	if ((XIo_In8(InstancePtr->BaseAddress + XIIC_SR_REG_OFFSET) &
	     XIIC_SR_ADDR_AS_SLAVE_MASK) == 0) {
		return FALSE;
	}
	return TRUE;
}

/*****************************************************************************/
/**
*
* Sets the receive callback function, the receive handler, which the driver
* calls when it finishes receiving data. The number of bytes used to signal
* when the receive is complete is the number of bytes set in the XIic_Recv
* function.
*
* The handler executes in an interrupt context such that it must minimize
* the amount of processing performed such as transferring data to a thread
* context.
*
* The number of bytes received is passed to the handler as an argument.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
* @param    CallBackRef is the upper layer callback reference passed back when
*           the callback function is invoked.
* @param    FuncPtr is the pointer to the callback function.
*
* @return
*
* None.
*
* @note
*
* The handler is called within interrupt context ...
*
****************************************************************************/
void
XIic_SetRecvHandler(XIic * InstancePtr, void *CallBackRef, XIic_Handler FuncPtr)
{
	XASSERT_VOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);
	XASSERT_VOID(InstancePtr != NULL);
	XASSERT_VOID(FuncPtr != NULL);

	InstancePtr->RecvHandler = FuncPtr;
	InstancePtr->RecvCallBackRef = CallBackRef;
}

/*****************************************************************************/
/**
*
* Sets the send callback function, the send handler, which the driver calls when
* it receives confirmation of sent data. The handler executes in an interrupt
* context such that it must minimize the amount of processing performed such
* as transferring data to a thread context.
*
* @param    InstancePtr the pointer to the XIic instance to be worked on.
* @param    CallBackRef the upper layer callback reference passed back when
*           the callback function is invoked.
* @param    FuncPtr the pointer to the callback function.
*
* @return
*
* None.
*
* @note
*
* The handler is called within interrupt context ...
*
****************************************************************************/
void
XIic_SetSendHandler(XIic * InstancePtr, void *CallBackRef, XIic_Handler FuncPtr)
{
	XASSERT_VOID(InstancePtr != NULL);
	XASSERT_VOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);
	XASSERT_VOID(FuncPtr != NULL);

	InstancePtr->SendHandler = FuncPtr;
	InstancePtr->SendCallBackRef = CallBackRef;
}

/*****************************************************************************/
/**
*
* Sets the status callback function, the status handler, which the driver calls
* when it encounters conditions which are not data related. The handler
* executes in an interrupt context such that it must minimize the amount of
* processing performed such as transferring data to a thread context. The
* status events that can be returned are described in xiic.h.
*
* @param    InstancePtr points to the XIic instance to be worked on.
* @param    CallBackRef is the upper layer callback reference passed back when
*           the callback function is invoked.
* @param    FuncPtr is the pointer to the callback function.
*
* @return
*
* None.
*
* @note
*
* The handler is called within interrupt context ...
*
****************************************************************************/
void
XIic_SetStatusHandler(XIic * InstancePtr, void *CallBackRef,
		      XIic_StatusHandler FuncPtr)
{
	XASSERT_VOID(InstancePtr != NULL);
	XASSERT_VOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);
	XASSERT_VOID(FuncPtr != NULL);

	InstancePtr->StatusHandler = FuncPtr;
	InstancePtr->StatusCallBackRef = CallBackRef;
}

/*****************************************************************************/
/**
*
* Looks up the device configuration based on the unique device ID. The table
* IicConfigTable contains the configuration info for each device in the system.
*
* @param DeviceId is the unique device ID to look for
*
* @return
*
* A pointer to the configuration data of the device, or NULL if no match is
* found.
*
* @note
*
* None.
*
******************************************************************************/
XIic_Config *
XIic_LookupConfig(u16 DeviceId)
{
	XIic_Config *CfgPtr = NULL;
	int i;

	for (i = 0; i < CONFIG_XILINX_IIC_NUM_INSTANCES; i++) {
		if (XIic_ConfigTable[i].DeviceId == DeviceId) {
			CfgPtr = &XIic_ConfigTable[i];
			break;
		}
	}

	return CfgPtr;
}

/*****************************************************************************
*
* This is a stub for the send and recv callbacks. The stub is here in case the
* upper layers forget to set the handlers.
*
* @param    CallBackRef is a pointer to the upper layer callback reference
* @param    ByteCount is the number of bytes sent or received
*
* @return
*
* None.
*
* @note
*
* None.
*
******************************************************************************/
static void
XIic_StubHandler(void *CallBackRef, int ByteCount)
{
	XASSERT_VOID_ALWAYS();
}

/*****************************************************************************
*
* This is a stub for the asynchronous error callback. The stub is here in case
* the upper layers forget to set the handler.
*
* @param    CallBackRef is a pointer to the upper layer callback reference
* @param    ErrorCode is the Xilinx error code, indicating the cause of the error
*
* @return
*
* None.
*
* @note
*
* None.
*
******************************************************************************/
static void
XIic_StubStatusHandler(void *CallBackRef, XStatus ErrorCode)
{
	XASSERT_VOID_ALWAYS();
}
