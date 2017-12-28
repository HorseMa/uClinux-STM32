

#ifndef RTL865XB_TBLASICDRV_H
#define RTL865XB_TBLASICDRV_H

#include "types.h"
#include "rtl8651_layer2.h"
#include "rtl8651_tblDrv.h"

/* chip version information */
extern int8 RtkHomeGatewayChipName[16];
extern int32 RtkHomeGatewayChipRevisionID;

#if defined(RTL865X_TEST) || defined(RTL865X_MODEL_USER) || defined(RTL865X_MODEL_KERNEL)
void rtl8651_setChipVersion(int8 *name, int32 *rev);
int32 rtl8651_getChipVersion(int8 *name,uint32 size, int32 *rev);
#endif /* RTL865X_TEST */


// ASIC specification part
#define RTL8651_PPPOE_NUMBER				8
#define RTL8651_ROUTINGTBL_SIZE			8
#define RTL8651_ARPTBL_SIZE				512
#define RTL8651_PPPOETBL_SIZE				8
#define RTL8651_TCPUDPTBL_SIZE			1024
#define RTL8651_TCPUDPTBL_BITS				10
#define RTL8651_ICMPTBL_SIZE				32
#define RTL8651_ICMPTBL_BITS				5
//#ifdef CONFIG_RTL865XB
#define RTL8651_IPTABLE_SIZE				16
#define RTL8651_SERVERPORTTBL_SIZE			16
#define RTL8651_NXTHOPTBL_SIZE			32
#define RTL8651_RATELIMITTBL_SIZE		32
//#else
//#define RTL8651_IPTABLE_SIZE				8
//#define RTL8651_SERVERPORTTBL_SIZE			8
//#endif /* CONFIG_RTL865XB */
#define RTL8651_ALGTBL_SIZE				128
#define RTL8651_MULTICASTTBL_SIZE			64
#define RTL8651_IPMULTICASTTBL_SIZE		64
#define RTL8651_NEXTHOPTBL_SIZE			32
#define RTL8651_RATELIMITTBL_SIZE			32
#define RTL8651_MACTBL_SIZE			1024
#define RTL8651_PROTOTRAPTBL_SIZE		8
#define RTL8651_VLANTBL_SIZE			8
#define RTL8651_ACLTBL_SIZE			125


/* Memory mapping of tables 
*/
#if defined(RTL865X_TEST) || defined(RTL865X_MODEL_USER) || defined(RTL865X_MODEL_KERNEL)
#define RTL8651_ASICTABLE_BASE_OF_ALL_TABLES pVirtualSWTable
#else
#define RTL8651_ASICTABLE_BASE_OF_ALL_TABLES REAL_SWTBL_BASE
#endif /* RTL865X_TEST */

#define RTL8651_ASICTABLE_ENTRY_LENGTH (8 * sizeof(uint32))
enum {
    TYPE_L2_SWITCH_TABLE = 0,
    TYPE_ARP_TABLE,
    TYPE_L3_ROUTING_TABLE,
    TYPE_MULTICAST_TABLE,
    TYPE_PROTOCOL_TRAP_TABLE,
    TYPE_VLAN_TABLE,
    TYPE_EXT_INT_IP_TABLE,
    TYPE_ALG_TABLE,
    TYPE_SERVER_PORT_TABLE,
    TYPE_L4_TCP_UDP_TABLE,
    TYPE_L4_ICMP_TABLE,
    TYPE_PPPOE_TABLE,
    TYPE_ACL_RULE_TABLE,
    TYPE_NEXT_HOP_TABLE,
    TYPE_RATE_LIMIT_TABLE,
};


/*#define rtl8651_asicTableAccessAddrBase(type) (RTL8651_ASICTABLE_BASE_OF_ALL_TABLES + 0x10000 * (type)) */
#define rtl8651_asicTableAccessAddrBase(type) (RTL8651_ASICTABLE_BASE_OF_ALL_TABLES + ((type)<<16) )


typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    uint16          mac31_16;
    uint16          mac15_0;
    /* word 1 */
    uint16          vhid        : 9;
    uint16          memberPort  : 6;
    uint16          valid       : 1;
    uint16          mac47_32;
    /* word 2 */
    uint8           reserv5     : 1;
    uint8           outACLEnd   : 7;
    uint8           reserv4     : 1;
    uint8           outACLStart : 7;
    uint8           reserv3     : 1;
    uint8           inACLEnd    : 7;
    uint8           reserv2     : 1;
    uint8           inACLStart  : 7;
    /* word 3 */
    uint32          mtuL        : 8;
    uint32          macMask     : 2;
    uint32          egressUntag : 6;
    uint32          promiscuous : 1;
    uint32          bcastToCPU  : 1;
    uint32          STPStatus   : 12;
    uint32          enHWRoute   : 1;
    uint32          isInternal  : 1;
    /* word 4 */
    uint32          reserv7         : 15;
    uint32          macNotExist     : 1;
    uint32          isDMZ           : 1;
    uint32          extSTPStatus    : 6;
    uint32          extEgressUntag  : 3;
    uint32          extMemberPort   : 3;
    uint32          mtuH        : 3;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    uint16          mac15_0;
    uint16          mac31_16;
    /* word 1 */
    uint16          mac47_32;
    uint16          valid       : 1;
    uint16          memberPort  : 6;
    uint16          vhid        : 9;
    /* word 2 */
    uint8           inACLStart  : 7;
    uint8           reserv2     : 1;
    uint8           inACLEnd    : 7;
    uint8           reserv3     : 1;
    uint8           outACLStart : 7;
    uint8           reserv4     : 1;
    uint8           outACLEnd   : 7;
    uint8           reserv5     : 1;
    /* word 3 */
    uint32          isInternal  : 1;
    uint32          enHWRoute   : 1;
    uint32          STPStatus   : 12;
    uint32          bcastToCPU  : 1;
    uint32          promiscuous : 1;
    uint32          egressUntag : 6;
    uint32          macMask     : 2;
    uint32          mtuL        : 8;
    /* word 4 */
    uint32          mtuH        : 3;
    uint32          extMemberPort   : 3;
    uint32          extEgressUntag  : 3;
    uint32          extSTPStatus    : 6;
    uint32          isDMZ           : 1;
    uint32          macNotExist     : 1;
    uint32          reserv7         : 15;
#endif /*_LITTLE_ENDIAN*/
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_vlanTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    uint16          reserv5     : 12;
    uint16          valid       : 1;
    uint16          trapProtocol: 3;
    uint16          trapContent;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    uint16          trapContent;
    uint16          trapProtocol: 3;
    uint16          valid       : 1;
    uint16          reserv5     : 12;
#endif /*_LITTLE_ENDIAN*/
    /* word 1 */
    uint32          reservw1;
    /* word 2 */
    uint32          reservw2;
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_protoTrapTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    ipaddr_t        internalIP;
    /* word 1 */
    ipaddr_t        externalIP;
    /* word 2 */
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 29;
#else
    uint32          reserv0     : 24;
    uint32          nextHop     : 5;
#endif /*CONFIG_RTL8650BBASIC*/
    uint32          isLocalPublic   : 1;
    uint32          isOne2One       : 1;
    uint32          valid       : 1;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    ipaddr_t        internalIP;
    /* word 1 */
    ipaddr_t        externalIP;
    /* word 2 */
    uint32          valid       : 1;
    uint32          isOne2One       : 1;
    uint32          isLocalPublic   : 1;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 29;
