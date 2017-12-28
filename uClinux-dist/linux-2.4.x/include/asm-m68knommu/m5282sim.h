/****************************************************************************/

/*
 *	m5282sim.h -- ColdFire 5282 System Integration Module support.
 *
 *	(C) Copyright 2003, Greg Ungerer (gerg@snapgear.com)
 */

/****************************************************************************/
#ifndef	m5282sim_h
#define	m5282sim_h
/****************************************************************************/

#include <linux/config.h>

/*
 *	Define the 5282 SIM register set addresses.
 */
#define	MCFICM_INTC0		0x0c00		/* Base for Interrupt Ctrl 0 */
#define	MCFICM_INTC1		0x0d00		/* Base for Interrupt Ctrl 1 */
#define	MCFINTC_IPRH		0x00		/* Interrupt pending 32-63 */
#define	MCFINTC_IPRL		0x04		/* Interrupt pending 1-31 */
#define	MCFINTC_IMRH		0x08		/* Interrupt mask 32-63 */
#define	MCFINTC_IMRL		0x0c		/* Interrupt mask 1-31 */
#define	MCFINTC_INTFRCH		0x10		/* Interrupt force 32-63 */
#define	MCFINTC_INTFRCL		0x14		/* Interrupt force 1-31 */
#define	MCFINTC_IRLR		0x18		/* */
#define	MCFINTC_IACKL		0x19		/* */
#define	MCFINTC_ICR0		0x40		/* Base ICR register */

#define	MCFINT_EPF1 		 1		/* Interrupt number for EPF1 (_IRQ1) */
#define	MCFINT_EPF2 		 2		/* Interrupt number for EPF2 (_IRQ2) */
#define	MCFINT_EPF3 		 3		/* Interrupt number for EPF3 (_IRQ3) */
#define	MCFINT_EPF4 		 4		/* Interrupt number for EPF4 (_IRQ4) */
#define	MCFINT_EPF5 		 5		/* Interrupt number for EPF5 (_IRQ5) */
#define	MCFINT_EPF6 		 6		/* Interrupt number for EPF6 (_IRQ6) */
#define	MCFINT_EPF7 		 7		/* Interrupt number for EPF7 (_IRQ7) */

#define	MCFINT_UART0		13		/* Interrupt number for UART0 */
#define	MCFINT_UART1		14		/* Interrupt number for UART1 */
#define	MCFINT_UART2		15		/* Interrupt number for UART2 */
#define	MCFINT_QSPI		18		/* Interrupt number for QSPI */
#define	MCFINT_PIT1		55		/* Interrupt number for PIT1 */

#define MCFINT_CAN_BUF00	8		/* Interrupt for CAN Buffer*/
#define MCFINT_CAN_BUF01	9		/* Interrupt for CAN Buffer*/
#define	MCFINT_CAN_BUF02	10		/* CAN BUF 2  register  10 */
#define	MCFINT_CAN_BUF03	11		/* CAN BUF 3  register  11 */
#define	MCFINT_CAN_BUF04	12		/* CAN BUF 4  register  12 */
#define	MCFINT_CAN_BUF05	13		/* CAN BUF 5  register  13 */
#define	MCFINT_CAN_BUF06	14		/* CAN BUF 6  register  14 */
#define	MCFINT_CAN_BUF07	15		/* CAN BUF 7  register  15 */
#define	MCFINT_CAN_BUF08	16		/* CAN BUF 8  register  16 */
#define	MCFINT_CAN_BUF09	17		/* CAN BUF 9  register  17 */
#define	MCFINT_CAN_BUF10	18		/* CAN BUF 10 register  18 */
#define	MCFINT_CAN_BUF11	19		/* CAN BUF 11 register  19 */
#define	MCFINT_CAN_BUF12	20		/* CAN BUF 12 register  20 */
#define	MCFINT_CAN_BUF13	21		/* CAN BUF 13 register  21 */
#define	MCFINT_CAN_BUF14	22		/* CAN BUF 14 register  22 */
#define	MCFINT_CAN_BUF15	23		/* CAN BUF 15 register  23 */
#define	MCFINT_CAN_WARN	        24		/* CAN warn register    24 */
#define	MCFINT_CAN_BUSOFF	25		/* CAN bus off register 25 */
#define	MCFINT_CAN_WUP	        26		/* CAN wake up register 26 */

