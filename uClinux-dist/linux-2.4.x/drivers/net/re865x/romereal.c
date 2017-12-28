/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : Packet Capturer of ROME Driver
* Abstract : 
* Author : Yung-Chieh Lo (yjlou@realtek.com.tw)               
* $Id: romereal.c,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $
*/
#include "romereal.h"
#include "rtl865x/rtl_glue.h"
#define KERNEL_SYSCALLS 
#include <asm/unistd.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

/* Global variables */
int8 rtl8651_romereal_enabled = FALSE; 
uint32 rtl8651_romereal_filterMask = 0xffffffff;
rtl8651_frameHdr_t *rtl8651_romereal_first = NULL;
void *rtl8651_romereal_buffer = NULL;
uint32 rtl8651_romereal_bufferSize = 0;


int32 rtl8651_romerealInit( void* buffer, uint32 size, uint32 mode )
{
	if ( mode == ROMEREAL_MODE_CIRCULAR ) return FAILED;

	rtl8651_romereal_enabled = FALSE;
	rtl8651_romereal_filterMask = 0xffffffff;
	rtl8651_romereal_first = NULL;
	rtl8651_romereal_buffer = buffer;
	rtl8651_romereal_bufferSize = size;

	return SUCCESS;
}

int32 rtl8651_romerealEnable( int8 enable )
{
	rtl8651_romereal_enabled = enable;
	return SUCCESS;
}

int32 rtl8651_romerealRecord( PKTHDR* pkt, uint32 recordPoint )
{
	int32 isVeryFirstOne;
	rtl8651_frameHdr_t *curr = NULL;
	rtl8651_frameHdr_t *newFrame;
	rtl8651_frameHdr_t tmp;

	/* not inited */
	if ( rtl8651_romereal_buffer == NULL ) return FAILED;

	/* Function disabled */
	if ( rtl8651_romereal_enabled == FALSE ) return SUCCESS;

	/* We have no interest about this packet. */
	if ( ( recordPoint & rtl8651_romereal_filterMask ) == 0 ) return SUCCESS;

	if ( rtl8651_romereal_first == NULL )
	{
		/* buffer is empty, we now insert first one. */
		isVeryFirstOne = TRUE;
		newFrame = rtl8651_romereal_buffer;
	}
	else
	{
		isVeryFirstOne = FALSE;
		/* find out the last packet in buffer */
		for( curr = rtl8651_romereal_first;
		     curr->next;
		     curr = curr->next );

		/* point to new-allocated frame */
		newFrame = (rtl8651_frameHdr_t*) UP_TO_4BYTE_ALIGN( ((char*)curr) + ROMEREAL_FRAMESIZE( curr ) );
	}

	tmp.timestamp = (((uint64)getmstime())<<32) + recordPoint;
	tmp.capturedSize = UP_TO_4BYTE_ALIGN(pkt->ph_len);
	tmp.actualSize = pkt->ph_len;
	tmp.recordPoint = recordPoint;
	tmp.next = NULL;

	if ( UP_TO_4BYTE_ALIGN( ((char*)newFrame) + ROMEREAL_FRAMESIZE( &tmp ) ) - (uint32)rtl8651_romereal_buffer > rtl8651_romereal_bufferSize )
	{
		/* If we record this packet, the buffer will run out. */
		return FAILED;
	}
	else
	{
		/* Good, there is still enough space to record this packet. */
		if ( isVeryFirstOne )
		{
			/* set this packet as the first packet in list. */
			rtl8651_romereal_first = rtl8651_romereal_buffer;
		}
		else
		{
			/* link this packet to the end of list. */
			if ( curr == NULL ) return FAILED;
			curr->next = newFrame;
		}
	}

	/* Append this packet */
	memcpy( newFrame, &tmp, sizeof(tmp) );
	newFrame->pktHdr = (PKTHDR*)(newFrame+1);
	newFrame->mbuf = (MBUF*)(newFrame->pktHdr+1);
	newFrame->cluster = (char*)(newFrame->mbuf+1);
	/* save p-m-c */
	memcpy( newFrame->pktHdr, pkt, sizeof(*pkt) );
	memcpy( newFrame->mbuf, pkt->ph_mbuf, sizeof(*pkt->ph_mbuf) );
	memcpy( newFrame->cluster, pkt->ph_mbuf->m_data, newFrame->capturedSize );

#if 0
	rtlglue_printf( "=== newFrame === %p ================\n", newFrame );
	rtlglue_printf( "timestamp   : %d\n", newFrame->timestamp );
	rtlglue_printf( "capturedSize: %d\n", newFrame->capturedSize );
	rtlglue_printf( "actualSize  : %d\n", newFrame->actualSize );
	rtlglue_printf( "recordPoint : %d\n", newFrame->recordPoint );
	rtlglue_printf( "pktHdr      : %p\n", newFrame->pktHdr );
	rtlglue_printf( "mbuf        : %p\n", newFrame->mbuf );
	rtlglue_printf( "cluster     : %p\n", newFrame->cluster );
	rtlglue_printf( "            : %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-...\n",
	        newFrame->mbuf->m_data[0], newFrame->mbuf->m_data[1],
	        newFrame->mbuf->m_data[2], newFrame->mbuf->m_data[3],
	        newFrame->mbuf->m_data[4], newFrame->mbuf->m_data[5],
	        newFrame->mbuf->m_data[6], newFrame->mbuf->m_data[7] );
	rtlglue_printf( "next        : %p\n", newFrame->next );
#endif /*0*/

	return SUCCESS;
}

