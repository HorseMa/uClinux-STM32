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
#include <pcmcia/driver_ops.h>

#include "version_info.h"
#include "isl_gen.h"
#include "isl_38xx.h"
#include "islpci_pcmcia.h"
#include "islpci_dev.h"

#ifdef INTERSIL_EVENTS
#include <bloboid.h>
#include "isl_mgt.h"
#include "islpci_mgt.h"
#endif

/******************************************************************************
        Global variable definition section
******************************************************************************/
MODULE_AUTHOR("W.Termorshuizen, R.Bastings");
MODULE_DESCRIPTION("Intersil 802.11 Wireless LAN adapter");
MODULE_LICENSE("GPL");


extern struct net_device *root_islpci_device;
extern int pc_debug;

struct driver_operations islpci_ops =
{
    DRIVER_NAME, islpci_attach, islpci_suspend, islpci_resume, islpci_detach
};


/******************************************************************************
    Module initialization functions
******************************************************************************/
dev_node_t *islpci_attach(dev_locator_t * loc)
{
    u32 io;
    u16 dev_id;
    u8 bus, devfn, irq, latency_tmr;
    struct pci_dev *pci_device;
    struct net_device *nw_device;
    dev_node_t *node;
    islpci_private *private_config;
    int rvalue;
    int dma_mask = 0xffffffff;
    char firmware[256];

    // perform some initial setting checks
    if (loc->bus != LOC_PCI)
        return NULL;
    bus = loc->b.pci.bus;
    devfn = loc->b.pci.devfn;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_attach(bus %d, function %d)\n",
        bus, devfn);
#endif

    // get some pci settings for verification
    pcibios_read_config_dword(bus, devfn, PCI_BASE_ADDRESS_0, &io);
    pcibios_read_config_byte(bus, devfn, PCI_INTERRUPT_LINE, &irq);
    pcibios_read_config_word(bus, devfn, PCI_DEVICE_ID, &dev_id);

	// check whether the latency timer is set correctly
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

    if (io &= ~3, io == 0 || irq == 0)
    {
        DEBUG(SHOW_ERROR_MESSAGES, "The ISL38XX Ethernet interface was not "
        "assigned an %s.\n" KERN_ERR "  It will not be activated.\n",
        io == 0 ? "I/O address" : "IRQ");
        return NULL;
    }
    // get pci device information by loading the pci_dev structure
    if (pci_device = pci_find_slot(bus, devfn), pci_device == NULL)
    {
        // error reading the pci device structure
        DEBUG(SHOW_ERROR_MESSAGES, "ERROR: %s could not get PCI device "
            "information \n", DRIVER_NAME );
        return NULL;
    }

    // determine what the supported DMA memory region is
    while( pci_set_dma_mask( pci_device, dma_mask ) != 0 )
    {
        // range not supported, shift the mask and check again
        if( dma_mask >>=  1, dma_mask == 0 )
        {
            // mask is zero, DMA memory not supported by PCI
            DEBUG(SHOW_ERROR_MESSAGES, "DMA Memory not supported\n" );
            return NULL;
        }
    }

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "DMA Memory support mask is 0x%x \n", dma_mask );
#endif

    // setup the network device interface and its structure
    if (nw_device = islpci_probe(NULL, pci_device, (long) io, (int) irq),
        nw_device == NULL)
    {
        // error configuring the driver as a network device
        DEBUG(SHOW_ERROR_MESSAGES, "ERROR: %s could not configure "
        "network device \n", DRIVER_NAME );
        return NULL;
    }

#ifdef WDS_LINKS
    mgt_indication_handler ( DEV_NETWORK, nw_device->name, DOT11_OID_WDSLINKADD, islpci_wdslink_add_hndl );
    mgt_indication_handler ( DEV_NETWORK, nw_device->name, DOT11_OID_WDSLINKREMOVE, islpci_wdslink_del_hndl );