/* QSPI Register */
#define MCF5282_QSPI_QMR	0x340		/* QSPI Mode Register (16 Bit) */
#define MCF5282_QSPI_QDLYR	0x344		/* QSPI Delay Register (16 Bit) */
#define MCF5282_QSPI_QWR	0x348		/* QSPI Wrap Register (16 Bit) */
#define MCF5282_QSPI_QIR	0x34C		/* QSPI Interrupt Register (16 Bit) */
#define MCF5282_QSPI_QAR	0x350		/* QSPI Address Register (16 Bit) */
#define MCF5282_QSPI_QDR	0x354		/* QSPI Data Register (16 Bit) */

/* DMA Timer 0 Register */
#define MCF5282_DTM_DTMR0	0x400		/* DMA Timer Mode Register 0 (16 Bit) */
#define MCF5282_DTM_DTXMR0	0x402		/* DMA Timer Extended Mode Register 0 (8 Bit) */
#define MCF5282_DTM_DTER0	0x403		/* DMA Timer Event Register 0 (8 Bit) */
#define MCF5282_DTM_DTRR0	0x404		/* DMA Timer Reference Register 0 (32 Bit) */
#define MCF5282_DTM_DTCR0	0x408		/* DMA Timer Capture Register 0 (32 Bit) */
#define MCF5282_DTM_DTCN0	0x40C		/* DMA Timer Counter Register 0 (32 Bit) */
/* DMA Timer 1 Register */
#define MCF5282_DTM_DTMR1	0x440		/* DMA Timer Mode Register 1 (16 Bit) */
#define MCF5282_DTM_DTXMR1	0x442		/* DMA Timer Extended Mode Register 1 (8 Bit) */
#define MCF5282_DTM_DTER1	0x443		/* DMA Timer Event Register 1 (8 Bit) */
#define MCF5282_DTM_DTRR1	0x444		/* DMA Timer Reference Register 1 (32 Bit) */
#define MCF5282_DTM_DTCR1	0x448		/* DMA Timer Capture Register 1 (32 Bit) */
#define MCF5282_DTM_DTCN1	0x44C		/* DMA Timer Counter Register 1 (32 Bit) */
/* DMA Timer 2 Register */
#define MCF5282_DTM_DTMR2	0x480		/* DMA Timer Mode Register 2 (16 Bit) */
#define MCF5282_DTM_DTXMR2	0x482		/* DMA Timer Extended Mode Register 2 (8 Bit) */
#define MCF5282_DTM_DTER2	0x483		/* DMA Timer Event Register 2 (8 Bit) */
#define MCF5282_DTM_DTRR2	0x484		/* DMA Timer Reference Register 2 (32 Bit) */
#define MCF5282_DTM_DTCR2	0x488		/* DMA Timer Capture Register 2 (32 Bit) */
#define MCF5282_DTM_DTCN2	0x48C		/* DMA Timer Counter Register 2 (32 Bit) */
/* DMA Timer 3 Register */
#define MCF5282_DTM_DTMR3	0x4C0		/* DMA Timer Mode Register 3 (16 Bit) */
#define MCF5282_DTM_DTXMR3	0x4C2		/* DMA Timer Extended Mode Register 3 (8 Bit) */
#define MCF5282_DTM_DTER3	0x4C3		/* DMA Timer Event Register 3 (8 Bit) */
#define MCF5282_DTM_DTRR3	0x4C4		/* DMA Timer Reference Register 3 (32 Bit) */
#define MCF5282_DTM_DTCR3	0x4C8		/* DMA Timer Capture Register 3 (32 Bit) */
#define MCF5282_DTM_DTCN3	0x4CC		/* DMA Timer Counter Register 3 (32 Bit) */

/* GPIO Register */
#define	MCF5282_GPIO_PORTA	0x100000	/* Port A Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTB	0x100001	/* Port B Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTC	0x100002	/* Port C Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTD	0x100003	/* Port D Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTE	0x100004	/* Port E Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTF	0x100005	/* Port F Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTG	0x100006	/* Port G Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTH	0x100007	/* Port H Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTJ	0x100008	/* Port J Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTDD	0x100009	/* Port DD Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTEH	0x10000A	/* Port EH Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTEL	0x10000B	/* Port EL Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTAS	0x10000C	/* Port AS Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTQS	0x10000D	/* Port QS Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTSD	0x10000E	/* Port SD Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTTC	0x10000F	/* Port TC Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTTD	0x100010	/* Port TD Output Data Register (8 Bit) */
#define	MCF5282_GPIO_PORTUA	0x100011	/* Port UA Output Data Register (8 Bit) */

