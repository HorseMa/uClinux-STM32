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

/*******************************************************************
 *
 * DESCRIPTION:
 *   The Low-level Controller Driver (LCD) contains generic routines
 *   to help handle the OTG protocols and to communicate with the
 *   CY7C67200/300 ASIC.  "Helper" functions are provided to for data
 *   transfer from the HCD's/PCD's to the ASIC.
 *
 *******************************************************************/

/** include files **/

#include <linux/slab.h>
#include <asm/io.h>
/* #include <stdio.h> */
#include "cy7c67200_300_otg.h"
#include "cy7c67200_300_lcd.h"
#include <linux/usb.h>
#include "cy7c67200_300_hcd.h"
#include "cy7c67200_300_debug.h"
#include "usbd/bi/cy7c67200_300_pcd.h"
#include "cy7c67200_300.h"
#include "lcp_cmd.h"
#include "lcp_data.h"


/** default settings **/

/** external functions **/
void enable_irq(unsigned int);

/** external data **/
int cy_dbg_on = 0x0;

/** internal functions **/
static int lcd_writeable_region(unsigned short chip_addr, int byte_length);

static int lcd_start_lcp(lcd_lcp_entry_t * lcp, cy_priv_t * cy_private_data);

static lcd_lcp_entry_t * lcd_create_lcp_entry(unsigned short command,
        unsigned short arg1, unsigned short arg2, unsigned short arg3,
        unsigned short arg4, unsigned short arg5, unsigned short arg6,
        unsigned short arg7, unsigned short arg8, unsigned short arg9,
        unsigned short arg10, unsigned short arg11, unsigned short arg12,
        unsigned short arg13, unsigned short arg14, unsigned short arg15,
        char * buf, int len, lcd_callback_t funcptr, int opt_arg,
        cy_priv_t * priv_data);

static int lcd_add_lcp_entry(lcd_lcp_entry_t * lcp, cy_priv_t *
                             cy_private_data);

static unsigned short lcd_get_ushort(char * data, int start_address);

static void lcp_handle_mailbox(cy_priv_t * cy_private_data);

/** public data **/

/** private data **/

/** public functions **/


/*
 *  FUNCTION: lcd_get_cb_address
 *
 *  PARAMETERS:
 *    port_num              - Port number
 *    endpoint              - Endpoint number
 *    cy_private_data       - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function returns the callback address for the specified
 *    port number and endpoint.
 *
 *  RETURNS:
 *      NULL    - Enpoint number invalid
 *      address - Callback address for specified port number and endpoint
 *
 */
unsigned short lcd_get_cb_address(unsigned short port_num,
                                  int endpoint,
                                  cy_priv_t * cy_private_data)
{
    unsigned short cb_address = 0x0;

    lcd_priv_data_t *  lcd_private_data =
        (lcd_priv_data_t *) cy_private_data->lcd_priv;

    if ((endpoint >= 0) && (endpoint < MAX_NUM_ENDPOINTS)) {
        if ((port_num == PORT0) || port_num == PORT1) {
            cb_address = lcd_private_data->sie1_cb_addresses[endpoint];
        } else {
            cb_address = lcd_private_data->sie2_cb_addresses[endpoint];
        }
    }
    return cb_address;
}

/*
 *  FUNCTION: lcd_load_ibios
 *
 *  PARAMETERS:
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function copies an image of BIOS for internal RAM to RAM and
 *    executes the new BIOS.
 *
 *    If there is not a BIOS image to download, then the size of ibios should
 *    be zero in length.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */

#ifdef LOAD_IBIOS
#include "ibios.h"
#else
static unsigned char ibios[] = {};
#endif

int lcd_load_ibios(cy_priv_t * cy_priv)
{
    int response;
    unsigned short address;
    int download_length;
    char *download_data;


    if( sizeof(ibios) == 0 )
    {
        unsigned short intStat;

        lcd_hpi_read_mbx(cy_priv);
        lcd_hpi_read_status_reg(cy_priv);

        /* 
         * Had to add the write due to a bug in BIOS where they overwrite
         * the mailbox after initialization with garbage.  The read clears
         * any pending interrupts.
         */
        lcd_write_reg (SIE1MSG_REG, 0, cy_priv);
        lcd_read_reg (SIE1MSG_REG, &intStat, cy_priv);
        lcd_write_reg (SIE2MSG_REG, 0, cy_priv);
        lcd_read_reg (SIE2MSG_REG, &intStat, cy_priv);

        lcd_write_reg (HOST1_STAT_REG, 0xFFFF, cy_priv);
        lcd_write_reg (HOST2_STAT_REG, 0xFFFF, cy_priv);


        lcd_write_reg( HPI_IRQ_ROUTING_REG, SOFEOP1_TO_HPI_EN |
        SOFEOP1_TO_CPU_EN |
                       RESUME1_TO_HPI_EN | VBUS_TO_HPI_EN | ID_TO_HPI_EN,
                       cy_priv );

        lcd_read_reg (HOST1_IRQ_EN_REG, &intStat, cy_priv);
        intStat |= VBUS_IRQ_EN | ID_IRQ_EN;
        lcd_write_reg(HOST1_IRQ_EN_REG, intStat, cy_priv);

        return SUCCESS;
    }

    if( lcd_get_ushort(ibios, 0x00) != 0xc3b6 )
    {
        return ERROR;
    }


    address = lcd_get_ushort(ibios, 0x0e);

    download_length = lcd_get_ushort(ibios, 0x0b);

    download_data = &ibios[16];

    cy_dbg("loading ibios, addr: 0x%04x, length 0x%04x \n",
            address, download_length);


    response = lcd_download_code(address, download_length, download_data, 0, 0,
                                 cy_priv);

    if( response == ERROR )
    {
        cy_err("lcd_load_ibios: download error\n");
    }
    else
    {
        unsigned short intStat;
        mdelay(500);

        lcd_hpi_read_mbx(cy_priv);
        lcd_hpi_read_status_reg(cy_priv);

        /* 
         * Had to add the write due to a bug in BIOS where they overwrite
         * the mailbox after initialization with garbage.  The read clears
         * any pending interrupts.
         */
        lcd_write_reg (SIE1MSG_REG, 0, cy_priv);
        lcd_read_reg (SIE1MSG_REG, &intStat, cy_priv);
        lcd_write_reg (SIE2MSG_REG, 0, cy_priv);
        lcd_read_reg (SIE2MSG_REG, &intStat, cy_priv);

        lcd_write_reg (HOST1_STAT_REG, 0xFFFF, cy_priv);
        lcd_write_reg (HOST2_STAT_REG, 0xFFFF, cy_priv);

        lcd_write_reg( HPI_IRQ_ROUTING_REG, SOFEOP1_TO_HPI_EN |
        SOFEOP1_TO_CPU_EN |
                       RESUME1_TO_HPI_EN | VBUS_TO_HPI_EN | ID_TO_HPI_EN,
                       cy_priv );

        lcd_read_reg (HOST1_IRQ_EN_REG, &intStat, cy_priv);
        intStat |= VBUS_IRQ_EN | ID_IRQ_EN;
        lcd_write_reg(HOST1_IRQ_EN_REG, intStat, cy_priv);

        /* 
         * Move buffer area around ibios
         * Would use length, but ibios has a data area at the end
         */
        cy_priv->cy_buf_addr = 0x2100;

    }

    return response;
}


/*
 *  FUNCTION: lcd_init
 *
 *  PARAMETERS:
 *    download_filename_ptr - Download filename/path null terminated string
 *    cy_private_data       - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function initializes the LCD private data structure, the LCP entry
 *    list and downloads code to the ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    ERROR           - Failure
 */

int lcd_init(char * download_data,
             int download_length,
             lcd_callback_t funcptr,
             int opt_arg,
             cy_priv_t * cy_private_data)
{
    lcd_priv_data_t * lcd_private_data = NULL;
    int response = ERROR;
    unsigned short address;

    cy_dbg("lcd_init \n");

    /* Initialize private data */
    if (cy_private_data != NULL) {

        cy_private_data->lcd_priv = kmalloc(sizeof(lcd_priv_data_t), 0);

        if (cy_private_data->lcd_priv != NULL) {

            /* point to lcd private data */
            lcd_private_data = (lcd_priv_data_t *) cy_private_data->lcd_priv;

            /* Initialize LCP List */
            INIT_LIST_HEAD(&lcd_private_data->lcd_lcp_list);

            /* Initialize to unlocked state */
            lcd_private_data->lcd_lock = SPIN_LOCK_UNLOCKED;

            response = lcd_load_ibios(cy_private_data);

            /* do it again in case we loaded bios */
            INIT_LIST_HEAD(&lcd_private_data->lcd_lcp_list);


            /* Download RAM Code */
            if ((download_data != NULL) && (download_length > 0)) {

                /* Org address */
                address = lcd_get_ushort(download_data, 0xe);

                download_length = lcd_get_ushort(download_data, 0xb) - 2;

                if ((response = lcd_download_code(address, download_length,
                                      &download_data[16], funcptr,
                                      opt_arg, cy_private_data)) == ERROR) {
                    cy_err("lcd_init: download error\n");
                }

                if( response != ERROR )
                {
                    cy_private_data->cy_buf_addr = address + download_length;

                    /* Word align address */
                    if( (cy_private_data->cy_buf_addr & 1) != 0 )
                    {
                        cy_private_data->cy_buf_addr += 1;
                    }
                }
            }
            else {
                response = SUCCESS;
            }
        }
        else
            cy_err("lcd_init: kmalloc error\n");
    }
    else
        cy_err("lcd_init: Invalid cy_priv_t pointer\n");

    if( response == SUCCESS )
    {
        enable_irq(cy_private_data->cy_irq);
    }

    cy_dbg("lcd_init: %x \n", response);

    return response;
}


