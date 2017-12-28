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

#define __KERNEL_SYSCALLS__

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

#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "isl_gen.h"
#include "isl_38xx.h"
#include "islpci_dev.h"
#include "islpci_mgt.h"


/******************************************************************************
        Global variable definition section
******************************************************************************/
extern int pc_debug;

int errno;

/******************************************************************************
    Device Interface & Control functions
******************************************************************************/
void isl38xx_enable_update_interrupt(void *device)
{
    u32 reg;

    // set the update queue status bit in the Interrupt Enable Register
    reg = readl(device + ISL38XX_INT_EN_REG);
    reg |= ISL38XX_INT_IDENT_UPDATE;
    writel(reg, device + ISL38XX_INT_EN_REG);
}

void isl38xx_disable_update_interrupt(void *device)
{
    u32 reg;

    // clear the update queue status bit in the Interrupt Enable Register
    reg = readl(device + ISL38XX_INT_EN_REG);
    reg &= ~ISL38XX_INT_IDENT_UPDATE;
    writel(reg, device + ISL38XX_INT_EN_REG);
}

void isl38xx_handle_sleep_request( isl38xx_control_block *control_block,
    int *powerstate, void *device_base )
{
    // device requests to go into sleep mode
    // check whether the transmit queues for data and management are empty
    if( isl38xx_in_queue( control_block, ISL38XX_CB_TX_DATA_LQ))
        // data tx queue not empty
        return;

    if( isl38xx_in_queue( control_block, ISL38XX_CB_TX_MGMTQ))
        // management tx queue not empty
        return;
 
    // check also whether received frames are pending
    if( isl38xx_in_queue( control_block, ISL38XX_CB_RX_DATA_LQ))
        // data rx queue not empty
        return;

    if( isl38xx_in_queue( control_block, ISL38XX_CB_RX_MGMTQ))
        // management rx queue not empty
        return;

//#if VERBOSE > SHOW_ERROR_MESSAGES 
        DEBUG(SHOW_ANYTHING, "Device going to sleep mode\n" );
//#endif
     
    // all queues are empty, allow the device to go into sleep mode
    *powerstate = ISL38XX_PSM_POWERSAVE_STATE;

    // assert the Sleep interrupt in the Device Interrupt Register
    writel( ISL38XX_DEV_INT_SLEEP, device_base + ISL38XX_DEV_INT_REG);
    udelay( ISL38XX_WRITEIO_DELAY );
}

void isl38xx_handle_wakeup( isl38xx_control_block *control_block,
    int *powerstate, void *device_base)
{
    // device is in active state, update the powerstate flag
    *powerstate = ISL38XX_PSM_ACTIVE_STATE;

    // now check whether there are frames pending for the card
    if( !isl38xx_in_queue( control_block, ISL38XX_CB_TX_DATA_LQ))
        // data tx queue empty, no pending frames
        if( !isl38xx_in_queue( control_block, ISL38XX_CB_TX_MGMTQ))
            // management tx queue empty, no pending frames
            return;

//#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_ANYTHING, "Wake up handler trigger the device\n" );
//#endif
        
    // either data or management transmit queue has a frame pending
    // trigger the device by setting the Update bit in the Device Int reg
    writel( ISL38XX_DEV_INT_UPDATE, device_base + ISL38XX_DEV_INT_REG);
    udelay( ISL38XX_WRITEIO_DELAY );
}

