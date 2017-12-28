/* -*- c -*- --------------------------------------------------------- *
 * file: beep_ktp178.h
 * Copyright 2003-09-23 sympat GmbH
 * Authors: Matthias Panten
 *
 * This driver supports pwm-signal
 *
 * ------------------------------------------------------------------------- */

#ifndef __BEEP_KTP178__
#define __BEEP_KTP178__

/**
 * IOCTL COMMANDS
 *
 */
#define BEEP_KTP178_SET_SND 1
#define BEEP_KTP178_GET_DEF 2
#define BEEP_KTP178_GET_MAX 3

/** 
 * This struct contains the 
 * @param frequence (unsigned int)
 * @param dutycycle (unsigned int)
 * @param duration (unsigned int)
 *
 * of the wanted beep / the default parameters
 */
typedef struct {
	unsigned int freq;
	unsigned int duty;
	unsigned int durat;
} beep_ktp178_t;


/** Device definintions: 
 * the devicename and major-number
 */
#define BEEP_KTP178_DEVICENAME "beep"
#define BEEP_KTP178_MAJOR_NUMBER 128

#endif /* beep_ktp178.h*/
