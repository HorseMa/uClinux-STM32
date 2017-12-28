/* -*- c -*- --------------------------------------------------------- *
 * Copyright 2003-09-23 sympat GmbH
 * Authors: Matthias Panten
 *
 * This driver supports one port of max 8 bit for 8 leds
 *
 * ------------------------------------------------------------------------- */

#ifndef __LED_OP77A__
#define __LED_OP77A__

/**
 * IOCTL COMMANDS
 *
 * LED_OP77A_SET_LED  
 *     parameter is one unsigned long as packed format for
 *     led-mode: 16 LSB 
 *     led-id: 16 MSB 
 *     e.g. mode ON for led 4: 0x00040003
 *  
 *     the following enums should be used to generarte the parameter 
 *     in this way: led_id_mode = (LED_K1 | LED_OP77A_MODE_ON); 
 */
#define LED_OP77A_SET_LED  1200

/** valid LED IDs: */
enum LED_OP77A_IDS {
    LED_HELP = 0,
    LED_K4   = 1,
    LED_K3   = 2,
    LED_K2   = 3,
    LED_K1   = 4,
    LED_ACK  = 5,
    LED_ALL  = 0xffff
};
/* LED_ACK must be the last led-number */

/** valid LED modes: */
enum LED_OP77A_MODES {
    LED_OP77A_MODE_OFF   = 0,
    LED_OP77A_MODE_FREQA = 1,
    LED_OP77A_MODE_FREQB = 2,
    LED_OP77A_MODE_ON    = 3
};
/* LED_OP77A_MODE_ON must be the last mode-number */

//------------------------------
// Device definintions
//------------------------------
/** the devicename and major-number */
#define LED_OP77A_DEVICENAME "led"
#define LED_OP77A_MAJOR_NUMBER 242

#endif /*led_op77a.h*/ 
