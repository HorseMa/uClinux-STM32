#ifndef _ASM_MMU_CONTEXT_H
#define _ASM_MMU_CONTEXT_H

#include <linux/config.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/pgalloc.h>

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}

#ifndef CONFIG_UCLINUX
extern int init_new_context(struct task_struct *tsk, struct mm_struct *mm);
extern void change_mm_context(mm_context_t *old, mm_context_t *ctx, pgd_t *_pgd);
extern void destroy_context(struct mm_struct *mm);

#else
#define init_new_context(tsk,mm)	({ 0; })
#define change_mm_context(old,ctx,_pgd)	do {} while(0)
#define destroy_context(mm)		do {} while(0)
#endif

#define switch_mm(prev, next, tsk, cpu)						\
do {										\
	if (prev != next)							\
		change_mm_context(&prev->context, &next->context, next->pgd);	\
} while(0)

#define activate_mm(prev, next)						\
do {									\
	change_mm_context(&prev->context, &next->context, next->pgd);	\
} while(0)

#endif