/*
 *  FUNCTION: lcd_complete_lcp_entry
 *
 *  PARAMETERS:
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function "completes" the first LCP entry by calling the "Callback"
 *    function (if any) and then deleting the entry from the list.  The function
 can
 *    be called from both a "task" and "interrupt" context.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_complete_lcp_entry(cy_priv_t * cy_private_data)
{
    lcd_priv_data_t * lcd_private_data =
        (lcd_priv_data_t *) cy_private_data->lcd_priv;

    lcd_lcp_entry_t * lcp;
    int response = ERROR;
    int lock_flags;

    if (!list_empty(&lcd_private_data->lcd_lcp_list)) {
        /* Get LCP entry */
        lcp = list_entry(lcd_private_data->lcd_lcp_list.next,
                         lcd_lcp_entry_t, list);

        /* Call callback (if necessary) */
        if (lcp->funcptr != NULL)
            lcp->funcptr(lcp->response, lcp->value, lcp->opt_arg,
                         lcp->cy_priv_data);

        /* Do not allow interrupts and etc. */
        spin_lock_irqsave(&lcd_private_data->lcd_lock, lock_flags);

        /* Remove entry from linked list */
        list_del(lcd_private_data->lcd_lcp_list.next);

        /* allow interrupts and etc. */
        spin_unlock_irqrestore(&lcd_private_data->lcd_lock, lock_flags);

        /* Free memory */
        kfree(lcp);

        /* Indicate success */
        response = SUCCESS;

        /* Start up next "valid" LCP entry */
        while (!list_empty(&lcd_private_data->lcd_lcp_list)) {
            /* Get LCP entry */
            lcp = list_entry(lcd_private_data->lcd_lcp_list.next,
                             lcd_lcp_entry_t, list);

            if (lcd_start_lcp(lcp, cy_private_data) == SUCCESS) {
                /* LCP entry successfully started */
                break;
            }
            else {
                /* Do not allow interrupts and etc. */
                spin_lock_irqsave(&lcd_private_data->lcd_lock, lock_flags);

                /* Remove "BAD" LCP entry and try again */
                list_del(lcd_private_data->lcd_lcp_list.next);

                /* allow interrupts and etc. */
                spin_unlock_irqrestore(&lcd_private_data->lcd_lock, lock_flags);

                /* Free memory */
                kfree(lcp);
            }
        }
    }
    else
        cy_err("lcd_complete_lcp_entry: No LCP entry to complete\n");

    return(response);
}


