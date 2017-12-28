/*******************************************************************************
 *
 * Copyright (c) 2003 Cypress Semiconductor
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __CY7C67200_300_LCDH_
#define __CY7C67200_300_LCDH_

/*******************************************************************
 *
 *    DESCRIPTION: CY7C67200_300 Low-level interface header file
 *
 *******************************************************************/

/** include file(s) **/

#include <linux/list.h>
#include <linux/spinlock.h>
#include "cy7c67200_300_common.h"

/** definitions **/

/* Endpoint definition(s) */

#define MAX_NUM_ENDPOINTS   0x8


/** OTG Command defines **/

#define LCD_OFFER_HNP                     0x00
#define LCD_INITIATE_SRP                  0x01
/* Reserved for future use - 0x02 to 0x0F */

/** HCD Command defines **/

#define LCD_DISABLE_OTG_VBUS              0x10
#define LCD_ENABLE_OTG_VBUS               0x11
#define LCD_START_SOF                     0x12
#define LCD_STOP_SOF                      0x13
#define LCD_CHANGE_TO_PERIPHERAL_ROLE     0x14
/* Reserved for future use - 0x15 to 0x1F */

/** PCD Command defines **/

#define LCD_CHANGE_TO_HOST_ROLE           0x20
#define LCD_REMOTE_WAKEUP                 0x21
/* Reserved for future use - 0x22 to 0x2F */

/** Shared Command defines */
#define LCD_DATALINE_RESISTOR_TERMINATION 0x30
/* Reserved for future use - 0x31 to 0x3F */


/** Dataline Termination Resistor Defines **/

#define DATALINE_TERMINATION_OFF    0x0
#define DPLUS_PULLUP                0x1
#define DMINUS_PULLUP               0x2
#define DPLUS_PULLDOWN              0x4
#define DMINUS_PULLDOWN             0x8

/** Download start address **/

#define DOWNLOAD_START_ADDRESS      0x04D4

/** Structures **/
typedef void (*lcd_callback_t)(unsigned short response,
                               unsigned short value,
                               int opt_arg,
                               cy_priv_t * cy_priv_data);

typedef struct lcd_lcp_entry_struct
{
    struct list_head list;
    unsigned short command;
    unsigned short arg[15];
    unsigned short response;
    unsigned short value;
    char * buf;
    int len;
    lcd_callback_t funcptr;
    int opt_arg;
    cy_priv_t * cy_priv_data;

} lcd_lcp_entry_t;

typedef struct lcd_priv_data_struct
{
    struct list_head lcd_lcp_list;
    spinlock_t lcd_lock;
    unsigned short sie1_cb_addresses[MAX_NUM_ENDPOINTS];
    unsigned short sie2_cb_addresses[MAX_NUM_ENDPOINTS];
} lcd_priv_data_t;

typedef struct lcd_int_data_struct
{
    unsigned short int_num;
    unsigned short reg[14];
} lcd_int_data_t;


/** Prototypes **/

unsigned short lcd_get_cb_address(unsigned short port_num,
                                  int endpoint,
                                  cy_priv_t * cy_private_data);

int lcd_init(char * download_data,
             int download_length,
             lcd_callback_t funcptr,
             int opt_arg,
             cy_priv_t * cy_private_data);

int lcd_complete_lcp_entry(cy_priv_t * cy_private_data);


int lcd_command(int command,
                int command_arg,
                unsigned short port_number,
                lcd_callback_t funcptr,
                int opt_arg,
                cy_priv_t * cy_private_data);

int lcd_send_data(unsigned short chip_addr,
                  unsigned short port_number,
                  unsigned short endpoint,
                  int byte_length,
                  char * data,
                  lcd_callback_t funcptr,
                  int opt_arg,
                  cy_priv_t * cy_private_data);

int lcd_recv_data(unsigned short chip_addr,
                  unsigned short port_number,
                  unsigned short endpoint,
                  int byte_length,
                  char * data,
                  lcd_callback_t funcptr,
                  int opt_arg,
                  cy_priv_t * cy_private_data);

int lcd_send_tdlist(unsigned short chip_addr,
                    int port_number,
                    int byte_length,
                    char * data,
                    cy_priv_t * cy_private_data);

int lcd_get_tdstatus(unsigned short chip_addr,
                     int byte_length,
                     char * data,
                     cy_priv_t * cy_private_data);

int lcd_read_memory(unsigned short chip_addr,
                    int byte_length,
                    char * data,
                    cy_priv_t * cy_private_data);

int lcd_read_memory_little(unsigned short chip_addr,
                    int byte_length,
                    char * data,
                    cy_priv_t * cy_private_data);

int lcd_write_memory(unsigned short chip_addr,
                     int byte_length,
                     char * data,
                     cy_priv_t * cy_private_data);

int lcd_write_memory_big(unsigned short chip_addr,
                     int byte_length,
                     char * data,
                     cy_priv_t * cy_private_data);

int lcd_read_xmemory(unsigned short xmem_addr,
                     unsigned short byte_length,
                     char * data,
                     lcd_callback_t funcptr,
                     int opt_arg,
                     cy_priv_t * cy_private_data);

int lcd_write_xmemory(unsigned short xmem_addr,
                      unsigned short byte_length,
                      char * data,
                      lcd_callback_t funcptr,
                      int opt_arg,
                      cy_priv_t * cy_private_data);

int lcd_read_reg(unsigned short reg_addr,
                 unsigned short *reg_value,
                 cy_priv_t * cy_private_data);

int lcd_write_reg(unsigned short reg_addr,
                  unsigned short reg_value,
                  cy_priv_t * cy_private_data);

int lcd_lcp_write_reg(unsigned short reg_addr,
                      unsigned short reg_value,
                      unsigned short flag,
                      cy_priv_t * cy_private_data);

int lcd_exec_interrupt(lcd_int_data_t *int_data,
                       lcd_callback_t funcptr,
                       int opt_arg,
                       cy_priv_t * cy_private_data);

int lcd_download_code(unsigned short chip_addr,
                      int byte_length,
                      char * data,
                      lcd_callback_t funcptr,
                      int opt_arg,
                      cy_priv_t * cy_private_data);

void lcd_hpi_write_word(unsigned short chip_addr,
                        unsigned short value,
                        cy_priv_t * cy_private_data);

unsigned short lcd_hpi_read_word(unsigned short chip_addr,
                                 cy_priv_t * cy_private_data);

void lcd_hpi_write_words(unsigned short chip_addr,
                         unsigned short * data,
                         int num_words,
                         cy_priv_t * cy_private_data);

void lcd_hpi_write_words_big(unsigned short chip_addr,
                         unsigned short * data,
                         int num_words,
                         cy_priv_t * cy_private_data);

void lcd_hpi_read_words(unsigned short chip_addr,
                        unsigned short * data,
                        int num_words,
                        cy_priv_t * cy_private_data);

void lcd_hpi_read_words_big(unsigned short chip_addr,
                        unsigned short * data,
                        int num_words,
                        cy_priv_t * cy_private_data);

void lcd_hpi_write_mbx(unsigned short value,
                       cy_priv_t * cy_private_data);

unsigned short lcd_hpi_read_mbx(cy_priv_t * cy_private_data);

unsigned short lcd_hpi_read_status_reg(cy_priv_t * cy_private_data);

void lcd_hpi_write_status_reg(unsigned short value,
                              cy_priv_t * cy_private_data);

extern void cy67x00_int_handler(int irq, void * cy_priv, struct pt_regs * r);


#endif /* __CY7C67200_300_LCDH_ */

