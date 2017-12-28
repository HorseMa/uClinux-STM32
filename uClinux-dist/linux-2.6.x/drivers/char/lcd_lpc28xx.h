#ifndef _LPC28XX_LCD_H_
#define _LPC28XX_LCD_H_

#define LCD_MAJOR	200
#define LCD_HEIGHT	128
#define LCD_WIDTH	64
#define LCD_SIZE	LCD_HEIGHT*LCD_WIDTH/8

/* The following definitions are the command codes that are */
/* passed to the display via the data bus.                  */
#define DISPLAY_ON 0xAF
#define DISPLAY_OFF 0xAE
#define START_LINE_SET 0x40
#define PAGE_ADDRESS_SET 0xB0

/* The Column Address is a two byte operation that writes   */
/* the most significant bits of the address to D3 - D0 and  */
/* then writes the least significant bits to D3- D0.        */
/* Since the Column Address auto increments after each      */
/* write, direct access is infrequent.                      */
#define COLUMN_ADDRESS_HIGH 0x10
#define COLUMN_ADDRESS_LOW 0x00
#define ADC_SELECT_NORMAL 0xA0
#define ADC_SELECT_REVERSE 0xA1
#define DISPLAY_NORMAL 0xA6
#define DISPLAY_REVERSE 0xA7
#define ALL_POINTS_ON 0xA5
#define ALL_POINTS_OFF 0xA4
#define LCD_BIAS_1_9 0xA2
#define LCD_BIAS_1_7 0xA3
#define READ_MODIFY_WRITE 0xE0
#define END 0xEE
#define RESET_DISPLAY 0xE2
#define COMMON_OUTPUT_NORMAL 0xC0
#define COMMON_OUTPUT_REVERSE 0xC8

/* The power control set value is obtained by OR'ing the    */
/* values together to create the appropriate data value.    */
/* e.g. data = (POWER_CONTROL_SET | BOOSTER_CIRCUIT |       */
/*              VOLTAGE_REGULATOR | VOLTAGE_FOLLOWER);      */
/* Only the bits that are desired need be OR'ed in because  */
/* the initial value of POWER_CONTROL_SET sets them to zero.*/
#define POWER_CONTROL_SET 0x28
#define BOOSTER_CIRCUIT 0x04
#define VOLTAGE_REGULATOR 0x02
#define VOLTAGE_FOLLOWER 0x01

/* The initial value of the V5_RESISTOR_RATIO sets the      */
/* Rb/Ra ratio to the smallest setting. The valid range of  */
/* the ratio is: 0x20 <= V5_RESISTOR_RATIO <= 0x27          */
#define V5_RESISTOR_RATIO 0x25

/* When the electronic volume command is input, the         */
/* electronic volume register set command becomes enabled.  */
/* Once the electronic volume mode has been set, no other   */
/* command except for the electronic volume register        */
/* command can be used. Once the electronic volume register */
/* set command has been used to set data into the register, */
/* then the electronic volume mode is released.             */
#define ELECTRONIC_VOLUME_SET 0x81
#define ELECTRONIC_VOLUME_INIT 0x30

#define CMD 0x01
#define DATA 0xFF

#endif