/*
 *  FUNCTION: lcd_command
 *
 *  PARAMETERS:
 *    command         - Command to perform
 *    command_arg     - Command argument
 *    chip_addr       - Offset Address of the ASIC
 *    port_number     - ASIC Port number
 *    funcptr         - Function (Callback) to call when command completes
 *                      (If not an immediate command)
 *    opt_arg         - Optional argument to pass to the Callback Function
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function handles the requested command.  If the action will not
 *    be completed immediately, an optional "Callback" function can be utilized.
 *
 *    The valid commands are:
 *      LCD_OFFER_HNP
 *      LCD_INITIATE_SRP
 *      LCD_DISABLE_OTG_VBUS
 *      LCD_ENABLE_OTG_VBUS
 *      LCD_START_SOF
 *      LCD_STOP_SOF
 *      LCD_CHANGE_TO_PERIPHERAL_ROLE
 *      LCD_CHANGE_TO_HOST_ROLE
 *      LCD_REMOTE_WAKEUP
 *      LCD_DATALINE_RESISTOR_TERMINIATION
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_command(int command,
                int command_arg,
                unsigned short port_number,
                lcd_callback_t funcptr,
                int opt_arg,
                cy_priv_t * cy_private_data)
{
    int response = ERROR;
    unsigned short reg_addr;
    unsigned short reg_value;
    unsigned short tmp_reg_value;
    lcd_lcp_entry_t * lcp;

    switch(command) {
        case LCD_OFFER_HNP:
            /* Offer HNP only on Port 0 */
            if (port_number == PORT0) {
                if ((response = lcd_read_reg(USB1_CTL_REG, &reg_value,
                                             cy_private_data)) == SUCCESS) {
                    /* Make sure we are in host mode */
                    if (reg_value & MODE_SEL) {
                        /* Build Register Value */
                        reg_value &= ~(A_SOF_EOP_EN | B_SOF_EOP_EN);

                        /* Stop SOFs on specified SIE */
                        response = lcd_write_reg(USB1_CTL_REG, reg_value,
                                                 cy_private_data);
                    }
                    else
                        response = ERROR;
                }
            }
            else
                cy_err("lcd_command: LCD_OFFER_HNP invalid Port\n");

            break;

        case LCD_INITIATE_SRP:
            /* Initiate SRP (Session Request Protocol) on specified SIE */
            lcp = lcd_create_lcp_entry(COMM_EXEC_INT,
                                       START_SRP_INT,0,0,0,0,
                                       0,0,0,0,0,
                                       0,0,0,0,0,
                                       0,0,funcptr,
                                       opt_arg,
                                       cy_private_data);

            /* Add lcp Entry */
            if (lcp != NULL)
                response = lcd_add_lcp_entry(lcp, cy_private_data);

            break;

        case LCD_DISABLE_OTG_VBUS:
            /* Disable VBUS */
            if (port_number == PORT0) {
                if ((response = lcd_read_reg(OTG_CTL_REG, &reg_value,
                                             cy_private_data)) == SUCCESS) {
                    /* Build Register Value */
                    reg_value &= ~(CHG_PUMP_EN);

                    /* Turn OTG VBUS OFF */
                    response = lcd_write_reg(OTG_CTL_REG, reg_value,
                                             cy_private_data);
                }
            }
            else
                cy_err("lcd_command: LCD_DISABLE_OTG_VBUS invalid Port\n");

            break;

        case LCD_ENABLE_OTG_VBUS:
            /* Enable VBUS */
            if (port_number == PORT0) {
                if ((response = lcd_read_reg(OTG_CTL_REG, &reg_value,
                                             cy_private_data)) == SUCCESS) {
                    /* Build Register Value */
                    reg_value |= (CHG_PUMP_EN);

                    /* Turn OTG VBUS ON */
                    response = lcd_write_reg(OTG_CTL_REG, reg_value,
                                             cy_private_data);
                }
            }
            else
                cy_err("lcd_command: LCD_ENABLE_OTG_VBUS invalid Port\n");

            break;

        case LCD_STOP_SOF:
            /* Determine USB Control register address and value for
             * specified SIE/port */
            switch(port_number) {
                case PORT0:
                    reg_addr = USB1_CTL_REG;
                    tmp_reg_value = ~(A_SOF_EOP_EN);
                    break;

                case PORT1:
                    reg_addr = USB1_CTL_REG;
                    tmp_reg_value = ~(B_SOF_EOP_EN);
                    break;

                case PORT2:
                    reg_addr = USB2_CTL_REG;
                    tmp_reg_value = ~(A_SOF_EOP_EN);
                    break;

                case PORT3:
                    reg_addr = USB2_CTL_REG;
                    tmp_reg_value = ~(B_SOF_EOP_EN);
                    break;

                default:
                    /* Error */
                    cy_err("lcd_command: LCD_STOP_SOF invalid Port 0x%X\n",
                           port_number);
                    return(response);
            }

            /* Read previous register value */
            if ((response = lcd_read_reg(reg_addr, &reg_value,
                            cy_private_data)) == SUCCESS) {
                /* Modify Register value */
                reg_value &= tmp_reg_value;

                /* Disable SOFs on specified SIE and Port */
                response = lcd_write_reg(reg_addr, reg_value, cy_private_data);
            }
            else
                cy_err("lcd_command: LCD_STOP_SOF error reading USB control"
                       " register address 0x%X\n", reg_addr);

            break;

        case LCD_START_SOF:
            /* Determine USB Control register address and value for
             * specified SIE/port */
            switch(port_number) {
                case PORT0:
                    reg_addr = USB1_CTL_REG;
                    tmp_reg_value = (A_SOF_EOP_EN);
                    break;

                case PORT1:
                    reg_addr = USB1_CTL_REG;
                    tmp_reg_value = (B_SOF_EOP_EN);
                    break;

                case PORT2:
                    reg_addr = USB2_CTL_REG;
                    tmp_reg_value = (A_SOF_EOP_EN);
                    break;

                case PORT3:
                    reg_addr = USB2_CTL_REG;
                    tmp_reg_value = (B_SOF_EOP_EN);
                    break;

                default:
                    /* Error */
                    cy_err("lcd_command: LCD_START_SOF invalid Port 0x%X\n",
                            port_number);
                    return(response);
            }

            /* Read previous register value */
            if ((response = lcd_read_reg(reg_addr, &reg_value,
                            cy_private_data)) == SUCCESS) {
                /* Modify Register value */
                reg_value |= tmp_reg_value;

                /* Disable SOFs on specified SIE and Port */
                response = lcd_write_reg(reg_addr, reg_value, cy_private_data);
            }
            else
                cy_err("lcd_command: LCD_START_SOF error reading USB control"
                       " register address 0x%X\n", reg_addr);

            break;

        case LCD_CHANGE_TO_PERIPHERAL_ROLE:
            /* not used */
            break;

        case LCD_CHANGE_TO_HOST_ROLE:
            /* Determine correct SIE interrupt */
            if ((port_number == PORT0) || (port_number == PORT1))
                reg_value = HUSB_SIE1_INIT_INT;
            else
                reg_value = HUSB_SIE2_INIT_INT;

            lcp = lcd_create_lcp_entry(COMM_EXEC_INT,
                                       reg_value,0,0,0,0,
                                       0,0,0,0,0,
                                       0,0,0,0,0,
                                       0,0,funcptr,
                                       opt_arg,
                                       cy_private_data);

            /* Add lcp Entry */
            if (lcp != NULL)
                response = lcd_add_lcp_entry(lcp, cy_private_data);

            break;

        case LCD_DATALINE_RESISTOR_TERMINATION:
            /* Determine USB Control register address for specified SIE/port */

            switch(port_number) {
                case PORT0:
                    /* 
                     * OTG SIE - Dataline pullups/pulldowns can be
                     * controlled with the OTG control register
                     */

                    if ((response = lcd_read_reg(OTG_CTL_REG, &reg_value,
                                    cy_private_data)) == SUCCESS) {
                        /* Mask */
                        reg_value &= 0xFC00;

                        if (command_arg & DPLUS_PULLUP)
                            reg_value |= DPLUS_PULLUP_EN;

                        if (command_arg & DMINUS_PULLUP)
                            reg_value |= DMINUS_PULLUP_EN;

                        if (command_arg & DPLUS_PULLDOWN)
                            reg_value |= DPLUS_PULLDOWN_EN;

                        if (command_arg & DMINUS_PULLDOWN)
                            reg_value |= DMINUS_PULLDOWN_EN;

                        /* Write OTG register */
                        response = lcd_write_reg(OTG_CTL_REG, reg_value,
                                                 cy_private_data);
                    }
                    break;

                case PORT1:
                    if ((response = lcd_read_reg(USB1_CTL_REG, &reg_value,
                                    cy_private_data)) == SUCCESS) {
                        /* Mask */
                        reg_value &= ~(B_RES_EN);

                        /* If a host allow dataline pulldowns to be
                         * enabled/disabled */
                        if (reg_value & MODE_SEL) {
                            /* The USB Control Reg only allows both
                             * dataline pulldowns */
                            if ((command_arg & DPLUS_PULLDOWN) ||
                                (command_arg & DMINUS_PULLDOWN))
                                reg_value |= B_RES_EN;

                            /* Make sure no pullups were asked for */
                            if ((command_arg & DPLUS_PULLUP) ||
                                (command_arg & DMINUS_PULLUP))
                                response = ERROR;
                            else
                                response = lcd_write_reg(USB1_CTL_REG,
                                                         reg_value,
                                                         cy_private_data);
                        }
                        else {
                            /* Must be a peripheral - allow appropriate
                             * dataline pullup */

                            /* Check for correct Pullup */
                            if ((!(reg_value & B_SPEED_SEL) &&
                                 (command_arg & DPLUS_PULLUP)) ||
                                ((reg_value & B_SPEED_SEL) &&
                                 (command_arg & DMINUS_PULLUP)))
                                reg_value |= B_RES_EN;

                            if (((reg_value & B_SPEED_SEL) &&
                                 (command_arg & DPLUS_PULLUP)) ||
                                (!(reg_value & B_SPEED_SEL) &&
                                 (command_arg & DMINUS_PULLUP)) ||
                                (command_arg & DPLUS_PULLDOWN) ||
                                (command_arg & DMINUS_PULLDOWN))
                                response = ERROR;
                            else
                                response = lcd_write_reg(USB1_CTL_REG,
                                                         reg_value,
                                                         cy_private_data);
                        }
                    }
                    break;

                case PORT2:
                    if ((response = lcd_read_reg(USB2_CTL_REG, &reg_value,
                                    cy_private_data)) == SUCCESS) {
                        /* Mask */
                        reg_value &= ~(A_RES_EN);

                        /* If a host allow dataline pulldowns to be
                         * enabled/disabled */
                        if (reg_value & MODE_SEL) {
                            /* The USB Control Reg only allows both
                             * dataline pulldowns */
                            if ((command_arg & DPLUS_PULLDOWN) ||
                                (command_arg & DMINUS_PULLDOWN))
                                reg_value |= A_RES_EN;

                            /* Make sure no pullups were asked for */
                            if ((command_arg & DPLUS_PULLUP) ||
                                (command_arg & DMINUS_PULLUP))
                                response = ERROR;
                            else
                                response = lcd_write_reg(USB2_CTL_REG,
                                                         reg_value,
                                                         cy_private_data);
                        }
                        else {
                            /* Must be a peripheral - allow appropriate
                             * dataline pullup */

                            /* Check for correct Pullup */
                            if ((!(reg_value & A_SPEED_SEL) &&
                                 (command_arg & DPLUS_PULLUP)) ||
                                ((reg_value & A_SPEED_SEL) &&
                                 (command_arg & DMINUS_PULLUP)))
                                reg_value |= A_RES_EN;

                            if (((reg_value & A_SPEED_SEL) &&
                                 (command_arg & DPLUS_PULLUP)) ||
                                (!(reg_value & A_SPEED_SEL) &&
                                 (command_arg & DMINUS_PULLUP)) ||
                                (command_arg & DPLUS_PULLDOWN) ||
                                (command_arg & DMINUS_PULLDOWN))
                                response = ERROR;
                            else
                                response = lcd_write_reg(USB2_CTL_REG,
                                                         reg_value,
                                                         cy_private_data);
                        }
                    }
                    break;

                case PORT3:
                    if ((response = lcd_read_reg(USB2_CTL_REG, &reg_value,
                                    cy_private_data)) == SUCCESS) {
                        /* Mask */
                        reg_value &= ~(B_RES_EN);

                        /* If a host allow dataline pulldowns to be
                         * enabled/disabled */
                        if (reg_value & MODE_SEL) {
                            /* The USB Control Reg only allows both
                             * dataline pulldowns */
                            if ((command_arg & DPLUS_PULLDOWN) ||
                                (command_arg & DMINUS_PULLDOWN))
                                reg_value |= B_RES_EN;

                            /* Make sure no pullups were asked for */
                            if ((command_arg & DPLUS_PULLUP) ||
                                (command_arg & DMINUS_PULLUP))
                                response = ERROR;
                            else
                                response = lcd_write_reg(USB2_CTL_REG,
                                                         reg_value,
                                                         cy_private_data);
                        }
                        else {
                            /* Must be a peripheral - allow appropriate
                             * dataline pullup */

                            /* Check for correct Pullup */
                            if ((!(reg_value & B_SPEED_SEL) &&
                                 (command_arg & DPLUS_PULLUP)) ||
                                ((reg_value & B_SPEED_SEL) &&
                                 (command_arg & DMINUS_PULLUP)))
                                reg_value |= B_RES_EN;

                            if (((reg_value & B_SPEED_SEL) &&
                                 (command_arg & DPLUS_PULLUP)) ||
                                (!(reg_value & B_SPEED_SEL) &&
                                 (command_arg & DMINUS_PULLUP)) ||
                                (command_arg & DPLUS_PULLDOWN) ||
                                (command_arg & DMINUS_PULLDOWN))
                                response = ERROR;
                            else
                                response = lcd_write_reg(USB2_CTL_REG,
                                                         reg_value,
                                                         cy_private_data);
                        }
                    }
                    break;

                default:
                    /* Unknown port */
                    cy_err("lcd_command : LCD_DATALINE_TERMINATION"
                           " invalid Port 0x%X\n", port_number);
                break;
            }

            break;

        case LCD_REMOTE_WAKEUP:
            /* Determine correct SIE number */
            if ((port_number == PORT0) || (port_number == PORT1))
                reg_value = 1;
            else
                reg_value = 2;

            /* Initiate Remote Wakeup LCP for specified SIE */
            lcp = lcd_create_lcp_entry(COMM_EXEC_INT,
                                       REMOTE_WAKEUP_INT,reg_value,0,0,0,
                                       0,0,0,0,0,
                                       0,0,0,0,0,
                                       0,0,funcptr,
                                       opt_arg,
                                       cy_private_data);

            /* Add lcp Entry */
            if (lcp != NULL)
                response = lcd_add_lcp_entry(lcp, cy_private_data);

            break;

        default:
            /* Unknown command */
            cy_err("lcd_command: error unsupported command %d\n", command);
            break;
    }

    return(response);
}


