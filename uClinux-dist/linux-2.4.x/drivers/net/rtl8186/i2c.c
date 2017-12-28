#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/string.h>
//#include <asm/rtl8181.h>

#define RTL8181_REG_BASE (0xBD010000)

#define rtl_inb(offset) (*(volatile unsigned char *)(RTL8181_REG_BASE+offset))
#define rtl_inw(offset) (*(volatile unsigned short *)(RTL8181_REG_BASE+offset))
#define rtl_inl(offset) (*(volatile unsigned long *)(RTL8181_REG_BASE+offset))

#define rtl_outb(offset,val)	(*(volatile unsigned char *)(RTL8181_REG_BASE+offset) = val)
#define rtl_outw(offset,val)	(*(volatile unsigned short *)(RTL8181_REG_BASE+ offset) = val)
#define rtl_outl(offset,val)	(*(volatile unsigned long *)(RTL8181_REG_BASE+offset) = val)

#define flush_dcache() \
	__asm__ __volatile__ ( \
		"mfc0 $9,$20\n\t" \
		"nop\n\t" \
		"la $10,0xfffffffd\n\t" \
		"and $9,$10\n\t" \
		"mtc0 $9,$12\n\t" \
		"nop\n\t" \
		"li $10,2\n\t" \
		"or $9,$10\n\t" \
		"mtc0 $9,$12\n\t" \
		"nop\n\t" \
		"nop\n\t" \
		: /* no output */ \
		: /* no input */ \
		)

#define WaitKey() \
{ \
	unsigned int _wcnt; \
	for(_wcnt=0; _wcnt<0x1000; _wcnt++); \
}

#define SIO_CNR		0x80
#define SIO_STR		0x84
#define SIO_CKDIV	0x88
#define SIO_ADDR	0x8c
#define SIO_DATA0	0x90
#define SIO_DATA1	0x94
#define SIO_DATA2	0x98
#define SIO_DATA3	0x9c


#define SIO_SIZE0	0x0000
#define SIO_SIZE1	0x0010
#define	SIO_SIZE2	0x0020
#define SIO_SIZE3	0x0030
#define SIO_SIZE4	0x0040
#define SIO_SIZE5	0x0050
#define SIO_ADDMODE00	0x0000
#define SIO_ADDMODE01	0x0004
#define SIO_ADDMODE02	0x0008
#define SIO_READISR_EN	0x0100
#define SIO_WRITEISR_EN	0x0200


#define SIO_RWC_R      	0x0001
#define SIO_RWC_W      	0x0000

#define ENABLE_SIO 	0x0001
#define ENGAGED_SIO	0x0002

#define SIO_CLK_DIV32	0x00
#define SIO_CLK_DIV64 	0x01
#define SIO_CLK_DIV128 	0x02
#define SIO_CLK_DIV256 	0x03

//extern union task_union sys_tasks[NUM_TASK];

unsigned char backup[256];
unsigned char read[256];

void i2c_init(int clock_mode)
{

	rtl_outl(SIO_CNR,ENABLE_SIO ); //enable SIO
	rtl_outl(SIO_CKDIV,clock_mode); //set clock speed
	rtl_outl(SIO_STR,0xffffffff);
	
	//rtl_outl(0x270000,0x1800); // enable PCM
	//rtl_outl(0x270000,0x0000);
	//rtl_outl(0x270000,0x1800);
	
		
}


void i2c_byte_write(int addr,int data)
{
	unsigned long comm;
	unsigned long i;
	//WaitKey();
	comm= (0xff &addr)<<24 | (data & 0xff)<<16;
	rtlglue_printf("comm=%x\n",(unsigned int)comm);
	rtl_outl(SIO_ADDR,SIO_RWC_W | 0xE2 ); // 0xE2 for write
	rtl_outl(SIO_DATA0,comm);//
	rtl_outl(SIO_CNR,SIO_SIZE1 |SIO_ADDMODE00|ENABLE_SIO | ENGAGED_SIO);//one word address
	for(i=0;i<0xfff;i++)
	{
		if( (rtl_inl(SIO_CNR) & 0x02)==0) 
		{
			flush_dcache();
			rtlglue_printf("i=%x\n",(unsigned int)i);
			break;
		}	
		//schedule((unsigned long)(get_ctxt_buf(&(sys_tasks[I2C_TASK]))));	
	}
}

