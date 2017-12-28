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

/*
 * requires (includes) CY7c7200_300_simple.[hc] simple generic HCD frontend
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>

#include <linux/smp_lock.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/usb.h>
#include "cy7c67200_300.h"
#include "cy7c67200_300_common.h"
#include "cy7c67200_300_lcd.h"
#include "lcp_cmd.h"
#include "lcp_data.h"
#include "cy7c67200_300_otg.h"
#include "cy7c67200_300_hcd.h"
#include "cy7c67200_300_hcd_simple.h"
#include "cy7c67200_300_debug.h"
#include "usbd/bi/cy7c67200_300_pcd.h"
#include "usbd/dedev/de3_bios.h"
#include "usbd/dedev/de1_bios.h"

#undef HC_URB_TIMEOUT
#undef HC_SWITCH_INT
#undef HC_ENABLE_ISOC
#undef HC_USE_SCR

int urb_debug = 0; 
extern int cy_dbg_on; 

#define HC_SWITCH_INT
#define HC_USE_SCR

#define DEFAULT_EOT                 4800
#define DFLT_HOST1_IRQ_ENBL         0xC2F1
#define DFLT_HOST2_IRQ_ENBL         0x02F1
#define DFLT_PERIPHERAL1_IRQ_ENBL   0xC200
#define DFLT_PERIPHERAL2_IRQ_ENBL   0x0200

/* configures to use SW INT on USB Reset or manually poll regs */
#undef ALT_USB_HOST_RESET

#define CY67X00_ADDR_SIZE           0x1000

#define GPIO_POS                    2

/* Default USB Reset time in (1 = 10ms) */
#define DFLT_USB_RESET_TIME         110

/* MACROS */


/* Prototypes */
extern int rh_connect_rh (cy_priv_t * cy_priv, int port_num);

/* The following are used by the peripheral driver */
/* extern unsigned int usb_address[2]; */

static int cy67x00_hw_reset (void);
static int cy67x00_get_hw_config (void);
static int __devinit hc_init_port (cy_priv_t * cy_priv, int port_num);
static int __devinit hc_init_sie (cy_priv_t * cy_priv, int sie_num);
static int __devinit hcd_init (cy_priv_t * cy_priv, int port_num);
static void init_irq(void);
static void hc_host_usb_init1 (unsigned short response,
                               unsigned short value,
                               int sie_num,
                               cy_priv_t * cy_priv);
void hc_host_usb_reset1 (unsigned long port_num);
static void hc_host_usb_reset2 (unsigned short response,
                                unsigned short value,
                                int port_num,
                                cy_priv_t * cy_priv);
__u32 get_port_stat_change(hci_t * hci, int port_num);
static unsigned short get_port_stat(hci_t *hci, int port_num);
void set_port_change(hci_t * hci, __u16 bitPos, int port_num);
void clr_port_change(hci_t * hci, __u16 bitPos, int port_num);
void clr_port_stat(hci_t * hci, __u16 bitPos, int port_num);
void set_port_stat(hci_t * hci, __u16 bitPos, int port_num);

/* BJC
 * The base_addr, data_reg_addr, and irq number are board specific.
 * The current values are design to run on the Xilinx ML40x
 * NOTE: values need to modify for different development boards
 * Refer to cy7c67200_300_hcd.h 
 */
static int base_addr     = CYPRESS_USB_BASE; 
static int data_reg_addr = CYPRESS_USB_DATA_REG;
static int irq 		 = CYPRESS_USB_IRQ;
static int trc_addr 	 = CYPRESS_USB_DBG_TRC;

static cy_priv_t * g_cy_priv;

static int reset_in_progress[] = {0, 0, 0, 0};

struct timer_list PORT0_timer;
struct timer_list PORT1_timer;
struct timer_list PORT2_timer;
struct timer_list PORT3_timer;

/* forward declarations */

MODULE_PARM(urb_debug,"i");
MODULE_PARM_DESC(urb_debug,"debug urb messages, default is 0 (no)");

MODULE_PARM(base_addr,"i");
MODULE_PARM_DESC(base_addr,"cy7c67200/300 base address");
MODULE_PARM(data_reg_addr,"i");
MODULE_PARM_DESC(data_reg_addr,"cy7c67200/300 data register address");
MODULE_PARM(irq,"i");
MODULE_PARM_DESC(irq,"Cy7c67200/300 IRQ number");



/************************************************************************
 * Function Name : HWTrace
 *
 * This function writes the "data" to the trace address for
 * debugging with a logic analyzer.
 *
 * Input:  data = data to write to trace address
 *
 * Return: N/A
 *
 ***********************************************************************/

void HWTrace(unsigned short data)
{
    outw(data, trc_addr);
}


/************************************************************************
 * Function Name : HWData
 *
 * This function writes the "data" to the trace address plus an offset
 * for debugging with a logic analyzer.
 *
 * Input:  data = data to write to the modified trace address
 *
 * Return: N/A
 *
 ***********************************************************************/

void HWData(unsigned short data)
{
    outw(data, trc_addr + 0xbee);
}


/************************************************************************
 * Function Name : is_host_mode
 *
 * This function returns TRUE if the system mode for the specified
 * SIE is in host mode and FALSE otherwise.
 *
 * Input:  cy_priv = private data
 *         sie_num = SIE Number
 *
 * Return: TRUE if currently in host mode
 *         FALSE otherwise
 ***********************************************************************/

int is_host_mode(cy_priv_t * cy_priv, int sie_num)
{
    if (cy_priv->system_mode[sie_num] == HOST_ROLE)
        return (TRUE);
    else
        return (FALSE);
}


/************************************************************************
 * Function Name : hcd_host_usb_init
 *
 * This function initialzes the sie of the CY7C67200/300 controller
 * to host mode. This can be done by calling the BIOS SW INT
 *
 * Input:  cy_priv = private data
 *         sie_num = SIE number
 *
 * Return: N/A
 *
 ***********************************************************************/

static void hc_host_usb_init (cy_priv_t * cy_priv, int sie_num)
{
    lcd_int_data_t int_data;
    int i;

    cy_dbg("enter hcd_host_usb_init - sie_num = 0x%x \n", sie_num);

    /* setup the lcd interrupt data structure */
    for (i=0; i<=14; i++) {
        int_data.reg[i] = 0;
    }

    /* Determine which SIE */
    if (sie_num == SIE1)
    {
        int_data.int_num = HUSB_SIE1_INIT_INT;
    }
    else
    {
        int_data.int_num = HUSB_SIE2_INIT_INT;
    }

    /* Issue LCP command */
    lcd_exec_interrupt (&int_data, hc_host_usb_init1, sie_num, cy_priv);
}


/************************************************************************
 * Function Name : hcd_host_usb_init1
 *
 * This is a callback function for when the HUSB_INIT_INT completes.
 *
 * Input:  response = LCP response
 *         value = return LCP value (if any)
 *         sie_num = SIE number
 *         cy_priv = private data
 *
 * Return: N/A
 *
 ***********************************************************************/

static void hc_host_usb_init1 (unsigned short response,
                               unsigned short value,
                               int sie_num,
                               cy_priv_t * cy_priv)
{
    unsigned short intStat;

    cy_dbg("enter hcd_host_usb_init1 - sie_num = 0x%x \n", sie_num);

    if (sie_num == SIE1) {
        lcd_read_reg (HOST1_IRQ_EN_REG, &intStat, cy_priv);
        intStat |= B_WAKE_IRQ_EN | A_WAKE_IRQ_EN | B_CHG_IRQ_EN | A_CHG_IRQ_EN |
        VBUS_IRQ_EN |
                   ID_IRQ_EN | SOF_EOP_IRQ_EN | DONE_IRQ_EN;
        lcd_write_reg(HOST1_IRQ_EN_REG, intStat, cy_priv);
    } else {
        lcd_read_reg (HOST2_IRQ_EN_REG, &intStat, cy_priv);
        intStat |= B_WAKE_IRQ_EN | A_WAKE_IRQ_EN | B_CHG_IRQ_EN | A_CHG_IRQ_EN |
        SOF_EOP_IRQ_EN |
                   DONE_IRQ_EN;
        lcd_write_reg(HOST2_IRQ_EN_REG, intStat, cy_priv);
    }
}

/************************************************************************
 * Function Name : hcd_host_start_timer
 *
 * This function starts the appropriate PORT timer.
 * Input:  port_num = Port number to reset
 *         time = time to wait before reset in 10ms increments
 *
 * Return: N/A
 *
 ***********************************************************************/

