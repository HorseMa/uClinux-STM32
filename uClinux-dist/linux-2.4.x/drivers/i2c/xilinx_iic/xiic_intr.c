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
* @file xiic_intr.c
*
* Contains interrupt functions of the XIic driver.  This file is required
* for the driver.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.01a rfp  10/19/01 release
* 1.01c ecm  12/05/02 new rev
* 1.01c rmm  05/14/03 Fixed diab compiler warnings relating to asserts.
* </pre>
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xiic.h"
#include "xiic_i.h"
#include "xio.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions ******************/

/*************** Macros (Inline Functions) Definitions ********************/

/************************** Function Prototypes ****************************/

static void StubFunction(XIic * InstancePtr);
static void TxErrorHandler(XIic * InstancePtr);

/*****************************************************************************
 * This is a temporary workaround for the single master mode to work correctly.
 * Until the platform independent IIC driver code is updated by Xilinx
 * XIic_BusNotBusyFuncPtr must be initialized to BusNotBusyStub instead of
 * StubFunction.
 ****************************************************************************/
static void
BusNotBusyStub(XIic * InstancePtr)
{
	return;
}

/************************** Variable Definitions *****************************/

/* The following function pointers are used to help allow finer partitioning
 * of the driver such that some parts of it are optional. These pointers are
 * setup by functions in the optional parts of the driver.
 */
void (*XIic_AddrAsSlaveFuncPtr) (XIic * InstancePtr) = StubFunction;
void (*XIic_NotAddrAsSlaveFuncPtr) (XIic * InstancePtr) = StubFunction;
void (*XIic_RecvSlaveFuncPtr) (XIic * InstancePtr) = StubFunction;
void (*XIic_SendSlaveFuncPtr) (XIic * InstancePtr) = StubFunction;
void (*XIic_RecvMasterFuncPtr) (XIic * InstancePtr) = StubFunction;
void (*XIic_SendMasterFuncPtr) (XIic * InstancePtr) = StubFunction;
void (*XIic_ArbLostFuncPtr) (XIic * InstancePtr) = StubFunction;
void (*XIic_BusNotBusyFuncPtr) (XIic * InstancePtr) = BusNotBusyStub;

