#ifndef	_RTL_MODULE
#define	_RTL_MODULE

#if defined(CONFIG_RTL865X_IPSEC)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
extern int32(*module_external_rtl8651_registerIpcompCallbackFun)(int32 (*compress)(struct rtl_pktHdr *pkthdr,void *iph,uint32 spi, int16 mtu),
											 int32 (*decompress)(struct rtl_pktHdr *pkthdr,void *iph,uint32 spi, int16 mtu));
extern int32(*module_external_rtl8651_addIpsecSpi)( ipaddr_t edstIp, uint32 spi, uint32 proto,
                           ipaddr_t srcIp, uint32 cryptoType, void* cryptoKey, uint32 authType, void* authKey,
                           ipaddr_t dstIp, int32 lifetime, int32 replayWindow, uint32 nattPort, uint32 flags );
extern int32(*module_external_rtl8651_delIpsecSpi)( ipaddr_t edstIp, uint32 spi, uint32 proto );
extern int32(*module_external_rtl8651_addIpsecSpiGrp)( ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                              ipaddr_t edstIp2, uint32 spi2, uint32 proto2,
                              ipaddr_t edstIp3, uint32 spi3, uint32 proto3,
                              ipaddr_t edstIp4, uint32 spi4, uint32 proto4,
                              uint32 flags );
extern int32(*module_external_rtl8651_delIpsecSpiGrp)( ipaddr_t edstIp1, uint32 spi1, uint32 proto1 );
extern int32(*module_external_rtl8651_delIpsecERoute)( ipaddr_t srcIp, ipaddr_t srcIpMask,
                              ipaddr_t dstIp, ipaddr_t dstIpMask );
extern int32(*module_external_rtl8651_addIpsecERoute)( ipaddr_t srcIp, ipaddr_t srcIpMask,
                              ipaddr_t dstIp, ipaddr_t dstIpMask,
                              ipaddr_t edstIp1, uint32 spi1, uint32 proto1,
                              uint32 flags );	


extern struct rtl_mBuf *(*module_external_mBuf_get)(int32 how, int32 unused, uint32 Nbuf);
extern int32 (*module_external_mBuf_reserve)(struct rtl_mBuf * m, uint16 headroom);
#endif


#ifdef CONFIG_RTL865XC 
		extern int32 (*module_external_swNic_write)(void * output, int txPkthdrRingIndex); 
		extern int32 (*module_external_swNic_isrReclaim)(uint32 rxDescIdx,uint32 mbufDescIdx, struct rtl_pktHdr*pPkthdr,struct rtl_mBuf *pMbuf);
#ifdef NIC_DEBUG_PKTDUMP
		extern void (*module_external_swNic_pktdump)(	uint32 currentCase,
									struct rtl_pktHdr *pktHdr_p,
									char *pktContent_p,
									uint32 pktLen,
									uint32 additionalInfo);
#endif									
		
#else
		extern int32 (*module_external_swNic_write)(void * output); 
		extern int32 (*module_external_swNic_isrReclaim)(uint32 rxDescIdx, struct rtl_pktHdr*pPkthdr,struct rtl_mBuf *pMbuf);
#endif
extern struct rtl_mBuf *(*module_external_mBuf_getPkthdr)(struct rtl_mBuf *pMbuf, int32 how);

		extern int32 (*module_external_data_rtl8651_totalExtPortNum); //this replaces all RTL8651_EXTPORT_NUMBER defines
		extern uint32 (*module_external_mBuf_freeMbufChain)(register struct rtl_mBuf * m);
		extern int32 (*module_external_rtl8651_fwdEngineRegExtDevice)(	uint32 portNumber,
											uint16 defaultVID,
											uint32 *linkID_p,
											void *extDevice);
		extern int32 (*module_external_rtl8651_fwdEngineUnregExtDevice)(uint32 linkID);	
		extern int32 (*module_external_rtl8651_fwdEngineGetLinkIDByExtDevice)(void *extDevice);

		extern int32 (*module_external_rtl8651_fwdEngineSetExtDeviceVlanProperty)(uint32 linkID, uint16 vid, uint32 property) ;
		extern int32 (*module_external_ether_ntoa_r)(ether_addr_t *n, uint8 *a) ;
		extern void (*module_external_memDump)(void *start, uint32 size, int8 * strHeader);
#ifdef CONFIG_RTL865XB
		extern uint32 *module_external_data_totalRxPkthdr;
		extern uint32 *module_external_data_totalTxPkthdr;
#endif
#ifdef CONFIG_RTL865XC
		extern int32 (*module_external_swNic_getRingSize)(uint32 ringType, uint32 *ringSize, uint32 ringIdx);
#endif
		extern int32 (*module_external_mBuf_freeOneMbufPkthdr)(struct rtl_mBuf *m, void **buffer, uint32 *id, uint16 *size);
		extern int32 (*module_external_mBuf_attachCluster)(struct rtl_mBuf *m, void *buffer, uint32 id, uint32 size, uint16 datalen, uint16 align);
		extern int32 (*module_external_mBuf_leadingSpace)(struct rtl_mBuf * m);
	
		extern int32 (*module_external_rtl8651_fwdEngineExtPortRecv)(	void *id,
														uint8 *data,
														uint32 len,
														uint16 myvid,
														uint32 myportmask,
														uint32 linkID);

    #ifdef SWNIC_DEBUG 
		extern int32 *module_external_data_nicDbgMesg;
    #endif														

    #if	defined(CONFIG_RTL865X_CLE)
/*    		extern cle_exec_t *module_external_data__initCmdList;	*/
/*    		extern int	module_external_data__initCmdTotal;			*/
    		extern int32 (*module_external_cle_cmdParser)( int8 *cmdString, cle_exec_t *CmdFmt, int8 *delimiter);
   #endif

#ifdef CONFIG_RTL865X_ROMEPERF
	extern int32 (*module_external_rtl8651_romeperfEnterPoint)( uint32 index );
	extern int32 (*module_external_rtl8651_romeperfExitPoint)( uint32 index );
#endif

#if defined(CONFIG_8139CP) || defined(CONFIG_8139TOO)
extern struct rtl_mBuf *(*module_external_mBuf_attachHeader)(void *buffer, uint32 id, uint32 bufsize,uint32 datalen, uint16 align);
extern int32 (*module_external_rtl8651_fwdEngineExtPortUcastFastRecv)(	struct rtl_pktHdr *pkt,
																	uint16 myvid,
																	uint32 myportmask);
extern int32 (*module_external_rtl8651_fwdEngineAddWlanSTA)(uint8 *smac, uint16 myvid, uint32 myportmask, uint32 linkId);																	
#endif   

#ifdef AIRGO_FAST_PATH
extern void (*module_external_rtlairgo_fast_tx_register)(void * fn, void * free_fn);
extern void (*module_external_rtlairgo_fast_tx_unregister)(void);
#endif

#if defined (CONFIG_RTL8185) || defined (CONFIG_RTL8185B) || defined (CONFIG_RTL8185_MODULE)
extern void (*module_external_rtl8651_8185flashCfg)(int8 *cmd, uint32 cmdLen);
#endif


#endif