#define	MCF5282_GPIO_DDRA	0x100014	/* Port A Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRB	0x100015	/* Port B Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRC	0x100016	/* Port C Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRD	0x100017	/* Port D Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRE	0x100018	/* Port E Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRF	0x100019	/* Port F Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRG	0x10001A	/* Port G Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRH	0x10001B	/* Port H Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRJ	0x10001C	/* Port J Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRDD	0x10001D	/* Port DD Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDREH	0x10001E	/* Port EH Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDREL	0x10001F	/* Port EL Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRAS	0x100020	/* Port AS Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRQS	0x100021	/* Port QS Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRSD	0x100022	/* Port SD Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRTC	0x100023	/* Port TC Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRTD	0x100024	/* Port TD Data Direction Register (8 Bit) */
#define	MCF5282_GPIO_DDRUA	0x100025	/* Port UA Data Direction Register (8 Bit) */

#define	MCF5282_GPIO_SETA	0x100028	/* Port A Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETB	0x100029	/* Port B Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETC	0x10002A	/* Port C Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETD	0x10002B	/* Port D Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETE	0x10002C	/* Port E Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETF	0x10002D	/* Port F Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETG	0x10002E	/* Port G Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETH	0x10002F	/* Port H Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETJ	0x100030	/* Port J Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETDD	0x100031	/* Port DD Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETEH	0x100032	/* Port EH Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETEL	0x100033	/* Port EL Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETAS	0x100034	/* Port AS Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETQS	0x100035	/* Port QS Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETSD	0x100036	/* Port SD Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETTC	0x100037	/* Port TC Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETTD	0x100038	/* Port TD Pin Data/Set Data Register (8 Bit) */
#define	MCF5282_GPIO_SETUA	0x100039	/* Port UA Pin Data/Set Data Register (8 Bit) */

#define	MCF5282_GPIO_CLRA	0x10003C	/* Port A Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRB	0x10003D	/* Port B Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRC	0x10003E	/* Port C Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRD	0x10003F	/* Port D Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRE	0x100040	/* Port E Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRF	0x100041	/* Port F Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRG	0x100042	/* Port G Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRH	0x100043	/* Port H Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRJ	0x100044	/* Port J Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRDD	0x100045	/* Port DD Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLREH	0x100046	/* Port EH Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLREL	0x100047	/* Port EL Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRAS	0x100048	/* Port AS Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRQS	0x100049	/* Port QS Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRSD	0x10004A	/* Port SD Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRTC	0x10004B	/* Port TC Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRTD	0x10004C	/* Port TD Clear Output Data Register (8 Bit) */
#define	MCF5282_GPIO_CLRUA	0x10004D	/* Port UA Clear Output Data Register (8 Bit) */

#define	MCF5282_GPIO_PBCDPAR	0x100050	/* Port B, C, and D Pin Assignment Register (8 Bit) */
#define	MCF5282_GPIO_PFPAR	0x100051	/* Port F Pin Assignment Register (8 Bit) */
#define	MCF5282_GPIO_PEPAR	0x100052	/* Port E Pin Assignment Register (16 Bit) */
#define	MCF5282_GPIO_PJPAR	0x100054	/* Port J Pin Assignment Register (8 Bit) */
#define	MCF5282_GPIO_PSDPAR	0x100055	/* Port SD Pin Assignment Register (8 Bit) */
#define	MCF5282_GPIO_PASPAR	0x100056	/* Port AS Pin Assignment Register (16 Bit) */
#define	MCF5282_GPIO_PEHLPAR	0x100058	/* Port EH and EL Pin Assignment Register (8 Bit) */
#define	MCF5282_GPIO_PQSPAR	0x100059	/* Port QS Pin Assignment Register (8 Bit) */
#define	MCF5282_GPIO_PTCPAR	0x10005A	/* Port TC Pin Assignment Register (8 Bit) */
#define	MCF5282_GPIO_PTDPAR	0x10005B	/* Port TD Pin Assignment Register (8 Bit) */
#define	MCF5282_GPIO_PUAPAR	0x10005C	/* Port UA Pin Assignment Register (8 Bit) */