#else
    uint32          nextHop     : 5;
    uint32          reserv0     : 24;
#endif /*CONFIG_RTL8650BBASIC*/
#endif /*_LITTLE_ENDIAN*/
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_extIpTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    ipaddr_t        internalIP;
    /* word 1 */
    ipaddr_t        externalIP;
    /* word 2 */
    uint16          externalPort;
    uint16          internalPort;
    /* word 3 */
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 31;
#else
    uint32          reserv0     : 25;
    uint32          isPortRange : 1;
    uint32          nextHop     : 5;
#endif /*CONFIG_RTL8650BBASIC*/
    uint32          valid       : 1;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    ipaddr_t        internalIP;
    /* word 1 */
    ipaddr_t        externalIP;
    /* word 2 */
    uint16          internalPort;
    uint16          externalPort;
    /* word 3 */
    uint32          valid       : 1;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 31;
#else
    uint32          nextHop     : 5;
    uint32          isPortRange : 1;
    uint32          reserv0     : 25;
#endif /*CONFIG_RTL8650BBASIC*/
#endif /*_LITTLE_ENDIAN*/
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_srvPortTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    uint16          reserv      : 15;
    uint16          valid       : 1;
    uint16          L4Port;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    uint16          L4Port;
    uint16          valid       : 1;
    uint16          reserv      : 15;
#endif /*_LITTLE_ENDIAN*/
    /* word 1 */
    uint32          reservw1;
    /* word 2 */
    uint32          reservw2;
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_algTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    ipaddr_t        IPAddr;
    /* word 1 */
    union {
        struct {
            uint32          reserv0     : 5;
            uint32 			ARPIpIdx	: 2;
            uint32          ARPEnd      : 6;
            uint32          ARPStart    : 6;
            uint32          IPMask      : 5;
            uint32          vid         : 3;
            uint32          hPriority   : 1;
            uint32          process     : 3;
            uint32          valid       : 1;
        } ARPEntry;
        struct {
            uint32          reserv0     : 9;
            uint32          nextHop     : 10;
            uint32          IPMask      : 5;
            uint32          vid         : 3;
            uint32          hPriority   : 1;
            uint32          process     : 3;
            uint32          valid       : 1;
        } L2Entry;
        struct {
            uint32          reserv0     : 6;
            uint32          PPPoEIndex  : 3;
            uint32          nextHop     : 10;
            uint32          IPMask      : 5;
            uint32          vid         : 3;
            uint32          hPriority   : 1;
            uint32          process     : 3;
            uint32          valid       : 1;        
        } PPPoEEntry;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
        struct {
            uint32          reserv0     : 5;
            uint32          IPDomain    : 3;
            uint32          nhAlgo      : 2;
            uint32          nhNxt       : 5;
            uint32          nhStart     : 4;
            uint32          IPMask      : 5;
            uint32          nhNum       : 3;
            uint32          hPriority   : 1;
            uint32          process     : 3;
            uint32          valid       : 1;
        } NxtHopEntry;
#endif /*CONFIG_RTL8650BBASIC*/
    } linkTo;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    ipaddr_t        IPAddr;
    /* word 1 */
    union {
        struct {
            uint32          valid       : 1;
            uint32          process     : 3;
            uint32          hPriority   : 1;
            uint32          vid         : 3;
            uint32          IPMask      : 5;
            uint32          ARPStart    : 6;
            uint32          ARPEnd      : 6;
            uint32			ARPIpIdx	: 2;
            uint32          reserv0     : 5;
        } ARPEntry;
        struct {
            uint32          valid       : 1;
            uint32          process     : 3;
            uint32          hPriority   : 1;
            uint32          vid         : 3;
            uint32          IPMask      : 5;
            uint32          nextHop     : 10;
            uint32          reserv0     : 9;
        } L2Entry;
        struct {
            uint32          valid       : 1;
            uint32          process     : 3;
            uint32          hPriority   : 1;
            uint32          vid         : 3;
            uint32          IPMask      : 5;
            uint32          nextHop     : 10;
            uint32          PPPoEIndex  : 3;
            uint32          reserv0     : 6;
        } PPPoEEntry;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
        struct {
            uint32          valid       : 1;
            uint32          process     : 3;
            uint32          hPriority   : 1;
            uint32          nhNum       : 3;
            uint32          IPMask      : 5;
            uint32          nhStart     : 4;
            uint32          nhNxt       : 5;
            uint32          nhAlgo      : 2;
            uint32          IPDomain    : 3;
            uint32          reserv0     : 5;
        } NxtHopEntry;
#endif /*CONFIG_RTL8650BBASIC*/
    } linkTo;
#endif /*_LITTLE_ENDIAN*/
    /* word 2 */
    uint32          reservw2;
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_l3RouteTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint16          reserv0;
#else
    uint16          reserv0     : 13;
    uint16          ageTime     : 3;
#endif /*CONFIG_RTL8650BBASIC*/
    uint16          sessionID;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    uint16          sessionID;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint16          reserv0;
#else
    uint16          ageTime     : 3;
    uint16          reserv0     : 13;
#endif /*CONFIG_RTL8650BBASIC*/
#endif /*_LITTLE_ENDIAN*/
    /* word 1 */
    uint32          reservw1;
    /* word 2 */
    uint32          reservw2;
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_pppoeTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    uint16          mac39_24;
    uint16          mac23_8;
    /* word 1 */
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint16          reserv0     : 11;
#else
    uint16          reserv0     : 8;
    uint16          extMemberPort   : 3;
#endif /*CONFIG_RTL8650BBASIC*/
    uint16          nxtHostFlag : 1;
    uint16          srcBlock    : 1;
    uint16          agingTime   : 2;
    uint16          isStatic    : 1;
    uint16          toCPU       : 1;
    uint16          hPriority   : 1;
    uint16          memberPort  : 6;
    uint16          mac47_40    : 8;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    uint16          mac23_8;
    uint16          mac39_24;
    /* word 1 */
    uint16          mac47_40    : 8;
    uint16          memberPort  : 6;
    uint16          hPriority   : 1;
    uint16          toCPU       : 1;
    uint16          isStatic    : 1;
    uint16          agingTime   : 2;
    uint16          srcBlock    : 1;
    uint16          nxtHostFlag : 1;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint16          reserv0     : 11;
#else
    uint16          extMemberPort   : 3;
    uint16          reserv0     : 8;
#endif /*CONFIG_RTL8650BBASIC*/
#endif /*_LITTLE_ENDIAN*/
    /* word 2 */
    uint32          reservw2;
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_l2Table_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    uint32          reserv0     : 21;
    uint32          nextHop     : 10;
    uint32          valid       : 1;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    uint32          valid       : 1;
    uint32          nextHop     : 10;
    uint32          reserv0     : 21;
#endif /*_LITTLE_ENDIAN*/
    /* word 1 */
    uint32          reservw1;
    /* word 2 */
    uint32          reservw2;
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_arpTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    ipaddr_t        intIPAddr;
    /* word 1 */
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 15;
    uint32          isStatic    : 1;
    uint32          reserv1     : 2;
//    uint32          collision2  : 1;//Edward remove with trouble
    uint32          offset      : 6;
    uint32          agingTime   : 6;
    uint32          collision   : 1;
    uint32          valid       : 1;
