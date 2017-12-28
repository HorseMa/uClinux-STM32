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
 * Board Specific Definitions
 */
#if defined(microblaze) || defined(__microblaze__)
/* Pcore Interface */
#define USB_GPIO_BASE           CONFIG_XILINX_GPIO_0_BASEADDR
#define OPB_GPIO2_TRI_REG       0x0C    /* opb_gpio cores tri state register */

/* Cypress Interface */
#define CYPRESS_USB_BASE        CONFIG_XILINX_CYPRESS_USB_0_MEM0_BASEADDR
#define CYPRESS_USB_DATA_REG    (CYPRESS_USB_BASE + 0x4)
#define CYPRESS_USB_IRQ         CONFIG_XILINX_CYPRESS_USB_0_IRQ
#define CYPRESS_USB_DBG_TRC     CYPRESS_USB_BASE
#define USB_RESET               0x00002000  /* GPIO Pin 13 */

/* HPI Interface */
#define HPI_DATA_ADDR           0x00
#define HPI_MAILBOX_ADDR        0x04
#define HPI_ADDR_ADDR           0x08
#define HPI_STATUS_ADDR         0x0C

/* ML40x SIE Configuration */
#define ML40X_HPI		0xFF
#endif

/* CY7C67200/300 HW RESET Macros */
#if defined(microblaze) || defined(__microblaze__)
/*
 * ML40x - CYPRESS HW Reset
 * Set usb_hpi_reset_n (GPIO2 pin 13) to be an input pin
 * This will cause the pin to get the default value = 1
 * Ref C_TRI_DEFAULT in opb_gpio_0.
 * In misc_logic pin13 is inverted and connected to usb_hpi_reset_n.
 */
#define CY_HW_RESET \
{	\
    int Addr, Data; \
    Addr = (u32) ioremap(USB_GPIO_BASE, 0x20);	\
    Data = in_be32((u32 *)(Addr + OPB_GPIO2_TRI_REG)); \
    out_be32(((volatile unsigned *) (Addr + OPB_GPIO2_TRI_REG)), \
                                                        Data | USB_RESET); \
    out_be32(((volatile unsigned *) (Addr + OPB_GPIO2_TRI_REG)), Data); \
}	
#else
/*
 * CY3663 - CYPRESS HW Reset 
 * Enable the CY7C67200_300.  By default, the CY7C67200_300 expansion card
 * will be held in reset mode.  This will take theCY7C67200_300 card
 * out of reset.
 */
#define CY_HW_RESET \
{    \
    int val; \
    val = inw(SBC_EXP_PORT_PRESENT_REG); \
    if (val == CP_PRESENT) \
    { \
        writew (0x1, SBC_EXP_PORT_RESET); \
        mdelay(500); \
        return SUCCESS; \
    } \
    else \
    { \
        cy_err("no CY7C67200_300 card present"); \
        return ERROR; \
    } \
}
#endif

/* END of Board Specific Definitions */


/*
 * Maximum number of root hub ports.
 */
#define MAX_ROOT_PORTS  1   /* maximum root hub ports */

#define URB_DEL 1

#define MAX_PORT_NUM                    0x4
#define PORT_STAT_DEFAULT               0x0100
#define PORT_CONNECT_STAT               0x1
#define PORT_ENABLE_STAT                0x2
#define PORT_SUSPEND_STAT               0x4
#define PORT_OVER_CURRENT_STAT          0x8
#define PORT_RESET_STAT                 0x10
#define PORT_POWER_STAT                 0x100
#define PORT_LOW_SPEED_DEV_ATTACH_STAT  0x200

#define PORT_CHANGE_DEFAULT             0x0
#define PORT_CONNECT_CHANGE             0x1
#define PORT_ENABLE_CHANGE              0x2
#define PORT_SUSPEND_CHANGE             0x4
#define PORT_OVER_CURRENT_CHANGE        0x8
#define PORT_RESET_CHANGE               0x10


/* OTG */

#define B_HNP_ENABLE                    0x3
#define A_HNP_SUPPORT                   0x4
#define A_ALT_HNP_SUPPORT               0x5

/* TD */

#define TD_PIDEP_OFFSET                 0x04
#define TD_PIDEPMASK_PID                0xF0
#define TD_PIDEPMASK_EP                 0x0F
#define TD_PORTLENMASK_DL               0x02FF
#define TD_PORTLENMASK_PN               0xC000

#define TD_STATUS_OFFSET                0x07
#define TD_STATUSMASK_ACK               0x01
#define TD_STATUSMASK_ERR               0x02
#define TD_STATUSMASK_TMOUT             0x04
#define TD_STATUSMASK_SEQ               0x08
#define TD_STATUSMASK_SETUP             0x10
#define TD_STATUSMASK_OVF               0x20
#define TD_STATUSMASK_NAK               0x40
#define TD_STATUSMASK_STALL             0x80

#define TD_RETRYCNT_OFFSET              0x08
#define TD_RETRYCNTMASK_ACT_FLG         0x10
#define TD_RETRYCNTMASK_TX_TYPE         0x0C
#define TD_RETRYCNTMASK_RTY_CNT         0x03


#define SIE_TD_SIZE             0x200   /* size of the td list */
#define SIE_TD_BUF_SIZE         0x400   /* size of the data buffer */
#define SIE1_TD_OFFSET          0
#define SIE1_BUF_OFFSET         SIE1_TD_OFFSET + SIE_TD_SIZE
#define SIE2_TD_OFFSET          SIE1_BUF_OFFSET + SIE_TD_BUF_SIZE
#define SIE2_BUF_OFFSET         SIE2_TD_OFFSET + SIE_TD_SIZE
#define CY_TD_SIZE              12


