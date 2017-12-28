/*
* Copyright c                  Realtek Semiconductor Corporation, 2004
* All rights reserved.
* 
* Program : Mass Production Test Program
* Abstract : 
* Author : Edward Jin-Ru Chen (jzchen@realtek.com.tw)
*/
#include "rtl865x/rtl8651_tblDrvLocal.h"
#include "rtl865x/rtl8651_tblDrvFwdLocal.h"
#include "flashdrv.h"

#define RRCP_REG_SIZE	80
static uint32 rtl8651_drvRealtekProtocolRegisters[RRCP_REG_SIZE]; /* The pseudo register used for management control */
static uint32 rtl8651_drvRealtekProtocolPseudoFlash[sizeof(bdinfo_t)/4];//The size should be the same as BDinfo
static uint32 rtl8651_drvRealtekProtocolFlashMirror[sizeof(bdinfo_t)/4];//The size should be the same as BDinfo
static uint32 rtl8651_drvRealtekProtocolGatewayConfigureControlRegister;
static int8	 rtl8651_drvRealtekProtocolFlashMirrored;


void rtl865x_MPTest_setRTKProcotolMirrorFlag(int8 flag) {
	rtl8651_drvRealtekProtocolFlashMirrored = flag;
}

int32  rtl865x_MPTest_Process(struct rtl_pktHdr* pPkt) {
	ether_addr_t vlanGMAC;
	uint16 * uint16Ptr, regAddr;
	uint32 regData;
	rtl8651_tblDrv_vlanTable_t *local_vlanp;
	uint8 *m_data;

	local_vlanp = &DrvTbl.vlan[pPkt->ph_vlanIdx];
	rtl8651_getVlanMacAddress( local_vlanp->vid, &vlanGMAC, NULL );/* Get VLAN Gateway MAC address*/
	m_data = pPkt->ph_mbuf->m_data;
			
	switch(m_data[14]) {
		case 1: /* Realtek Remote Control Protocol */
			switch(m_data[15]) {
				case 0:/* Station to switch Hello */
					uint16Ptr = (uint16 *)m_data;
					if(uint16Ptr[0] == 0xFFFF && uint16Ptr[1] == 0xFFFF && uint16Ptr[2] == 0xFFFF) {
						struct rtl_mBuf *dup_mBuf;
								
						dup_mBuf = mBuf_dupPacket( pPkt->ph_mbuf, MBUF_DONTWAIT );
						/* Reply to the source */
						memcpy(&m_data[0], &m_data[6], 6);
						memcpy(&m_data[6], &vlanGMAC, 6);
						m_data[15] |= 0x80;
						m_data[18] = pPkt->ph_portlist;
						m_data[19] = 0;
						memset(&m_data[20], 0, 12);
						SET_UINT16_LITTLE_ENDIAN_UNALIGNED(0x8650, &m_data[26]);
						pPkt->ph_portlist = 1<<pPkt->ph_portlist;
						if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
							mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
						/* Broadcast to downstream stations*/
						if (dup_mBuf) {
							dup_mBuf->m_pkthdr->ph_portlist = local_vlanp->memberPortUpStatus & ~(1<<dup_mBuf->m_pkthdr->ph_portlist);
							if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)dup_mBuf->m_pkthdr, local_vlanp->vid,-1)!=SUCCESS) 
								mBuf_driverFreeMbufChain(dup_mBuf);
						}
					}
					else if (memcmp(m_data, &vlanGMAC, 6) == 0) {
						memcpy(&m_data[0], &m_data[6], 6);
						memcpy(&m_data[6], &vlanGMAC, 6);
						m_data[15] |= 0x80;
						m_data[18] = pPkt->ph_portlist;
						m_data[19] = 0;
						memset(&m_data[20], 0, 12);
						SET_UINT16_LITTLE_ENDIAN_UNALIGNED(0x8650, &m_data[26]);
						pPkt->ph_portlist = 1<<pPkt->ph_portlist;
						if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
							mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					} else {
						pPkt->ph_portlist = rtl8651_asicL2DAlookup(m_data) & ~(1<<pPkt->ph_portlist);
						if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
							mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					}
					break;
				case 1: /* Station to switch Get */
					if (memcmp(m_data, &vlanGMAC, 6) == 0) {
						memcpy(&m_data[0], &m_data[6], 6);
						memcpy(&m_data[6], &vlanGMAC, 6);
						m_data[15] |= 0x80;
						regAddr = GET_UINT16_LITTLE_ENDIAN_UNALIGNED(&m_data[18]);
						switch((regAddr>>12)&0xF) {
							case 0x0:
							if(regAddr < RRCP_REG_SIZE) 
								SET_UINT32_LITTLE_ENDIAN_UNALIGNED(rtl8651_drvRealtekProtocolRegisters[regAddr], &m_data[20]);
							 else
							 	SET_UINT32_LITTLE_ENDIAN_UNALIGNED(0xFFFFFFFF, &m_data[20]);
							 break;
							 case 0x1:
								switch((regAddr>>10)&0x3) {
									case 0:
										if((regAddr&0x3FF)<sizeof(bdinfo_t)/4) {
											if(rtl8651_drvRealtekProtocolFlashMirrored==0) {
												uint32 i;
												flashdrv_read((void *)rtl8651_drvRealtekProtocolFlashMirror, (void *)flashdrv_getBoardInfoAddr(), sizeof(bdinfo_t));
												rtlglue_printf("\nRead from flash address %x\n", flashdrv_getBoardInfoAddr());
												for(i=0; i<sizeof(bdinfo_t)/4; i++) {
													rtlglue_printf("%08x ", rtl8651_drvRealtekProtocolFlashMirror[i]);
													if(i%4==3 || i==(sizeof(bdinfo_t)/4-1)) rtlglue_printf("\n");
												}
												rtl8651_drvRealtekProtocolFlashMirrored = 1;
											}
											SET_UINT32_LITTLE_ENDIAN_UNALIGNED(rtl8651_drvRealtekProtocolFlashMirror[regAddr&0x3FF], &m_data[20]);
										}
										else
											SET_UINT32_LITTLE_ENDIAN_UNALIGNED(0xFFFFFFFF, &m_data[20]);
									break;
									case 1:
										if((regAddr&0x3FF)<sizeof(bdinfo_t)/4)
											SET_UINT32_LITTLE_ENDIAN_UNALIGNED(rtl8651_drvRealtekProtocolPseudoFlash[regAddr&0x3FF], &m_data[20]);
										else
											SET_UINT32_LITTLE_ENDIAN_UNALIGNED(0xFFFFFFFF, &m_data[20]);
									break;
									case 2:
										if((regAddr&0x3FF)==0) 
											SET_UINT32_LITTLE_ENDIAN_UNALIGNED(rtl8651_drvRealtekProtocolGatewayConfigureControlRegister, &m_data[20]);
										else
										 	SET_UINT32_LITTLE_ENDIAN_UNALIGNED(0xFFFFFFFF, &m_data[20]);
									break;
									default:
									 	SET_UINT32_LITTLE_ENDIAN_UNALIGNED(0xFFFFFFFF, &m_data[20]);
									break;
								}
							break;
							default:
							 	SET_UINT32_LITTLE_ENDIAN_UNALIGNED(0xFFFFFFFF, &m_data[20]);
							break;
						}
						pPkt->ph_portlist = 1<<pPkt->ph_portlist;
						if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
							mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					} else {
						pPkt->ph_portlist = rtl8651_asicL2DAlookup(m_data) & ~(1<<pPkt->ph_portlist);
						if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
							mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					}
					break;
				case 2: /* Station to switch Set */
					if (memcmp(m_data, &vlanGMAC, 6) == 0) {
						regAddr = GET_UINT16_LITTLE_ENDIAN_UNALIGNED(&m_data[18]);
						regData = GET_UINT32_LITTLE_ENDIAN_UNALIGNED(&m_data[20]);
						switch((regAddr>>12)&0xF) {
							case 0x0:
							if(regAddr < RRCP_REG_SIZE) {
								/* regAddr value changed from 0 to 1 means starting testing item, value meaning:
									0: Reset, 1: (0->1) means start self diagnostic, 2: on-going, 3: Finished and Success, 4: Finished and Failed */
								if(rtl8651_drvRealtekProtocolRegisters[regAddr] == 0 && regData == 1) {
									rtl8651_drvRealtekProtocolRegisters[regAddr] = 3;
								}
								else 
									rtl8651_drvRealtekProtocolRegisters[regAddr] = regData;
							}
							break;
							case 0x1:
								switch((regAddr>>10)&0x3) {
									case 0://Do nothing since the flash cannot set flash data directly
									break;
									case 1:
										if((regAddr&0x3FF)<sizeof(bdinfo_t)/4)
											rtl8651_drvRealtekProtocolPseudoFlash[regAddr&0x3FF] = regData;
									break;
									case 2:
										if((regAddr&0x3FF)==0) {
											if(rtl8651_drvRealtekProtocolGatewayConfigureControlRegister == 0 && regData == 1) {
												flashdrv_read((void *)rtl8651_drvRealtekProtocolPseudoFlash, (void *)flashdrv_getBoardInfoAddr(), sizeof(bdinfo_t));
												rtl8651_drvRealtekProtocolGatewayConfigureControlRegister = 3;
											}
											else if (rtl8651_drvRealtekProtocolGatewayConfigureControlRegister == 5 && regData == 6) {
												uint32 i;
												flashdrv_updateImg((void *)rtl8651_drvRealtekProtocolPseudoFlash, (void *)flashdrv_getBoardInfoAddr(), sizeof(bdinfo_t));
												rtl8651_drvRealtekProtocolGatewayConfigureControlRegister = 8;
												rtlglue_printf("\nConfigure to flash address %x\n", flashdrv_getBoardInfoAddr());
												for(i=0; i<sizeof(bdinfo_t)/4; i++) {
													rtlglue_printf("%08x ", rtl8651_drvRealtekProtocolPseudoFlash[i]);
													if(i%4==3 || i==(sizeof(bdinfo_t)/4-1)) rtlglue_printf("\n");
												}
												flashdrv_read((void *)rtl8651_drvRealtekProtocolFlashMirror, (void *)flashdrv_getBoardInfoAddr(), sizeof(bdinfo_t));
												rtl8651_drvRealtekProtocolFlashMirrored = 1;
												rtlglue_printf("\nRead from flash address %x\n", flashdrv_getBoardInfoAddr());
												for(i=0; i<sizeof(bdinfo_t)/4; i++) {
													rtlglue_printf("%08x ", rtl8651_drvRealtekProtocolFlashMirror[i]);
													if(i%4==3 || i==(sizeof(bdinfo_t)/4-1)) rtlglue_printf("\n");
												}
											}
											else
												rtl8651_drvRealtekProtocolGatewayConfigureControlRegister = regData;
										}
									break;
								}
							break;
						}

						mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					} else {
						pPkt->ph_portlist = rtl8651_asicL2DAlookup(m_data) & ~(1<<pPkt->ph_portlist);
						if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
							mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					}
					break;
				case 0x80: /* Switch to station Hello reply */
					if(m_data[19] == 0) {
						m_data[19] = pPkt->ph_portlist;
						memcpy(&m_data[20], &vlanGMAC, 6);
					}
					pPkt->ph_portlist = rtl8651_asicL2DAlookup(m_data) & ~(1<<pPkt->ph_portlist);
					if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
						mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					break;
				case 0x81: /* Switch to station Get reply */
					pPkt->ph_portlist = rtl8651_asicL2DAlookup(m_data) & ~(1<<pPkt->ph_portlist);
					if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
						mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					break;
				default:
					mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
					break;
			}
			break;
		case 2: /* Realtek Echo Protocol */
			uint16Ptr = (uint16 *)m_data;
			if(uint16Ptr[0] == 0xFFFF && uint16Ptr[1] == 0xFFFF && uint16Ptr[2] == 0xFFFF) {
				memcpy(&m_data[0], &m_data[6], 6);
				memcpy(&m_data[6], &vlanGMAC, 6);
				pPkt->ph_portlist = 1<<pPkt->ph_portlist;
				if(_rtl8651_fwdEngineSend(FWDENG_L2PROC|FWDENG_FROMDRV, (void*)pPkt, local_vlanp->vid,-1)!=SUCCESS) 
					mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
			}
			else
				mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
			break;
		default:
			mBuf_driverFreeMbufChain(pPkt->ph_mbuf);
			break;
	}
	return SUCCESS;
}