static void hc_host_start_timer(int port_num, int time)
{
    if( time < 0 )
    {
        return;
    }

    switch(port_num)
    {
    case PORT0:
        if( timer_pending(&PORT0_timer) )
        {
            cy_err("ERROR: PORT0 timer in use - restarting");
        }

        PORT0_timer.data = (unsigned long) port_num;
        mod_timer( &PORT0_timer, jiffies + time );
    break;
    case PORT1:
        if( timer_pending(&PORT1_timer) )
        {
            cy_err("ERROR: PORT1 timer in use - restarting");
        }

        PORT1_timer.data = (unsigned long) port_num;
        mod_timer( &PORT1_timer, jiffies + time );
    break;
    case PORT2:
        if( timer_pending(&PORT2_timer) )
        {
            cy_err("ERROR: PORT2 timer in use - restarting");
        }

        PORT2_timer.data = (unsigned long) port_num;
        mod_timer( &PORT2_timer, jiffies + time );
    break;
    case PORT3:
        if( timer_pending(&PORT3_timer) )
        {
            cy_err("ERROR: PORT3 timer in use - restarting");
        }

        PORT3_timer.data = (unsigned long) port_num;
        mod_timer( &PORT3_timer, jiffies + time );
    break;
    }
}


/************************************************************************
 * Function Name : hcd_host_usb_reset
 *
 * This function resets the usb_port of CY7C67200/300 controller and
 * detects the speed of the connecting device
 *
 * Input:  cy_priv = private data structure for the host controller
 *         port_num = port number
 *
 * Return: N/A
 *
 ***********************************************************************/

static void hc_host_usb_reset (cy_priv_t * cy_priv, int port_num)
{
    otg_t * otg = cy_priv->otg;
    int i;
    unsigned short usb_ctl_val;

    cy_dbg("enter hcd_host_usb_reset - port_num = 0x%x \n", port_num);

    if( reset_in_progress[port_num] != 0 )
    {
        cy_err("hc_host_usb_reset: reset in-progress\n");
        return;
    }
    else
    {
        reset_in_progress[port_num] = 1;
    }

    /* Check if OTG Device */
    if( otg != NULL )
    {
        /* Check if A-Device */
        if( otg->id == A_DEV )
        {
            /* Delay for over 100 ms */
            /* Note:  This code requires a resolution lower than that of
             *        the timer, which has a resolution of 10ms.  That is
             *        is the reason the code utilzes a busy-wait loop.
             */
            for (i=0; i<1050; i++) {
                mdelay(1);

                /* Watch for possible disconnect */
                lcd_read_reg(USB1_CTL_REG, &usb_ctl_val, cy_priv);

                if (!(usb_ctl_val & (A_DP_STAT | A_DM_STAT))) {
                    /* Device disconnected, cancel reset */
                    reset_in_progress[port_num] = 0;
                    otg->b_conn = FALSE;
                    return;
                }
            }
        }

        /* Reset the board */
        hc_host_usb_reset1((unsigned long) port_num);
    }
    else
    {
        /* Disable Insert-Remove Interrupt for the correct port */

        switch(port_num)
        {
        case PORT0:
            lcd_read_reg (HOST1_IRQ_EN_REG, &usb_ctl_val, cy_priv);
            usb_ctl_val &= ~A_CHG_IRQ_EN;
            lcd_write_reg(HOST1_IRQ_EN_REG, usb_ctl_val, cy_priv);
        break;

        case PORT1:
            lcd_read_reg (HOST1_IRQ_EN_REG, &usb_ctl_val, cy_priv);
            usb_ctl_val &= ~B_CHG_IRQ_EN;
            lcd_write_reg(HOST1_IRQ_EN_REG, usb_ctl_val, cy_priv);
        break;

        case PORT2:
            lcd_read_reg (HOST2_IRQ_EN_REG, &usb_ctl_val, cy_priv);
            usb_ctl_val &= ~A_CHG_IRQ_EN;
            lcd_write_reg(HOST2_IRQ_EN_REG, usb_ctl_val, cy_priv);
        break;

        case PORT3:
            lcd_read_reg (HOST2_IRQ_EN_REG, &usb_ctl_val, cy_priv);
            usb_ctl_val &= ~B_CHG_IRQ_EN;
            lcd_write_reg(HOST2_IRQ_EN_REG, usb_ctl_val, cy_priv);
        break;
        }

#ifdef HC_USE_SCR
        /* Change the Speed Control Register */
        lcd_lcp_write_reg(CPU_SPEED_REG, 1, 0, cy_priv);
#endif

        /* Start timer - call hc_host_usb_reset1 when timeout */
        hc_host_start_timer(port_num, DFLT_USB_RESET_TIME);
    }
    cy_dbg("exiting hcd_host_usb_reset - port_num = 0x%x \n", port_num);
}


/************************************************************************
 * Function Name : hcd_host_usb_reset1
 *
 * This is a callback function for when the 100 ms timer expires.
 *
 * Input:  port_num = port number
 *
 * Return: N/A
 *
 ***********************************************************************/

void hc_host_usb_reset1 (unsigned long port_num)
{
    cy_priv_t * cy_priv = g_cy_priv;
    hci_t *hci = (hci_t *) cy_priv->hci;
    hcipriv_t *hp = &hci->port[port_num];
    lcd_int_data_t int_data;
    int i;
    unsigned short usb_ctl_val;

    cy_dbg("enter hcd_host_usb_reset1 - port_num = 0x%lx\n", port_num);

    if (cy_priv == NULL)
    {
        cy_err("hc_host_usb_reset: cy_priv = NULL\n");
        return;
    }

    if (hci == NULL)
    {
        cy_err("hc_host_usb_reset: hci = NULL\n");
        return;
    }

    if (hp == NULL)
    {
        cy_err("hc_host_usb_reset: hp = NULL\n");
        return;
    }

    /* Enable Insert-Remove Interrupt for the correct port */

    switch(port_num)
    {
    case PORT0:
        /* Clear any pending insert-remove interrupt */
        lcd_write_reg(HOST1_STAT_REG, A_CHG_IRQ_FLG, cy_priv);

        /* Enable insert-remove interrupt */
        lcd_read_reg (HOST1_IRQ_EN_REG, &usb_ctl_val, cy_priv);
        usb_ctl_val |= A_CHG_IRQ_EN;
        lcd_write_reg(HOST1_IRQ_EN_REG, usb_ctl_val, cy_priv);
    break;

    case PORT1:
        /* Clear any pending insert-remove interrupt */
        lcd_write_reg(HOST1_STAT_REG, B_CHG_IRQ_FLG, cy_priv);

        /* Enable insert-remove interrupt */
        lcd_read_reg (HOST1_IRQ_EN_REG, &usb_ctl_val, cy_priv);
        usb_ctl_val |= B_CHG_IRQ_EN;
        lcd_write_reg(HOST1_IRQ_EN_REG, usb_ctl_val, cy_priv);
    break;

    case PORT2:
        /* Clear any pending insert-remove interrupt */
        lcd_write_reg(HOST2_STAT_REG, A_CHG_IRQ_FLG, cy_priv);

        /* Enable insert-remove interrupt */
        lcd_read_reg (HOST2_IRQ_EN_REG, &usb_ctl_val, cy_priv);
        usb_ctl_val |= A_CHG_IRQ_EN;
        lcd_write_reg(HOST2_IRQ_EN_REG, usb_ctl_val, cy_priv);
    break;

    case PORT3:
        /* Clear any pending insert-remove interrupt */
        lcd_write_reg(HOST2_STAT_REG, B_CHG_IRQ_FLG, cy_priv);

        /* Enable insert-remove interrupt */
        lcd_read_reg (HOST2_IRQ_EN_REG, &usb_ctl_val, cy_priv);
        usb_ctl_val |= B_CHG_IRQ_EN;
        lcd_write_reg(HOST2_IRQ_EN_REG, usb_ctl_val, cy_priv);
    break;
    }

#ifdef HC_USE_SCR
    /* Change the Speed Control Register */
    lcd_lcp_write_reg(CPU_SPEED_REG, 0, 0, cy_priv);
#endif

    /* Enable the port to become a host */
    hp->host_disabled = 0;

    /* Set the SIE as a host */
    int_data.int_num = HUSB_RESET_INT;
    int_data.reg[0] = 60;        /* Reset USB port for 50 ms */
    int_data.reg[1] = port_num;  /* set the port number */
    for (i=2; i<=14; i++) {
        int_data.reg[i] = 0;
    }

    lcd_exec_interrupt (&int_data, hc_host_usb_reset2, port_num, cy_priv);
    cy_dbg("Exiting hc_host_usb_reset1\n");
}


/************************************************************************
 * Function Name : hcd_host_usb_reset2
 *
 * This is a callback function for when the HUSB_RESET_INT completes.
 *
 * Input:  response = LCP response
 *         value = return LCP value (if any)
 *         sie_num = SIE number
 *         cy_priv = private data
 *
 * Return: N/A
 *
 ***********************************************************************/

