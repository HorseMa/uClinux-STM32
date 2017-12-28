#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include "rtl_types.h"
#include "pcm.h"

/* These identify the driver base version and may not be removed. */
MODULE_DESCRIPTION("RealTek PCM driver");
MODULE_LICENSE("GPL");
MODULE_PARM (debug, "i");
MODULE_PARM_DESC (debug, "Realtek PCM bitmapped message enable number");


//#define LOOPBACK
#define TEST_ALL_CHANNEL
#define TXBUF			0xa0c00000
#define RXBUF			0xa0d00000
#define TXBUF_PHYADDR  (TXBUF & 0x1fffffff)
#define RXBUF_PHYADDR  (RXBUF & 0x1fffffff)

#define RTL8181_REG_BASE (0xBD010000)

#define rtl_inb(offset) (*(volatile unsigned char *)(RTL8181_REG_BASE+offset))
#define rtl_inw(offset) (*(volatile unsigned short *)(RTL8181_REG_BASE+offset))
#define rtl_inl(offset) (*(volatile unsigned long *)(RTL8181_REG_BASE+offset))

#define rtl_outb(offset,val)	(*(volatile unsigned char *)(RTL8181_REG_BASE+offset) = val)
#define rtl_outw(offset,val)	(*(volatile unsigned short *)(RTL8181_REG_BASE+ offset) = val)
#define rtl_outl(offset,val)	(*(volatile unsigned long *)(RTL8181_REG_BASE+offset) = val)

#undef REG32
#define REG32(offset) 			(*((volatile uint32 *)(RTL8181_REG_BASE+offset)))

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

#ifdef BIT
#error	"BIT define occurred earlier elsewhere!\n"
#endif

#define BIT(x)	( 1 << (x))

#define	PGCR			(0x270000+0x00)
#define PCSCR			(0x270000+0x04)
#define PTSAR			(0x270000+0x08)
#define PBSIZE			(0x270000+0x0c)
#define	CH0TXBSA		(0x270000+0x10)
#define	CH1TXBSA		(0x270000+0x14)
#define	CH2TXBSA		(0x270000+0x18)
#define	CH3TXBSA		(0x270000+0x1c)
#define	CH0RXBSA		(0x270000+0x20)
#define	CH1RXBSA		(0x270000+0x24)
#define	CH2RXBSA		(0x270000+0x28)
#define	CH3RXBSA		(0x270000+0x2c)
#define	PIMR			(0x270000+0x30)
#define	PISR			(0x270000+0x34)
//PGCR 
#define	PCMENABLE		BIT(12)
#define	PCMCLKDIR		BIT(11)
#define P0EXDSE			BIT(10)
#define	FSINV			BIT(9)
//PCSCR
#define	CH0ILBE			BIT(28)		//channel 0 loopback
#define	CH0CMPE			BIT(27)		//channel 0 compander enable
#define CH0Alaw			BIT(26)		//channel 0 u/A-law
#define CH0TE			BIT(25)		//channel 0 TX enable
#define	CH0RE			BIT(24)		//channel 0 RX enable

#define	CH1ILBE			BIT(20)		//channel 1 loopback
#define	CH1CMPE			BIT(19)		//channel 1 compander enable
#define CH1Alaw			BIT(18)		//channel 1 u/A-law
#define CH1TE			BIT(17)		//channel 1 TX enable
#define	CH1RE			BIT(16)		//channel 1 RX enable

#define	CH2ILBE			BIT(12)		//channel 0 loopback
#define	CH2CMPE			BIT(11)		//channel 0 compander enable
#define CH2Alaw			BIT(10)		//channel 0 u/A-law
#define CH2TE			BIT(9)		//channel 0 TX enable
#define	CH2RE			BIT(8)		//channel 0 RX enable

#define	CH3ILBE			BIT(4)		//channel 1 loopback
#define	CH3CMPE			BIT(3)		//channel 1 compander enable
#define CH3Alaw			BIT(2)		//channel 1 u/A-law
#define CH3TE			BIT(1)		//channel 1 TX enable
#define	CH3RE			BIT(0)		//channel 1 RX enable

#define	CH0_1			0x1
#define	CH0_2			0x2
#define	CH0_3			0x3	
#define	CH1_2			0x4
#define	CH1_3			0x5
#define	CH2_3			0x6	

//#define DEBUG

#define PAGE1_OWN	BIT(1)
#define PAGE0_OWN	BIT(0)

