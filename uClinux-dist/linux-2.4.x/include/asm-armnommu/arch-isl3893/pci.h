#ifndef __PCI_h
#define __PCI_h

/* Class values */

/* Register values */

/* - ARMIntReg */
#define	PCIARMIntReg			(0x0)
#define	PCIARMIntRegMASTERCOMPLETE	(0x80000000)
#define	PCIARMIntRegPM_STATUS_CHANGE	(0x40000000)
#define	PCIARMIntRegPME_CLEAR_EVENT	(0x20000000)
#define	PCIARMIntRegPME_ENABLE_CHANGE	(0x10000000)
#define	PCIARMIntRegPCI_FATAL_ERROR	(0x8000000)
#define	PCIARMIntRegPCI_PARITY_ERROR	(0x4000000)
#define	PCIARMIntRegARM_ASLEEP		(0x2000000)
#define	PCIARMIntRegPCI_UNDER_RUN	(0x1000000)
#define	PCIARMIntRegCLKRUN_CHANGE	(0x800000)
#define	PCIARMIntRegHOSTMSG		(0xffff)
#define	PCIARMIntRegHOSTMSGShift	(0x0)
#define	PCIARMIntRegMask		(0xff80ffff)
#define	PCIARMIntRegTestMask		(0xefffffff)
#define	PCIARMIntRegAccessType		(RO)
#define	PCIARMIntRegInitialValue	(0x0)

/* - ARMIntAckReg */
#define	PCIARMIntAckReg			(0x4)
#define	PCIARMIntAckRegMASTERCOMPLETE	(0x80000000)
#define	PCIARMIntAckRegPM_STATUS_CHANGE	(0x40000000)
#define	PCIARMIntAckRegPME_CLEAR_EVENT	(0x20000000)
#define	PCIARMIntAckRegPME_ENABLE_CHANGE	(0x10000000)
#define	PCIARMIntAckRegPCI_FATAL_ERROR	(0x8000000)
#define	PCIARMIntAckRegPCI_PARITY_ERROR	(0x4000000)
#define	PCIARMIntAckRegARM_ASLEEP	(0x2000000)
#define	PCIARMIntAckRegHOSTMSG		(0xffff)
#define	PCIARMIntAckRegHOSTMSGShift	(0x0)
#define	PCIARMIntAckRegMask		(0xfe00ffff)
#define	PCIARMIntAckRegAccessType	(WO)
#define	PCIARMIntAckRegInitialValue	(0x0)
#define	PCIARMIntAckRegTestMask		(0xfe00ffff)

/* - ARMIntEnReg */
#define	PCIARMIntEnReg			(0x8)
#define	PCIARMIntEnRegMASTERCOMPLETE	(0x80000000)
#define	PCIARMIntEnRegPM_STATUS_CHANGE	(0x40000000)
#define	PCIARMIntEnRegPME_CLEAR_EVENT	(0x20000000)
#define	PCIARMIntEnRegPME_ENABLE_CHANGE	(0x10000000)
#define	PCIARMIntEnRegPCI_FATAL_ERROR	(0x8000000)
#define	PCIARMIntEnRegPCI_PARITY_ERROR	(0x4000000)
#define	PCIARMIntEnRegARM_ASLEEP	(0x2000000)
#define	PCIARMIntEnRegHOSTMSG		(0xffff)
#define	PCIARMIntEnRegHOSTMSGShift	(0x0)
#define	PCIARMIntEnRegMask		(0xfe00ffff)
#define	PCIARMIntEnRegTestMask		(0xefffffff)
#define	PCIARMIntEnRegAccessType	(RW)
#define	PCIARMIntEnRegInitialValue	(0x0)

/* - Fill0x0C */
#define	PCIFill0x0C			(0xc)
#define	PCIFill0x0CAccessType		(NO_TEST)
#define	PCIFill0x0CInitialValue		(0x0)
#define	PCIFill0x0CMask			(0xffffffff)
#define	PCIFill0x0CTestMask		(0xffffffff)