void i2c_read_multi(int addr, int byte_count,volatile unsigned long *data0)
{
	unsigned long i;
	if (byte_count>4) {
		rtlglue_printf("The read string too much.\n");
//		return NULL;
	}
	//WaitKey();
	rtl_outl(SIO_ADDR,SIO_RWC_R | 0xE2 | (addr<<8) ); // 0xE3 read
	rtl_outl(SIO_CNR,(byte_count<<4)|SIO_ADDMODE01|ENABLE_SIO | ENGAGED_SIO);//one word address
	for(i=0;i<0xfff;i++)
	{
		if( (rtl_inl(SIO_CNR) & 0x02)==0)
		{
			flush_dcache();
			//rtlglue_printf("i=%x\n",i); 
			break;
		}
		//schedule((unsigned long)(get_ctxt_buf(&(sys_tasks[I2C_TASK]))));	
	}
	
	*data0 =rtl_inl(SIO_DATA0);
	//return tmp;
}



void i2c_task(void)
{
	unsigned long i2c_data0;
	long j;
	static int loop=0;
	
	i2c_init(SIO_CLK_DIV256);

	rtlglue_printf("PCM codec I2C test \n");
	memset(backup,0,256);
	rtlglue_printf("PCM codec I2C test loop=%d\n",loop++);
#if 0 
	for(j=0;j<7;j++) {
		i2c_read_multi(j,3,&i2c_data0);
		rtlglue_printf("reg%d = %x\n",j,i2c_data0);
		WaitKey();
	}
#endif
//#define DTMF_test
#ifdef DTMF_test
	i2c_byte_write(6,0);	//AUX, use default value 
	WaitKey();
	i2c_byte_write(4,209); 	//DTMF HI tone
	WaitKey();
	i2c_byte_write(5,99);	//DTMF LO tone
	WaitKey();
	i2c_byte_write(0,0x9b); //power control, use default
	WaitKey();
	i2c_byte_write(2,0x40);	//TX PGA
	WaitKey();
	i2c_byte_write(3,0xa0);	//RX PGA
	WaitKey();
	i2c_byte_write(1,0x02); //enable tone mode
	rtlglue_printf("written data\n");
	for(j=0;j<7;j++) {
		i2c_read_multi(j,3,&i2c_data0);
		rtlglue_printf("reg%d = %x\n",j,i2c_data0);
		WaitKey();
	}
	while(1);
#endif
#if 1 
	//i2c_byte_write(6,0);	//AUX, use default value 
	//WaitKey();
	//i2c_byte_write(0,0x9b); //power control, use default, (reg0)
	//WaitKey();
	//i2c_byte_write(2,0x40);	//TX PGA (reg2)
	//WaitKey();
	//i2c_byte_write(3,0xa4);	//RX PGA (reg3)
	//WaitKey();
	i2c_byte_write(1,0x92);	//Mode control (reg1)
	WaitKey();
	
	
#endif
	for(j=0;j<7;j++) {
		i2c_read_multi(j,3,&i2c_data0);
		rtlglue_printf("reg%d = %x\n",(unsigned int)j,(unsigned int)i2c_data0);
		WaitKey();
	}
	//while(1);  //can read reg0,reg1,reg2,reg3
	rtl_outl(SIO_CNR,0 ); //enable SIO
	//rtl_outl(0x270000,0x0000);
	rtl_outl(SIO_STR,0x00000000);
	//rtlglue_printf("PCM should be enabled, hold now...\n");
	//while(1);
	
}