// if remark both, foreground polling mode, ok
//#define INT_MODE	1	//process in foreground, ok
#define PCM_PKT_SIZE	2048	// pre-allocated big-enoungh data buffer	
#define PAGESIZE	1024
#define CH_NUM		4
#define PKTSIZE		255	// real voice data size = 4 * (PKTSIZE+1), max.=1024 	
				// note: the larger data size, the larger delay
//extern union task_union sys_tasks[NUM_TASK];
void init_pcm(int channel);
void pcm_tx(int channel,int slot,int seed,int size);
void parse_pcm_rx(int channel,int size);
void prepare_pcm_data(unsigned char *buf, int size,int seed);
void polltx(int channel);

extern void i2c_task(void);

struct pcm_pkt {
	unsigned char 	tx_rx;
	unsigned char 	*payload;
};	
static struct pcm_pkt tx[CH_NUM];
static struct pcm_pkt rx[CH_NUM];
static int8 chanEnabled[CH_NUM];
static uint8 chanTxCurrPage[CH_NUM];
static uint8 chanRxCurrPage[CH_NUM];

#ifdef INT_MODE
static int pcm_int = 0;
static void pcm_interrupt(void)
{
	unsigned int status;
	unsigned int flags;

	status = rtl_inl(PISR);
	rtl_outl(PISR,status);	
	pcm_int = 1;
	return;
}
struct irqaction pcm_irq7 = { pcm_interrupt, NULL, 7, "pcm", NULL, NULL };
#endif
void pcm_task(uint32 mode, uint32 count)
{
	int		channel,slot;
#ifdef LOOPBACK
	static int	loop=0;
	unsigned char   j;
	unsigned int 	flags;
#else
	int 		i;
#endif
	
	rtlglue_printf("This is pcm task init speaking\n");
	
	//i2c_task();		// this is for configuring TI CODEC	
	channel = 0;
	slot = 0;
#ifdef LOOPBACK
	for(;;)
	{
		init_pcm(channel); // reset each time before testing a channel
			
		rtl_outl(PCSCR,rtl_inl(PCSCR)| CH0ILBE);
		rtlglue_printf("pcm loopback channel%d slot%d\n",channel,slot);
		pcm_tx(channel,slot,loop,PKTSIZE);
		for(;;) {
			 if((rtl_inl(CH0RXBSA+channel*4) & 3) == 0)
				break;
		}
		//compare data
		parse_pcm_rx(channel,4*(PKTSIZE+1));	
	#ifdef TEST_ALL_CHANNEL
		channel ++;
		channel &= 0x3;
	#endif
		rtlglue_printf("Hello! This is %d-th pcm_task speaking\n",loop++);
	}
#else 	// use channel 0 to do voice playback test 

	switch(mode){
		case 0:
			rtlglue_printf("linear mode\n");
			mode = 0;
			break;
		case 1:
			rtlglue_printf("A-law mode");
			mode =  CH0CMPE|CH0Alaw;
			break;
		case 2:
			rtlglue_printf("u-law mode");
			mode = CH0CMPE;
		default:
			mode = 0;
		}
	init_pcm(0);
	//enable all PCM channels 
	//i = 0;
	rtl_outl(PBSIZE,PKTSIZE<<24);
	rtl_outl(PCSCR,mode| CH0RE);	// enable channel 0 rx
	for(i=0; i<count; i++) {
	#ifdef INT_MODE	
		 static int static_pcm_int ;
		 save_flags(flags);cli();
		 static_pcm_int = pcm_int;
		 restore_flags(flags);
		 if(static_pcm_int) {	
	#endif
			// channel 0, page 0 has data in
			for(;;) {
				if(!(rtl_inl(CH0RXBSA) & 1))
					break;
			}
//			memcpy((void*) TXBUF, (void*) RXBUF,(PKTSIZE+1)*4);
			rtl_outl(CH0TXBSA,RXBUF_PHYADDR | PAGE0_OWN);
			rtl_outl(PCSCR,mode| CH0TE|CH0RE);	
			for(;;) {
				if(!(rtl_inl(CH0TXBSA) & 1))
					break;
			}	
			rtl_outl(CH0RXBSA,RXBUF_PHYADDR | PAGE0_OWN);
			// channel 0, page 1 has data inside
			for(;;) {
				if(!(rtl_inl(CH0RXBSA) & 2))
					break;
			}
//			memcpy((void*) (TXBUF+(PKTSIZE+1)*4),
//			       (void*) (RXBUF+(PKTSIZE+1)*4),(PKTSIZE+1)*4);
			rtl_outl(CH0TXBSA,RXBUF_PHYADDR | PAGE1_OWN);
	                rtl_outl(PCSCR,mode| CH0TE|CH0RE);	
			for(;;) {
				if(!(rtl_inl(CH0TXBSA) & 2))
					break;
			}	
			rtl_outl(CH0RXBSA,RXBUF_PHYADDR | PAGE1_OWN);
	#ifdef INT_MODE
		   }	// static_pcm_int	
		   save_flags(flags);cli();	
		   if(pcm_int)
		     pcm_int = 0;
	 	   restore_flags(flags);
	#endif			
		}	// for(;;)
#endif
}