/*
 *  FUNCTION: lcd_send_data
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    port_number     - ASIC Port number
 *    endpoint        - USB Endpoint number
 *    byte_length     - Length of Control data to send
 *    data            - Control Data to send pointer
 *    funcptr         - Function (Callback) to call when the send completes
 *    opt_arg         - Optional argument to pass to the Callback Function
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function sends data to the specified Port and Endpoint.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_send_data(unsigned short chip_addr,
                  unsigned short port_number,
                  unsigned short endpoint,
                  int byte_length,
                  char * data,
                  lcd_callback_t funcptr,
                  int opt_arg,
                  cy_priv_t * cy_private_data)

{
    int response = ERROR;
    lcd_lcp_entry_t * lcp;
    unsigned short int_num;


    cy_dbg("lcd_send_data %x %x \n", chip_addr, byte_length);

    /* Write control structure to ASIC Memory */
    if (lcd_write_memory(chip_addr, byte_length,
                         data, cy_private_data) == SUCCESS) {

        /* Determine correct SIE ptr address */
        if ((port_number == PORT0) || (port_number == PORT1))
            int_num = SUSB1_SEND_INT;
        else
            int_num = SUSB2_SEND_INT;

        lcp = lcd_create_lcp_entry(COMM_EXEC_INT,
                                   int_num,0,endpoint,0,0,
                                   0,0,0,0,chip_addr,
                                   0,0,0,0,0,
                                   0,0,funcptr,
                                   opt_arg,
                                   cy_private_data);

        /* Add lcp Entry */
        if (lcp != NULL)
            response = lcd_add_lcp_entry(lcp, cy_private_data);
    }

    return response;
}


/*
 *  FUNCTION: lcd_recv_data
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    port_number     - ASIC Port number
 *    endpoint        - USB Endpoint number
 *    byte_length     - Length of data to receive
 *    data            - Control Data to receive pointer (if any)
 *    funcptr         - Function (Callback) to call when the receive completes
 *    opt_arg         - Optional argument to pass to the Callback Function
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function receives data from the specified Port and Endpoint.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_recv_data(unsigned short chip_addr,
                  unsigned short port_number,
                  unsigned short endpoint,
                  int byte_length,
                  char * data,
                  lcd_callback_t funcptr,
                  int opt_arg,
                  cy_priv_t * cy_private_data)
{
    int response = ERROR;
    lcd_lcp_entry_t * lcp;
    unsigned short int_num;

    cy_dbg("lcd_recv_data %x %x \n", chip_addr, byte_length);

    /* Write control structure to ASIC Memory */
    if (lcd_write_memory(chip_addr, byte_length, data,
                         cy_private_data) == SUCCESS) {
        /* Determine correct SIE ptr address */
        if ((port_number == PORT0) || (port_number == PORT1))
            int_num = SUSB1_RECEIVE_INT;
        else
            int_num = SUSB2_RECEIVE_INT;

        lcp = lcd_create_lcp_entry(COMM_EXEC_INT,
                                   int_num,0,endpoint,0,0,
                                   0,0,0,0,chip_addr,
                                   0,0,0,0,0,
                                   0,0,funcptr,
                                   opt_arg,
                                   cy_private_data);

        /* Add lcp Entry */
        if (lcp != NULL)
            response = lcd_add_lcp_entry(lcp, cy_private_data);
    }

    return response;
}


/*
 *  FUNCTION: lcd_send_tdlist
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    port_number     - ASIC Port number
 *    byte_length     - Length of data to send
 *    data            - Data to send pointer (TD List)
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function sends the TD list to the specified port.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_send_tdlist(unsigned short chip_addr,
                    int port_number,
                    int byte_length,
                    char * data,
                    cy_priv_t * cy_private_data)
{
    int response = ERROR;
    char * ptr = (char *) &chip_addr;
    unsigned short addr;

    cy_dbg("lcd_send_tdlist \n");
    /* Write TD to ASIC Memory */
    if (lcd_write_memory(chip_addr, byte_length, data,
                         cy_private_data) == SUCCESS) {
        /* Determine correct SIE ptr address */
        if ((port_number == PORT0) || (port_number == PORT1))
            addr = HUSB_SIE1_pCurrentTDPtr;
        else
            addr = HUSB_SIE2_pCurrentTDPtr;

        /* Write address to the SIEx TD pointer */
        response = lcd_write_memory(addr, 2, ptr, cy_private_data);
    }

    return response;
}

/*
 *  FUNCTION: lcd_get_tdstatus
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    byte_length     - Length of data to receive
 *    data            - Data to read pointer (TD Status)
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function retrieves the TD Status from the specified address.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_get_tdstatus(unsigned short chip_addr,
                     int byte_length,
                     char * data,
                     cy_priv_t * cy_private_data)
{
    int response = ERROR;
    cy_dbg("lcd_get_tdstatus \n");

    /* Read TD from ASIC Memory */
    if (byte_length > 0)
        response = lcd_read_memory(chip_addr, byte_length, data,
                                   cy_private_data);

    return response;
}



/*
 *  FUNCTION: lcd_read_memory
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    byte_length     - Length of data to read
 *    data            - Data to receive pointer
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads data from the specified address and length on the
 ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_read_memory(unsigned short chip_addr,
                    int byte_length,
                    char * data,
                    cy_priv_t * cy_private_data)
{
    int num_words = 0;
    unsigned short short_int;
    /* lcd_lcp_entry_t * lcp; */

    /* Check for unaligned address */
    if ((chip_addr & 0x1) == 0x1) {

        /* Read Word */
        short_int = lcd_hpi_read_word((chip_addr - 1), cy_private_data);

        /* Save upper byte */
        *data++ = (char) ((unsigned short) short_int >> 0x8);

        /* Update address */
        chip_addr = chip_addr + 1;

        /* Update length */
        byte_length--;
    }

    /* Determine number of words to read */
    num_words = byte_length >> 1;

    /* Check for words to read */
    if (num_words > 0) {

        lcd_hpi_read_words_big(chip_addr, (unsigned short *) data, num_words,
                           cy_private_data);

        byte_length -= (2 * num_words);

        chip_addr += (2 * num_words);
    }

    /* Check for a remaining byte to read*/
    if (byte_length > 0) {

        /* Read Word */
        short_int = lcd_hpi_read_word(chip_addr, cy_private_data);

        /* Save lower byte */
        *(data + 2*num_words) = (char) ((unsigned short) short_int >> 0x0);

        /* Update length */
        byte_length--;
    }

    /* Consistancy check */
    if (byte_length != 0) {
        cy_err("lcd_read_memory: Internal error\n");
    }

    return(SUCCESS);
}

/*
 *  FUNCTION: lcd_read_memory
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    byte_length     - Length of data to read
 *    data            - Data to receive pointer
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads data from the specified address and length on the
 ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_read_memory_little(unsigned short chip_addr,
                    int byte_length,
                    char * data,
                    cy_priv_t * cy_private_data)
{
    int num_words = 0;
    unsigned short short_int;
    /* lcd_lcp_entry_t * lcp; */

    /* Check for unaligned address */
    if ((chip_addr & 0x1) == 0x1) {

        /* Read Word */
        short_int = lcd_hpi_read_word((chip_addr - 1), cy_private_data);

        /* Save upper byte */
        *data++ = (char) ((unsigned short) short_int >> 0x8);

        /* Update address */
        chip_addr = chip_addr + 1;

        /* Update length */
        byte_length--;
    }

    /* Determine number of words to read */
    num_words = byte_length >> 1;

    /* Check for words to read */
    if (num_words > 0) {

        lcd_hpi_read_words(chip_addr, (unsigned short *) data, num_words,
                           cy_private_data);

        byte_length -= (2 * num_words);

        chip_addr += (2 * num_words);
    }

    /* Check for a remaining byte to read*/
    if (byte_length > 0) {

        /* Read Word */
        short_int = lcd_hpi_read_word(chip_addr, cy_private_data);

        /* Save lower byte */
        *(data + 2*num_words) = (char) ((unsigned short) short_int >> 0x0);

        /* Update length */
        byte_length--;
    }

    /* Consistancy check */
    if (byte_length != 0) {
        cy_err("lcd_read_memory: Internal error\n");
    }
    return(SUCCESS);
}

