/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : 	rtl8651_log.c
* Abstract : 
* Creator : 
* Author :
*/
#include "rtl865x/rtl_types.h"
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include "rtl865x/assert.h"
#include "rtl865x/types.h"
#include "rtl865x/rtl_errno.h"
#include "rtl865x/rtl_glue.h"
#include "rtl865x/rtl8651_tblDrvProto.h"
#include "rtl865x_log.h"
#include "locale/log_curr.h"
extern int rtl8651_user_pid;

#ifdef _RTL_NEW_LOGGING_MODEL

/************************************************
		Global Variables and definition
*************************************************/
#ifdef RTL865X_TEST
#define rtlglue_printf	printf
#endif /* RTL865X_TEST */
#define MAX(a,b) ((a>b)?a:b)
#define MIN(a,b) ((a>b)?b:a)
/* get time */
#define FEBRUARY			2
#define STARTOFTIME			1970
#define SECDAY				86400L
#define SECYR				(SECDAY * 365)
#define leapyear(y)			((!(y % 4) && (y % 100)) || !(y % 400))
#define days_in_year(a)		(leapyear(a) ? 366 : 365)
#define days_in_month(a)		(month_days[(a) - 1])

static int month_days[12]	= {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
/* log reated */
int32		timezone_diff;
static int8		logMsgBuf[RTL8651_MAX_LOG_MSGSIZE];
static char 	*log_buf[RTL8651_MAX_LOG_MODULES] = {0};
static uint32	reg_proc = 0;
#if 0
static uint8	myLog_enable = TRUE;
#endif
/************************************************
		Internal Function declaration
*************************************************/
static int proc_read(char* buffer,char** start, off_t  offset,int count, int* eof, void*  data);
static int8 circ_msg(struct circ_buf *buf,const char *msg);
static void to_tm(uint32 tim, struct rtc_time * tm);
/************************************************
		Utility implementation
*************************************************/
static int proc_read(char* buffer,char** start, off_t  offset,int count, int* eof, void*  data)
{
	char *buf;
	int i, ret;

	rtlglue_drvMutexLock();

	buf = buffer;
	for( i = 0; i < RTL8651_MAX_LOG_MODULES ; i ++)
	{
		buf += sprintf(buf ,"%x\n", (uint32)log_buf[i]);
	}
	ret = (buf - buffer);

	rtlglue_drvMutexUnlock();

	return ret;
}

static void to_tm(uint32 tim, struct rtc_time * tm)
{
	uint32 hms, day, gday;
	uint32 i;

	gday = day = tim / SECDAY;
	hms = tim % SECDAY;

	/* Hours, minutes, seconds are easy */
	tm->tm_hour = hms / 3600;
	tm->tm_min = (hms % 3600) / 60;
	tm->tm_sec = (hms % 3600) % 60;

	/* Number of years in days */
	for (i = STARTOFTIME; day >= days_in_year(i); i++)
	day -= days_in_year(i);
	tm->tm_year = i;

	/* Number of months in days left */
	if (leapyear(tm->tm_year))
	days_in_month(FEBRUARY) = 29;
	for (i = 1; day >= days_in_month(i); i++)
	day -= days_in_month(i);
	days_in_month(FEBRUARY) = 28;
	tm->tm_mon = i-1;	/* tm_mon starts from 0 to 11 */

	/* Days are what is left over (+1) from all that. */
	tm->tm_mday = day + 1;

	/*
	 * Determine the day of week
	 */
	tm->tm_wday = (gday + 4) % 7; /* 1970/1/1 was Thursday */
}

static int8 circ_msg(struct circ_buf *buf,const char *msg)
{
	int i, l, realBufNum;	/* the real number of buffer */
	int8 isChanged = FALSE;

	rtlglue_drvMutexLock();

	/* The real buffer number is one more than "buf->size", however, we don't use the last one byte. */
	realBufNum = buf->size + 1;

	/* The length of new string to be added into circular buffer. */
	l = strlen(msg) + 1;
	
	if(l > buf->size)	/*this message is too large, do not care about it*/
		goto out;
	
	if ((buf->tail + l) <= buf->size) {	/* "buf->tail" is used to store number of buffers used, which is counted from position 0. */
		if (buf->tail < buf->head) {
			if ((buf->tail + l - 1) >= buf->head) {
				int nextHeadSeekPos = buf->tail + l;	/* the start position to seek the next head */
				char *c =
					memchr(buf->data + nextHeadSeekPos, '\0',
						   buf->size - nextHeadSeekPos);
				if (c != NULL) {					
					buf->head = c - buf->data + 1;	

					/* Clear the fragmented message */
					for (i = (buf->tail + l); i <= buf->head; i++)
						buf->data[i] = '\0';
				} else {
					buf->head = 0;

					/* Clear the fragmented message */
					for (i = (buf->tail + l); i < buf->size; i++)
						buf->data[i] = '\0';
				}
			}
		}		
		strncpy(buf->data + buf->tail, msg, l);
		buf->tail += l;	
		isChanged = TRUE;
	} else {
		/* In this case, we need to partition the message string into 2 parts. Second part will be put in the beginning of circular buffer. */
		char *c;
		int newTailPos;

		if (buf->tail != 0)		/* If this message to be added is larger than length of circ buf, we don't add this message to circ buf. */
		{
			if (buf->tail  == buf->size)
				newTailPos = l;
			else
				newTailPos = buf->tail + l - (buf->size + 1);

			c = memchr(buf->data + newTailPos, '\0', buf->tail - newTailPos);

			if (c != NULL) {
				int firstPart_Num;	/* the number of first part string */
				if (buf->tail == buf->size)
					firstPart_Num = 0;
				else
					firstPart_Num = buf->size -(buf->tail - 1);
				buf->head = c - buf->data + 1;

				/* Clear the fragmented message */
				for (i = newTailPos; i < buf->head; i++)
					buf->data[i] = '\0';
				
				strncpy(buf->data + buf->tail, msg, firstPart_Num);
				buf->data[buf->size] = '\0';
				strncpy(buf->data, &msg[firstPart_Num], l - firstPart_Num);
				buf->tail = newTailPos;
			} else {
				/* Clear all the data in the circ buf */
				for (i = 0; i < buf->size; i++)
					buf->data[i] = '\0';
				buf->head = 0;
				buf->tail = 0;
			}
			
			buf->round++;
			isChanged = TRUE;
		}
	}

out:	
	rtlglue_drvMutexUnlock();
	return isChanged;
}

/*
	This function is used in kernel mode, get the start address of circular buffer which related to the moduleId.
	Note that for unknown module Id or NULL circular buffer, this function would return NULL.
*/
void* rtl8651_log_getCircBufPtr(uint32 moduleId, uint32* logSize)
{
	void *ptr = NULL;
	uint32 moduleId_tmp;
	uint32 idx;
	uint32 buf_size = RTL8651_MAX_LOG_BUFFERSIZE;
	uint32 total_size = buf_size + sizeof(struct circ_buf);	/* our buffer size */

	rtlglue_drvMutexLock();

	idx = 0;
	moduleId_tmp = moduleId;
	while (moduleId_tmp > 1)
	{
		idx ++;
		moduleId_tmp = moduleId_tmp >> 1;
	}

	if ( idx >= RTL8651_MAX_LOG_MODULES )
	{
		goto out;
	}

	ptr = log_buf[idx];

	if ( ptr && logSize )
	{
		/* initial value */
		*logSize = total_size;
	}

out:
	rtlglue_drvMutexUnlock();
	
	return ptr;
}

/*
	This function is used in kernel mode, clear the circular buffer which related to the moduleId.
*/
int32 rtl8651_log_clearCircBuf(uint32 moduleId)
{
	struct circ_buf *ptr = NULL;
	uint32 moduleId_tmp;
	uint32 idx;
	int32 ret = FAILED;

	rtlglue_drvMutexLock();

	idx = 0;
	moduleId_tmp = moduleId;
	while (moduleId_tmp > 1)
	{
		idx ++;
		moduleId_tmp = moduleId_tmp >> 1;
	}

	if ( idx >= RTL8651_MAX_LOG_MODULES )
	{
		goto out;
	}

	ptr = (struct circ_buf *)log_buf[idx];

	if ( ptr )
	{
		/* clear circular buffer */
		ptr->head = 0;
		ptr->tail = 0;
	}

	ret = SUCCESS;

out:
	rtlglue_drvMutexUnlock();
	return ret;

}

/************************************************
		External Function implementation
*************************************************/
#if 0
/*
	Initial Function
*/
int32 myLoggingInit(void)
{
	/***** Init all variables *****/
	myLog_enable = FALSE;
	memset(logMsgBuf, 0, sizeof(logMsgBuf));
	memset(log_buf, 0, sizeof(log_buf));
	reg_proc = 0;
	myLog_enable = TRUE;
	return SUCCESS;
}
#endif
/*
	Log function for rome driver
*/
int32 myLoggingFunction(uint32 moduleId, uint32 logNo, rtl8651_logInfo_t *info)
{
	struct circ_buf *cbuf;
	struct timeval tv;
	struct rtc_time tm;
	uint32 moduleId_tmp;
	uint32 idx;

	#if 0
	if (myLog_enable != TRUE)
	{
		return FAILED;
	}
	#endif

	assert(logNo <= RTL8651_TOTAL_USERLOG_NO);

	/*************************************************************************************************
							Find the buffer address to record Log	
	*************************************************************************************************/
	/* Calculate Index of log_buf */
	idx = 0;
	moduleId_tmp = moduleId;
	while (moduleId_tmp > 1)
	{
		idx ++;
		moduleId_tmp = moduleId_tmp >> 1;
	}
	assert(idx < RTL8651_MAX_LOG_MODULES);
	/* allocate circular buffer for log */
	if(log_buf[idx] == NULL)
	{
		uint32 buf_size = RTL8651_MAX_LOG_BUFFERSIZE;
		uint32 total_size = buf_size + sizeof(struct circ_buf);	/* our buffer size */
		/* register proc entry */
		if(reg_proc == 0)
		{
			struct proc_dir_entry* pProcDirEntry = NULL;	
			pProcDirEntry = create_proc_entry(RTL8651_LOGPROC_FILE, 0444, NULL);
			if ( pProcDirEntry )
			{
				pProcDirEntry->uid = 0;
				pProcDirEntry->gid = 0;
				pProcDirEntry->read_proc = proc_read;
				pProcDirEntry->write_proc = NULL;
				reg_proc=1;
			} else
			{
				rtlglue_printf("%s(%d) Failed to create proc entry!\n", __FUNCTION__, __LINE__);
				return FAILED;
			}
		}			
		/* Allocate buffer for this log module */
		if ((log_buf[idx] = kmalloc(total_size,GFP_ATOMIC)) == NULL)
		{
			rtlglue_printf("%s(%d) Out of memory!\n", __FUNCTION__, __LINE__);
			return FAILED;
		}
		/* init log buffer */
		cbuf = (struct circ_buf *)log_buf[idx];
		cbuf->size = buf_size;
		cbuf->head = cbuf->tail = 0;		
		cbuf->round = 0;
		
	}else
	{
		cbuf = (struct circ_buf *)log_buf[idx];
	}

	/*************************************************************************************************
		Start to generate the logging message:

			All messages include 3 sections:

				[Time]
					- time of this event
				[Logging descriptor]
					- descriptor of each Log Message
					- language resource defined in locale/log_curr.h, each logNo has one corresponding descriptor
				[log detail information]
					- detail information of each log
					- stored in "info"
					- if (info == NULL) it means there is no any detail information for this event
					- language resource format defined in locale/log_curr.h, each different information type has one corresponding
						format

				- customers can modify log message layout in locale/log_curr.h
	*************************************************************************************************/
	{
		uint16 dsid;
		char msg_time[48];
		char msg_desc[80];
		char msg_detail[128];

		dsid = 0xffff;	/* no dsid assigned */
		memset(msg_time, 0, sizeof(msg_time));
		memset(msg_desc, 0, sizeof(msg_desc));
		memset(msg_detail, 0, sizeof(msg_detail));
		memset(logMsgBuf, 0, sizeof(logMsgBuf));
		/**************************** Get event time *****************************/
		do_gettimeofday(&tv);
		tv.tv_sec+=timezone_diff;
		to_tm(tv.tv_sec, &tm);
		snprintf(msg_time, sizeof(msg_time), RTL8651_LOG_TIME_STRING, RTL8651_LOG_TIME_VARLIST);
		/**************************** Get message descriptor *****************************/
		snprintf(msg_desc, sizeof(msg_desc), "%s", log_desc[logNo - 1]);
		/**************************** Get detail information *****************************/
		if (info)
		{
			uint8 *sip, *dip;
			switch (info->infoType)
			{
				case RTL8651_LOG_INFO_PKT:
					dsid = info->pkt_dsid;
					sip = (uint8 *)(&(info->pkt_sip));
					dip = (uint8 *)(&(info->pkt_dip));
					switch (info->pkt_proto)
					{
						case IPPROTO_TCP:
							snprintf(msg_detail, sizeof(msg_detail), RTL8651_LOG_INFO_PKT_TCPSTRING, RTL8651_LOG_INFO_PKT_TCPVARLIST);
							break;
						case IPPROTO_UDP:
							snprintf(msg_detail, sizeof(msg_detail), RTL8651_LOG_INFO_PKT_UDPSTRING, RTL8651_LOG_INFO_PKT_UDPVARLIST);
							break;
						case IPPROTO_ICMP:
							snprintf(msg_detail, sizeof(msg_detail), RTL8651_LOG_INFO_PKT_ICMPSTRING, RTL8651_LOG_INFO_PKT_ICMPVARLIST);
							break;
						default:
							snprintf(msg_detail, sizeof(msg_detail), RTL8651_LOG_INFO_PKT_OTHERSTRING, RTL8651_LOG_INFO_PKT_OTHERVARLIST);
							break;
					}
					break;
				case RTL8651_LOG_INFO_URL:
					dsid = info->url_dsid;
					sip = (uint8 *)(&(info->url_sip));
					dip = (uint8 *)(&(info->url_dip));
					switch (info->url_proto)
					{
						case IPPROTO_TCP:
							snprintf(msg_detail, sizeof(msg_detail), RTL8651_LOG_INFO_URL_TCPSTRING, RTL8651_LOG_INFO_URL_TCPVARLIST);
							break;
						case IPPROTO_UDP:
							snprintf(msg_detail, sizeof(msg_detail), RTL8651_LOG_INFO_URL_UDPSTRING, RTL8651_LOG_INFO_URL_UDPVARLIST);
							break;
						default:
							snprintf(msg_detail, sizeof(msg_detail), RTL8651_LOG_INFO_URL_OTHERSTRING, RTL8651_LOG_INFO_URL_OTHERVARLIST);
							break;
					}
					break;
				default:
					assert(0);
					return FAILED;
			}
		}
		/************** Generate Log message **************/
		{
			char usrDefinedMsgfont[] = RTL8651_LOG_STRING_ITEM_STRING;
			char logMsgLayout[24];

			/* generate log message layout */
			if (dsid == 0xffff)
			{	/* non dsid related message: let "x" be this message's prefix */
				snprintf(logMsgLayout, sizeof(logMsgLayout), "x%s\n", usrDefinedMsgfont);
			}else
			{	/* dsid related message: let dsid be this message's prefix */
				snprintf(logMsgLayout, sizeof(logMsgLayout), "%1d%s\n", dsid, usrDefinedMsgfont);
			}
			/* generate message */
			snprintf(logMsgBuf, sizeof(logMsgBuf), logMsgLayout, RTL8651_LOG_STRING_ITEM_VARLIST);
		}
	}
	/** put this message into circular buffer **/
	if(circ_msg(cbuf, logMsgBuf) == TRUE){
		sys_kill(rtl8651_user_pid,60+idx); /* send a signal to userspace for mail alert*/
	}
	return SUCCESS;
} /* end myLoggingFunction */

#endif /* _RTL_NEW_LOGGING_MODEL */
