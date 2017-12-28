/****************************************************************************
 *
 *  Name:		cnxtflashio.c
 *
 *  Description:	Flash read/write support driver for applications.
 *			
 *
 *  Copyright (c) 1999-2002 Conexant Systems, Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/
#include <asm/arch/bsptypes.h>
#include "cnxtflash.h"
#include <asm/arch/hardware.h>
#include <linux/kernel.h> 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <asm/system.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/compatmac.h>

#define UNUSED __attribute__((unused))

#define CNXTFLASHIO_MAJOR		61

#if 0
#define DPRINTK(format, args...) printk("cnxtflashio.c: " format, ##args)
#else
#define DPRINTK(format, args...)
#endif

UINT16 AppFlashSegment[CNXT_FLASH_SEGMENT_SIZE];

static int CnxtFlashIORead( CNXT_FLASH_SEGMENT_E segment, int count );
static int CnxtFlashIOWrite( CNXT_FLASH_SEGMENT_E segment, int count );

static int cnxtflashio_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int cnxtflashio_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t cnxtflashio_read(struct file *file, char *usrbuf, size_t count, loff_t *ppos)
{

	// The device minor number represents the flash segment to be accessed
	CNXT_FLASH_SEGMENT_E minor_num;
	minor_num = (CNXT_FLASH_SEGMENT_E) MINOR( file->f_dentry->d_inode->i_rdev );

	if (count > CNXT_FLASH_SEGMENT_SIZE)
		return -EFAULT;
			
	if(CnxtFlashIORead( minor_num, count ))
	{
		return -EFAULT;
	}
	else 
	{
		if (copy_to_user(usrbuf, &AppFlashSegment, count))
			return -EFAULT;
	}

	return 0;
}

static ssize_t cnxtflashio_write(struct file *file, const char *usrbuf, size_t count, loff_t *ppos)
{

	// The device minor number represents the flash segment to be accessed
	CNXT_FLASH_SEGMENT_E minor_num;
	minor_num = (CNXT_FLASH_SEGMENT_E) MINOR( file->f_dentry->d_inode->i_rdev );

	if (count > CNXT_FLASH_SEGMENT_SIZE)
		return -EFAULT;
	
	// This updates are local copy of the flash segment as well.
	if(copy_from_user( &AppFlashSegment[0], usrbuf, count))
			return -EFAULT;
	
	if(CnxtFlashIOWrite( minor_num, count ) == -1)
		return -EFAULT;
	else
		return 0;
}

static struct file_operations cnxtflashio_fops = {
	owner: THIS_MODULE,
	llseek: NULL,
	open: cnxtflashio_open,
	release: cnxtflashio_release,
	ioctl: NULL,
	read: cnxtflashio_read,
	write: cnxtflashio_write,
	poll: NULL,
	mmap: NULL,
	flush: NULL,
	fsync: NULL,
	fasync: NULL,
};

/*---------------------------------------------------------------------------
 *  Module Function Prototypes
 *-------------------------------------------------------------------------*/
static int __init UNUSED CnxtFlashIOInit( void )
{

	if (register_chrdev(CNXTFLASHIO_MAJOR, "flashio", &cnxtflashio_fops)) {
		printk("Unable to register cnxtflashio driver on %d\n", CNXTFLASHIO_MAJOR);
		return -1;
	}

	// Check the 'application' flash segment. Write 1 byte if its empty
	// (just to calculate a CRC on the segment).
	//CnxtFlashIORead( 1 );

	return 0;
}
/*******************************************************************************
FUNCTION NAME:
	CnxtFlashIOWrite

ABSTRACT:	
	Stores the data into the flash storage segment.

RETURN:
	Status

DETAILS:
*******************************************************************************/
static int CnxtFlashIOWrite( CNXT_FLASH_SEGMENT_E segment, int count )
{

	CNXT_FLASH_STATUS_E flashStatus;

	int Status;

	// Open the flash segment for writing.
	flashStatus = CnxtFlashOpen( segment );
	
#ifdef DEBUG
	printk("AppFlashSegment: %x  &AppFlashSegment: %x\n", 
			AppFlashSegment, &AppFlashSegment
	);
	printk("AppFlashSegment[0]: %x  &AppFlashSegment[0]: %x\n", 
			AppFlashSegment[0], &AppFlashSegment[0]
	);
#endif

	switch( flashStatus )
	{
		// Either a bad CRC or segment is empty, if empty initialize.
		case CNXT_FLASH_DATA_ERROR:


			flashStatus = CnxtFlashWriteRequest( 
				segment, 
				(UINT16 *)&AppFlashSegment[0],
				count
			);
			break;

		case CNXT_FLASH_OWNER_ERROR:
			printk("Wrong owner for requested Flash Segment!\n");
			Status = - 1;

			break;

		default:

			flashStatus = CnxtFlashWriteRequest( 
				segment, 
				(UINT16 *)AppFlashSegment,
				count
			);


			break;
	}

			
	if( flashStatus == CNXT_FLASH_SUCCESS )
	{
		Status = 0;	
		printk("Flash Data Success: CNXT_FLASH_SEGMENT_%d\n", segment+1 );

	} else {
		Status = -1;
		printk("Flash Data Failed: CNXT_FLASH_SEGMENT_%d\n", segment+1 );
	}

	return Status;
}
/*******************************************************************************
FUNCTION NAME:
	CnxtFlashIORead

ABSTRACT:	
	Get the data from the flash storage segment.

RETURN:
	Status

DETAILS:
*******************************************************************************/
static int CnxtFlashIORead( CNXT_FLASH_SEGMENT_E segment, int count )
{

	CNXT_FLASH_STATUS_E flashStatus;

	int Status = 0;

	flashStatus = CnxtFlashOpen( segment );

	switch( flashStatus )
	{
		// Either a bad CRC or segment is empty, if empty initialize.
		case CNXT_FLASH_DATA_ERROR:

			printk("Flash Data Error: CNXT_FLASH_SEGMENT_%d\n", segment+1 );
#if 0
			flashStatus = CnxtFlashWriteRequest( 
				segment, 
				(UINT16 *)&AppFlashSegment[0],
				count
			);
#else
			Status = - 1;
#endif
			break;

		case CNXT_FLASH_OWNER_ERROR:
			printk("Wrong owner for requested Flash Segment!\n");
			Status = - 1;

			break;

		default:
			printk("Flash Data Success: CNXT_FLASH_SEGMENT_%d\n", segment+1 );

			CnxtFlashReadRequest( 
				segment, 
				(UINT16 *)&AppFlashSegment[0],
				count
			);


			break;
	}

	return Status;
}

static void __exit UNUSED CnxtFlashIOExit( void )
{
	unregister_chrdev(CNXTFLASHIO_MAJOR, "cnxtflashio");
}

module_init( CnxtFlashIOInit );
module_exit( CnxtFlashIOExit );