void isl38xx_trigger_device( int *powerstate, void *device_base )
{
    struct timeval current_time;
    u32 reg, counter = 0;

#if VERBOSE > SHOW_ERROR_MESSAGES 
        DEBUG(SHOW_FUNCTION_CALLS, "isl38xx trigger device\n" );
#endif

     // check whether the device is in power save mode
    if( *powerstate == ISL38XX_PSM_POWERSAVE_STATE )
    {
        // device is in powersave, trigger the device for wakeup
//#if VERBOSE > SHOW_ERROR_MESSAGES 
        do_gettimeofday( &current_time );
        DEBUG(SHOW_ANYTHING, "%08li.%08li Device wakeup triggered\n", 
            current_time.tv_sec, current_time.tv_usec );
//#endif

        DEBUG(SHOW_ANYTHING, "%08li.%08li Device register read %08x\n", 
            current_time.tv_sec, current_time.tv_usec,
            readl( device_base + ISL38XX_CTRL_STAT_REG) );
        udelay(ISL38XX_WRITEIO_DELAY);

        if( reg = readl( device_base + ISL38XX_INT_IDENT_REG),
            reg == 0xabadface )
        {
//#if VERBOSE > SHOW_ERROR_MESSAGES 
            do_gettimeofday( &current_time );
            DEBUG(SHOW_ANYTHING, "%08li.%08li Device register abadface\n", 
                current_time.tv_sec, current_time.tv_usec );
//#endif
            // read the Device Status Register until Sleepmode bit is set
            while( reg = readl( device_base + ISL38XX_CTRL_STAT_REG),
                (reg & ISL38XX_CTRL_STAT_SLEEPMODE) == 0 )
            {
                udelay(ISL38XX_WRITEIO_DELAY);
                counter++;   
            }

            DEBUG(SHOW_ANYTHING, "%08li.%08li Device register read %08x\n", 
                current_time.tv_sec, current_time.tv_usec,
                readl( device_base + ISL38XX_CTRL_STAT_REG) );
            udelay(ISL38XX_WRITEIO_DELAY);

//#if VERBOSE > SHOW_ERROR_MESSAGES 
        do_gettimeofday( &current_time );
        DEBUG(SHOW_ANYTHING, "%08li.%08li Device asleep counter %i\n", 
            current_time.tv_sec, current_time.tv_usec, counter );
//#endif
        }
    
        // assert the Wakeup interrupt in the Device Interrupt Register
        writel( ISL38XX_DEV_INT_WAKEUP, device_base + ISL38XX_DEV_INT_REG);
        udelay(ISL38XX_WRITEIO_DELAY);

        // perform another read on the Device Status Register
        reg = readl( device_base + ISL38XX_CTRL_STAT_REG);
        udelay(ISL38XX_WRITEIO_DELAY);

//#if VERBOSE > SHOW_ERROR_MESSAGES 
        do_gettimeofday( &current_time );
        DEBUG(SHOW_ANYTHING, "%08li.%08li Device register read %08x\n", 
            current_time.tv_sec, current_time.tv_usec, reg );
//#endif
    }
    else
    {
        // device is (still) awake 
#if VERBOSE > SHOW_ERROR_MESSAGES 
        DEBUG(SHOW_TRACING, "Device is in active state\n" );
#endif
        // trigger the device by setting the Update bit in the Device Int reg
        writel(ISL38XX_DEV_INT_UPDATE, device_base + ISL38XX_DEV_INT_REG);
        udelay(ISL38XX_WRITEIO_DELAY);
    }
}

void isl38xx_interface_reset( void *device_base, dma_addr_t host_address )
{
    u32 reg;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_FUNCTION_CALLS, "isl38xx_interface_reset \n");
#endif

    // load the address of the control block in the device
    writel( host_address, device_base + ISL38XX_CTRL_BLK_BASE_REG);
    udelay(ISL38XX_WRITEIO_DELAY);

#ifdef CONFIG_ARCH_ISL3893
    /* enable the mini PCI target for the 3893 */
    writel( 0x3893, device_base + ISL38XX_GEN_PURP_COM_REG_2);
    udelay(ISL38XX_WRITEIO_DELAY);

#endif
    
    // set the reset bit in the Device Interrupt Register
    writel(ISL38XX_DEV_INT_RESET, device_base + ISL38XX_DEV_INT_REG);
    udelay(ISL38XX_WRITEIO_DELAY);

    // enable the interrupt for detecting initialization
    reg = ISL38XX_INT_IDENT_INIT | ISL38XX_INT_IDENT_UPDATE |
        ISL38XX_INT_IDENT_SLEEP | ISL38XX_INT_IDENT_WAKEUP;