/* - HostIntReg */
#define	PCIHostIntReg			(0x10)
#define	PCIHostIntRegMASTERCOMPLETE	(0x80000000)
#define	PCIHostIntRegSWRESET		(0x40000000)
#define	PCIHostIntRegTIMERRESET		(0x20000000)
#define	PCIHostIntRegARM_ASLEEP		(0x10000000)
#define	PCIHostIntRegASSERT_PME		(0x8000000)
#define	PCIHostIntRegHOSTMSG		(0xffff)
#define	PCIHostIntRegHOSTMSGShift	(0x0)
#define	PCIHostIntRegMask		(0xf800ffff)
#define	PCIHostIntRegTestMask		(0xffff)
#define	PCIHostIntRegAccessType		(NO_TEST)
#define	PCIHostIntRegInitialValue	(0x0)

/* - HostIntAckReg */
#define	PCIHostIntAckReg		(0x14)
#define	PCIHostIntAckRegMASTERCOMPLETE	(0x80000000)
#define	PCIHostIntAckRegSWRESET		(0x40000000)
#define	PCIHostIntAckRegTIMERRESET	(0x20000000)
#define	PCIHostIntAckRegARM_ASLEEP	(0x10000000)
#define	PCIHostIntAckRegASSERT_PME	(0x8000000)
#define	PCIHostIntAckRegHOSTMSG		(0xffff)
#define	PCIHostIntAckRegHOSTMSGShift	(0x0)
#define	PCIHostIntAckRegMask		(0xf800ffff)
#define	PCIHostIntAckRegAccessType	(NO_TEST)
#define	PCIHostIntAckRegInitialValue	(0x0)
#define	PCIHostIntAckRegTestMask	(0xf800ffff)

/* - HostIntEnReg */
#define	PCIHostIntEnReg			(0x18)
#define	PCIHostIntEnRegMASTERCOMPLETE	(0x80000000)
#define	PCIHostIntEnRegSWRESET		(0x40000000)
#define	PCIHostIntEnRegTIMERRESET	(0x20000000)
#define	PCIHostIntEnRegARM_ASLEEP	(0x10000000)
#define	PCIHostIntEnRegASSERT_PME	(0x8000000)
#define	PCIHostIntEnRegHOSTMSG		(0xffff)
#define	PCIHostIntEnRegHOSTMSGShift	(0x0)
#define	PCIHostIntEnRegMask		(0xf800ffff)
#define	PCIHostIntEnRegAccessType	(RO)
#define	PCIHostIntEnRegInitialValue	(0x0)
#define	PCIHostIntEnRegTestMask		(0xf800ffff)

/* - Fill0x1C */
#define	PCIFill0x1C			(0x1c)
#define	PCIFill0x1CAccessType		(NO_TEST)
#define	PCIFill0x1CInitialValue		(0x0)
#define	PCIFill0x1CMask			(0xffffffff)
#define	PCIFill0x1CTestMask		(0xffffffff)

/* - GenPurpComReg1 */
#define	PCIGenPurpComReg1		(0x20)
#define	PCIGenPurpComReg1InitialValue	(0x12345678)
#define	PCIGenPurpComReg1AccessType	(RW)
#define	PCIGenPurpComReg1Mask		(0xffffffff)
#define	PCIGenPurpComReg1TestMask	(0xffffffff)

/* - GenPurpComReg2 */
#define	PCIGenPurpComReg2		(0x24)
#define	PCIGenPurpComReg2AccessType	(RW)
#define	PCIGenPurpComReg2InitialValue	(0x0)
#define	PCIGenPurpComReg2Mask		(0xffffffff)
#define	PCIGenPurpComReg2TestMask	(0xffffffff)

/* - Fill0x28 */
#define	PCIFill0x28			(0x28)
#define	PCIFill0x28AccessType		(NO_TEST)
#define	PCIFill0x28InitialValue		(0x0)
#define	PCIFill0x28Mask			(0xffffffff)
#define	PCIFill0x28TestMask		(0xffffffff)

/* - Fill0x2C */
#define	PCIFill0x2C			(0x2c)
#define	PCIFill0x2CAccessType		(NO_TEST)
#define	PCIFill0x2CInitialValue		(0x0)
#define	PCIFill0x2CMask			(0xffffffff)
#define	PCIFill0x2CTestMask		(0xffffffff)

