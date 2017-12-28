/*
 * linux/include/asm-arm/arch-lpc/hardware.h
 * for lpc
 * 2004-06-19 added by ksh,tsinghua
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

/* 0=TC0, 1=TC1*/
#define KERNEL_TIMER 0	

#define LPC_TC_BASE 0xe0004000
#define HARD_RESET_NOW()
/*
*	add by lyh, use inner-ram for IRQ*/
//#define RAM_BASE 0x0
#define RAM_BASE 0x40000000

/*clocks*/
#define Fosc            11059200   
#define Fcclk           (Fosc * 4) 
#define Fcco            (Fcclk * 4)
#define Fpclk           (Fcclk / 4)

/*
 *  added by ksh,2004-06-18,defined interrupt register
 */
#define VIC_BASE 	0xfffff000
#define VIC_ISR  		(VIC_BASE+0x000)
#define VIC_FSR  		(VIC_BASE+0x004)
#define VIC_RISR		(VIC_BASE+0x008)
#define VIC_ISLR		(VIC_BASE+0x00c) //interrupt select register
#define VIC_IER		(VIC_BASE+0x010)
#define VIC_IECR		(VIC_BASE+0x014)
#define VIC_SIR		(VIC_BASE+0x018)
#define VIC_SICR		(VIC_BASE+0x01c)
#define VIC_PER		(VIC_BASE+0x020)
#define VIC_AR	(VIC_BASE+0x030)
#define VIC_DVAR	(VIC_BASE+0x034)
#define VIC_VAR(i)	(VIC_BASE+0x100+i*4)
#define VIC_VCR(i)	(VIC_BASE+0x200+i*4)

/* EXTERNAL MEMORY CONTROLLER (EMC) */
/* 外部总线控制器 */
#define BCFG0           0xFFE00000    /* lpc22xx only */
#define BCFG1           0xFFE00004    /* lpc22xx only */
#define BCFG2           0xFFE00008    /* lpc22xx only */
#define BCFG3           0xFFE0000C    /* lpc22xx only */
                        
/* External Interrupts */
/* 外部中断控制寄存器 */
#define EXTINT           0xE01FC140
#define EXTWAKE          0xE01FC144
#define EXTMODE          0xE01FC148     /* no in lpc210x*/
#define EXTPOLAR         0xE01FC14C     /* no in lpc210x*/
                        
/* SMemory mapping control. */                       
/* 内存remap控制寄存器 */                            
#define MEMMAP          0xE01FC040
                        
/* Phase Locked Loop (PLL) */                        
/* PLL控制寄存器 */                              
#define PLLCON           0xE01FC080
#define PLLCFG           0xE01FC084
#define PLLSTAT          0xE01FC088
#define PLLFEED          0xE01FC08C
                        
/* Power Control */     
/* 功率控制寄存器 */    
#define PCON             0xE01FC0C0
#define PCONP            0xE01FC0C4
                              
/* VPB Divider */                              
/* VLSI外设总线（VPB）分频寄存器 */                  
#define VPBDIV           0xE01FC100
                              
/* Memory Accelerator Module (MAM) */                
/* 存储器加速模块 */                              
#define MAMCR          0xE01FC000
#define MAMTIM         0xE01FC004
                              
