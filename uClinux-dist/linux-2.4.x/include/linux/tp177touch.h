/* -*- c -*- --------------------------------------------------------- *
 * Copyright 2003-09-23 sympat GmbH
 * Authors: Stefan Macher, Alexander Assel
 *
 * This driver is not compatible with the generic linux mouse protocol.
 * There is a microwindows driver available that supports this touchscreen
 * driver.
 *
 * The program povides a more accurate calibration based on the provisions
 * of Carlos E. Vidales found at www.embedded.com
 *
 * ------------------------------------------------------------------------- */

#ifndef __TP177TOUCH_INC__
#define __TP177TOUCH_INC__


/* debugging stuff */
#define TP177_TS_DEBUG_ERR    (1 << 0)
#define TP177_TS_DEBUG_INFO   (1 << 1)
#define TP177_TS_DEBUG_ADC    (1 << 2)
#define TP177_TS_DEBUG_POS    (1 << 3)
#define TP177_TS_DEBUG_PRESS  (1 << 4)
#define TP177_TS_DEBUG_CONFIG (1 << 5)
#define TP177_TS_DEBUG_IRQ    (1 << 6)
#define TP177_TS_DEBUG(x,args...) if(x&tp177_ts_debug_mask) printk("tp177_touch: " args);
#define TOUCH_MEASURE_DUMMY 1
#define TOUCH_MEASURE_REAL 1

/* hardware settings */
#define TP177_TS_IRQ 23 /* EXTINT2 */
#define TP177_TS_X_RES 320
#define TP177_TS_Y_RES 240

//------------------------------
//IOCTL COMMANDS und structures
//------------------------------
#define TP177_TS_SET_CAL_PARAMS   0
#define TP177_TS_GET_CAL_PARAMS   1
#define TP177_TS_SET_X_RES        2
#define TP177_TS_SET_Y_RES        3
#define TP177_TS_GET_CAL_MAX      4
#define TP177_TS_GET_CAL_DEF      5
#define TP177_TS_GET_X_RES        6
#define TP177_TS_GET_Y_RES        7
#define TP177_TS_GET_X_REVERSE    8
#define TP177_TS_GET_Y_REVERSE    9

struct tp177_ts_cal_set {
  int cal_x_off;
  unsigned int cal_x_scale;
  int cal_y_off;
  unsigned int cal_y_scale;
};

//------------------------------
// PRESS THRESHOLD
//------------------------------
#define TP177_TS_PRESS_THRESHOLD 30
//------------------------------

//------------------------------
// Device definintions
//------------------------------
#define TS_DEVICE "TP177"
#define TS_DEVICE_FILE "/dev/touchscreen/tp177"


/*! \brief Interface struct used for reading from touchscreen device
 */
struct ts_event
{
  unsigned char pressure; //!< zero -> not pressed, all other values indicate that the touch is pressed
//  unsigned char pressed; //!< 1: touch is pressed  0: touch is not pressed
  short x; //!< The x position
  short y; //!< The y position
  short pad; //!< No meaning for now
};

#endif /*tp177touch.h*/