#else
    uint32          reserv0     : 1;
    uint32          selEIdx     : 10;
    uint32          selIPIdx    : 4;
    uint32          isStatic    : 1;
    uint32          dedicate    : 1;
    uint32          collision2  : 1;
    uint32          offset      : 6;
    uint32          agingTime   : 6;
    uint32          collision   : 1;
    uint32          valid       : 1;
#endif
    /* word 2 */
    uint32          reserv2     : 12;
    uint32          isTCP       : 1;
    uint32          TCPFlag     : 3;
    uint32          intPort     : 16;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    ipaddr_t        intIPAddr;
    /* word 1 */
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          valid       : 1;
    uint32          collision   : 1;
    uint32          agingTime   : 6;
    uint32          offset      : 6;
//    uint32          collision2  : 1;//Edward remove with trouble
    uint32          reserv1     : 2;
    uint32          isStatic    : 1;
    uint32          reserv0     : 15;
#else
    uint32          valid       : 1;
    uint32          collision   : 1;
    uint32          agingTime   : 6;
    uint32          offset      : 6;
    uint32          collision2  : 1;
    uint32          dedicate    : 1;
    uint32          isStatic    : 1;
    uint32          selIPIdx    : 4;
    uint32          selEIdx     : 10;
    uint32          reserv0     : 1;
#endif
    /* word 2 */
    uint32          intPort     : 16;
    uint32          TCPFlag     : 3;
    uint32          isTCP       : 1;
    uint32          reserv2     : 12;
#endif /*_LITTLE_ENDIAN*/
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_naptTcpUdpTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    ipaddr_t        intIPAddr;
    /* word 1 */
    uint32          ICMPIDL     : 15;
    uint32          isStatic    : 1;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv1     : 2;
#else
    uint32          type        : 2;
#endif /*CONFIG_RTL8650BBASIC*/
    uint32          offset      : 6;
    uint32          agingTime   : 6;
    uint32          collision   : 1;
    uint32          valid       : 1;
    /* word 2 */
    uint32          reserv2     : 15;
    uint32          count       : 16;
    uint32          ICMPIDH     : 1;
#else /*_LITTLE_ENDIAN*/
    /* word 0 */
    ipaddr_t        intIPAddr;
    /* word 1 */
    uint32          valid       : 1;
    uint32          collision   : 1;
    uint32          agingTime   : 6;
    uint32          offset      : 6;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv1     : 2;
#else
    uint32          type        : 2;
#endif /*CONFIG_RTL8650BBASIC*/
    uint32          isStatic    : 1;
    uint32          ICMPIDL     : 15;
    /* word 2 */
    uint32          ICMPIDH     : 1;
    uint32          count       : 16;
    uint32          reserv2     : 15;
#endif /*_LITTLE_ENDIAN*/
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_naptIcmpTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    union {
        struct {
            /* word 0 */
            uint16          dMacP31_16;
            uint16          dMacP15_0;
            /* word 1 */
            uint16          dMacM15_0;
            uint16          dMacP47_32;
            /* word 2 */
            uint16          dMacM47_32;
            uint16          dMacM31_16;
            /* word 3 */
            uint16          sMacP31_16;
            uint16          sMacP15_0;
            /* word 4 */
            uint16          sMacM15_0;
            uint16          sMacP47_32;
            /* word 5 */
            uint16          sMacM47_32;
            uint16          sMacM31_16;
            /* word 6 */
            uint16          ethTypeM;
            uint16          ethTypeP;
        } ETHERNET;
        struct {
            /* word 0 */
            uint32          reserv1     : 24;
            uint32          gidxSel     : 8;
            /* word 1~6 */
            uint32          reserv2[6];
        } IFSEL;
        struct {
            /* word 0 */
            ipaddr_t        sIPP;
            /* word 1 */
            ipaddr_t        sIPM;
            /* word 2 */
            ipaddr_t        dIPP;
            /* word 3 */
            ipaddr_t        dIPM;
            union {
                struct {
                    /* word 4 */
                    uint8           IPProtoM;
                    uint8           IPProtoP;
                    uint8           IPTOSM;
                    uint8           IPTOSP;
                    /* word 5 */
#if 	0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
                    uint32          reserv0     : 26;
#else
                    uint32          reserv0     : 20;
                    uint32          identSDIPM  : 1;
                    uint32          identSDIPP  : 1;
                    uint32          HTTPM       : 1;
                    uint32          HTTPP       : 1;
                    uint32          FOM         : 1;
                    uint32          FOP         : 1;
#endif /*CONFIG_RTL8650BBASIC*/
                    uint32          IPFlagM     : 3;
                    uint32          IPFlagP     : 3;
                    /* word 6 */
                    uint32          reserv1;
                } IP;
                struct {
                    /* word 4 */
                    uint8           ICMPTypeM;
                    uint8           ICMPTypeP;
                    uint8           IPTOSM;
                    uint8           IPTOSP;
                    /* word 5 */
                    uint16          reserv0;
                    uint8           ICMPCodeM;
                    uint8           ICMPCodeP;
                    /* word 6 */
                    uint32          reserv1;
                } ICMP;
                struct {
                    /* word 4 */
                    uint8           IGMPTypeM;
                    uint8           IGMPTypeP;
                    uint8           IPTOSM;
                    uint8           IPTOSP;
                    /* word 5,6 */
                    uint32          reserv0[2];
                } IGMP;
                struct {
                    /* word 4 */
                    uint8           TCPFlagM;
                    uint8           TCPFlagP;
                    uint8           IPTOSM;
                    uint8           IPTOSP;
                    /* word 5 */
                    uint16          TCPSPLB;
                    uint16          TCPSPUB;
                    /* word 6 */
                    uint16          TCPDPLB;
                    uint16          TCPDPUB;
                } TCP;
                struct {
                    /* word 4 */
                    uint16          reserv0;
                    uint8           IPTOSM;
                    uint8           IPTOSP;
                    /* word 5 */
                    uint16          UDPSPLB;
                    uint16          UDPSPUB;
                    /* word 6 */
                    uint16          UDPDPLB;
                    uint16          UDPDPUB;
                } UDP;
            } is;
        } L3L4;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
        struct {
            /* word 0 */
            uint16          sMacP31_16;
            uint16          sMacP15_0;
            /* word 1 */
            uint16          sMacM15_0;
            uint16          sMacP47_32;
            /* word 2 */
            uint16          sMacM47_32;
            uint16          sMacM31_16;
            /* word 3 */
            uint32          reserv2     : 6;
            uint32          protoType   : 2;
            uint32          sVidxM      : 3;
            uint32          sVidxP      : 3;
            uint32          spaM        : 9;
            uint32          spaP        : 9;
            /* word 4 */
            ipaddr_t        sIPP;
            /* word 5 */
            ipaddr_t        sIPM;
            /* word 6 */
            uint16          SPORTLB;
            uint16          SPORTUB;
        } SRC_FILTER;
        struct {
            /* word 0 */
            uint16          dMacP31_16;
            uint16          dMacP15_0;
            /* word 1 */
            uint16          dMacM15_0;
            uint16          dMacP47_32;
            /* word 2 */
            uint16          dMacM47_32;
            uint16          dMacM31_16;
            /* word 3 */
            uint32          reserv2     : 24;
            uint32          protoType   : 2;
            uint32          vidxM      : 3;
            uint32          vidxP      : 3;
            /* word 4 */
            ipaddr_t        dIPP;
            /* word 5 */
            ipaddr_t        dIPM;
            /* word 6 */
            uint16          DPORTLB;
            uint16          DPORTUB;
        } DST_FILTER;
#endif /*CONFIG_RTL8650BBASIC*/
    } is;
    /* word 7 */
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 7;
#else
    uint32          pktOpApp    : 3;
    uint32          reserv0     : 4;