/* Vectored Interrupt Controller (VIC) */            
/* 向量中断控制器(VIC)的特殊寄存器 */                
#define VICIRQStatus     0xFFFFF000
#define VICFIQStatus     0xFFFFF004
#define VICRawIntr       0xFFFFF008
#define VICIntSelect     0xFFFFF00C
#define VICIntEnable     0xFFFFF010
#define VICIntEnClr      0xFFFFF014
#define VICSoftInt       0xFFFFF018
#define VICSoftIntClear  0xFFFFF01C
#define VICProtection    0xFFFFF020
#define VICVectAddr      0xFFFFF030
#define VICDefVectAddr   0xFFFFF034
#define VICVectAddr0     0xFFFFF100
#define VICVectAddr1     0xFFFFF104
#define VICVectAddr2     0xFFFFF108
#define VICVectAddr3     0xFFFFF10C
#define VICVectAddr4     0xFFFFF110
#define VICVectAddr5     0xFFFFF114
#define VICVectAddr6     0xFFFFF118
#define VICVectAddr7     0xFFFFF11C
#define VICVectAddr8     0xFFFFF120
#define VICVectAddr9     0xFFFFF124
#define VICVectAddr10    0xFFFFF128
#define VICVectAddr11    0xFFFFF12C
#define VICVectAddr12    0xFFFFF130
#define VICVectAddr13    0xFFFFF134
#define VICVectAddr14    0xFFFFF138
#define VICVectAddr15    0xFFFFF13C
#define VICVectCntl0     0xFFFFF200
#define VICVectCntl1     0xFFFFF204
#define VICVectCntl2     0xFFFFF208
#define VICVectCntl3     0xFFFFF20C
#define VICVectCntl4     0xFFFFF210
#define VICVectCntl5     0xFFFFF214
#define VICVectCntl6     0xFFFFF218
#define VICVectCntl7     0xFFFFF21C
#define VICVectCntl8     0xFFFFF220
#define VICVectCntl9     0xFFFFF224
#define VICVectCntl10    0xFFFFF228
#define VICVectCntl11    0xFFFFF22C
#define VICVectCntl12    0xFFFFF230
#define VICVectCntl13    0xFFFFF234
#define VICVectCntl14    0xFFFFF238
#define VICVectCntl15    0xFFFFF23C
                              
/* Pin Connect Block */                              
/* 管脚连接模块控制寄存器 */                         
#define PINSEL0         0xE002C000
#define PINSEL1         0xE002C004
#define PINSEL2         0xE002C014     /* no in lpc210x*/
                        
/* General Purpose Input/Output (GPIO) */            
/* 通用并行IO口的特殊寄存器 */                       
#define IOPIN           0xE0028000     /* lpc210x only */
#define IOSET           0xE0028004     /* lpc210x only */
#define IODIR           0xE0028008     /* lpc210x only */
#define IOCLR           0xE002800C     /* lpc210x only */
                        
#define IO0PIN          0xE0028000     /* no in lpc210x*/
#define IO0SET          0xE0028004     /* no in lpc210x*/
#define IO0DIR          0xE0028008     /* no in lpc210x*/
#define IO0CLR          0xE002800C     /* no in lpc210x*/
                        
#define IO1PIN          0xE0028010     /* no in lpc210x*/
#define IO1SET          0xE0028014     /* no in lpc210x*/
#define IO1DIR          0xE0028018     /* no in lpc210x*/
#define IO1CLR          0xE002801C     /* no in lpc210x*/
                        
#define IO2PIN          0xE0028020     /* lpc22xx only */
#define IO2SET          0xE0028024     /* lpc22xx only */
#define IO2DIR          0xE0028028     /* lpc22xx only */
#define IO2CLR          0xE002802C     /* lpc22xx only */
                        
#define IO3PIN          0xE0028030     /* lpc22xx only */
#define IO3SET          0xE0028034     /* lpc22xx only */
#define IO3DIR          0xE0028038     /* lpc22xx only */
#define IO3CLR          0xE002803C     /* lpc22xx only */
                              
/* Universal Asynchronous Receiver Transmitter 0 (UART0) */
/* 通用异步串行口0(UART0)的特殊寄存器 */             
#define U0RBR           0xE000C000
#define U0THR           0xE000C000
#define U0IER           0xE000C004
#define U0IIR           0xE000C008
#define U0FCR           0xE000C008
#define U0LCR           0xE000C00C
#define U0LSR           0xE000C014
#define U0SCR           0xE000C01C
#define U0DLL           0xE000C000
#define U0DLM           0xE000C004
                              
/* Universal Asynchronous Receiver Transmitter 1 (UART1) */
/* 通用异步串行口1(UART1)的特殊寄存器 */             
#define U1RBR           0xE0010000
#define U1THR           0xE0010000
#define U1IER           0xE0010004
#define U1IIR           0xE0010008
#define U1FCR           0xE0010008
#define U1LCR           0xE001000C
#define U1MCR           0xE0010010
#define U1LSR           0xE0010014
#define U1MSR           0xE0010018
#define U1SCR           0xE001001C
#define U1DLL           0xE0010000
#define U1DLM           0xE0010004
                        
