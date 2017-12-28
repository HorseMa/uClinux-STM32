#ifndef __ASM_SH_SHMPARAM_H
#define __ASM_SH_SHMPARAM_H

#if defined(CONFIG_CPU_SH4)
/*
 * SH-4 has D-cache alias issue
 */
#define	SHMLBA (PAGE_SIZE*4)		 /* attach addr a multiple of this */
#else
#define	SHMLBA PAGE_SIZE		 /* attach addr a multiple of this */
#endif

#endif /* __ASM_SH_SHMPARAM_H */