static void hc_host_usb_reset2 (unsigned short response,
                                unsigned short value,
                                int port_num,
                                cy_priv_t * cy_priv)
{
    hci_t *hci = (hci_t *) cy_priv->hci;
    hcipriv_t *hp = &hci->port[port_num];
    sie_t * sie = &hci->sie[port_num/2];
    volatile unsigned short intStat = 0;
    unsigned short usb_ctl_val;
    otg_t * otg = cy_priv->otg;
    unsigned short ctl_reg = 0;
    unsigned short no_device = 0;
    unsigned short fs_device = 0;

    cy_dbg("enter hcd_host_usb_reset2 - port_num = 0x%x\n", port_num);
    if (cy_priv == NULL)
    {
        cy_err("hc_host_usb_reset2: cy_priv = NULL\n");
        return;
    }

    if (hci == NULL)
    {
        cy_err("hc_host_usb_reset2: hci = NULL\n");
        return;
    }

    if (hp == NULL)
    {
        cy_err("hc_host_usb_reset2: hp = NULL\n");
        return;
    }

    switch(port_num)
    {
    case PORT0:
        ctl_reg = USB1_CTL_REG;
        no_device = (A_DP_STAT | A_DM_STAT);
        fs_device = A_DP_STAT;
    break;
    case PORT1:
        ctl_reg = USB1_CTL_REG;
        no_device = (B_DP_STAT | B_DM_STAT);
        fs_device = B_DP_STAT;
    break;
    case PORT2:
        ctl_reg = USB2_CTL_REG;
        no_device = (A_DP_STAT | A_DM_STAT);
        fs_device = A_DP_STAT;
    break;
    case PORT3:
        ctl_reg = USB2_CTL_REG;
        no_device = (B_DP_STAT | B_DM_STAT);
        fs_device = B_DP_STAT;
    break;
    }

    /* Check if a device has been connected */
    lcd_read_reg(ctl_reg, &usb_ctl_val, cy_priv);

    if (!(usb_ctl_val & no_device))
    {
        cy_dbg("hcd_host_usb_reset2: no device \n");

        /* no device connected */
        hp->RHportStatus->portStatus &= ~(PORT_CONNECT_STAT | PORT_ENABLE_STAT);
        if (otg != NULL)
        {
            otg->b_conn = FALSE;
            otg->a_conn = FALSE;
        }
    }
    else
    {
        /* check for low speed or full speed by reading D+ and D- lines */
        if (usb_ctl_val & fs_device)
        {
            cy_dbg("hcd_host_usb_reset2: full speed device \n");
            /* full speed */

            /* configure to be a host */
            if (otg != NULL)
            {
                if(otg->id == A_DEV)
                {
                    otg->b_conn = TRUE;
                }
                else
                {
                    otg->a_conn = TRUE;
                }
            }

            hp->RHportStatus->portStatus |= (PORT_CONNECT_STAT);
            hp->RHportStatus->portStatus &= ~PORT_LOW_SPEED_DEV_ATTACH_STAT;
        }
        else
        {
            cy_dbg("hcd_host_usb_reset2: low speed device \n");
            /* configure to be a host */
            if (otg != NULL)
            {
                if(otg->id == A_DEV)
                {
                    otg->b_conn = TRUE;
                }
                else
                {
                    otg->a_conn = TRUE;
                }
            }
            hp->RHportStatus->portStatus |= (PORT_CONNECT_STAT
                | PORT_LOW_SPEED_DEV_ATTACH_STAT);
        }
    }

    update_otg_state(otg);

    /* Enable the HPI SOF interrupt */
    lcd_read_reg(HPI_IRQ_ROUTING_REG, (unsigned short *) &intStat, cy_priv);

    if ((port_num == PORT0) || (port_num == PORT1))
    {
        intStat |= SOFEOP1_TO_HPI_EN;
    }
    else
    {
        intStat |= SOFEOP2_TO_HPI_EN | SOFEOP2_TO_CPU_EN |
                   RESUME2_TO_HPI_EN;
    }

    lcd_write_reg(HPI_IRQ_ROUTING_REG, intStat, cy_priv);

    /* Set HUSB_pEOT time */
    lcd_write_reg(HUSB_pEOT, DEFAULT_EOT, cy_priv);

    sie->td_active = FALSE;

    /* Indicate change in connection status */
    set_port_change(hci, PORT_CONNECT_CHANGE, port_num);

    reset_in_progress[port_num] = 0;
    cy_dbg("Exiting hc_host_usb_reset2\n");
}


/************************************************************************
 * Function Name : hc_flush_data_cache
 *
 * This function flushes the data cache.  For this architecture it does
 * do anything.
 *
 * Input:  hci = data structure for the host controller
 *         data = data pointer
 *         len = length
 *
 * Return: N/A
 ***********************************************************************/

void hc_flush_data_cache (hci_t * hci, void * data, int len)
{
}


/************************************************************************
 * Function Name : hc_disable_port
 *
 * This function disables the specified port
 *
 * Input:  cy_priv = private data structure
 *         port_num = Port number
 *
 * Return: N/A
 ***********************************************************************/

void hc_disable_port (cy_priv_t * cy_priv, int port_num)
{
    hci_t * hci = cy_priv->hci;
    hcipriv_t * hp = &hci->port[port_num];

    hp->RHportStatus->portStatus &= ~(PORT_CONNECT_STAT | PORT_ENABLE_STAT);
    hp->RHportStatus->portChange |= PORT_CONNECT_CHANGE | PORT_ENABLE_CHANGE;
}


/************************************************************************
 * Function Name : hc_enable_port
 *
 * This function enables the specified port
 *
 * Input:  cy_priv = private data structure
 *         port_num = Port number
 *
 * Return: N/A
 ***********************************************************************/

void hc_enable_port (cy_priv_t * cy_priv, int port_num)
{
    if (cy_priv->otg == NULL)
    {
        hc_host_usb_reset(cy_priv, port_num);
    }
}


/************************************************************************
 * Function Name : hcd_switch_rol
 *
 * This function is used to switch betwen host and peripheral modes.
 *
 * Input:  new_role = The role to change to
 *         cy_priv = private data structure
 *
 * Return: N/A
 ***********************************************************************/

void hcd_switch_role(int new_role, cy_priv_t * cy_priv)
{
    switch (new_role)
    {
    case PERIPHERAL_ROLE:
        /* Check for valid hci */
        if (cy_priv->hci != NULL)
        {
            /* Disable host port */
            hc_disable_port(cy_priv, PORT0);
        }

        break;

    case HOST_ROLE:
        /* Check for valid hci */
        if (cy_priv->hci != NULL)
        {
            /* Initialize the SIE */
            hc_host_usb_init (cy_priv, SIE1);

            /* Enable host port(s) */
            hc_enable_port(cy_priv, PORT0);
        }

        break;

    case HOST_INACTIVE_ROLE:
        /* Check for valid hci */
        if (cy_priv->hci != NULL)
        {
            /* Disable host port(s) */
            hc_disable_port(cy_priv, PORT0);

            /* Initialize the SIE */
            hc_host_usb_init (cy_priv, SIE1);
        }
        break;
    }
}


/************************************************************************
 * Function Name : handle_device_insertion
 *
 * This function handles the insertion of a device on CY7C67200/300 ASIC
 * It causes a USB reset on the bus
 *
 * Input:  cy_priv = private data structure
 *         port_num = Port number
 *
 * Return: N/A
 ***********************************************************************/

void handle_device_insertion (cy_priv_t * cy_priv, int port_num)
{
    cy_dbg("handle_device_insertion %x\n", port_num);
    hc_host_usb_reset(cy_priv, port_num);
}


/************************************************************************
 * Function Name : handle_device_removal
 *
 * This function handles the removal of a device on CY7C67200/300 ASIC
 * It causes a USB reset on the bus
 *
 * Input:  cy_priv = private data structure
 *         port_num = Port number
 *
 * Return: N/A
 ***********************************************************************/

void handle_device_removal (cy_priv_t * cy_priv, int port_num)
{
    cy_dbg("handle_device_removal %x\n", port_num);
    hc_host_usb_reset (cy_priv, port_num);
}


/************************************************************************
 * Function Name : handle_remote_wakeup
 *
 * This function handles remote wakeup.  (Not implemented at this time)
 *
 * Input:  cy_priv = private data structure
 *         port_num = Port number
 *
 * Return: N/A
 ***********************************************************************/

void handle_remote_wakeup (cy_priv_t * cy_priv, int port_num)
{
    /* Not handled at this time */
}