/*
 *  FUNCTION: lcd_write_memory
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    byte_length     - Length of data to write
 *    data            - Data to write pointer
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes data to the specfied address and length on the ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_write_memory(unsigned short chip_addr,
                     int byte_length,
                     char * data,
                     cy_priv_t * cy_private_data)
{
    int response = ERROR;
    int num_words = 0;
    unsigned short short_int;
    unsigned short tmp_short_int;
    /* lcd_lcp_entry_t * lcp; */

    /* Check if the memory region is writable or not */
    cy_dbg("lcd_write_memory %x\n", byte_length);
    if (lcd_writeable_region(chip_addr, byte_length) == SUCCESS) {

        /* Check for unaligned address */
        if ((chip_addr & 0x1) == 0x1) {

            cy_dbg("lcd_write_memory \n");
            /* Read Word */
            short_int = lcd_hpi_read_word((chip_addr - 1), cy_private_data);

            /* Modify upper byte */
            tmp_short_int = (unsigned short) (0x00FF & *data++);
            short_int = ((0x00FF & short_int) | (tmp_short_int << 8));

            /* Write Word */
            lcd_hpi_write_word((chip_addr - 1), short_int, cy_private_data);

            /* Update address */
            chip_addr = chip_addr + 1;

            /* Update length */
            byte_length--;
        }

        /* Determine number of words to write */
        num_words = (int) byte_length/2;

        /* Check for words to write */
        if (num_words > 0) {
            lcd_hpi_write_words(chip_addr, (unsigned short *) data, num_words,
                                cy_private_data);

            byte_length -= (2 * num_words);

            chip_addr += (2 * num_words);

            data += (2 * num_words);
        }

        /* Check for a remaining byte to read*/
        if (byte_length > 0) {

            /* Read Word */
            short_int = lcd_hpi_read_word(chip_addr, cy_private_data);

            /* Modify lower byte */
            tmp_short_int = (unsigned short) (0x00FF & *data++);
            short_int = ((0xFF00 & short_int) | tmp_short_int);

            /* Write Word */
            lcd_hpi_write_word(chip_addr, short_int, cy_private_data);

            /* Update length */
            byte_length--;
            cy_dbg("lcd_write_memory \n");
        }

        /* Consistancy check */
        if (byte_length != 0) {
            cy_err("lcd_write_memory: Internal error\n");
        }

        response = SUCCESS;
    }
    else
        cy_err ("lcd_write_memory: this memory region is not writable\n");

    return(response);
}


/*
 *  FUNCTION: lcd_write_memory
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    byte_length     - Length of data to write
 *    data            - Data to write pointer
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes data to the specfied address and length on the ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_write_memory_big(unsigned short chip_addr,
                     int byte_length,
                     char * data,
                     cy_priv_t * cy_private_data)
{
    int response = ERROR;
    int num_words = 0;
    unsigned short short_int;
    unsigned short tmp_short_int;
    /* lcd_lcp_entry_t * lcp; */

    /* Check if the memory region is writable or not */
    cy_dbg("lcd_write_memory %x\n", byte_length);
    if (lcd_writeable_region(chip_addr, byte_length) == SUCCESS) {

        /* Check for unaligned address */
        if ((chip_addr & 0x1) == 0x1) {

            /* Read Word */
            short_int = lcd_hpi_read_word((chip_addr - 1), cy_private_data);

            /* Modify upper byte */
            tmp_short_int = (unsigned short) (0x00FF & *data++);
            short_int = ((0x00FF & short_int) | (tmp_short_int << 8));

            /* Write Word */
            lcd_hpi_write_word((chip_addr - 1), short_int, cy_private_data);

            /* Update address */
            chip_addr = chip_addr + 1;

            /* Update length */
            byte_length--;
        }

        /* Determine number of words to write */
        num_words = (int) byte_length/2;

        /* Check for words to write */
        if (num_words > 0) {
            lcd_hpi_write_words_big(chip_addr, (unsigned short *) data,
            num_words,
                                cy_private_data);

            byte_length -= (2 * num_words);

            chip_addr += (2 * num_words);

            data += (2 * num_words);
        }

        /* Check for a remaining byte to read*/
        if (byte_length > 0) {

            /* Read Word */
            short_int = lcd_hpi_read_word(chip_addr, cy_private_data);

            /* Modify lower byte */
            tmp_short_int = (unsigned short) (0x00FF & *data++);
            short_int = ((0xFF00 & short_int) | tmp_short_int);

            /* Write Word */
            lcd_hpi_write_word(chip_addr, short_int, cy_private_data);

            /* Update length */
            byte_length--;
        }

        /* Consistancy check */
        if (byte_length != 0) {
            cy_err("lcd_write_memory: Internal error\n");
        }

        response = SUCCESS;
    }
    else
        cy_err ("lcd_write_memory: this memory region is not writable\n");

    return(response);
}


/*
 *  FUNCTION: lcd_read_xmemory
 *
 *  PARAMETERS:
 *    xmen_addr       - Offset Address of the external memory on the ASIC
 *    byte_length     - Length of data to read
 *    data            - Data to read pointer
 *    funcptr         - Function (Callback) to call when the read completes
 *    opt_arg         - Optional argument to pass to the Callback Function
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads data from the specfied external address and length
 *    through the ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_read_xmemory(unsigned short xmem_addr,
                     unsigned short byte_length,
                     char * data,
                     lcd_callback_t funcptr,
                     int opt_arg,
                     cy_priv_t * cy_private_data)
{
    int response = ERROR;
    lcd_lcp_entry_t * lcp;

    /* Only allow reads of 32 bytes or less */
    if (byte_length > 32)
        return (response);

    /* Set Xmemory Address */
    lcd_hpi_write_word(COMM_MEM_ADDR, xmem_addr, cy_private_data);

    /* Set data length */
    lcd_hpi_write_word(COMM_MEM_LEN, byte_length, cy_private_data);

    /* Build LCP entry */

    lcp = lcd_create_lcp_entry(COMM_READ_XMEM,
                               0,0,0,0,0,
                               0,0,0,0,0,
                               0,0,0,0,0,
                               data,(int) byte_length,
                               funcptr,
                               opt_arg,
                               cy_private_data);

    /* Add lcp Entry */

    if (lcp != NULL)
        response = lcd_add_lcp_entry(lcp, cy_private_data);

    return(response);
}

/*
 *  FUNCTION: lcd_write_xmemory
 *
 *  PARAMETERS:
 *    xmen_addr       - Offset Address of the external memory on the ASIC
 *    byte_length     - Length of data to write
 *    data            - Data to write pointer
 *    funcptr         - Function (Callback) to call when the write completes
 *    opt_arg         - Optional argument to pass to the Callback Function
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes data to the specfied external address and length
 *    through the ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_write_xmemory(unsigned short xmem_addr,
                      unsigned short byte_length,
                      char * data,
                      lcd_callback_t funcptr,
                      int opt_arg,
                      cy_priv_t * cy_private_data)
{
    int response = ERROR;
    lcd_lcp_entry_t * lcp;

    /* Only allow writes of 32 bytes or less */
    if (byte_length > 32)
        return (response);

    /* Write Xmemory */

    /* Set Xmemory Address */
    lcd_hpi_write_word(COMM_MEM_ADDR, xmem_addr, cy_private_data);

    /* Set data length */
    lcd_hpi_write_word(COMM_MEM_LEN, byte_length, cy_private_data);

    /* Write Data */
    lcd_hpi_write_words(COMM_XMEM_BUF, (unsigned short *) data,
                        (byte_length/2), cy_private_data);

    /* Build LCP entry */

    lcp = lcd_create_lcp_entry(COMM_WRITE_XMEM,
                               0,0,0,0,0,
                               0,0,0,0,0,
                               0,0,0,0,0,
                               0,0,funcptr,
                               opt_arg,
                               cy_private_data);

    /* Add lcp Entry */

    if (lcp != NULL)
        response = lcd_add_lcp_entry(lcp, cy_private_data);

    return(response);
}


/*
 *  FUNCTION: lcd_lcp_write_reg
 *
 *  PARAMETERS:
 *    reg_addr        - Offset Register Address of the ASIC
 *    reg_value       - Register Value
 *    flag            - Logic Flag
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes data to the specified register address
 *    on the ASIC using LCP.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */

int lcd_lcp_write_reg(  unsigned short reg_addr,
                        unsigned short reg_value,
                        unsigned short flag,
                        cy_priv_t * cy_private_data)
{
    int response = ERROR;
    lcd_lcp_entry_t * lcp;

    /* Build LCP entry */

    lcp = lcd_create_lcp_entry(COMM_WRITE_CTRL_REG,
                               reg_addr, reg_value, flag, 0,0,
                               0,0,0,0,0,
                               0,0,0,0,0,
                               0,0,0,0,
                               cy_private_data);

    /* Add lcp Entry */

    if (lcp != NULL)
        response = lcd_add_lcp_entry(lcp, cy_private_data);

    return(response);
}



/*
 *  FUNCTION: lcd_read_reg
 *
 *  PARAMETERS:
 *    reg_addr        - Offset Register Address of the ASIC
 *    reg_value       - Register Value pointer
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads data from the specified register address
 *    on the ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_read_reg(unsigned short reg_addr,
                 unsigned short *reg_value,
                 cy_priv_t * cy_private_data)
{
    /* lcd_lcp_entry_t * lcp; */

    /* Hardware Specific Code to Write to the ASIC via HPI Port */
    *reg_value = lcd_hpi_read_word(reg_addr, cy_private_data);

    return (SUCCESS);
}

/*
 *  FUNCTION: lcd_write_reg
 *
 *  PARAMETERS:
 *    reg_addr        - Offset Register Address of the ASIC
 *    reg_value       - Register Value
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes data to the specified register address
 *    on the ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_write_reg(unsigned short reg_addr,
                  unsigned short reg_value,
                  cy_priv_t * cy_private_data)
{
    /* lcd_lcp_entry_t * lcp; */

    /* Hardware Specific Code to Write to the ASIC via HPI Port */
    lcd_hpi_write_word(reg_addr, reg_value, cy_private_data);

    return (SUCCESS);
}


/*
 *  FUNCTION: lcd_exec_interrupt
 *
 *  PARAMETERS:
 *    int_data        - Integer Data structure pointer
 *    funcptr         - Function (Callback) to call when the execute interrupt
 *                      completes
 *    opt_arg         - Optional argument to pass to the Callback Function
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function executes the specificed interrupt handler on the ASIC.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */

