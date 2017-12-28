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

#ifndef CY7C67200_300_PCD_H
#define CY7C67200_300_PCD_H


/* Structure used to communicate with BIOS for send and receive. */

struct TRANSFER_FRAME_STRUCT
{
    unsigned short link_pointer;
    unsigned short absolute_address;
    unsigned short data_length;
    unsigned short callback_function_location;
} __attribute__((packed));

typedef struct TRANSFER_FRAME_STRUCT TRANSFER_FRAME;


/* 
 * EZ-HOST/OTG memory allocation.  This is shared with hcd, so be 
 * sure cy_priv->cy_buf_addr is updated.
 */

#define UDC_MEMORY_BASE                    (cy_priv->cy_buf_addr)
#define DEVICE_DESCRIPTOR_SIZE             0x12
#define CONFIG_DESCRIPTOR_SIZE             0x100
#define STRING_DESCRIPTOR_SIZE             0x100
#define TXRX_STRUCT_SIZE                   sizeof(TRANSFER_FRAME)
#define RECV_BUFF_LENGTH                   64
#define SEND_BUFF_LENGTH                   (1024 + 2)
#define MEM_PADDING                        32

#define SIE1_BASE_MEMORY                   UDC_MEMORY_BASE

#define SIE1_DEVICE_DESCRIPTOR_LOCATION    SIE1_BASE_MEMORY
#define SIE1_CONFIG_DESCRIPTOR_LOCATION    \
            (SIE1_DEVICE_DESCRIPTOR_LOCATION + DEVICE_DESCRIPTOR_SIZE)
#define SIE1_STRING_DESCRIPTOR_LOCATION    \
            (SIE1_CONFIG_DESCRIPTOR_LOCATION + CONFIG_DESCRIPTOR_SIZE)

#define SIE1_RECV_STRUCT_ADDR              \
            (SIE1_STRING_DESCRIPTOR_LOCATION + STRING_DESCRIPTOR_SIZE)
#define SIE1_RECV_BUFF_ADDR                \
            (SIE1_RECV_STRUCT_ADDR + 8*TXRX_STRUCT_SIZE)

#define SIE1_SEND_STRUCT_ADDR              \
            (SIE1_RECV_BUFF_ADDR + 8*RECV_BUFF_LENGTH)
#define SIE1_SEND_BUFF_ADDR                \
            (SIE1_SEND_STRUCT_ADDR + TXRX_STRUCT_SIZE)
#define SIE1_MEM_END                       \
            (SIE1_SEND_BUFF_ADDR + SEND_BUFF_LENGTH + MEM_PADDING)


#define SIE2_BASE_MEMORY                   UDC_MEMORY_BASE

#define SIE2_DEVICE_DESCRIPTOR_LOCATION    SIE2_BASE_MEMORY
#define SIE2_CONFIG_DESCRIPTOR_LOCATION    \
            (SIE2_DEVICE_DESCRIPTOR_LOCATION + DEVICE_DESCRIPTOR_SIZE)
#define SIE2_STRING_DESCRIPTOR_LOCATION    \
            (SIE2_CONFIG_DESCRIPTOR_LOCATION + CONFIG_DESCRIPTOR_SIZE)

#define SIE2_RECV_STRUCT_ADDR              \
            (SIE2_STRING_DESCRIPTOR_LOCATION + STRING_DESCRIPTOR_SIZE)
#define SIE2_RECV_BUFF_ADDR                \
            (SIE2_RECV_STRUCT_ADDR + 8*TXRX_STRUCT_SIZE)

#define SIE2_SEND_STRUCT_ADDR              \
            (SIE2_RECV_BUFF_ADDR + 8*RECV_BUFF_LENGTH)
#define SIE2_SEND_BUFF_ADDR                \
            (SIE2_SEND_STRUCT_ADDR + TXRX_STRUCT_SIZE)
#define SIE2_MEM_END                       \
            (SIE2_SEND_BUFF_ADDR + SEND_BUFF_LENGTH + MEM_PADDING)


#define DESCRIPTOR_BUFFER_SIZE  0x100
#define EP0_PACKETSIZE          8          
#define UDC_MAX_ENDPOINTS       8          
#define UDC_IRQ                 SL16_IRQ
#define UDC_ADDR                SL16_ADDR 
#define UDC_ADDR_SIZE           4          
#define UDC_NAME                "EZ-HOST"

#define ERROR                   -1
#define SUCCESS                 0

#define CONTROL_EP_RECV_URB_COUNT           5
#define OUT_EP_URB_COUNT                    5


typedef struct sie_info 
{
    unsigned short            susb_recv_interrupt;
    unsigned short            susb_stall_interrupt;
    unsigned short            susb_standard_int;
    unsigned short            susb_standard_loader_vec;
    unsigned short            susb_vendor_interrupt;
    unsigned short            susb_vendor_loader_vec;
    unsigned short            susb_class_interrupt;
    unsigned short            susb_class_loader_vec; 
    unsigned short            susb_finish_interrupt;
    unsigned short            susb_dev_desc_vec;
    unsigned short            susb_config_desc_vec;
    unsigned short            susb_string_desc_vec;
    unsigned short            susb_parse_config_interrupt;
    unsigned short            susb_loader_interrupt;
    unsigned short            susb_delta_config_interrupt;
    
    unsigned short            susb_init_interrupt;
    unsigned short            susb_remote_wakeup_interrupt;
    unsigned short            susb_start_srp_interrupt;

    unsigned short            sie_dev_desc_location;
    unsigned short            sie_config_desc_location;
    unsigned short            sie_string_desc_location;

    unsigned short            recv_struct_location;
    unsigned short            recv_buffer_location;
    unsigned short            recv_buffer_length;

    unsigned short            send_struct_location;
    unsigned short            send_buffer_location;
    unsigned short            send_buffer_length;
    
    struct usb_bus_instance *       bus;
    void *                          cy_priv;
    
    unsigned char                   usb_address;
    int                             udc_suspended;
    int                             sie_number;
    struct usb_endpoint_instance    *rcv_data_pend_ep[UDC_MAX_ENDPOINTS];

} sie_info;


extern void udc_rcv_urb_recycled(void);
extern int  pcd_init(int sie_num, int de_num, cy_priv_t * cy_priv);
extern int  pcd_switch_role(int role, cy_priv_t *cy_priv);

extern void pcd_irq_id(cy_priv_t *cy_priv);
extern void pcd_irq_vbus(cy_priv_t *cy_priv);
extern void pcd_irq_rst2(cy_priv_t *cy_priv);
extern void pcd_irq_rst1(cy_priv_t *cy_priv);
extern void pcd_irq_sofeop2(cy_priv_t *cy_priv);
extern void pcd_irq_sofeop1(cy_priv_t *cy_priv);
extern void pcd_irq_mb_in(cy_priv_t *cy_priv);
extern void pcd_irq_mb_out(unsigned int message, int sie, cy_priv_t * cy_priv);

#endif /* CY7C67200_300_PCD_H */
