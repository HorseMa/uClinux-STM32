/* $Header$
 *
 * asm/arch-armnommu/arch-isl3893/dma.h:
 *         isl3893-specific macros for DMA support.
 * copyright:
 *         (C) 2001 RidgeRun, Inc. (http://www.ridgerun.com)
 * author: Gordon McNutt <gmcnutt@ridgerun.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

/*
 * MAX_DMA_ADDRESS -- max address we can program into the DMA address registers?
 * Not sure. Various drivers use this value as a test before setting up a DMA.
 * Also, the macros in linux/bootmem.h use this value as a goal for many of the
 * bootmem allocations.
 * Open questions:
 * a) the DMA address register is only 26 bits -- does this limit us to this
 *    section of memory, or can we only DMA in certain size chunks?
 * b) can the ARM processor even directly program the DMA?
 * I'm going to go out on a limb and assume that the DMA range is the first
 * addresseable 26 bits.
 * --gmcnutt
 */
#define MAX_DMA_ADDRESS		0x80000000

/*
 * MAX_DMA_CHANNELS -- # of DMA control register sets we possess. For the dsc21
 * we have one that can run in either direction and between two different
 * destinations.
 * --gmcnutt
 */
#define MAX_DMA_CHANNELS        4

/* Register values */

/* - CONTROL */
#define	DMACONTROL			(0x0)
#define	DMACONTROLDRQIntEn		(0x4)
#define	DMACONTROLBAUIntEn		(0x2)
#define	DMACONTROLEnable		(0x1)
#define	DMACONTROLMask			(0x7)
#define	DMACONTROLTestMask		(0x6)
#define	DMACONTROLInitialValue		(0x0)
#define	DMACONTROLAccessType		(RW)

/* - MAXCNT */
#define	DMAMAXCNT			(0x4)
#define	DMAMAXCNTMask			(0xffff)
#define	DMAMAXCNTInitialValue		(0x0)
#define	DMAMAXCNTTestMask		(0xffff)
#define	DMAMAXCNTAccessType		(RW)

/* - BASE */
#define	DMABASE				(0x8)
#define	DMABASEMask			(0x1fffffff)
#define	DMABASEInitialValue		(0x0)
#define	DMABASETestMask			(0x1fffffff)
#define	DMABASEAccessType		(RW)

/* - STATUS */
#define	DMASTATUS			(0xc)
#define	DMASTATUSBytes			(0x1f0)
#define	DMASTATUSBytesShift		(0x4)
#define	DMASTATUSTranErr		(0x8)
#define	DMASTATUSDRQ			(0x4)
#define	DMASTATUSBAUInt			(0x2)
#define	DMASTATUSStallInt		(0x1)
#define	DMASTATUSMask			(0x1ff)
#define	DMASTATUSAccessType		(RO)
#define	DMASTATUSTestMask		(0x1fb)
#define	DMASTATUSInitialValue		(0x0)

/* - CURRENT */
#define	DMACURRENT			(0x10)
#define	DMACURRENTMask			(0x1fffffff)
#define	DMACURRENTAccessType		(RO)
#define	DMACURRENTInitialValue		(0x0)
#define	DMACURRENTTestMask		(0x1fffffff)

/* - REMAIN */
#define	DMAREMAIN			(0x14)
#define	DMAREMAINMask			(0xffff)
#define	DMAREMAINAccessType		(RO)
#define	DMAREMAINInitialValue		(0x0)
#define	DMAREMAINTestMask		(0xffff)

/* - DATA */
#define	DMADATA				(0x18)
#define	DMADATAEnd			(0x100)
#define	DMADATAData			(0xff)
#define	DMADATADataShift		(0x0)
#define	DMADATAMask			(0x1ff)
// #define	DMADATAAccessType		(NO_TEST)
#define	DMADATAInitialValue		(0x0)
#define	DMADATATestMask			(0x1ff)


typedef struct s_DMA {
 unsigned CONTROL; /* @0 */
 unsigned MAXCNT; /* @4 */
 unsigned BASE; /* @8 */
 unsigned STATUS; /* @12 */
 unsigned CURRENT; /* @16 */
 unsigned REMAIN; /* @20 */
 unsigned fill0;
 unsigned DATA; /* @28 */
} s_DMA;

#define uHostTxDma     ((volatile struct s_DMA *) 0xc0000f00)
#define uHostRxDma     ((volatile struct s_DMA *) 0xc0000f20)
#define uBasebandTxDma ((volatile struct s_DMA *) 0xc0000f40)
#define uBasebandRxDma ((volatile struct s_DMA *) 0xc0000f60)
#define uWEPTxDma      ((volatile struct s_DMA *) 0xc0000f80)
#define uWEPRxDma      ((volatile struct s_DMA *) 0xc0000fa0)


/*
 * arch_dma_init -- called by arch/armnommu/kernel/dma.c init_dma.
 * Don't know what's needed here (if anything).
 * --gmcnutt
 */
#define arch_dma_init(dma_chan)

#endif /* _ASM_ARCH_DMA_H */