/*****************************************************************
 * Function Name: fillTd
 *
 * This functions fills the TD structure
 *
 * Input:  Assorted
 *
 * Return: 0 = error; 1 = successful
 *****************************************************************/

int fillTd(hci_t * hci, td_t *td, __u16 * td_addr, __u16 * buf_addr,
           __u8 devaddr, __u8 epaddr, int pid, int len, int toggle, int slow,
           int tt, int port_num)
{

    __u8 cmd = 0;
    __u16 speed;
    __u8 activeFlag = 1;
    __u8 retryCnt = 1; /* Srikanth changed it 1 on 24th Januray from 3 */

    speed = get_port_stat(hci, port_num);

    /* If the directly connected device is a full-speed hub
     * the low-speed preamble must be turned on to communicate
     * to a low-speed device connected to the hub
     */
    if (!(speed & PORT_LOW_SPEED_DEV_ATTACH_STAT) && slow)
    {
        cmd |= PREAMBLE_EN;
        cy_dbg("FillTD\n");
    }

    /* Check for isochronous pipe */

    if (tt == TT_ISOCHRONOUS)
    {
        cmd |= ISO_EN;
    }

    if (toggle)
    {
        cmd |= SEQ_SEL;
    }

    cmd |=  ARM_EN;

    /* make sure the buffer address is word align */
    *buf_addr = (*buf_addr + 1) & 0xFFFE;
    *td_addr = (*td_addr + 1) & 0xFFFE;



    td->ly_base_addr = *buf_addr;
    td->port_length = (port_num << 14) + (len & 0x3FF);  /* Port Length + port  Number */
    td->pid_ep = ((pid & 0xF) << 4) | (epaddr & 0xF);
    td->dev_addr = devaddr & 0x7F;
    td->ctrl_reg = cmd;
    td->status = 0;
    td->retry_cnt = (tt << 2) | (activeFlag << 4) | retryCnt;
    td->residue = 0;
    td->next_td_addr = *td_addr + CY_TD_SIZE;
    td->last_td_flag = 0;

  cy_dbg("FillTD ly_base_addr 0x%x port_length 0x%x \n", td->ly_base_addr,td->port_length);
  /* cy_dbg("pid_ep 0x%x, dev_addr 0x%x \n", td->pid_ep,td->dev_addr); */
  /* cy_dbg("ctrl_reg 0x%x \n", td->ctrl_reg); */

    return 1;
}


/************************************************************************
 * Function Name : is_host_enable
 *
 * This function returns a value if the host is enabled.
 *
 * Input:  hci = data structure for the host controller
 *         port_num = Port number
 *
 * Return: N/A
 ***********************************************************************/

int is_host_enable(hci_t * hci, int port_num)
{
    hcipriv_t * hp = &hci->port[port_num];
    return (!hp->host_disabled);
}


/*****************************************************************
 * Function Name: get_port_stat_change
 *
 * This function gets the port change status for the specified port
 *
 * Input:  hci = data structure for the host controller
 *         port_num = Port number
 *
 * Return value  : port status and change
 *****************************************************************/

__u32 get_port_stat_change(hci_t * hci, int port_num)
{
    hcipriv_t * hp = &hci->port[port_num];
    __u32 portstatus;


/*    cy_dbg("enter getPorStatusAndChange"); */

    portstatus = hp->RHportStatus->portChange << 16 |
                 hp->RHportStatus->portStatus;

    return (portstatus);
}


/*****************************************************************
 * Function Name: get_port_stat
 *
 * This function gets the port status for the specified port
 *
 * Input:  hci = data structure for the host controller
 *         port_num = Port number
 *
 * Return value  : port status and change
 *****************************************************************/

static unsigned short get_port_stat(hci_t *hci, int port_num)
{
    hcipriv_t * hp = &hci->port[port_num];
    __u32 portstatus;

/*    cy_dbg("enter getPorStatusAndChange \n"); */

    portstatus = hp->RHportStatus->portStatus;

    return (portstatus);
}


/*****************************************************************
 * Function Name: setPortChange
 *
 * This function set the bit position of portChange.
 *
 * Input:  hci = data structure for the host controller
 *         bitPos = the bit position
 *         port_num = Port number
 *
 * Return value  : N/A
 *****************************************************************/

void set_port_change(hci_t * hci, __u16 bitPos, int port_num)
{
    hcipriv_t * hp = &hci->port[port_num];

    switch (bitPos)
    {
    case PORT_CONNECT_CHANGE:
            hp->RHportStatus->portChange |= bitPos;
            break;

    case PORT_ENABLE_CHANGE:
            hp->RHportStatus->portChange |= bitPos;
            break;

    case PORT_RESET_CHANGE:
            hp->RHportStatus->portChange |= bitPos;
            break;

    case PORT_SUSPEND_CHANGE:
            hp->RHportStatus->portChange |= bitPos;
            break;

    case PORT_OVER_CURRENT_CHANGE:
            hp->RHportStatus->portChange |= bitPos;
            break;
    }
}


/*****************************************************************
 * Function Name: clrPortChange
 *
 * This function clear the bit position of portChange.
 *
 * Input:  hci = data structure for the host controller
 *         bitPos = the bit position
 *         port_num = Port number
 *
 * Return value  : N/A
 *****************************************************************/

void clr_port_change(hci_t * hci, __u16 bitPos, int port_num)
{
    hcipriv_t * hp = &hci->port[port_num];
    switch (bitPos)
    {
    case PORT_CONNECT_CHANGE:
            hp->RHportStatus->portChange &= ~bitPos;
            break;

    case PORT_ENABLE_CHANGE:
            hp->RHportStatus->portChange &= ~bitPos;
            break;

    case PORT_RESET_CHANGE:
            hp->RHportStatus->portChange &= ~bitPos;
            break;

    case PORT_SUSPEND_CHANGE:
            hp->RHportStatus->portChange &= ~bitPos;
            break;

    case PORT_OVER_CURRENT_CHANGE:
            hp->RHportStatus->portChange &= ~bitPos;
            break;
    }
}


/*****************************************************************
 * Function Name: clrPortStatus
 *
 * This function clear the bit position of portStatus.
 *
 * Input:  hci = data structure for the host controller
 *         bitPos = the bit position
 *         port_num = Port number
 *
 * Return value  : N/A
 *****************************************************************/

void clr_port_stat(hci_t * hci, __u16 bitPos, int port_num)
{
    hcipriv_t * hp = &hci->port[port_num];
    switch (bitPos)
    {
    case PORT_ENABLE_STAT:
            hp->RHportStatus->portStatus &= ~bitPos;
            break;

    case PORT_RESET_STAT:
            hp->RHportStatus->portStatus &= ~bitPos;
            break;

    case PORT_POWER_STAT:
            hp->RHportStatus->portStatus &= ~bitPos;
            break;

    case PORT_SUSPEND_STAT:
            hp->RHportStatus->portStatus &= ~bitPos;
            break;
    }
}


/*****************************************************************
 * Function Name: setPortStatus
 *
 * This function set the bit position of portStatus.
 *
 * Input:  hci = data structure for the host controller
 *         bitPos = the bit position
 *         port_num = Port number
 *
 * Return value  : N/A
 *****************************************************************/

void set_port_stat(hci_t * hci, __u16 bitPos, int port_num)
{
    hcipriv_t * hp = &hci->port[port_num];
    switch (bitPos)
    {
    case PORT_ENABLE_STAT:
            hp->RHportStatus->portStatus |= bitPos;
            break;

    case PORT_RESET_STAT:
            hp->RHportStatus->portStatus |= bitPos;
            break;

    case PORT_POWER_STAT:
            hp->RHportStatus->portStatus |= bitPos;
            break;

    case PORT_SUSPEND_STAT:
            hp->RHportStatus->portStatus |= bitPos;
            break;
    }
}


/*****************************************************************
 * Function Name: hcd_start
 *
 * This function starts the root hub functionality.
 *
 * Input:  cy_priv = private data structure
 *         port_num = Port number
 *
 * Return value  : 0
 *****************************************************************/

static int hcd_start (cy_priv_t * cy_priv, int port_num)
{
    cy_dbg("Enter hc_start \n");

    rh_connect_rh (cy_priv, port_num);

    return 0;
}


/*****************************************************************
 * Function Name: hc_alloc_hci
 *
 * This function allocates all data structures and stores them in
 * the private data structure.
 *
 * Input:  cy_priv = private data structure
 *         port_num = Port number
 *
 * Return value  : NULL if error
 *                 Pointer to hci
 *****************************************************************/