void init_pcm(int channel)
{
	//int i;

	//prom_printf("initialize pcm\n");
	//prom_printf("Before enable,PGCR=%x\n",rtl_inl(PGCR));
	rtl_outl(PGCR,0x1800);	// 0->1 enable PCM
	rtl_outl(PGCR,0x0000);	// 1->0 reset PCM
	rtl_outl(PGCR,0x1800);	// 0->1 enable PCM
	//prom_printf("After enable,PGCR=%x\n",rtl_inl(PGCR));
#if (INT_MODE)
	rtlglue_printf("request PCM irq\n");
	request_irq(7, (struct irqaction *)&pcm_irq7, NULL);
#endif
	tx[channel].payload = (unsigned char*) TXBUF;
	rx[channel].payload = (unsigned char*) RXBUF;	
	
	// allocate buffer address
	rtl_outl(CH0TXBSA+channel*4,(unsigned int)tx[channel].payload & 0xffffff);
	rtl_outl(CH0RXBSA+channel*4,(unsigned int)rx[channel].payload & 0xffffff);

#if (INT_MODE)	
	rtlglue_printf("enable PCM interrupt\n");
	rtl_outl(PIMR,0xffffffff);
#endif

	// set RX owned by PCM controller	
	rtl_outl(CH0RXBSA+channel*4, rtl_inl(CH0RXBSA+channel*4) | 0x3);
	//prom_printf("enable PCM....\n");

}
void pcm_tx(int channel,int slot,int seed,int size)
{
	unsigned char *buf;

	buf = tx[channel].payload;
	// prepare data
	prepare_pcm_data(buf,4*(size+1),seed);
	size = size << 24;
		
	rtl_outl(PBSIZE,size >> (8*channel));
	//rtlglue_printf("tx %x at channel%d\n",rtl_inl(PBSIZE),channel);	
	//rtlglue_printf("channel %d rx %x\n",channel,rtl_inl(CH0RXBSA+channel*4));
	// kick-off channel/slot	
	polltx(channel);

}
void parse_pcm_rx(int channel,int size)
{
	//unsigned char ret;
	//int i;

	flush_dcache();
	if(memcmp(tx[channel].payload,rx[channel].payload,size)) {
		rtlglue_printf("compare tx %x rx %x\n",(unsigned int) tx[channel].payload,
					       (unsigned int) rx[channel].payload);	
		rtlglue_printf("channel%d data error\n",channel);
		while(1);
	}
	else
		rtlglue_printf("pcm tx = rx\n");
}
// generate sine wave based on freq and data size
void prepare_pcm_data(unsigned char *buf, int size, int seed)
{
	memset(buf,0,size);
	int i;
	// get PCM (8-bit) pattern 
	for(i=0;i<size;i++) { 
		*(buf+i) = i % 255 + seed;
	}

}
void polltx(int channel)
{

	// set owned by PCM controller
	rtlglue_printf("poll 1 channels\n");
	rtl_outl(CH0TXBSA+channel*4,rtl_inl(CH0TXBSA+channel*4) | 3); 
	switch(channel) {
	case 0: 	
		// enable channel 0 TX/RX/LOOPBACK
		//rtl_outl(PCSCR,CH0CMPE|CH0Alaw|CH0RE|CH0TE|CH0ILBE);
		rtl_outl(PCSCR,rtl_inl(PCSCR)| CH0RE|CH0TE);
		memDump(0xbd280000, 16, "pcm reg");
		break;	
	case 1: 	
		// enable channel 1 TX/RX/LOOPBACK
		rtl_outl(PCSCR,rtl_inl(PCSCR)| CH1RE|CH1TE);
		break;	
	case 2: 	
		// enable channel 2 TX/RX/LOOPBACK
		rtl_outl(PCSCR,rtl_inl(PCSCR)| CH2RE|CH2TE);
		break;	
	case 3: 	
		// enable channel 3 TX/RX/LOOPBACK
		rtl_outl(PCSCR,rtl_inl(PCSCR)| CH3RE|CH3TE);
		break;	
	}
}
int32 check_pcm_rx(int channel,int size)
{
	//unsigned char ret;
	//int i;

	//flush_dcache();
	if(memcmp(tx[channel].payload,rx[channel].payload,size)) {
		rtlglue_printf("compare tx %x rx %x\n",(unsigned int) tx[channel].payload,
					       (unsigned int) rx[channel].payload);	
		rtlglue_printf("channel%d data error\n",channel);
		return FAILED;
	}
	else
		return SUCCESS;
}

