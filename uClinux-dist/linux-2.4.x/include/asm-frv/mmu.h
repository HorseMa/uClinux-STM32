#ifndef __MMU_H
#define __MMU_H

typedef struct {
#ifndef CONFIG_UCLINUX
	struct list_head id_link;		/* link in list of context ID owners */
	unsigned short	id;			/* MMU context ID */
	unsigned short	id_busy;		/* true if ID is in CXNR */
	unsigned long	itlb_cached_pge;	/* [SCR0] PGE cached for insn TLB handler */
	unsigned long	itlb_ptd_mapping;	/* [DAMR4] PTD mapping for itlb cached PGE */
	unsigned long	dtlb_cached_pge;	/* [SCR1] PGE cached for data TLB handler */
	unsigned long	dtlb_ptd_mapping;	/* [DAMR5] PTD mapping for dtlb cached PGE */
#endif
} mm_context_t;

#ifndef CONFIG_UCLINUX
extern int __nongpreldata cxn_pinned;
extern int cxn_pin_by_pid(pid_t pid);
#endif

#endif

