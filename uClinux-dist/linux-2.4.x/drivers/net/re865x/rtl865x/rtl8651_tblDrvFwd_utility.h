/*
*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program :	rtl8651_tblDrvFwd_utility.h
* Abstract :	External Header file to store utilities used by forwarding engine related modules
* Creator :	Yi-Lun Chen (chenyl@realtek.com.tw)
* Author :	Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/

#ifndef _RTL8651_TBLDRVFWD_UTILITY_H
#define _RTL8651_TBLDRVFWD_UTILITY_H

/* =======================================================================================
	memory process
    ======================================================================================== */
#define BZERO_VAR(var) \
		do { \
			memset(&(var), 0, sizeof((var))); \
		} while (0);
#define BZERO_ARRAY(arr) \
		do { \
			memset((arr), 0, sizeof((arr))); \
		} while (0);
#define BZERO_PTR(ptr,size) \
		do { \
			if (size) { \
				memset(ptr, 0, size); \
			} \
		} while (0);
#define MEM_ALLOCATE_PTR(ptr,size) \
		do { \
			ptr = (void*)rtlglue_malloc(size); \
			if ( ptr ) \
				BZERO_PTR(ptr, size); \
		} while (0);

/* =======================================================================================
	Linked List
    ======================================================================================== */


#define HTLIST_ADD2TAIL(entry, next, prev, hdr, tail) \
	do { \
		if (tail) \
		{ \
			tail->next = entry; \
		} else \
		{ \
			hdr = entry; \
		} \
		entry->prev = tail; \
		entry->next = NULL; \
		tail = entry; \
	} while (0);

#define HTLIST_REMOVE(entry, next, prev, hdr, tail) \
	do { \
		if (entry->next) \
		{ \
			entry->next->prev = entry->prev; \
		} \
		if (entry->prev) \
		{ \
			entry->prev->next = entry->next; \
		} \
		if (hdr == entry) \
		{ \
			hdr = entry->next; \
		} \
		if (tail == entry) \
		{ \
			tail = entry->prev; \
		} \
		entry->next = entry->prev = NULL; \
	} while (0);

#define HTLIST_MOVE2TAIL(entry, next, prev, hdr, tail) \
	do { \
		HTLIST_REMOVE(entry, next, prev, hdr, tail); \
		HTLIST_ADD2TAIL(entry, next, prev, hdr, tail); \
	} while (0);

#define HLIST_ADD2HDR(entry, next, prev, hdr) \
	do { \
		if (hdr) \
		{ \
			hdr->prev = entry; \
		} \
		entry->next = hdr; \
		entry->prev = NULL; \
		hdr = entry; \
	} while (0);

#define HLIST_REMOVE(entry, next, prev, hdr) \
	do { \
		if (entry->next) \
		{ \
			entry->next->prev = entry->prev; \
		} \
		if (entry->prev) \
		{ \
			entry->prev->next = entry->next; \
		} \
		if (hdr == entry) \
		{ \
			hdr = entry->next; \
		} \
		entry->next = entry->prev = NULL; \
	} while (0);

/* =======================================================================================
	Linked List - type 2
    ======================================================================================== */
/* <- For free list control -> */
#define _GET_FROM_FREELIST(entry,freeList) \
		(entry)=(freeList); \
		if (freeList) {(freeList)=(freeList)->next;}

#define _PUT_TO_FREELIST(entry,freeList) \
		(entry)->next=(freeList); \
		(freeList)=(entry);

/* <- For normal Link/Unlink -> */
#define _NORMAL_LINK(ItemHdr,LinkItem) \
		do { \
			(LinkItem)->hdr=&(ItemHdr); \
			(LinkItem)->next=(ItemHdr); \
			(LinkItem)->prev=NULL; \
			if (ItemHdr){((ItemHdr)->prev=(LinkItem));}\
			(ItemHdr)=(LinkItem); \
		} while (0);

#define _NORMAL_UNLINK(UnlinkItem) \
		do { \
			if ((UnlinkItem)->prev) {(UnlinkItem)->prev->next=(UnlinkItem)->next;} \
			if ((UnlinkItem)->next) {(UnlinkItem)->next->prev=(UnlinkItem)->prev;} \
			if (*((UnlinkItem)->hdr)==(UnlinkItem)) {(*((UnlinkItem)->hdr)=(UnlinkItem)->next);} \
		} while (0);

/* <- For user defined Link/Unlink -> */
#define _USR_DEFINE_LINK(ItemHdr,LinkItem,identify) \
		do { \
			(LinkItem)->identify##_hdr=&(ItemHdr); \
			(LinkItem)->identify##_next=(ItemHdr); \
			(LinkItem)->identify##_prev=NULL; \
			if (ItemHdr){((ItemHdr)->identify##_prev=(LinkItem));}\
			(ItemHdr)=(LinkItem); \
		} while (0);

#define _USR_DEFINE_UNLINK(UnlinkItem,identify) \
		do { \
			if ((UnlinkItem)->identify##_prev) {(UnlinkItem)->identify##_prev->identify##_next=(UnlinkItem)->identify##_next;} \
			if ((UnlinkItem)->identify##_next) {(UnlinkItem)->identify##_next->identify##_prev=(UnlinkItem)->identify##_prev;} \
			if (*((UnlinkItem)->identify##_hdr)==(UnlinkItem)) {(*((UnlinkItem)->identify##_hdr)=(UnlinkItem)->identify##_next);} \
		} while (0);


#endif /* _RTL8651_TBLDRVFWD_UTILITY_H */
