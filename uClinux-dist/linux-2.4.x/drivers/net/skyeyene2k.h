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
#define NE_CR              (NE_BASE+0)             //R/W���Բ�ͬ��ҳ��CR����ͬһ��
#define	NE_DMA		   (NE_BASE+64)	     //0x10-0x10 ��DMA�˿�,�ظ���ֻ��0x10.
#define	NE_RESET	   (NE_BASE+124)	     //0x18��0x1f ��������λ�˿ڣ��ظ���ֻ��0x1f����д������λ����

//page0 registers
#define NE_PSTART          (NE_BASE+4)            //W�����ջ��廷��ʼҳ
#define NE_PSTOP           (NE_BASE+8)            //W�����ջ��廷��ֹҳ����������ҳ��
#define NE_BNRY            (NE_BASE+12)            //R/W�����ջ��廷��ָ�룬ָ����һ��������ʱ����ʼҳ,Ӧ��ʼ���ɣ�CURR��PSTART
#define NE_TPSR            (NE_BASE+16)            //W��Local DMA���ͻ�����ʼҳ�Ĵ���
#define NE_TBCR0           (NE_BASE+20)            //W��Local DMA���ͳ��ȵ�λ
#define NE_TBCR1           (NE_BASE+24)            //W��Local DMA���ͳ��ȸ�λ
#define NE_ISR             (NE_BASE+28)            //R/W���ж�״̬�Ĵ���
#define NE_RSAR0           (NE_BASE+32)            //W��Remote DMAĿ����ʼ��ַ��λ
#define NE_RSAR1           (NE_BASE+36)            //W��Remote DMAĿ����ʼ��ַ��λ
//��������CPU������д���������ݰ���ʵ�ʳ��ȣ�ִ��Remote DMA����ǰ����
#define NE_RBCR0           (NE_BASE+40)            //W��Remote DMA���ݳ��ȵ�λ
#define NE_RBCR1           (NE_BASE+44)            //W��Remote DMA���ݳ��ȸ�λ
#define NE_RCR             (NE_BASE+48)            //W���������üĴ���,��ʼ��ʱд��0x04,��ʾֻ���շ���������MAC��ַ��,����64�ֽڵ���̫������㲥��
#define NE_TCR             (NE_BASE+52)            //�������üĴ���,��ʼ����ʼʱд��0x02��������ΪLoop Backģʽ��ֹͣ�������ݰ�,��ʼ������д��0x00�������������ݰ�������CRC
#define NE_DCR             (NE_BASE+56)            //W���������üĴ���,��ʼ��ʱд��0x48��8λģʽ��FIFO���8�ֽڣ�DMA��ʽ
#define NE_IMR             (NE_BASE+60)            //W���ж����μĴ������ĸ�λ��ISR�еĸ�λ���Ӧ����IMRд��ֵ��Ϊ����Ӧ�ж�
//page1 registers
#define NE_PAR0            (NE_BASE+4)            //R/W������MAC��ַ���λ
#define NE_PAR1            (NE_BASE+8)            //R/W������MAC��ַ
#define NE_PAR2            (NE_BASE+12)            //R/W������MAC��ַ
#define NE_PAR3            (NE_BASE+16)            //R/W������MAC��ַ
#define NE_PAR4            (NE_BASE+20)            //R/W������MAC��ַ
#define NE_PAR5            (NE_BASE+24)            //R/W������MAC��ַ���λ
#define NE_CURR            (NE_BASE+28)            //R/W�����ջ��廷дָ��
#define NE_MAR0            (NE_BASE+32)            //R/W���鲥�Ĵ��� 
#define NE_MAR1            (NE_BASE+36)            //R/W���鲥�Ĵ��� 
#define NE_MAR2            (NE_BASE+40)            //R/W���鲥�Ĵ��� 
#define NE_MAR3            (NE_BASE+44)            //R/W���鲥�Ĵ��� 
#define NE_MAR4            (NE_BASE+48)            //R/W���鲥�Ĵ��� 
#define NE_MAR5            (NE_BASE+52)            //R/W���鲥�Ĵ��� 
#define NE_MAR6            (NE_BASE+56)            //R/W���鲥�Ĵ��� 
#define NE_MAR7            (NE_BASE+60)            //R/W���鲥�Ĵ��� 
 
