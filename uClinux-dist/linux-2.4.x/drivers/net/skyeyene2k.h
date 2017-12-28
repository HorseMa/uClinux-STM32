#ifndef SKYEYE_NE2K_H
#define SKYEYE_NE2K_H

struct ethframe {
	unsigned char dst[ETH_ALEN];
	unsigned char src[ETH_ALEN];
	unsigned short proto;
	unsigned char data[ETH_DATA_LEN+22];
};

//#ifndef SKYEYE_NE2K_H
//#define SKYEYE_NE2K_H

#define AT91_NET_BASE    (0xfffa0000)
#define NE_BASE           AT91_NET_BASE   
#define AT91_NET_SIZE	 255
#define AT91_NET_IRQNUM	 16



//yangye 2003-1-20
//all addr[1,0] must be 00
#define NE_CR              (NE_BASE+0)             //R/W，对不同的页，CR都是同一个
#define	NE_DMA		   (NE_BASE+64)	     //0x10-0x10 是DMA端口,重复，只用0x10.
#define	NE_RESET	   (NE_BASE+124)	     //0x18－0x1f 是网卡复位端口，重复，只用0x1f，读写它将复位网卡

//page0 registers
#define NE_PSTART          (NE_BASE+4)            //W，接收缓冲环起始页
#define NE_PSTOP           (NE_BASE+8)            //W，接收缓冲环终止页（不包括此页）
#define NE_BNRY            (NE_BASE+12)            //R/W，接收缓冲环读指针，指向下一个包到来时的起始页,应初始化成＝CURR＝PSTART
#define NE_TPSR            (NE_BASE+16)            //W，Local DMA发送缓冲起始页寄存器
#define NE_TBCR0           (NE_BASE+20)            //W，Local DMA发送长度低位
#define NE_TBCR1           (NE_BASE+24)            //W，Local DMA发送长度高位
#define NE_ISR             (NE_BASE+28)            //R/W，中断状态寄存器
#define NE_RSAR0           (NE_BASE+32)            //W，Remote DMA目的起始地址低位
#define NE_RSAR1           (NE_BASE+36)            //W，Remote DMA目的起始地址高位
//这两个是CPU向网卡写入或读出数据包的实际长度，执行Remote DMA命令前设置
#define NE_RBCR0           (NE_BASE+40)            //W，Remote DMA数据长度低位
#define NE_RBCR1           (NE_BASE+44)            //W，Remote DMA数据长度高位
#define NE_RCR             (NE_BASE+48)            //W，接收配置寄存器,初始化时写入0x04,表示只接收发给本网卡MAC地址的,大于64字节的以太网包或广播包
#define NE_TCR             (NE_BASE+52)            //发送配置寄存器,初始化开始时写入0x02，置网卡为Loop Back模式，停止发送数据包,初始化结束写入0x00。正常发送数据包并加上CRC
#define NE_DCR             (NE_BASE+56)            //W，数据配置寄存器,初始化时写入0x48，8位模式，FIFO深度8字节，DMA方式
#define NE_IMR             (NE_BASE+60)            //W，中断屏蔽寄存器它的各位和ISR中的各位相对应，向IMR写入值即为打开相应中断
//page1 registers
#define NE_PAR0            (NE_BASE+4)            //R/W，网卡MAC地址最高位
#define NE_PAR1            (NE_BASE+8)            //R/W，网卡MAC地址
#define NE_PAR2            (NE_BASE+12)            //R/W，网卡MAC地址
#define NE_PAR3            (NE_BASE+16)            //R/W，网卡MAC地址
#define NE_PAR4            (NE_BASE+20)            //R/W，网卡MAC地址
#define NE_PAR5            (NE_BASE+24)            //R/W，网卡MAC地址最低位
#define NE_CURR            (NE_BASE+28)            //R/W，接收缓冲环写指针
#define NE_MAR0            (NE_BASE+32)            //R/W，组播寄存器 
#define NE_MAR1            (NE_BASE+36)            //R/W，组播寄存器 
#define NE_MAR2            (NE_BASE+40)            //R/W，组播寄存器 
#define NE_MAR3            (NE_BASE+44)            //R/W，组播寄存器 
#define NE_MAR4            (NE_BASE+48)            //R/W，组播寄存器 
#define NE_MAR5            (NE_BASE+52)            //R/W，组播寄存器 
#define NE_MAR6            (NE_BASE+56)            //R/W，组播寄存器 
#define NE_MAR7            (NE_BASE+60)            //R/W，组播寄存器 
 
