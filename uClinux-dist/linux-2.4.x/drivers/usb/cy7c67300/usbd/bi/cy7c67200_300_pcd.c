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

#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-export.h"
#include "../usbd-build.h"
/* #include "../usbd-module.h" */

MODULE_AUTHOR("Cypress Semiconductor");
MODULE_DESCRIPTION("CY7C67200_300 Peripheral Bus Interface");

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/timer.h>

#include <asm/atomic.h>
#include <asm/io.h>

#include <asm/irq.h>
#include <asm/system.h>

#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>

#include "../usbd.h"
extern int __init usbd_device_init(void);

#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"
extern int __init bi_modinit(void);

#include "../../cy7c67200_300.h"
#include "../../lcp_cmd.h"
#include "../../cy7c67200_300_common.h"
#include "../../cy7c67200_300_lcd.h"
#include "../../cy7c67200_300_otg.h"
#include "../../cy7c67200_300_debug.h"
#include "cy7c67200_300_pcd.h"

extern void de1_modinit(void);
extern void de3_modinit(void);
extern void de4_modinit(void);

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* Local functions */
static void ep_receive_data(struct usb_endpoint_instance *, int, cy_priv_t *);
static int  load_configuration_data(cy_priv_t *);

static int  pcd_dbg_on = 0;