//    reg = ISL38XX_INT_IDENT_INIT;
    writel(reg, device_base + ISL38XX_INT_EN_REG);
    udelay(ISL38XX_WRITEIO_DELAY);
}

int isl38xx_upload_firmware( char *filename, void *device_base, 
    dma_addr_t host_address )
{
    u32 reg;
    int ifp;
    long bcount, length;
    mm_segment_t fs;
    static char buffer[ISL38XX_MEMORY_WINDOW_SIZE];

    // clear the RAMBoot and the Reset bit
    reg = readl(device_base + ISL38XX_CTRL_STAT_REG);
    reg &= ~ISL38XX_CTRL_STAT_RESET;
    reg &= ~ISL38XX_CTRL_STAT_RAMBOOT;
    writel(reg, device_base + ISL38XX_CTRL_STAT_REG);
    udelay(ISL38XX_WRITEIO_DELAY);

    // set the Reset bit without reading the register !
    reg |= ISL38XX_CTRL_STAT_RESET;
    writel(reg, device_base + ISL38XX_CTRL_STAT_REG);
    udelay(ISL38XX_WRITEIO_DELAY);

    // clear the Reset bit
    reg &= ~ISL38XX_CTRL_STAT_RESET;
    writel(reg, device_base + ISL38XX_CTRL_STAT_REG);

    // wait a while for the device to reboot
    mdelay(50);

    // prepare the Direct Memory Base register
    reg = ISL38XX_DEV_FIRMWARE_ADDRES;

    // for file opening temporarily tell the kernel I am not a user for
    // memory management segment access
    
    fs = get_fs();
    set_fs(KERNEL_DS);

    // open the file with the firmware for uploading
    if (ifp = open( filename, O_RDONLY, 0 ), ifp < 0)
    {
        // error opening the file
        DEBUG(SHOW_ERROR_MESSAGES, "ERROR: Could not open file (%s)\n", filename );
        set_fs(fs);
        return -1;
    }
    // enter a loop which reads data blocks from the file and writes them
    // to the Direct Memory Windows
    do
    {
        // set the cards base address for writting the data
        writel(reg, device_base + ISL38XX_DIR_MEM_BASE_REG);
        length = 0;

        // read a block of data until buffer is full or end of file
        do
        {
            bcount = read(ifp, &buffer[length], sizeof(buffer) - length);
            length += bcount;
        }
        while ((length != ISL38XX_MEMORY_WINDOW_SIZE) && (bcount != 0));

        // write the data to the Direct Memory Window
        memcpy_toio(device_base + ISL38XX_DIRECT_MEM_WIN, buffer, length);

        // increment the write address
        reg += ISL38XX_MEMORY_WINDOW_SIZE;
    }
    while (bcount != 0);

    // close the file
    close(ifp);

    // switch back the segment setting
    set_fs(fs);

    // now reset the device
    // clear the Reset & ClkRun bit, set the RAMBoot bit
    reg = readl(device_base + ISL38XX_CTRL_STAT_REG);
	reg &= ~ISL38XX_CTRL_STAT_CLKRUN;
    reg &= ~ISL38XX_CTRL_STAT_RESET;
    reg |= ISL38XX_CTRL_STAT_RAMBOOT;
    writel(reg, device_base + ISL38XX_CTRL_STAT_REG);
    udelay(ISL38XX_WRITEIO_DELAY);

    // set the reset bit latches the host override and RAMBoot bits
    // into the device for operation when the reset bit is reset
    reg |= ISL38XX_CTRL_STAT_RESET;
    writel(reg, device_base + ISL38XX_CTRL_STAT_REG);
    udelay(ISL38XX_WRITEIO_DELAY);

    // clear the reset bit should start the whole circus
    reg &= ~ISL38XX_CTRL_STAT_RESET;
    writel(reg, device_base + ISL38XX_CTRL_STAT_REG);
    udelay(ISL38XX_WRITEIO_DELAY);

    // now the last step is to reset the interface
    isl38xx_interface_reset( device_base, host_address );

    return 0;
}

