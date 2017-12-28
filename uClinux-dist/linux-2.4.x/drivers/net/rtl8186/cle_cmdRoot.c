#include <linux/config.h>

#ifdef __linux__
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#endif
#include "rtl_types.h"

/*command line engine supported*/
#include "cle/rtl_cle.h"
#include "pcm.h"



static int totalmem;
void *rtlglue_malloc(size_t NBYTES) {
	totalmem = totalmem+NBYTES;
	if(NBYTES==0) return NULL;
	return kmalloc(NBYTES,GFP_KERNEL);
}

void rtlglue_free(void *APTR) {
	kfree(APTR);
}

void memDump (void *start, uint32 size, int8 * strHeader){
  int32 row, column, index, index2, max;
  uint8 *buf, ascii[17];
  int8 empty = ' ';

  if(!start ||(size==0))
  	return;
  buf = (uint8 *) start;

  /*
     16 bytes per line
   */
  if (strHeader)
    rtlglue_printf ("%s", strHeader);
  column = size % 16;
  row = (size / 16) + 1;
  for (index = 0; index < row; index++, buf += 16) {
      memset (ascii, 0, 17);
      rtlglue_printf ("\n%08x ", (memaddr) buf);

      max = (index == row - 1) ? column : 16;

      //Hex
      for (index2 = 0; index2 < max; index2++){
	  if (index2 == 8)
	    rtlglue_printf ("  ");
	  rtlglue_printf ("%02x ", (uint8) buf[index2]);
	  ascii[index2] = ((uint8) buf[index2] < 32) ? empty : buf[index2];
	}

      if (max != 16){
	  if (max < 8)
	    rtlglue_printf ("  ");
	  for (index2 = 16 - max; index2 > 0; index2--)
	    rtlglue_printf ("   ");
	}

      //ASCII
      rtlglue_printf ("  %s", ascii);
    }
  rtlglue_printf ("\n");
  return;
}


void startCOP3Counters(int32 countInst){
	/* counter control modes:
		0x10	cycles
		0x11	new inst fetches
		0x12	inst fetch cache misses
		0x13	inst fetch miss busy cycles
		0x14	data store inst
		0x15	data load inst
		0x16	load or store inst
		0x1a	load or store cache misses
		0x1b	load or store miss busy cycles
		*/
	uint32 cntmode;

	if(countInst == TRUE)
		cntmode = 0x13121110;
	else
		cntmode = 0x1b1a1610;
	
	__asm__ __volatile__ (
		/* update status register CU[3] usable */
		"mfc0 $9,$12\n\t"
		"nop\n\t"
		"la $10,0x80000000\n\t"
		"or $9,$10\n\t"
		"mtc0 $9,$12\n\t"
		"nop\n\t"
		"nop\n\t"
		/* stop counters */
		"ctc3 $0,$0\n\t"
		/* clear counters */
		"mtc3 $0,$8\n\t"
		"mtc3 $0,$9\n\t"
		"mtc3 $0,$10\n\t"
		"mtc3 $0,$11\n\t"
		"mtc3 $0,$12\n\t"
		"mtc3 $0,$13\n\t"
		"mtc3 $0,$14\n\t"
		"mtc3 $0,$15\n\t"
		/* set counter controls */
		"ctc3 %0,$0"
		: /* no output */
		: "r" (cntmode)
		);
}


uint32 stopCOP3Counters(void){
	uint32 cntmode;
	uint32 cnt0h, cnt0l, cnt1h, cnt1l, cnt2h, cnt2l, cnt3h, cnt3l;
	__asm__ __volatile__ (
		/* update status register CU[3] usable */
		"mfc0 $9,$12\n\t"
		"nop\n\t"
		"la $10,0x80000000\n\t"
		"or $9,$10\n\t"
		"mtc0 $9,$12\n\t"
		"nop\n\t"
		"nop\n\t"
		/* get counter controls */
		"cfc3 %0,$0\n\t"
		/* stop counters */
		"ctc3 $0,$0\n\t"
		/* save counter contents */
		"mfc3 %1,$9\n\t"
		"mfc3 %2,$8\n\t"
		"mfc3 %3,$11\n\t"
		"mfc3 %4,$10\n\t"
		"mfc3 %5,$13\n\t"
		"mfc3 %6,$12\n\t"
		"mfc3 %7,$15\n\t"
		"mfc3 %8,$14\n\t"
		"nop\n\t"
		"nop\n\t"
		: "=r" (cntmode), "=r" (cnt0h), "=r" (cnt0l), "=r" (cnt1h), "=r" (cnt1l), "=r" (cnt2h), "=r" (cnt2l), "=r" (cnt3h), "=r" (cnt3l)
		: /* no input */
		);
	if(cntmode == 0x13121110){
		rtlglue_printf("COP3 counter for instruction access\n");
		rtlglue_printf("%10d cycles\n", cnt0l);
		rtlglue_printf("%10d new inst fetches\n", cnt1l);
		rtlglue_printf("%10d inst fetch cache misses\n", cnt2l);
		rtlglue_printf("%10d inst fetch miss busy cycles\n", cnt3l);
		}
	else{
		rtlglue_printf("COP3 counter for data access\n");
		rtlglue_printf("%10d cycles\n", cnt0l);
		rtlglue_printf("%10d load or store inst\n", cnt1l);
		rtlglue_printf("%10d load or store cache misses\n", cnt2l);
		rtlglue_printf("%10d load or store miss busy cycles\n", cnt3l);
		}

	return cnt0l;
}