#endif /*CONFIG_RTL8650BBASIC*/
    uint32          PPPoEIndex  : 3;
    uint32          vid         : 3;
    uint32          hPriority   : 1;
    uint32          nextHop     : 10; //index of l2, next hop, or rate limit tables
    uint32          actionType  : 4;
    uint32          ruleType    : 4;
#else
    union {
        struct {
            /* word 0 */
            uint16          dMacP15_0;
            uint16          dMacP31_16;
            /* word 1 */
            uint16          dMacP47_32;
            uint16          dMacM15_0;
            /* word 2 */
            uint16          dMacM31_16;
            uint16          dMacM47_32;
            /* word 3 */
            uint16          sMacP15_0;
            uint16          sMacP31_16;
            /* word 4 */
            uint16          sMacP47_32;
            uint16          sMacM15_0;
            /* word 5 */
            uint16          sMacM31_16;
            uint16          sMacM47_32;
            /* word 6 */
            uint16          ethTypeP;
            uint16          ethTypeM;
        } ETHERNET;
        struct {
            /* word 0 */
            uint32          gidxSel     : 8;
            uint32          reserv1     : 24;
            /* word 1~6 */
            uint32          reserv2[6];
        } IFSEL;
        struct {
            /* word 0 */
            ipaddr_t        sIPP;
            /* word 1 */
            ipaddr_t        sIPM;
            /* word 2 */
            ipaddr_t        dIPP;
            /* word 3 */
            ipaddr_t        dIPM;
            union {
                struct {
                    /* word 4 */
                    uint8           IPTOSP;
                    uint8           IPTOSM;
                    uint8           IPProtoP;
                    uint8           IPProtoM;
                    /* word 5 */
                    uint32          IPFlagP     : 3;
                    uint32          IPFlagM     : 3;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
                    uint32          reserv0     : 26;
#else
                    uint32          FOP         : 1;
                    uint32          FOM         : 1;
                    uint32          HTTPP       : 1;
                    uint32          HTTPM       : 1;
                    uint32          identSDIPP  : 1;
                    uint32          identSDIPM  : 1;
                    uint32          reserv0     : 20;
#endif /*CONFIG_RTL8650BBASIC*/
                    /* word 6 */
                    uint32          reserv1;
                } IP;
                struct {
                    /* word 4 */
                    uint8           IPTOSP;
                    uint8           IPTOSM;
                    uint8           ICMPTypeP;
                    uint8           ICMPTypeM;
                    /* word 5 */
                    uint8           ICMPCodeP;
                    uint8           ICMPCodeM;
                    uint16          reserv0;
                    /* word 6 */
                    uint32          reserv1;
                } ICMP;
                struct {
                    /* word 4 */
                    uint8           IPTOSP;
                    uint8           IPTOSM;
                    uint8           IGMPTypeP;
                    uint8           IGMPTypeM;
                    /* word 5,6 */
                    uint32          reserv0[2];
                } IGMP;
                struct {
                    /* word 4 */
                    uint8           IPTOSP;
                    uint8           IPTOSM;
                    uint8           TCPFlagP;
                    uint8           TCPFlagM;
                    /* word 5 */
                    uint16          TCPSPUB;
                    uint16          TCPSPLB;
                    /* word 6 */
                    uint16          TCPDPUB;
                    uint16          TCPDPLB;
                } TCP;
                struct {
                    /* word 4 */
                    uint8           IPTOSP;
                    uint8           IPTOSM;
                    uint16          reserv0;
                    /* word 5 */
                    uint16          UDPSPUB;
                    uint16          UDPSPLB;
                    /* word 6 */
                    uint16          UDPDPUB;
                    uint16          UDPDPLB;
                } UDP;
            } is;
        } L3L4;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
        struct {
            /* word 0 */
            uint16          sMacP15_0;
            uint16          sMacP31_16;
            /* word 1 */
            uint16          sMacP47_32;
            uint16          sMacM15_0;
            /* word 2 */
            uint16          sMacM31_16;
            uint16          sMacM47_32;
            /* word 3 */
            uint32          spaP        : 9;
            uint32          spaM        : 9;
            uint32          sVidxP      : 3;
            uint32          sVidxM      : 3;
            uint32          protoType   : 2;
            uint32          reserv2     : 6;
            /* word 4 */
            ipaddr_t        sIPP;
            /* word 5 */
            ipaddr_t        sIPM;
            /* word 6 */
            uint16          SPORTUB;
            uint16          SPORTLB;
        } SRC_FILTER;
        struct {
            /* word 0 */
            uint16          dMacP15_0;
            uint16          dMacP31_16;
            /* word 1 */
            uint16          dMacP47_32;
            uint16          dMacM15_0;
            /* word 2 */
            uint16          dMacM31_16;
            uint16          dMacM47_32;
            /* word 3 */
            uint32          vidxP      : 3;
            uint32          vidxM      : 3;
            uint32          protoType   : 2;
            uint32          reserv2     : 24;
            /* word 4 */
            ipaddr_t        dIPP;
            /* word 5 */
            ipaddr_t        dIPM;
            /* word 6 */
            uint16          DPORTUB;
            uint16          DPORTLB;
        } DST_FILTER;
#endif /*CONFIG_RTL8650BBASIC*/
    } is;
    /* word 7 */
    uint32          ruleType    : 4;
    uint32          actionType  : 4;
    uint32          nextHop     : 10; //index of l2, next hop, or rate limit tables
    uint32          hPriority   : 1;
    uint32          vid         : 3;
    uint32          PPPoEIndex  : 3;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 7;
#else
    uint32          reserv0     : 4;
    uint32          pktOpApp    : 3;
#endif /*CONFIG_RTL8650BBASIC*/
#endif /*_LITTLE_ENDIAN*/
} rtl8651_tblAsic_aclTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    ipaddr_t        srcIPAddr;
    /* word 1 */
    uint32          srcPortL    : 1;
    uint32          srcVid      : 3;
    uint32          destIPAddrLsbs : 28;
    /* word 2*/
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 19;
#else
    uint32          reserv0     : 11;
    uint32  	   	extIPIndexH : 1;
    uint32          ageTime     : 3;
    uint32          extPortList : 3;
    uint32          srcPortExt  : 1;
#endif /*CONFIG_RTL8650BBASIC*/
    uint32          toCPU       : 1;
    uint32          valid       : 1;
    uint32          extIPIndex  : 3;
    uint32          portList    : 6;
    uint32          srcPortH    : 2;
#else
    /* word 0 */
    ipaddr_t        srcIPAddr;
    /* word 1 */
    uint32          destIPAddrLsbs : 28;
    uint32          srcVid      : 3;
    uint32          srcPortL    : 1;
    /* word 2*/
    uint32          srcPortH    : 2;
    uint32          portList    : 6;
    uint32          extIPIndex  : 3;
    uint32          valid       : 1;
    uint32          toCPU       : 1;