/* I2C (8/16 bit data bus) */                        
/* 芯片间总线（I2C）的特殊寄存器 */                  
#define I2CONSET        0xE001C000
#define I2STAT          0xE001C004
#define I2DAT           0xE001C008
#define I2ADR           0xE001C00C
#define I2SCLH          0xE001C010
#define I2SCLL          0xE001C014
#define I2CONCLR        0xE001C018
                              
/* SPI (Serial Peripheral Interface) */              
/* SPI总线接口的特殊寄存器 */                        
        /* only for lpc210x*/                        
#define SPI_SPCR         0xE0020000
#define SPI_SPSR         0xE0020004
#define SPI_SPDR         0xE0020008
#define SPI_SPCCR        0xE002000C
#define SPI_SPINT        0xE002001C
                        
#define S0PCR            0xE0020000   /* no in lpc210x*/
#define S0PSR            0xE0020004   /* no in lpc210x*/
#define S0PDR            0xE0020008   /* no in lpc210x*/
#define S0PCCR           0xE002000C   /* no in lpc210x*/
#define S0PINT           0xE002001C   /* no in lpc210x*/
                        
#define S1PCR            0xE0030000   /* no in lpc210x*/
#define S1PSR            0xE0030004   /* no in lpc210x*/
#define S1PDR            0xE0030008   /* no in lpc210x*/
#define S1PCCR           0xE003000C   /* no in lpc210x*/
#define S1PINT           0xE003001C   /* no in lpc210x*/
                              
/* CAN CONTROLLERS AND ACCEPTANCE FILTER */          
/* CAN控制器和接收路波器 */                          
#define CAN1MOD         0xE0044000  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1CMR         0xE0044004  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1GSR         0xE0044008  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1ICR         0xE004400C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1IER         0xE0044010  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1BTR         0xE0044014  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1EWL         0xE004401C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1SR          0xE0044020  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1RFS         0xE0044024  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1RDA         0xE0044028  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1RDB         0xE004402C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TFI1        0xE0044030  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TID1        0xE0044034  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TDA1        0xE0044038  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TDB1        0xE004403C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TFI2        0xE0044040  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TID2        0xE0044044  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TDA2        0xE0044048  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TDB2        0xE004404C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TFI3        0xE0044050  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TID3        0xE0044054  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TDA3        0xE0044058  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN1TDB3        0xE004405C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
                       
#define CAN2MOD         0xE0048000  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2CMR         0xE0048004  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2GSR         0xE0048008  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2ICR         0xE004800C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2IER         0xE0048010  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2BTR         0xE0048014  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2EWL         0xE004801C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2SR          0xE0048020  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2RFS         0xE0048024  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2RDA         0xE0048028  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2RDB         0xE004802C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TFI1        0xE0048030  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TID1        0xE0048034  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TDA1        0xE0048038  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TDB1        0xE004803C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TFI2        0xE0048040  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TID2        0xE0048044  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TDA2        0xE0048048  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TDB2        0xE004804C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TFI3        0xE0048050  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TID3        0xE0048054  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TDA3        0xE0048058  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN2TDB3        0xE004805C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
                       
#define CAN3MOD         0xE004C000  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3CMR         0xE004C004  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3GSR         0xE004C008  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3ICR         0xE004C00C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3IER         0xE004C010  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3BTR         0xE004C014  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3EWL         0xE004C01C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3SR          0xE004C020  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3RFS         0xE004C024  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3RDA         0xE004C028  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3RDB         0xE004C02C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TFI1        0xE004C030  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TID1        0xE004C034  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TDA1        0xE004C038  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TDB1        0xE004C03C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TFI2        0xE004C040  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TID2        0xE004C044  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TDA2        0xE004C048  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TDB2        0xE004C04C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TFI3        0xE004C050  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TID3        0xE004C054  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TDA3        0xE004C058  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN3TDB3        0xE004C05C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
                       
