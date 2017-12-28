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

/* This file implements the OTG state diagram. */

/* Included files. */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include "cy7c67200_300_otg.h"
#include "cy7c67200_300_lcd.h"
#include "usbd/bi/cy7c67200_300_pcd.h"
#include "cy7c67200_300_debug.h"
#include "cy7c67200_300.h"

 
/* Data definitions */

#define START_SOF          10
#define STOP_SOF          11

#define RAISE_VBUS          20
#define DROP_VBUS          21


/* global */

char *state_names[] =
{
    "a_idle",
    "a_wait_vrise",
    "a_wait_bcon",
    "a_host",
    "a_suspend",
    "a_peripheral",
    "a_wait_vfall",
    "a_vbus_err",
    "b_idle",
    "b_srp_init",
    "b_peripheral",
    "b_wait_acon",
    "b_host"
};

extern void detected_hnp_accepted(void);
extern void hcd_switch_role(int new_role, cy_priv_t * cy_priv);

extern int pcd_signal_srp(size_t vbus_pulse_time, cy_priv_t *cy_priv);

extern void handle_device_insertion (cy_priv_t * cy_priv, int port_num);
extern void handle_device_removal (cy_priv_t * cy_priv, int port_num);
extern void handle_remote_wakeup (cy_priv_t * cy_priv, int port_num);

static cy_priv_t * otg_cy_priv = NULL;

/* forware declaration */
static void a_host_state(otg_t * otg);
static void a_idle_state(otg_t * otg);
static void a_peripheral_state(otg_t * otg);
static void a_suspend_state(otg_t * otg);
static void a_vbus_err_state(otg_t * otg);
static void a_wait_bcon_state(otg_t * otg);
static void a_wait_vfall_state(otg_t * otg);
static void a_wait_vrise_state(otg_t * otg);
static void b_host_state(otg_t * otg);
static void b_idle_state(otg_t * otg);
static void b_peripheral_state(otg_t * otg);
static void b_srp_init_state(otg_t * otg);
static void b_wait_acon_state(otg_t * otg);

/* Functionality. */

/*
 *  FUNCTION: Timer utilities
 *
 *  DESCRIPTION:
 *    These functions are used to support timeout functionality for
 *    the otg state diagram.
 */

struct timer_list to_timer;


/* The following enumerations are in jiffies. */
typedef enum timeout_times
{
    Ta_wait_vrise =  10,    /* 100ms */
    Ta_wait_bcon  =  -1,    /* infinite */
    Ta_aidl_bdis  =  35,    /* 350ms */
    Tb_ase0_brst  =  31,    /* We need 3.125 ms */

} OTG_Timeouts;


void otg_timeout(unsigned long data)
{
    cy_err("timeout");
    *((boolean*)data) = TRUE;
    update_otg_state( (otg_t*)otg_cy_priv->otg );
}


static void otg_start_timer(boolean *state, OTG_Timeouts time)
{
    if( time < 0 )
    {
        return;
    }

    if( timer_pending(&to_timer) )
    {
        cy_err("ERROR: timer in use - restarting");
    }

    *state = FALSE;
    to_timer.data = (unsigned long)state;
    mod_timer( &to_timer, jiffies + time );
}


static void otg_cancel_timer(boolean *state)
{
    *state = FALSE;
    del_timer(&to_timer);
}

/*
 *
 */

void switch_role(int new_role)
{
    switch (new_role)
    {
    case PERIPHERAL_ROLE:
        hcd_switch_role(new_role, otg_cy_priv);
        pcd_switch_role(new_role, otg_cy_priv);        
        break;

    case HOST_ROLE:
    case HOST_INACTIVE_ROLE:
        pcd_switch_role(new_role, otg_cy_priv);        
        hcd_switch_role(new_role, otg_cy_priv);
        break;
    }

    otg_cy_priv->system_mode[SIE1] = new_role; 
}