#if 0 //chhuang: #ifndef CONFIG_RTL8650BBASIC
    uint32          reserv0     : 19;
#else
    uint32          srcPortExt  : 1;
    uint32          extPortList : 3;
    uint32          ageTime     : 3;
    uint32  	   	extIPIndexH : 1;
    uint32          reserv0     : 11;
#endif /*CONFIG_RTL8650BBASIC*/
#endif /*_LITTLE_ENDIAN*/
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_ipMulticastTable_t;

#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    uint32          reserv0     : 11;
    uint32          nextHop     : 10;
    uint32          PPPoEIndex  : 3;
    uint32          dstVid      : 3;
    uint32          IPIndex     : 4;
    uint32          type        : 1;
#else
    /* word 0 */
    uint32          type        : 1;
    uint32          IPIndex     : 4;
    uint32          dstVid      : 3;
    uint32          PPPoEIndex  : 3;
    uint32          nextHop     : 10;
    uint32          reserv0     : 11;
#endif /*_LITTLE_ENDIAN*/
    /* word 1 */
    uint32          reservw1;
    /* word 2 */
    uint32          reservw2;
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_nextHopTable_t;

typedef struct {
#ifndef _LITTLE_ENDIAN
    /* word 0 */
    uint32          reserv0     : 2;
    uint32          refillRemainTime    : 6;
    uint32          token       : 24;
    /* word 1 */
    uint32          reserv1     : 2;
    uint32          refillTime  : 6;
    uint32          maxToken    : 24;
    /* word 2 */
    uint32          reserv2     : 8;
    uint32          refill      : 24;
#else
    /* word 0 */
    uint32          token       : 24;
    uint32          refillRemainTime    : 6;
    uint32          reserv0     : 2;
    /* word 1 */
    uint32          maxToken    : 24;
    uint32          refillTime  : 6;
    uint32          reserv1     : 2;
    /* word 2 */
    uint32          refill      : 24;
    uint32          reserv2     : 8;
#endif /*_LITTLE_ENDIAN*/
    /* word 3 */
    uint32          reservw3;
    /* word 4 */
    uint32          reservw4;
    /* word 5 */
    uint32          reservw5;
    /* word 6 */
    uint32          reservw6;
    /* word 7 */
    uint32          reservw7;
} rtl8651_tblAsic_rateLimitTable_t;
#endif /*CONFIG_RTL8650BBASIC*/






int32 _rtl8651_addAsicEntry(uint32 tableType, uint32 eidx, void *entryContent_P);
int32 _rtl8651_forceAddAsicEntry(uint32 tableType, uint32 eidx, void *entryContent_P);
int32 _rtl8651_readAsicEntry(uint32 tableType, uint32 eidx, void *entryContent_P);
int32 _rtl8651_delAsicEntry(uint32 tableType, uint32 startEidx, uint32 endEidx);

uint32 _rtl8651_NaptAgingToSec(uint32 value);
uint32 _rtl8651_NaptAgingToUnit(uint32 sec);
uint32 rtl8651_filterDbIndex(ether_addr_t * macAddr);
uint32 rtl8651_naptTcpUdpTableIndex(int8 isTCP, ipaddr_t srcAddr, uint16 srcPort, ipaddr_t destAddr, uint16 destPort);
uint32 rtl8651_naptIcmpTableIndex(ipaddr_t srcAddr, uint16 icmpId, ipaddr_t destAddr, uint32 * tblIdx);
uint32 rtl8651_ipMulticastTableIndex(ipaddr_t srcAddr, ipaddr_t dstAddr);

int32 rtl8651_clearAsicAllTable(void);

int32 _rtl8651_mapToVirtualRegSpace( void );
int32 _rtl8651_mapToRealRegSpace( void );
int32 rtl8651_initAsic(void);

int32 rtl8651_setAsicOperationLayer(uint32 layer);
int32 rtl8651_getAsicOperationLayer(void);
int32 rtl8651_setAsicSpanningEnable(int8 spanningTreeEnabled);
int32 rtl8651_getAsicSpanningEnable(int8 *spanningTreeEnabled);
void rtl8651_setEthernetPortLinkStatus(uint32 port, int8 linkUp);
int32 rtl8651_updateLinkStatus(void);
int32 rtl8651_setAsicEthernetLinkStatus(uint32 port, int8 linkUp);
int32 rtl8651_getAsicEthernetLinkStatus(uint32 port, int8 *linkUp);
int32 rtl8651_setAsicEthernetPHY(uint32 port, int8 autoNegotiation, uint32 advCapability, uint32 speed, int8 fullDuplex);
int32 rtl8651_getAsicEthernetPHY(uint32 port, int8 *autoNegotiation, uint32 *advCapability, uint32 *speed, int8 *fullDuplex);
int32 rtl8651_setAsicEthernetBandwidthControl(uint32 port, int8 input, uint32 rate);//RTL8651_ASICBC_xxx
int32 rtl8651_getAsicEthernetBandwidthControl(uint32 port, int8 input, uint32 *rate);//RTL8651_ASICBC_xxx
int32 rtl8651_setAsicEthernetBandwidthControlX4(int8 enable);
int32 rtl8651_getAsicEthernetBandwidthControlX4(int8 *enable);
int32 rtl8651_setAsicEthernetBandwidthControlX8(int8 enable);
int32 rtl8651_getAsicEthernetBandwidthControlX8(int8 *enable);
int32 rtl8651_setAsicEthernetMII(uint32 phyAddress, int32 mode, int32 enabled);
int32 rtl8651_setAsicEthernetPHYLoopback(uint32 port, int32 enabled);
int32 rtl8651_getAsicEthernetPHYLoopback(uint32 port, int32 *flag);
int32 rtl8651_setAsicMulticastSpanningTreePortState(uint32 port, uint32 portState);//RTL8651_PORTSTA_xxx
int32 rtl8651_getAsicMulticastSpanningTreePortState(uint32 port, uint32 *portState);//RTL8651_PORTSTA_xxx
int32 rtl8651_setAsicMulticastPortInternal(uint32 port, int8 isInternal);
int32 rtl8651_getAsicMulticastPortInternal(uint32 port, int8 *isInternal);

typedef struct rtl865x_tblAsicDrv_l2Param_s {
	ether_addr_t	macAddr;
	uint32 		memberPortMask; //extension ports [rtl8651_totalExtPortNum-1:0] are located at bits [RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum-1:RTL8651_PORT_NUMBER]
	uint32 		ageSec;
	uint32	 	cpu:1,
				srcBlk:1,
				isStatic:1,
				nhFlag:1;
} rtl865x_tblAsicDrv_l2Param_t;
int32 rtl8651_setAsicL2Table(uint32 row, uint32 column, rtl865x_tblAsicDrv_l2Param_t *l2p);
int32 rtl8651_delAsicL2Table(uint32 row, uint32 column);
unsigned int rtl8651_asicL2DAlookup(uint8 *dmac);
//rtl8651_getAsicL2Table() is NULl allowed
int32 rtl8651_getAsicL2Table(uint32 row, uint32 column, rtl865x_tblAsicDrv_l2Param_t *l2p);
int32  rtl8651_updateAsicLinkAggregatorLMPR(int32 portmask);
int32 rtl8651_setAsicLinkAggregator(uint32 portMask);
int32 rtl8651_getAsicLinkAggregator(uint32 * portMask, uint32 *mapping);
int32 rtl8651_turnOnHardwiredProtoTrap(uint8 protoType, uint16 protoContent);
int32 rtl8651_turnOffHardwiredProtoTrap(uint8 protoType, uint16 protoContent);
int32 rtl8651_getHardwiredProtoTrap(uint8 protoType, uint16 protoContent, int8 *isEnable);
typedef struct rtl865x_tblAsicDrv_protoTrapParam_s {
	uint8 type;
	uint16 content;
} rtl865x_tblAsicDrv_protoTrapParam_t;
int32 rtl8651_setAsicProtoTrap(uint32 index, rtl865x_tblAsicDrv_protoTrapParam_t *protoTrapp);
int32 rtl8651_delAsicProtoTrap(uint32 index);
int32 rtl8651_getAsicProtoTrap(uint32 index, rtl865x_tblAsicDrv_protoTrapParam_t *protoTrapp);
int32 rtl8651_setAsicPvid(uint32 port, uint32 pvidx);
int32 rtl8651_getAsicPvid(uint32 port, uint32 *pvidx);