static hci_t * __devinit hc_alloc_hci (cy_priv_t * cy_priv, int port_num)
{
    hci_t * hci;
    int i = 0;

    cy_dbg("Enter hc_alloc_hci \n");

    /* alloc the hci structure */
    hci = (hci_t *) kmalloc (sizeof (hci_t), GFP_KERNEL);
    if (!hci)
    {
        cy_err("can't alloc hci");
        return NULL;
    }
    memset (hci, 0, sizeof (hci_t));

    INIT_LIST_HEAD (&hci->hci_hcd_list);
    list_add (&hci->hci_hcd_list, &hci_hcd_list);
    init_waitqueue_head (&hci->waitq);

    /* set all host port to be invalid at first */
    for (i = 0; i< MAX_NUM_PORT; i++)
    {
        hci->valid_host_port[i] = -1;
        hci->port[i].host_disabled = 1;
    }

    return hci;
}


/*****************************************************************
 * Function Name: hc_init_port
 *
 * This function initializes the port specified by port_num.
 *
 * Input:  cy_priv = data structure for the controller
 *         port_num = Port number
 *
 * Return value  : SUCCESS or ERROR
 *****************************************************************/

static int __devinit hc_init_port (cy_priv_t * cy_priv, int port_num)
{
    hci_t * hci = cy_priv->hci;
    hcipriv_t * hp = &hci->port[port_num];
    portstat_t * ps;
    struct usb_bus * bus;

    cy_dbg("port_num = 0x%x \n", port_num);

    /* set the initial port structure to zero */
    memset (hp, 0, sizeof(hcipriv_t));

    /* setup root hub port status */
    ps = (portstat_t *) kmalloc (sizeof (portstat_t), GFP_KERNEL);

    if (!ps)
    {
        return ERROR;
    }

    ps->portStatus = PORT_STAT_DEFAULT;
    ps->portChange = PORT_CHANGE_DEFAULT;
    hp->RHportStatus = ps;
    hp->frame_number = 0;
    hp->host_disabled = 1;
    INIT_LIST_HEAD (&hp->ctrl_list);
    INIT_LIST_HEAD (&hp->bulk_list);
    INIT_LIST_HEAD (&hp->iso_list);
    INIT_LIST_HEAD (&hp->intr_list);
    INIT_LIST_HEAD (&hp->del_list);
    INIT_LIST_HEAD (&hp->frame_urb_list);

    bus = usb_alloc_bus (&hci_device_operations);
    if (!bus)
    {
        cy_err("unable to alloc bus");
        kfree (hci);
        return ERROR;
    }

    hp->bus = bus;
    bus->bus_name = "CYEZ-Host"; /* Srikanth 3rd Feb */
    bus->hcpriv = (void *) cy_priv;
    hci->valid_host_port[port_num] = 1;
    return SUCCESS;
}


/*****************************************************************
 * Function Name: hc_init_sie
 *
 * This function initializes the sie specified by sie_num.
 *
 * Input:  cy_priv = data structure for the controller
 *         sie_num = SIE number
 *
 * Return value  : SUCCESS or ERROR
 *****************************************************************/

static int __devinit hc_init_sie (cy_priv_t * cy_priv, int sie_num)
{
    hci_t * hci = cy_priv->hci;
    sie_t * sie = &hci->sie[sie_num];

    cy_dbg("Enter hc_init_sie, sie_num = 0x%x \n", sie_num);

    /* set the initial port structure to zero */
    memset (sie, 0, sizeof(sie_t));

    /* set TD base address and TD buffer address for this sie:
       TD list size = 256
       TD buffer size = 1k
       see cy_7c67200_300_comm.h for detail on addr
     */

    if (sie_num == SIE1)
    {
        sie->tdBaseAddr = cy_priv->cy_buf_addr + SIE1_TD_OFFSET;
    }
    else if (sie_num == SIE2)
    {
        sie->tdBaseAddr = cy_priv->cy_buf_addr + SIE2_TD_OFFSET;
    }
    else
    {
        cy_err("unknown sie_num %d", sie_num);
        return ERROR;
    }
    /* Initialize the buffer */
    sie->td_active = FALSE;
    sie->bufBaseAddr = sie->tdBaseAddr + SIE_TD_SIZE;

    /* initalize the list heads */
    INIT_LIST_HEAD (&sie->done_list_head);
    INIT_LIST_HEAD (&sie->td_list_head);
    return SUCCESS;
}


/*****************************************************************
 * Function Name: cy67x00_release_priv
 *
 * This function De-allocate all resources
 *
 * Input:  cy_priv = private data structure
 *
 * Return value  : N/A
 *****************************************************************/

static void cy67x00_release_priv (cy_priv_t * cy_priv)
{
    int port_num;
    hci_t * hci = cy_priv->hci;
    hcipriv_t * hp;

    cy_dbg("Enter cy67x00_release_priv \n");
    for (port_num = 0; port_num < MAX_NUM_PORT; port_num++)
    {
        if (hci->valid_host_port[port_num] == 1) {
            hp = &hci->port[port_num];

            /* disconnect all devices */
            if (hp->bus->root_hub)
            {
                usb_disconnect (&hp->bus->root_hub);
            }
            usb_deregister_bus (hp->bus);
            usb_free_bus (hp->bus);
        }
    }

    if (cy_priv->cy_addr > 0)
    {
        release_region (cy_priv->cy_addr, CY67X00_ADDR_SIZE);
        cy_priv->cy_addr = 0;
    }

    if (cy_priv->cy_irq > 0)
    {
        free_irq (cy_priv->cy_irq, cy_priv);
        cy_priv->cy_irq = -1;
    }

    list_del (&hci->hci_hcd_list);
    INIT_LIST_HEAD (&hci->hci_hcd_list);
    kfree (hci);
    kfree (cy_priv);
}


/*****************************************************************
 * Function Name: hcd_init
 *
 * This function initializes the hci structure
 *
 * Input:  cy_priv = private data structure
 *         port_num = Port number
 *
 * Return value  : N/A
 *****************************************************************/

int __devinit hcd_init (cy_priv_t * cy_priv, int port_num)
{
    hci_t * hci = NULL;
    int sie_num = port_num / 2;

    cy_dbg("In hcd_init port num %x \n", port_num);

    if (cy_priv == NULL)
    {
        cy_err("cy_priv == NULL");
    }

    /* Initalize data structure */
    if (cy_priv->hci == NULL)
    {
        cy_priv->hci = hc_alloc_hci (cy_priv, port_num);
        if (cy_priv->hci == NULL)
        {
            cy_err("Error allocating hci");
            return -ENOMEM;
        }
    }

    hci = cy_priv->hci;

    if ((port_num == PORT0) || (port_num == PORT2))
    {
        /* initialize  the sie structure */
        if (hc_init_sie(cy_priv, sie_num) == ERROR)
        {
            cy_err("can't init sie %d", sie_num);
        }
    }

    /* Initialize the port structure*/
    if (hc_init_port(cy_priv, port_num) == ERROR)
    {
        cy_err("can't init port %d", port_num);
    }

    /* register the bus */
    usb_register_bus (hci->port[port_num].bus);

    hcd_start(cy_priv, port_num);

    return SUCCESS;
}



/*****************************************************************
 *
 *
 *  THE FOLLOWING IS CY7C67200/300 HARDWARE SPECIFIC STUFF
 *  IT IS COMMON TO HCD, PCD and LCD
 *
 *
******************************************************************/


/*****************************************************************
 * Function Name: cy67x00_reg_init
 *
 * This function reads the mailbox register and clears the
 * HPI interrupt enable register
 *
 * Input: cy_priv = private data
 *
 * Return: N/A
 *****************************************************************/

void __devinit cy67x00_reg_init(cy_priv_t * cy_priv)
{
    lcd_hpi_read_mbx(cy_priv);
    lcd_write_reg(HPI_IRQ_ROUTING_REG, 0, cy_priv);
}


/*****************************************************************
 * Function Name: cy67x00_init
 *
 * This function request IO memory regions, request IRQ, and
 * allocate all other resources.
 *
 * Input: addr = first IO address
 *        addr2 = second IO address
 *        irq = interrupt number
 *
 * Return: SUCCESS or ERROR
 *****************************************************************/

