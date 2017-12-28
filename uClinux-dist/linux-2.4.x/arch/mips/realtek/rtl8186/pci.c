#include <linux/config.h>

/*
 *  arch/mips/realtek/rtl865x/pci.c
 *
 *  Copyright (C) 2004 Hsin-I Wu (hiwu@realtek.com.tw)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */ 



#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/paccess.h>
#include <asm/pci_channel.h>
#include <asm-mips/rtl865x/interrupt.h>
#include "pci.h"

int rtl865x_isLinuxIncompliantEndianMode=1;//Default assume 8650B 


#define REG8(offset)	(*(volatile unsigned char *)(offset))
#define REG16(offset)	(*(volatile unsigned short *)(offset))
#define REG32(offset)	(*(volatile unsigned long *)(offset))

#define PCI_ILEV 5

#define PCI_SLOT_NUMBER 4
#if (PCI_SLOT_NUMBER < 1) || (PCI_SLOT_NUMBER > 4)
#error "wrong range of PCI_SLOT_NUMBER, rtl865xB should between 1 and 4"
#endif

static void __init quirk_pci_bridge(struct pci_dev *dev)
{

}

int rtl_pci_isLinuxCompliantEndianMode(){
   	rtl865x_isLinuxIncompliantEndianMode=!((*(volatile unsigned int *)(0xbd012064))&2);
	return ((*(volatile unsigned int *)(0xbd012064))&2);
}

struct pci_fixup pcibios_fixups[] = {	{ PCI_FIXUP_HEADER, PCI_ANY_ID, PCI_ANY_ID, quirk_pci_bridge },	{ 0 }};


#if 0 //8181
#define PCI_SLOT_MEM_BASE 0xbd600000
#define PCI_SLOT_IO_BASE  0xbd500000
#define PCI_MEM_SPACE_SIZE 0x00100000
#define PCI_IO_SPACE_SIZE  0x00100000
#endif

typedef struct BASE_ADDRESS_STRUCT{
  u32 Address;
  enum type_s {
    IO_SPACE=0x80,
    MEM_SPACE
  }Type;
  enum width_s {
    WIDTH32,
    WIDTH64
  }Width;
  u32 Prefetch;
  u32 Size;
}base_address_s;

typedef struct PCI_CONFIG_SPACE_STRUCT{
  u32 Config_Base;  //config space base address
  enum status_s{
    ENABLE,
    DISABLE
  }Status;  //device is enable or disable
  u32 Vendor_Device_ID;
  u8  Revision_ID;
  u32 Class_Code;
  u8  Header_Type;
  base_address_s BAR[6];
  u32 SubSystemVendor_ID;
}pci_config_s;


static pci_config_s *pci_slot[PCI_SLOT_NUMBER][8];

u32 rtlpci_read_config_endian_free(u32 address)
{		
	if(!rtl865x_isLinuxIncompliantEndianMode)
	{
		return le32_to_cpu(REG32(address));		
	}
	return REG32(address);
}

void rtlpci_write_config_endian_free(u32 address,u32 value)
{	

	if(!rtl865x_isLinuxIncompliantEndianMode)
		REG32(address)=le32_to_cpu(value);	
	else
		REG32(address)=value;
}

/* scan the resource needed by PCI device */
void scan_resource(pci_config_s *pci_config, int slot_num, int dev_function, u32 config_base)
{
	int i;
	u32 BaseAddr, data;
		
	BaseAddr = config_base+PCI_CONFIG_BASE0;	

    for (i=0;i<6;i++) {  //detect resource usage
    	rtlpci_write_config_endian_free(BaseAddr,0xffffffff);
    	data = rtlpci_read_config_endian_free(BaseAddr);
    	    	
    	if (data!=0) {  //resource request exist
    	    int j;
    	    if (data&1) {  //IO space
    	    
    	    	pci_config->BAR[i].Type = IO_SPACE;
    	    	    	    	
  	        //scan resource size
#if 0  	        
  	        for (j=2;j<32;j++)
  	            if (data&(1<<j)) break;
  	        if (j<32) pci_config->BAR[i].Size = 1<<j;
  	              else  pci_config->BAR[i].Size = 0;
#else
			pci_config->BAR[i].Size=((~(data&PCI_BASE_ADDRESS_IO_MASK))+1)&0xffff;
#endif
  	        printk("IO Space %i, data=0x%x size=0x%x \n",i,data,pci_config->BAR[i].Size);
  	  
    	    } else {  //Memory space
    	    	
    	    	pci_config->BAR[i].Type = MEM_SPACE;
    	    	//bus width
    	    	if ((data&0x0006)==4) pci_config->BAR[i].Width = WIDTH64; //bus width 64
    	    	else pci_config->BAR[i].Width = WIDTH32;  //bus width 32
    	    	//prefetchable
    	    	if (data&0x0008) pci_config->BAR[i].Prefetch = 1; //prefetchable
    	    	else pci_config->BAR[i].Prefetch = 0;  //no prefetch
  	        //scan resource size
  	        if (pci_config->BAR[i].Width==WIDTH32) {
  	          for (j=4;j<32;j++)
  	            if (data&(1<<j)) break;
  	          if (j<32) pci_config->BAR[i].Size = 1<<j;
  	          else  pci_config->BAR[i].Size = 0;  	          
  	        } else //width64 is not support
  	          {
  	          	pci_config->BAR[i].Size = 0;
  	          }
  	        printk("Memory Space %i data=0x%x size=0x%x\n",i,data,pci_config->BAR[i].Size);	
    	    }
    	} else {  //no resource
    	    memset(&(pci_config->BAR[i]), 0, sizeof(base_address_s));    		
    	}
	BaseAddr += 4;  //next base address
    }
    
}