typedef struct rtl865x_tblAsicDrv_vlanParam_s {
	ether_addr_t macAddr;
	uint32 	memberPortMask; //extension ports [rtl8651_totalExtPortNum-1:0] are located at bits [RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum-1:RTL8651_PORT_NUMBER]
	uint32 	untagPortMask; //extension ports [rtl8651_totalExtPortNum-1:0] are located at bits [RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum-1:RTL8651_PORT_NUMBER]
	uint16 	macAddrNumber;
	uint16 	vid;
	uint32 	inAclStart, inAclEnd, outAclStart, outAclEnd;
	int8 		portState[9]; //extension ports [rtl8651_totalExtPortNum-1:0] are located at entries [RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum-1:RTL8651_PORT_NUMBER]
	uint32 	mtu;
	uint16 	internal:1,
			enableRoute:1,
			broadcastToCpu:1,
			promiscuous:1,
			DMZFlag:1,
#ifdef CONFIG_RTL865XB_ENRT	/* refer to _rtl8651_addVlan() for the meaning of this compile flag */
			macNonExist:1,	/* If this bit is set, only L2 forwarding is performed over this VLAN. */
#endif
			valid:1;
} rtl865x_tblAsicDrv_vlanParam_t;
int32 rtl8651_setAsicVlan(rtl865x_tblAsicDrv_vlanParam_t *vlanp);
int32 rtl8651_delAsicVlan(uint16 vid);
int32 rtl8651_getAsicVlan(uint16 vid, rtl865x_tblAsicDrv_vlanParam_t *vlanp);
typedef struct rtl865x_tblAsicDrv_pppoeParam_s {
	uint16 sessionId;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
	uint16 age;
#endif /* CONFIG_RTL8650BBASIC */
} rtl865x_tblAsicDrv_pppoeParam_t;
int32 rtl8651_setAsicPppoe(uint32 index, rtl865x_tblAsicDrv_pppoeParam_t *pppoep);
int32 rtl8651_getAsicPppoe(uint32 index, rtl865x_tblAsicDrv_pppoeParam_t *pppoep);
typedef struct rtl865x_tblAsicDrv_routingParam_s {
	    ipaddr_t ipAddr;
	    ipaddr_t ipMask;
	    uint32 process; //0: pppoe, 1:direct, 2:indirect, 4:Strong CPU, 5:napt nexthop
	    uint32 vidx;
	    uint32 arpStart;
	    uint32 arpEnd;
	    uint32 arpIpIdx; /* for RTL8650B C Version Only */
	    uint32 nextHopRow;
	    uint32 nextHopColumn;
	    uint32 pppoeIdx;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
	    uint32 nhStart; //exact index
	    uint32 nhNum; //exact number
	    uint32 nhNxt;
	    uint32 nhAlgo;
	    uint32 ipDomain;
#endif
} rtl865x_tblAsicDrv_routingParam_t;
int32 rtl8651_setAsicRouting(uint32 index, rtl865x_tblAsicDrv_routingParam_t *routingp);
int32 rtl8651_delAsicRouting(uint32 index);
int32 rtl8651_getAsicRouting(uint32 index, rtl865x_tblAsicDrv_routingParam_t *routingp);
typedef struct rtl865x_tblAsicDrv_arpParam_s {
	uint32 nextHopRow;
	uint32 nextHopColumn;
} rtl865x_tblAsicDrv_arpParam_t;
int32 rtl8651_setAsicArp(uint32 index, rtl865x_tblAsicDrv_arpParam_t *arpp);
int32 rtl8651_delAsicArp(uint32 index);
int32 rtl8651_getAsicArp(uint32 index, rtl865x_tblAsicDrv_arpParam_t *arpp);
int32 rtl8651_setAsicGidxRegister(uint32 regValue);
int32 rtl8651_getAsicGidxRegister(uint32 * reg);
typedef struct rtl865x_tblAsicDrv_extIntIpParam_s {
	    ipaddr_t 	extIpAddr;
	    ipaddr_t 	intIpAddr;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
	    uint32 		nhIndex; //index of next hop table
#endif
	    uint32 		localPublic:1,
	           		nat:1;
} rtl865x_tblAsicDrv_extIntIpParam_t;
int32 rtl8651_setAsicExtIntIpTable(uint32 index, rtl865x_tblAsicDrv_extIntIpParam_t *extIntIpp);
#ifdef RTL865xB_MCAST_CUTCD_TTL_PATCH
int32 rtl8651_setInvalidAsicExtIntIpTable(uint32 index, rtl865x_tblAsicDrv_extIntIpParam_t *extIntIpp);
#endif	/* RTL865xB_MCAST_CUTCD_TTL_PATCH */
int32 rtl8651_delAsicExtIntIpTable(uint32 index);
int32 rtl8651_getAsicExtIntIpTable(uint32 index, rtl865x_tblAsicDrv_extIntIpParam_t *extIntIpp);
typedef struct rtl865x_tblAsicDrv_serverPortParam_s {
	ipaddr_t extIpAddr;
	ipaddr_t intIpAddr;
	uint16 extPort;
	uint16 intPort;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
	uint32 nhIndex; //index of next hop table
	uint32 portRange:1;
#endif
	uint32 valid:1;
} rtl865x_tblAsicDrv_serverPortParam_t;
int32 rtl8651_setAsicServerPortTable(uint32 index, rtl865x_tblAsicDrv_serverPortParam_t *serverPortp);
int32 rtl8651_delAsicServerPortTable(uint32 index);
int32 rtl8651_getAsicServerPortTable(uint32 index, rtl865x_tblAsicDrv_serverPortParam_t *serverPortp);
int32 rtl8651_setAsicAgingFunction(int8 l2Enable, int8 l4Enable);
int32 rtl8651_getAsicAgingFunction(int8 * l2Enable, int8 * l4Enable);
int32 rtl8651_setAsicNaptAutoAddDelete(int8 autoAdd, int8 autoDelete);
int32 rtl8651_getAsicNaptAutoAddDelete(int8 *autoAdd, int8 *autoDelete);
int32 rtl8651_setAsicNaptIcmpTimeout(uint32 timeout);
int32 rtl8651_getAsicNaptIcmpTimeout(uint32 *timeout);
int32 rtl8651_setAsicNaptUdpTimeout(uint32 timeout);
int32 rtl8651_getAsicNaptUdpTimeout(uint32 *timeout);
int32 rtl8651_setAsicNaptTcpLongTimeout(uint32 timeout);
int32 rtl8651_getAsicNaptTcpLongTimeout(uint32 *timeout);
int32 rtl8651_setAsicNaptTcpMediumTimeout(uint32 timeout);
int32 rtl8651_getAsicNaptTcpMediumTimeout(uint32 *timeout);
int32 rtl8651_setAsicNaptTcpFastTimeout(uint32 timeout);
int32 rtl8651_getAsicNaptTcpFastTimeout(uint32 *timeout);
//Mirror Port
int32 rtl8651_setAsicPortMirror(uint32 mTxMask, uint32 mRxMask, uint32 mPortMask);
int32 rtl8651_getAsicPortMirror(uint32 *mRxMask, uint32 *mTxMask, uint32 *mPortMask);