int lcd_exec_interrupt(lcd_int_data_t *int_data,
                       lcd_callback_t funcptr,
                       int opt_arg,
                       cy_priv_t * cy_private_data)
{
    int response = ERROR;
    lcd_lcp_entry_t * lcp;

    lcp = lcd_create_lcp_entry(COMM_EXEC_INT, int_data->int_num,
                               int_data->reg[0], int_data->reg[1],
                               int_data->reg[2], int_data->reg[3],
                               int_data->reg[4], int_data->reg[5],
                               int_data->reg[6], int_data->reg[7],
                               int_data->reg[8], int_data->reg[9],
                               int_data->reg[10], int_data->reg[11],
                               int_data->reg[12], int_data->reg[13],
                               0, 0, funcptr, opt_arg, cy_private_data);

    /* Add lcp Entry */
    if (lcp != NULL)
        response = lcd_add_lcp_entry(lcp, cy_private_data);

    return response;
}


/*
 *  FUNCTION: lcd_download_code
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    byte_length     - Length of data to download (write)
 *    data            - Data to download pointer
 *    funcptr         - Function (Callback) to call when the download completes
 *    opt_arg         - Optional argument to pass to the Callback Function
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    ERROR           - Failure
 */

int lcd_download_code(unsigned short chip_addr,
                      int byte_length,
                      char * data,
                      lcd_callback_t funcptr,
                      int opt_arg,
                      cy_priv_t * cy_private_data)
{
    int response = ERROR;
    lcd_lcp_entry_t * lcp;

    if (byte_length > 0) {

        /* cy_dbg_on = 0; */

        response = lcd_write_memory(chip_addr, byte_length, data,
                                    cy_private_data);
        /* cy_dbg_on = 1; */

        if ( response == SUCCESS) {

            lcp = lcd_create_lcp_entry(COMM_JUMP2CODE,
                                       chip_addr,0,0,0,0,
                                       0,0,0,0,0,
                                       0,0,0,0,0,
                                       0,0,funcptr,
                                       opt_arg,
                                       cy_private_data);
            if (lcp != NULL)
                response = lcd_add_lcp_entry(lcp, cy_private_data);
        }
    }

    return response;
}

/*
 *  FUNCTION: lcd_hpi_write_status_reg
 *
 *  PARAMETERS:
 *    value           - Length
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes to the hpi mailbox
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
void lcd_hpi_write_status_reg(unsigned short value,
                       cy_priv_t * cy_private_data)
{
    cy_dbg("write_status_reg: addr = 0x%x, value = 0x%04x \n",
           HPI_STATUS_ADDR  + cy_private_data->cy_addr, value);
    outw (value, HPI_STATUS_ADDR  + cy_private_data->cy_addr);
}

/*
 *  FUNCTION: lcd_hpi_read_status_reg
 *
 *  PARAMETERS:
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads from the hpi status register
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
unsigned short lcd_hpi_read_status_reg(cy_priv_t * cy_private_data)
{
    unsigned short value;

    value = inw(HPI_STATUS_ADDR + cy_private_data->cy_addr);
    /* cy_dbg("read_status_reg: addr = 0x%x, value = 0x%04x",
           HPI_STATUS_ADDR  + cy_private_data->cy_addr, value); */

    return value;
}


/*
 *  FUNCTION: lcd_hpi_write_mbx
 *
 *  PARAMETERS:
 *    value           - Length
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes to the hpi mailbox
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
void lcd_hpi_write_mbx(unsigned short value,
                       cy_priv_t * cy_private_data)
{
    cy_dbg("lcd_hpi_write_mbx: 0x%04x \n", value);
    outw (value, HPI_MAILBOX_ADDR + cy_private_data->cy_addr);
}


/*
 *  FUNCTION: lcd_hpi_read_mbx
 *
 *  PARAMETERS:
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads from hpi
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
unsigned short lcd_hpi_read_mbx(cy_priv_t * cy_private_data)
{
    unsigned short value;

    value = inw(HPI_MAILBOX_ADDR + cy_private_data->cy_addr);
    /* cy_dbg("read_mbx: 0x%04x", value); */

    return value;
}


/*
 *  FUNCTION: lcd_hpi_write_word
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    value           - Value to write
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes to hpi
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
void lcd_hpi_write_word(unsigned short chip_addr,
                        unsigned short value,
                        cy_priv_t * cy_private_data)
{
    spinlock_t lock;
    unsigned long int_flags;

    /* cy_dbg("lcd_hpi_write_word: addr=0x%04x, value=0x%04x \n", 
               chip_addr, value); */
    spin_lock_irqsave(&lock, int_flags);
        outw (chip_addr, HPI_ADDR_ADDR + cy_private_data->cy_addr);
        outw (value, HPI_DATA_ADDR + cy_private_data->cy_addr);
    spin_unlock_irqrestore(&lock, int_flags);
}


/*
 *  FUNCTION: lcd_hpi_read_word
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads from hpi
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
unsigned short lcd_hpi_read_word(unsigned short chip_addr,
                                 cy_priv_t * cy_private_data)
{
    unsigned short value;
    spinlock_t lock;
    unsigned long int_flags;


    spin_lock_irqsave(&lock, int_flags);
        outw (chip_addr, HPI_ADDR_ADDR + cy_private_data->cy_addr);
        value = inw(HPI_DATA_ADDR + cy_private_data->cy_addr);
    spin_unlock_irqrestore(&lock, int_flags);

    /* cy_dbg("lcd_hpi_read_word: addr=0x%04x, value=0x%04x \n", 
		chip_addr, value); */

    return(value);
}


/*
 *  FUNCTION: lcd_hpi_write_words
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    data            - data pointer
 *    num_words       - Length
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes words to hpi
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
void lcd_hpi_write_words(unsigned short chip_addr,
                         unsigned short *data,
                         int num_words,
                         cy_priv_t * cy_private_data)
{
    int i;
    spinlock_t lock;
    unsigned long int_flags;
    unsigned short tempdata;


    /* Hardware Specific Code to Write to the ASIC via HPI Port */
    cy_dbg("write_words: addr=0x%04x, num_words=%d \n", chip_addr, num_words);

    spin_lock_irqsave(&lock, int_flags);
        outw (chip_addr, HPI_ADDR_ADDR + cy_private_data->cy_addr);
            for (i=0; i<num_words; i++) {
            /* cy_dbg(" value = 0x%04x \n", *data); */
            tempdata = ((*data) & 0xFF)<< 8;
            tempdata =  tempdata | ((*data) & 0xFF00) >> 8;

            /* outw (*data++, HPI_DATA_ADDR + cy_private_data->cy_addr); */
            outw (tempdata, HPI_DATA_ADDR + cy_private_data->cy_addr); data++;
        }

    spin_unlock_irqrestore(&lock, int_flags);
}


/*
 *  FUNCTION: lcd_hpi_write_words
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    data            - data pointer
 *    num_words       - Length
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function writes words to hpi
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
void lcd_hpi_write_words_big(unsigned short chip_addr,
                         unsigned short *data,
                         int num_words,
                         cy_priv_t * cy_private_data)
{
    int i;
    spinlock_t lock;
    unsigned long int_flags;
    unsigned short tempdata;


    /* Hardware Specific Code to Write to the ASIC via HPI Port */
    cy_dbg("write_words: addr=0x%04x, num_words=%d \n", chip_addr, num_words);

    spin_lock_irqsave(&lock, int_flags);
        outw (chip_addr, HPI_ADDR_ADDR + cy_private_data->cy_addr);
        for (i=0; i<num_words; i++) {
            /* cy_dbg(" value = 0x%04x \n", *data); */

              outw (*data++, HPI_DATA_ADDR + cy_private_data->cy_addr);
        }
     spin_unlock_irqrestore(&lock, int_flags);
}

/*
 *  FUNCTION: lcd_hpi_read_words
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    data            - data pointer
 *    num_words       - Length
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads words from hpi
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
void lcd_hpi_read_words_big(unsigned short chip_addr,
                        unsigned short *data,
                        int num_words,
                        cy_priv_t * cy_private_data)
{
    int i;
    spinlock_t lock;
    unsigned long int_flags;
    unsigned short tempdata;
    unsigned short tempdata1;


    /* Hardware Specific Code to Read From the ASIC via HPI Port */
    cy_dbg("read_words: addr=0x%04x, num of words 0x%04x \n", chip_addr, num_words);

    spin_lock_irqsave(&lock, int_flags);
        outw (chip_addr, HPI_ADDR_ADDR + cy_private_data->cy_addr);
        for (i=0; i<num_words; i++) {
            *data++ = inw (HPI_DATA_ADDR + cy_private_data->cy_addr);
            /* cy_dbg(" value = 0x%04x \n", *(data-1)); */
        }

    spin_unlock_irqrestore(&lock, int_flags);
}



