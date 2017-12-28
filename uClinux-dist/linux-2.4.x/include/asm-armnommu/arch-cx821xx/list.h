/*******************************************************************************
****	Copyright (c) 1998
****	Rockwell Semiconductor Systems
****	Personal Computing Division
****	All Rights Reserved
****
*******************************************************************************
**
**	MODULE NAME:
**		List Handling
**
**	FILE NAME:
**		list.h
**
**	ABSTRACT:
**		Queues, stacks, travels linked lists.
**
**	DETAILS:
**		Maintains double linked lists. You must declare an ANCHOR of type LIST_LINK_T
**		to begin a list. Embed a LIST_LINK_T in each object you wish to link into
**		the list.
**		An empty list has an anchor that points to itself. This simplifies dealing
**		with lists by avoiding any problems with dereferncing 0 as might happen
**		if NULL pointers are used.
**		To traverse a list, get the next link after the anchor as a beginning
**		point. Process each item, getting another item, checking for when the
**		next function returns a pointer to the anchor to terminate the traversal.
**
*******************************************************************************/
#ifndef _LIST_H
#define _LIST_H

#ifdef _MSC_VER
	#define GLOBAL
	typedef enum {False, True}				BOOLEAN ;
#else
#include "Common.h"
#endif

typedef struct LIST_LINK_S
{
	struct LIST_LINK_S *BwdPtr ;
	struct LIST_LINK_S *FwdPtr ;
} LIST_LINK_T ;

GLOBAL __inline void 		 ListInit 		( LIST_LINK_T *Anchor ) ;
GLOBAL __inline LIST_LINK_T	*ListNext		( LIST_LINK_T *Item ) ;
GLOBAL __inline LIST_LINK_T	*ListPrev		( LIST_LINK_T *Item ) ;
GLOBAL __inline void		 ListInsertNext	( LIST_LINK_T *List, LIST_LINK_T *Item ) ;
GLOBAL __inline void		 ListInsertPrev	( LIST_LINK_T *List, LIST_LINK_T *Item ) ;
GLOBAL __inline LIST_LINK_T *ListRemove		( LIST_LINK_T *Item ) ;
GLOBAL __inline void 		 ListPush		( LIST_LINK_T *Anchor, LIST_LINK_T *Item ) ;
GLOBAL __inline LIST_LINK_T	*ListPop		( LIST_LINK_T *Anchor ) ;
GLOBAL __inline void		 ListEnqueue	( LIST_LINK_T *Anchor, LIST_LINK_T *Item ) ;
GLOBAL __inline LIST_LINK_T	*ListDequeue	( LIST_LINK_T *Anchor ) ;
GLOBAL __inline BOOLEAN		 ListIsEmpty	( LIST_LINK_T *Anchor ) ;

// include the function definitions since they are inline
#include "list.c"

#endif