typedef struct rtl865x_tblAsicDrv_nextHopParam_s {
	uint32 nextHopRow;
	uint32 nextHopColumn;
	uint32 pppoeIdx;
	uint32 dvid;
	uint32 extIntIpIdx;
	uint32 isPppoe:1;
} rtl865x_tblAsicDrv_nextHopParam_t;
int32 rtl8651_setAsicNextHopTable(uint32 index, rtl865x_tblAsicDrv_nextHopParam_t *nextHopp);
int32 rtl8651_getAsicNextHopTable(uint32 index, rtl865x_tblAsicDrv_nextHopParam_t *nextHopp);



//NAPT entryType definitions

#define RTL8651_DYNAMIC_NAPT_ENTRY		(0x0<<0)
#define RTL8651_STATIC_NAPT_ENTRY			(0x1<<0)
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
#define 	RTL8651_LIBERAL_NAPT_ENTRY			(0x2<<0)

//In RTL8651B, all 3 bits in TCPFlag field is reused if entry is a RTL8651_LIBERAL_NAPT_ENTRY. 
//Don't change these values!!!
#define RTL8651_NAPT_OUTBOUND_FLOW				(1<<2) 	//exact value in ASIC
#define RTL8651_NAPT_INBOUND_FLOW				(0<<2)	//exact value in ASIC
#define RTL8651_NAPT_UNIDIRECTIONAL_FLOW		(2<<2)	//exact value in ASIC
#define RTL8651_NAPT_SYNFIN_QUIET				(4<<2)	//exact value in ASIC 
#define RTL8651_NAPT_CHKAUTOLEARN				(1<<5)
#endif
//In RTL8651, TCPFlag field records current state of entry
#define RTL8651_TCPNAPT_WAIT4FIN			(0x4 <<2) //exact value in ASIC
#define RTL8651_TCPNAPT_WAITINBOUND			(0x2 <<2) //exact value in ASIC
#define RTL8651_TCPNAPT_WAITOUTBOUND		(0x1 <<2) //exact value in ASIC


typedef struct rtl865x_tblAsicDrv_naptTcpUdpParam_s {
	ipaddr_t 	insideLocalIpAddr;
	uint16 	insideLocalPort;
	uint32 	ageSec;
	uint8 	tcpFlag;
	uint8 	offset;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
	uint8 	selExtIPIdx;
	uint16 	selEIdx;
#endif /* CONFIG_RTL8650BBASIC */
	uint32 	isTcp:1,
			isCollision:1,
			isStatic:1,
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
			isCollision2:1,
			isDedicated:1,
#endif
			isValid:1;
} rtl865x_tblAsicDrv_naptTcpUdpParam_t;
int32 rtl8651_setAsicNaptTcpUdpTable(int8 forced, uint32 index, rtl865x_tblAsicDrv_naptTcpUdpParam_t *naptTcpUdpp);
int32 rtl8651_getAsicNaptTcpUdpTable(uint32 index, rtl865x_tblAsicDrv_naptTcpUdpParam_t *naptTcpUdpp);
int32 rtl8651_delAsicNaptTcpUdpTable(uint32 start, uint32 end);
int32 _rtl8651_findAsicExtIpTableIdx(ipaddr_t extIp);
int32 rtl8651_setAsicRawNaptTable(uint32 index, void * entry, int8 forced);
int32 rtl8651_getAsicRawNaptTable(uint32 index, void  *entry);
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
int32 rtl8651_setAsicLiberalNaptTcpUdpTable(int8 forced, uint16 index, ipaddr_t insideLocalIpAddr, uint16 insideLocalPort, int8 selExtIPIdx, uint16 insideGlobalPort, uint32 ageSec, int8 entryType, int8 isTcp, int8 isCollision, int8 isCollision2, int8 isValid);
#endif

typedef struct rtl865x_tblAsicDrv_naptIcmpParam_s {
	ipaddr_t 	insideLocalIpAddr;
	uint16 	insideLocalId;
	uint16 	ageSec;
	uint8 	offset;
	uint32 	isStatic:1,
			isCollision:1,
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
			isSpi:1,
			isPptp:1,
#endif
			isValid:1;
	uint16	count;
} rtl865x_tblAsicDrv_naptIcmpParam_t;
int32 rtl8651_setAsicNaptIcmpTable(int8 forced, uint32 index, rtl865x_tblAsicDrv_naptIcmpParam_t *naptIcmpp);
int32 rtl8651_getAsicNaptIcmpTable(uint32 index, rtl865x_tblAsicDrv_naptIcmpParam_t *naptIcmpp);
int32 rtl8651_setAsicL4Offset(uint16 start, uint16 end);
int32 rtl8651_getAsicL4Offset(uint16 *start, uint16 *end);

typedef struct rtl865x_tblAsicDrv_algParam_s {
	uint16 port;
} rtl865x_tblAsicDrv_algParam_t;
int32 rtl8651_setAsicAlg(uint32 index, rtl865x_tblAsicDrv_algParam_t *algp);
int32 rtl8651_delAsicAlg(uint32 index);
int32 rtl8651_getAsicAlg(uint32 index, rtl865x_tblAsicDrv_algParam_t *algp);
int32 rtl8651_getAsicNaptTcpUdpOffset(uint16 index, uint16 * offset, int8 * isValid);
int32 rtl8651_getAsicNaptIcmpOffset(uint16 index, uint16 * offset, int8 * isValid);

typedef struct rtl865x_tblAsicDrv_multiCastParam_s {
	ipaddr_t	sip;
	ipaddr_t	dip;
	uint16	svid;
	uint16	port;
	uint32	mbr;
	uint16	age;
	uint16	cpu;
	uint16	extIdx;
} rtl865x_tblAsicDrv_multiCastParam_t;

int32 rtl8651_setAsicIpMulticastTable(rtl865x_tblAsicDrv_multiCastParam_t *mCast_t);
int32 rtl8651_delAsicIpMulticastTable(uint32 index);
int32 rtl8651_getAsicIpMulticastTable(uint32 index, rtl865x_tblAsicDrv_multiCastParam_t *mCast_t);
//Counter

typedef struct rtl865x_tblAsicDrv_basicCounterParam_s {
	uint32	mbr;
	uint32	txPackets;
	uint32	txBytes;
	uint32	rxPackets;
	uint32	rxBytes;
	uint32	rxErrors;
	uint32	drops;
	uint32	cpus;
} rtl865x_tblAsicDrv_basicCounterParam_t;

int32 rtl8651_returnAsicCounter(uint32 offset);//Backward compatable, deprecated
int32 rtl8651_clearAsicCounter(void);//Backward compatable, deprecated