/*
 *  FUNCTION: lcd_hpi_read_words
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    data            - data pointer
 *    num_words       - Length
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function reads words from hpi
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
void lcd_hpi_read_words(unsigned short chip_addr,
                        unsigned short *data,
                        int num_words,
                        cy_priv_t * cy_private_data)
{
    int i;
    spinlock_t lock;
    unsigned long int_flags;
    unsigned short tempdata;
    unsigned short tempdata1;


    /* Hardware Specific Code to Read From the ASIC via HPI Port */
    cy_dbg("read_words: addr=0x%04x, num of words 0x%04x \n", chip_addr, num_words);

    spin_lock_irqsave(&lock, int_flags);
        outw (chip_addr, HPI_ADDR_ADDR + cy_private_data->cy_addr);

            for (i=0; i<num_words; i++) {

            tempdata1 = inw (HPI_DATA_ADDR + cy_private_data->cy_addr);
            tempdata = ((tempdata1) & 0xFF)<< 8;
            tempdata =  tempdata | (((tempdata1) & 0xFF00) >> 8);

            *data++ = tempdata;

            /* *data++ = inw (HPI_DATA_ADDR + cy_private_data->cy_addr); */

            /* cy_dbg(" value = 0x%04x \n", *(data-1)); */
        }

    spin_unlock_irqrestore(&lock, int_flags);
}

/** private functions **/


/*
 *  FUNCTION: lcd_get_ushort
 *
 *  PARAMETERS:
 *    data            - Data
 *    start_address   - Start address in data to convert
 *
 *  DESCRIPTION:
 *    This function builds an unsigned short from the data
 *    provided.
 *
 *  RETURNS:
 *    An unsigned short.
 */

unsigned short lcd_get_ushort(char * data, int start_address)
{
    unsigned short value = 0;

    value = (((unsigned short) *(data + start_address + 1) << 8) |
             ((unsigned short) *(data + start_address)));

    return value;
}



/*
 *  FUNCTION: lcd_writeable_region
 *
 *  PARAMETERS:
 *    chip_addr       - Offset Address of the ASIC
 *    byte_length     - Length
 *
 *  DESCRIPTION:
 *    This function determines if the address and length falls within
 *    a valid "writeable" region.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_writeable_region(unsigned short chip_addr,
                         int byte_length)
{
    int response = ERROR;

    /* Check that address is in a valid writable range */
    if (((int) chip_addr + byte_length) <= 0xFFFF)
        response = SUCCESS;

    return response;
}


/*
 *  FUNCTION: lcd_start_lcp
 *
 *  PARAMETERS:
 *    lcp             - LCP entry pointer
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function submits the (first queued) LCP entry to the ASIC
 *    for processing with the correct arguments.
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_start_lcp(lcd_lcp_entry_t * lcp,
                  cy_priv_t * cy_private_data)
{
    int response = SUCCESS;
    cy_dbg("lcd_start_lcp enter 0x%x \n", lcp->command);

    /* Handle command */
    switch (lcp->command) {
        case COMM_RESET:
            cy_dbg("COMM_RESET \n");

            /* Send CMD */
            lcd_hpi_write_mbx(COMM_RESET, cy_private_data);

            break;

        case COMM_JUMP2CODE:
            cy_dbg("COMM_JUMP2CODE \n");
           /* Set Jump Address */
            lcd_hpi_write_word(COMM_CODE_ADDR, lcp->arg[0], cy_private_data);

            /* Send CMD */
            lcd_hpi_write_mbx(COMM_JUMP2CODE, cy_private_data);
            break;

        case COMM_CALL_CODE:
            cy_dbg("COMM_CALL_CODE \n");
            /* Set Call Address */
            lcd_hpi_write_word(COMM_CODE_ADDR, lcp->arg[0], cy_private_data);

            /* Send CMD */
            lcd_hpi_write_mbx(COMM_CALL_CODE, cy_private_data);
            break;

        case COMM_WRITE_CTRL_REG:
            cy_dbg("COMM_WRITE_CTRL_REG \n");

            /* Set Control Reg Address */
            lcd_hpi_write_word(COMM_CTRL_REG_ADDR, lcp->arg[0],
            cy_private_data);


            /* Set Value */
            lcd_hpi_write_word(COMM_CTRL_REG_DATA, lcp->arg[1], cy_private_data);

            /* Set flag */
            lcd_hpi_write_word(COMM_CTRL_REG_LOGIC, lcp->arg[2],
            cy_private_data);

            /* Send CMD */
            lcd_hpi_write_mbx(COMM_WRITE_CTRL_REG, cy_private_data);

            break;

        case COMM_READ_CTRL_REG:
            cy_dbg("COMM_READ_CTRL_REG \n");

            /* Set Control Reg Address */
            lcd_hpi_write_word(COMM_CTRL_REG_ADDR, lcp->arg[0], cy_private_data);

            /* Send CMD */
            lcd_hpi_write_mbx(COMM_READ_CTRL_REG, cy_private_data);

            break;

        case COMM_EXEC_INT:

            cy_dbg("COMM_EXEC_INT \n");
            /* Set Int Number */

            lcd_hpi_write_word(COMM_INT_NUM, lcp->arg[0], cy_private_data);


            /* Set Register values */
            lcd_hpi_write_word(COMM_R0, lcp->arg[1], cy_private_data);
            lcd_hpi_write_word(COMM_R1, lcp->arg[2], cy_private_data);
            lcd_hpi_write_word(COMM_R2, lcp->arg[3], cy_private_data);
            lcd_hpi_write_word(COMM_R3, lcp->arg[4], cy_private_data);
            lcd_hpi_write_word(COMM_R4, lcp->arg[5], cy_private_data);
            lcd_hpi_write_word(COMM_R5, lcp->arg[6], cy_private_data);
            lcd_hpi_write_word(COMM_R6, lcp->arg[7], cy_private_data);
            lcd_hpi_write_word(COMM_R7, lcp->arg[8], cy_private_data);
            lcd_hpi_write_word(COMM_R8, lcp->arg[9], cy_private_data);
            lcd_hpi_write_word(COMM_R9, lcp->arg[10], cy_private_data);
            lcd_hpi_write_word(COMM_R10, lcp->arg[11], cy_private_data);
            lcd_hpi_write_word(COMM_R11, lcp->arg[12], cy_private_data);
            lcd_hpi_write_word(COMM_R12, lcp->arg[13], cy_private_data);
            lcd_hpi_write_word(COMM_R13, lcp->arg[14], cy_private_data);

            /* Send CMD */
            lcd_hpi_write_mbx(COMM_EXEC_INT, cy_private_data);
            break;

        case COMM_READ_XMEM:
            /* Send CMD */
            lcd_hpi_write_mbx(COMM_READ_XMEM, cy_private_data);
            break;

        case COMM_WRITE_XMEM:
            /* Send CMD */
            lcd_hpi_write_mbx(COMM_WRITE_XMEM, cy_private_data);
            break;

        case COMM_READ_MEM:
        case COMM_WRITE_MEM:
            /* NOP for HPI */
            break;

        default:
            /* Indicate error */
            response = ERROR;

            cy_err("lcd_start_lcp: error unknown LCP command %d\n",
                   lcp->command);
            break;
    }

    return(response);
}


/*
 *  FUNCTION: lcd_create_lcp_entry
 *
 *  PARAMETERS:
 *    command         - LCP command
 *    arg1            - Argument 1
 *      .                   .
 *      .                   .
 *    arg15           - Argument 15
 *    buf             - Data buffer pointer
 *    len             - Buffer length
 *    funcptr         - Function (Callback) to call when the LCP command
 *                      completes
 *    opt_arg         - Optional argument to pass to the Callback Function
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function creates an new LCP entry structure and fills it.
 *
 *  RETURNS:
 *    Pointer to a new lcd_lcp_entry_t entry or NULL
 */
lcd_lcp_entry_t * lcd_create_lcp_entry(unsigned short command,
                                       unsigned short arg1,
                                       unsigned short arg2,
                                       unsigned short arg3,
                                       unsigned short arg4,
                                       unsigned short arg5,
                                       unsigned short arg6,
                                       unsigned short arg7,
                                       unsigned short arg8,
                                       unsigned short arg9,
                                       unsigned short arg10,
                                       unsigned short arg11,
                                       unsigned short arg12,
                                       unsigned short arg13,
                                       unsigned short arg14,
                                       unsigned short arg15,
                                       char * buf,
                                       int len,
                                       lcd_callback_t funcptr,
                                       int opt_arg,
                                       cy_priv_t * cy_private_data)
{
    lcd_lcp_entry_t * lcp;

    /* Allocate memory for lcp entry */
    lcp = (lcd_lcp_entry_t *) kmalloc(sizeof(lcd_lcp_entry_t), 0);

    if (lcp != NULL) {
        lcp->command = command;
        lcp->arg[0] = arg1;
        lcp->arg[1] = arg2;
        lcp->arg[2] = arg3;
        lcp->arg[3] = arg4;
        lcp->arg[4] = arg5;
        lcp->arg[5] = arg6;
        lcp->arg[6] = arg7;
        lcp->arg[7] = arg8;
        lcp->arg[8] = arg9;
        lcp->arg[9] = arg10;
        lcp->arg[10] = arg11;
        lcp->arg[11] = arg12;
        lcp->arg[12] = arg13;
        lcp->arg[13] = arg14;
        lcp->arg[14] = arg15;
        lcp->buf = buf;
        lcp->len = len;
        lcp->funcptr = funcptr;
        lcp->opt_arg = opt_arg;
        lcp->cy_priv_data = cy_private_data;
    }
    else
        cy_err("lcd_create_lcp_entry: kmalloc error\n");

    return lcp;
}