/* scan the PCI bus, save config information into pci_slot,
   return the number of PCI functions */
int rtl_pci_scan_slot(void)
{
    u32 config_base, data, vendor_device;
    int i, dev_function;
    int dev_num = 0;
    u16 device,vendor;
	
	for (i=0;i<PCI_SLOT_NUMBER;i++) {  //probe 2 pci slots
#ifndef CONFIG_RTL865XB_PCI_SLOT0
		if(i==0) continue;
#endif
#ifndef CONFIG_RTL865XB_PCI_SLOT1
		if(i==1) continue;
#endif
#ifndef CONFIG_RTL865XB_PCI_SLOT2
		if(i==2) continue;
#endif
#ifndef CONFIG_RTL865XB_PCI_SLOT3
		if(i==3) continue;
#endif


   		// this delay is necessary to 
   		// work around pci configuration timing issue       		
   	   	config_base = 0;

   	   	while(config_base < 3000000){
   	   		config_base++;
   	   	}   
       	   	
    	switch(i){	
       	case 0:      	   	       	   	
           	config_base = PCI_SLOT0_CONFIG_BASE;           
           	break;

        case 1:
           	config_base = PCI_SLOT1_CONFIG_BASE;
           	break;
           	
       	case 2:      	   	       	   	
           	config_base = PCI_SLOT2_CONFIG_BASE;           
           	break;
           	
       	case 3:      	   	       	   	
           	config_base = PCI_SLOT3_CONFIG_BASE;           
           	break;
           	
       	default:
       		return 0;
		}
      
      dev_function=0;


//  	  printk("timeout=%x base=%x\n",REG32(0xbd012004),config_base+PCI_CONFIG_VENDER_DEV);
//  	  printk("timeout=%x\n",REG32(0xbd012004));

	vendor_device= rtlpci_read_config_endian_free(config_base+PCI_CONFIG_VENDER_DEV);



  	  vendor=vendor_device&0xffff;
  	  device=vendor_device>>16;

 	  
      while (dev_function<8) {  //pci device exist

	  switch(vendor)
	  {
	  	case 0x17a0: //Genesys
	      	if(!((dev_function==0)||(dev_function==3)) )
      		{
      			goto next_func;
      		}
	  		printk("Found Genesys USB 2.0 PCI Card!, function=%d!\n",dev_function);
			//REG32(config_base+0x50)&=(0xffffffef);
	      	break;
	      	
		case PCI_VENDOR_ID_AL: //ALi
		    if(dev_function>3)
      		{
      			goto next_func;
      		}
	  		printk("Found ALi USB 2.0 PCI Card, function=%d!\n",dev_function);			      	
	      	break;
	
		case PCI_VENDOR_ID_VIA: //0x1106
	      	if(dev_function>2)
      		{
      			goto next_func;
      		}
	  		printk("Found VIA USB 2.0 PCI Card!, function=%d!\n",dev_function);			      	
	      	break;

		case PCI_VENDOR_ID_NEC: //0x1033
	      	if(dev_function>2)
      		{
      			goto next_func;
      		}
	  		printk("Found NEC USB 2.0 PCI Card, function=%d!\n",dev_function);			      	
	      	break;

		case PCI_VENDOR_ID_REALTEK:
			if(dev_function>0) ////assume realtek's chip only have one function.
			{
				if((device==0x8139)||(device==0x8185))
				{
					goto next_func;
				}
			}
			if(device==0x8139)
		  		printk("Found Realtek 8139 PCI Card, function=%d!\n",dev_function);
			else if(device==0x8185)
		  		printk("Found Realtek 8185 PCI Card, function=%d!\n",dev_function);						
			break;

		case 0x168c: // Atheros
			if(dev_function>0) //assume atheros's chip only have one function.
			{
				//if(device==0x0013)
				{
					goto next_func;
				}
			}
			if(device==0x0013)			
				printk("Found Atheros AR5212 802.11a/b/g PCI Card, function=%d!\n",dev_function);
			else if(device==0x0012)
				printk("Found Atheros AR5211 802.11a/b PCI Card, function=%d!\n",dev_function);				
			else
				printk("Found Atheros unknow PCI Card, function=%d!\n",dev_function);				
			break;
	      	
		case PCI_VENDOR_ID_PROMISE: //0x105a
	      	if(dev_function>0)
      		{
next_func:     
      			dev_function++;
      			config_base += 256;
      			continue;
      		}
	  		printk("Found Promise IDE PCI Card, function=%d!\n",dev_function);
      	break;

      	default:
      		break;
	  	}

   		  if ((0==rtlpci_read_config_endian_free(config_base+PCI_CONFIG_VENDER_DEV))||
   		  	  ((rtlpci_read_config_endian_free(config_base+PCI_CONFIG_VENDER_DEV)&0xffff)==0xffff))
   		  { 		  
   		  	
		  	dev_function++;
        	  config_base += 256;  //next function's config base
        	  continue;
   		 }   		  

          pci_slot[i][dev_function] = kmalloc(sizeof(pci_config_s),GFP_KERNEL);
          pci_slot[i][dev_function]->Config_Base = config_base;
          pci_slot[i][dev_function]->Status = DISABLE;
          pci_slot[i][dev_function]->Vendor_Device_ID = rtlpci_read_config_endian_free(config_base+PCI_CONFIG_VENDER_DEV);
          data = rtlpci_read_config_endian_free(config_base+PCI_CONFIG_CLASS_REVISION);
          pci_slot[i][dev_function]->Revision_ID = data&0x00FF;
          pci_slot[i][dev_function]->Class_Code = data>>8;
          pci_slot[i][dev_function]->Header_Type = (rtlpci_read_config_endian_free(config_base+PCI_CONFIG_CACHE)>>16)&0x000000FF;
          pci_slot[i][dev_function]->SubSystemVendor_ID = rtlpci_read_config_endian_free(config_base+PCI_CONFIG_SUBSYSTEMVENDOR);
          scan_resource(pci_slot[i][dev_function], i, dev_function, config_base);  //probe resource request
          	
          printk("PCI device exists: slot %d function %d VendorID %x DeviceID %x %x\n", i, dev_function,           	
              pci_slot[i][dev_function]->Vendor_Device_ID&0x0000FFFF,
              pci_slot[i][dev_function]->Vendor_Device_ID>>16,config_base);
          dev_num++;
          dev_function++;
          ////config_base += dev_function*64*4;  //next function's config base
          config_base += 256;  //next function's config base
//          if (!(pci_slot[i][0]->Header_Type&0x80)) break;  //single function card
          if (dev_function>=8) break;  //only 8 functions allow in a PCI card  
      }      
    }	
    return dev_num;
}

