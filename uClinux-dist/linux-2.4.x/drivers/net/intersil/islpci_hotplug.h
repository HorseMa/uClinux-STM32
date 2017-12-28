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

#ifndef _ISLPCI_HOTPLUG_H
#define _ISLPCI_HOTPLUG_H


#ifndef DRIVER_NAME
#define DRIVER_NAME             "islpci"
#endif

#ifdef CONFIG_ARCH_ISL3893
#define ISL3877_IMAGE_FILE      "/boot/3877.arm"
#define ISL3890_IMAGE_FILE  	"/boot/3890.arm"
#else
#define ISL3877_IMAGE_FILE      "/etc/hotplug/isl3877.arm"
#define ISL3890_IMAGE_FILE  	"/etc/hotplug/isl3890.arm"
#endif

// PCI Class & Sub-Class code, Network-'Other controller'
#define PCI_CLASS_NETWORK_OTHERS 0x280

#define PCI_TYPE                (PCI_USES_MEM | PCI_ADDR0 | PCI_NO_ACPI_WAKE)


#ifdef CONFIG_ARCH_ISL3893
int islpci_probe_pci(struct net_device *nw_device, struct pci_dev *pci_device,
                     const struct pci_device_id *id);
#else
int islpci_probe_pci(struct pci_dev *pci_device, const struct pci_device_id *id);
#endif

void islpci_remove_pci( struct pci_dev * );
int init_module( void );
void cleanup_module( void );


#endif  // _ISLPCI_HOTPLUG_H
