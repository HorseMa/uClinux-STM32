/*
 * YAFFS: Yet another FFS. A NAND-flash specific file system. 
 * yaffs_fileem.c  NAND emulation on top of files
 *
 * Copyright (C) 2002 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
 //yaffs_fileem.c

#include "yaffs_fileem.h"
#include "yaffs_guts.h"
#include "yaffsinterface.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define FILE_SIZE_IN_MEG 2

// #define YAFFS_ERROR_TESTING 

#define BLOCK_SIZE (32 * 528)
#define BLOCKS_PER_MEG ((1024*1024)/(32 * 512))
#define FILE_SIZE_IN_BLOCKS (FILE_SIZE_IN_MEG * BLOCKS_PER_MEG)
#define FILE_SIZE_IN_BYTES (FILE_SIZE_IN_BLOCKS * BLOCK_SIZE)


static int h;
static __u8 ffChunk[528];

static int eraseDisplayEnabled;

static int markedBadBlocks[] = { 1, 4, -1};

static int IsAMarkedBadBlock(int blk)
{
#if YAFFS_ERROR_TESTING
	int *m = markedBadBlocks;
	
	while(*m >= 0)
	{
		if(*m == blk) return 1;
		m++;
	}
#endif
	return 0;
}


static __u8 yaffs_WriteFailCorruption(int chunkInNAND)
{
#if YAFFS_ERROR_TESTING

	// Whole blocks that fail
	switch(chunkInNAND/YAFFS_CHUNKS_PER_BLOCK)
	{
		case 50:
		case 52:
					return 7;
	}
	
	// Single blocks that fail
	switch(chunkInNAND)
	{
		case 2000:
		case 2003:
		case 3000:
		case 3001:
					return 3;// ding two bits
		case 2001:
		case 3003:
		case 3004:
		case 3005:
		case 3006:
		case 3007:  return 1;// ding one bit
		
		
	}
#endif
	return 0;
}

static void yaffs_ModifyWriteData(int chunkInNAND,__u8 *data)
{
#if YAFFS_ERROR_TESTING
	if(data)
	{
		*data ^= yaffs_WriteFailCorruption(chunkInNAND);	
	}
#endif
}

static __u8 yaffs_ReadFailCorruption(int chunkInNAND)
{
	switch(chunkInNAND)
	{
#if YAFFS_ERROR_TESTING
		case 500:
					return 3;// ding two bits
		case 700:
		case 750:
					return 1;// ding one bit
		
#endif
		default: return 0;
		
	}
}

static void yaffs_ModifyReadData(int chunkInNAND,__u8 *data)
{
#if YAFFS_ERROR_TESTING
	if(data)
	{
		*data ^= yaffs_ReadFailCorruption(chunkInNAND);	
	}
#endif
}





static void  CheckInit(yaffs_Device *dev)
{
	static int initialised = 0;

	int length;
	int nWritten;

	
	if(!initialised)
	{
		memset(ffChunk,0xFF,528);
		
//#ifdef YAFFS_DUMP
//		h = open("yaffs-em-file" , O_RDONLY);
//#else
		h = open("yaffs-em-file" , O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
//#endif
		if(h < 0)
		{
			perror("Fatal error opening yaffs emulation file");
			exit(1);
		}
		initialised = 1;
		
		length = lseek(h,0,SEEK_END);
		nWritten = 528;
		while(length <  FILE_SIZE_IN_BYTES && nWritten == 528)
		{
			write(h,ffChunk,528);
			length = lseek(h,0,SEEK_END);		
		}
		if(nWritten != 528)
		{
			perror("Fatal error expanding yaffs emulation file");
			exit(1);

		}
		
		close(h);
		
#ifdef YAFFS_DUMP
		h = open("yaffs-em-file" , O_RDONLY);
#else
		h = open("yaffs-em-file" , O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
#endif
	}
}

int yaffs_FEWriteChunkToNAND(yaffs_Device *dev,int chunkInNAND,const __u8 *data, yaffs_Spare *spare)
{
	__u8 localData[512];
	
	int pos;
	
	pos = chunkInNAND * 528;
	
	CheckInit(dev);
	
	
	if(data)
	{
		memcpy(localData,data,512);
		yaffs_ModifyWriteData(chunkInNAND,localData);
		lseek(h,pos,SEEK_SET);
		write(h,localData,512);
	}
	
	pos += 512;
	
	if(spare)
	{
		lseek(h,pos,SEEK_SET);
		write(h,spare,16);	
	}

	return YAFFS_OK;
}


int yaffs_FEReadChunkFromNAND(yaffs_Device *dev,int chunkInNAND, __u8 *data, yaffs_Spare *spare)
{
	int pos;

	pos = chunkInNAND * 528;
	
	
	CheckInit(dev);
	
	if(data)
	{
		lseek(h,pos,SEEK_SET);
		read(h,data,512);
		yaffs_ModifyReadData(chunkInNAND,data);
	}
	
	pos += 512;
	
	if(spare)
	{
		lseek(h,pos,SEEK_SET);
		read(h,spare,16);	
	}

	return YAFFS_OK;
}


int yaffs_FEEraseBlockInNAND(yaffs_Device *dev,int blockInNAND)
{
	int i;
	
	CheckInit(dev);
	
	if(eraseDisplayEnabled)
	{
		printf("Erasing block %d\n",blockInNAND);
	}
	
	lseek(h,blockInNAND * BLOCK_SIZE,SEEK_SET);
	for(i = 0; i < 32; i++)
	{
		write(h,ffChunk,528);
	}
	
	switch(blockInNAND)
	{
		case 10: 
		case 15: return YAFFS_FAIL;
		
	}
	return YAFFS_OK;
}

int yaffs_FEInitialiseNAND(yaffs_Device *dev)
{
	return YAFFS_OK;
}
