#ifndef _ALG_QOS_H
#define _ALG_QOS_H

#include "rtl_types.h"

/* definition of macros */
#ifndef RTL8651_NETINTF_NAME_LEN
#define RTL8651_NETINTF_NAME_LEN	16		/* length of network interface name */
#endif
#ifndef RTL8651_ALG_QOS
#define RTL8651_ALG_QOS
#endif

/************************** definition of ALG QoS types ***************************/
/* format: MSB --- 0 | dstPort | dstIP | extPort | extIP | srcPort | srcIP | isTCP --- LSB */
#define RTL8651_ALG_QOS_TYPE_PASV_FTP_CLIENT 0x63
#define RTL8651_ALG_QOS_TYPE_PASV_FTP_SERVER 0x27
#define RTL8651_ALG_QOS_TYPE_ACTIVE_FTP 0x67

/* live time of different ALG QoS flows in second */
#define RTL8651_ALG_QOS_PASV_FTP_LIVETIME 3
#define RTL8651_ALG_QOS_ACTIVE_FTP_LIVETIME 3

/* maximal flow number of ALG QoS table */
#define RTL8651_ALG_QOS_DEFAULT_TBL_SIZE 32

/* flag indicating whether the ALG QoS table is initialized */
extern uint8 rtl8651_algQos_isInit;


/************************** definition of ALG QoS flow ***************************/
typedef struct rtl8651_alg_qos_s {
	/* 7-tuple to discriminate a NAPT connection */
	uint32 srcIP, extIP, dstIP;			/* source internal IP, source external IP, destination IP */
	uint16 srcPort, extPort, dstPort;	/* source internal port, source externl port, destination port */
	uint8 isTCP:1;					/* UDP or TCP */

	/* additional information for adding ACL */
	uint8 isHigh:1;		/* high queue or low queue */
	uint8 isOneShot:1;	/* the entry with oneshot flag set will be deleted after fist match */
	uint8 algType;		/* indicate different type of comparisons for different ALGs */
	int8 ifName[RTL8651_NETINTF_NAME_LEN];	/* name of network interface */
	uint32 liveTime;		/* time to live (uint: second) */
	uint32 qosRatio;		/* QoS ratio (percent) */
	uint32 groupId;		/* QoS group ID */
} rtl8651_alg_qos_t;


/************************** declarartion of APIs ***************************/

/* initialize ALG QoS table */
int32 _rtl8651_algQos_init( uint32 tblSize );

/* destroy ALG QoS table */
int32 _rtl8651_algQos_destroy( void );

/* add an entry to ALG QoS table, return FAILED if no free space exists */
/* Note: if the entry already exists, then the input will update the old entry */
int32 _rtl8651_algQos_addEntry( rtl8651_alg_qos_t *entry );

/* 
 * Search ALG QoS table for a match with specified 7-tuple. 
 * If matched, copy QoS relevant contents to <alg_qos>, and return SUCCESS, otherwise return FAILED
 */
int32 _rtl8651_algQos_match( uint8 isTCP, uint32 srcIP, uint16 srcPort, uint32 extIP, uint16 extPort, 
								 uint32 dstIP, uint16 dstPort, rtl8651_alg_qos_t *entry );

/* 
 * Search ALG QoS table for a match with specified 7-tuple. 
 * If matched, copy QoS relevant contents to <alg_qos>, and return SUCCESS, otherwise return FAILED.
 * In addition, if the entry which is tagged as "oneShot" will be deleted simultaneously.
 */
int32 _rtl8651_algQos_matchAndDelete( uint8 isTCP, uint32 srcIP, uint16 srcPort, uint32 extIP, uint16 extPort, 
								 uint32 dstIP, uint16 dstPort, rtl8651_alg_qos_t *algQos );

/*
 * Subtract <secPassed> from "liveTime" of each entry in ALG QoS table;
 * and delete obsolete entries (whose "liveTime" is zero) from the table
 */
int32 _rtl8651_algQos_timeUpdate( uint32 secPassed );

/*
 * flush ALG QoS table
 */
int32 _rtl8651_algQos_flush( void );

#endif