//page2 registers (read only in 8019as)
//#define NE_PSTART          0x01            //R�����ջ��廷��ʼҳ
//#define NE_PSTOP           0x02            //R�����ջ��廷��ֹҳ����������ҳ��
//#define NE_TPSR            0x04            //R��Local DMA���ͻ�����ʼҳ�Ĵ���
//#define NE_RCR             0x0c            //R���������üĴ���
//#define NE_TCR             0x0d            //R,�������üĴ���
//#define NE_DCR             0x0e            //R���������üĴ���
//#define NE_IMR             0x0f            //R���������ж����μĴ���IMR״̬


//CR����Ĵ��������� 
#define	CMD_STOP	0x01       //����ֹͣ�շ�����
#define	CMD_RUN	    	0x02       //����ִ�������ʼ�շ����ݰ�������Ϊ�������֣�
#define	CMD_XMIT	0x04       //Local DMA SEND�������D�D>��̫�� ��
#define	CMD_READ	0x08       //Remote DMA READ�������ֶ��������ݣ������D�D>CPU��
#define	CMD_WRITE	0x10       //Remote DMA WRITE ������<�D�DCPU��
#define	CMD_SEND	0x18       //SEND COMMAND��������Զ��������ݰ�                                      �������D�D>CPU��
#define	CMD_NODMA	0x20       //ֹͣDMA����
#define	CMD_PAGE0	0x00       //   ѡ���0ҳ��Ҫ��ѡҳ���ٶ�д��ҳ�Ĵ�����
#define	CMD_PAGE1	0x40       //   ѡ���1ҳ
#define	CMD_PAGE2	0x80       //   ѡ���2ҳ

//д��TPSR��ֵ 
#define	XMIT_START	 0x4000       //���ͻ�����ʼ��ַ��д��ʱҪ����8λ�õ�ҳ�ţ�
//д��PSTART��ֵ 
#define	RECV_START	 0x4600       //���ջ�����ʼ��ַ��д��ʱҪ����8λ�õ�ҳ�ţ�
//д��PSTOP��ֵ 
#define	RECV_STOP	 0x6000       //���ջ��������ַ��д��ʱҪ����8λ�õ�ҳ�ţ�

//�ж�״̬�Ĵ�����ֵ 
#define	ISR_PRX	    	0x01       //��ȷ�������ݰ��жϡ������մ���
#define	ISR_PTX		0x02       //��ȷ�������ݰ��жϡ�����������Ҫ���ϲ�����ˡ�
#define	ISR_RXE	    	0x04       //�������ݰ���������������BNRY��CURR���� 
#define	ISR_TXE	    	0x08       //���ڳ�ͻ�������࣬���ͳ������ط�����
#define	ISR_OVW	    	0x10       //�����ڴ��������������������������ֲᡣ
#define	ISR_CNT	    	0x20       //����������жϣ����ε���������IMR�Ĵ�������
#define	ISR_RDC	    	0x40       //Remote DMA���� �����ε�����ѯ�ȴ�DMA������
#define	ISR_RST		0x80       //����Reset�����ε���

//�ж����μĴ�����ֵ 
#define	ISR_PRX	    	0x01
#define	ISR_PTX		0x02
#define	ISR_RXE	    	0x04
#define	ISR_TXE	    	0x08
#define	ISR_OVW	    	0x10
#define	ISR_CNT 	0x20
#define	ISR_RDC	    	0x40
#define	ISR_RST		0x80

//���ݿ��ƼĴ���
//��ʼ��ʱд��0x48��8λģʽ��FIFO���8�ֽڣ�DMA��ʽ��
#define	DCR_WTS		0x01
#define	DCR_BOS		0x02
#define	DCR_LAS		0x04
#define	DCR_LS		0x08
#define	DCR_ARM		0x10
#define	DCR_FIFO2	0x00
#define	DCR_FIFO4	0x20
#define	DCR_FIFO8	0x40
#define	DCR_FIFO12	0x60

//TCR�������üĴ���
//��ʼ����ʼʱд��0x02��������ΪLoop Backģʽ��ֹͣ�������ݰ���
//��ʼ������д��0x00�������������ݰ�������CRC��
#define	TCR_CRC		0x01
#define	TCR_LOOP_NONE	0x00
#define	TCR_LOOP_INT	0x02
#define	TCR_LOOP_EXT	0x06
#define	TCR_ATD		0x08
#define	TCR_OFST	0x10

//RCR�������üĴ���
//��ʼ��ʱд��0x04��ֻ���շ���������MAC��ַ����64�ֽڵ���̫������㲥��
#define	RCR_SEP		0x01
#define	RCR_AR		0x02
#define	RCR_AB		0x04
#define	RCR_AM		0x08
#define	RCR_PRO		0x10
#define	RCR_MON		0x20


#endif /* SKYEYE_NE2K_H */