/* EPORT Register */
#define MCF5282_EPORT_EPPAR	0x130000	/* EPORT Pin Assignment Register (16 Bit) */
#define MCF5282_EPORT_EPDDR	0x130002	/* EPORT Data Direction Register (8 Bit) */
#define MCF5282_EPORT_EPIER	0x130003	/* EPORT Interrupt Enable Register (8 Bit) */
#define MCF5282_EPORT_EPDR	0x130004	/* EPORT Data Register (8 Bit) */
#define MCF5282_EPORT_EPPDR	0x130005	/* EPORT Pin Data Direction Register (8 Bit) */
#define MCF5282_EPORT_EPFR	0x130006	/* EPORT Flag Register (8 Bit) */

/* Watchdog Timer Module */
#define MCFSIM_WCR		0x140000
#define MCFSIM_WMR		0x140002
#define MCFSIM_WCNTR		0x140004
#define MCFSIM_WSR		0x140006

/* Programmable Interrupt Timer (PIT) Module 0 */
#define MCF5282_PIT_PCSR0	0x150000	/* PIT Control and Status Register 0 (16 Bit) */
#define MCF5282_PIT_PMR0	0x150002	/* PIT Modulus Register 0 (16 Bit) */
#define MCF5282_PIT_PCNTR0	0x150004	/* PIT Count Register 0 (16 Bit) */
/* Programmable Interrupt Timer (PIT) Module 1 */
#define MCF5282_PIT_PCSR1	0x160000	/* PIT Control and Status Register 1 (16 Bit) */
#define MCF5282_PIT_PMR1	0x160002	/* PIT Modulus Register 1 (16 Bit) */
#define MCF5282_PIT_PCNTR1	0x160004	/* PIT Count Register 1 (16 Bit) */
/* Programmable Interrupt Timer (PIT) Module 2 */
#define MCF5282_PIT_PCSR2	0x170000	/* PIT Control and Status Register 2 (16 Bit) */
#define MCF5282_PIT_PMR2	0x170002	/* PIT Modulus Register 2 (16 Bit) */
#define MCF5282_PIT_PCNTR2	0x170004	/* PIT Count Register 2 (16 Bit) */
/* Programmable Interrupt Timer (PIT) Module 3 */
#define MCF5282_PIT_PCSR3	0x180000	/* PIT Control and Status Register 3 (16 Bit) */
#define MCF5282_PIT_PMR3	0x180002	/* PIT Modulus Register 3 (16 Bit) */
#define MCF5282_PIT_PCNTR3	0x180004	/* PIT Count Register 3 (16 Bit) */

/* QADC Register */
#define MCF5282_QADC_QADCMCR	0x190000	/* QADC Module Configuartion Register (16 Bit) */
#define MCF5282_QADC_PORTQA	0x190006	/* Port QA Data Register (8 Bit) */
#define MCF5282_QADC_PORTQB	0x190007	/* Port QB Data Register (8 Bit) */
#define MCF5282_QADC_DDRQA	0x190008	/* Port QA Data Direction Register (8 Bit) */
#define MCF5282_QADC_DDRQB	0x190009	/* Port QB Data Direction Register (8 Bit) */
#define MCF5282_QADC_QACR0	0x19000A	/* QADC Control Register 0 (16 Bit) */
#define MCF5282_QADC_QACR1	0x19000C	/* QADC Control Register 1 (16 Bit) */
#define MCF5282_QADC_QACR2	0x19000E	/* QADC Control Register 2 (16 Bit) */
#define MCF5282_QADC_QASR0	0x190010	/* QADC Status Register 0 (16 Bit) */
#define MCF5282_QADC_QASR1	0x190012	/* QADC Status Register 1 (16 Bit) */
#define MCF5282_QADC_CCW	0x190200	/* Conversion Command Word Table (64x16 Bit) */
#define MCF5282_QADC_RJURR	0x190280	/* Right Justified, Unsigned Result Register (64x16 Bit) */
#define MCF5282_QADC_LJSRR	0x190300	/* Left Justified, Signed Result Register (64x16 Bit) */
#define MCF5282_QADC_LJURR	0x190380	/* Left Justified, Unsigned Result Register (64x16 Bit) */

/* DMA Register */
#define MCF5282_DMA_DMAREQC	0x014		/* DMA Request Control Register (32 Bit) */