/* - DirectMemBase */
#define	PCIDirectMemBase		(0x30)
#define	PCIDirectMemBaseAccessType	(RW)
#define	PCIDirectMemBaseInitialValue	(0x0)
#define	PCIDirectMemBaseMask		(0xffffffff)
#define	PCIDirectMemBaseTestMask	(0xffffffff)

/* - DirectMemEn */
#define	PCIDirectMemEn			(0x34)
#define	PCIDirectMemEnMask		(0xfff)
#define	PCIDirectMemEnInitialValue	(0xfff)
#define	PCIDirectMemEnAccessType	(RW)
#define	PCIDirectMemEnTestMask		(0xfff)

/* - Fill0x38 */
#define	PCIFill0x38			(0x38)
#define	PCIFill0x38AccessType		(NO_TEST)
#define	PCIFill0x38InitialValue		(0x0)
#define	PCIFill0x38Mask			(0xffffffff)
#define	PCIFill0x38TestMask		(0xffffffff)

/* - Fill0x3C */
#define	PCIFill0x3C			(0x3c)
#define	PCIFill0x3CAccessType		(NO_TEST)
#define	PCIFill0x3CInitialValue		(0x0)
#define	PCIFill0x3CMask			(0xffffffff)
#define	PCIFill0x3CTestMask		(0xffffffff)

/* - EventReg */
#define	PCIEventReg			(0x40)
#define	PCIEventRegAccessType		(RO)
#define	PCIEventRegInitialValue		(0x0)
#define	PCIEventRegMask			(0xffffffff)
#define	PCIEventRegTestMask		(0xffffffff)

/* - EventMask */
#define	PCIEventMask			(0x44)
#define	PCIEventMaskAccessType		(RO)
#define	PCIEventMaskInitialValue	(0x0)
#define	PCIEventMaskMask		(0xffffffff)
#define	PCIEventMaskTestMask		(0xffffffff)

/* - PresStateReg */
#define	PCIPresStateReg			(0x48)
#define	PCIPresStateRegINTR		(0x8000)
#define	PCIPresStateRegWKUP		(0x4000)
#define	PCIPresStateRegPWM		(0x40)
#define	PCIPresStateRegBAM		(0x20)
#define	PCIPresStateRegGWAKE		(0x10)
#define	PCIPresStateRegBVD2		(0x8)
#define	PCIPresStateRegBVD1		(0x4)
#define	PCIPresStateRegREADY		(0x2)
#define	PCIPresStateRegWP		(0x1)
#define	PCIPresStateRegMask		(0xc07f)
#define	PCIPresStateRegInitialValue	(0xe)
#define	PCIPresStateRegAccessType	(RW)
#define	PCIPresStateRegTestMask		(0xc07f)

/* - Fill0x4C */
#define	PCIFill0x4C			(0x4c)
#define	PCIFill0x4CAccessType		(NO_TEST)
#define	PCIFill0x4CInitialValue		(0x0)
#define	PCIFill0x4CMask			(0xffffffff)
#define	PCIFill0x4CTestMask		(0xffffffff)

/* - ConfigLoadReg */
#define	PCIConfigLoadReg		(0x50)
#define	PCIConfigLoadRegMask		(0x8000ffff)
#define	PCIConfigLoadRegAccessType	(WO)
#define	PCIConfigLoadRegInitialValue	(0x0)
#define	PCIConfigLoadRegTestMask	(0x8000ffff)

/* - Fill0x54 */
#define	PCIFill0x54			(0x54)
#define	PCIFill0x54AccessType		(NO_TEST)
#define	PCIFill0x54InitialValue		(0x0)
#define	PCIFill0x54Mask			(0xffffffff)
#define	PCIFill0x54TestMask		(0xffffffff)

/* - Fill0x58 */
#define	PCIFill0x58			(0x58)
#define	PCIFill0x58AccessType		(NO_TEST)
#define	PCIFill0x58InitialValue		(0x0)
#define	PCIFill0x58Mask			(0xffffffff)
#define	PCIFill0x58TestMask		(0xffffffff)