/*****************************************************************************/
/**
*
* This function is the interrupt handler for the XIic driver. This function
* should be connected to the interrupt system.
*
* Only one interrupt source is handled for each interrupt allowing
* higher priority system interrupts quicker response time.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
*
* @return
*
* None.
*
* @internal
*
* The XIIC_INTR_ARB_LOST_MASK and XIIC_INTR_TX_ERROR_MASK interrupts must have
* higher priority than the other device interrupts so that the IIC device does
* not get into a potentially confused state. The remaining interrupts may be
* rearranged with no harm.
*
* All XIic device interrupts are ORed into one device interrupt. This routine
* reads the pending interrupts via the IpIf interface and masks that with the
* interrupt mask to evaluate only the interrupts enabled.
*
******************************************************************************/
void
XIic_InterruptHandler(void *InstancePtr)
{
	u8 Status;
	u32 IntrStatus;
	u32 IntrPending;
	u32 IntrEnable;
	XIic *IicPtr = NULL;
	u32 Clear = 0;

	/*
	 * Verify that each of the inputs are valid.
	 */
	XASSERT_VOID(InstancePtr != NULL);

	/*
	 * Convert the non-typed pointer to an IIC instance pointer
	 */
	IicPtr = (XIic *) InstancePtr;

	/* Get the interrupt Status from the IPIF. There is no clearing of
	 * interrupts in the IPIF. Interrupts must be cleared at the source.
	 * To find which interrupts are pending; AND interrupts pending with
	 * interrupts masked.
	 */
	IntrPending = XIIF_V123B_READ_IISR(IicPtr->BaseAddress);
	IntrEnable = XIIF_V123B_READ_IIER(IicPtr->BaseAddress);
	IntrStatus = IntrPending & IntrEnable;

	/* Do not processes a devices interrupts if the device has no
	 * interrupts pending or the global interrupts have been disabled
	 */

	if ((IntrStatus == 0) |
	    (XIIF_V123B_IS_GINTR_ENABLED(IicPtr->BaseAddress) == FALSE)) {
		return;
	}

	/* Update interrupt stats and get the contents of the status register
	 */
	IicPtr->Stats.IicInterrupts++;
	Status = XIo_In8(IicPtr->BaseAddress + XIIC_SR_REG_OFFSET);

	/* Service requesting interrupt
	 */
	if (IntrStatus & XIIC_INTR_ARB_LOST_MASK) {
		/* Bus Arbritration Lost */

		IicPtr->Stats.ArbitrationLost++;
		XIic_ArbLostFuncPtr(IicPtr);

		Clear = XIIC_INTR_ARB_LOST_MASK;
	}

	else if (IntrStatus & XIIC_INTR_TX_ERROR_MASK) {
		/* Transmit errors (no acknowledge) received */

		IicPtr->Stats.TxErrors++;
		TxErrorHandler(IicPtr);

		Clear = XIIC_INTR_TX_ERROR_MASK;
	}

	else if (IntrStatus & XIIC_INTR_NAAS_MASK) {
		/* Not Addressed As Slave */

		XIic_NotAddrAsSlaveFuncPtr(IicPtr);
		Clear = XIIC_INTR_NAAS_MASK;
	}

	else if (IntrStatus & XIIC_INTR_RX_FULL_MASK) {
		/* Receive register/FIFO is full */

		IicPtr->Stats.RecvInterrupts++;

		if (Status & XIIC_SR_ADDR_AS_SLAVE_MASK) {
			XIic_RecvSlaveFuncPtr(InstancePtr);
		} else {
			XIic_RecvMasterFuncPtr(InstancePtr);
		}

		Clear = XIIC_INTR_RX_FULL_MASK;
	}

	else if (IntrStatus & XIIC_INTR_AAS_MASK) {
		/* Addressed As Slave */

		XIic_AddrAsSlaveFuncPtr(IicPtr);
		Clear = XIIC_INTR_AAS_MASK;
	}

	else if (IntrStatus & XIIC_INTR_BNB_MASK) {
		/* IIC bus has transitioned to not busy */

		/* check if send callback needs to run */
		if (IicPtr->BNBOnly == TRUE) {
			XIic_BusNotBusyFuncPtr(IicPtr);
			IicPtr->BNBOnly = FALSE;
		} else {
			IicPtr->SendHandler(IicPtr->SendCallBackRef, 0);
		}

		Clear = XIIC_INTR_BNB_MASK;

		/* The bus is not busy, disable BusNotBusy interrupt */
		XIic_mDisableIntr(IicPtr->BaseAddress, XIIC_INTR_BNB_MASK);

	}

	else if ((IntrStatus & XIIC_INTR_TX_EMPTY_MASK) ||
		 (IntrStatus & XIIC_INTR_TX_HALF_MASK)) {
		/* Transmit register/FIFO is empty or � empty *
		 */
		IicPtr->Stats.SendInterrupts++;

		if (Status & XIIC_SR_ADDR_AS_SLAVE_MASK) {
			XIic_SendSlaveFuncPtr(InstancePtr);
		} else {
			XIic_SendMasterFuncPtr(InstancePtr);
		}

		/* Clear Interrupts
		 */
		IntrStatus = XIIF_V123B_READ_IISR(IicPtr->BaseAddress);
		Clear = IntrStatus & (XIIC_INTR_TX_EMPTY_MASK |
				      XIIC_INTR_TX_HALF_MASK);
	}

	XIIF_V123B_WRITE_IISR(IicPtr->BaseAddress, Clear);
}

/******************************************************************************
*
* This function fills the FIFO using the occupancy register to determine the
* available space to be filled. When the repeated start option is on, the last
* byte is withheld to allow the control register to be properly set on the last
* byte.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
*
* @param    Role indicates the role of this IIC device, a slave or a master, on
*           the IIC bus (XIIC_SLAVE_ROLE or XIIC_MASTER_ROLE)
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
void
XIic_TransmitFifoFill(XIic * InstancePtr, int Role)
{
	u8 AvailBytes;
	s32 LoopCnt;
	s32 NumBytesToSend;

	/* Determine number of bytes to write to FIFO. Number of bytes that can be
	 * put into the FIFO is (FIFO depth) - (current occupancy + 1)
	 * When more room in FIFO than msg bytes put all of message in the FIFO.
	 */
	AvailBytes = IIC_TX_FIFO_DEPTH -
	    (XIo_In8(InstancePtr->BaseAddress + XIIC_TFO_REG_OFFSET) + 1);

	if (InstancePtr->SendByteCount > AvailBytes) {
		NumBytesToSend = AvailBytes;
	} else {
		/* More space in FIFO than bytes in message
		 */
		if ((InstancePtr->Options & XII_REPEATED_START_OPTION) ||
		    (Role == XIIC_SLAVE_ROLE)) {
			NumBytesToSend = InstancePtr->SendByteCount;
		} else {
			NumBytesToSend = InstancePtr->SendByteCount - 1;
		}
	}

	/* fill FIFO with amount determined above */

	for (LoopCnt = 0; LoopCnt < NumBytesToSend; LoopCnt++) {
		XIic_mWriteSendByte(InstancePtr);
	}
}