#define CAN4MOD         0xE0050000  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4CMR         0xE0050004  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4GSR         0xE0050008  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4ICR         0xE005000C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4IER         0xE0050010  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4BTR         0xE0050014  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4EWL         0xE005001C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4SR          0xE0050020  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4RFS         0xE0050024  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4RDA         0xE0050028  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4RDB         0xE005002C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TFI1        0xE0050030  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TID1        0xE0050034  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TDA1        0xE0050038  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TDB1        0xE005003C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TFI2        0xE0050040  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TID2        0xE0050044  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TDA2        0xE0050048  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TDB2        0xE005004C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TFI3        0xE0050050  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TID3        0xE0050054  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TDA3        0xE0050058  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN4TDB3        0xE005005C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
                       
#define CAN5MOD         0xE0054000  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5CMR         0xE0054004  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5GSR         0xE0054008  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5ICR         0xE005400C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5IER         0xE0054010  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5BTR         0xE0054014  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5EWL         0xE005401C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5SR          0xE0054020  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5RFS         0xE0054024  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5RDA         0xE0054028  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5RDB         0xE005402C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TFI1        0xE0054030  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TID1        0xE0054034  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TDA1        0xE0054038  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TDB1        0xE005403C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TFI2        0xE0054040  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TID2        0xE0054044  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TDA2        0xE0054048  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TDB2        0xE005404C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TFI3        0xE0054050  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TID3        0xE0054054  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TDA3        0xE0054058  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CAN5TDB3        0xE005405C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
                       
#define CAN6MOD         0xE0058000  /* lpc2292\lpc2294 only */
#define CAN6CMR         0xE0058004  /* lpc2292\lpc2294 only */
#define CAN6GSR         0xE0058008  /* lpc2292\lpc2294 only */
#define CAN6ICR         0xE005800C  /* lpc2292\lpc2294 only */
#define CAN6IER         0xE0058010  /* lpc2292\lpc2294 only */
#define CAN6BTR         0xE0058014  /* lpc2292\lpc2294 only */
#define CAN6EWL         0xE005801C  /* lpc2292\lpc2294 only */
#define CAN6SR          0xE0058020  /* lpc2292\lpc2294 only */
#define CAN6RFS         0xE0058024  /* lpc2292\lpc2294 only */
#define CAN6RDA         0xE0058028  /* lpc2292\lpc2294 only */
#define CAN6RDB         0xE005802C  /* lpc2292\lpc2294 only */
#define CAN6TFI1        0xE0058030  /* lpc2292\lpc2294 only */
#define CAN6TID1        0xE0058034  /* lpc2292\lpc2294 only */
#define CAN6TDA1        0xE0058038  /* lpc2292\lpc2294 only */
#define CAN6TDB1        0xE005803C  /* lpc2292\lpc2294 only */
#define CAN6TFI2        0xE0058040  /* lpc2292\lpc2294 only */
#define CAN6TID2        0xE0058044  /* lpc2292\lpc2294 only */
#define CAN6TDA2        0xE0058048  /* lpc2292\lpc2294 only */
#define CAN6TDB2        0xE005804C  /* lpc2292\lpc2294 only */
#define CAN6TFI3        0xE0058050  /* lpc2292\lpc2294 only */
#define CAN6TID3        0xE0058054  /* lpc2292\lpc2294 only */
#define CAN6TDA3        0xE0058058  /* lpc2292\lpc2294 only */
#define CAN6TDB3        0xE005805C  /* lpc2292\lpc2294 only */
                       
#define CANTxSR         0xE0040000  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANRxSR         0xE0040004  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANMSR          0xE0040008  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
                       
#define CANAFMR         0xE003C000  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANSFF_sa       0xE003C004  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANSFF_GRP_sa   0xE003C008  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANEFF_sa       0xE003C00C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANEFF_GRP_sa   0xE003C010  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANENDofTable   0xE003C014  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANLUTerrAd     0xE003C018  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
#define CANLUTerr       0xE003C01C  /* lpc2119\lpc2129\lpc2292\lpc2294 only */
                              
                              
/* Timer 0 */                        
/* 定时器0的特殊寄存器 */                      
#define T0IR            0xE0004000
#define T0TCR           0xE0004004
#define T0TC            0xE0004008
#define T0PR            0xE000400C
#define T0PC            0xE0004010
#define T0MCR           0xE0004014
#define T0MR0           0xE0004018
#define T0MR1           0xE000401C
#define T0MR2           0xE0004020
#define T0MR3           0xE0004024
#define T0CCR           0xE0004028
#define T0CR0           0xE000402C
#define T0CR1           0xE0004030
#define T0CR2           0xE0004034
#define T0CR3           0xE0004038
#define T0EMR           0xE000403C
                        
