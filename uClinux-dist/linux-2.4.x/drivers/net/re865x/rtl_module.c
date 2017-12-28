

#include "rtl865x/rtl_types.h"
#include "rtl865x/asicRegs.h"
#include "rtl865x/mbuf.h"
#include <linux/module.h>

#if !defined(RTL865X_TEST) && !defined(RTL865X_MODEL_USER)
#ifdef __linux__
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#endif
#include "swNic2.h"
#include "rtl865x/rtl_glue.h"
#endif
#include "rtl865x/rtl_utils.h"
#include "rtl865x/assert.h"
#include "rtl865x/rtl8651_tblDrv.h"
#include "rtl865x/rtl8651_tblDrvFwd.h"
#include "rtl865x/rtl8651_layer2fwd.h"

#if defined(RTL865X_TEST) || defined(RTL865X_MODEL_USER)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include <linux/linkage.h>
#ifndef CONFIG_RTL865X_LIGHT_ROMEDRV
#include <mbufGen.h>
#endif
#include <time.h>
#include <sys/time.h>
#include "extPortModule.h"
#endif

#ifdef __linux__
#include <asm/unistd.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#include <linux/init.h>

/* =============================	*/
/*	These header files should be removed.		*/
/*	Only for model compilation temporarily.	*/
/* =============================	*/
#ifdef RTL865X_MODEL_USER
#include "rtl865x/rtl8651_tblDrvLocal.h"
#include "rtl865x/rtl8651_layer2local.h"
#include "rtl865x/rtl8651_alg_qos.h"
#endif
#include "rtl865x/rtl8651_layer2fwdLocal.h"
#include "rtl865x/rtl8651_layer2fwd.h"

#include "cle/rtl_cle.h"
/*****************************************************
	** functions only used by rtl_glue.c **
******************************************************/


		int32 *module_external_data_rtl8651_totalExtPortNum = NULL; //this replaces all RTL8651_EXTPORT_NUMBER defines
		int32 (*module_external_rtl8651_fwdEngineRegExtDevice)(	uint32 portNumber,
											uint16 defaultVID,
											uint32 *linkID_p,
											void *extDevice)=NULL;
		int32 (*module_external_rtl8651_fwdEngineUnregExtDevice)(uint32 linkID)=NULL;				
		int32 (*module_external_rtl8651_fwdEngineGetLinkIDByExtDevice)(void *extDevice)=NULL;
		int32 (*module_external_rtl8651_fwdEngineSetExtDeviceVlanProperty)(uint32 linkID, uint16 vid, uint32 property) = NULL;
		int32 (*module_external_ether_ntoa_r)(ether_addr_t *n, uint8 *a) = NULL;
		void (*module_external_memDump)(void *start, uint32 size, int8 * strHeader)=NULL;
#ifdef CONFIG_RTL865XB
		uint32 *module_external_data_totalRxPkthdr = NULL;
		uint32 *module_external_data_totalTxPkthdr = NULL;
#endif
#ifdef CONFIG_RTL865XC
		int32 (*module_external_swNic_getRingSize)(uint32 ringType, uint32 *ringSize, uint32 ringIdx) = NULL;
#endif		
		int32 (*module_external_mBuf_freeOneMbufPkthdr)(struct rtl_mBuf *m, void **buffer, uint32 *id, uint16 *size)=NULL;
		int32 (*module_external_mBuf_attachCluster)(struct rtl_mBuf *m, void *buffer, uint32 id, uint32 size, uint16 datalen, uint16 align)=NULL;
		int32 (*module_external_mBuf_leadingSpace)(struct rtl_mBuf * m)=NULL;
		int32 (*module_external_rtl8651_fwdEngineExtPortRecv)(	void *id,
														uint8 *data,
														uint32 len,
														uint16 myvid,
														uint32 myportmask,
														uint32 linkID)=NULL;
    #ifdef SWNIC_DEBUG 
		int32 *module_external_data_nicDbgMesg = NULL;
    #endif	
   #if defined(CONFIG_RTL865X_CLE) 
/*    		cle_exec_t *module_external_data__initCmdList=NULL;	*/
/*    		int	module_external_data__initCmdTotal = 0;			*/
    		int32 (*module_external_cle_cmdParser)( int8 *cmdString, cle_exec_t *CmdFmt, int8 *delimiter) = NULL;
   #endif

