/*  $Header$
 *  
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/version.h>
#ifdef MODULE
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#else
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT
#endif

#include <linux/pci.h>
#ifdef CONFIG_ARCH_ISL3893
#include <asm/arch/pci.h>
#endif
#include "version_info.h"
#include "isl_gen.h"
#include "isl_38xx.h"
#include "islpci_hotplug.h"
#include "islpci_dev.h"

#ifdef INTERSIL_EVENTS
#include <bloboid.h>
#include "isl_mgt.h"
#include "islpci_mgt.h"
#endif

/******************************************************************************
        Global variable definition section
******************************************************************************/
#ifdef MODULE
MODULE_AUTHOR("W.Termorshuizen, R.Bastings");
MODULE_DESCRIPTION("Intersil 802.11 Wireless LAN adapter");
MODULE_LICENSE("GPL");
#endif


extern struct net_device *root_islpci_device;
extern int pc_debug;

struct pci_device_id pci_id_table[] =
{
    {
        PCIVENDOR_INTERSIL, PCIDEVICE_ISL3877,
        PCI_ANY_ID, PCI_ANY_ID, 0, 0,
        // Driver data, we just put the name here
        (unsigned long) "Intersil PRISM Indigo Wireless LAN adapter"
    },
    {
        PCIVENDOR_INTERSIL, PCIDEVICE_ISL3890,
        PCI_ANY_ID, PCI_ANY_ID, 0, 0,
        // Driver data, we just put the name here
        (unsigned long) "Intersil PRISM Duette Wireless LAN adapter"
    },
    {
        0, 0, 0, 0, 0, 0, 0
    }
};

// register the device with the Hotplug facilities of the kernel
#ifdef MODULE
MODULE_DEVICE_TABLE( pci, pci_id_table );
#endif

struct pci_driver islpci_pci_drv_id =
{
    {},
    DRIVER_NAME,                // Driver name
    pci_id_table,               // id table
    islpci_probe_pci,           // probe function
    islpci_remove_pci,          // remove function
    NULL,                       // save_state function
    NULL,                       // suspend function
    NULL,                       // resume function
    NULL,                       // enable_wake function
};

/******************************************************************************
    Module initialization functions
******************************************************************************/

#ifdef CONFIG_ARCH_ISL3893

int prism_uap_probe(struct net_device *ndev)
{
    struct pci_dev *pdev = NULL;
    int error;
        
    if ( ndev->base_addr != 4 )      
        return -ENODEV;
    
    printk("prism_uap_probe(%s)\n", ndev->name );
    
    if ( pdev = pci_find_device( PCI_VENDOR_ID_INTERSIL, 0x3877, NULL ) ) 
    {
        printk("pci: Intersil 3877 found\n" );
        
        if ( error = islpci_probe_pci( ndev, pdev, &(pci_id_table[0])), error < 0 )
        {
            printk("islpci_probe_pci failed!\n" );
            return error;
        }

        return 0;
    }

    if ( pdev = pci_find_device( PCI_VENDOR_ID_INTERSIL, 0x3890, NULL ) ) 
    {
        printk("pci: Intersil 3890 found\n" );
        
        if ( error = islpci_probe_pci( ndev, pdev, &(pci_id_table[1])), error < 0 )
        {
            printk("islpci_probe_pci failed!\n" );
            return error;
        }
        
        return 0;
    }

    // no device found
    return -ENODEV;
}

int islpci_probe_pci(struct net_device *nw_device, struct pci_dev *pci_device,
                     const struct pci_device_id *id)
{
#else
int  islpci_probe_pci(struct pci_dev *pci_device, const struct pci_device_id *id)
{
    struct net_device *nw_device=NULL;
#endif
    void *phymem;
    u16 irq;
    u8 bus, devfn, latency_tmr;
    islpci_private *private_config;
    int rvalue, dev_id;
    int dma_mask = 0xffffffff;
    char firmware[256];

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_probe_pci\n");
#endif

    // Enable the pci device
    if (pci_enable_device(pci_device))
    {
        DEBUG(SHOW_ERROR_MESSAGES, "%s: pci_enable_device() failed.\n",
            DRIVER_NAME );
        return -EIO;
    }
    // Figure out our resources
    irq = pci_device->irq;
    bus = pci_device->bus->number;
    devfn = pci_device->devfn;
    dev_id = pci_device->device;

	// check whether the latency timer is set correctly
#ifdef CONFIG_ARCH_ISL3893
    pcibios_write_config_byte(bus, devfn, PCI_TRDY_TIMEOUT_VALUE, 0 );
    pcibios_write_config_byte(bus, devfn, PCI_RETRY_TIMEOUT_VALUE, 0 );
#else
    pcibios_read_config_byte(bus, devfn, PCI_LATENCY_TIMER, &latency_tmr);
#if VERBOSE > SHOW_ERROR_MESSAGES 
	DEBUG( SHOW_TRACING, "latency timer: %x\n", latency_tmr );
#endif
	if( latency_tmr < PCIDEVICE_LATENCY_TIMER_MIN )
	{
		// set the latency timer
		pcibios_write_config_byte(bus, devfn, PCI_LATENCY_TIMER, 
			PCIDEVICE_LATENCY_TIMER_VAL );
	}
#endif

    // determine what the supported DMA memory region is
    while( pci_set_dma_mask( pci_device, dma_mask ) != 0 )
    {
        // range not supported, shift the mask and check again
        if( dma_mask >>=  1, dma_mask == 0 )
        {
            // mask is zero, DMA memory not supported by PCI
            DEBUG(SHOW_ERROR_MESSAGES, "DMA Memory not supported\n" );
            return -EIO;
        }
    }

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "DMA Memory support mask is 0x%x \n", dma_mask );
#endif

    // call for the physical address to address the PCI device
    phymem = (void *) pci_resource_start(pci_device, 0);
#ifdef CONFIG_ARCH_ISL3893
    // FIXme
    printk("!!pci_resource_start = %p, shound be 0x10000000\n", phymem );
    phymem = 0x10000000;
#endif
    // request the pci device for regions ???
    
    if (rvalue = pci_request_regions(pci_device, DRIVER_NAME ), rvalue)
    {
        DEBUG(SHOW_ERROR_MESSAGES, "pci_request_regions failure, rvalue %i\n",
            rvalue );
        return -EIO;
    }
    
    // enable PCI bus-mastering
    pci_set_master(pci_device);

    // Log the device resources information
#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "islpci device: phymem:0x%lx, irq:%d \n",
        (long) phymem, irq);