#define MCF5282_DMA_SAR0	0x100		/* DMA Source Address Register 0 (32 Bit) */
#define MCF5282_DMA_DAR0	0x104		/* DMA Destination Address Register 0 (32 Bit) */
#define MCF5282_DMA_DCR0	0x108		/* DMA Control Register 0 (32 Bit) */
#define MCF5282_DMA_BCR0	0x10C		/* DMA Byte Count Register 0 (32 Bit) */
#define MCF5282_DMA_DSR0	0x110		/* DMA Status Register 0 (8 Bit) */
#define MCF5282_DMA_SAR1	0x140		/* DMA Source Address Register 1 (32 Bit) */
#define MCF5282_DMA_DAR1	0x144		/* DMA Destination Address Register 1 (32 Bit) */
#define MCF5282_DMA_DCR1	0x148		/* DMA Control Register 1 (32 Bit) */
#define MCF5282_DMA_BCR1	0x14C		/* DMA Byte Count Register 1 (32 Bit) */
#define MCF5282_DMA_DSR1	0x150		/* DMA Status Register 1 (8 Bit) */
#define MCF5282_DMA_SAR2	0x180		/* DMA Source Address Register 2 (32 Bit) */
#define MCF5282_DMA_DAR2	0x184		/* DMA Destination Address Register 2 (32 Bit) */
#define MCF5282_DMA_DCR2	0x188		/* DMA Control Register 2 (32 Bit) */
#define MCF5282_DMA_BCR2	0x18C		/* DMA Byte Count Register 2 (32 Bit) */
#define MCF5282_DMA_DSR2	0x190		/* DMA Status Register 2 (8 Bit) */
#define MCF5282_DMA_SAR3	0x1C0		/* DMA Source Address Register 3 (32 Bit) */
#define MCF5282_DMA_DAR3	0x1C4		/* DMA Destination Address Register 3 (32 Bit) */
#define MCF5282_DMA_DCR3	0x1C8		/* DMA Control Register 3 (32 Bit) */
#define MCF5282_DMA_BCR3	0x1CC		/* DMA Byte Count Register 3 (32 Bit) */
#define MCF5282_DMA_DSR3	0x1D0		/* DMA Status Register 3 (8 Bit) */

/* CFI Register */
#define MCF5282_CFM_MCR		0x1D0000		/* CFM Configuration Register (16 Bit) */
#define MCF5282_CFM_CLKD	0x1D0002		/* CFM Clock Divider Register (8 Bit) */
#define MCF5282_CFM_SEC		0x1D0008		/* CFM Security Register (32 Bit) */
#define MCF5282_CFM_PROT	0x1D0010		/* CFM Protection Register (32 Bit) */
#define MCF5282_CFM_SACC	0x1D0014		/* CFM Supervisor Access Register (32 Bit) */
#define MCF5282_CFM_DACC	0x1D0018		/* CFM Data Access Register (32 Bit) */
#define MCF5282_CFM_USTAT	0x1D0020		/* CFM User Status Register (8 Bit) */
#define MCF5282_CFM_CMD		0x1D0024		/* CFM Command Register (8 Bit) */

/* Interrupt vectors INTC0 */
#define MCF5282_INT_EPORT1	(64+1)
#define MCF5282_INT_EPORT2	(64+2)
#define MCF5282_INT_EPORT3	(64+3)
#define MCF5282_INT_EPORT4	(64+4)
#define MCF5282_INT_EPORT5	(64+5)
#define MCF5282_INT_EPORT6	(64+6)
#define MCF5282_INT_EPORT7	(64+7)
#define MCF5282_INT_SWT1	(64+8)
#define MCF5282_INT_DMA0	(64+9)
#define MCF5282_INT_DMA1	(64+10)
#define MCF5282_INT_DMA2	(64+11)
#define MCF5282_INT_DMA3	(64+12)

#define MCF5282_INT_QSPI	(64+18)
#define MCF5282_INT_DTIM0	(64+19)
#define MCF5282_INT_DTIM1	(64+20)
#define MCF5282_INT_DTIM2	(64+21)
#define MCF5282_INT_DTIM3	(64+22)

#define MCF5282_INT_PIT0	(64+55)
#define MCF5282_INT_PIT1	(64+56)
#define MCF5282_INT_PIT2	(64+57)
#define MCF5282_INT_PIT3	(64+58)

/****************************************************************************/
#endif	/* m5282sim_h */