EXPORT_SYMBOL(module_external_data_rtl8651_totalExtPortNum);
EXPORT_SYMBOL(module_external_rtl8651_fwdEngineRegExtDevice);
EXPORT_SYMBOL(module_external_rtl8651_fwdEngineUnregExtDevice);
EXPORT_SYMBOL(module_external_rtl8651_fwdEngineGetLinkIDByExtDevice);
EXPORT_SYMBOL(module_external_rtl8651_fwdEngineSetExtDeviceVlanProperty);
EXPORT_SYMBOL(module_external_ether_ntoa_r);
EXPORT_SYMBOL(module_external_memDump);
#ifdef CONFIG_RTL865XB
EXPORT_SYMBOL(module_external_data_totalRxPkthdr);
EXPORT_SYMBOL(module_external_data_totalTxPkthdr);
#endif
#ifdef CONFIG_RTL865XC
EXPORT_SYMBOL(module_external_swNic_getRingSize);
#endif
EXPORT_SYMBOL(module_external_mBuf_freeOneMbufPkthdr);
EXPORT_SYMBOL(module_external_mBuf_leadingSpace);
EXPORT_SYMBOL(module_external_mBuf_attachCluster);
EXPORT_SYMBOL(module_external_rtl8651_fwdEngineExtPortRecv);
EXPORT_SYMBOL(module_external_data_nicDbgMesg);
#if	defined(CONFIG_RTL865X_CLE)
/*EXPORT_SYMBOL(module_external_data__initCmdList);	*/
/*EXPORT_SYMBOL(module_external_data__initCmdTotal);	*/
EXPORT_SYMBOL(module_external_cle_cmdParser);
#endif


#if defined(CONFIG_RTL865X_IPSEC)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)

int32(*module_external_rtl8651_registerIpcompCallbackFun)(int32 (*compress)(struct rtl_pktHdr *pkthdr,void *iph,uint32 spi, int16 mtu),
											 int32 (*decompress)(struct rtl_pktHdr *pkthdr,void *iph,uint32 spi, int16 mtu)) = NULL;
int32(*module_external_rtl8651_addIpsecSpi)( ipaddr_t edstIp, uint32 spi, uint32 proto,
                           ipaddr_t srcIp, uint32 cryptoType, void* cryptoKey, uint32 authType, void* authKey,
                           ipaddr_t dstIp, int32 lifetime, int32 replayWindow, uint32 nattPort, uint32 flags ) = NULL;
int32(*module_external_rtl8651_delIpsecSpi)( ipaddr_t edstIp, uint32 spi, uint32 proto ) = NULL;
int32(*module_external_rtl8651_addIpsecSpiGrp)( ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                              ipaddr_t edstIp2, uint32 spi2, uint32 proto2,
                              ipaddr_t edstIp3, uint32 spi3, uint32 proto3,
                              ipaddr_t edstIp4, uint32 spi4, uint32 proto4,
                              uint32 flags ) = NULL;
int32(*module_external_rtl8651_delIpsecSpiGrp)( ipaddr_t edstIp1, uint32 spi1, uint32 proto1 ) = NULL;
int32(*module_external_rtl8651_delIpsecERoute)( ipaddr_t srcIp, ipaddr_t srcIpMask,
                              ipaddr_t dstIp, ipaddr_t dstIpMask ) = NULL;
int32(*module_external_rtl8651_addIpsecERoute)( ipaddr_t srcIp, ipaddr_t srcIpMask,
                              ipaddr_t dstIp, ipaddr_t dstIpMask,
                              ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                              uint32 flags ) = NULL;


EXPORT_SYMBOL(module_external_rtl8651_registerIpcompCallbackFun);
EXPORT_SYMBOL(module_external_rtl8651_addIpsecSpi);
EXPORT_SYMBOL(module_external_rtl8651_delIpsecSpi);
EXPORT_SYMBOL(module_external_rtl8651_addIpsecSpiGrp);
EXPORT_SYMBOL(module_external_rtl8651_delIpsecSpiGrp);
EXPORT_SYMBOL(module_external_rtl8651_delIpsecERoute);
EXPORT_SYMBOL(module_external_rtl8651_addIpsecERoute);
#endif