void pcm_enableChan(uint32 chan){
	uint8 *txbuf;
	uint8 *rxbuf;

	if(chanEnabled[chan] == TRUE)
		return;

	/* allocate buffer */
	txbuf = (uint8*) rtlglue_malloc(4 * (PKTSIZE+1) * 2);
	rxbuf = (uint8*) rtlglue_malloc(4 * (PKTSIZE+1) * 2);
	tx[chan].payload = (uint8*) ((uint32) txbuf | 0x20000000UL);
	rx[chan].payload = (uint8*) ((uint32) rxbuf | 0x20000000UL);	

	/* setup buffer size */
	REG32(PBSIZE) &= (0xff << (chan * 8));
	REG32(PBSIZE) |= (PKTSIZE << (chan * 8));
	
	/* setup base address registers */
	REG32(CH0TXBSA + chan * 4) = (uint32) tx[chan].payload & 0x1fffffff;
	REG32(CH0RXBSA + chan * 4) = (uint32) rx[chan].payload & 0x1fffffff;

	/* set rx owned by asic */
	REG32(CH0RXBSA + chan * 4) |= (PAGE0_OWN | PAGE1_OWN);

	/* enable rx */
	REG32(PCSCR) |= ((CH0TE | CH0RE) >> (chan * 8));

	chanRxCurrPage[chan] = 0;
	chanEnabled[chan] = TRUE;
}

void pcm_disableChan(uint32 chan){
	if(chanEnabled[chan] == TRUE)
		return;

	/* free buffer */
	rtlglue_free((void*) tx[chan].payload);
	rtlglue_free((void*) rx[chan].payload);

	/* setup buffer size */
	REG32(PBSIZE) &= (0xff << (chan * 8));
	
	/* setup base address registers */
	REG32(CH0TXBSA + chan * 4) = 0;
	REG32(CH0RXBSA + chan * 4) = 0;

	/* disable tx,rx */
	REG32(PCSCR) &= ~((CH0TE | CH0RE) >> (chan * 8));

	chanEnabled[chan] = FALSE;
}

int32 pcm_write(uint32 chan, uint8 *buf){
	uint8 *txbuf;
	
	if(REG32(CH0TXBSA + chan * 4) & (PAGE0_OWN << chanTxCurrPage[chan]))
		return FAILED;

	txbuf = tx[chan].payload + (chanTxCurrPage[chan] * 4 * (PKTSIZE+1));
//	rtlglue_printf("write to channel %d tx page%d\n", chan, chanTxCurrPage[chan]);
	memcpy(txbuf, buf, 4 * (PKTSIZE+1));
//	memDump(txbuf, 64, "");

	/* set own bit */
	REG32(CH0TXBSA + chan * 4) |= (PAGE0_OWN << chanTxCurrPage[chan]);

	chanTxCurrPage[chan] ^= 1;
	
	return SUCCESS;
}

int32 pcm_read(uint32 chan, uint8 *buf){
	uint8 *rxbuf;
	
	if(REG32(CH0RXBSA + chan * 4) & (PAGE0_OWN << chanRxCurrPage[chan]))
		return FAILED;

	rxbuf = rx[chan].payload + (chanRxCurrPage[chan] * 4 * (PKTSIZE+1));
//	rtlglue_printf("read from channel %d rx page%d\n", chan, chanRxCurrPage[chan]);
//	memDump((void*) rxbuf, 64, "");
	memcpy(buf, (void*) rxbuf, 4 * (PKTSIZE+1));

	/* reset own bit */
	REG32(CH0RXBSA + chan * 4) |= (PAGE0_OWN << chanRxCurrPage[chan]);

	chanRxCurrPage[chan] ^= 1;

	return SUCCESS;
}