#endif

    // save the interrupt request line and use the remapped device base address
    // as the device identification both for uniqueness and parameter passing
    // to the interrupt handler
    private_config = nw_device->priv;
    private_config->pci_irq = irq;
    private_config->pci_dev_id = dev_id;
    private_config->device_id = private_config->remapped_device_base;
	spin_lock_init( &private_config->slock );

    // request for the interrupt before uploading the firmware
    if (rvalue = request_irq(irq, &islpci_interrupt, SA_INTERRUPT | SA_SHIRQ,
        DRIVER_NAME, private_config), rvalue != 0)
    {
        // error, could not hook the handler to the irq
        DEBUG(SHOW_ERROR_MESSAGES, "ERROR: %s could not install "
            "IRQ-handler \n", DRIVER_NAME );
        return NULL;
    }

    // select the firmware file depending on the device id, take for default
    // the 3877 firmware file
    if( dev_id == PCIDEVICE_ISL3890 )
    	strcpy( firmware, ISL3890_IMAGE_FILE );
    else
    	strcpy( firmware, ISL3877_IMAGE_FILE );

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Device %x, firmware file: %s \n", dev_id, firmware );
#endif
	
    if (isl38xx_upload_firmware( firmware, private_config->remapped_device_base,
        private_config->device_host_address  ) == -1)
    {
        // error uploading the firmware
        DEBUG(SHOW_ERROR_MESSAGES, "ERROR: %s could not upload the "
        "firmware \n", DRIVER_NAME );
        return NULL;
    }

    // finally setup the node structure with the device information
    node = kmalloc(sizeof(dev_node_t), GFP_KERNEL);
    strcpy(node->dev_name, nw_device->name);
    node->major = 0;
    node->minor = 0;
    node->next = NULL;
    MOD_INC_USE_COUNT;
    return node;
}

void islpci_suspend(dev_node_t * node)
{
    struct net_device **devp, **next;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_suspend(%s)\n", node->dev_name);
#endif

    for (devp = &root_islpci_device; *devp; devp = next)
    {
        next = &((islpci_private *) (*devp)->priv)->next_module;
        if (strcmp((*devp)->name, node->dev_name) == 0)
        break;
    }

    if (*devp)
    {
//        long ioaddr = (*devp)->base_addr;
//        islpci_pause(*devp);
    }
}

void islpci_resume(dev_node_t * node)
{
    struct net_device **devp, **next;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_resume(%s)\n", node->dev_name);
#endif

    for (devp = &root_islpci_device; *devp; devp = next)
    {
        next = &((islpci_private *) (*devp)->priv)->next_module;
        if (strcmp((*devp)->name, node->dev_name) == 0)
        break;
    }

    if (*devp)
    {
//        islpci_restart(*devp);
    }
}

void islpci_detach(dev_node_t * node)
{
    struct net_device **devp, **next;
    islpci_private *private_config;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "islpci_detach(%s)\n", node->dev_name);
#endif

    for (devp = &root_islpci_device; *devp; devp = next)
    {
        next = &((islpci_private *) (*devp)->priv)->next_module;
        if (strcmp((*devp)->name, node->dev_name) == 0)
        break;
    }

    if (*devp)
    {
        // free the interrupt request
        private_config = (*devp)->priv;
        writel(0, private_config->remapped_device_base + ISL38XX_INT_EN_REG);
        free_irq(private_config->pci_irq, private_config);

        // free the node
        kfree(node);
        MOD_DEC_USE_COUNT;
    }
}

int init_module(void)
{
    DEBUG(SHOW_ANYTHING, "Loaded %s, version %s\n", DRIVER_NAME, VERSIONID );

#ifdef INTERSIL_EVENTS
    mgt_initialize();
#endif
    
    register_driver(&islpci_ops);
    
#ifdef INTERSIL_EVENTS
    mgt_indication_handler ( DEV_NETWORK, 0, 0, islpci_mgt_indication );
#endif
    
    return 0;
}

void cleanup_module(void)
{
    struct net_device *next_dev;
    islpci_private *private_config;

#ifdef INTERSIL_EVENTS
    mgt_cleanup();
#endif
    
    unregister_driver(&islpci_ops);

    /* No need to check MOD_IN_USE, as sys_delete_module() checks. */
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

        // free the separately allocated areas
        kfree(private_config);
        kfree(root_islpci_device);

        // change the root device pointer to the next device for clearing
        root_islpci_device = next_dev;
    }
    
    DEBUG(SHOW_ANYTHING, "Unloaded %s\n", DRIVER_NAME );
}
