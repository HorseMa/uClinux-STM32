/*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program : 	rtl8651_callBack.c
* Abstract : 
		user-defined callBack functions
* Creator : 
* Author :
*/

#include "rtl865x/rtl_types.h"
#include "rtl865x/assert.h"
#include "rtl865x/types.h"
#include "rtl865x/rtl_errno.h"
#include "rtl865x/rtl_glue.h"
#include "rtl865x/rtl8651_tblDrv.h"
#include "rtl865x_callBack.h"
#include "rtl865x/mbuf.h"
#include "rtl865x/rtl8651_tblDrvFwd.h"

/* =====================================================================================
	Resource files
   ===================================================================================== */
static uint16 _rtl865x_callBack_ipChecksum(struct ip * pip)
{


	uint32 sum=0, oddbyte=0;
	uint16 *ptr = (uint16 *)pip;
	uint32 nbytes = ((pip->ip_vhl & 0xf) << 2);	

	while (nbytes > 1)
	{
		
		sum += (*ptr++);
		nbytes -= 2;
	}
	if (nbytes == 1)
	{
		oddbyte = (*ptr & 0xff00);
		sum += oddbyte;
	}
	
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	
	return (~sum);


}

static uint16 _rtl865x_callBack_tcpChecksum(struct ip *pip)
{
	int32 sum, nbytes, nhdr, i;
	uint16 *sip, *dip, *ptr;

	nhdr =  ((pip->ip_vhl & 0xf) << 2);
	nbytes = ntohs(pip->ip_len) - nhdr;
	ptr = (uint16 *) ((char *) pip + nhdr);
	sum = 0;

	/* Note: We always padding zero to the tail of the packet!! */
	*(((uint8 *)pip) + nhdr + nbytes) = (uint8)0;
	for (i=0; i<nbytes ;i=i+2){
		sum += (unsigned long)*ptr++;
	}

	/* "Pseudo-header" data */
	dip=(uint16 *)&pip->ip_dst;
	sum += *dip; dip++;
	sum += *dip;
	sip=(uint16 *)&pip->ip_src;
	sum += *sip; sip++;
	sum += *sip;	

	sum +=  nbytes;
	sum += ((uint16) pip->ip_p);

	/* Roll over carry bits */
	while (sum>>16)
		sum = (sum & 0xFFFF)+(sum >> 16);
	
	/* Take the one's complement of sum */
	sum = ~sum;
	return (uint16)sum;
	
}