int32 pcm_ioctl(uint32 chan, uint32 cmd, uint32 val){
	uint32 reg;
	if(chan >= CH_NUM)
		return FAILED;
	switch(cmd){
		case PCM_GET_MODE:
			reg = REG32(PCSCR);
			if(!(reg & (CH0CMPE >> (chan * 8))))
				*(uint32*)val =  PCM_MODE_UNIFORM;
			else if(reg & (CH0Alaw >> (chan * 8)))
				*(uint32*)val = PCM_MODE_ALAW;
			else
				*(uint32*)val = PCM_MODE_ULAW;
			return SUCCESS;
		case PCM_GET_LOOPBACK:
			reg = REG32(PCSCR);
			if((chan == 0) && (reg & CH0ILBE))
				*(uint32*)val = TRUE;
			else
				*(uint32*)val = FALSE;
			return SUCCESS;
		case PCM_GET_TIMESLOT:
			*(uint32*)val = REG32(PTSAR);
			return SUCCESS;
		case PCM_SET_MODE:
			switch(val){
				case PCM_MODE_UNIFORM:
					REG32(PCSCR) &= ~(CH0CMPE >> (chan * 8));
					break;
				case PCM_MODE_ULAW:
					reg = REG32(PCSCR);
					reg |= (CH0CMPE >> (chan * 8));
					reg &= ~(CH0Alaw >> (chan * 8));
					REG32(PCSCR) = reg;
					break;
				case PCM_MODE_ALAW:
					reg = REG32(PCSCR);
					reg |= (CH0CMPE >> (chan * 8));
					reg |= (CH0Alaw >> (chan * 8));
					REG32(PCSCR) = reg;
					break;
				default:
					return FAILED;
				}
			return SUCCESS;
		case PCM_SET_LOOPBACK:
			if(chan == 0){
				if(val == TRUE)
					REG32(PCSCR) |= CH0ILBE;
				else
					REG32(PCSCR) &= ~CH0ILBE;
				return SUCCESS;
				}
			else
				return FALSE;
		case PCM_SET_TIMESLOT:
			if(val > 0x1f)
				return FAILED;
			reg = REG32(PTSAR);
			reg &= ~(0xff << ((CH_NUM - 1 - chan) * 8));
			reg |= (val << ((CH_NUM - 1 - chan) * 8));
			REG32(PTSAR) = reg;
			return SUCCESS;
		default:
			return FAILED;
		}
	return FAILED;
}

int32 pcm_autoTest(uint32 chan, uint32 repeat, uint32 seed){
	uint32 i;
	uint8 txbuf[4 * (PKTSIZE+1)];
	uint8 rxbuf[4 * (PKTSIZE+1)];
	int32 retry = 200;

	rtlglue_printf("This rtl8186 speaking\n");
	if(chan >= CH_NUM)
		return FAILED;
	
	for(i=0; i<repeat; i++)
	{
		prepare_pcm_data(txbuf, 4 * (PKTSIZE+1), seed);
		pcm_write(chan, txbuf);
		while(retry-- > 0)
			if(pcm_read(chan, rxbuf) == SUCCESS)
				break;
		/* compare data */
		if(memcmp(txbuf, rxbuf, 4 * (PKTSIZE+1))){
			rtlglue_printf("%d times tested\n", i);
			return FAILED;
			}
		rtlglue_printf("\b%c","-\\|/"[i%4]);
	}
	return SUCCESS;
}

int32 pcm_humanTest(uint32 chan){
	uint32 i = 0;
	uint8 buf[4 * (PKTSIZE+1)];
	
	if(chan >= CH_NUM)
		return FAILED;
	
	while(1)
	{
		while(1)
			if(pcm_read(chan, buf) == SUCCESS)
				break;
		pcm_write(chan, buf);
		rtlglue_printf("\b%c","-\\|/"[((i++)>>4)%4]);
	}
	return SUCCESS;
}

int __init pcm_probe(void){
	uint32 chan;
	
	rtlglue_printf("Initialize PCM ...");
 	REG32(PGCR) = PCMENABLE | PCMCLKDIR;	// 0->1 enable PCM
	REG32(PGCR) = 0;	// 1->0 reset PCM
 	REG32(PGCR) = PCMENABLE | PCMCLKDIR;	// 0->1 enable PCM

	for (chan=0; chan<CH_NUM; chan++)
		pcm_enableChan(chan);

	return 0;
}

void __exit pcm_exit(void){
	uint32 chan;

	rtlglue_printf("Exit PCM ...");
	for (chan=0; chan<CH_NUM; chan++)
		pcm_disableChan(chan);
	REG32(PGCR) = 0;	// 1->0 reset PCM
}

module_init(pcm_probe);
module_exit(pcm_exit);
	