/* sort resource by its size */
void bubble_sort(int space_size[PCI_SLOT_NUMBER*8*6][2], int num)
{
  int i, j;
  int tmp_swap[2];
  
    for (i=0;i<num-1;i++) {
    	for (j=i;j<num-1;j++) {
          if (space_size[j][1]<space_size[j+1][1]) {
            tmp_swap[0] = space_size[j][0];
            tmp_swap[1] = space_size[j][1];
            space_size[j][0] = space_size[j+1][0];
            space_size[j][1] = space_size[j+1][1];
            space_size[j+1][0] = tmp_swap[0];
            space_size[j+1][1] = tmp_swap[1];
          };
        };
    };
}



int rtl_pci_reset(){
	u32 tmp, divisor, scr;	
	u32 ratio[]={5,5,4,5,4,4,3,2};//check Datasheet SCR and MACCR.
	tmp = REG32(MACCR)&0xFCFF8FFF; 
    REG32(MACMR) = REG32(MACMR) & ~SYS_CLK_MASK;
	scr=(REG32(MACMR)>>16)&7;	
	REG32(MACCR) = tmp|(1<<15);//reset PCI bridge
	divisor=ratio[scr];    
	//patch: set PCI clk = /6
	//divisor=5;     
	REG32(MACCR) = tmp|(divisor<<12);
	return divisor;
}

/* scan pci bus, assign Memory & IO space to PCI card, 
	and call init_XXXX to enable & register PCI device
	rtn 0:ok, else:fail */
int rtl_pci_init(void)
{
  	int function_num;
	u32 tmp, divisor, scr;
	tmp=rtl_pci_isLinuxCompliantEndianMode();
	printk("NEW PCI Driver...isLinuxCompliantEndianMode=%s\n",(tmp==2)?"True(Little Endian)":"False(Big Endian)");
	rtl_pci_reset();
	

#if 0
	printk("SLOT 0: %x\n", REG32(PCI_SLOT0_CONFIG_BASE));
	printk("SLOT 1: %x\n", REG32(PCI_SLOT1_CONFIG_BASE));
	printk("SLOT 2: %x\n", REG32(PCI_SLOT2_CONFIG_BASE));
	printk("SLOT 3: %x\n", REG32(PCI_SLOT3_CONFIG_BASE));
	printk("REG32(0xbd012004)&0x10000=%x\n",REG32(0xbd012004)&0x10000);
#endif	

  	
    //printk("Probe PCI Bus : There must be one device at the slot.\n");
    
    memset(pci_slot, 0, 4*PCI_SLOT_NUMBER*8);

    function_num = rtl_pci_scan_slot();

    if (function_num==0) {
    	printk("No PCI device exist!!\n");
    	return -1;
    };
	
    //auto assign resource
    if (rtl_pci_assign_resource()) {
    	printk("PCI Resource assignment failed!\n");
    	return -2;
    };

    printk("Find Total %d PCI functions\n", function_num);
    return 0;
      
}





static int rtlpci_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	u32 tmp;
	u32 addr;
	u32 shift=3-(where & 0x3);	
	
	
	if(PCI_FUNC(dev->devfn) >= 8) return PCIBIOS_FUNC_NOT_SUPPORTED;
	if(PCI_SLOT(dev->devfn) >= PCI_SLOT_NUMBER) return PCIBIOS_FUNC_NOT_SUPPORTED;

	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)] == NULL)
		return PCIBIOS_FUNC_NOT_SUPPORTED;

	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Status != ENABLE)
		return PCIBIOS_FUNC_NOT_SUPPORTED;
			
	addr = pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Config_Base;
	
	if(addr == 0) return PCIBIOS_FUNC_NOT_SUPPORTED;
	
	if (dev->bus->number != 0)
	{
		*value = 0;
		return PCIBIOS_SUCCESSFUL;
	}

	

	if(rtl865x_isLinuxIncompliantEndianMode)
	{
		addr |= (where&~0x3);
		tmp = REG32(addr);
		for (addr=0;addr<(3-shift);addr++)
			tmp = tmp >>8;
		*value = (u8)tmp;
	}
	else
	{
		//*value = REG8(addr+where);
		addr |= (where&0xfffffffc);
		tmp = REG32(addr);
		for (addr=0;addr<shift;addr++)
			tmp = tmp >>8;
		*value = (u8)tmp;		
	}
	
	return PCIBIOS_SUCCESSFUL;
}