rtl8651_frameHdr_t* rtl8651_romerealNext( rtl8651_frameHdr_t* curr )
{
	if ( curr == NULL )
	{
		return rtl8651_romereal_first;
	}
	else
	{
		return curr->next;
	}
}

int32 rtl8651_romerealFilter( uint32 mask )
{
	rtl8651_romereal_filterMask = mask;

	return SUCCESS;
}


/* kernel-to-kernel system call */
int k2kOpen(const char *path, int flags, int mode )
{
	int ret;
	mm_segment_t fs;
	fs = get_fs();     /* save previous value */
	set_fs(get_ds()); /* use kernel limit */
	ret = sys_open( path, flags, mode );
	set_fs(fs);        /* recover value*/
	return ret;
}

int k2kWrite(int fd, void* buf, int nbytes)
{
	int ret;
	mm_segment_t fs;
	fs = get_fs();     /* save previous value */
	set_fs(get_ds()); /* use kernel limit */
	ret = sys_write( fd, buf, nbytes );
	set_fs(fs);        /* recover value*/
	return ret;
}

int k2kClose(int fd)
{
	int ret;
	mm_segment_t fs;
	fs = get_fs();     /* save previous value */
	set_fs(get_ds()); /* use kernel limit */
	ret = sys_close( fd );
	set_fs(fs);        /* recover value*/
	return ret;
}


int32 rtl8651_romerealSaveAsFile( char* filename )
{
	int fd;
	rtl8651_fileHdr_t fileHdr;
	rtl8651_frameHdr_t *frame;
	int pktCnt;

	fd = k2kOpen( filename, O_WRONLY|O_CREAT|O_TRUNC, 0644 );
	if ( fd == -1 )
	{
		rtlglue_printf( "open(%s) for write error\n", filename );
		return FAILED;
	}

	fileHdr.magicNum = 0xa1b2c3d4;
	fileHdr.majorVer = 0x0002;
	fileHdr.minorVer = 0x0004;
	fileHdr.timezoneOffset = 0;
	fileHdr.timestampAccu = 0;
	fileHdr.snapshotLen = 0xffff;
	fileHdr.linkLayerType = 1; /* Ethernet */

	if ( -1 == k2kWrite( fd, &fileHdr, sizeof(fileHdr) ) ) { rtlglue_printf( "write(fileHdr) error\n" ); k2kClose( fd ); return FAILED; }

	for( frame = rtl8651_romerealNext( NULL ), pktCnt = 0;
	     frame;
	     frame = rtl8651_romerealNext( frame ), pktCnt++ )
	{
#if 0
		rtlglue_printf( "=== frame === %p ================\n", frame );
		rtlglue_printf( "timestamp   : %llu\n", frame->timestamp );
		rtlglue_printf( "capturedSize: %d\n", frame->capturedSize );
		rtlglue_printf( "actualSize  : %d\n", frame->actualSize );
		rtlglue_printf( "recordPoint : %d\n", frame->recordPoint );
		rtlglue_printf( "pktHdr      : %p\n", frame->pktHdr );
		rtlglue_printf( "mbuf        : %p\n", frame->mbuf );
		rtlglue_printf( "cluster     : %p\n", frame->cluster );
		rtlglue_printf( "            : %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-...\n", 
		                frame->cluster[0], frame->cluster[1],
		                frame->cluster[2], frame->cluster[3],
		                frame->cluster[4], frame->cluster[5],
		                frame->cluster[6], frame->cluster[7] );
		rtlglue_printf( "next        : %p\n", frame->next );
#endif/*0*/

		if ( -1 == k2kWrite( fd, &frame->timestamp, sizeof(frame->timestamp) ) ) { rtlglue_printf( "write(frame) error\n" ); k2kClose( fd ); return FAILED; }
		if ( -1 == k2kWrite( fd, &frame->capturedSize, sizeof(frame->capturedSize) ) ) { rtlglue_printf( "write(frame) error\n" ); k2kClose( fd ); return FAILED; }
		if ( -1 == k2kWrite( fd, &frame->actualSize, sizeof(frame->actualSize) ) ) { rtlglue_printf( "write(frame) error\n" ); k2kClose( fd ); return FAILED; }
		if ( -1 == k2kWrite( fd, frame->cluster, frame->capturedSize ) ) { rtlglue_printf( "write(frame) error\n" ); k2kClose( fd ); return FAILED; }
	}

	k2kClose( fd );

	rtlglue_printf("**********************************\n");
	rtlglue_printf("File saved: %s\n", filename );
	rtlglue_printf("Total Packet#: %d\n", pktCnt );

	return SUCCESS;
}