/* SBC info */

#define SBC_DIP_REG                 0xd7200000
#define SBC_EXP_PORT_PRESENT_REG    0xd7100000
#define CP_PRESENT                  0x0
#define SBC_EXP_PORT_RESET          0xd7000000

/* Design Examples */

#define SAB_DIP_READ_REG            0x0
#define SAB_DIP_MASK                0x3f
#define DE1_HPI                     0x39
#define DE2_HPI                     0x35
#define DE2_HSS                     0x34
#define DE2_SPI                     0x36
#define DE3_HPI                     0x31
#define DE4_HPI                     0x2D

#define USB_HOST_MODE               1
#define USB_PERIPHERAL_MODE         2

/* Port Status Request info */

typedef struct portstat {
    __u16 portChange;
    __u16 portStatus;
} portstat_t;

typedef struct hw_td {
    __u16 ly_base_addr;             /* location of the data buffer in CY */
    __u16 port_length;
    __u8 pid_ep;
    __u8 dev_addr;
    __u8 ctrl_reg;
    __u8 status;
    __u8 retryCnt;
    __u8 residue;
    __u16 next_td_Addr;
} hw_td_t;

#if 0
typedef struct td {
    __u16 ly_base_addr;             /* location of the data buffer in CY */
    __u16 port_length;
    __u8 pid_ep;
    __u8 dev_addr;
    __u8 ctrl_reg;
    __u8 status;
    __u8 retry_cnt;
    __u8 residue;
    __u16 next_td_addr;
    char * sa_data_ptr;        /* the data that reside in the external host */
    int last_td_flag;          /* stage for this td */
    struct list_head td_list;       /* td list */
    struct urb * urb;
} td_t;
#endif  /* Srikanth */

typedef struct td {
    __u16 ly_base_addr;             /* location of the data buffer in CY */
    __u16 port_length;
    __u8 dev_addr;
    __u8 pid_ep;
    __u8 status;
    __u8 ctrl_reg;
    __u8 residue;
    __u8 retry_cnt;
    __u16 next_td_addr;
    char * sa_data_ptr;        /* the data that reside in the external host */
    int last_td_flag;          /* stage for this td */
    struct list_head td_list;       /* td list */
    struct urb * urb;
} td_t;



typedef struct sie {
    __u16 tdBaseAddr;   /* SIE TD list base address */
    __u16 tdLen;        /* SIE TD list length */
    __u16 bufBaseAddr;  /* SIE buffer base address */
    __u16 bufLen;       /* SIE buffer length */
    __u16 last_td_addr; /* the last TD addr in cy7c67200/300 */
    __u16 last_buf_addr; /* the last buffer addr in cy7c67200/300 */
    int bandwidth_allocated;  /* USB bandwidth allocated for this SIE */
    int periodic_bw_allocated; /* USB bandwidth allocated for ISOC/Interrupt transfer */
    int td_active;
    struct list_head td_list_head;     /* head pointer to td_list */
    struct list_head done_list_head;   /* head pointer to a done_list */
} sie_t;

typedef struct virt_root_hub {
    int devnum;         /* Address of Root Hub endpoint */
    void * urb;         /* interrupt URB of root hub */
    int send;           /* active flag */
    int interval;       /* intervall of roothub interrupt transfers */
    struct timer_list rh_int_timer; /* intervall timer for rh interrupt EP */
}virt_root_hub_t;

typedef struct hcipriv {
    virt_root_hub_t rh;        /* roothub for this port */
    int host_disabled;      /* 0 = host port, 1 = otg or peripheral port*/
    atomic_t resume_count;      /* defending against multiple resumes */

    struct usb_bus * bus;           /* our bus */
    struct portstat * RHportStatus; /* root hub port status */

    int frame_number;

    struct list_head ctrl_list;     /* set of ctrl endpoints */
    struct list_head bulk_list;     /* set of bulk endpoints */
    struct list_head iso_list;      /* set of isoc endpoints */
    struct list_head intr_list;     /* set of int endpoints */
    struct list_head del_list;      /* set of entpoints to be deleted */
    struct list_head frame_urb_list;/* list of all urb in current frame */
    otg_t otg;

    int active_urbs;        /* A number of active urbs */
    int xferPktLen;
    int units_left;

} hcipriv_t;
struct hci;

extern int urb_debug;

extern int cy67x00_get_current_frame_number (cy_priv_t * cy_priv, int port_num);
extern int cy67x00_get_next_frame_number (cy_priv_t * cy_priv, int port_num);
extern void hcd_irq_sofeop1(cy_priv_t *cy_priv);
extern void hcd_irq_sofeop2(cy_priv_t *cy_priv);
extern void hcd_irq_mb_in(cy_priv_t *cy_priv);
extern void hcd_irq_mb_out(unsigned int message, int sie, cy_priv_t *cy_priv);
extern void hcd_irq_resume1(cy_priv_t *cy_priv);
extern void hcd_irq_resume2(cy_priv_t *cy_priv);
extern void hcd_irq_enable_host_periph_ints(cy_priv_t *cy_priv);
void hcd_switch_role(int new_role, cy_priv_t * cy_priv);

#define HCD_PID_SETUP   0x2d   /* USB Specification 1.1 Standard Definition */
#define HCD_PID_SOF     0xA5
#define HCD_PID_IN      0x69
#define HCD_PID_OUT     0xe1

#define TD_PID_SETUP    0xD0
#define TD_PID_SOF      0x50
#define TD_PID_IN       0x90
#define TD_PID_OUT      0x10


#define TIMEOUT     50    /* 2 mseconds */


