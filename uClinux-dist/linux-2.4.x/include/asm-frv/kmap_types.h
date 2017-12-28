                                                                                
#ifndef _ASM_KMAP_TYPES_H
#define _ASM_KMAP_TYPES_H
 
enum km_type {
	/* arch specific kmaps - change the numbers attached to these at your peril */
	__KM_CACHE,		/* cache flush page attachment point */
	__KM_PGD,		/* current page directory */
	__KM_ITLB_PTD,		/* current instruction TLB miss page table lookup */
	__KM_DTLB_PTD,		/* current data TLB miss page table lookup */

	/* general kmaps */
        KM_BOUNCE_READ,
        KM_SKB_SUNRPC_DATA,
        KM_SKB_DATA_SOFTIRQ,
        KM_USER0,
        KM_USER1,
        KM_BH_IRQ,
        KM_SOFTIRQ0,
        KM_SOFTIRQ1,
        KM_TYPE_NR
};
 
#endif