#define pcd_dbg(format, arg...) \
    if( pcd_dbg_on != 0 ) \
        printk(KERN_DEBUG __FILE__ ":"  "%d: " format "\n" ,__LINE__, ## arg)


static struct timer_list vbus_timer;

static sie_info *sie;

static sie_info sie_init_data[] =
{
    {
        susb_recv_interrupt:          SUSB1_RECEIVE_INT,
        susb_stall_interrupt:         SUSB1_STALL_INT,
        susb_standard_int:            SUSB1_STANDARD_INT,
        susb_standard_loader_vec:     SUSB1_STANDARD_LOADER_VEC,
        susb_vendor_interrupt:        SUSB1_VENDOR_INT,
        susb_vendor_loader_vec:       SUSB1_VENDOR_LOADER_VEC,
        susb_class_interrupt:         SUSB1_CLASS_INT,
        susb_class_loader_vec:        SUSB1_CLASS_LOADER_VEC,
        susb_finish_interrupt:        SUSB1_FINISH_INT,
        susb_dev_desc_vec:            SUSB1_DEV_DESC_VEC,
        susb_config_desc_vec:         SUSB1_CONFIG_DESC_VEC,
        susb_string_desc_vec:         SUSB1_STRING_DESC_VEC,
        susb_parse_config_interrupt:  SUSB1_PARSE_CONFIG_INT,
        susb_loader_interrupt:        SUSB1_LOADER_INT,
        susb_delta_config_interrupt:  SUSB1_DELTA_CONFIG_INT,
        susb_init_interrupt:          SUSB_INIT_INT,
        susb_remote_wakeup_interrupt: REMOTE_WAKEUP_INT,
        susb_start_srp_interrupt:     START_SRP_INT,
        recv_buffer_length:           RECV_BUFF_LENGTH,
        send_buffer_length:           SEND_BUFF_LENGTH,
        bus:                          0,
        cy_priv:                      0,
        usb_address:                  0,
        udc_suspended:                TRUE,
        sie_number:                   SIE1
    },

    {
        susb_recv_interrupt:          SUSB2_RECEIVE_INT,
        susb_stall_interrupt:         SUSB2_STALL_INT,
        susb_standard_int:            SUSB2_STANDARD_INT,
        susb_standard_loader_vec:     SUSB2_STANDARD_LOADER_VEC,
        susb_vendor_interrupt:        SUSB2_VENDOR_INT,
        susb_vendor_loader_vec:       SUSB2_VENDOR_LOADER_VEC,
        susb_class_interrupt:         SUSB2_CLASS_INT,
        susb_class_loader_vec:        SUSB2_CLASS_LOADER_VEC,
        susb_finish_interrupt:        SUSB2_FINISH_INT,
        susb_dev_desc_vec:            SUSB2_DEV_DESC_VEC,
        susb_config_desc_vec:         SUSB2_CONFIG_DESC_VEC,
        susb_string_desc_vec:         SUSB2_STRING_DESC_VEC,
        susb_parse_config_interrupt:  SUSB2_PARSE_CONFIG_INT,
        susb_loader_interrupt:        SUSB2_LOADER_INT,
        susb_delta_config_interrupt:  SUSB2_DELTA_CONFIG_INT,
        susb_init_interrupt:          SUSB_INIT_INT,
        susb_remote_wakeup_interrupt: REMOTE_WAKEUP_INT,
        susb_start_srp_interrupt:     START_SRP_INT,
        recv_buffer_length:           RECV_BUFF_LENGTH,
        send_buffer_length:           SEND_BUFF_LENGTH,
        bus:                          0,
        cy_priv:                      0,
        usb_address:                  0,
        udc_suspended:                TRUE,
        sie_number:                   SIE2
    }
};


/******************************************************************************
 * FUNCTION: pcd_init
 *
 * DESCRIPTION: Records boot time configuration of desired sie and design
 *              example to be used later.
 */

int pcd_init(int sie_num, int de_num, cy_priv_t *cy_priv)
{
    size_t i;
    unsigned short mem_start = cy_priv->cy_buf_addr;

    pcd_dbg("pcd_init enter");

    /* Check for valid SIE */
    if( sie_num >= sizeof(sie_init_data)/sizeof(sie_info) )
    {
        return ERROR;
    }

    /* Initialize some SIE members */
    sie = &sie_init_data[sie_num];

    sie->cy_priv = cy_priv;
    cy_priv->pcdi = sie;
    
    if( sie_num == SIE1 )
    {
        sie->sie_dev_desc_location    = SIE1_DEVICE_DESCRIPTOR_LOCATION;
        sie->sie_config_desc_location = SIE1_CONFIG_DESCRIPTOR_LOCATION;
        sie->sie_string_desc_location = SIE1_STRING_DESCRIPTOR_LOCATION;
        sie->recv_struct_location     = SIE1_RECV_STRUCT_ADDR;
        sie->recv_buffer_location     = SIE1_RECV_BUFF_ADDR;
        sie->send_struct_location     = SIE1_SEND_STRUCT_ADDR;
        sie->send_buffer_location     = SIE1_SEND_BUFF_ADDR;
        cy_priv->cy_buf_addr          = SIE1_MEM_END;
    }
    else
    {
        sie->sie_dev_desc_location    = SIE2_DEVICE_DESCRIPTOR_LOCATION;
        sie->sie_config_desc_location = SIE2_CONFIG_DESCRIPTOR_LOCATION;
        sie->sie_string_desc_location = SIE2_STRING_DESCRIPTOR_LOCATION;
        sie->recv_struct_location     = SIE2_RECV_STRUCT_ADDR;
        sie->recv_buffer_location     = SIE2_RECV_BUFF_ADDR;
        sie->send_struct_location     = SIE2_SEND_STRUCT_ADDR;
        sie->send_buffer_location     = SIE2_SEND_BUFF_ADDR;
        cy_priv->cy_buf_addr          = SIE2_MEM_END;
    }

    pcd_dbg("PCD memory usage: start:0x%04x, end+1:0x%04x, size:0x%04x", 
            mem_start, cy_priv->cy_buf_addr, cy_priv->cy_buf_addr - mem_start);
        
    for(i=0; i<UDC_MAX_ENDPOINTS; i++)
    {
        sie->rcv_data_pend_ep[i] = 0;
    }

    /* Initialize device core */
    if (usbd_device_init()) 
    {
        cy_err("usbd_device_init failed");
        sie = 0;
        return ERROR;
    }

    /* setup function driver based upon design example */
    switch( de_num )
    {
    case DE1:
        {
        lcd_int_data_t int_data;

        pcd_dbg("Initializing peripheral function driver for DE1");

        de1_modinit();
        
        /* Initialize BIOS for debug port */
        int_data.int_num = SUSB_INIT_INT;
        int_data.reg[1]  = FULL_SPEED;
        int_data.reg[2]  = 2;   /* SIE2 */
        
        pcd_dbg("Initializing debug port");
        lcd_exec_interrupt(&int_data, 0, 0, cy_priv);
        }
        break;

    case DE3:
        pcd_dbg("Initializing peripheral function driver for DE3");
        de3_modinit();
        break;

    case DE4:
        pcd_dbg("Initializing peripheral function driver for DE4");
        de4_modinit();
        break;

    default:
        cy_err("unhandled design example %d", de_num);
        return ERROR;
        break;
    }

    /* Initialize bus interface */
    if( bi_modinit() ) 
    {
        cy_err("bi_modinit failed");
        sie = 0;
        return ERROR;
    }

    init_timer(&vbus_timer);

    return SUCCESS;
}


/*****************************************************************
 *
 * Function Name: pcd_switch_role
 *
 * Description: This function activates/deactivates the peripheral
 *              code when doing a role switch.
 *
 * Input: role - the role to switch to.
 *
 * Return: none 
 *                
 *****************************************************************/

int pcd_switch_role(int role, cy_priv_t *cy_priv)
{
    unsigned short intStat;
    lcd_int_data_t int_data;
    int i;

    pcd_dbg("pcd_switch_role enter");

    if( cy_priv == 0 || sie == 0 )
    {
        return ERROR;
    }

    switch(role)
    {
    case PERIPHERAL_ROLE:

        pcd_dbg("pcd role switch to PERIPHERAL_ROLE");

        /* Setup interrupt steering */
        lcd_read_reg(HPI_IRQ_ROUTING_REG, &intStat, cy_priv);
        intStat &= ~SOFEOP1_TO_HPI_EN;
        intStat &= ~SOFEOP2_TO_HPI_EN;
        intStat |=  SOFEOP1_TO_CPU_EN;
        intStat |=  SOFEOP2_TO_CPU_EN;
        lcd_write_reg(HPI_IRQ_ROUTING_REG, intStat, cy_priv);

        /* Save descriptors to BIOS */
        if( load_configuration_data(cy_priv) == ERROR )
        {
            cy_err("load_configuration_data failed");
            return ERROR;
        }

        /* Initialize BIOS for slave */
        int_data.int_num = SUSB_INIT_INT;
        int_data.reg[1] = FULL_SPEED;
        int_data.reg[2] = sie->sie_number + 1;
        
        if( lcd_exec_interrupt(&int_data, 0, 0, cy_priv) == ERROR )
        {
            cy_err("lcd_exec_interrupt failed");
            return ERROR;
        }

        for(i=0; i<10; i++)
        {
            lcd_read_reg(USB1_CTL_REG, &intStat, cy_priv);

            if( (intStat & (A_DP_STAT | A_DM_STAT)) ) break;

            mdelay(1);
        }

        /* Turn on VBUS and ID interrupts */
        lcd_read_reg (HOST1_IRQ_EN_REG, &intStat, cy_priv);
        intStat |= VBUS_IRQ_EN;
        intStat |= ID_IRQ_EN;
        intStat |= SOF_EOP_TMOUT_IRQ_EN; /* MSE */
        lcd_write_reg(HOST1_IRQ_EN_REG, intStat, cy_priv);
        break;

    case HOST_ROLE:
        pcd_dbg("pcd role switch to HOST_ROLE");
        usbd_device_event_irq(sie->bus->device, DEVICE_BUS_INACTIVE, 0);   
        usbd_device_event(sie->bus->device, DEVICE_RESET, 0);
        break;

    case HOST_INACTIVE_ROLE:
        pcd_dbg("pcd role switch to HOST_INACTIVE_ROLE");
        usbd_device_event_irq(sie->bus->device, DEVICE_BUS_INACTIVE, 0);   
        usbd_device_event(sie->bus->device, DEVICE_RESET, 0);
        
        /* Setup interrupt steering */
        lcd_read_reg(HPI_IRQ_ROUTING_REG, &intStat, cy_priv);
        intStat &= ~SOFEOP1_TO_HPI_EN;
        intStat &= ~SOFEOP2_TO_HPI_EN;
        intStat |=  SOFEOP1_TO_CPU_EN;
        intStat |=  SOFEOP2_TO_CPU_EN;
        lcd_write_reg(HPI_IRQ_ROUTING_REG, intStat, cy_priv);
        
        /* Turn on VBUS and ID interrupts */
        lcd_read_reg (HOST1_IRQ_EN_REG, &intStat, cy_priv);
        intStat |= VBUS_IRQ_EN;
        intStat |= ID_IRQ_EN;
        intStat |= 0x800; /* MSE, IROS pg 103 */
        intStat &= ~A_CHG_IRQ_EN;
        intStat &= ~B_CHG_IRQ_EN;
        lcd_write_reg(HOST1_IRQ_EN_REG, intStat, cy_priv);

         /* 
          * for initialization when we don't get an IRQ for 
          * connection at reset 
          */
         pcd_irq_vbus(cy_priv);  
         break;
    }

    return SUCCESS;
}

/*****************************************************************
 *
 * Function Name: pcd_signal_srp
 *
 * Description: This function performs a session request protocol.
 *
 *****************************************************************/

int pcd_signal_srp(size_t vbus_pulse_time, cy_priv_t *cy_priv)
{
    unsigned short reg_val;

    /* Check SE0 state for 2ms */

    lcd_read_reg(USB1_CTL_REG, &reg_val, cy_priv);
    
    if( reg_val & (A_DP_STAT | A_DM_STAT) )
    {
        cy_err("Not SE0 state");
        /* Not SE0 state */
        return ERROR;
    }
    
    mdelay(2);

    lcd_read_reg(USB1_CTL_REG, &reg_val, cy_priv);
    
    if( reg_val & (A_DP_STAT | A_DM_STAT) )
    {
        cy_err("Not SE0 state");
        /* Not SE0 state */
        return ERROR;
    }

    
    /* check session ended */

    lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);

    if( reg_val & VBUS_VALID_FLG )
    {
        cy_err("VBUS high 1");
        /* A-Device still driving VBUS.*/
        return ERROR;
    }
    else
    {
        /* Pulse VBUS discharge resistor */
        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        lcd_write_reg(OTG_CTL_REG, reg_val | VBUS_DISCH_EN, cy_priv);
        mdelay(30);
        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        lcd_write_reg(OTG_CTL_REG, reg_val & ~VBUS_DISCH_EN, cy_priv);

        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        if( reg_val & OTG_DATA_STAT )
        {
            cy_err("VBUS high 2");
            /* Error: The USB host may be driving the VBUS */
            return ERROR;
        }

        /* Pulse D+ data line */
        /* pcd_dbg("START D+ PULLUP"); */
        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        reg_val |= DPLUS_PULLUP_EN;
        lcd_write_reg(OTG_CTL_REG, reg_val, cy_priv);
        mdelay(8);
        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        reg_val &= ~DPLUS_PULLUP_EN;
        lcd_write_reg(OTG_CTL_REG, reg_val, cy_priv);
        /* pcd_dbg("END D+ PULLUP"); */

        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        if( reg_val & VBUS_VALID_FLG )
        {
            pcd_dbg("VBUS high 3");
            /* A-Device has raised VBUS */
            return SUCCESS;
        }

        /* Pulse VBUS */
        /* pcd_dbg("START VBUS PULLUP"); */
        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        lcd_write_reg(OTG_CTL_REG, reg_val | VBUS_PULLUP_EN, cy_priv);
        mdelay(vbus_pulse_time);
        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        lcd_write_reg(OTG_CTL_REG, reg_val & ~VBUS_PULLUP_EN, cy_priv);
        /* pcd_dbg("END VBUS PULLUP"); */

        /* Discharge VBUS again to prevent false transitions to b_peripheral */
        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        lcd_write_reg(OTG_CTL_REG, reg_val | VBUS_DISCH_EN, cy_priv);
        mdelay(30);
        lcd_read_reg(OTG_CTL_REG, &reg_val, cy_priv);
        lcd_write_reg(OTG_CTL_REG, reg_val & ~VBUS_DISCH_EN, cy_priv);
    }


    return SUCCESS;
}