static int32 _startCOP3Counters(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;
	
	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp("inst", nextToken)==0){
		startCOP3Counters(TRUE);
		}
	else if(strcmp("data", nextToken)==0){
		startCOP3Counters(FALSE);
		}
	else if(strcmp("end", nextToken)==0){
		stopCOP3Counters();
		}
	else
		return FAILED;

	return SUCCESS;
}

static int32	_memdump(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;
	uint32 *startaddr, *addr;
	uint32 len, value;
	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp(nextToken, "read") == 0) {
		cle_getNextCmdToken(&nextToken,&size,saved); //base address
		startaddr = (uint32*)U32_value(nextToken);
		cle_getNextCmdToken(&nextToken,&size,saved); //length
		len=U32_value(nextToken);

	}else {
		len=0;
		cle_getNextCmdToken(&nextToken,&size,saved); //base address
		addr=startaddr = (uint32 *)U32_value(nextToken);
		//while(
		cle_getNextCmdToken(&nextToken,&size,saved);// !=FAILED){
			value = (uint32)U32_value(nextToken);
			*addr=value;
			len+=4;
			addr++;
		//}
	}
	memDump(startaddr, len, "Result");
	return SUCCESS;
}


static int32 _pcmTest(uint32 userId,  int32 argc,int8 **saved){
	int8 *nextToken;
	int32 size;
	uint32 chan;
	uint32 value;
	cle_getNextCmdToken(&nextToken,&size,saved);
	chan = U32_value(nextToken);
	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(strcmp(nextToken, "config") == 0) {
		cle_getNextCmdToken(&nextToken,&size,saved);
		if(strcmp(nextToken, "status") == 0) {
			if(pcm_ioctl(chan, PCM_GET_MODE, &value) == SUCCESS){
				rtlglue_printf("PCM channel %d configuration: ", chan);
				rtlglue_printf("%s mode", (value == PCM_MODE_UNIFORM)? "Uniform": 
									(value == PCM_MODE_ULAW)? "u-law": "A-law");
				pcm_ioctl(chan, PCM_GET_LOOPBACK, &value);
				if(value == TRUE)
					rtlglue_printf(", internal loopback");
				pcm_ioctl(chan, PCM_GET_TIMESLOT, &value);
				rtlglue_printf(", time slot %d\n", value);
			}else
				return FAILED;
		}else if(strcmp(nextToken, "uniform") == 0) {
			return pcm_ioctl(chan, PCM_SET_MODE, PCM_MODE_UNIFORM);
		}else if(strcmp(nextToken, "u-law") == 0) {
			return pcm_ioctl(chan, PCM_SET_MODE, PCM_MODE_ULAW);
		}else if(strcmp(nextToken, "A-law") == 0) {
			return pcm_ioctl(chan, PCM_SET_MODE, PCM_MODE_ALAW);
		}else if(strcmp(nextToken, "loopback") == 0) {
			cle_getNextCmdToken(&nextToken,&size,saved);
			if(strcmp(nextToken, "enable") == 0)
				return pcm_ioctl(chan, PCM_SET_LOOPBACK, TRUE);
			else
				return pcm_ioctl(chan, PCM_SET_LOOPBACK, FALSE);
		}else if(strcmp(nextToken, "slot") == 0) {
			cle_getNextCmdToken(&nextToken,&size,saved);
			value = U32_value(nextToken);
			pcm_ioctl(chan, PCM_SET_TIMESLOT, value);
		}
	}else if(strcmp(nextToken, "autotest") == 0) {
		uint32 seed;
		cle_getNextCmdToken(&nextToken,&size,saved);
		value = U32_value(nextToken);
		cle_getNextCmdToken(&nextToken,&size,saved);
		seed = U32_value(nextToken);
		//return pcm_autoTest(chan, value, seed);*/
		pcm_task(value, seed);
	}else if(strcmp(nextToken, "humantest") == 0) {
		pcm_humanTest(chan);
	}else if(strcmp(nextToken, "reset") == 0) {
		init_pcm(chan);
	}
	return SUCCESS;
}


static cle_exec_t _initCmdList[] = {
	{	"memory",
		"read/write memory",
		"{ read'read memory from specified base address' %d'base address' %d'Length' | write'write memory from specified base address' %d'base address' %d'4 byte value' } ",
		_memdump,
		CLE_USECISCOCMDPARSER,	
		0,
		NULL
	},	
	{	"cop3",
		"Coprocessor 3 counters",
		"{ inst | data | end }",
		_startCOP3Counters,
		CLE_USECISCOCMDPARSER,
		0,
		NULL
	},	
	{	"pcm",
		"PCM test",
		" %d'Channel' { reset | config { status | uniform | u-law | a-law | loopback { enable | disable } | slot %d'Time slot' } | autotest %d'Repeat times' %d'Random seed' | humantest }",
		_pcmTest,
		CLE_USECISCOCMDPARSER,
		0,
		NULL
	},	

};


static cle_grp_t _initCmdGrpList[] = {

	/* more ...... */
};


cle_grp_t cle_newCmdRoot[] = {
	{
		"Realtek",								//cmdStr
		"Realtek command line interface",	//cmdDesc
		"Realtek",								//cmdPrompt
		_initCmdList,					//List of commands in this layer
		_initCmdGrpList,						//List of child command groups
		sizeof(_initCmdList)/sizeof(cle_exec_t),		//Number of commands in this layer
		sizeof(_initCmdGrpList)/sizeof(cle_grp_t),	//Number of child command groups
		0,									//access level
	}
	/* more ...... */
};

