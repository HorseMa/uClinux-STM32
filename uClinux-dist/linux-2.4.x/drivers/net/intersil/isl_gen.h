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

#ifndef _ISL_GEN_H
#define _ISL_GEN_H

#include <linux/skbuff.h>


// General driver definitions
#define PCIVENDOR_INTERSIL                      0x1260UL
#define PCIDEVICE_ISL3877                       0x3877UL
#define PCIDEVICE_ISL3890                       0x3890UL
#define PCIDEVICE_LATENCY_TIMER_MIN 			0x40
#define PCIDEVICE_LATENCY_TIMER_VAL 			0x50

// Debugging verbose definitions
#define SHOW_NOTHING                            0x00    // overrules everything
#define SHOW_ANYTHING                           0xFF
#define SHOW_ERROR_MESSAGES                     0x01
#define SHOW_TRAPS                              0x02
#define SHOW_FUNCTION_CALLS                     0x04
#define SHOW_TRACING                            0x08
#define SHOW_QUEUE_INDEXES                      0x10
#define SHOW_PIMFOR_FRAMES                      0x20
#define SHOW_BUFFER_CONTENTS                    0x40
#define VERBOSE                                 0x01



/*
 *  Function definitions
 */

//#if TARGET_SYSTEM == RAWPCI
//#define K_DEBUG( f, m, args... ) if(( f & m ) != 0 ) printk( KERN_DEBUG args )
//#else
#define K_DEBUG( f, m, args... ) if(( f & m ) != 0 ) printk( KERN_INFO args )
//#endif
#define DEBUG( f, args... )     K_DEBUG( f, pc_debug, args )

static inline void add_le32p(u32 *le_number, u32 add)
{
    *le_number = cpu_to_le32(le32_to_cpup(le_number) + add);
}

static inline void driver_lock( spinlock_t *lock, unsigned long *pflags )
{
  /* Disable interrupts and aquire the lock */
  spin_lock_irqsave( lock, *pflags );
}

static inline void driver_unlock( spinlock_t *lock, unsigned long *pflags )
{
  /* Release the lock and reenable interrupts */
  spin_unlock_irqrestore( lock, *pflags );
}


void display_buffer( char *, int );
void schedule_wait( int );
void print_frame( struct sk_buff * );
void string_to_macaddress( unsigned char *, unsigned char * );
int address_compare( unsigned char *, unsigned char * );


#endif  // _ISL_GEN_H