/* Timer 1 */                              
/* 定时器1的特殊寄存器 */                            
#define T1IR            0xE0008000
#define T1TCR           0xE0008004
#define T1TC            0xE0008008
#define T1PR            0xE000800C
#define T1PC            0xE0008010
#define T1MCR           0xE0008014
#define T1MR0           0xE0008018
#define T1MR1           0xE000801C
#define T1MR2           0xE0008020
#define T1MR3           0xE0008024
#define T1CCR           0xE0008028
#define T1CR0           0xE000802C
#define T1CR1           0xE0008030
#define T1CR2           0xE0008034
#define T1CR3           0xE0008038
#define T1EMR           0xE000803C
                              
/* Pulse Width Modulator (PWM) */                    
/* 脉宽调制器的特殊寄存器 */                         
#define PWMIR            0xE0014000
#define PWMTCR           0xE0014004
#define PWMTC            0xE0014008
#define PWMPR            0xE001400C
#define PWMPC            0xE0014010
#define PWMMCR           0xE0014014
#define PWMMR0           0xE0014018
#define PWMMR1           0xE001401C
#define PWMMR2           0xE0014020
#define PWMMR3           0xE0014024
#define PWMMR4           0xE0014040
#define PWMMR5           0xE0014044
#define PWMMR6           0xE0014048
#define PWMPCR           0xE001404C
#define PWMLER           0xE0014050
                              
/* A/D CONVERTER */                              
/* A/D转换器 */                              
#define ADCR            0xE0034000     /* no in lpc210x*/
#define ADDR            0xE0034004     /* no in lpc210x*/
                        
/* Real Time Clock */                              
/* 实时时钟的特殊寄存器 */                           
#define ILR              0xE0024000
#define CTC              0xE0024004
#define CCR              0xE0024008
#define CIIR             0xE002400C
#define AMR              0xE0024010
#define CTIME0           0xE0024014
#define CTIME1           0xE0024018
#define CTIME2           0xE002401C
#define SEC              0xE0024020
#define MIN              0xE0024024
#define HOUR             0xE0024028
#define DOM              0xE002402C
#define DOW              0xE0024030
#define DOY              0xE0024034
#define MONTH            0xE0024038
#define YEAR             0xE002403C
#define ALSEC            0xE0024060
#define ALMIN            0xE0024064
#define ALHOUR           0xE0024068
#define ALDOM            0xE002406C
#define ALDOW            0xE0024070
#define ALDOY            0xE0024074
#define ALMON            0xE0024078
#define ALYEAR           0xE002407C
#define PREINT           0xE0024080
#define PREFRAC          0xE0024084
                              
/* Watchdog */                              
/* 看门狗的特殊寄存器 */                             
#define WDMOD           0xE0000000
#define WDTC            0xE0000004
#define WDFEED          0xE0000008
#define WDTV            0xE000000C

/* Define firmware Functions */
/* 定义固件函数 */
#define rm_init_entry()             ((void (*)())(0x7fffff91))()
#define rm_undef_handler()          ((void (*)())(0x7fffffa0))()
#define rm_prefetchabort_handler()  ((void (*)())(0x7fffffb0))()
#define rm_dataabort_handler()      ((void (*)())(0x7fffffc0))()
#define rm_irqhandler()             ((void (*)())(0x7fffffd0))()
#define rm_irqhandler2()            ((void (*)())(0x7fffffe0))()
#define iap_entry(a, b)             ((void (*)())(0x7ffffff1))(a, b)


#ifndef __ASSEMBLER__


struct lpc_timers
{
	