static int rtlpci_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	
	u32 tmp,addr;

	if(PCI_FUNC(dev->devfn) >= 8) return PCIBIOS_FUNC_NOT_SUPPORTED;
	if(PCI_SLOT(dev->devfn) >= PCI_SLOT_NUMBER) return PCIBIOS_FUNC_NOT_SUPPORTED;

	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)] == NULL) 
		return PCIBIOS_FUNC_NOT_SUPPORTED;

	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Status != ENABLE)
		return PCIBIOS_FUNC_NOT_SUPPORTED;

	addr = pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Config_Base;
	
	if(addr == 0) return PCIBIOS_FUNC_NOT_SUPPORTED;

	if (dev->bus->number != 0)
	{
		*value = 0;
		return PCIBIOS_SUCCESSFUL;
	}

	addr |= (where&~0x3);	

	if(rtl865x_isLinuxIncompliantEndianMode)
	{
		tmp=REG32(addr);
		if (where&0x2)
			*value = (u16)(tmp>>16);
		else
			*value = (u16)(tmp);
	}
	else
	{
		tmp=REG32(addr);
		if(where&0x2)
			*value=le16_to_cpu((u16)(tmp));
		else
			*value=le16_to_cpu((u16)(tmp>>16));
	}
	
	return PCIBIOS_SUCCESSFUL;
}

static int rtlpci_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	u32 addr;


	if(PCI_FUNC(dev->devfn) >= 8) return PCIBIOS_FUNC_NOT_SUPPORTED;

	if(PCI_SLOT(dev->devfn) >= PCI_SLOT_NUMBER) return PCIBIOS_FUNC_NOT_SUPPORTED;

	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)] == NULL)
		return PCIBIOS_FUNC_NOT_SUPPORTED;
		
	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Status != ENABLE)
		return PCIBIOS_FUNC_NOT_SUPPORTED;		

	addr = pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Config_Base;
	

	if(addr == 0) return PCIBIOS_FUNC_NOT_SUPPORTED;

		
	if (dev->bus->number != 0)
	{
		*value=0;
		return PCIBIOS_SUCCESSFUL;
	}

	if(rtl865x_isLinuxIncompliantEndianMode)
	{
		*value = REG32(addr+where);		
	}
	else
	{
		*value = le32_to_cpu(REG32(addr+where));		
	}
	

//	*value = REG32(addr+where);
	
	return PCIBIOS_SUCCESSFUL;
}


static int rtlpci_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	u32 tmp;
	u32 addr;
	u32 shift=(where & 0x3);	
	
	
	if(PCI_FUNC(dev->devfn) >= 8) return PCIBIOS_FUNC_NOT_SUPPORTED;
	if(PCI_SLOT(dev->devfn) >= PCI_SLOT_NUMBER) return PCIBIOS_FUNC_NOT_SUPPORTED;
		
	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)] == NULL)
		return PCIBIOS_FUNC_NOT_SUPPORTED;
				
	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Status != ENABLE)
		return PCIBIOS_FUNC_NOT_SUPPORTED;				
				
	addr = pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Config_Base;
	if(addr == 0) return PCIBIOS_FUNC_NOT_SUPPORTED;

	if (dev->bus->number != 0)		
		return PCIBIOS_SUCCESSFUL;
	

	
	if(rtl865x_isLinuxIncompliantEndianMode)
	{
		u32 newval=(u32)value,mask=0xff;
		addr |= (where&0xfffffffc);
		tmp = REG32(addr);
		newval <<= (8*(shift));
		mask <<= (8*(shift));
		REG32(addr)=(tmp&(~mask))|newval;	

	}
	else
	{
		//*value = REG8(addr+where);
		u32 newval=(u32)value,mask=0xff;
		addr |= (where&0xfffffffc);
		tmp = REG32(addr);
		newval <<= (8*(3-shift));
		mask <<= (8*(3-shift));
		REG32(addr)=(tmp&(~mask))|newval;
	}
	
	return PCIBIOS_SUCCESSFUL;
}

static int rtlpci_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	u32 addr;
	
	if(PCI_FUNC(dev->devfn) >= 8) return PCIBIOS_FUNC_NOT_SUPPORTED;
	if(PCI_SLOT(dev->devfn) >= PCI_SLOT_NUMBER) return PCIBIOS_FUNC_NOT_SUPPORTED;	
	
	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)] == NULL)
		return PCIBIOS_FUNC_NOT_SUPPORTED;

	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Status != ENABLE)
		return PCIBIOS_FUNC_NOT_SUPPORTED;
				
	addr = pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Config_Base;
	
	if(addr == 0) return PCIBIOS_FUNC_NOT_SUPPORTED;
	
	if (dev->bus->number != 0)		
		return PCIBIOS_SUCCESSFUL;

	if(rtl865x_isLinuxIncompliantEndianMode)
	{
		u32 tmp;
		addr |= (where&0xfffffffc);
		tmp = REG32(addr);
		if(where&2)			
			REG32(addr)=(tmp&0xffff)|(((u32)(value)<<16));			
		else
			REG32(addr)=(tmp&0xffff0000)|((u32)(value));
	}
	else
	{
		u32 tmp;
		addr |= (where&0xfffffffc);
		tmp = REG32(addr);
		if(where&2)			
			REG32(addr)=(tmp&0xffff0000)|((u32)(le16_to_cpu(value)));			
		else
			REG32(addr)=(tmp&0xffff)|((u32)((le16_to_cpu(value))<<16));
			
	}	

	return PCIBIOS_SUCCESSFUL;
}