/*****************************************************************
 *
 *  Function Name: load_configuration_data
 *
 *  DESCRIPTION:  This function will obtain descriptors from the function 
 *  drivers attached above it, and will then load them to the EZ-HOST 
 *  memory.
 *
 *  A device will contain a single device descriptor.  Device, configuration, 
 *  interface, and endpoint descriptors are written to EZ-HOST RAM during 
 *  initialization.  The BIOS then will handle any standard requests made 
 *  by a host to the control endpoint.  
 *  
 *****************************************************************/

int load_configuration_data(cy_priv_t * cy_priv)
{
    int port = 0;
    int bNumConfigurations;
    int bNumInterfaces;
    int bLength = 0;
    unsigned short wTotalLength = 0;
    unsigned short length_remaining = 0;
    char * descriptor_buffer;
    char * buffer_position;

    size_t index = 0;
    struct usb_string_descriptor *string;
    unsigned short string_loc;
    int num_char;

    struct usb_device_instance * dev;
    
    int class;
    int bNumEndpoint;
    struct usb_device_descriptor        * device_descriptor;
    struct usb_device_descriptor        device_descriptor_le;
    struct usb_configuration_descriptor * configuration_descriptor;    
    struct usb_interface_descriptor * interface_descriptor;
    struct usb_alternate_instance * alternate_instance;        
    struct usb_otg_descriptor * otg_descriptor;    
    
    pcd_dbg("load_configuration_data enter");

    dev = sie->bus->device;
    
    /* load the descriptors to the ASIC to handle enumeration. 
       load the device descriptor */
    if(!(device_descriptor = usbd_device_device_descriptor(dev, 
                                                           port))) {
        cy_err("usbd_device_device_descriptor() failed, dev:%p, port:%d",
                dev, port);
        return(ERROR);
    }

    /* BJC - Endian conversion before write to ASIC */
    memcpy(&device_descriptor_le, device_descriptor, device_descriptor->bLength);
    device_descriptor_le.idVendor = cpu_to_le16(device_descriptor_le.idVendor);
    device_descriptor_le.idProduct = cpu_to_le16(device_descriptor_le.idProduct);
    device_descriptor_le.bcdDevice = cpu_to_le16(device_descriptor_le.bcdDevice);
    device_descriptor = (struct usb_device_descriptor *)&device_descriptor_le;

    if(lcd_write_memory(sie->sie_dev_desc_location,
                        sizeof(*device_descriptor), 
                        (char *)device_descriptor,
                        cy_priv) == ERROR) {
        cy_err("lcd_write_memory() failed");
        return(ERROR);
    }

    /* write the vector address to the software interrupt. */
    if(lcd_write_reg(sie->susb_dev_desc_vec,
                     sie->sie_dev_desc_location,
                     cy_priv) == ERROR) {
        cy_err("lcd_write_reg() failed");
        return(ERROR);
    }

    /* create a buffer and write all descriptor information to the buffer. */
    descriptor_buffer = (char *)kmalloc(DESCRIPTOR_BUFFER_SIZE, GFP_ATOMIC);
    
    /* determine the number of configurations. */
    for(bNumConfigurations = 0; bNumConfigurations < 
        device_descriptor->bNumConfigurations; bNumConfigurations++) {
        configuration_descriptor = usbd_device_configuration_descriptor(
                                                        dev, 
                                                        port, 
                                                        bNumConfigurations);
            
        /* get config descriptor length */
        bLength = configuration_descriptor->bLength;
    
        /* get the total length of all descriptors for this configuration */
        wTotalLength = configuration_descriptor->wTotalLength;
        length_remaining = wTotalLength - bLength;

        memcpy(descriptor_buffer, 
               configuration_descriptor, 
               bLength);
               
        buffer_position = descriptor_buffer + bLength;
    
        /* iterate across interfaces for specified configuration */
        for(bNumInterfaces = 0; bNumInterfaces < 
            configuration_descriptor->bNumInterfaces ; bNumInterfaces++) {
            
            int bAlternateSetting;
            struct usb_interface_instance * interface_instance;

            if(!(interface_instance = usbd_device_interface_instance(dev, 
                port, 0, bNumInterfaces))) {
                cy_err("usbd_device_interface_instance() failed");
                return(ERROR);
            }
            
            /* iterate across interface alternates */
            for(bAlternateSetting = 0; bAlternateSetting < 
                interface_instance->alternates; bAlternateSetting++) {
                
                if (!(alternate_instance = usbd_device_alternate_instance(
                    dev, port, 0, bNumInterfaces, bAlternateSetting))) {
                    cy_err("usbd_device_alternate_instance() failed");
                    return(ERROR);
                }
                
                length_remaining = length_remaining - 
                    alternate_instance->interface_descriptor->bLength;

                /* copy interface descriptor to buffer */
                memcpy(buffer_position, 
                       alternate_instance->interface_descriptor, 
                       alternate_instance->interface_descriptor->bLength);

                buffer_position = buffer_position + 
                                  alternate_instance->
                                  interface_descriptor->
                                  bLength;
            
                /* iterate across classes for this alternate interface */
                for (class = 0; class < alternate_instance->classes ; class++) {
                
                    struct usb_class_descriptor * class_descriptor;

                    if (!(class_descriptor = 
                        usbd_device_class_descriptor_index(dev, port, 0, 
                            bNumInterfaces, bAlternateSetting, class))) {
                        cy_err("usbd_device_class_descriptor_index() failed");
                        return(ERROR);
                    }
                    
                    length_remaining = length_remaining - 
                        class_descriptor->descriptor.generic.bFunctionLength;

                    /* copy descriptor for this class */
                    memcpy(buffer_position, class_descriptor, 
                        class_descriptor->descriptor.generic.bFunctionLength);

                    buffer_position = buffer_position + 
                        class_descriptor->descriptor.generic.bFunctionLength;                    
                }
    
                /* iterate across endpoints for this alternate interface */
                interface_descriptor = alternate_instance->interface_descriptor;
                for (bNumEndpoint = 0; 
                     bNumEndpoint < alternate_instance->endpoints; 
                     bNumEndpoint++) {
                     
                    struct usb_endpoint_descriptor * endpoint_descriptor;
            
                    if (!(endpoint_descriptor = 
                        usbd_device_endpoint_descriptor_index(dev, 
                                                              port, 
                                                              0, 
                                                              bNumInterfaces, 
                                                              bAlternateSetting, 
                                                              bNumEndpoint))) {
                        cy_err("usbd_device_endpoint_descriptor_index()"
                               " failed");
                        return(ERROR);
                    }
                    
                    length_remaining = length_remaining - 
                        endpoint_descriptor->bLength;
                
                    /* copy descriptor for this endpoint */
                    memcpy(buffer_position, 
                           endpoint_descriptor, 
                           endpoint_descriptor->bLength);
                   
                    /* BJC - Byte Order conversion.  Not done in the nicest way */
		    ((struct usb_endpoint_descriptor *) buffer_position)->wMaxPacketSize
			= cpu_to_le16(((struct usb_endpoint_descriptor *) 
					buffer_position)->wMaxPacketSize);

                    buffer_position = buffer_position + 
                        endpoint_descriptor->bLength;
                }

                /* get the otg descriptor if it is available, the 
                 * length_remaining if the otg_descriptor is present should 
                 * be three, otherwise the otg_descriptor will not be present 
                 * for this function 
                 */
                   
                if(alternate_instance->otg_descriptor != NULL) {
                    
                    otg_descriptor = usbd_device_otg_descriptor(dev, port, 
                                bNumConfigurations, bNumInterfaces,
                                bAlternateSetting);
                    
                    memcpy(buffer_position,
                           otg_descriptor,
                           otg_descriptor->bLength);
                    
                    buffer_position = buffer_position + otg_descriptor->bLength;
                }
            }
        }
    }

    /* write the config descriptor to EZ-HOST */
    if (lcd_write_memory(sie->sie_config_desc_location,
                         wTotalLength,
                         descriptor_buffer,
                         cy_priv) == ERROR) 
    {
        kfree(descriptor_buffer);
        cy_err("lcd_write_memory() failed for config descriptors");
        return(ERROR);
    }

    /* write the config descriptor address to the software interrupt. */
    if (lcd_write_reg(sie->susb_config_desc_vec,
                      sie->sie_config_desc_location,
                      cy_priv) == ERROR) 
    {
        kfree(descriptor_buffer);
        cy_err("lcd_write_reg() failed");
        return(ERROR);
    }
    
    /* Enable HNP in BIOS */
    if (lcd_write_reg(0xb0, 3, cy_priv) == ERROR) 
    {
        kfree(descriptor_buffer);
        cy_err("lcd_write_reg() failed");
        return(ERROR);
    }
    
    /* write string descriptors to EZ-HOST */
    string_loc = sie->sie_string_desc_location;

    while( (string = usbd_get_string(index++)) != NULL )
    {
        /* BJC - Convert the string endinaness 
         * Note conversion is on the actual descriptor structure
         */
	for (num_char = 0; num_char < string->bLength; num_char++) {
            string->wData[num_char] = cpu_to_le16(string->wData[num_char]);  
        }

        if (lcd_write_memory(string_loc,
                             string->bLength,
                             (char*)string,
                             cy_priv) == ERROR) {
            kfree(descriptor_buffer);
            cy_err("lcd_write_memory() failed for string descriptors");
            return(ERROR);
        }

        string_loc += string->bLength;
    }

    pcd_dbg("Device descriptor address: 0x%04x", sie->sie_dev_desc_location);
    pcd_dbg("Config descriptor address: 0x%04x", sie->sie_config_desc_location);
    pcd_dbg("String descriptor address: 0x%04x", sie->sie_string_desc_location);

    /* write the string descriptor address to the software interrupt. */
    if (lcd_write_reg(sie->susb_string_desc_vec,
                      sie->sie_string_desc_location,
                      cy_priv) == ERROR) 
    {
        kfree(descriptor_buffer);
        cy_err("lcd_write_reg() failed for string descriptors");
        return(ERROR);
    }
    
    kfree(descriptor_buffer);

    return(SUCCESS);
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
 * Return: none 
 *                
 *****************************************************************/

void pcd_irq_sofeop1(cy_priv_t *cy_priv)
{
    /* not used */
}

/*****************************************************************/

void pcd_irq_sofeop2(cy_priv_t *cy_priv)
{
    /* not used */
}

/*****************************************************************/

void pcd_irq_rst1(cy_priv_t *cy_priv)
{
    sie_info *sie_var = (sie_info *)cy_priv->pcdi;

    pcd_dbg("pcd_irq_rst1 enter");

    /* not initialized yet, ignore interrupt */
    if(sie_var == NULL)
    {
        return;
    }

    if( sie_var->sie_number == SIE1 )
    {
        sie_var->usb_address = 0;
        usbd_device_event(sie_var->bus->device, 
                          DEVICE_RESET, 
                          0);
    }

    /* clear the associated interrupt */
    lcd_write_reg(HOST1_STAT_REG, RST_IRQ_FLG, cy_priv);
}

/*****************************************************************/

void pcd_irq_rst2(cy_priv_t *cy_priv)
{
    sie_info *sie_var = (sie_info *)cy_priv->pcdi;

    pcd_dbg("pcd_irq_rst2 enter");

    /* not initialized yet, ignore interrupt */
    if(sie_var == NULL)
    {
        return;
    }

    if( sie_var->sie_number == SIE2 )
    {
        sie_var->usb_address = 0;
        usbd_device_event(sie_var->bus->device, 
                          DEVICE_RESET, 
                          0);
    }

    /* clear the associated interrupt */
    lcd_write_reg(HOST2_STAT_REG, RST_IRQ_FLG, cy_priv);
}

/*****************************************************************/
/* OTG 4.4V Peripheral Mode only */

void vbus_timeout(unsigned long data)
{
    cy_priv_t *cy_priv = (cy_priv_t*)data;
    unsigned short value;
    otg_t *otg = cy_priv->otg;
    boolean prev_a_vbus_vld;
    boolean prev_a_sess_vld;

    lcd_read_reg(OTG_CTL_REG, &value, cy_priv);

    prev_a_vbus_vld = otg->a_vbus_vld;
    prev_a_sess_vld = otg->a_sess_vld;

    if (value & VBUS_VALID_FLG)
    {
        pcd_dbg("pcd_irq_vbus enter, VBUS_VLD, otg_reg:0x%04x", value);
        otg->a_vbus_vld = TRUE;
        /* HPI can not distinguish between the two thresholds. */
        otg->a_sess_vld = otg->b_sess_vld = TRUE;
        otg->b_sess_end = FALSE;
    }
    else
    { 
        pcd_dbg("pcd_irq_vbus enter, !vbus_vld, otg_reg:0x%04x", value);
        otg->a_vbus_vld = FALSE;

        /* HPI can not distinguish between the two thresholds. */
        otg->a_sess_vld = otg->b_sess_vld = 
            (value & OTG_DATA_STAT) ? TRUE : FALSE;

        otg->b_sess_end = 
            (value & OTG_DATA_STAT) ? FALSE : TRUE;
    }

    if( prev_a_vbus_vld != otg->a_vbus_vld || 
        prev_a_sess_vld != otg->a_sess_vld )
    {
        update_otg_state(otg);
    }

    lcd_read_reg (HOST1_IRQ_EN_REG, &value, cy_priv);
    value |= VBUS_IRQ_EN;
    lcd_write_reg(HOST1_IRQ_EN_REG, value, cy_priv);

}

void pcd_irq_vbus(cy_priv_t *cy_priv)
{
    unsigned short intStat;

    lcd_read_reg (HOST1_IRQ_EN_REG, &intStat, cy_priv);
    intStat &= ~VBUS_IRQ_EN;
    lcd_write_reg(HOST1_IRQ_EN_REG, intStat, cy_priv);

    /* Acknowledge interrupt */
    lcd_write_reg(HOST1_STAT_REG, VBUS_IRQ_FLG, cy_priv); 

    if( cy_priv->lcd_priv == NULL || cy_priv->otg == NULL )
    {
        return;
    }

    vbus_timer.function = vbus_timeout;
    vbus_timer.data = (unsigned long)cy_priv;
    mod_timer( &vbus_timer, jiffies + 3 ); /* 50 ms */
}

/*****************************************************************/

void pcd_irq_id(cy_priv_t *cy_priv)
{
    unsigned short otg_stat_reg;
    otg_t *otg = cy_priv->otg;
    int prev_otg_id;

    pcd_dbg("pcd_irq_id enter");
        
    /* Acknowledge interrupt */
    /* Need to do this first since there is no conditioning on the interrupt. */
    lcd_write_reg(HOST1_STAT_REG, ID_IRQ_FLG, cy_priv);

    /* not initialized */
    if( cy_priv->lcd_priv == NULL || otg == NULL )
    {
        return;
    }

    /* OTG ID has changed - we think */
    /* Read the OTG ID Status bit USB Status Register for OTG ID pin */
    lcd_read_reg(OTG_CTL_REG, &otg_stat_reg, cy_priv);

    prev_otg_id = otg->id;

    if (otg_stat_reg & OTG_ID_STAT)
    {
        otg->id = B_DEV;
    }
    else
    {
        otg->id = A_DEV;
    }

    if( prev_otg_id != otg->id )
    {
        update_otg_state(otg);   
    }
}

/*****************************************************************/

void pcd_irq_mb_in(cy_priv_t *cy_priv)
{
    /* not used */
}

/*****************************************************************/

void pcd_irq_mb_out(unsigned int message, int sie, cy_priv_t * cy_priv)
{
    unsigned i;             /* loop index */
    unsigned short ep_number;
    struct usb_endpoint_instance * endpoint;
    sie_info *sie_var = (sie_info *)cy_priv->pcdi;
    otg_t * otg = cy_priv->otg;

    
    /* not initialized yet, or wrong sie, ignore interrupt */
    if( sie_var == NULL || sie_var->sie_number != sie || sie_var->bus == NULL )
    {
        return;
    }

    for( i=1,ep_number=0; i!=0; ep_number++, i<<=1 )
    {
        /* pcd_dbg("i:0x%04x, ep_num:%d", i, ep_number); */
        switch( message & i )
        {
        case SUSB_EP0_MSG: /* ignore ep0 */
        default:
            break;

        case SUSB_EP1_MSG: /* endpoint done */
        case SUSB_EP2_MSG:
        case SUSB_EP3_MSG:
        case SUSB_EP4_MSG:
        case SUSB_EP5_MSG:
        case SUSB_EP6_MSG:
        case SUSB_EP7_MSG:
            /* pcd_dbg("EP_DONE_MSG_ID RECEIVED. Endpoint: %d", ep_number); */
            endpoint = &sie_var->bus->endpoint_array[ep_number];

            if (endpoint->endpoint_address & USB_DIR_IN) 
            {
                usbd_tx_complete_irq(endpoint, 0);            

                if( endpoint->tx_urb )
                {
                    udc_start_in_irq(endpoint, endpoint->tx_urb->device);
                }
            }
            else 
            {
                ep_receive_data(endpoint, sie, cy_priv);
            }
            break;

        case SUSB_RST_MSG: /* reset */
            pcd_dbg("SUSB_RST_MSG");
            usbd_device_event(sie_var->bus->device, DEVICE_RESET, 0);


            if( otg != NULL )
            {
                otg->b_hnp_en = FALSE;
                otg->a_bus_suspend = FALSE;
                otg->b_bus_suspend = FALSE;

                update_otg_state(otg);
            }
            break;

        case SUSB_SOF_MSG:  /* active */
            pcd_dbg("SUSB_SOF_MSG");
            usbd_device_event(sie_var->bus->device, DEVICE_BUS_ACTIVITY, 0);

            if( otg != NULL )
            {
                otg->a_bus_suspend = FALSE;
                otg->b_bus_suspend = FALSE;

                update_otg_state(otg);
            }
            break;

        case SUSB_CFG_MSG: /* configured */
            pcd_dbg("SUSB_CFG_MSG");
            usbd_device_event(sie_var->bus->device, DEVICE_BUS_ACTIVITY, 0);
            usbd_device_event(sie_var->bus->device, DEVICE_CONFIGURED, 0);

            if( otg != NULL )
            {
                otg->a_bus_suspend = FALSE;
                otg->b_bus_suspend = FALSE;

                update_otg_state(otg);
            }
            break;

        case SUSB_SUS_MSG: /* suspend */
            pcd_dbg("SUSB_SUS_MSG");
            usbd_device_event(sie_var->bus->device, DEVICE_BUS_INACTIVE, 0);

            if( otg != NULL )
            {
                otg->a_bus_suspend = TRUE;
                otg->b_bus_suspend = TRUE;
                otg->b_bus_req = TRUE;

                update_otg_state(otg);
            }
            break;

        case 0x4000: /* b_hnp_enable */
            if( otg != NULL )
            {
                pcd_dbg("b_hnp_enable");
                otg->b_hnp_en = TRUE;
                update_otg_state(otg);
            }
            break;

        }
    }
}


/*****************************************************************
 *
 * Function Name: ep_receive_data
 *
 * Description: This function sets up an endpoint to receive data
 *              on the EZ-HOST.
 *
 *****************************************************************/

void ep_receive_data(struct usb_endpoint_instance *endpoint, 
                      int sie_num, cy_priv_t * cy_priv)
{
    unsigned short length;
    TRANSFER_FRAME frame;
    int port_num;


    /* pcd_dbg("ep_receive_data enter: sie_num = %d", sie_num); */

    if( sie->sie_number != sie_num )
    {
        cy_err("message received on wrong sie: exptected=%d, actual= %d", 
                sie->sie_number, sie_num);
        return;
    }

    port_num = (sie_num == SIE1) ? PORT0 : PORT2;

    if(endpoint->rcv_urb == NULL) 
    {
        endpoint->rcv_urb = first_urb_detached(&endpoint->rdy);
    }

    /* check to see that we have an URB to receive data */
    if(endpoint->rcv_urb) 
    {
        int ep = endpoint->endpoint_address & 0xf;

        unsigned short struct_location = 
            sie->recv_struct_location + ep*TXRX_STRUCT_SIZE;

        unsigned short buff_location =
            sie->recv_buffer_location + ep*RECV_BUFF_LENGTH;


        /* read the buffer remainder and compute the number of bytes sent */
        lcd_read_memory( struct_location + 4, 
                        2, 
                        (char *)&length, 
                        cy_priv);
        
        length = sie->recv_buffer_length - length;

        lcd_read_memory(buff_location, 
                        length, 
                        endpoint->rcv_urb->buffer, 
                        cy_priv);

        /* submit the urb to the function driver */
        usbd_rcv_complete_irq(endpoint, length, 0);
        
        /* set the endpoint up to receive again with a pending receive SW
           interrupt */
        frame.link_pointer = 0x0000;
        frame.absolute_address = buff_location;
        frame.data_length = sie->recv_buffer_length; 
        frame.callback_function_location =  0;
           
        lcd_recv_data(struct_location, 
                      port_num, 
                      ep,
                      8, 
                      (char*)&frame, 
                      NULL, 
                      0, 
                      cy_priv);                
    }              
    else
    {
        sie->rcv_data_pend_ep[endpoint->endpoint_address & 0xf] = endpoint;
    }

}


/*****************************************************************
 *
 * Function Name: udc_rcv_urb_recycled
 *
 * Description: Notification from the core layer that an urb has been 
 *              recycled by the function driver.
 *
 *****************************************************************/

void udc_rcv_urb_recycled(void)
{
    size_t i;
    struct usb_endpoint_instance *endpoint;

    for(i=0; i<UDC_MAX_ENDPOINTS; i++)
    {
        endpoint = sie->rcv_data_pend_ep[i];

        if( endpoint != 0 )
        {
            sie->rcv_data_pend_ep[i] = 0;

            ep_receive_data(endpoint, sie->sie_number, sie->cy_priv);
        }
    }
}


/*****************************************************************
 *
 * Function Name: udc_start_in_irq
 *
 * Description:  Called by bi_send_urb, this function is registered with 
 * the peripheral core layer to transmit URB's onto the bus.  Called with 
 * interrupts disabled.
 *
 * We are requiring the URB buffer to be <= the send_data_buffer on the EZ-HOST
 * chip, so we check that it meets this requirement, and then move on.  We 
 * write the data to the buffer in EZ-HOST, and then trigger the SUSBx_SEND_INT
 * SW interrupt.  EZ-HOST then will take care of sending the data in packets 
 * over the bus.  When it is completed, it calls our callback function to 
 * notify us of success of failure.  It is possible to handle urb buffers 
 * larger than the EZ-HOST RAM send buffer space, by setting a flag here and 
 * then waiting for the callback to clear the flag, letting us know we can send
 * more data down to EZ-HOST.  For now anyway, we will keep it simple. If the 
 * data length of the URB is larger than the RAM buffer, we signal an error to 
 * the function driver. 
 *
 *****************************************************************/

void udc_start_in_irq(struct usb_endpoint_instance * endpoint,
                      struct usb_device_instance * device)
{
    int sie;
    int port_num;
    int ep_addr;
    struct usbd_urb * urb;
    sie_info * sie_data;
    
    /* pcd_dbg("udc_start_in_irq enter"); */

    /* determine the SIE this endpoint is associated with */
    sie_data = (sie_info *)device->bus->privdata;
    sie = sie_data->sie_number;
    
    if (sie == SIE1) 
        port_num = PORT0;
    else
        port_num = PORT2;

    ep_addr = endpoint->endpoint_address & 0x000f;
    
    urb = endpoint->tx_urb;

    /* check that we have an active URB to transmit */
    if (urb) 
    {

        if ((urb->actual_length - endpoint->sent) > 0) 
        {
            endpoint->last = MIN(urb->actual_length - endpoint->sent, 
                                 sie_data->send_buffer_length);
        }
        else 
        {
            endpoint->last = 0; /* zero length packet */
        }

        /* write the data to the RAM buffer */
        if (lcd_write_memory(sie_data->send_buffer_location, 
                             endpoint->last, 
                             urb->buffer + endpoint->sent, 
                             sie_data->cy_priv) == SUCCESS) 
        {
            TRANSFER_FRAME frame;

            frame.link_pointer = 0x0000;
            frame.absolute_address = sie_data->send_buffer_location;
            frame.data_length = endpoint->last;
            frame.callback_function_location = 0;

            lcd_send_data(sie_data->send_struct_location, 
                          port_num, 
                          ep_addr, 
                          8, 
                          (char *)&frame, 
                          NULL, 
                          0, 
                          sie_data->cy_priv);
        }                
    }
    else
    {
        pcd_dbg("No urb for transmit");
    }

}



/*****************************************************************
 *
 * Function Name: udc_stall_ep
 *
 *
 * Description: This function will trigger the SUSBx_STALL_INT software 
 * interrupt on EZ-HOST.  This software interrupt is specific to the control
 * endpoint, so the only valid endpoint number is zero.  Triggering this 
 * interrupt will cause EZ-HOST to stall the next transaction on the default
 * endpoint.
 *
 *****************************************************************/

void udc_stall_ep(unsigned int ep, struct usb_device_instance * device)
{
    sie_info * sie_data = (sie_info *) device->bus->privdata;
    lcd_int_data_t int_data;
    
    pcd_dbg("udc_stall_ep enter: ep=%d", ep);

    int_data.int_num = sie_data->susb_stall_interrupt;
    
    /* ensure that the stall is for endpoint zero, otherwise do nothing */
    if (ep == 0) 
    {
        lcd_exec_interrupt(&int_data, NULL, 0, sie_data->cy_priv);
    }
}


/*****************************************************************
 *
 * Function Name: udc_reset_ep
 *
 * Description: The BIOS does not provide a way to directly reset a specific
 * endpoint, so nothing will be done inside of this function.  The purpose of
 * the function is for bus interfaces that allow direct manipulation of each
 * endpoint, this gives the peripheral core a chance to reset each endpoint
 * during initialization.  However, with the EZ-HOST BIOS, this is performed
 * by calling the SUSB_INIT_INT software interrupt, to reset all endpoints 
 * based on the configuration loaded to EZ-HOST.
 *
 *****************************************************************/

void udc_reset_ep(unsigned int ep, struct usb_device_instance * device)
{
}


/*****************************************************************
 *
 * Function Name: udc_endpoint_halted
 *
 * Description: This function is not used for EZ-HOST
 *
 *****************************************************************/

int udc_endpoint_halted(unsigned int ep, struct usb_device_instance * device)
{
    return 0;
}


/*****************************************************************
 *
 * Function Name: udc_set_address
 *
 * Description: Called from control endpoint function after it decodes a set 
 *              address setup packet.
 *****************************************************************/

void udc_set_address(unsigned char address, 
                     struct usb_device_instance * device)
{
    sie_info * sie_data = (sie_info * ) device->bus->privdata;
    
    /* pcd_dbg("udc_set_address enter"); */

    sie_data->usb_address = address;
}


/*****************************************************************
 *
 * Function Name: udc_serial_init
 *
 * Description: This function sets up links in the data structures.
 *
 *****************************************************************/

int __init udc_serial_init(struct usb_bus_instance * bus)
{
    /* At this time, the device instance does not exist */
    /* The sie global should have been initialized by now in perhiperhal init */

    bus->privdata = sie;
    sie->bus = bus;

    return SUCCESS;
}



/*****************************************************************
 *
 * Function Name: udc_max_endpoints
 *
 * Description: Return the number of pysical endpoints
 *
 *****************************************************************/

int udc_max_endpoints(void)
{
    return(UDC_MAX_ENDPOINTS);
}



/*****************************************************************
 *
 * Function Name: udc_check_ep
 *
 * Description: This function will verify whether a function driver has set
 * up its endpoint descriptors properly.  It will retrieve the
 * endpoint descriptors, and then check their endpoint numbers
 * against what the bus interface is capable of handling, i.e.
 * whether or not a function driver tries to implement nine
 * endpoints, when only eight are supported on the hardware.
 *
 *****************************************************************/
      
int udc_check_ep(int logical_endpoint, int packetsize, 
                 struct usb_device_instance * device)
{
    int pys_ep;

    pys_ep = (((logical_endpoint & 0xf) >= UDC_MAX_ENDPOINTS) 
            || (packetsize > 64)) ?  0 : (logical_endpoint & 0xf);

    return pys_ep;
}



/*****************************************************************
 *
 * Function Name: udc_setup_ep
 *
 * Description:    This function will prepare the endpoint_instance structure
 * for each physical endpoint.  It allocates the number of 
 * URB's to be held in the receive queue for each endpoint.  
 * Additionally, this endpoint will call the BIOS SW interrupt
 * SUSBx_RECEIVE_INT to set up the receive endpoint for a 
 * pending USB packet.                    
 *****************************************************************/
      
void udc_setup_ep(struct usb_device_instance * device, unsigned int ep, 
                  struct usb_endpoint_instance * endpoint)
{
    int port_num;
    sie_info * sie_data = (sie_info *)device->bus->privdata;
    cy_priv_t * cy_priv = (cy_priv_t * ) sie_data->cy_priv;

    pcd_dbg("udc_setup_ep enter: ep = %d", ep);
    
    port_num = (sie_data->sie_number == SIE1) ? PORT0 : PORT2;

    if (ep < UDC_MAX_ENDPOINTS) 
    {
        device->bus->endpoint_array[ep] = *endpoint;
    
        if (ep == 0) 
        {
            usbd_fill_rcv(device, endpoint, CONTROL_EP_RECV_URB_COUNT);
            endpoint->rcv_urb = first_urb_detached(&endpoint->rdy);
        }
        else if (endpoint->endpoint_address & 0x80) 
        {
            /* IN ENDPOINT -> send */
        }
        else if( endpoint->endpoint_address )
        {
            /* OUT ENDPOINT -> receive */

            TRANSFER_FRAME frame;    

            unsigned short struct_location = 
                sie_data->recv_struct_location + ep*TXRX_STRUCT_SIZE;

            unsigned short buff_location =
                sie_data->recv_buffer_location + ep*RECV_BUFF_LENGTH;


            usbd_fill_rcv(device, endpoint, OUT_EP_URB_COUNT);
            endpoint->rcv_urb = first_urb_detached(&endpoint->rdy);

                
            frame.link_pointer = 0x0000;
            frame.absolute_address = buff_location;
            frame.data_length = sie_data->recv_buffer_length;
            frame.callback_function_location =  0;
               
            lcd_recv_data(struct_location, 
                          port_num, 
                          ep,
                          8, 
                          (char*) &frame, 
                          NULL, 
                          0, 
                          cy_priv);                        

            pcd_dbg("Setup ep%d", endpoint->endpoint_address & 0xf);

        }                              
    }
}


/*****************************************************************
 *
 * Function Name: udc_disable_ep
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_disable_ep(unsigned int ep)
{
}


/*****************************************************************
 *
 * Function Name: udc_connected
 *
 * Description: This function is not used.
 *
 *****************************************************************/

int udc_connected(void)
{
    return 1;
}


/*****************************************************************
 *
 * Function Name: udc_connect
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_connect()
{
}


/*****************************************************************
 *
 * Function Name: udc_disconnect
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_disconnect()
{
}


/*****************************************************************
 *
 * Function Name: udc_all_interrupts
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_all_interrupts(struct usb_device_instance *device)
{
}


/*****************************************************************
 *
 * Function Name: udc_suspened_interrupts
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_suspended_interrupts(struct usb_device_instance *device)
{
}


/*****************************************************************
 *
 * Function Name: udc_disable_interrupts
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_disable_interrupts(struct usb_device_instance *device)
{
}


/*****************************************************************
 *
 * Function Name: udc_ep0_packetsize
 *
 * Description: This function returns ep0 packet size.
 *
 *****************************************************************/

int udc_ep0_packetsize(void)
{
    return EP0_PACKETSIZE;
}


/*****************************************************************
 *
 * Function Name: udc_enable
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_enable(struct usb_device_instance *device)
{
}


/*****************************************************************
 *
 * Function Name: udc_disable
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_disable(void)
{
}


/*****************************************************************
 *
 * Function Name: udc_startup_events
 *
 * Description: This function issues events to the usbd core as
 * part of initialization.
 *
 *****************************************************************/

void udc_startup_events(struct usb_device_instance * device)
{
    usbd_device_event(device, DEVICE_INIT, 0);
    usbd_device_event(device, DEVICE_CREATE, 0);
}


/*****************************************************************
 *
 * Function Name: udc_init
 *
 * Description: This function is not used.
 *
 *****************************************************************/

int udc_init()
{
    return SUCCESS;
}


/*****************************************************************
 *
 * Function Name: udc_regs
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_regs(void)
{
}


/*****************************************************************
 *
 * Function Name: udc_name
 *
 * Description: This function returns the name of the interface 
 * to the usbd core.
 *
 *****************************************************************/

char * udc_name(void)
{
    return UDC_NAME;
}


/*****************************************************************
 *
 * Function Name: udc_request_udc_irq
 *
 * Description: This function is not used.
 *
 *****************************************************************/

int udc_request_udc_irq(void)
{
    return SUCCESS;
}


/*****************************************************************
 *
 * Function Name: udc_request_cable_irq
 *
 * Description: This function is not used.
 *
 *****************************************************************/

int udc_request_cable_irq(void)
{
    return SUCCESS;
}


/*****************************************************************
 *
 * Function Name: udc_request_io
 *
 * Description: This function is not used.
 *
 *****************************************************************/

int udc_request_io(void)
{
    return SUCCESS;
}


/*****************************************************************
 *
 * Function Name: udc_release_udc_irq
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_release_udc_irq(void)
{    
}


/*****************************************************************
 *
 * Function Name: udc_release_cable_irq
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_release_cable_irq(void)
{
}


/*****************************************************************
 *
 * Function Name: udc_release_io
 *
 * Description: This function is not used.
 *
 *****************************************************************/

void udc_release_io(void)
{
}