struct rtl_mBuf *(*module_external_mBuf_get)(int32 how, int32 unused, uint32 Nbuf) = NULL;
int32 (*module_external_mBuf_reserve)(struct rtl_mBuf * m, uint16 headroom) = NULL;
uint32 (*module_external_mBuf_freeMbufChain)(register struct rtl_mBuf *m) = NULL;
struct rtl_mBuf *(*module_external_mBuf_getPkthdr)(struct rtl_mBuf *pMbuf, int32 how)=NULL;

#ifdef CONFIG_RTL865XC 
		int32 (*module_external_swNic_write)(void * output,int isHighQueue) = NULL; 
		int32 (*module_external_swNic_isrReclaim)(uint32 rxDescIdx,uint32 mbufDescIdx, struct rtl_pktHdr*pPkthdr,struct rtl_mBuf *pMbuf) = NULL;
#ifdef NIC_DEBUG_PKTDUMP
		void (*module_external_swNic_pktdump)(	uint32 currentCase,
									struct rtl_pktHdr *pktHdr_p,
									char *pktContent_p,
									uint32 pktLen,
									uint32 additionalInfo) = NULL;
#endif									
#else
		int32 (*module_external_swNic_write)(void * output) = NULL; 
		int32 (*module_external_swNic_isrReclaim)(uint32 rxDescIdx, struct rtl_pktHdr*pPkthdr,struct rtl_mBuf *pMbuf) = NULL;
#endif
#ifdef CONFIG_RTL865X_ROMEPERF
int32 (*module_external_rtl8651_romeperfEnterPoint)( uint32 index ) = NULL;
int32 (*module_external_rtl8651_romeperfExitPoint)( uint32 index ) = NULL;
#endif


EXPORT_SYMBOL(module_external_mBuf_get);
EXPORT_SYMBOL(module_external_mBuf_reserve);
EXPORT_SYMBOL(module_external_mBuf_freeMbufChain);
EXPORT_SYMBOL(module_external_swNic_write);
EXPORT_SYMBOL(module_external_swNic_isrReclaim);
EXPORT_SYMBOL(module_external_mBuf_getPkthdr);
#ifdef CONFIG_RTL865XC 
	#ifdef NIC_DEBUG_PKTDUMP
EXPORT_SYMBOL(module_external_swNic_pktdump);
	#endif
#endif	
#ifdef CONFIG_RTL865X_ROMEPERF
EXPORT_SYMBOL(module_external_rtl8651_romeperfEnterPoint);
EXPORT_SYMBOL(module_external_rtl8651_romeperfExitPoint);
#endif


/*******************************************************************
	**for the functions which used in kernel but defined in RomeDrv **
*******************************************************************/

inline int32 rtl8651_fwdEngineRegExtDevice(	uint32 portNumber,
											uint16 defaultVID,
											uint32 *linkID_p,
											void *extDevice)
{
	return module_external_rtl8651_fwdEngineRegExtDevice(portNumber, defaultVID, linkID_p, extDevice);
}
inline int32 rtl8651_fwdEngineUnregExtDevice(uint32 linkID)
{
	return module_external_rtl8651_fwdEngineUnregExtDevice(linkID);
}
inline int32 rtl8651_fwdEngineGetLinkIDByExtDevice(void *extDevice)
{
	return module_external_rtl8651_fwdEngineGetLinkIDByExtDevice(extDevice);
}
inline int32 rtl8651_fwdEngineSetExtDeviceVlanProperty(uint32 linkID, uint16 vid, uint32 property)
{
	return module_external_rtl8651_fwdEngineSetExtDeviceVlanProperty(linkID, vid, property);
}
inline int32 ether_ntoa_r(ether_addr_t *n, uint8 *a)
{
	return module_external_ether_ntoa_r(n, a);
}
inline void memDump(void *start, uint32 size, int8 * strHeader)
{
	return module_external_memDump(start, size, strHeader);
}