static void _rtl865x_callBack_replyTcpUdpPktGen(struct rtl_pktHdr *pkt, struct ip *ipHdr, uint8 *replyMsg, uint32 replyLen, uint8 extraTcpFlag)
{
	/* Incoming pkt */
	struct rtl_mBuf *mbuf = NULL;
	char *pktData = NULL;
	uint32 pktLen = 0;

	/* Reply pkt */
	struct rtl_pktHdr *replyPkt = NULL;
	struct rtl_mBuf *replyMbuf = NULL;
	char *replyData = NULL;
	struct ip *replyIp = NULL;

	assert(pkt && pkt->ph_mbuf && pkt->ph_mbuf->m_data && ipHdr);

	mbuf = pkt->ph_mbuf;
	pktData = mbuf->m_data;

	RTL865X_CALLBACK_INFO("=== Generate/send Reply packet ===");

	/* get pkt for reply */
	/* ---------------------------------------------------------------------------------------------------------------- */
	if ((replyMbuf = mBuf_get(MBUF_DONTWAIT, MBUFTYPE_DATA, 1)) == NULL)
	{
		goto out;
	}
	if (	(mBuf_getPkthdr(replyMbuf, MBUF_DONTWAIT) == NULL)
#if defined(CONFIG_RTL865X_MBUF_HEADROOM) && defined(CONFIG_RTL865X_MULTILAYER_BSP)
		|| (mBuf_reserve(replyMbuf, CONFIG_RTL865X_MBUF_HEADROOM) != SUCCESS)
#endif
	)
	{
		goto free_out;
	}

	replyPkt = replyMbuf->m_pkthdr;
	replyData = replyMbuf->m_data;
	/* ---------------------------------------------------------------------------------------------------------------- */

	#define CONTENT_ADD_OK(mbuf, size)		((((uint32)((mbuf)->m_data) - (uint32)((mbuf)->m_extbuf)) + (mbuf)->m_len + (size)) < (mbuf)->m_extsize)
	#define CONTENT_CHECK_OK(mbuf, len)	((mbuf)->m_len >= len)

	/* construct L2 information */
	/* ---------------------------------------------------------------------------------------------------------------- */
	{
		uint32 l2Len;

		l2Len = (uint32)ipHdr - (uint32)pktData;
		pktLen += l2Len;

		if (! CONTENT_ADD_OK(replyMbuf, l2Len))
		{
			goto free_out;
		}

		if (! CONTENT_CHECK_OK(mbuf, l2Len))
		{
			goto free_out;
		}

		RTL865X_CALLBACK_INFO("\tGenerate L2 Information");

		/* simply copy packet from original packet to reply packet */
		mBuf_copyToMbuf(replyMbuf, 0, l2Len, pktData);

		/* check DMAC */
		if (pktData[0] & 0x01)	/* DMAC of original packet is broadcast MAC */
		{
			RTL865X_CALLBACK_WARN("Reply packet with L2-multicast MAC address.");
			RTL865X_CALLBACK_WARN("\tWe ignore it in rtl865x_callBack module instead of check the ARP entry of DIP.");
			RTL865X_CALLBACK_WARN("\tIt's simple and correct for current design, due to only L34 packet process use call-back module.");
			RTL865X_CALLBACK_WARN("\tBut it might be wrong if this assumption is not correct anymore in the future.");
			RTL865X_CALLBACK_WARN("\tTherefore, we MIGHT need to modify code here if the assumption changes.\n");
			goto free_out;
		}

		/* exchange SMAC & DMAC */
		memcpy(replyData, &(pktData[sizeof(ether_addr_t)]), sizeof(ether_addr_t));	/* DMAC must be the SMAC of original packet */
		memcpy(&(replyData[sizeof(ether_addr_t)]), pktData, sizeof(ether_addr_t));	/* SMAC must be the DMAC of original packet */

		/* sync pkthdr's length */
		replyPkt->ph_len = replyMbuf->m_len;
	}

	/* construct IP information */
	/* ---------------------------------------------------------------------------------------------------------------- */
	{
		static uint16 replyIp_id = 0x00;
		ipaddr_t tmpIp;

		replyIp = (struct ip *)&(replyData[pktLen]);
		pktLen += IPHDR_LEN(ipHdr);

		if (! CONTENT_ADD_OK(replyMbuf, sizeof(struct ip) /* we assume IP header would be 20 bytes */))
		{
			goto free_out;
		}

		if (! CONTENT_CHECK_OK(mbuf, pktLen))
		{
			goto free_out;
		}

		if (	(ipHdr->ip_p != IPPROTO_TCP) &&
			(ipHdr->ip_p != IPPROTO_UDP))
		{
			goto free_out;
		}

		if (IS_FRAGMENT_REMAIN(ipHdr->ip_off))
		{
			goto free_out;
		}

		RTL865X_CALLBACK_INFO("\tGenerate L3 Information");

		/* construct IP header by ourself */
		replyIp->ip_vhl = 0x45;
		replyIp->ip_id = (replyIp_id ++);
		replyIp->ip_off = htons(IP_DF);
		replyIp->ip_ttl = 0xff;
		replyIp->ip_p = ipHdr->ip_p;
		replyIp->ip_sum = 0;
		tmpIp = PKTGET_UINT32_UNALIGNED(&(ipHdr->ip_dst));
		PKTSET_UINT32_UNALIGNED(tmpIp, &(replyIp->ip_src));
		tmpIp = PKTGET_UINT32_UNALIGNED(&(ipHdr->ip_src));
		PKTSET_UINT32_UNALIGNED(tmpIp, &(replyIp->ip_dst));

		switch (ipHdr->ip_p)
		{
			case IPPROTO_TCP:
				replyIp->ip_len = htons(sizeof(struct ip) + sizeof(struct tcphdr) /* Normal TCP header (without options) */ + replyLen);
				break;
			case IPPROTO_UDP:
				replyIp->ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) /* Normal UDP header (without options) */ + replyLen);
				break;
		}

		replyMbuf->m_len += sizeof(struct ip);
		replyPkt->ph_len += sizeof(struct ip);
	}
	/* ---------------------------------------------------------------------------------------------------------------- */

	/* construct L4 information */
	/* ---------------------------------------------------------------------------------------------------------------- */
	switch (ipHdr->ip_p)
	{
		case IPPROTO_TCP:
		{
			struct tcphdr *replyTcp = (struct tcphdr*)L4HDR_ADDR(replyIp);
			struct tcphdr *tcpHdr = (struct tcphdr*)L4HDR_ADDR(ipHdr);

			if (!CONTENT_ADD_OK(replyMbuf, sizeof(struct tcphdr) + replyLen))
			{
				goto free_out;
			}

			if (!CONTENT_CHECK_OK(mbuf, sizeof(struct tcphdr)))
			{
				goto free_out;
			}

			RTL865X_CALLBACK_INFO("\tGenerate L4 Information");

			replyTcp->th_sport = tcpHdr->th_dport;
			replyTcp->th_dport = tcpHdr->th_sport;
			replyTcp->th_off_x = (uint8)((((sizeof(struct tcphdr)) >> 2) & 0x0f) << 4);

			RTL865X_CALLBACK_INFO("\t\tTCP Flag: 0x%x\n", (tcpHdr->th_flags | TH_ACK | extraTcpFlag));
			replyTcp->th_flags = (tcpHdr->th_flags | TH_ACK | extraTcpFlag);
			replyTcp->th_win = htons(0x1000);	/* set to a small size */
			replyTcp->th_urp = 0;
			replyTcp->th_sum = 0;

			/* try to get the ACK/SEQ of this reply */
			{
				uint32 orgSeq = PKTGET_UINT32_UNALIGNED(&(tcpHdr->th_seq));
				uint32 orgAck = PKTGET_UINT32_UNALIGNED(&(tcpHdr->th_ack));
				uint32 replySeq, replyAck;

				replyAck = orgSeq + (ntohs(ipHdr->ip_len) - (IPHDR_LEN(ipHdr)) - ((tcpHdr->th_off_x & 0xf0) >> 2 /* TCPHDR Length*/));

				PKTSET_UINT32_UNALIGNED(replyAck, &(replyTcp->th_ack));

				replySeq = orgAck;

				PKTSET_UINT32_UNALIGNED(replySeq, &(replyTcp->th_seq));
			}

			/* update pkt len */
			replyMbuf->m_len += sizeof(struct tcphdr);
			replyPkt->ph_len = replyMbuf->m_len;

			if (replyLen && replyMsg)
			{
				mBuf_copyToMbuf(replyMbuf, replyMbuf->m_len, replyLen, replyMsg);

				/* sync pkthdr's length */
				replyPkt->ph_len = replyMbuf->m_len;
			}

			/* L4 checksum */
			replyTcp->th_sum = htons(_rtl865x_callBack_tcpChecksum(replyIp));
		}
			break;
		case IPPROTO_UDP:
		{
			struct udphdr *replyUdp = (struct udphdr*)L4HDR_ADDR(replyIp);
			struct udphdr *udpHdr = (struct udphdr*)L4HDR_ADDR(ipHdr);

			if (!CONTENT_ADD_OK(replyMbuf, sizeof(struct udphdr) + replyLen))
			{
				goto free_out;
			}

			if (!CONTENT_CHECK_OK(mbuf, sizeof(struct udphdr)))
			{
				goto free_out;
			}

			replyUdp->uh_sport = udpHdr->uh_dport;
			replyUdp->uh_dport = udpHdr->uh_sport;
			replyUdp->uh_ulen = htons(sizeof(struct udphdr) + replyLen);
			replyUdp->uh_sum = 0;

			/* update pkt len */
			replyMbuf->m_len += sizeof(struct udphdr);
			replyPkt->ph_len = replyMbuf->m_len;

			if (replyLen && replyMsg)
			{
				mBuf_copyToMbuf(replyMbuf, replyMbuf->m_len, replyLen, replyMsg);

				/* sync pkthdr's length */
				replyPkt->ph_len = replyMbuf->m_len;
			}

			/* L4 checksum */
			replyUdp->uh_sum = htons(_rtl865x_callBack_tcpChecksum(replyIp));
		}
			break;
	}

	/* IP checksum */
	replyIp->ip_sum = htons(_rtl865x_callBack_ipChecksum(replyIp));

	/* ---------------------------------------------------------------------------------------------------------------- */
	replyPkt->ph_pppeTagged = FALSE;
	replyPkt->ph_flags &= ~PKTHDR_PPPOE_AUTOADD;
	replyPkt->ph_proto = PKTHDR_ETHERNET;
	replyPkt->ph_flags &= ~(CSUM_IP|CSUM_L4);


	RTL865X_CALLBACK_INFO("\t==> Send packet to VLAN (%d)", reCore_getDEVIdByVlanIdx(pkt->ph_vlanIdx));

	if (rtl8651_fwdEngineSend((void*)replyPkt, reCore_getDEVIdByVlanIdx(pkt->ph_vlanIdx), -1) != SUCCESS)
	{
		RTL865X_CALLBACK_ERR("Send pkt FAILED !");
	}

	RTL865X_CALLBACK_INFO("==================");

	goto out;