void otg_print_state(void)
{
	unsigned short intStat;

    if (otg_cy_priv != NULL && otg_cy_priv->otg != NULL) 
    {
        otg_t * otg = otg_cy_priv->otg;

        printk("\n");
        printk("\n");
        printk("\n");
        printk("state              = %s\n", 
                state_names[otg->state]);
        printk("id                 = %s\n", 
                otg->id == A_DEV ? "A_DEV" : "B_DEV");
        printk("a_srp_det          = %s\n", 
                otg->a_srp_det == 0 ? "false" : "true");
        printk("a_set_b_hnp_en     = %s\n", 
                otg->a_set_b_hnp_en == 0 ? "false" : "true");
        printk("b_conn             = %s\n", 
                otg->b_conn == 0 ? "false" : "true");
        printk("a_bus_drop         = %s\n", 
                otg->a_bus_drop == 0 ? "false" : "true");
        printk("a_bus_req          = %s\n", 
                otg->a_bus_req == 0 ? "false" : "true");
        printk("a_suspend_req      = %s\n", 
                otg->a_suspend_req == 0 ? "false" : "true");
        printk("b_bus_suspend      = %s\n", 
                otg->b_bus_suspend == 0 ? "false" : "true");
        printk("b_bus_resume       = %s\n", 
                otg->b_bus_resume == 0 ? "false" : "true");
        printk("a_sess_vld         = %s\n", 
                otg->a_sess_vld == 0 ? "false" : "true");
        printk("a_vbus_vld         = %s\n", 
                otg->a_vbus_vld == 0 ? "false" : "true");
        printk("a_wait_vrise_tmout = %s\n", 
                otg->a_wait_vrise_tmout == 0 ? "false" : "true");
        printk("a_wait_bcon_tmout  = %s\n", 
                otg->a_wait_bcon_tmout == 0 ? "false" : "true");
        printk("a_aidl_bdis_tmout  = %s\n", 
                otg->a_aidl_bdis_tmout == 0 ? "false" : "true");
        printk("b_bus_req          = %s\n", 
                otg->b_bus_req == 0 ? "false" : "true");
        printk("b_sess_end         = %s\n", 
                otg->b_sess_end == 0 ? "false" : "true");
        printk("b_se0_srp          = %s\n", 
                otg->b_se0_srp == 0 ? "false" : "true");
        printk("b_srp_done         = %s\n", 
                otg->b_srp_done == 0 ? "false" : "true");
        printk("b_sess_vld         = %s\n", 
                otg->b_sess_vld == 0 ? "false" : "true");
        printk("a_bus_resume       = %s\n", 
                otg->a_bus_resume == 0 ? "false" : "true");
        printk("b_hnp_en           = %s\n", 
                otg->b_hnp_en == 0 ? "false" : "true");
        printk("a_bus_suspend      = %s\n", 
                otg->a_bus_suspend == 0 ? "false" : "true");
        printk("b_se0_brst_tmout   = %s\n", 
                otg->b_se0_brst_tmout == 0 ? "false" : "true");
        printk("a_conn             = %s\n", 
                otg->a_conn == 0 ? "false" : "true");
        printk("b_ase0_brst_tmout  = %s\n", 
                otg->b_ase0_brst_tmout == 0 ? "false" : "true");
        printk("b_wait_acon_tmout  = %s\n", 
                otg->b_wait_acon_tmout == 0 ? "false" : "true");

        lcd_read_reg (HOST1_IRQ_EN_REG, &intStat, otg_cy_priv);
        printk("HOST1_IRQ_EN_REG = 0x%X\n", intStat);
        printk("\n");
        printk("\n");
    }
}

int otg_state(int * state)
{
    if (otg_cy_priv != NULL && otg_cy_priv->otg != NULL) 
    {
        *state = ((otg_t*)otg_cy_priv->otg)->state;
        return 0;
    }

    return -1;
}

int otg_id(int * id)
{
    int ret = -1;
    otg_t * otg;

    if (otg_cy_priv != NULL) {
        if (otg_cy_priv->otg != NULL) {
            otg = otg_cy_priv->otg;
            *id = otg->id;
            ret = 0;
        }
    }
    return ret;
}

