/****************************************************************************
*
*	Name:			dma_regs.h
*
*	Description:	
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
*					Personal Computing Division
*					All Rights Reserved
*
****************************************************************************
*  $Author:   davidsdj  $
*  $Revision:   1.0  $
*  $Modtime:   Mar 19 2003 12:25:20  $
****************************************************************************/

#ifndef __DMA_REGS_H
#define __DMA_REGS_H


/*******************************************************************************
 * CX821xx: Reg base addr & Reg offset.
 * The reg offset is same for all the sub-devices (like DMAC, EMAC & 
 * Interrupt controller).
 */


#ifndef REG_BASE_ADDR
#define REG_BASE_ADDR	 ((BYTE*)(0x00300000)) /*  */
#endif

#ifndef BE_OFST
#define BE_OFST		 0   /* 0 for little endian, 7 for big  */
#endif

#ifndef REG_OFFSET
#define REG_OFFSET	 (BE_OFST + 0x04) /* double word aligned */
#endif

/*******************************************************************************
 *
 * Define DMAC REGs and macros to access them.
 * To optimize REG accesses, redefine DMA_*_REG_READ and
 * DMA_*_REG_WRITE macros in a wrapper file.
 */

/*
 * Current pointers ( DMA{x}_Ptr1 )
 */

#define DMA_PTR1_BASE_ADDR	    REG_BASE_ADDR
#define DMA_PTR1_REG_OFFSET	    REG_OFFSET	

/* Reg numbering w.r.t base */

#define DMA1_PTR1	0
#define DMA2_PTR1	1
#define DMA3_PTR1	2 /* EMAC Tx channel */
#define DMA4_PTR1	3 /* EMAC Rx channel */
#define DMA5_PTR1	4
#define DMA6_PTR1	5
#define DMA7_PTR1	6
#define DMA8_PTR1	7
#define DMA9_PTR1	8
#define DMA10_PTR1	9

/* Reg access macros */

#define DMA_PTR1_REG_ADDR(regNum)   (DMA_PTR1_BASE_ADDR + \
                                     (regNum) * DMA_PTR1_REG_OFFSET)

#ifndef DMA_PTR1_REG_READ
#define DMA_PTR1_REG_READ(regNum)   (*((volatile ULONG*)DMA_PTR1_REG_ADDR((regNum))))
#endif /* DMA_PTR1_REG_READ */

#ifndef DMA_PTR1_REG_WRITE
#define DMA_PTR1_REG_WRITE(regNum,val)  (*((volatile ULONG*)DMA_PTR1_REG_ADDR((regNum))) = (val))
#endif /* DMA_PTR1_REG_WRITE */


#define DMA4_PTR1_BASE_ADDR	   	0x30000c 
#define DMA4_PTR1_REG_ADDR(regNum)   ((volatile ULONG *)(DMA4_PTR1_BASE_ADDR + \
                                    ((regNum) * (DMA_PTR1_REG_OFFSET))))

#undef DMA4_PTR1_REG_WRITE
#define DMA4_PTR1_REG_WRITE(regNum,val) \
	 (*(DMA4_PTR1_REG_ADDR(regNum)) = (val))



#ifndef DMA_PTR1_REG_UPDATE
#define DMA_PTR1_REG_UPDATE(regNum,val) DMA_PTR1_REG_WRITE((regNum), \
                                        DMA_PTR1_REG_READ((regNum)) | (val))
#endif /* DMA_PTR1_REG_UPDATE */
    
#ifndef DMA_PTR1_REG_RESET
#define DMA_PTR1_REG_RESET(regNum,val)  DMA_PTR1_REG_WRITE((regNum), \
                                        DMA_PTR1_REG_READ((regNum)) & ~(val))
#endif /* DMA_PTR1_REG_RESET */

/*
 * Indirect/Return pointers ( DMA{x}_Ptr2 ). 
 */

#define DMA_PTR2_BASE_ADDR	((BYTE*)0x00300030) /* Base Address */
#define DMA_PTR2_REG_OFFSET	    REG_OFFSET	

/* Reg numbering w.r.t base */

#define DMA1_PTR2	0
#define DMA2_PTR2	1
#define DMA3_PTR2	2 /* EMAC Tx channel */
#define DMA4_PTR2	3 /* EMAC Rx channel */
#define DMA5_PTR2	4
#define DMA6_PTR2	5
#define DMA7_PTR2	6
#define DMA8_PTR2	7
#define DMA9_PTR2	8
#define DMA10_PTR2	9

/* Reg access macros */