	unsigned  long ir;				// 
	unsigned  long tcr;				// 
	unsigned  long tc;				// 
	unsigned  long pr;				// 
	unsigned  long pc;				// 
	unsigned  long mcr;				// 
	unsigned  long mr0;				// 
	unsigned  long mr1;				// 
	unsigned  long mr2;				// 
	unsigned  long mr3;				// 
	unsigned  long ccr;				// 
	unsigned  long cr0;				// 
	unsigned  long cr1;				// 
	unsigned  long cr2;				// 
	unsigned  long cr3;				// 
	unsigned  long emr;				// 
};
#endif


/*  TC control register */
#define TC_SYNC	(1)

/*  TC mode register */
#define TC2XC2S(x)	(x & 0x3)
#define TC1XC1S(x)	(x<<2 & 0xc)
#define TC0XC0S(x)	(x<<4 & 0x30)
#define TCNXCNS(timer,v) ((v) << (timer<<1))

/* TC channel control */
#define TC_CLKEN	(1)			
#define TC_CLKDIS	(1<<1)			
#define TC_SWTRG	(1<<2)			

/* TC interrupts enable/disable/mask and status registers */
#define TC_MTIOB	(1<<18)
#define TC_MTIOA	(1<<17)
#define TC_CLKSTA	(1<<16)

#define TC_ETRGS	(1<<7)
#define TC_LDRBS	(1<<6)
#define TC_LDRAS	(1<<5)
#define TC_CPCS		(1<<4)
#define TC_CPBS		(1<<3)
#define TC_CPAS		(1<<2)
#define TC_LOVRS	(1<<1)
#define TC_COVFS	(1)

/*
 *	USART registers
 */

#define LPC_UART_CNT 2
#define LPC_UART0_BASE 0xe000c000
#define LPC_UART1_BASE 0xe0010000

/*  US control register */
#define US_SENDA	(1<<12)
#define US_STTO		(1<<11)
#define US_STPBRK	(1<<10)
#define US_STTBRK	(1<<9)
#define US_RSTSTA	(1<<8)
#define US_TXDIS	(1<<7)
#define US_TXEN		(1<<6)
#define US_RXDIS	(1<<5)
#define US_RXEN		(1<<4)
#define US_RSTTX	(1<<3)
#define US_RSTRX	(1<<2)

/* US mode register */
#define US_CLK0		(1<<18)
#define US_MODE9	(1<<17)
#define US_CHMODE(x)(x<<14 & 0xc000)
#define US_NBSTOP(x)(x<<12 & 0x3000)
#define US_PAR(x)	(x<<9 & 0xe00)
#define US_SYNC		(1<<8)
#define US_CHRL(x)	(x<<6 & 0xc0)
#define US_USCLKS(x)(x<<4 & 0x30)

/* US interrupts enable/disable/mask and status register */
#define US_DMSI		(1<<10)
#define US_TXEMPTY	(1<<9)
#define US_TIMEOUT	(1<<8)
#define US_PARE		(1<<7)
#define US_FRAME	(1<<6)
#define US_OVRE		(1<<5)
#define US_ENDTX	(1<<4)
#define US_ENDRX	(1<<3)
#define US_RXBRK	(1<<2)
#define US_TXRDY	(1<<1)
#define US_RXRDY	(1)

#define US_ALL_INTS (US_DMSI|US_TXEMPTY|US_TIMEOUT|US_PARE|US_FRAME|US_OVRE|US_ENDTX|US_ENDRX|US_RXBRK|US_TXRDY|US_RXRDY)

#ifndef __ASSEMBLER__
#if 0
static inline void lpc_uart_init(int baudrate)
{

       //unsigned long reg = Fpclk / 16 /9600;		/*9600*/
       unsigned int reg;
	reg = 72;		/*default 9600*/
	
	
	PINSEL0 |= 0x5;		/*connect TXD0, RXD0*/
	U0LCR = 0x83;		/*DLAB = 1, 8N1*/

	U0DLL = reg;	
	U0DLM = 0;

	U0LCR = 0x3;		/*DLAB = 0*/
}

static inline void lpc_uart_putc(unsigned char c)
{
      U0THR = c;
	while((U0LSR & 0x40) == 0);
	/* If \n, also do \r */
	if (c == '\n')
		lpc_uart_putc ('\r');
}
#endif
#endif

#endif  /* _ASM_ARCH_HARDWARE_H */