static int __devinit cy67x00_init (int addr, int addr2, int irq)
{
    int hw_config;     /* for dip switch setting */
    long tempAddr1;

    cy_dbg ("Enter cy67x00_init \n");

    /******************************************************************
     * It reads the DIP switches from the controller card and determine
     * the demo mode which can be:
     *
     * DE1 OTG demo on SIE1 -- OTG, HPI
     * DE2 slave demo  on SIE1 -- loopback using all comm ports
     * DE3 host on SIE1 and peripheral on SIE2 -- HPI
     * DE4 slave demo on SIE1  -- act as display device
     *
     * The DIP switches also indicates the communication mode: HPI, HSS
     * or SPI
     *******************************************************************/


#if defined(microblaze) || defined(__microblaze__)
    hw_config = cy67x00_get_hw_config();
#else
    hw_config = cy67x00_get_hw_config() & SAB_DIP_MASK;
#endif
    switch(hw_config)
    {
        case DE2_HPI:
            cy_err("Configuring for DE2_HPI");
            return cy67x00_hw_reset();

        case DE2_HSS:
            cy_err("Configuring for DE2_HSS");
            return cy67x00_hw_reset();

        case DE2_SPI:
            cy_err("Configuring for DE2_SPI");
            return cy67x00_hw_reset();

        case DE1_HPI:
        case DE3_HPI:
        case DE4_HPI:
        default:
            break;
    }  

    /* allocate the private data */
    g_cy_priv = (cy_priv_t *) kmalloc(sizeof(cy_priv_t), 0);

    if (!g_cy_priv)
    {
        return -ENOMEM;
    }

    g_cy_priv->otg = NULL;
    g_cy_priv->hci = NULL;
    g_cy_priv->lcd_priv = NULL;
    g_cy_priv->pcdi = NULL;
    g_cy_priv->system_mode[SIE1] = HOST_ROLE;
    g_cy_priv->system_mode[SIE2] = HOST_ROLE;
    g_cy_priv->cy_buf_addr = 0x500; /* Start of arena */


    cy_dbg ("Before request region \n");

   /* The follwoing lines have been added for ioremapping srikanth*/
    tempAddr1 = (u32) ioremap(addr, CY67X00_ADDR_SIZE);
    cy_dbg("cy_addr 0x%08lx \n", tempAddr1);

    /* allocated I/O address for CY67x00 */
    if (!request_region (tempAddr1, CY67X00_ADDR_SIZE,
                         "CY7C67200/300 Embedded USB controller"))
    {
        cy_err("request address %d failed", addr);
        kfree(g_cy_priv);
        g_cy_priv = 0;
        return -EBUSY;
    }
    g_cy_priv->cy_addr = tempAddr1;

#if 0
    /* allocated I/O address for CY67x00 */
    if (!request_region (addr, CY67X00_ADDR_SIZE,
                         "CY7C67200/300 Embedded USB controller"))
    {
        cy_err("request address %d failed", addr);
        kfree(g_cy_priv);
        g_cy_priv = 0;
        return -EBUSY;
    }

    g_cy_priv->cy_addr = addr;
#endif

    init_irq();

    /*  BJC - Software Reset of Cypress Chip 
     *  Prevent race condition between RH enumeration and
     *  interrupts triggering due to Ez-USB chip not being POR
     *  while interrupt is enable via request_irq.
     *  This has to be carried out before request_irq is enabled.
     */
    if (cy67x00_hw_reset() != SUCCESS)
    {
        cy_err("no CY7C67200_300 card present in addr 0x%x", addr);
        release_region (g_cy_priv->cy_addr, CY67X00_ADDR_SIZE);
        kfree (g_cy_priv);
        g_cy_priv = 0;
        return -EBUSY;
    }

    /* allocated interrupt for CY67x00, the standard cy67x00_int_handler is for
       the HPI protocol only.  Handler resides in lcd layer. */
    if (request_irq (irq, cy67x00_int_handler, 0,
                     "CY7C67200/300 Embedded USB controller HPI",
                     g_cy_priv) != 0)

    {
        cy_err("request interrupt %d failed", irq);
        release_region (g_cy_priv->cy_addr, CY67X00_ADDR_SIZE);
        kfree(g_cy_priv);
        g_cy_priv = 0;
        return -EBUSY;
    }

    g_cy_priv->cy_irq = irq;

    /* Gets re-enabled when lcd_init is done */
    disable_irq(g_cy_priv->cy_irq);

    /* initialze the registers */
    cy67x00_reg_init(g_cy_priv);

    /* Initialize Port timers */
    init_timer(&PORT0_timer);
    PORT0_timer.function = hc_host_usb_reset1;
    init_timer(&PORT1_timer);
    PORT1_timer.function = hc_host_usb_reset1;
    init_timer(&PORT2_timer);
    PORT2_timer.function = hc_host_usb_reset1;
    init_timer(&PORT3_timer);
    PORT3_timer.function = hc_host_usb_reset1;

    /* Setup drivers based upon design example */
    switch (hw_config)
    {
        case DE1_HPI:
            {
                extern void switch_role(int new_role);
                otg_t * otg;

                cy_err("Configuring for DE1");

                /* Initialize the Low Level Controller (lcd) structures */
                if (lcd_init(&de1_bios[0], sizeof(de1_bios), 0, 0, g_cy_priv)
                   == ERROR)
                {
                    release_region (g_cy_priv->cy_addr, CY67X00_ADDR_SIZE);
                    free_irq (g_cy_priv->cy_irq, g_cy_priv);
                    kfree (g_cy_priv);
                    g_cy_priv = 0;
                    return ERROR;
                }

                /* Initialize OTG */
                otg_init(g_cy_priv);
                otg = g_cy_priv->otg;

                /* Init pcd first so remainder EZ-HOST memory is left for hcd */
                pcd_init(SIE1, DE1, g_cy_priv);
                hcd_init(g_cy_priv, PORT0);

                switch_role(HOST_INACTIVE_ROLE);

                if(otg->id == A_DEV)
                {
                    otg->a_bus_req = TRUE;
                    update_otg_state(otg);
                }
            }
            break;

        case DE3_HPI:
            cy_err("Configuring for DE3");

            g_cy_priv->otg = NULL;

            /* Initialize the Low Level Controller (lcd) structures */
            if (lcd_init(&de3_bios[0], sizeof(de3_bios), 0, 0, g_cy_priv)
                == ERROR)
            {
                release_region (g_cy_priv->cy_addr, CY67X00_ADDR_SIZE);
                free_irq (g_cy_priv->cy_irq, g_cy_priv);
                kfree (g_cy_priv);
                g_cy_priv = 0;
                return ERROR;
            }

            /* Init pcd first so remainder EZ-HOST memory is left for hcd */
            /* Set HW SIE2 as a peripherial */
            pcd_init(SIE2, DE3, g_cy_priv);
            pcd_switch_role(PERIPHERAL_ROLE, g_cy_priv);
            g_cy_priv->system_mode[SIE2] = PERIPHERAL_ROLE;

            /* Initialize the ports */
            hcd_init(g_cy_priv, PORT0);
            hcd_init(g_cy_priv, PORT1);

            /* Initialize the SIE */
            hc_host_usb_init (g_cy_priv, SIE1);

            /* Enable host ports */
            hc_enable_port(g_cy_priv, PORT0);
            hc_enable_port(g_cy_priv, PORT1);
            g_cy_priv->system_mode[SIE1] = HOST_ROLE;

            break;

        case DE4_HPI:
            cy_err("Configuring for DE4");

            /* Initialize the Low Level Controller (lcd) structures */
            if (lcd_init(0, 0, 0, 0, g_cy_priv) == ERROR)
            {
                release_region (g_cy_priv->cy_addr, CY67X00_ADDR_SIZE);
                free_irq (g_cy_priv->cy_irq, g_cy_priv);
                kfree (g_cy_priv);
                g_cy_priv = 0;
                return ERROR;
            }

            /* Set HW SIE2 as a peripherial */
            pcd_init(SIE1, DE4, g_cy_priv);
            pcd_switch_role(PERIPHERAL_ROLE, g_cy_priv);
            g_cy_priv->system_mode[SIE1] = PERIPHERAL_ROLE;

            break;

#if defined(microblaze) || defined(__microblaze__)
        case ML40X_HPI:
            /* 
             * For Xilinx ML40X boards
             */
            cy_err("Configuring for Xilinx ML40x");

            /* Initialize the Low Level Controller (lcd) structures */
            if (lcd_init(0, 0, 0, 0, g_cy_priv) == ERROR)
            {
                release_region (g_cy_priv->cy_addr, CY67X00_ADDR_SIZE);
                free_irq (g_cy_priv->cy_irq, g_cy_priv);
                kfree (g_cy_priv);
                g_cy_priv = 0;
                return ERROR;
            }

           /* Init pcd first so remainder EZ-HOST memory is left for hcd */
            /* Set HW SIE2 as a peripherial */
            pcd_init(SIE2, DE3, g_cy_priv);
            pcd_switch_role(PERIPHERAL_ROLE, g_cy_priv);
            g_cy_priv->system_mode[SIE2] = PERIPHERAL_ROLE;


            /* Initialize host port(s) */
            hcd_init(g_cy_priv, PORT0);
            hcd_init(g_cy_priv, PORT1);

            /* Initialize the SIEs */
            hc_host_usb_init (g_cy_priv, SIE1);

            /* Enable host ports */
            hc_enable_port(g_cy_priv, PORT0);
            hc_enable_port(g_cy_priv, PORT1);

            break;
#endif
        default:
            cy_err("Error HW mode, dip = 0x%X\n", hw_config);
            cy_err("Configuring for host");

            /* Initialize the Low Level Controller (lcd) structures */
            if (lcd_init(0, 0, 0, 0, g_cy_priv) == ERROR)
            {
                release_region (g_cy_priv->cy_addr, CY67X00_ADDR_SIZE);
                free_irq (g_cy_priv->cy_irq, g_cy_priv);
                kfree (g_cy_priv);
                g_cy_priv = 0;
                return ERROR;
            }

            /* Initialize host port(s) */
            hcd_init(g_cy_priv, PORT0);
            hcd_init(g_cy_priv, PORT1);
            hcd_init(g_cy_priv, PORT2);
            hcd_init(g_cy_priv, PORT3);

            /* Initialize the SIEs */
            hc_host_usb_init (g_cy_priv, SIE1);
            hc_host_usb_init (g_cy_priv, SIE2);

            /* Enable host ports */
            hc_enable_port(g_cy_priv, PORT0);
            hc_enable_port(g_cy_priv, PORT1);
            hc_enable_port(g_cy_priv, PORT2);
            hc_enable_port(g_cy_priv, PORT3);

            break;
    }

    cy_dbg("exit \n");
    return SUCCESS;
}