free_out:

	RTL865X_CALLBACK_WARN("Exception occurs in packet generation / forwarding, stop packet reply.");

	assert(replyMbuf);
	mBuf_freeMbufChain(replyMbuf);
out:
	return;
}

/* =====================================================================================
	URL Filter call-back functions
   ===================================================================================== */
#define RTL865X_CALLBACK_URLFILTER_FUNCs
/*
	redirect the HTTP request to gateway's 'BLOCK PAGE' if its requested URI is been blocked.
*/

void rtl865x_callBack_urlFilterRedirect (
	uint32 sessionId,
	struct rtl_pktHdr *pkt,
	struct ip *iphdr,
	const char *urlFiltered,
	const char *pathFiltered)
{
	char retmsg[1024] = {0};
	char httpmsg[512] = {0};

	/* generate packet */
	sprintf(	httpmsg,
			"<html><head><META HTTP-EQUIV=\"Cache-Control\" CONTENT=\"no-cache\"><title>Gateway: HTTP Request is blocked</title></head><body bgColor=#ddeeff><center><BR><BR><font size=22 color=\"#8000ff\" face=\"Modern\"><STRONG>Access Denied</STRONG></font><BR><BR>Dear Sir: The URI <font style=\"color:blue\" bgColor=#eeffdd><B>%s%s</B></font> is <font style=\"color:red\"><B>BLOCKED</B></font> by gateway.<BR>Please contact your Network Administrator to solve this problem.</center></body></html>",
			urlFiltered,
			pathFiltered);

	sprintf(	retmsg,
			"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s\r\n",
			strlen(httpmsg),
			httpmsg
		);

	/* reply packet back to sender */
	_rtl865x_callBack_replyTcpUdpPktGen((struct rtl_pktHdr*)pkt, iphdr, retmsg, strlen(retmsg), TH_FIN);
	_rtl865x_callBack_replyTcpUdpPktGen((struct rtl_pktHdr*)pkt, iphdr, NULL, 0, TH_RST);

	return;
}

/* =====================================================================================
	main functions
   ===================================================================================== */
#define RTL865X_CALLBACK_MAIN_FUNCs

static _rtl865x_callBack_registrationInfo_t callBacks[] = {
	/* =========================== Add Registration API / callBack functions here =========================== */
	{ rtl8651_registerURLFilterCallBackFunction,	rtl865x_callBack_urlFilterRedirect,	"URL-FILTER"},
	/* ==================================================================================== */
	{ NULL,										NULL,								""}
};

static int32 _rtl865x_callBack_registration (void)
{
	int32 index = 0;

	while ( callBacks[index].registerFunc != NULL )
	{
		RTL865X_CALLBACK_INFO("Call back function : ( %s )", callBacks[index].functionName);

		RTL865X_CALLBACK_FUNCCALL(	((_dummyFunc)(callBacks[index].registerFunc))( callBacks[index].callBackFunc ),
										SUCCESS,
										FAILED);
		index ++;
	}


	return SUCCESS;
}

/* init function of callBack module */
int32 rtl865x_callBack_init (void)
{
	RTL865X_CALLBACK_FUNCCALL(_rtl865x_callBack_registration(), SUCCESS, FAILED);

	return SUCCESS;
}