static int rtlpci_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	u32 addr;

	if(PCI_FUNC(dev->devfn) >= 8) return PCIBIOS_FUNC_NOT_SUPPORTED;
	if(PCI_SLOT(dev->devfn) >= PCI_SLOT_NUMBER) return PCIBIOS_FUNC_NOT_SUPPORTED;
	
	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)] == NULL)
		return PCIBIOS_FUNC_NOT_SUPPORTED;
		
	if(pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Status != ENABLE)
		return PCIBIOS_FUNC_NOT_SUPPORTED;		
				
	addr = pci_slot[PCI_SLOT(dev->devfn)][PCI_FUNC(dev->devfn)]->Config_Base;
	
	if(addr == 0) return PCIBIOS_FUNC_NOT_SUPPORTED;
	
	if (dev->bus->number != 0)		
		return PCIBIOS_SUCCESSFUL;


	if(rtl865x_isLinuxIncompliantEndianMode)
	{
		REG32(addr+where)=value;		
	}
	else
	{
		REG32(addr+where)=le32_to_cpu(value);
	}
		
	return PCIBIOS_SUCCESSFUL;
}


static struct pci_ops pcibios_ops = {
	rtlpci_read_config_byte,
	rtlpci_read_config_word,
	rtlpci_read_config_dword,
	rtlpci_write_config_byte,
	rtlpci_write_config_word,
	rtlpci_write_config_dword
};

#if 1 //865x
/*
#define PCI_IO_START      0x10000000
#define PCI_IO_END        0x1000ffff
#define PCI_MEM_START     0x18000000
#define PCI_MEM_END       0x18ffffff
*/

#define PCI_IO_START      0x1be00000
#define PCI_IO_END        0x1befffff
#define PCI_MEM_START     0x1bf00000
#define PCI_MEM_END       0x1bffffff



#else //8181

#define PCI_IO_START      (0xbd500000 - 0xbd010000)
#define PCI_IO_END        (0xbdfffff - 0xbd010000)
#define PCI_MEM_START     0xbd600000
#define PCI_MEM_END       0xbd6fffff
#endif

static struct resource pci_io_resource = {
	"pci IO space", 
	PCI_IO_START,
	PCI_IO_END,
	IORESOURCE_IO
};

static struct resource pci_mem_resource = {
	"pci memory space", 
	PCI_MEM_START,
	PCI_MEM_END,
	IORESOURCE_MEM
};


struct pci_channel mips_pci_channels[] = {
	{&pcibios_ops, &pci_io_resource, &pci_mem_resource, 0, 1},
	{(struct pci_ops *) NULL, (struct resource *) NULL,
	 (struct resource *) NULL, (int) NULL, (int) NULL}
};


int pcibios_enable_resources(struct pci_dev *dev)
{
	return 0;
}

#if 0
static void memdump(unsigned address, unsigned length) {
	unsigned data, currentaddress, i, j;
	char c;

	i=0;
	printk("Memory dump: start address 0x%8.8x , length %d bytes\n\n\r",address,length);
	printk("Address        Data\n\r");

	while(i<length) {
		currentaddress = address + i;
		data = *(unsigned long*)(currentaddress);
		printk("0x%8.8x:   %8.8x    ",currentaddress,data);
		for(j=0;j<4;j++) {
			c = (data >> 8*j) & 0xFF;
			if(c<0x20 || c>0x7e) {
				printk("\\%3.3d ",(int)c);
			} else {
				printk(" %c   ",c);
			}
		}
		printk("\n\r");
		i += 4;
	}
	printk("\n\rEnd of memory dump.\n\n\r");
}
#endif /* if 0 */

/*
 *  If we set up a device for bus mastering, we need to check the latency
 *  timer as certain crappy BIOSes forget to set it properly.
 *
 *  
 */


unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}


void __init pcibios_fixup_resources(struct pci_dev *dev)
{	
#if 1	
	int i;
	/* Search for the IO base address.. */
	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		unsigned int io_addr = pci_resource_start(dev, i);
		unsigned int io_size = pci_resource_len(dev, i);
		unsigned int new_io_addr ;
		if (!(pci_resource_flags(dev,i) & IORESOURCE_IO))
			continue;
		// to work around pci card with IO mappping <64K limitation
		io_addr &= 0x000fffff;
		io_addr |= 0x1d500000;

		pci_resource_start(dev, i) = (io_addr - 0x1D010000);
		pci_resource_end(dev, i) = (io_addr + io_size - 0x1D010000 -1 );
		new_io_addr = pci_resource_start(dev, i);
		printk("pcibios_fixup_resources IO form %x to %x\n",io_addr,new_io_addr);
		
		/* Is it already in use? */
		if (check_region (new_io_addr, io_size))
			break;		
	}
#endif 		
}



void __init pcibios_fixup(void)
{
	/* nothing to do here */
}

void __init pcibios_fixup_irqs(void)
{
    struct pci_dev *dev;

	pci_for_each_dev(dev) {
		dev->irq = 5; // fix irq
	}
}