#ifdef CONFIG_RTL865XC
inline int32 swNic_getRingSize(uint32 ringType, uint32 *ringSize, uint32 ringIdx)
{
	return module_external_swNic_getRingSize(ringType, ringSize, ringIdx);
}
#endif

inline int32 mBuf_freeOneMbufPkthdr(struct rtl_mBuf *m, void **buffer, uint32 *id, uint16 *size)
{
	return module_external_mBuf_freeOneMbufPkthdr(m, buffer, id, size);
}
inline int32 mBuf_attachCluster(struct rtl_mBuf *m, void *buffer, uint32 id, uint32 size, uint16 datalen, uint16 align)
{
	return module_external_mBuf_attachCluster(m, buffer, id, size, datalen, align);
}
inline int32 mBuf_leadingSpace(struct rtl_mBuf * m)
{
	return module_external_mBuf_leadingSpace(m);
}
inline int32 rtl8651_fwdEngineExtPortRecv(	void *id,
														uint8 *data,
														uint32 len,
														uint16 myvid,
														uint32 myportmask,
														uint32 linkID)
{
	return module_external_rtl8651_fwdEngineExtPortRecv(id, data, len, myvid, myportmask, linkID);
}

   #if defined(CONFIG_RTL865X_CLE) 
inline int32 cle_cmdParser( int8 *cmdString, cle_exec_t *CmdFmt, int8 *delimiter)
{
	return module_external_cle_cmdParser(cmdString, CmdFmt, delimiter);
}
   #endif




   
#if defined(CONFIG_RTL865X_IPSEC)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
inline int32 rtl8651_registerIpcompCallbackFun(int32 (*compress)(struct rtl_pktHdr *pkthdr,void *iph,uint32 spi, int16 mtu),
											 int32 (*decompress)(struct rtl_pktHdr *pkthdr,void *iph,uint32 spi, int16 mtu))
{
	return module_external_rtl8651_registerIpcompCallbackFun(compress, decompress);
}

inline int32 rtl8651_addIpsecSpi( ipaddr_t edstIp, uint32 spi, uint32 proto,
                           ipaddr_t srcIp, uint32 cryptoType, void* cryptoKey, uint32 authType, void* authKey,
                           ipaddr_t dstIp, int32 lifetime, int32 replayWindow, uint32 nattPort, uint32 flags )
{
	return module_external_rtl8651_addIpsecSpi(edstIp, spi, proto, 
					srcIp, cryptoType, cryptoKey, authType, authKey,
					dstIp, lifetime, replayWindow, nattPort, flags);
}

inline int32 rtl8651_delIpsecSpi( ipaddr_t edstIp, uint32 spi, uint32 proto )
{
	return module_external_rtl8651_delIpsecSpi( edstIp, spi, proto ); 
}


inline int32 rtl8651_addIpsecSpiGrp( ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                              ipaddr_t edstIp2, uint32 spi2, uint32 proto2,
                              ipaddr_t edstIp3, uint32 spi3, uint32 proto3,
                              ipaddr_t edstIp4, uint32 spi4, uint32 proto4,
                              uint32 flags )
{
	return  module_external_rtl8651_addIpsecSpiGrp( edstIp1, spi1, proto1,
                              edstIp2, spi2, proto2,
                              edstIp3, spi3, proto3,
                              edstIp4, spi4, proto4,
                              flags );
}
inline int32 rtl8651_delIpsecSpiGrp( ipaddr_t edstIp1, uint32 spi1, uint32 proto1 )
{
	return module_external_rtl8651_delIpsecSpiGrp( edstIp1, spi1, proto1 );
}


inline int32 rtl8651_delIpsecERoute( ipaddr_t srcIp, ipaddr_t srcIpMask,
                              ipaddr_t dstIp, ipaddr_t dstIpMask )
{
	return  module_external_rtl8651_delIpsecERoute( srcIp, srcIpMask,
                              dstIp, dstIpMask );
}


inline int32 rtl8651_addIpsecERoute( ipaddr_t srcIp, ipaddr_t srcIpMask,
                              ipaddr_t dstIp, ipaddr_t dstIpMask,
                              ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                              uint32 flags )
{
	return module_external_rtl8651_addIpsecERoute( srcIp, srcIpMask,
                              dstIp, dstIpMask,
                              edstIp1, spi1, proto1,
                              flags );
}
#endif