/* - Fill0x5C */
#define	PCIFill0x5C			(0x5c)
#define	PCIFill0x5CAccessType		(NO_TEST)
#define	PCIFill0x5CInitialValue		(0x0)
#define	PCIFill0x5CMask			(0xffffffff)
#define	PCIFill0x5CTestMask		(0xffffffff)

/* - MasHostMemAddr */
#define	PCIMasHostMemAddr		(0x60)
#define	PCIMasHostMemAddrAccessType	(RW)
#define	PCIMasHostMemAddrInitialValue	(0x0)
#define	PCIMasHostMemAddrMask		(0xffffffff)
#define	PCIMasHostMemAddrTestMask	(0xffffffff)

/* - MasLenReg */
#define	PCIMasLenReg			(0x64)
#define	PCIMasLenRegMask		(0xff)
#define	PCIMasLenRegAccessType		(RW)
#define	PCIMasLenRegInitialValue	(0x0)
#define	PCIMasLenRegTestMask		(0xff)

/* - MasControlReg */
#define	PCIMasControlReg		(0x68)
#define	PCIMasControlRegPME_ENABLE	(0x40000000)
#define	PCIMasControlRegPOWER_STAT	(0x30000000)
#define	PCIMasControlRegPOWER_STATShift	(0x1c)
#define	PCIMasControlRegPME_CLEAR	(0x8000000)
#define	PCIMasControlRegIGNORELENGTH	(0x80)
#define	PCIMasControlRegUSEDATAREG	(0x10)
#define	PCIMasControlRegTOHOST		(0x8)
#define	PCIMasControlRegENABLE		(0x4)
#define	PCIMasControlRegSpecialMode	(0x2)
#define	PCIMasControlRegNoAddrIncr	(0x1)
#define	PCIMasControlRegMask		(0x7800009f)
#define	PCIMasControlRegTestMask	(0x9c)
#define	PCIMasControlRegAccessType	(RW)
#define	PCIMasControlRegInitialValue	(0x0)

/* - Fill0x6C */
#define	PCIFill0x6C			(0x6c)
#define	PCIFill0x6CAccessType		(NO_TEST)
#define	PCIFill0x6CInitialValue		(0x0)
#define	PCIFill0x6CMask			(0xffffffff)
#define	PCIFill0x6CTestMask		(0xffffffff)

/* - MasDataReg */
#define	PCIMasDataReg			(0x70)
#define	PCIMasDataRegAccessType		(RW)
#define	PCIMasDataRegInitialValue	(0x0)
#define	PCIMasDataRegMask		(0xffffffff)
#define	PCIMasDataRegTestMask		(0xffffffff)

/* - MasOptReg */
#define	PCIMasOptReg			(0x74)
#define	PCIMasOptRegOPTIONENABLE	(0x80000000)
#define	PCIMasOptRegBE			(0xf0)
#define	PCIMasOptRegBEShift		(0x4)
#define	PCIMasOptRegCMD			(0xf)
#define	PCIMasOptRegCMDShift		(0x0)
#define	PCIMasOptRegMask		(0x800000ff)
#define	PCIMasOptRegAccessType		(RW)
#define	PCIMasOptRegInitialValue	(0x0)
#define	PCIMasOptRegTestMask		(0x800000ff)

/* - DevStatReg */
#define	PCIDevStatReg			(0x78)
#define	PCIDevStatRegSetHostOverride	(0x80000000)
#define	PCIDevStatRegSetStartHalted	(0x40000000)
#define	PCIDevStatRegSetRAMBoot		(0x20000000)
#define	PCIDevStatRegSetHostReset	(0x10000000)
#define	PCIDevStatRegSetHostCpuEn	(0x8000000)
#define	PCIDevStatRegSetBootBank	(0x6000000)
#define	PCIDevStatRegSetBootBankShift	(0x19)
#define	PCIDevStatRegSetTargetDMA	(0x1000000)
#define	PCIDevStatRegSetPCIDriveCLKRUN_N	(0x800000)
#define	PCIDevStatRegStartHalted	(0x10000)
#define	PCIDevStatRegRestartAsserted	(0x8000)
#define	PCIDevStatRegPciResetInterrupt	(0x4000)
#define	PCIDevStatRegSoftRes		(0x2000)
#define	PCIDevStatRegRTCRes		(0x1000)
#define	PCIDevStatRegHardRes		(0x800)
#define	PCIDevStatRegHostRes		(0x400)
#define	PCIDevStatRegSleepMode		(0x200)
#define	PCIDevStatRegClockDivisor	(0x1ff)
#define	PCIDevStatRegClockDivisorShift	(0x0)
#define	PCIDevStatRegMask		(0xff81ffff)
#define	PCIDevStatRegAccessType		(NO_TEST)
#define	PCIDevStatRegInitialValue	(0x0)
#define	PCIDevStatRegTestMask		(0xff81ffff)

