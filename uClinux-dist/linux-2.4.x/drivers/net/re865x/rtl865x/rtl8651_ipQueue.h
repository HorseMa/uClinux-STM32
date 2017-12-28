#ifndef RTL8651_IPQUEUE_H
#define RTL8651_IPQUEUE_H

#include "rtl8651_tblDrvProto.h"

/******************************************************
		External Functions
******************************************************/
int32 _rtl8651_ip_defrag(struct rtl_pktHdr *, struct ip *);

/******************************************************
		Queue System management
******************************************************/
typedef struct _rtl8651_IpFragQueuePara_s{
	uint32 MaxFragPktCnt;					/* Maximum Fragment Packet Count in system */
	uint32 MaxFragSubPktCnt;					/* Maximum Fragment Queue Length of each packet in system (0: no limit) */
	uint32 MaxNegativeListEntryCnt;			/* Maximum Negative List Entry Count */
	uint32 MaxFragPoolCnt;					/* Maximum Fragment Pool Count (total Packet can be queued in fragment packet) (0: no limit) */
	uint32 MaxFragTimeOut;					/* Maximum Fragment Packet Timeout */
	uint32 MaxNegativeListTimeOut;			/* Maximum Negative List Entry Timeout */
} _rtl8651_IpFragQueuePara_t;

typedef struct _rtl8651_IpFragQueueTimeout_s {
	uint32 MaxFragTimeout;					/* Maximum Fragment Packet Timeout */
	uint32 MaxNegativeListTimeOut;			/* Maximum Negative List Entry Timeout */
} _rtl8651_IpFragQueueTimeout_t;

int32 rtl8651_IpFragQueueReinit(void);
void rtl8651_IpFragQueueSetTimeout(_rtl8651_IpFragQueueTimeout_t *para);
#endif /* RTL8651_IPQUEUE_H */