inline struct rtl_mBuf *mBuf_get(int32 how, int32 unused, uint32 Nbuf)
{
	return module_external_mBuf_get(how, unused, Nbuf);
}

inline int32 mBuf_reserve(struct rtl_mBuf * m, uint16 headroom)
{
	return module_external_mBuf_reserve(m, headroom);
}

inline uint32 mBuf_freeMbufChain(register struct rtl_mBuf *m)
{
	return module_external_mBuf_freeMbufChain(m);
}

inline struct rtl_mBuf *mBuf_getPkthdr(struct rtl_mBuf *pMbuf, int32 how)
{
		return module_external_mBuf_getPkthdr(pMbuf,how);
}

#ifdef CONFIG_RTL865XC 
inline int32 swNic_write(void *outPkt, int32 txPkthdrRingIndex)
{
	return module_external_swNic_write(outPkt, txPkthdrRingIndex);
}
inline int32 swNic_isrReclaim(uint32 rxDescIdx,uint32 mbufDescIdx, struct rtl_pktHdr*pPkthdr,struct rtl_mBuf *pMbuf)
{
	return module_external_swNic_isrReclaim(rxDescIdx, mbufDescIdx, pPkthdr, pMbuf);
}

#ifdef NIC_DEBUG_PKTDUMP
inline void swNic_pktdump(	uint32 currentCase,
									struct rtl_pktHdr *pktHdr_p,
									char *pktContent_p,
									uint32 pktLen,
									uint32 additionalInfo)
{
 	module_external_swNic_pktdump(	currentCase, pktHdr_p, pktContent_p, pktLen, additionalInfo);
}
#endif
#else
inline int32 swNic_write(void * output) 
{
	return module_external_swNic_write(output);
}
inline int32 swNic_isrReclaim(uint32 rxDescIdx, struct rtl_pktHdr*pPkthdr,struct rtl_mBuf *pMbuf)
{
	return module_external_swNic_isrReclaim(rxDescIdx, pPkthdr, pMbuf);
}
#endif
#if defined(CONFIG_RTL865X_ROMEPERF)
inline int32 rtl8651_romeperfEnterPoint( uint32 index )
{
	return module_external_rtl8651_romeperfEnterPoint(index);
}
inline int32 rtl8651_romeperfExitPoint( uint32 index )
{
	return module_external_rtl8651_romeperfExitPoint(index);
}
#endif


#if defined(CONFIG_8139CP) || defined(CONFIG_8139TOO)

struct rtl_mBuf *(*module_external_mBuf_attachHeader)(void *buffer, uint32 id, uint32 bufsize,uint32 datalen, uint16 align) = NULL;
int32 (*module_external_rtl8651_fwdEngineExtPortUcastFastRecv)(	struct rtl_pktHdr *pkt,
																	uint16 myvid,
																	uint32 myportmask) = NULL;
int32 (*module_external_rtl8651_fwdEngineAddWlanSTA)(uint8 *smac, uint16 myvid, uint32 myportmask, uint32 linkId) = NULL;																	


EXPORT_SYMBOL(module_external_mBuf_attachHeader);
EXPORT_SYMBOL(module_external_rtl8651_fwdEngineExtPortUcastFastRecv);
EXPORT_SYMBOL(module_external_rtl8651_fwdEngineAddWlanSTA);


inline struct rtl_mBuf *mBuf_attachHeader(void *buffer, uint32 id, uint32 bufsize,uint32 datalen, uint16 align)
{
	return module_external_mBuf_attachHeader(buffer, id, bufsize, datalen, align);
}

inline int32 rtl8651_fwdEngineExtPortUcastFastRecv(	struct rtl_pktHdr *pkt,
																	uint16 myvid,
																	uint32 myportmask)
{
	return module_external_rtl8651_fwdEngineExtPortUcastFastRecv(pkt, myvid, myportmask);
}

inline int32 rtl8651_fwdEngineAddWlanSTA(uint8 *smac, uint16 myvid, uint32 myportmask, uint32 linkId)
{
	return module_external_rtl8651_fwdEngineAddWlanSTA(smac, myvid, myportmask, linkId);
}

#endif