int otg_start_session(void)
{
    int ret = -1;
    otg_t * otg;

    if (otg_cy_priv != NULL) {
        if (otg_cy_priv->otg != NULL) {
            otg = otg_cy_priv->otg;
            otg->a_bus_drop = FALSE;
            otg->a_bus_req    = TRUE;
            /*
            cy_err("otg_start_session: state = %s", 
                    state_names[otg->state]);
                    */
            update_otg_state(otg);
            ret = 0;
        }
    }
    return ret;
}

int otg_end_session(void)
{
    int ret = -1;
    otg_t * otg;

    if (otg_cy_priv != NULL) {
        if (otg_cy_priv->otg != NULL) {
            otg = otg_cy_priv->otg;
            otg->a_bus_drop = TRUE;
            otg->a_bus_req  = FALSE;
            otg->a_srp_det = FALSE;
            /*
            cy_err("otg_end_session: state = %s", 
                    state_names[otg->state]);
                    */
            update_otg_state(otg);
            ret = 0;
        }
    }
    return ret;
}


int otg_offer_hnp(void)
{
    int ret = -1;
    otg_t * otg;

    if (otg_cy_priv != NULL) 
    {
        if (otg_cy_priv->otg != NULL) 
        {
            otg = otg_cy_priv->otg;

            if( otg->state == a_host )
            {
                otg->a_set_b_hnp_en = TRUE;
                otg->a_bus_req = FALSE;
                otg->b_bus_resume = FALSE;
                update_otg_state(otg);
                ret = 0;
            }
        }
    }

    return ret;
}


int otg_end_hnp(void)
{
    int ret = -1;
    otg_t * otg;

    if (otg_cy_priv != NULL) 
    {
        if (otg_cy_priv->otg != NULL) 
        {
            otg = otg_cy_priv->otg;

            if(otg->state == b_host)
            {
                otg->b_bus_req = FALSE;
                update_otg_state(otg);
                ret = 0;
            }
        }
    }

    return ret;
}


void otg_set_device_not_respond(void)
{
    otg_t * otg;

    if (otg_cy_priv != NULL) 
    {
        if (otg_cy_priv->otg != NULL) 
        {
            otg = otg_cy_priv->otg;
            otg->device_not_respond = TRUE;
            otg->a_bus_drop = TRUE;
            otg->a_bus_req  = FALSE;
            otg->a_srp_det = FALSE;
            update_otg_state(otg);
        }
    }
}


int otg_get_device_not_respond(void)
{
    int ret = FALSE;
    otg_t * otg;

    if (otg_cy_priv != NULL) 
    {
        if (otg_cy_priv->otg != NULL) 
        {
            otg = otg_cy_priv->otg;

			if (otg->device_not_respond == TRUE)
			{
				ret = TRUE;
				otg->device_not_respond = FALSE;
			}
        }
    }

    return ret;
}

void otg_set_unsupport_device(void)
{
    otg_t * otg;

    if (otg_cy_priv != NULL) 
    {
        if (otg_cy_priv->otg != NULL) 
        {
            otg = otg_cy_priv->otg;
            otg->unsupported_device = TRUE;
            otg->a_bus_drop = TRUE;
            otg->a_bus_req  = FALSE;
            otg->a_srp_det = FALSE;
            update_otg_state(otg);
        }
    }
}


int otg_get_unsupport_device(void)
{
    int ret = FALSE;
    otg_t * otg;

    if (otg_cy_priv != NULL) 
    {
        if (otg_cy_priv->otg != NULL) 
        {
            otg = otg_cy_priv->otg;

			if (otg->unsupported_device == TRUE)
			{
				ret = TRUE;
				otg->unsupported_device = FALSE;
			}
        }
    }

    return ret;
}



