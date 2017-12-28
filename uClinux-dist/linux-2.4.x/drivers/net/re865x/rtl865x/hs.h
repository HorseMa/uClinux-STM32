/*
* ----------------------------------------------------------------
* Copyright c              Realtek Semiconductor Corporation, 2002
* All rights reserved.
* 
* $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/rtl865x/Attic/hs.h,v 1.1.2.1 2007/09/28 14:42:31 davidm Exp $
*
* Abstract: header stamp module header file.
*
* $Author: davidm $
*
* $Log: hs.h,v $
* Revision 1.1.2.1  2007/09/28 14:42:31  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.4  2004/05/03 14:38:38  cfliu
* no message
*
* Revision 1.3  2004/04/08 13:18:12  tony
* add PPTP/L2TP routine for MII lookback port.
*
* Revision 1.2  2004/03/26 07:14:02  danwu
* *** empty log message ***
*
* Revision 1.1  2004/03/03 10:40:38  yjlou
* *: commit for mergence the difference in rtl86xx_tbl/ since 2004/02/26.
*
* Revision 1.1  2004/02/26 04:27:57  cfliu
* move hs.* into table driver directory
*
* Revision 1.2  2004/02/24 04:04:28  cfliu
* no message
*
* Revision 1.1  2003/10/24 13:29:50  orlando
* poring hs.h for debug command "dhs"
*
* Revision 1.2  2002/09/25 13:35:18  waynelee
* add hs_dumpHsb(), hs_dumpHsa(), hs_dumpHsb_S() and hs_dumpHsa_S(),
* and change the arguments of hs_readHsb() and hs_readHsa()
*
* Revision 1.1  2002/08/28 03:41:03  orlando
* Header stamp module header file.
*
* ---------------------------------------------------------------
*/

//#include <csp/rtl8650/asic/hsRegs.h>
#include "hsRegs.h"

#ifndef _HS_H_
#define _HS_H_

uint32 hsb_aDport(void);
uint32 hsb_dport(int32);

void hs_readHsb(hsb_param_watch_t *hsbWatch, hsb_param_t *hsbParam);
void hs_displayHsb(hsb_param_watch_t * hsbWatch);
void hs_displayHsb_S(hsb_param_watch_t * hsbWatch);
void hs_dumpHsb(uint32 flag);
void hs_readHsa(hsa_param_watch_t *hsaWatch, hsa_param_t *hsaParam);
void hs_displayHsa(hsa_param_watch_t * hsaWatch);
void hs_displayHsa_S(hsa_param_watch_t *hsaWatch);
void hs_dumpHsa(uint32 flag);

#endif /* _HS_H_ */
