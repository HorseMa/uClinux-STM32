/****************************************************************************
*
*	Name:			mac.h
*
*	Description:	Header file for unicast, multicast and broadcast MAC 
*					address handling.
*
*	Copyright:		(c) 1999-2002 Conexant Systems Inc.
*					Personal Computing Division
*****************************************************************************

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA
*
****************************************************************************
*  $Author:   davidsdj  $
*  $Revision:   1.0  $
*  $Modtime:   Mar 19 2003 12:25:20  $
****************************************************************************/

#ifndef __MAC_H
#define __MAC_H
/****************************************************************************
 * Include Files
 ****************************************************************************/

#include <vxWorks.h>
#include <string.h>


/****************************************************************************
 * Defines
 ****************************************************************************/

/* length of MAC address (in bytes) */
#define MAC_ADDR_LEN          ( 6 ) 
           
/* return value for MAC_cmp */
#define MAC_GREATER           ( 1 )    
#define MAC_EQUAL             ( 0 )
#define MAC_LESS              ( -1 )

/* size of multicast address and hash tables */
#define MULTI_TABLE_SIZE      ( 256 )
#define HASH_TABLE_SIZE       ( 32 )

/* bit which defines whether a MAC Address is multi or uni cast */
#define MULTICAST_MSK         ( 0x01 )

/* hash table initialization values */
#define FILTER_ALL            ( 0x00 )
#define PASS_ALL              ( 0xFF )


/****************************************************************************
 * Structures
 ****************************************************************************/

/* MAC Address */
typedef UINT8 MAC_ADDR[MAC_ADDR_LEN];

/* aligned MAC address */
typedef struct aligned_mac_addr
{
   UINT16      usLSW;      /* least significant word */
   UINT16      usCSW;      /* middle word */
   UINT16      usMSW;      /* most significant word */
} ALIGNED_MAC_ADDR;


/****************************************************************************
 * Prototypes
 ****************************************************************************/

int   MAC_cmp(const void *, const void *);
void  MAC_SetupHashTable(MAC_ADDR *, UINT16 , UINT8 *);
#endif /*  End of __MAC_H  */
/*###################################################*/