int32 rtl8651_clearAsicSpecifiedCounter(uint32 counterIdx);//Clear specified ASIC counter
int32 rtl8651_resetAsicCounterMemberPort(uint32 counterIdx);//Clear the specified ASIC counter member port to null set
int32 rtl8651_addAsicCounterMemberPort(uint32 counterIdx, uint32 port);//Add the specified physical port into counter monitor set
int32 rtl8651_delAsicCounterMemberPort(uint32 counterIdx, uint32 port);//Delete the specified physical port into counter monitor set
int32 rtl8651_getAsicCounter(uint32 counterIdx, rtl865x_tblAsicDrv_basicCounterParam_t * basicCounter);


//Rate Limit
typedef struct rtl865x_tblAsicDrv_rateLimitParam_s {
	uint32 	token;
	uint32	maxToken;
	uint32	t_remainUnit;
	uint32 	t_intervalUnit;
	uint32	refill_number;
} rtl865x_tblAsicDrv_rateLimitParam_t;

int32 rtl8651_setAsicRateLimitTable(uint32 index, rtl865x_tblAsicDrv_rateLimitParam_t *rateLimit_t);
int32 rtl8651_delAsicRateLimitTable(uint32 index);
int32 rtl8651_getAsicRateLimitTable(uint32 index, rtl865x_tblAsicDrv_rateLimitParam_t *rateLimit_t);


//Misc.
int32 rtl8651_setBroadCastStormReg(int8 enable);
int32 rtl8651_getBroadCastSTormReg(int8 *enable);
int32 rtl8651_testAsicDrv(void);

typedef struct {
    uint32 spa;
    uint32 bc;
    uint32 vid;
    uint32 vlan;
    uint32 pppoe;
    uint8  sip[4];
    uint32 sprt;
                       
    uint8  dip[4];
    uint32 dprt;
    
    uint32 ipptl;
    uint32 ipflg;
    uint32 iptos;
    uint32 tcpflg;
    uint32 type;
    uint32 prtnmat;
	uint32 ethrtype;
    uint8  da[6];
    uint8  pad1[2];
    uint8  sa[6];
    uint8  pad2[2];
    uint32 hp;
    uint32 llc;
    uint32 udp_nocs;
    uint32 ttlst;
    uint32 pktend;
    uint32 dirtx;
    uint32 l4crcok;
    uint32 l3crcok;
    uint32 ipfragif;
    uint32 dp ;
    uint32 hp2;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
	uint16	ipLen;
	uint8	L2only;
#endif
	
} rtl8651_tblAsic_hsb_param_watch_t;

typedef struct {

	uint8  mac[6];
	uint8  pad1[2];
	uint8  ip[4];
	uint32 prt;
	uint32 l3cs;
	uint32 l4cs;
	uint32 egress;
	uint32 l2act;
	uint32 l34act;
	uint32 dirtx;
	uint32 type;
	uint32 llc;
	uint32 vlan;
	uint32 dvid;
	uint32 pppoe;
	uint32 pppid;
	uint32 ttl_1;
	uint32 dpc;									
	uint32 bc;
	uint32 pktend;
	uint32 mulcst;
	uint32 svid;
	uint32 cpursn;
	uint32 spa;
	uint32 lastfrag;
	uint32 frag;
	uint32 l4csok;
	uint32 l3csok;
	uint32 bc10_5;
#if 1 //chhuang: #ifdef CONFIG_RTL8650BBASIC
	uint32 extSrcPortNum;
	uint32 extDstPortMask;
	uint32 cpuacl;
	uint32 extTTL_1;
#endif
} rtl8651_tblAsic_hsa_param_watch_t;










void rtl8651_updateLinkChangePendingCount(void);

int32 rtl8651_getAsicHsB(rtl8651_tblAsic_hsb_param_watch_t * hsbWatch);
int32 rtl8651_getAsicHsA(rtl8651_tblAsic_hsa_param_watch_t * hsaWatch);

extern int8 rtl8651_tblAsicDrv_Id[];

int32 rtl8651_setAsicPortPatternMatch(uint32 port, uint32 pattern, uint32 patternMask, int32 operation);
int32 rtl8651_getAsicFlowControlRegister(uint32 *fcren);
int32 rtl8651_setAsicFlowControlRegister(uint32 port, int8 enable);
int32 rtl8651_setAsicHLQueueWeight(uint32 weight);
int32 rtl8651_setAsicPortPriority(uint32 port, int8 highPriority);
int32 rtl8651_getAsicQoSControlRegister(uint32 *qoscr);
int32 rtl8651_setAsicDiffServReg(uint32 dscp, int8 highPriority);
int32 rtl8651_getAsicDiffServReg(uint32 *dscr0, uint32 *dscr1);

int32 rtl8651_asicEthernetCableMeter(uint32 port, int32 *rxStatus, int32 *txStatus);
int32 rtl8651_getAsicEthernetMII(uint32 *phyAddress);
int32 rtl8651_queryProtocolBasedVLAN( uint32 ruleNo, uint8* ProtocolType, uint16* ProtocolValue );


/*===============================================
 * ASIC DRIVER DEFINITION: Protocol-based VLAN
 *==============================================*/
#define RTL8651_PBV_RULE_IPX				1	/* Protocol-based VLAN rule 1: IPX */
#define RTL8651_PBV_RULE_NETBIOS			2	/* Protocol-based VLAN rule 2: NetBIOS */
#define RTL8651_PBV_RULE_PPPOE_CONTROL		3	/* Protocol-based VLAN rule 3: PPPoE Control */
#define RTL8651_PBV_RULE_PPPOE_SESSION		4	/* Protocol-based VLAN rule 4: PPPoE Session */
#define RTL8651_PBV_RULE_USR1				5	/* Protocol-based VLAN rule 5: user-defined 1 */
#define RTL8651_PBV_RULE_USR2				6	/* Protocol-based VLAN rule 6: user-defined 2 */
#define RTL8651_PBV_RULE_MAX				7

int32 rtl8651_defineProtocolBasedVLAN( uint32 ruleNo, uint8 ProtocolType, uint16 ProtocolValue );
int32 rtl8651_setProtocolBasedVLAN( uint32 ruleNo, uint32 port, uint8 valid, uint8 vlanIdx );
int32 rtl8651_getProtocolBasedVLAN( uint32 ruleNo, uint32 port, uint8* valid, uint8* vlanIdx );

int32 rtl8651_autoMdiMdix(uint32 port, uint32 isEnable);
int32 rtl8651_getAutoMdiMdix(uint32 port, uint32 *isEnable);
int32 rtl8651_selectMdiMdix(uint32 port, uint32 isMdi);
int32 rtl8651_getSelectMdiMdix(uint32 port, uint32 *isMdi);




uint32 _Is4WayHashEnabled( void );
int32 _set4WayHash( int32 enable );

/* Note: 	!IS_ENTRY_VALID() != IS_ENTRY_INVALID() when "v" is meaningful */
#define IS_ENTRY_VALID(entry)		((entry)->valid & (entry)->v)
#define IS_ENTRY_INVALID(entry)		((entry)->valid==0 && (entry)->v==1)

#if	RTL8651_ENABLETTLMINUSINE
int32	rtl8651_setAsicTtlMinusStatus(int32 enable);
#endif

#endif