/*
 *  FUNCTION: lcd_add_lcp_entry
 *
 *  PARAMETERS:
 *    lcp             - LCP entry pointer
 *    cy_private_data - Private data structure pointer
 *
 *  DESCRIPTION:
 *    This function adds the LCP entry to the LCP Queue (FIFO).
 *
 *  RETURNS:
 *    SUCCESS         - Success
 *    Error           - Failure
 */
int lcd_add_lcp_entry(lcd_lcp_entry_t * lcp,
                      cy_priv_t * cy_private_data)
{
    lcd_priv_data_t * lcd_private_data =
        (lcd_priv_data_t *) cy_private_data->lcd_priv;

    int response = ERROR;
    int startLCP = FALSE;
    int lock_flags;

    if ((lcp != NULL) && (lcd_private_data != NULL)) {

        /* Do not allow interrupts and etc. */
        spin_lock_irqsave(&lcd_private_data->lcd_lock, lock_flags);

        /* If list is empty we neet to start up the LCP entry */
        if (list_empty(&lcd_private_data->lcd_lcp_list))
            startLCP = TRUE;

        /* Add LCP entry to list */
        list_add_tail(&lcp->list, &lcd_private_data->lcd_lcp_list);

        /* Start up LCP if necessary */
        if (startLCP == TRUE)
            response = lcd_start_lcp(lcp, cy_private_data);
        else
            response = SUCCESS;

        /* allow interrupts and etc. */
        spin_unlock_irqrestore(&lcd_private_data->lcd_lock, lock_flags);
    }

    return(response);
}


/*****************************************************************
 *
 * Function Name: cy67x00_int_handler
 *
 * Interrupt service routine.
 *
 * Main interrupt entry for both host and peripheral controllers.
 *
 * Input:  irq = interrupt line associated with the controller
 *         cy_priv = private data structure for controllers
 *         r = holds the snapshot of the processor's context before
 *             the processor entered interrupt code. (not used here)
 *
 * Return value  : None.
 *
 *****************************************************************/

void cy67x00_int_handler(int irq, void * cy_priv, struct pt_regs * r)
{
    extern int is_host_mode(cy_priv_t * cy_priv, int sie_num);
    volatile unsigned short intStat;
    static int count = 0;
    unsigned short msg;

    count++;

    /* cy_dbg_on = 0x1; */
    /* cy_dbg("cy67x00_int_handler: irq = 0x%x", irq); */

    /* Get value from the HPI interrupt status register */
    (volatile unsigned short) intStat = lcd_hpi_read_status_reg(cy_priv);

    while( intStat != 0 )
    {
        /* VBUS Changed */
        if (intStat & VBUS_FLG)
        {
            cy_dbg("vbus_irq \n");
            pcd_irq_vbus(cy_priv);
        }


        /* ID Changed */
        if (intStat & ID_FLG)
        {
            cy_dbg("id_irq \n");
            /* Enabled for OTG only */
            pcd_irq_id(cy_priv);

            /* Acknowledge interrupt - Done in handler */
        }


        /* Start of Frame - End of Packet Host 1 */
        if (intStat & SOFEOP1_FLG)
        {

            /* cy_dbg("sofeop1_irq\n"); */
            if (is_host_mode(cy_priv, SIE1))
            {
                hcd_irq_sofeop1(cy_priv);
            }
            else
            {
                  /* BJC - Not implemented. Empty function */
                pcd_irq_sofeop1(cy_priv);
            }
        /* otg_timer_notify(cy_priv); */
        lcd_write_reg(HOST1_STAT_REG, SOF_EOP_IRQ_FLG, cy_priv);
        }


        /* Start of Frame - End of Packet Host 2 */
        if (intStat & SOFEOP2_FLG)
        {
            cy_dbg("sofeop2_irq \n");
            if (is_host_mode(cy_priv, SIE2))
            {
                hcd_irq_sofeop2(cy_priv);
            }
            else
            {
                /* BJC - Not implemented. Empty function */
                pcd_irq_sofeop2(cy_priv);
            }
        lcd_write_reg(HOST2_STAT_REG, SOF_EOP_IRQ_FLG, cy_priv);
        }


        /* Reset Host 1 */
        if (intStat & RST1_FLG)
        {
            cy_dbg("rst1_irq \n");
            pcd_irq_rst1(cy_priv);
        }


        /* Reset Host 2 */
        if (intStat & RST2_FLG)
        {
            cy_dbg("rst2_irq \n");
            pcd_irq_rst2(cy_priv);
        }


        /* Mailbox Message Sent to the ASIC */
        if (intStat & MBX_IN_FLG)
        {
            /* cy_dbg("mb_in_irq \n"); */
            hcd_irq_mb_in(cy_priv);
            /* BJC - Not implemented. Empty function*/
            pcd_irq_mb_in(cy_priv);
        }


        /* Mailbox Message Received from the ASIC */
        if (intStat & MBX_OUT_FLG)
        {
            cy_dbg("mb_out_irq \n");
            lcp_handle_mailbox(cy_priv);
        }


        if (intStat & SIE1MSG_FLG)
        {
            unsigned short message;

            /* cy_dbg("sie1msg_irq \n"); */

            /* read the mailbox message */
            lcd_read_reg(SIE1MSG_REG, &message, cy_priv);
            lcd_write_reg(SIE1MSG_REG, 0, cy_priv);
            hcd_irq_mb_out(message, SIE1, cy_priv);


            /* Enable For initate receive data on SIE1 */
            pcd_irq_mb_out(message, SIE1, cy_priv);
        }

        if (intStat & SIE2MSG_FLG)
        {
            unsigned short message;

            cy_dbg("sie2msg_irq \n");

            /* read the mailbox message */
            lcd_read_reg(SIE2MSG_REG, &message, cy_priv);
            lcd_write_reg(SIE2MSG_REG, 0, cy_priv);

            hcd_irq_mb_out(message, SIE2, cy_priv);
            pcd_irq_mb_out(message, SIE2, cy_priv);
        }

        if (intStat & RESUME1_FLG)
        {
            cy_dbg("resume1_irq\n");
            hcd_irq_resume1(cy_priv);
        }

        if (intStat & RESUME2_FLG)
        {
            cy_dbg("resume2_irq \n");
            hcd_irq_resume2(cy_priv);
        }

        if (intStat & DONE2_FLG)
        {
            /* USB host done:  This should be disabled!!! */
            /* cy_err("cy67x00_int_handler: ERROR USB DONE2 IRQ  \
				has occurred\n"); */
        }
        if (intStat & DONE1_FLG)
        {
            /* USB host done:  This should be disabled!!! */
            /* cy_err("cy67x00_int_handler: ERROR USB DONE1 IRQ \
				has occurred\n"); */
        }

        (volatile unsigned short) intStat = lcd_hpi_read_status_reg(cy_priv);

    }  /* while( intStat != 0 ) */

    /* cy_dbg_on = 0x0; */
    count--;
}


/******************************************************************/

void lcp_handle_mailbox(cy_priv_t * cy_private_data)
{
    unsigned short shortInt;
    lcd_lcp_entry_t * lcp;
    lcd_priv_data_t * lcd_priv_data =
        (lcd_priv_data_t *) cy_private_data->lcd_priv;

    /* Received HPI Mailbox message interrupt */

    /* Read HPI Mailbox message */
    shortInt = lcd_hpi_read_mbx(cy_private_data);

    /* LCP entry */
    lcp = list_entry(lcd_priv_data->lcd_lcp_list.next, lcd_lcp_entry_t, list);

    if (lcp != NULL)
    {

        /* Default response */
        lcp->response = shortInt;
        lcp->value = lcd_hpi_read_word(COMM_R0, cy_private_data);

        switch(shortInt)
            {
            case COMM_ACK:
                 cy_dbg("lcp_handle_mailbox: COMM_ACK \n");

                switch (lcp->command)
                    {
                    case COMM_READ_CTRL_REG:
                        /* Read register value */
                        lcp->value = lcd_hpi_read_mbx(cy_private_data);
                        cy_dbg("lcp_handle_mailbox: %x\n", lcp->value);
                        break;

                    case COMM_READ_XMEM:
                        /* Read Data */
                        lcd_hpi_read_words(COMM_XMEM_BUF,
                                           (unsigned short *) lcp->buf,
                                           ((lcp->len)/2),
                                           cy_private_data);
                        break;

                    default:
                        /* Do nothing special */
                        break;
                    }

                /* Complete LCP entry and start next if required */
                lcd_complete_lcp_entry(cy_private_data);
                break;

            case COMM_NAK:

                /* Print Debug error message */
                cy_err("LCP_HPI_handler: LCP command failed 0x%X\n",
                        lcp->command);

                /* Complete LCP entry and start next if required */
                lcd_complete_lcp_entry(cy_private_data);
                break;

            default:
                /* Print Debug error message */
                cy_err("LCP_HPI_handler: Unknown HPI Mailbox message 0x%X\n",
                       shortInt);
                break;
            }
    }
    else
    {
        cy_err ("lcp_hpi_handler: no pending LCP command \n");
    }

}