/*****************************************************************************/
/**
*
* This interrupt occurs four different ways: Two as master and two as slave.
* Master:
* <pre>
*  (1) Transmitter (IMPLIES AN ERROR)
*      The slave receiver did not acknowledge properly.
*  (2) Receiver (Implies tx complete)
*      Interrupt caused by setting TxAck high in the IIC to indicate to the
*      the last byte has been transmitted.
* </pre>
*
* Slave:
* <pre>
*  (3) Transmitter (Implies tx complete)
*      Interrupt caused by master device indicating last byte of the message
*      has been transmitted.
*  (4) Receiver (IMPLIES AN ERROR)
*      Interrupt caused by setting TxAck high in the IIC to indicate Rx
*      IIC had a problem - set by this device and condition already known
*      and interrupt is not enabled.
* </pre>
*
* This interrupt is enabled during Master send and receive and disabled
* when this device knows it is going to send a negative acknowledge (Ack = No).
*
* Signals user of Tx error via status callback sending: XII_TX_ERROR_EVENT
*
* When MasterRecv has no message to send and only receives one byte of data
* from the salve device, the TxError must be enabled to catch addressing
* errors, yet there is not opportunity to disable TxError when there is no
* data to send allowing disabling on last byte. When the slave sends the
* only byte the NOAck causes a Tx Error. To disregard this as no real error,
* when there is data in the Receive FIFO/register then the error was not
* a device address write error, but a NOACK read error - to be ignored.
* To work with or without FIFO's, the Rx Data interrupt is used to indicate
* data is in the rx register.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
*
* @return
*
* None.
*
* @note
*
* No action is required to clear this interrupt in the device as it is a
* pulse. The interrupt need only be cleared in the IpIf interface.
*
******************************************************************************/
static void
TxErrorHandler(XIic * InstancePtr)
{
	u32 IntrStatus;
	u8 CntlReg;

	/* When Sending as a slave, Tx error signals end of msg. Not Addressed As
	 * Slave will handle the callbacks. this is used to only flush the Tx fifo.
	 * The addressed as slave bit is gone as soon as the bus has been released
	 * such that the buffer pointers are used to determine the direction of
	 * transfer (send or receive).
	 */
	if (InstancePtr->RecvBufferPtr == NULL) {
		/* Master Receiver finished reading message. Flush Tx fifo to remove an
		 * 0xFF that was written to prevent bus throttling, and disable all
		 * transmit and receive interrupts
		 */
		XIic_mFlushTxFifo(InstancePtr);
		XIic_mDisableIntr(InstancePtr->BaseAddress,
				  XIIC_TX_RX_INTERRUPTS);

		return;
	}

	/* Data in the receive register from either master or slave receive
	 * When:slave, indicates master sent last byte, message completed.
	 * When:master, indicates a master Receive with one byte received. When a
	 * byte is in Rx reg then the Tx error indicates the Rx data was recovered
	 * normally Tx errors are not enabled such that this should not occur.
	 */
	IntrStatus = XIIF_V123B_READ_IISR(InstancePtr->BaseAddress);
	if (IntrStatus & XIIC_INTR_RX_FULL_MASK) {
		/* Rx Reg/FIFO has data,  Disable tx error interrupts */

		XIic_mDisableIntr(InstancePtr->BaseAddress,
				  XIIC_INTR_TX_ERROR_MASK);
		return;
	}

	XIic_mFlushTxFifo(InstancePtr);

	/* Disable and clear tx empty, � empty, Rx Full or tx error interrupts
	 */
	XIic_mDisableIntr(InstancePtr->BaseAddress, XIIC_TX_RX_INTERRUPTS);
	XIic_mClearIntr(InstancePtr->BaseAddress, XIIC_TX_RX_INTERRUPTS);

	/* Clear MSMS as on TX error when Rxing, the bus will be
	 * stopped but MSMS bit is still set. Reset to proper state
	 */
	CntlReg = XIo_In8(InstancePtr->BaseAddress + XIIC_CR_REG_OFFSET);
	CntlReg &= ~XIIC_CR_MSMS_MASK;
	XIo_Out8(InstancePtr->BaseAddress + XIIC_CR_REG_OFFSET, CntlReg);

	/* set FIFO occupancy depth = 1 so that the first byte will throttle
	 * next recieve msg
	 */
	XIo_Out8(InstancePtr->BaseAddress + XIIC_RFD_REG_OFFSET, 0);

	/* make event callback */

	InstancePtr->StatusHandler(InstancePtr->StatusCallBackRef,
				   XII_SLAVE_NO_ACK_EVENT);
}

/*****************************************************************************/
/**
*
* This function is a stub function that is used for the default function for
* events that are handled optionally only when the appropriate modules are
* linked in.  Function pointers are used to handle some events to allow
* some events to be optionally handled.
*
* @param    InstancePtr is a pointer to the XIic instance to be worked on.
*
******************************************************************************/
static void
StubFunction(XIic * InstancePtr)
{
	XASSERT_VOID_ALWAYS();
}
