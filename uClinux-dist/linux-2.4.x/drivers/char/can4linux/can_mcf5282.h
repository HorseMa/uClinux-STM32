/*
 * can_mc5282.h - can4linux CAN driver module
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2003 port GmbH Halle/Saale
 *------------------------------------------------------------------
 * $Header: Exp $
 *
 *--------------------------------------------------------------------------
 *
 *
 * modification history
 * --------------------
 * $Log: $
 *
 *
 *
 *--------------------------------------------------------------------------
 */


extern unsigned int Base[];


#define CAN_SYSCLK 32

/* define some types, header file comes from CANopen */
#define UNSIGNED8 u8
#define UNSIGNED16 u16


#define MCFFLEXCAN_BASE (MCF_MBAR + 0x1c0000)	/* Base address FlexCAN module */


/* can4linux does not use all the full CAN features, partly because it doesn't
   make sense.
 */

/* We use only one transmit object of all messages to be transmitted */
#define TRANSMIT_OBJ 0
#define RECEIVE_OBJ 1


#include "TouCAN.h"



