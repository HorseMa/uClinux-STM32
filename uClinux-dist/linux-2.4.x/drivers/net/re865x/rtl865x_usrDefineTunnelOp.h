/* =============================================================

	Header file of user tunnel 

   ============================================================= */
#ifndef RTL865X_USRDEFINETUNNELOP_H
#define RTL865X_USRDEFINETUNNELOP_H

#include <linux/skbuff.h>

#define RTL865X_USRTUNNELOPERATION_DEFAULTNUM 8
#define RTL865X_USRTUNNELOPERATION_DIR_INBOUND 0x1
#define RTL865X_USRTUNNELOPERATION_DIR_OUTBOUND 0x2

#define RTL865X_USRTUNNELOPNAME_MAXLEN 	32
#define RTL865X_USRTUNNEL_DEFAULT_VLAN 8
#define RTL865X_USRTUNNEL_DEFAULT_EXTPORT 8
#define RTL865X_USRTUNNEL_ACTUAL_WAN_VLAN 10

typedef struct usrTunnelOp
{
	uint32 direction;	/*decide that we should call this callBackFun at the direction */
	uint32 valid; /*valid?*/

	/*callBackFun name*/
	char funName[RTL865X_USRTUNNELOPNAME_MAXLEN];

	/*callBack function*/
	int32 (*callBackFun)(struct sk_buff *skb);
}rtl865x_usrTunnelOp;

int32 rtl865x_register_Op(rtl865x_usrTunnelOp *usrDefineTunnelOp);
int32 rtl865x_remove_Op(rtl865x_usrTunnelOp *usrDefineTunnelOp);
int32 rtl865x_usrDefineTunnel_init(void);
#endif
