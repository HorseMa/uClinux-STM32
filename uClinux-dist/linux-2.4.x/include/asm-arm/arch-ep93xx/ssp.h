/*
 * 
 *  FILE:           ssp.h
 *
 *  DESCRIPTION:    SSP Interface Driver Module implementation
 *
 *  Copyright Cirrus Logic Corporation, 2001-2003.  All rights reserved
 *
 */
#ifndef _SSP_DRV_H_
#define _SSP_DRV_H_

typedef enum 
{
	PS2_KEYBOARD = 0,
	I2S_CODEC    = 1,
	SERIAL_FLASH = 2
} SSPDeviceType;

typedef void (*SSPDataCallback)(unsigned int Data);

typedef struct _SSP_DRIVER_API
{
    int (*Open)(SSPDeviceType Device, SSPDataCallback Callback);
    int (*Read)(int Handle, unsigned int Addr, unsigned int *pValue);
    int (*Write)(int Handle, unsigned int Addr, unsigned int Value);
    int (*Close)(int Handle);
} SSP_DRIVER_API;

extern SSP_DRIVER_API *SSPDriver;

#endif /* _SSP_DRV_H_ */