void otg_init(cy_priv_t * cy_priv)
{
    unsigned short otg_ctl = 0;
    otg_t * otg;
    
    /* store the cy_priv to our global cy_priv structure */
    otg_cy_priv = cy_priv;

    /* alloc and initialize OTG data structure */
    otg = (otg_t *) kmalloc (sizeof(otg_t), GFP_KERNEL);
    memset(otg, 0, sizeof(otg_t));

    /* Initialize timeout timer */
    init_timer(&to_timer);
    to_timer.function = otg_timeout;

    /* Disable VBUS */
    lcd_command(LCD_DISABLE_OTG_VBUS, 0, PORT0, NULL, 0, otg_cy_priv);

    /* read the OTG ID pin */
    lcd_read_reg(OTG_CTL_REG, &otg_ctl, cy_priv);

     if (otg_ctl & OTG_ID_STAT)
    {
        otg->state = b_idle;
        otg->id = B_DEV;
        otg->b_sess_end = TRUE;
    }
    else
    {
        otg->state = a_idle;
        otg->id = A_DEV;
    }

    cy_priv->otg = otg;
}

void a_idle_state(otg_t * otg)
{
    if (otg->state != a_idle) 
    {
        otg->a_bus_drop = FALSE;

        otg->state = a_idle;
    }
    
    switch (otg->id)
    {
    case A_DEV:
        if ((otg->a_bus_drop == 0) && (otg->a_bus_req || otg->a_srp_det)) 
        {
            switch_role(HOST_ROLE);
            a_wait_vrise_state(otg);
        }
        break;
    case B_DEV:
        b_idle_state(otg);
        break;
    }
}

void a_wait_vrise_state(otg_t * otg)
{
    if (otg->state != a_wait_vrise) 
    {
        otg->state = a_wait_vrise;

        /* start a_wait_vrise timer */
        otg_start_timer( &otg->a_wait_vrise_tmout, Ta_wait_vrise );

        otg->a_srp_det = FALSE;

        /* driver VBUS */
        lcd_command(LCD_ENABLE_OTG_VBUS, 0, PORT0, NULL, 0, otg_cy_priv);
    }
    
    if( otg->a_wait_vrise_tmout )
    {
        unsigned short value;

        lcd_read_reg(OTG_CTL_REG, &value, otg_cy_priv);

        if (value & VBUS_VALID_FLG)
        {
            otg->a_vbus_vld = TRUE;
            /* HPI can not distinguish between the two thresholds. */
            otg->a_sess_vld = otg->b_sess_vld = TRUE;
            otg->a_wait_vrise_tmout = FALSE;
        }
        else
        { 
            otg->a_vbus_vld = FALSE;
            /* HPI can not distinguish between the two thresholds.*/
            otg->a_sess_vld = otg->b_sess_vld = 
                (value & OTG_DATA_STAT) ? TRUE : FALSE;
        }
    }

    switch (otg->id)
    {
    case A_DEV:
        if (otg->a_bus_drop || otg->a_vbus_vld || otg->a_wait_vrise_tmout) 
        {
            otg_cancel_timer( &otg->a_wait_vrise_tmout );
            a_wait_bcon_state(otg);
        }
        break;
    case B_DEV:
        otg_cancel_timer( &otg->a_wait_vrise_tmout );
        a_wait_bcon_state(otg);
        break;
    } 
}

void a_wait_bcon_state(otg_t * otg)
{
    if (otg->state != a_wait_bcon) 
    {
        unsigned short usb_ctl_val;

        /* Check for B-connected */
        lcd_read_reg(USB1_CTL_REG, &usb_ctl_val, otg_cy_priv);

        /* if device is connected */
        if ( usb_ctl_val & (A_DP_STAT | A_DM_STAT) )
        {
            otg->b_conn = TRUE;
        }

        if (otg->state == a_host && otg->a_bus_req == TRUE)
        {
            /* the previous state is a_host. This means that device has been 
             * removed 
             */
            handle_device_removal(otg_cy_priv, PORT0);
        }
       
        otg_start_timer( &otg->a_wait_bcon_tmout, Ta_wait_bcon );

        otg->state = a_wait_bcon;

        otg->b_bus_suspend = FALSE;
    } 
    
    switch (otg->id)
    {
    case A_DEV:
        if (otg->a_bus_drop || otg->a_wait_bcon_tmout) 
        {
            otg_cancel_timer( &otg->a_wait_bcon_tmout );
            a_wait_vfall_state(otg);
        }
        else if (!otg->a_vbus_vld)
        {
            otg_cancel_timer( &otg->a_wait_bcon_tmout );
            a_vbus_err_state(otg);
        }
        else if (otg->b_conn)
        {
            otg_cancel_timer( &otg->a_wait_bcon_tmout );
            a_host_state(otg);
        }
        break;
    case B_DEV:
        otg_cancel_timer( &otg->a_wait_bcon_tmout );
        a_wait_vfall_state(otg);
        break;
    } 
}

