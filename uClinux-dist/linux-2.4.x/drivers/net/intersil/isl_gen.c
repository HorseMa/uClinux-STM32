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

#include <linux/skbuff.h>
#include "isl_gen.h"



/******************************************************************************
    Global variable definition section
******************************************************************************/
extern int pc_debug;


/******************************************************************************
    Driver general functions
******************************************************************************/
void display_buffer(char *buffer, int length)
{
    if( ( pc_debug & SHOW_BUFFER_CONTENTS ) == 0 )
        return;

    while (length > 0)
    {
        printk( "[%02x]", *buffer & 255 );
        length--;
        buffer++;
    }

    printk( "\n" );
}

void print_frame(struct sk_buff *skb)
{
    int offset;

    printk("pr_frame: size %d, skb->data %p, skb %p\n",
        skb->len, skb->data, skb);
    for (offset = 0; offset < skb->len; offset++)
        printk("%02X ", skb->data[offset]);
    printk("\n");
}


void string_to_macaddress( unsigned char *string, unsigned char *mac )
{
    unsigned char *wp = mac, *rp;
    unsigned int value;
    unsigned int count = 0;

    for( rp = string; *rp != '\0'; rp++, wp++, count++ )
    {
        if( count == 6 )
            break;

        if( *rp >= '0' && *rp <= '9' )
            value = *rp - '0';
        else if( *rp >= 'a' && *rp <= 'f' )
            value = *rp - 'a' + 10;
        else if( *rp >= 'A' && *rp <= 'F' )
            value = *rp - 'A' + 10;
        else
            break;

        value <<= 4;
            rp++;
        if( *rp >= '0' && *rp <= '9' )
            value |= *rp - '0';
        else if( *rp >= 'a' && *rp <= 'f')
            value |= *rp - 'a' + 10;
        else if( *rp >= 'A' && *rp <= 'F')
            value |= *rp - 'A' + 10;
        else
            break;

        *wp = (unsigned char) value;
    }
}

int address_compare( unsigned char *address1, unsigned char *address2 )
{
    u32 *address1_hi, *address2_hi;
    u16 *address1_lo, *address2_lo;

    address2_hi = (u32 *) address2;
    address1_hi = (u32 *) address1;
    address2_lo = (u16 *) (address2 + 4);
    address1_lo = (u16 *) (address1 + 4);

    DEBUG(SHOW_TRACING, "address_compare: a1 %08x:%04x a2 %08x:%04x\n",
        *address1_hi, *address1_lo, *address2_hi, *address2_lo );

    return ((*address2_hi == *address1_hi) && (*address2_lo == *address1_lo));
}









