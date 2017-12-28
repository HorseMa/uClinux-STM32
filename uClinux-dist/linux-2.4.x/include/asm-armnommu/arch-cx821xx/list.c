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
**		list.c
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
**		Use MS compiler option /Ob1 to make functions inline
**
*******************************************************************************/
#include "list.h"
#include <stddef.h>


/******************************************************************************
FUNCTION NAME:
		ListInit

	ABSTRACT:
		Initializes the list.

	RETURN:
		void

	DETAILS:
  		Must be called before any other function

******************************************************************************/
GLOBAL __inline void 		 ListInit 		( LIST_LINK_T *Anchor )
{
	Anchor->FwdPtr =
	Anchor->BwdPtr = Anchor ;
}



/******************************************************************************
	FUNCTION NAME:
		ListNext

	ABSTRACT:
		returns the successive item on the list.

	RETURN:
		Next Item

	DETAILS:

******************************************************************************/
GLOBAL __inline LIST_LINK_T	*ListNext		( LIST_LINK_T *Item )
{
	return Item->FwdPtr ;
}



/******************************************************************************
	FUNCTION NAME:
		ListPrev

	ABSTRACT:
		returns the previous item on the list.

	RETURN:
		Next Item

	DETAILS:

******************************************************************************/
GLOBAL __inline LIST_LINK_T	*ListPrev		( LIST_LINK_T *Item )
{
	return Item->BwdPtr ;
}



/******************************************************************************
	FUNCTION NAME:
		ListInsertNext

	ABSTRACT:
		Inserts the new item AFTER the current item on the list.

	RETURN:
		void

	DETAILS:

******************************************************************************/
GLOBAL __inline void		 ListInsertNext	( LIST_LINK_T *List, LIST_LINK_T *Item )
{
	Item->BwdPtr = List ;
	Item->FwdPtr = List->FwdPtr ;
	List->FwdPtr->BwdPtr = Item ;
	List->FwdPtr = Item ;
}

/******************************************************************************
	FUNCTION NAME:
		ListInsertPrev

	ABSTRACT:
		Inserts the new item BEFORE the current item on the list.

	RETURN:
		void

	DETAILS:

******************************************************************************/
GLOBAL __inline void		 ListInsertPrev	( LIST_LINK_T *List, LIST_LINK_T *Item )
{
	Item->FwdPtr = List  ;
	Item->BwdPtr = List->BwdPtr ;
	List->BwdPtr->FwdPtr = Item ;
	List->BwdPtr = Item ;
}



/******************************************************************************
	FUNCTION NAME:
		ListRemoveItem

	ABSTRACT:
		removes and returns the item from the list.

	RETURN:
		Item

	DETAILS:

******************************************************************************/
GLOBAL __inline LIST_LINK_T	*ListRemove		( LIST_LINK_T *Item )
{
	Item->BwdPtr->FwdPtr = Item->FwdPtr ;
	Item->FwdPtr->BwdPtr = Item->BwdPtr ;
	#if DEBUG
	Item->FwdPtr = NULL ;
	Item->BwdPtr = NULL ;
	#endif
	return Item ;
}



/******************************************************************************
	FUNCTION NAME:
		ListPush

	ABSTRACT:
		Pushes the new item on the list.

	RETURN:
		void

	DETAILS:
		When used in conjunction with ListPop, items are removed in LIFO order.

******************************************************************************/
GLOBAL __inline void 		 ListPush		( LIST_LINK_T *Anchor, LIST_LINK_T *Item )
{
	ListInsertNext ( Anchor, Item ) ;
}



/******************************************************************************
	FUNCTION NAME:
		ListPop

	ABSTRACT:
		removes and returns the last item pushed on the list.

	RETURN:
		Item

	DETAILS:
		When used in conjunction with ListPush, items are removed in LIFO order.

******************************************************************************/
GLOBAL __inline LIST_LINK_T	*ListPop		( LIST_LINK_T *Anchor )
{
	return ListRemove ( Anchor->FwdPtr ) ;
}



/******************************************************************************
	FUNCTION NAME:
		ListEnqueue

	ABSTRACT:
		Queues the new item on the list.

	RETURN:
		void

	DETAILS:
		When used in conjunction with ListDequeue, items are removed in FIFO order.

******************************************************************************/
GLOBAL __inline void		 ListEnqueue	( LIST_LINK_T *Anchor, LIST_LINK_T *Item )
{
	ListInsertPrev ( Anchor, Item ) ;
}



/******************************************************************************
	FUNCTION NAME:
		ListDequeue

	ABSTRACT:
		removes and returns the oldest item queued on the list.

	RETURN:
		Item

	DETAILS:
		When used in conjunction with ListQueue, items are removed in FIFO order.

******************************************************************************/
GLOBAL __inline LIST_LINK_T	*ListDequeue	( LIST_LINK_T *Anchor )
{
	return ListRemove ( Anchor->FwdPtr ) ;
}



/******************************************************************************
	FUNCTION NAME:
		ListIsEmpty

	ABSTRACT:
		Determines if list is empty.

	RETURN:
		TRUE if empty

	DETAILS:
		.

******************************************************************************/
GLOBAL __inline BOOLEAN	ListIsEmpty	( LIST_LINK_T *Anchor )
{
	return Anchor->FwdPtr == Anchor ;
}