void a_host_state(otg_t * otg)
{
    if (otg->state != a_host) 
    {
        if (otg->state == a_wait_bcon)
        {
            /* Device has been inserted */
            handle_device_insertion(otg_cy_priv, PORT0);
        }
        else if (otg->state == a_suspend && otg->b_bus_resume)
        {
            /* Remote Wakeup */
            handle_remote_wakeup(otg_cy_priv, PORT0);
            lcd_command(LCD_START_SOF, 0, PORT0, NULL, 0, otg_cy_priv);
        }

        otg->b_bus_resume = FALSE;
        otg->state = a_host;
    }

    switch (otg->id)
    {
    case A_DEV:
        if (otg->a_bus_drop || !otg->b_conn) 
        {
            a_wait_bcon_state(otg);
        }
        else if (!otg->a_vbus_vld)
        {
            a_vbus_err_state(otg);
        }
        else if (!otg->a_bus_req || otg->a_suspend_req)
        {
            a_suspend_state(otg);
        }
        break;

    case B_DEV:
        a_wait_bcon_state(otg);
        break;
    } 
}

void a_suspend_state(otg_t * otg)
{
    if (otg->state != a_suspend) {
        lcd_command(LCD_STOP_SOF, 0, PORT0, NULL, 0, otg_cy_priv);    

        /* start the wait for b-device disconnect timer */
        otg_start_timer( &otg->a_aidl_bdis_tmout, Ta_aidl_bdis );

        otg->a_suspend_req = FALSE;
        otg->state = a_suspend;
    } 
    
    switch(otg->id)
    {
    case A_DEV:
        if (otg->a_bus_drop || otg->a_aidl_bdis_tmout) 
        {
            if( otg->a_aidl_bdis_tmout )
            {
                otg->a_bus_req = TRUE;
            }

            otg_cancel_timer( &otg->a_aidl_bdis_tmout );

            a_wait_vfall_state(otg);
        }
        else if (!otg->a_vbus_vld)
        {
            otg_cancel_timer( &otg->a_aidl_bdis_tmout );
            a_vbus_err_state(otg);
        }
        else if (otg->a_bus_req || otg->b_bus_resume)
        {
            otg_cancel_timer( &otg->a_aidl_bdis_tmout );
            a_host_state(otg);
        }
        else if (!otg->b_conn && otg->a_set_b_hnp_en)
        {
            otg_cancel_timer( &otg->a_aidl_bdis_tmout );
            switch_role(PERIPHERAL_ROLE);
            a_peripheral_state(otg);
        }
        else if (!otg->b_conn && !otg->a_set_b_hnp_en)
        {
            otg_cancel_timer( &otg->a_aidl_bdis_tmout );
            a_wait_bcon_state(otg);
        }
        break;

    case B_DEV:
        otg_cancel_timer( &otg->a_aidl_bdis_tmout );
        a_wait_vfall_state(otg);
        break;
    } 
}

void a_peripheral_state(otg_t * otg)
{
    if (otg->state != a_peripheral) {
        otg->state = a_peripheral;

        otg->a_set_b_hnp_en = FALSE;
        otg->a_bus_req = TRUE;
        otg->b_conn = FALSE;
    }

    switch(otg->id)
    {
    case A_DEV:
        if (otg->a_bus_drop) 
        {
            switch_role(HOST_INACTIVE_ROLE);
            a_wait_vfall_state(otg);
        }
        else if (!otg->a_vbus_vld)
        {
            switch_role(HOST_INACTIVE_ROLE);
            a_vbus_err_state(otg);
        }
        else if (otg->b_bus_suspend)
        {
            switch_role(HOST_ROLE);
            a_wait_bcon_state(otg);
        }
        break;
    case B_DEV:
        switch_role(HOST_INACTIVE_ROLE);
        a_wait_vfall_state(otg);
        break;
    }  
}

