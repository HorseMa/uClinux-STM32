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

/* This header file for the OTG state diagram. */

#ifndef OTG_H
#define OTG_H

#include "cy7c67200_300_common.h"

#define A_DEV			0
#define B_DEV 			1

#define CHRG_VBUS_COUNT 100
#define WAIT_BCON_TIMEOUT_COUNT 100
#define WAIT_ACON_TIMEOUT_COUNT 100
typedef int boolean;

/* OTG states definition */

typedef enum otg_states
{
    a_idle,
    a_wait_vrise,
    a_wait_bcon,
    a_host,
    a_suspend,
    a_peripheral,
    a_wait_vfall,
    a_vbus_err,
    b_idle,
    b_srp_init,
    b_peripheral,
    b_wait_acon,
    b_host
} otg_states;

extern char *state_names[];


typedef struct otg_t {
	otg_states state;  		/* The current state of the device */
	int hnp_offer_flag; /* 1 = hnp offered, 0 = hnp has not offerred */
	int hnp_enabled;    /* 1 = hnp enabled, 0 = hnp has not enabled */     
	int srp_start_flag; /* 1 = apps has requested to start srp,
                         * 0 = apps has not requested to start srp */
	int vbus_drop_flag;	  /* 1 = vbus has been down, 0 = vbus is up */

	int unsupported_device; /* 1 = unsupported device inserted */
	int device_not_respond; /* 1 = device not responding */

	/* input parameter to the A-device state diagram */

	int id;     		/* 0 = a-device 1 = b-device */
	
	/* USB host input parameter */
	boolean a_srp_det;      /* SRP detected.. Signal comes from Ins/Rmv IRQ */
	boolean a_set_b_hnp_en;	/* enable hnp: comes from app */
	boolean b_conn;			/* b-device connected: comes from Ins/Rmv IRQ */
	boolean a_bus_drop;		/* A-device request bus drop: app */
	boolean  a_bus_req;		/* A-device request bus: app */
	boolean  a_suspend_req;	/* A-device request bus suspend: app */
	boolean  b_bus_suspend;	/* B-device act as a host suspend bus: missing SOF */
	boolean  b_bus_resume;	/* B-device wants to resume: IRQ */
	boolean  a_sess_vld;		/* session valid: OTG register, but only implemented for HNP */
	boolean  a_vbus_vld;		/* vbus valid: IRQ */
	boolean  a_wait_vrise_tmout;
	boolean  a_wait_bcon_tmout;
	boolean  a_aidl_bdis_tmout;

    /* USB peripheral input parameter */
	boolean  b_bus_req;
	boolean  b_sess_end;	   /* vbus voltage < 0.8 V */
	boolean  b_se0_srp;
	boolean  b_srp_done;
	boolean  b_sess_vld;	    /* vbus voltage between 0.8 and 4.0 V */
	boolean  a_bus_resume;
	boolean  b_hnp_en;
	boolean  a_bus_suspend;
	boolean  b_se0_brst_tmout;
	boolean  a_conn;
	boolean  b_ase0_brst_tmout;
	boolean  b_wait_acon_tmout;
} otg_t;

/* prototypes */

extern void otg_init(cy_priv_t * cy_priv);
extern void update_otg_state (otg_t * otg);
extern void otg_timer_notify(cy_priv_t * cy_priv);
extern int otg_state(int * state);
extern int otg_id(int * id);
extern int otg_start_session(void);
extern int otg_end_session(void);
extern int otg_offer_hnp(void);
extern int otg_end_hnp(void);
extern void otg_print_state(void);
extern void otg_set_device_not_respond(void);
extern int otg_get_device_not_respond(void);
extern void otg_set_unsupport_device(void);
extern int otg_get_unsupport_device(void);

#endif

