/*
 * Copyright c                Realtek Semiconductor Corporation, 2002
 * All rights reserved.                                                    
 * 
 * $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/Attic/rtl_flashdrv.h,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $
 *
 * $Author: davidm $
 *
 * Abstract:
 *
 *   Flash driver header file for export include.
 *
 * $Log: rtl_flashdrv.h,v $
 * Revision 1.1.2.1  2007/09/28 14:42:22  davidm
 * #12420
 *
 * Pull in all the appropriate networking files for the RTL8650,  try to
 * clean it up a best as possible so that the files for the build are not
 * all over the tree.  Still lots of crazy dependencies in there though.
 *
 * Revision 1.1  2006/05/19 05:59:52  chenyl
 * *: modified new SDK framework.
 *
 * Revision 1.1  2005/04/19 04:58:15  tony
 * +: BOA web server initial version.
 *
 * Revision 1.2  2004/05/10 10:48:50  yjlou
 * *: porting flashdrv from loader
 *
 * Revision 1.3  2004/03/31 01:49:20  yjlou
 * *: all text files are converted to UNIX format.
 *
 * Revision 1.2  2004/03/22 05:54:55  yjlou
 * +: support two flash chips.
 *
 * Revision 1.1  2004/03/16 06:36:13  yjlou
 * *** empty log message ***
 *
 * Revision 1.1.1.1  2003/09/25 08:16:56  tony
 *  initial loader tree 
 *
 * Revision 1.1.1.1  2003/05/07 08:16:07  danwu
 * no message
 *
 */

#ifndef _RTL_FLASHDRV_H_
#define _RTL_FLASHDRV_H_



typedef struct flashdriver_obj_s {
	uint32  flashSize;
	uint32  flashBaseAddress;
	uint32 *blockBaseArray_P;
	uint32  blockBaseArrayCapacity;
	uint32  blockNumber;
} flashdriver_obj_t;



/*
 * FUNCTION PROTOTYPES
 */
void _init( void );
uint32 flashdrv_getDevSize( void );
uint32 flashdrv_eraseBlock( uint32 ChipSeq, uint32 BlockSeq );
uint32 flashdrv_read (void *dstAddr_P, void *srcAddr_P, uint32 size);
uint32 flashdrv_write(void *dstAddr_P, void *srcAddr_P, uint32 size);



#endif  /* _RTL_FLASHDRV_H_ */