void a_vbus_err_state(otg_t * otg)
{
    if (otg->state != a_vbus_err) 
    {
        otg->state = a_vbus_err;

        lcd_command(LCD_DISABLE_OTG_VBUS, 0, PORT0, NULL, 0, otg_cy_priv);    
    } 

    switch(otg->id)
    {
    case A_DEV:
        if (otg->a_bus_drop) 
        {
            a_wait_vfall_state(otg);
        }
        break;
    case B_DEV:
        a_wait_vfall_state(otg);
        break;
    }  

}

void a_wait_vfall_state(otg_t * otg)
{
	unsigned short usb_ctl_val;

    if (otg->state != a_wait_vfall) 
    {
        /* if the state is coming from the a_suspend state and
         * the A-device has timeout waiting for a disconnect, and
         * OTG app has offer hnp, then we need to notify the app that
         * an unsuccessful hnp offer occurred 
         */
        switch_role(HOST_INACTIVE_ROLE);

        lcd_command(LCD_DISABLE_OTG_VBUS, 0, PORT0, NULL, 0, otg_cy_priv);

        otg->state = a_wait_vfall;
        otg->a_bus_drop = FALSE;
    }

	/* Check for B-disconnect */
    lcd_read_reg(USB1_CTL_REG, &usb_ctl_val, otg_cy_priv);

	/* if device is not connected */
	if (!(usb_ctl_val & (A_DP_STAT | A_DM_STAT)))
	{
		otg->b_conn = FALSE;
	}

    switch(otg->id)
    {
    case A_DEV:
        if (otg->a_bus_req || (!otg->a_sess_vld && !otg->b_conn)) 
        {
			otg->b_conn = FALSE;
            a_idle_state(otg);
        }
        break;
    case B_DEV:
		otg->b_conn = FALSE;
		otg->b_sess_vld = FALSE;
        a_idle_state(otg);
        break;
    }  
}

void b_idle_state(otg_t * otg)
{
    if (otg->state != b_idle) {
        otg->state = b_idle;
		otg->b_conn = FALSE;
        otg->b_srp_done = FALSE;
    } 

    otg->b_sess_end = !(otg->b_sess_vld);

    switch(otg->id)
    {
    case A_DEV:
        otg->a_bus_req = TRUE;
        a_idle_state(otg);
        break;

    case B_DEV:
        if (otg->b_bus_req && otg->b_sess_end && otg->b_se0_srp)
        {
            b_srp_init_state(otg);
        }
        else if (otg->b_sess_vld)
        {
            b_peripheral_state(otg);
        }
        break;
    }  
}

void b_srp_init_state(otg_t * otg)
{
    if (otg->state != b_srp_init) {
        otg->state = b_srp_init;

        otg->b_se0_srp = FALSE;

        otg->b_srp_done = FALSE;
        pcd_signal_srp( 20, otg_cy_priv );
        otg->b_srp_done = TRUE;
    } 

    switch(otg->id)
    {
    case A_DEV:
        b_idle_state(otg);
        break;

    case B_DEV:
        if (otg->b_srp_done)
        {
            b_idle_state(otg);
        }
        break;
    }  
}

void b_peripheral_state(otg_t * otg)
{
    if (otg->state != b_peripheral) {
        otg->state = b_peripheral;
        otg->a_bus_resume = FALSE;
        otg->a_bus_suspend = FALSE;
        switch_role(PERIPHERAL_ROLE);
		otg->b_bus_req = TRUE;
    } 

    switch(otg->id)
    {
    case A_DEV:
        switch_role(HOST_INACTIVE_ROLE);
        b_idle_state(otg);
        break;

    case B_DEV:
        if (!otg->b_sess_vld)
        {
            switch_role(HOST_INACTIVE_ROLE);
            b_idle_state(otg);
        }
        else if (otg->b_bus_req && otg->b_hnp_en && otg->a_bus_suspend)
        {
            /* Switching to host */
            switch_role(HOST_ROLE);
            b_wait_acon_state(otg);
        }
        break;
    }  
}