int32 rtl8651_romerealDump()
{
	int count = 0;
	rtl8651_frameHdr_t *frame;

	for( frame = rtl8651_romerealNext( NULL );
	     frame;
	     frame = rtl8651_romerealNext( frame ) )
	{
		rtlglue_printf( "=== frame === %p ================\n", frame );
		rtlglue_printf( "timestamp   : %llu\n", frame->timestamp );
		rtlglue_printf( "capturedSize: %d\n", frame->capturedSize );
		rtlglue_printf( "actualSize  : %d\n", frame->actualSize );
		rtlglue_printf( "recordPoint : %d\n", frame->recordPoint );
		rtlglue_printf( "pktHdr      : %p\n", frame->pktHdr );
		rtlglue_printf( "mbuf        : %p\n", frame->mbuf );
		rtlglue_printf( "cluster     : %p\n", frame->cluster );
		rtlglue_printf( "            : %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x=%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
		                frame->cluster[0x00], frame->cluster[0x01], frame->cluster[0x02], frame->cluster[0x03],
		                frame->cluster[0x04], frame->cluster[0x05], frame->cluster[0x06], frame->cluster[0x07],
		                frame->cluster[0x08], frame->cluster[0x09], frame->cluster[0x0A], frame->cluster[0x0B],
		                frame->cluster[0x0C], frame->cluster[0x0D], frame->cluster[0x0E], frame->cluster[0x0F] );
		rtlglue_printf( "            : %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x=%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
		                frame->cluster[0x10], frame->cluster[0x11], frame->cluster[0x12], frame->cluster[0x13],
		                frame->cluster[0x14], frame->cluster[0x15], frame->cluster[0x16], frame->cluster[0x17],
		                frame->cluster[0x18], frame->cluster[0x19], frame->cluster[0x1A], frame->cluster[0x1B],
		                frame->cluster[0x1C], frame->cluster[0x1D], frame->cluster[0x1E], frame->cluster[0x1F] );
		rtlglue_printf( "            : %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x=%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
		                frame->cluster[0x20], frame->cluster[0x21], frame->cluster[0x22], frame->cluster[0x23],
		                frame->cluster[0x24], frame->cluster[0x25], frame->cluster[0x26], frame->cluster[0x27],
		                frame->cluster[0x28], frame->cluster[0x29], frame->cluster[0x2A], frame->cluster[0x2B],
		                frame->cluster[0x2C], frame->cluster[0x2D], frame->cluster[0x2E], frame->cluster[0x2F] );
		rtlglue_printf( "            : %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x=%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
		                frame->cluster[0x30], frame->cluster[0x31], frame->cluster[0x32], frame->cluster[0x33],
		                frame->cluster[0x34], frame->cluster[0x35], frame->cluster[0x36], frame->cluster[0x37],
		                frame->cluster[0x38], frame->cluster[0x39], frame->cluster[0x3A], frame->cluster[0x3B],
		                frame->cluster[0x3C], frame->cluster[0x3D], frame->cluster[0x3E], frame->cluster[0x3F] );
		rtlglue_printf( "next        : %p\n", frame->next );

		count++;
	}

	rtlglue_printf("**********************************\n");
	rtlglue_printf("Total Count: %d\n", count );

	return SUCCESS;
}