/*	assign memory location to MEM & IO space
	return 0:OK, else:fail */
int rtl_pci_assign_resource(void)
{
  	int i, slot, func, BARnum;
  	int mem_space_size[PCI_SLOT_NUMBER*8*6][2]; //[0]:store device index, [1]:store resource size
  	int io_space_size[PCI_SLOT_NUMBER*8*6][2]; //[0]:store device index, [1]:store resource size
  	int mem_idx, io_idx, total_size, tmp;
  	u16 config_command[PCI_SLOT_NUMBER][8];  	//record config space resource usage, 

#if defined(CONFIG_RTL865XB_PCI_SLOT2)||defined(CONFIG_RTL865XB_PCI_SLOT3)
	printk("PCI: PCI slot 2/3 inuse. Config GPIOA pins for PCI use\n");
 	REG32(0xbd012054)|=0xf0000000;			
	REG32(0xbd01200c)|=0xf0000000;			
#endif
  
    memset(mem_space_size, 0, sizeof(mem_space_size));
    memset(io_space_size, 0, sizeof(io_space_size));
    memset(config_command, 0, sizeof(config_command));
    
    //collect resource
    mem_idx = io_idx =0;
    for (slot=0;slot<PCI_SLOT_NUMBER;slot++) {
#ifndef CONFIG_RTL865XB_PCI_SLOT0
		if(slot==0) continue;
#endif
#ifndef CONFIG_RTL865XB_PCI_SLOT1
		if(slot==1) continue;
#endif
#ifndef CONFIG_RTL865XB_PCI_SLOT2
		if(slot==2) continue;
#endif
#ifndef CONFIG_RTL865XB_PCI_SLOT3
		if(slot==3) continue;
#endif



    	
      if (pci_slot[slot][0]==0) continue;  //this slot is null      
      if (pci_slot[slot][0]->Vendor_Device_ID==0) continue;  //this slot is null      
      for (func=0;func<8;func++) {
        if (pci_slot[slot][func]==0) continue;
        pci_slot[slot][func]->Status = ENABLE;
        for (BARnum=0;BARnum<6;BARnum++) {
      		if (pci_slot[slot][func]->BAR[BARnum].Type==MEM_SPACE){  //memory space
      			printk("memory mapping BAnum=%d slot=%d func=%d\n",BARnum,slot,func);
      		    config_command[slot][func] |= CMD_MEM_SPACE;  //this device use Memory
      		    mem_space_size[mem_idx][0] = (slot<<16)|(func<<8)|(BARnum<<0);
      		    mem_space_size[mem_idx][1] = pci_slot[slot][func]->BAR[BARnum].Size;
      		    mem_idx++;
      		} else if (pci_slot[slot][func]->BAR[BARnum].Type==IO_SPACE){  //io space
	      		printk("io mapping BAnum=%d slot=%d func=%d\n",BARnum,slot,func);
      		    config_command[slot][func] |= CMD_IO_SPACE;  //this device use IO
      		    io_space_size[io_idx][0] = (slot<<16)|(func<<8)|(BARnum<<0);
      		    io_space_size[io_idx][1] = pci_slot[slot][func]->BAR[BARnum].Size;
      		    io_idx++;
      		}
      		else
      		{
//      			printk("unknow mapping BAnum=%d slot=%d func=%d\n",BARnum,slot,func);
      		}
	    }  //for (BARnum=0;BARnum<6;BARnum++) 
      }  //for (func=0;func<8;func++)
    } //for (slot=0;slot<PCI_SLOT_NUMBER;slot++)

    //sort by size
    if (mem_idx>1) bubble_sort(mem_space_size, mem_idx);
    if (io_idx>1)  bubble_sort(io_space_size, io_idx);   
    
    //check mem total size
    total_size = 0;
    for (i=0;i<mem_idx;i++) {  	
    	tmp = mem_space_size[i][1]-1;
        total_size = (total_size+tmp)&(~tmp);     
        total_size = total_size + mem_space_size[i][1];
    };
    if (total_size>PCI_MEM_SPACE_SIZE) {
	printk("lack of memory map space resource \n");
	return -1;  //lack of memory space resource
    }

    //check io total size
    total_size = 0;
    for (i=0;i<io_idx;i++) {     	
    	tmp = io_space_size[i][1]-1;
        total_size = (total_size+tmp)&(~tmp);     
        total_size = total_size + io_space_size[i][1];
    }
    
    if (total_size>PCI_IO_SPACE_SIZE) 
    {
	printk("lack of io map space resource\n");
		return -2;  //lack of io space resource
    }
    

    //assign memory space
    total_size = 0;
    for (i=0;i<mem_idx;i++) {
    	unsigned int config_base;     	
    	tmp = mem_space_size[i][1]-1;
        total_size = (total_size+tmp)&(~tmp);        
        tmp = mem_space_size[i][0];
		
        //assign to struct
        if (pci_slot[(tmp>>16)][(tmp>>8)&0x00FF]->BAR[tmp&0x00FF].Type!= MEM_SPACE)
        	continue;
        
        pci_slot[(tmp>>16)][(tmp>>8)&0x00FF]->BAR[tmp&0x00FF].Address = PCI_SLOT_MEM_BASE+total_size;
     	switch((tmp>>16)){
     	case 0:
     		config_base = PCI_SLOT0_CONFIG_BASE;
     		break;
     	case 1:
     		config_base = PCI_SLOT1_CONFIG_BASE;
     		break;
     	case 2:
     		config_base = PCI_SLOT2_CONFIG_BASE;
     		break;     		
     	case 3:
     		config_base = PCI_SLOT3_CONFIG_BASE;
     		break;     		
     	default:
     		panic("PCI slot assign error");
    	}        
        //get BAR address and assign to PCI device
        tmp = config_base   //SLOT
              +((tmp>>8)&0x00FF)*64*4					//function
              +((tmp&0x00FF)*4+PCI_CONFIG_BASE0);			//BAR bumber
//        REG32(tmp) = (PCI_SLOT_MEM_BASE+total_size)&0x1FFFFFFF;  //map to physical address
        rtlpci_write_config_endian_free(tmp,(PCI_SLOT_MEM_BASE+total_size)&0x1FFFFFFF);
        printk("assign mem base %x~%x at %x size=%d\n",(PCI_SLOT_MEM_BASE+total_size)&0x1FFFFFFF,((PCI_SLOT_MEM_BASE+total_size)&0x1FFFFFFF)+mem_space_size[i][1]-1,tmp,mem_space_size[i][1]);
	
        total_size = total_size + mem_space_size[i][1];  //next address
    };
    //assign IO space
    total_size = 0;
    for (i=0;i<io_idx;i++) {
    	unsigned int config_base;    	
    	tmp = io_space_size[i][1]-1;
        total_size = (total_size+tmp)&(~tmp);        
        tmp = io_space_size[i][0];
        //assign to struct
        if (pci_slot[(tmp>>16)][(tmp>>8)&0x00FF]->BAR[tmp&0x00FF].Type!= IO_SPACE)
        	continue;

        pci_slot[(tmp>>16)][(tmp>>8)&0x00FF]->BAR[tmp&0x00FF].Address = PCI_SLOT_IO_BASE+total_size;
        //get BAR address and assign to PCI device
     	switch((tmp>>16)){
     	case 0:
     		config_base = PCI_SLOT0_CONFIG_BASE;
     		break;
     	case 1:
     		config_base = PCI_SLOT1_CONFIG_BASE;
     		break;
     	case 2:
     		config_base = PCI_SLOT2_CONFIG_BASE;
     		break;
     	case 3:
     		config_base = PCI_SLOT3_CONFIG_BASE;
     		break;      		
     	default:
     		panic("PCI slot assign error");
    	}
        tmp = config_base   //SLOT
              +((tmp>>8)&0x00FF)*64*4					//function
              +((tmp&0x00FF)*4+PCI_CONFIG_BASE0);			//BAR bumber
        
//        REG32(tmp) = (PCI_SLOT_IO_BASE+total_size)&0x1FFFFFFF;  //map to physical address
	 rtlpci_write_config_endian_free(tmp,(PCI_SLOT_IO_BASE+total_size)&0x1FFFFFFF);
	 
	printk("assign I/O base %x~%x at %x size=%d\n",(PCI_SLOT_IO_BASE+total_size)&0x1FFFFFFF,((PCI_SLOT_IO_BASE+total_size)&0x1FFFFFFF)+io_space_size[i][1]-1,tmp,io_space_size[i][1]);
	total_size += io_space_size[i][1];  //next address	
       
        
    }
    
    //enable device
    for (slot=0;slot<PCI_SLOT_NUMBER;slot++) {

	#ifndef CONFIG_RTL865XB_PCI_SLOT0
			if(slot==0) continue;
	#endif
	#ifndef CONFIG_RTL865XB_PCI_SLOT1
			if(slot==1) continue;
	#endif
	#ifndef CONFIG_RTL865XB_PCI_SLOT2
			if(slot==2) continue;
	#endif
	#ifndef CONFIG_RTL865XB_PCI_SLOT3
			if(slot==3) continue;
	#endif
    	
      if (pci_slot[slot][0]==0) continue;  //this slot is null      
      if (pci_slot[slot][0]->Vendor_Device_ID==0) continue;  //this slot is null      
      for (func=0;func<8;func++) {
      	unsigned int config_base;
        if (pci_slot[slot][func]==0) continue;  //this slot:function is null      
        if (pci_slot[slot][func]->Vendor_Device_ID==0) continue;  //this slot:function is null      
        //get config base address
     	switch(slot){
     	case 0:
     		config_base = PCI_SLOT0_CONFIG_BASE;
     		break;
     	case 1:
     		config_base = PCI_SLOT1_CONFIG_BASE;
     		break;
     	case 2:
     		config_base = PCI_SLOT2_CONFIG_BASE;
     		break;
     	case 3:
     		config_base = PCI_SLOT3_CONFIG_BASE;
     		break;      		
     	default:
     		panic("PCI slot assign error");
    	}        
        
        tmp = config_base   //SLOT
              +func*64*4;					   //function
        //set Max_Lat & Min_Gnt, irq
        //printk("1 REG32(tmp+PCI_CONFIG_INT_LINE) = %x\n",REG32(tmp+PCI_CONFIG_INT_LINE));
		//REG32(tmp+PCI_CONFIG_INT_LINE) = 0x40200100|(PCI_ILEV); //IRQ level
		rtlpci_write_config_endian_free(tmp+PCI_CONFIG_INT_LINE,0x40200100|(PCI_ILEV));
		//printk("2 REG32(tmp+PCI_CONFIG_INT_LINE) = %x\n",REG32(tmp+PCI_CONFIG_INT_LINE));

		//enable cache line size, lantancy
		//REG32(tmp+PCI_CONFIG_CACHE) = (REG32(tmp+PCI_CONFIG_CACHE)&0xFFFF0000)|0x2004;  //32 byte cache, 20 latency
		//printk("(REG32(tmp+PCI_CONFIG_CACHE)&0xFFFF0000)|0x2004=%x\n",(REG32(tmp+PCI_CONFIG_CACHE)&0xFFFF0000)|0x2004);
		rtlpci_write_config_endian_free(tmp+PCI_CONFIG_CACHE,(rtlpci_read_config_endian_free(tmp+PCI_CONFIG_CACHE)&0xFFFF0000)|0x2004);
	
		
		
        //set command register
        //REG32(tmp+PCI_CONFIG_COMMAND) = (REG32(tmp+PCI_CONFIG_COMMAND)&0xFFFF0000)|config_command[slot][func]
        //					|CMD_BUS_MASTER|CMD_WRITE_AND_INVALIDATE|CMD_PARITY_ERROR_RESPONSE;
	// this delay is necessary to 
       	// work around pci configuration timing issue
       	config_base = 0;	       	
       	while(config_base < 1000){
       	config_base++;
       	} 


	if(!rtl865x_isLinuxIncompliantEndianMode){
		u32 command_status=rtlpci_read_config_endian_free(tmp+PCI_CONFIG_COMMAND);
		command_status=(config_command[slot][func]|CMD_BUS_MASTER|CMD_PARITY_ERROR_RESPONSE)|(command_status&0xffff0000);
		rtlpci_write_config_endian_free(tmp+PCI_CONFIG_COMMAND,command_status);		
	}
	else
	{
       	REG16(tmp+PCI_CONFIG_COMMAND) = config_command[slot][func]
        	|CMD_BUS_MASTER|CMD_PARITY_ERROR_RESPONSE;//|CMD_WRITE_AND_INVALIDATE;
	}


      };
    };

    return 0;
}

 u8 rtl865x_pci_ioread8(u32 addr){
 	//For legacy 8650's linux incompliant PCI access endian mode 	
	if(rtl865x_isLinuxIncompliantEndianMode){
	
		u8 tmp;
		u32 tmp32;
		int diff = addr&0x3;
		addr=(0xa0000000|addr)&(~0x3);
		tmp32=*(volatile unsigned int *)(addr);

		tmp=(u8)((tmp32>>(diff*8))&0xff);
		return tmp;
	}
		return (*(volatile unsigned char *)(addr|0xa0000000));

}

 u16 rtl865x_pci_ioread16(u32 addr){
 	//For legacy 8650's linux incompliant PCI access endian mode 	
	if(rtl865x_isLinuxIncompliantEndianMode){
		u16 tmp;
		u32 tmp32;
		int diff = addr&0x3;
		addr=(0xa0000000|addr)&(~0x3);
		tmp32=*(volatile unsigned int *)(addr);
		tmp=(u16)((tmp32>>(diff*8))&0xffff);
		return tmp;
	}
	return cpu_to_le16((*(volatile unsigned short *)(addr|0xa0000000)));
}