#endif

    // setup the network device interface and its structure
    if (nw_device = islpci_probe(nw_device, pci_device, (long) phymem, (int) irq),
        nw_device == NULL)
    {
        // error configuring the driver as a network device
        DEBUG(SHOW_ERROR_MESSAGES, "ERROR: %s could not configure "
            "network device \n", DRIVER_NAME );
        return -EIO;
    }

    // save the interrupt request line and use the remapped device base address
    // as the device identification both for uniqueness and parameter passing
    // to the interrupt handler
    private_config = nw_device->priv;
    private_config->pci_irq = irq;
    private_config->pci_dev_id = dev_id;
    private_config->device_id = private_config->remapped_device_base;
	
    spin_lock_init( &private_config->slock );

    // request for the interrupt before uploading the firmware
    printk("request_irq(%d)\n", irq );
    if ( rvalue = request_irq(irq, &islpci_interrupt, SA_INTERRUPT | SA_SHIRQ, DRIVER_NAME, private_config), rvalue != 0 )
    {
        // error, could not hook the handler to the irq
        DEBUG(SHOW_ERROR_MESSAGES, "ERROR: %s could not install IRQ-handler \n", DRIVER_NAME );
        return -EIO;
    }

#ifdef CONFIG_ARCH_ISL3893
    uPCI->ARMIntEnReg = 0x20000FF;
#endif

#ifndef CONFIG_ARCH_ISL3893
    // select the firmware file depending on the device id, take for default
    // the 3877 firmware file
    if( dev_id == PCIDEVICE_ISL3890 )
    	strcpy( firmware, ISL3890_IMAGE_FILE );
    else
    	strcpy( firmware, ISL3877_IMAGE_FILE );

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Firmware file: %s \n", firmware );
#endif
	
    if (isl38xx_upload_firmware( firmware, private_config->remapped_device_base,
        private_config->device_host_address ) == -1)
    {
        DEBUG(SHOW_ERROR_MESSAGES, "ERROR: %s could not upload the "
            "firmware \n", DRIVER_NAME );
        return -EIO;
    }
#endif

#ifdef CONFIG_ARCH_ISL3893
    mgt_initialize();
#endif

#ifdef INTERSIL_EVENTS 
    mgt_indication_handler ( DEV_NETWORK, nw_device->name, 0, islpci_mgt_indication );
#endif
#ifdef WDS_LINKS 
    mgt_indication_handler ( DEV_NETWORK, nw_device->name, DOT11_OID_WDSLINKADD, islpci_wdslink_add_hndl );
    mgt_indication_handler ( DEV_NETWORK, nw_device->name, DOT11_OID_WDSLINKREMOVE, islpci_wdslink_del_hndl );
#endif

    return 0;
}

void islpci_remove_pci( struct pci_dev *pci_device )
{
    struct net_device **devp, **next;
    islpci_private *private_config;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_remove_pci\n");
#endif

    for (devp = &root_islpci_device; *devp; devp = next)
    {
        next = &((islpci_private *) (*devp)->priv)->next_module;
        private_config = (*devp)->priv;

        // free the interrupt request
        writel(0, private_config->remapped_device_base + ISL38XX_INT_EN_REG);
        free_irq(private_config->pci_irq, private_config);
    }

}

int init_module( void )
{
    DEBUG(SHOW_ANYTHING, "Loaded %s, version %s\n", DRIVER_NAME, VERSIONID );

    if (pci_register_driver(&islpci_pci_drv_id) <= 0)
    {
        DEBUG(SHOW_ERROR_MESSAGES, "%s: No devices found, driver not "
            "installed.\n", DRIVER_NAME);
        pci_unregister_driver(&islpci_pci_drv_id);
        return -ENODEV;
    }

    return 0;
}


void cleanup_module( void )
{
    struct net_device *next_dev;
    islpci_private *private_config;

#ifdef INTERSIL_EVENTS
    mgt_cleanup();
#endif
    
    pci_unregister_driver(&islpci_pci_drv_id);

    // No need to check MOD_IN_USE, as sys_delete_module() checks
    while (root_islpci_device)
    {
        private_config = (islpci_private *) root_islpci_device->priv;
        next_dev = private_config->next_module;

#if VERBOSE > SHOW_ERROR_MESSAGES 
        DEBUG(SHOW_TRACING, "Cleanup netdevice \n");
#endif
        // unregister the network device
        unregister_netdev(root_islpci_device);

        // free the PCI memory and unmap the remapped page
        islpci_free_memory( private_config, ALLOC_MEMORY_MODE );
        iounmap(private_config->remapped_device_base);
        pci_release_regions(private_config->pci_device);

        // free the separately allocated areas
        kfree(private_config);
        kfree(root_islpci_device);

        // change the root device pointer to the next device for clearing
        root_islpci_device = next_dev;
    }

    DEBUG(SHOW_ANYTHING, "Unloaded %s\n", DRIVER_NAME );
}
