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

#ifndef _ISLPCI_MGT_H
#define _ISLPCI_MGT_H

#include "islpci_dev.h"


// Default card definitions
#define CARD_DEFAULT_SSID                       "uAP"
#define CARD_DEFAULT_SSIDOVERRIDE               "uAP"
#define CARD_DEFAULT_CHANNEL                    4
#define CARD_DEFAULT_MODE                       INL_MODE_AP
#define CARD_DEFAULT_BSSTYPE                    DOT11_BSSTYPE_INFRA
#define CARD_DEFAULT_KEY1                       "default_key_1"
#define CARD_DEFAULT_KEY2                       "default_key_2"
#define CARD_DEFAULT_KEY3                       "default_key_3"
#define CARD_DEFAULT_KEY4                       "default_key_4"
#define CARD_DEFAULT_WEP                        0
#define CARD_DEFAULT_FILTER                     0
#ifdef WDS_LINKS
#define CARD_DEFAULT_WDS                        1
#else
#define CARD_DEFAULT_WDS                        0
#endif
#define	CARD_DEFAULT_AUTHEN                     DOT11_AUTH_OS
#define	CARD_DEFAULT_DOT1X			            0
#define CARD_DEFAULT_MLME_MODE			        DOT11_MLME_AUTO

// PIMFOR package definitions
#define PIMFOR_ETHERTYPE                        0x8828
#define PIMFOR_HEADER_SIZE                      12
#define PIMFOR_VERSION                          1
#define PIMFOR_OP_GET                           0
#define PIMFOR_OP_SET                           1
#define PIMFOR_OP_RESPONSE                      2
#define PIMFOR_OP_ERROR                         3
#define PIMFOR_OP_TRAP                          4
#define PIMFOR_OP_RESERVED                      5       // till 255
#define PIMFOR_DEV_ID_MHLI_MIB                  0
#define PIMFOR_FLAG_APPLIC_ORIGIN               0x01
#define PIMFOR_FLAG_LITTLE_ENDIAN               0x02

// Driver specific oid definitions (NDIS driver)
#define OID_INL_TUNNEL                          0xFF020000
#define OID_INL_MEMADDR                         0xFF020001
#define OID_INL_MEMORY                          0xFF020002
#define OID_INL_MODE                            0xFF020003
#define OID_INL_COMPONENT_NR                    0xFF020004
#define OID_INL_VERSION                         0xFF020005
#define OID_INL_INTERFACE_ID                    0xFF020006
#define OID_INL_COMPONENT_ID                    0xFF020007
#define OID_INL_CONFIG                          0xFF020008

// Mode object definitions (OID_INL_MODE)
#define INL_MODE_NONE                           -1
#define INL_MODE_PROMISCUOUS                    0
#define INL_MODE_CLIENT                         1
#define INL_MODE_AP                             2
#define INL_MODE_SNIFFER                        3

// Config object definitions (OID_INL_CONFIG)
#define INL_CONFIG_MANUALRUN                    0x01
#define INL_CONFIG_FRAMETRAP                    0x02
#define INL_CONFIG_RXANNEX                      0x04
#define INL_CONFIG_TXANNEX                      0x08
#define INL_CONFIG_WDS                          0x10


/*
 *  Type definition section
 *
 *  The PIMFOR package is defined using all unsigned bytes, because this makes
 *  it easier to handle 32 bits words on the two different systems the structure
 *  defines only the header allowing copyless frame handling
 */
typedef struct
{
    u8 version;
    u8 operation;
    u8 oid[4];                          // 32 bits word
    u8 device_id;
    u8 flags;
    u8 length[4];                       // 32 bits word
} pimfor_header;

void pimfor_encode_header( int, unsigned long, int, int, int, pimfor_header * );
void pimfor_decode_header( pimfor_header *, int *, unsigned long *, int *,
    	    	    	    	int *, int * );

void islpci_init_queue( queue_root * );
int islpci_put_queue( void *, queue_root *, queue_entry * );
int islpci_get_queue( void *, queue_root *, queue_entry ** );
int islpci_queue_size( void *, queue_root * );

void islpci_mgt_initialize( islpci_private * );
int islpci_mgt_transmit( islpci_private * );
int islpci_mgt_queue( islpci_private *, int, unsigned long, int, void *, int, int);
int islpci_mgt_receive( islpci_private * );
int islpci_mgt_response(islpci_private *, long, int *, void **, long *, queue_entry** );
void islpci_mgt_release( islpci_private *, queue_entry * );

int islpci_mgt_mlme( unsigned long, char * );

#ifdef INTERSIL_EVENTS
int islpci_mgt_indication( unsigned int, unsigned int, unsigned int, unsigned int,
                           unsigned int, unsigned int, void *, unsigned long );
#endif
#ifdef WDS_LINKS
int islpci_wdslink_add_hndl( unsigned int, unsigned int, unsigned int, unsigned,
                             unsigned int, unsigned int, void *, unsigned long );
int islpci_wdslink_del_hndl( unsigned int, unsigned int, unsigned int, unsigned,
                             unsigned int, unsigned int, void *, unsigned long );
#endif

#endif  // _ISLPCI_MGT_H