//page2 registers (read only in 8019as)
//#define NE_PSTART          0x01            //R，接收缓冲环起始页
//#define NE_PSTOP           0x02            //R，接收缓冲环终止页（不包括此页）
//#define NE_TPSR            0x04            //R，Local DMA发送缓冲起始页寄存器
//#define NE_RCR             0x0c            //R，接收配置寄存器
//#define NE_TCR             0x0d            //R,发送配置寄存器
//#define NE_DCR             0x0e            //R，数据配置寄存器
//#define NE_IMR             0x0f            //R，用来读中断屏蔽寄存器IMR状态


//CR命令寄存器的命令 
#define	CMD_STOP	0x01       //网卡停止收发数据
#define	CMD_RUN	    	0x02       //网卡执行命令并开始收发数据包（命令为下面四种）
#define	CMD_XMIT	0x04       //Local DMA SEND（网卡――>以太网 ）
#define	CMD_READ	0x08       //Remote DMA READ，用于手动接收数据（网卡――>CPU）
#define	CMD_WRITE	0x10       //Remote DMA WRITE （网卡<――CPU）
#define	CMD_SEND	0x18       //SEND COMMAND命令，用于自动接收数据包                                      （网卡――>CPU）
#define	CMD_NODMA	0x20       //停止DMA操作
#define	CMD_PAGE0	0x00       //   选择第0页（要先选页，再读写该页寄存器）
#define	CMD_PAGE1	0x40       //   选择第1页
#define	CMD_PAGE2	0x80       //   选择第2页

//写入TPSR的值 
#define	XMIT_START	 0x4000       //发送缓冲起始地址（写入时要右移8位得到页号）
//写入PSTART的值 
#define	RECV_START	 0x4600       //接收缓冲起始地址（写入时要右移8位得到页号）
//写入PSTOP的值 
#define	RECV_STOP	 0x6000       //接收缓冲结束地址（写入时要右移8位得到页号）

//中断状态寄存器的值 
#define	ISR_PRX	    	0x01       //正确接收数据包中断。做接收处理
#define	ISR_PTX		0x02       //正确发送数据包中断。做不做处理要看上层软件了。
#define	ISR_RXE	    	0x04       //接收数据包出错。做重新设置BNRY＝CURR处理。 
#define	ISR_TXE	    	0x08       //由于冲突次数过多，发送出错。做重发处理
#define	ISR_OVW	    	0x10       //网卡内存溢出。做软件重启网卡处理。见手册。
#define	ISR_CNT	    	0x20       //出错计数器中断，屏蔽掉（屏蔽用IMR寄存器）。
#define	ISR_RDC	    	0x40       //Remote DMA结束 。屏蔽掉。轮询等待DMA结束。
#define	ISR_RST		0x80       //网卡Reset，屏蔽掉。

//中断屏蔽寄存器的值 
#define	ISR_PRX	    	0x01
#define	ISR_PTX		0x02
#define	ISR_RXE	    	0x04
#define	ISR_TXE	    	0x08
#define	ISR_OVW	    	0x10
#define	ISR_CNT 	0x20
#define	ISR_RDC	    	0x40
#define	ISR_RST		0x80

//数据控制寄存器
//初始化时写入0x48，8位模式，FIFO深度8字节，DMA方式。
#define	DCR_WTS		0x01
#define	DCR_BOS		0x02
#define	DCR_LAS		0x04
#define	DCR_LS		0x08
#define	DCR_ARM		0x10
#define	DCR_FIFO2	0x00
#define	DCR_FIFO4	0x20
#define	DCR_FIFO8	0x40
#define	DCR_FIFO12	0x60

//TCR发送配置寄存器
//初始化开始时写入0x02，置网卡为Loop Back模式，停止发送数据包，
//初始化结束写入0x00。正常发送数据包并加上CRC。
#define	TCR_CRC		0x01
#define	TCR_LOOP_NONE	0x00
#define	TCR_LOOP_INT	0x02
#define	TCR_LOOP_EXT	0x06
#define	TCR_ATD		0x08
#define	TCR_OFST	0x10

//RCR接收配置寄存器
//初始化时写入0x04。只接收发给本网卡MAC地址大于64字节的以太网包或广播包
#define	RCR_SEP		0x01
#define	RCR_AR		0x02
#define	RCR_AB		0x04
#define	RCR_AM		0x08
#define	RCR_PRO		0x10
#define	RCR_MON		0x20


#endif /* SKYEYE_NE2K_H */
