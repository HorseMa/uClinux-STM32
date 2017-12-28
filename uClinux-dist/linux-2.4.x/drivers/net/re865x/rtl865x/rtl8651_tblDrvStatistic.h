/*
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* Program : Switch core table driver statistic
* Abstract : 
* Author : chih-hua huang (chhuang@realtek.com.tw)               
* $Id: rtl8651_tblDrvStatistic.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
*/

#ifndef RTL8651_TBLDRV_STATISTIC_H
#define RTL8651_TBLDRV_STATISTIC_H

typedef struct rtl8651_tblDrvNaptCounter_s {
	uint32 protoAddCount,	/* The totalnumber of entries added by protocol stack */
		   protoDelCount,	/* The totalnumber of entries deleted by protocol */
		   drvAddCount, 	/* The total number of entries added by table driver */
		   drvAgeoutCount,
		   addAttempts, addSucceeded,
		   threadWriteBackCount, /* How many entries the maintenance thread write back to ASIC */
		   naptUnitAddCnt,		/* added napt count per second */
		   maxNaptUnitAddCnt,	/* the max value of naptUnitAddCnt */
		   dosCountedTcpCnt, dosCountedUdpCnt,	/* The totalnumber of tcp/udp entries counted in DoS system */
		   dosCountedFromInternalTcpCnt, dosCountedFromInternalUdpCnt;	/* The totalnumber of tcp/udp entries initiated by internal host and counted in DoS system */
} rtl8651_tblDrvNaptCounter_t;


typedef struct rtl8651_tblDrvIcmpCounter_s {
	uint32 protoAddCount,	/* The totalnumber of entries added by protocol stack */
		   protoDelCount,	/* The totalnumber of entries deleted by protocol */
		   drvAddCount, 	/* The total number of entries added by table driver */
		   learntFromAsic,	/* The total number of entries added from learning ASIC entry */		
		   drvDelCount,		/* The total number of entries deleted by table driver */
		   diffHashCount,	/* The total difference hash algo. count */
		   threadWriteBackCount, /* How many entries the maintenance thread write back to ASIC */
		   syncFailure,		/* How many entries sync fail */
		   addFailure;		/* add fail */

	uint16 curActiveCount,		/* Current active entries */
		   curInactiveCount,	/* Current in-active entries */
		   curInDriverCount,	/* Current entries in the driver table */
		   curInAsicCount,		/* Current entries in the ASIC table */
		   curDiffHashCount,	/* Current differ hash algo. counter */
		   curFromAsicCount; 	/* Current entries read from ASIC */
	
} rtl8651_tblDrvIcmpCounter_t;

extern rtl8651_tblDrvNaptCounter_t rtl8651_tblDrvNaptCounter;
extern rtl8651_tblDrvIcmpCounter_t  rtl8651_tblDrvIcmpCounter;


int32 _rtl8651_dumpAsicNaptTableEntry(uint32 index, uint32 findport);
int32 _rtl8651_dumpNaptTableEntry(uint32 index, uint32 findport);
int32 _rtl8651_dumpAlgCtrlConnections(uint32 index);
int32 _rtl8651_dumpNaptIcmpTable(void);
int32 rtl8651_getTblDrvNaptIcmpCounter(rtl8651_tblDrvIcmpCounter_t *counter);
int32 _rtl8651_tblDrvStatisticReset(void);
void rtl8651_getTblDrvNaptInfo( int32 TopN, int32 listN );

#endif 





