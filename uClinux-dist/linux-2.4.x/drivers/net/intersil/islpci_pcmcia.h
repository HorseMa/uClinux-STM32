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

#ifndef _ISLPCI_PCMCIA_H
#define _ISLPCI_PCMCIA_H


#ifndef DRIVER_NAME
#define DRIVER_NAME             "islpci_cb"
#endif

#define ISL3877_IMAGE_FILE      "/etc/pcmcia/isl3877.arm"
#define ISL3890_IMAGE_FILE  	"/etc/pcmcia/isl3890.arm"


dev_node_t *islpci_attach( dev_locator_t * );
void islpci_suspend( dev_node_t * );
void islpci_resume( dev_node_t * );
void islpci_detach( dev_node_t * );
int init_module( void );
void cleanup_module( void );


#endif  // _ISLPCI_PCMCIA_H