void b_wait_acon_state(otg_t * otg)
{
    unsigned short usb_ctl_val;
	int i;

    if (otg->state != b_wait_acon) 
    {
        /* Do not start b_ase0_brst timer, poll instead because
		 * the timer has a resolution of 10ms!!!
		 * otg_start_timer( &otg->b_ase0_brst_tmout, Tb_ase0_brst );
		 */

		otg->b_ase0_brst_tmout = FALSE;
        otg->state = b_wait_acon;
        otg->a_bus_suspend = FALSE;
        otg->b_hnp_en = FALSE;

		/* Wait 3.125 ms for the a-device to connect */
		for (i=0; i<Tb_ase0_brst; i++)
		{
			udelay(100);

			/* if the a-device has not connected then indicate timeout */
		    lcd_read_reg(USB1_CTL_REG, &usb_ctl_val, otg_cy_priv);

			/* if device connects */
			if (usb_ctl_val & A_DP_STAT)
			{
				otg->a_conn = TRUE;
				break;
			}
		}

		if (!(usb_ctl_val & A_DP_STAT))
		{
			otg->b_ase0_brst_tmout = TRUE;
		}
    } 
    
    switch(otg->id)
    {
    case A_DEV:
        switch_role(HOST_INACTIVE_ROLE);
        b_idle_state(otg);
        break;

    case B_DEV:
        if (!otg->b_sess_vld)
        {
			otg->b_ase0_brst_tmout = FALSE;
            switch_role(HOST_INACTIVE_ROLE);
            b_idle_state(otg);
        }
        else if (otg->a_bus_resume || otg->b_ase0_brst_tmout)
        {
			otg->b_ase0_brst_tmout = FALSE;
            otg->device_not_respond = TRUE;
            b_peripheral_state(otg);
        }
        else if (otg->a_conn)
        {
			otg->b_ase0_brst_tmout = FALSE;
            b_host_state(otg);
        }
        break;
    }  
}

void b_host_state(otg_t * otg)
{
    if (otg->state != b_host) 
    {
        handle_device_insertion(otg_cy_priv, PORT0);
        otg->state = b_host;
    }

    switch(otg->id)
    {
    case A_DEV:
        /* Switching to Peripheral */
        switch_role(HOST_INACTIVE_ROLE);
        b_idle_state(otg);
        break;

    case B_DEV:
        if (!otg->b_sess_vld)
        {
            /* Switching to Peripheral */
            switch_role(HOST_INACTIVE_ROLE);
            b_idle_state(otg);
        }
        else if (!otg->b_bus_req || !otg->a_conn)
        {
            /* Switching to Peripheral */
            b_peripheral_state(otg);
        }
        break;
    } 
}

void update_otg_state (otg_t * otg)
{
    otg_states in_state;

    if (otg == NULL)
    {
        return;
    }

    in_state = otg->state;

    switch(otg->state) 
    {
    case a_idle:
        a_idle_state(otg);
        break;

    case a_wait_vrise:
        a_wait_vrise_state(otg);
        break;

    case a_wait_bcon:
        a_wait_bcon_state(otg);
        break;
        
    case a_host:
        a_host_state(otg);
        break;
    case a_suspend:
        a_suspend_state(otg);
        break;  
        
    case a_peripheral:
        a_peripheral_state(otg);
        break;
                
    case a_vbus_err:
        a_vbus_err_state(otg);
        break;
        
    case a_wait_vfall:
        a_wait_vfall_state(otg);
        break;
    
    case b_idle:
        b_idle_state(otg);
        break;

    case b_srp_init:
        b_srp_init_state(otg);
        break;

    case b_peripheral:
        b_peripheral_state(otg);
        break;

    case b_wait_acon:
        b_wait_acon_state(otg);
        break;

    case b_host:
        b_host_state(otg);
        break;
    default: 
        cy_err("update_otg_state: unknown state!");
        break;
    }
}

/* End of file: otg.c */