#ifdef CONFIG_RTL8139_FASTPATH
void (*module_external_rtlairgo_fast_tx_register)(void * fn, void * free_fn) = NULL;
void (*module_external_rtlairgo_fast_tx_unregister)(void) = NULL;

EXPORT_SYMBOL(module_external_rtlairgo_fast_tx_register);
EXPORT_SYMBOL(module_external_rtlairgo_fast_tx_unregister);

inline void rtlairgo_fast_tx_register(void * fn, void * free_fn)
{
	module_external_rtlairgo_fast_tx_register(fn, free_fn);
}

inline void rtlairgo_fast_tx_unregister(void)
{
	module_external_rtlairgo_fast_tx_unregister();
}
#endif


#if defined (CONFIG_RTL8185) || defined (CONFIG_RTL8185B) || defined (CONFIG_RTL8185_MODULE)
//glue function for 8185 iwpriv ioctl to call our own CLE engine.

void (*module_external_rtl8651_8185flashCfg)(int8 *cmd, uint32 cmdLen) = NULL;

inline void rtl8651_8185flashCfg(int8 *cmd, uint32 cmdLen){
	module_external_rtl8651_8185flashCfg(cmd, cmdLen);
}
EXPORT_SYMBOL(module_external_rtl8651_8185flashCfg);
#endif



/********************************************************************
	**for the functions which define in kernel and used by RomeDrv **
*********************************************************************/

#if defined(CONFIG_RTL865X_IPSEC)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
extern int32 rtl8651_registerCompDecompFun(void);
extern void ipsec_manualKeyFlagSet(char manual, __u32 addr);

EXPORT_SYMBOL(rtl8651_registerCompDecompFun);
EXPORT_SYMBOL(ipsec_manualKeyFlagSet);
#endif

#if defined(CONFIG_RTL865XB_EXP_CRYPTOENGINE)
extern void AES_cbc_encrypt(const unsigned char *in, unsigned char *out,
	const unsigned long length, const void *key,
	unsigned char *ivec, const int enc);
extern void des_ede3_cbcm_encrypt(const unsigned char *in,unsigned char *out,
			   long length,
			   void *ks1,void *ks2,
			   void *ks3,
			   void *ivec1,void *ivec2,
			   int enc);
extern void des_ecb3_encrypt(const unsigned char *input, unsigned char *output,
		      void *ks1,void *ks2,
		      void *ks3, int enc);
extern void des_ncbc_encrypt(const unsigned char *input,unsigned char *output,
		      long length,void *schedule,void *ivec,
		      int enc);

extern void des_xcbc_encrypt(const unsigned char *input,unsigned char *output,
		      long length,void *schedule,void *ivec,
		      void *inw,void *outw,int enc);
extern void des_ecb_encrypt(void *input,void *output,
		     void *ks,int enc);	
extern int des_set_key(void  *key,void *schedule);
/*extern void lx4180_writeCacheCtrl(int32  value);*/
extern void des_ede3_cbc_encrypt(const unsigned char *input,unsigned char *output, 
			  long length,
			  void *ks1,void  *ks2,
			  void *ks3,void *ivec,int enc);

EXPORT_SYMBOL(AES_cbc_encrypt);
EXPORT_SYMBOL(des_ede3_cbcm_encrypt);
EXPORT_SYMBOL(des_ecb3_encrypt);
EXPORT_SYMBOL(des_ncbc_encrypt);

EXPORT_SYMBOL(des_xcbc_encrypt);
EXPORT_SYMBOL(des_ecb_encrypt);
EXPORT_SYMBOL(des_set_key);
EXPORT_SYMBOL(des_ede3_cbc_encrypt);
#endif

#if defined(CONFIG_RTL865X_CLE)
#if defined(CONFIG_RTL8185) || defined(CONFIG_RTL8185B)
extern int get_eeprom_info(void *priv, unsigned char *data);
EXPORT_SYMBOL(get_eeprom_info);
#endif
#endif
EXPORT_SYMBOL(memchr);


extern asmlinkage long sys_kill(int pid, int sig);
EXPORT_SYMBOL(sys_kill);
EXPORT_SYMBOL(rtlglue_pktToProtocolStackPreprocess);