/* Instance values */
#define aPCIARMIntReg		 0x980
#define aPCIARMIntAckReg		 0x984
#define aPCIARMIntEnReg		 0x988
#define aPCIFill0x0C			 0x98c
#define aPCIHostIntReg		 0x990
#define aPCIHostIntAckReg		 0x994
#define aPCIHostIntEnReg		 0x998
#define aPCIFill0x1C			 0x99c
#define aPCIGenPurpComReg1		 0x9a0
#define aPCIGenPurpComReg2		 0x9a4
#define aPCIFill0x28			 0x9a8
#define aPCIFill0x2C			 0x9ac
#define aPCIDirectMemBase		 0x9b0
#define aPCIDirectMemEn		 0x9b4
#define aPCIFill0x38			 0x9b8
#define aPCIFill0x3C			 0x9bc
#define aPCIEventReg			 0x9c0
#define aPCIEventMask		 0x9c4
#define aPCIPresStateReg		 0x9c8
#define aPCIFill0x4C			 0x9cc
#define aPCIConfigLoadReg		 0x9d0
#define aPCIFill0x54			 0x9d4
#define aPCIFill0x58			 0x9d8
#define aPCIFill0x5C			 0x9dc
#define aPCIMasHostMemAddr		 0x9e0
#define aPCIMasLenReg		 0x9e4
#define aPCIMasControlReg		 0x9e8
#define aPCIFill0x6C			 0x9ec
#define aPCIMasDataReg		 0x9f0
#define aPCIMasOptReg		 0x9f4
#define aPCIDevStatReg		 0x9f8

/* C struct view */

#ifndef __ASSEMBLER__

typedef struct s_PCI {
 unsigned ARMIntReg; /* @0 */
 unsigned ARMIntAckReg; /* @4 */
 unsigned ARMIntEnReg; /* @8 */
 unsigned Fill0x0C; /* @12 */
 unsigned HostIntReg; /* @16 */
 unsigned HostIntAckReg; /* @20 */
 unsigned HostIntEnReg; /* @24 */
 unsigned Fill0x1C; /* @28 */
 unsigned GenPurpComReg1; /* @32 */
 unsigned GenPurpComReg2; /* @36 */
 unsigned Fill0x28; /* @40 */
 unsigned Fill0x2C; /* @44 */
 unsigned DirectMemBase; /* @48 */
 unsigned DirectMemEn; /* @52 */
 unsigned Fill0x38; /* @56 */
 unsigned Fill0x3C; /* @60 */
 unsigned EventReg; /* @64 */
 unsigned EventMask; /* @68 */
 unsigned PresStateReg; /* @72 */
 unsigned Fill0x4C; /* @76 */
 unsigned ConfigLoadReg; /* @80 */
 unsigned Fill0x54; /* @84 */
 unsigned Fill0x58; /* @88 */
 unsigned Fill0x5C; /* @92 */
 unsigned MasHostMemAddr; /* @96 */
 unsigned MasLenReg; /* @100 */
 unsigned MasControlReg; /* @104 */
 unsigned Fill0x6C; /* @108 */
 unsigned MasDataReg; /* @112 */
 unsigned MasOptReg; /* @116 */
 unsigned DevStatReg; /* @120 */
} s_PCI;

#endif /* !__ASSEMBLER__ */

#define uPCI ((volatile struct s_PCI *) 0xc0000980)

#endif /* !defined(__PCI_h) */