/*****************************************************************
 * Function Name: hci_hcd_init
 *
 * This is an init function, and it is the first function being called
 *
 * Input: none
 *
 * Return: SUCCESS or error condition
 *****************************************************************/

static int __init hci_hcd_init (void)
{
    int ret;

    console_verbose();

    cy_dbg("Enter hci_hcd_init \n");

    ret = cy67x00_init (base_addr, data_reg_addr, irq);

    if( ret != SUCCESS )
    {
        cy_err("cy67x00_init returned error.");
    }

    return ret;
}


/*****************************************************************
 * Function Name: hci_hcd_cleanup
 *
 * This is a cleanup function, and it is called when module is
 * unloaded.
 *
 * Input: N/A
 *
 * Return: N/A
 *****************************************************************/

static void __exit hci_hcd_cleanup (void)
{
    struct list_head *  hci_l;
    hci_t * hci = g_cy_priv->hci;

    cy_dbg("Enter hci_hcd_cleanup \n");
    for (hci_l = hci->hci_hcd_list.next; hci_l != &hci_hcd_list;)
        {
        hci = list_entry (hci_l, hci_t, hci_hcd_list);
        hci_l = hci_l->next;
        cy67x00_release_priv(g_cy_priv);
        }
}

/*****************************************************************
 *
 * Function Name: Various
 *
 * This is a collection of handlers called at interrupt context
 * from the main interrupt handler defined in lcd.
 *
 * Input: cy_priv - Private data structure maintaining certain
 *                  state for hcd, pcd, lcd, otg.
 *
 * Return: N/A
 *
 *****************************************************************/

void hcd_irq_sofeop1(cy_priv_t *cy_priv)
{
    hci_t * hci = cy_priv->hci;

    if( hci == 0 )
    {
        return;
    }
    /* cy_dbg("sh_schedule_trans \n"); */
    sh_schedule_trans (cy_priv, SIE1);
}

/******************************************************************/

void hcd_irq_sofeop2(cy_priv_t *cy_priv)
{
    hci_t * hci = cy_priv->hci;

    if( hci == 0 )
    {
        return;
    }

    sh_schedule_trans (cy_priv, SIE2);
}

/******************************************************************/

void hcd_irq_mb_in(cy_priv_t *cy_priv)
{
    /* currently not used */
}

/******************************************************************/

void hcd_irq_mb_out(unsigned int message, int sie, cy_priv_t *cy_priv)
{
    hci_t *hci = cy_priv->hci;
    sie_t * sie_struct = &hci->sie[sie];

    if( hci == NULL )
    {
        return;
    }

    if (message & HUSB_TDListDone)
    {
        /* HOST: CY7C67200_300 has finished transfer of the TD */

        /* Check the status of the TDs */
        sh_td_list_check_stat (cy_priv, sie);

        sie_struct->td_active = FALSE;

        /* shedule the next set of TDs */
        cy_dbg("sh_schedule_trans \n");
        sh_schedule_trans (cy_priv, sie);
    }
}

/******************************************************************/

void hcd_irq_resume1(cy_priv_t *cy_priv)
{
    unsigned short usb_stats;
    hci_t *hci = cy_priv->hci;
    unsigned short usb_stat_sie = 0;
    otg_t *otg = cy_priv->otg;

    if( hci == 0 )
    {
        return;
    }

    /* Read the USB Interrupt Status Register for SIE 1*/

    lcd_read_reg(HOST1_STAT_REG, &usb_stats, cy_priv);

    cy_dbg("HOST1_STAT_REG = 0x%X \n", usb_stats);

    if (otg != NULL)
    {
        /* Are we in the USB OTG States to detect HNP Accept? */

        if (usb_stats & A_CHG_IRQ_FLG)
        {
            if( reset_in_progress[PORT0] != 0 )
            {
                usb_stat_sie =  A_CHG_IRQ_FLG | A_SE0_STAT | A_WAKE_IRQ_FLG |
                                 B_CHG_IRQ_FLG | B_SE0_STAT | B_WAKE_IRQ_FLG;

                lcd_write_reg(HOST1_STAT_REG, usb_stat_sie, cy_priv);
                return;
            }

            /* determine if the HNP has been accepted by the device */

            if (usb_stats & A_SE0_STAT)
            {
                otg->b_conn = FALSE;
                otg->a_conn = FALSE;
                usb_stat_sie |= A_SE0_STAT;
            }
            else
            {
                if(otg->id == A_DEV)
                {
                    otg->b_conn = TRUE;
                }
                else
                {
                    otg->a_conn = TRUE;
                }
            }

            if (!otg->a_vbus_vld && otg->state == a_idle)
            {
                /* OTG SRP Functionality:
                 * SRP request has been detected.
                 */

                otg->b_conn = FALSE;
                otg->a_srp_det = TRUE;
                otg->a_bus_req = TRUE;
            }
            else
            {
                otg->a_srp_det = FALSE;
            }

            usb_stat_sie |= A_CHG_IRQ_FLG;
        }

        /* Detect Remote Wakeup */

        if (usb_stats & A_WAKE_IRQ_FLG)
        {
            /* Detected Remote Wakeup on port A */
            /* Set the HCD to be active and start SOF */

            otg->b_bus_resume = TRUE;
            usb_stat_sie |= A_WAKE_IRQ_FLG;
        }

        update_otg_state(otg);

        /* Clear the B port interrupts just in case */
        usb_stat_sie |= B_SE0_STAT | B_CHG_IRQ_FLG | B_WAKE_IRQ_FLG;
    }
    else
    {
        if (usb_stats & A_CHG_IRQ_FLG)
        {
            if (reset_in_progress[PORT0] == 0)
            {
                /* USB Insert/Remove Interrupt on port B*/
                /* qualify with the USB Reset */

                /* Determine if a device has connected or disconnected by
                 * reading the USB Reset bit of the USB status register */
                if (hci->valid_host_port[PORT0] != 1)
                {
                    cy_err("invalid host port 0");
                }
                else
                {
                    if (usb_stats & A_SE0_STAT)
                    {
                        /* Device has been disconnected */
                        handle_device_removal(cy_priv, PORT0);

                    }
                    else
                    {
                        /* Device has been connected */
                        handle_device_insertion(cy_priv, PORT0);
                    }
                }
            }
            usb_stat_sie |= A_CHG_IRQ_FLG;
        }

        if (usb_stats & B_CHG_IRQ_FLG)
        {
            if (reset_in_progress[PORT1] == 0)
            {
                /* USB Insert/Remove Interrupt on port B*/
                /* qualify with the USB Reset */

                /* Determine if a device has connected or disconnected by
                 * reading the USB Reset bit of the USB status register */

                if (hci->valid_host_port[PORT1] != 1)
                {
                    cy_err("invalid host port 1");
                }
                else
                {
                    if (usb_stats & B_SE0_STAT)
                    {
                        /* Device has been disconnected */
                        handle_device_removal(cy_priv, PORT1);
                    }
                    else
                    {
                        /* Device has been connected */
                        handle_device_insertion(cy_priv, PORT1);
                    }
                }
            }
            usb_stat_sie |= B_CHG_IRQ_FLG;
        }

        if (usb_stats & A_WAKE_IRQ_FLG)
        {
            /* Detected Remote Wakeup on port A */
            handle_remote_wakeup(cy_priv, PORT0);
            usb_stat_sie |= A_WAKE_IRQ_FLG;
        }

        if (usb_stats & B_WAKE_IRQ_FLG)
        {
            /* Detected Remote Wakeup on port B */
            handle_remote_wakeup(cy_priv, PORT1);
            usb_stat_sie |= B_WAKE_IRQ_FLG;
        }
    }

    /* Detect Remote Wakeup */
    /* clear the associated interrupt(s) */

    lcd_write_reg(HOST1_STAT_REG, usb_stat_sie, cy_priv);
}