#define DMA_PTR2_REG_ADDR(regNum)   (DMA_PTR2_BASE_ADDR + \
                                     (regNum) * DMA_PTR2_REG_OFFSET)

#ifndef DMA_PTR2_REG_READ
#define DMA_PTR2_REG_READ(regNum)       (*((volatile ULONG*)DMA_PTR2_REG_ADDR((regNum))))
#endif /* DMA_PTR2_REG_READ */

#ifndef DMA_PTR2_REG_WRITE
#define DMA_PTR2_REG_WRITE(regNum,val)  (*((volatile ULONG*)DMA_PTR2_REG_ADDR((regNum))) = (val))
#endif /* DMA_PTR2_REG_WRITE */

#ifndef DMA_PTR2_REG_UPDATE
#define DMA_PTR2_REG_UPDATE(regNum,val) DMA_PTR2_REG_WRITE((regNum), \
                                        DMA_PTR2_REG_READ((regNum)) | (val))
#endif /* DMA_PTR2_REG_UPDATE */
    
#ifndef DMA_PTR2_REG_RESET
#define DMA_PTR2_REG_RESET(regNum,val)  DMA_PTR2_REG_WRITE((regNum), \
                                        DMA_PTR2_REG_READ((regNum)) & ~(val))
#endif /* DMA_PTR2_REG_RESET */

/*
 * Buffer size/Pointer mode ( DMA{x}_Cnt1 )
 */

#define DMA_CNT1_BASE_ADDR	((BYTE*)0x00300040) /* Base Address */
#define DMA_CNT1_REG_OFFSET	REG_OFFSET	

/* Reg numbering w.r.t base */

#define DMA1_CNT1	0
#define DMA2_CNT1	1
#define DMA3_CNT1	2 /* EMAC Tx channel */
#define DMA4_CNT1	3 /* EMAC Rx channel */

/* Reg access macros */

#define DMA_CNT1_REG_ADDR(regNum)   (DMA_CNT1_BASE_ADDR + \
                                     (regNum) * DMA_CNT1_REG_OFFSET)

#ifndef DMA_CNT1_REG_READ
#define DMA_CNT1_REG_READ(regNum)       (*((volatile ULONG*)DMA_CNT1_REG_ADDR((regNum))))
#endif /* DMA_CNT1_REG_READ */

#ifndef DMA_CNT1_REG_WRITE
#define DMA_CNT1_REG_WRITE(regNum,val)  (*((volatile ULONG*)DMA_CNT1_REG_ADDR((regNum))) = (val))
#endif /* DMA_CNT1_REG_WRITE */

#ifndef DMA_CNT1_REG_UPDATE
#define DMA_CNT1_REG_UPDATE(regNum,val) DMA_CNT1_REG_WRITE((regNum), \
                                        DMA_CNT1_REG_READ((regNum)) | (val))
#endif /* DMA_CNT1_REG_UPDATE */
    
#ifndef DMA_CNT1_REG_RESET
#define DMA_CNT1_REG_RESET(regNum,val)  DMA_CNT1_REG_WRITE((regNum), \
                                        DMA_CNT1_REG_READ((regNum)) & ~(val))
#endif /* DMA_CNT1_REG_RESET */



/* 
 * DMA addr & count reg macros
 */

/* Get dma addr from physical addr */
#define DMA_ADDR_MASK ((ULONG)0x00ffffff)   /* [23:0] bit dma addr */
#define DMA_ADDR(addr)\
        (DMA_ADDR_MASK & (addr))

/* Get dma cnt from byte cnt */
#if     (MAC_HW_VER < REV_C)
/* AD use for RevA or higher */
#define DMA_CNT_MASK    ((ULONG)0x000007ff) /* [10:0] bit dma cnt */
#else
/* AD use for RevC and higher */
#define DMA_CNT_MASK    ((ULONG)0x00001fff) /* [12:0] bit dma cnt */ 
#endif

/* Specify linked list modes used by dma */
#define DMA_LMODE_MASK  ((ULONG)0x03000000) /* [25:24] bit dma link list mode */
#define DMA_LMODE_TAIL  ((ULONG)0x00000000) /* ptr/cnt at buf-tail */
#define DMA_LMODE_TBL   ((ULONG)0x01000000) /* ptr/cnt at Ptr2(table) */
#define DMA_LMODE_RETN  ((ULONG)0x02000000) /* ptr/cnt at Ptr2(return ptr) */

/* Form dma cnt reg from LMode & count - qwords - */
#define DMA_CNT(lmode,cnt)\
        ((DMA_LMODE_MASK & (lmode)) | (DMA_CNT_MASK & (cnt)))

#endif /* __DMA_REGS_H */