u32 rtl865x_pci_ioread32(u32 addr){	
 	//For legacy 8650's linux incompliant PCI access endian mode 	
	if(rtl865x_isLinuxIncompliantEndianMode){
		u32 tmp;
		tmp= *(volatile u32*)(0xa0000000|addr);		
		return tmp;
	}
	return cpu_to_le32((*(volatile unsigned int *)(addr|0xa0000000)));
}


void rtl865x_pci_iowrite8(u32 addr, u8 val){
	if(rtl865x_isLinuxIncompliantEndianMode)
	{
		u32 tmp,tmp2;
		u32 shift=addr&0x3;
		addr&=(~0x3);
		tmp=rtl865x_pci_ioread32(addr);
		tmp2=(u32)val;
		tmp&=(~(0xff<<(shift*8)));
		tmp|=(tmp2<<(shift*8));				
		(*(volatile unsigned int*)(addr|0xa0000000))=tmp;		
	}
	else
		(*(volatile unsigned char *)(addr|0xa0000000)) = val;
}

 void rtl865x_pci_iowrite16(u32 addr, u16 val){
	if(rtl865x_isLinuxIncompliantEndianMode)
	{
		u32 tmp,tmp2;
		u32 shift=addr&0x3;
		addr&=(~0x3);
		tmp=rtl865x_pci_ioread32(addr);
		tmp2=(u32)val;
		tmp&=(~(0xffff<<(shift*8)));
		tmp|=(tmp2<<(shift*8));				
		(*(volatile unsigned int*)(addr|0xa0000000))=tmp;	
	}
	else
		(*(volatile unsigned short *)(addr|0xa0000000)) = cpu_to_le16(val);
}

 void rtl865x_pci_iowrite32(u32 addr, u32 val){
	if(rtl865x_isLinuxIncompliantEndianMode)		
	{
		(*(volatile unsigned long*)(addr|0xa0000000))=val;
	}
	else
		(*(volatile unsigned long *)(addr|0xa0000000)) = cpu_to_le32(val);

}

EXPORT_SYMBOL(rtl865x_pci_iowrite32);
EXPORT_SYMBOL(rtl865x_pci_iowrite16);
EXPORT_SYMBOL(rtl865x_pci_iowrite8);
EXPORT_SYMBOL(rtl865x_pci_ioread32);
EXPORT_SYMBOL(rtl865x_pci_ioread16);
EXPORT_SYMBOL(rtl865x_pci_ioread8);