/******************************************************************/

void hcd_irq_resume2(cy_priv_t *cy_priv)
{
    unsigned short usb_stats;
    hci_t *hci = cy_priv->hci;
    unsigned short usb_stat_sie = 0;

    if( hci == 0 )
    {
        return;
    }

    /* Read the USB Interrupt Status Register for SIE 1*/

    lcd_read_reg(HOST2_STAT_REG, &usb_stats, cy_priv);

    cy_dbg("HOST2_STAT_REG = 0x%X \n", usb_stats);

    if (usb_stats & A_CHG_IRQ_FLG)
    {
        if (reset_in_progress[PORT2] == 0)
        {
            /* USB Insert/Remove Interrupt on port A*/

            /* Determine if a device has connected or disconnected by
             * reading the USB Reset bit of the USB status register */

            if (hci->valid_host_port[PORT2] != 1)
            {
                cy_err("invalid host port 2");
            }
            else
            {
                if (usb_stats & A_SE0_STAT)
                {
                    /* device has been removed */
                    handle_device_removal(cy_priv, PORT2);

                }
                else
                {
                    /* device has been inserted */
                    handle_device_insertion(cy_priv, PORT2);
                }
            }
        }
        usb_stat_sie |= A_CHG_IRQ_FLG;
    }

    if (usb_stats & B_CHG_IRQ_FLG)
    {
        if (reset_in_progress[PORT3] == 0)
        {
            /* USB Insert/Remove Interrupt on port B*/
            /* qualify with the USB Reset */

            /* Determine if a device has connected or disconnected by
             * reading the USB Reset bit of the USB status register */
            if (hci->valid_host_port[PORT3] != 1)
            {
                cy_err("invalid host port 3");
            }
            else
            {
                if (usb_stats & B_SE0_STAT)
                {
                    /* device has been removed */
                    handle_device_removal(cy_priv, PORT3);

                }
                else
                {
                    /* Device has been connected */
                    handle_device_insertion(cy_priv, PORT3);
                }
            }
        }
        usb_stat_sie |= B_CHG_IRQ_FLG;
    }

    /* Detect Remote Wakeup */

    if (usb_stats & A_WAKE_IRQ_FLG)
    {
        /* Detected Remote Wakeup on port A */
        handle_remote_wakeup(cy_priv, PORT2);
        usb_stat_sie |= A_WAKE_IRQ_FLG;
    }

    if (usb_stats & B_WAKE_IRQ_FLG)
    {
        /* Detected Remote Wakeup on port B */
        handle_remote_wakeup(cy_priv, PORT3);
        usb_stat_sie |= B_WAKE_IRQ_FLG;
    }

    /* clear the associated interrupt */
    lcd_write_reg(HOST2_STAT_REG, usb_stat_sie, cy_priv);
}

/*****************************************************************/

void hcd_irq_enable_host_periph_ints(cy_priv_t *cy_priv)
{
    hci_t *hci = cy_priv->hci;

    if( hci == 0 )
    {
        return;
    }

    /* enable the interrupt:
     * At this point, the host may become a peripheral or vise versa
     * It is necessary to setup Interrupt Enable based on the "hostship"
     * of the current SIE.
     */

    cy_dbg("hcd_irq_enable_host_periph_ints enter: cy_priv = %p \n", cy_priv);

    if (is_host_enable(hci, PORT0))
    {
        lcd_write_reg(HOST1_IRQ_EN_REG, DFLT_HOST1_IRQ_ENBL, cy_priv);
    }
    else
    {
        lcd_write_reg(HOST1_IRQ_EN_REG, DFLT_PERIPHERAL1_IRQ_ENBL, cy_priv);
    }

    if (is_host_enable(hci, PORT2))
    {
        lcd_write_reg(HOST2_IRQ_EN_REG, DFLT_HOST2_IRQ_ENBL, cy_priv);
    }
    else
    {
        lcd_write_reg(HOST2_IRQ_EN_REG, DFLT_PERIPHERAL2_IRQ_ENBL, cy_priv);
    }
}


/******************************************************************
 * THE FOLLOWING FUNCTIONS ARE BOARD SPECIFIC
 *
 * MODIFIED FOR YOUR OWN BOARD!!
 *
 *******************************************************************/

/*****************************************************************
 * Function Name: cy67x00_get_hw_config
 *
 * This function is board specific.  It reads the DIP switch from
 * CY7C67200_300 to determine the HW operating mode
 *
 * Input: n/a
 *
 * Return:  DIP switch value
 *****************************************************************/

static int cy67x00_get_hw_config (void)
{
    unsigned short val;

#if defined(microblaze) || defined(__microblaze__)
    val = ML40X_HPI;
#else
    val = inw(SBC_DIP_REG);
#endif
    return val;
}


/*****************************************************************
 * Function Name: cy67x00_get_current_frame_number
 *
 * This function is board specific.  It reads the frame number
 * from CY7C67200_300 for the specified sie.
 *
 * Input:
 *          cy_priv = private data structure
 *          port_num = Port number
 *
 * Return:  Current Frame Number
 *****************************************************************/

int cy67x00_get_current_frame_number (cy_priv_t * cy_priv, int port_num)
{
    unsigned short temp_val = 0;


    if ((port_num == PORT0) || (port_num == PORT1))
        {
        lcd_read_reg(HOST1_FRAME_REG, &temp_val, cy_priv);
        }
    else
        {
        lcd_read_reg(HOST2_FRAME_REG, &temp_val, cy_priv);
        }

    temp_val &= HOST_FRAME_NUM;

    if (temp_val == 0)
        temp_val = HOST_FRAME_NUM;
    else
        temp_val -= 1;

    return temp_val;
}


/*****************************************************************
 * Function Name: cy67x00_get_next_frame_number
 *
 * This function is board specific.  It reads the frame number
 * from CY7C67200_300 for the specified sie.
 *
 * Input:
 *          cy_priv = private data structure
 *          port_num = Port number
 *
 * Return:  Next Frame Number
 *****************************************************************/

int cy67x00_get_next_frame_number (cy_priv_t * cy_priv, int port_num)
{
    unsigned short temp_val = 0;

    if ((port_num == PORT0) || (port_num == PORT1))
        {
        lcd_read_reg(HOST1_FRAME_REG, &temp_val, cy_priv);
        }
    else
        {
        lcd_read_reg(HOST2_FRAME_REG, &temp_val, cy_priv);
        }

    temp_val &= HOST_FRAME_NUM;
    return temp_val;
}


/*****************************************************************
 * Function Name: init_irq
 *
 * This function is board specific.  It sets up the interrupt to
 * be an edge trigger and trigger on the rising edge
 *
 * Input: N/A
 *
 * Return value  : N/A
 *****************************************************************/

void init_irq(void)
{
/* 
 * Initialize the IRQ to be edge trigger
 * BJC - This is not required for ML40x boards.
 * IRQ attribute is set in the opb_cypress_usb core.
 */
#if !(defined(microblaze) || defined(__microblaze__)) 
    GPDR &= ~(1<<GPIO_POS);
    set_GPIO_IRQ_edge (1<<GPIO_POS , GPIO_RISING_EDGE);
#endif
}


/*****************************************************************
 * Function Name: cy67x00_hw_reset
 *
 * This function is board specific.  The board starts out with the
 * ASIC in Reset on the mezzanine card (if present).
 *
 * Input: N/A
 *
 * Return value  : SUCCESS or ERROR
 *****************************************************************/

static int cy67x00_hw_reset (void)
{
    /* hard reset usb */
    CY_HW_RESET;

    mdelay(500);

    /* 
     * Write 0x4 to address HPI Mail box register
     * Send a soft reset command to lcd_idle task in Cypress BIOS
     */
    out_be16((volatile unsigned *) g_cy_priv->cy_addr + HPI_MAILBOX_ADDR,
								 COMM_RESET);

    /* Set the reset to settle.  This requires about 500 ms */
    mdelay(500);
    cy_dbg("After USB Reset \n");

    return SUCCESS;
}

module_init (hci_hcd_init);
module_exit (hci_hcd_cleanup);


MODULE_AUTHOR ("<usbapps@cypress.com>");
MODULE_DESCRIPTION ("CY7C67200/300 USB Host/Peripheral Embedded Controller Driver");
