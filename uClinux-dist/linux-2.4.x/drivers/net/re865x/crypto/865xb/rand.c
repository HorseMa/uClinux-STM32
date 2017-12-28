/*
* ----------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2002  
* All rights reserved.
* 
* $Header: /cvs/sw/linux-2.4.x/drivers/net/re865x/crypto/865xb/Attic/rand.c,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
*
* Abstract: Pseudo-random number generator source code.
*           The following code combines two Tausworthe Random Number Generators. 
*           It is a reformating and translation of the algorithm in "Efficient and 
*           Portable Combined Tausworthe Random Number Generators" by Shu Tezuka 
*           and Pierre L'Ecuyer in the ACM Transactions on Modeling and Computer 
*           Simulation, Vol. 1, No. 2, April 1991, pages 99-112. It consists of one 
*           initialization function and one function: 
*               void tauset(int32 i1, int32 i2);
*               int32 taurand(int32 ir);
*           The function tauset sets the seed from the two long integer numbers pi1, 
*           pi2. Taurand returns a uniformly distributed integer in the range [0,ir-1]. 
*           Reference http://www.serve.net/buz/Notes.1st.year/HTML/C6/rand.004.html
*
* $Author: davidm $
*
* $Log: rand.c,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.2  2004/06/23 10:15:45  yjlou
* *: convert DOS format to UNIX format
*
* Revision 1.1  2004/06/23 09:18:57  yjlou
* +: support 865xB CRYPTO Engine
*   +: CONFIG_RTL865XB_EXP_CRYPTOENGINE
*   +: basic encry/decry functions (DES/3DES/SHA1/MAC)
*   +: old-fashion API (should be removed in next version)
*   +: batch functions (should be removed in next version)
*
* Revision 1.1  2002/08/20 06:08:59  danwu
* Create.
*
* ---------------------------------------------------------------
*/

#include <rtl_types.h>



/* STATIC VARIABLE DECLARATIONS
 */
static int32   state1, state2;



void tauset (int32 i1, int32 i2)
/* i1 & i2 are the initial seeds, 0,0 if you are lazy */
{
    if ( i1 == 0 )
        state1 = 0x2037B45D;
    else
        state1 = i1 & 0x7fffffff;
    
    if ( i2 == 0 )
        state1 = 0x16EC40DF;
    else
        state2 = i2 & 0x1fffffff;
    
    return;
}


int32 taurand (int32 ir)
/* ir is the integer range [0..ir-1] of taurand */
{
    int32  b, j, s1, s2;

    /* 1st RN: 12 bit circular jumps in a 31 bit field */

    s1 = state1;
    b = ((s1 << 13) ^ s1) & 0x7fffffff;
    s1 = ((s1 << 12) ^ (b >> 19)) & 0x7fffffff;
    state1 = s1;

    /* 2nd RN: 17 bit circular jumps in a 29 bit field */

    s2 = state2;
    b = (( s2 << 2) ^ s2) & 0x1fffffff;
    s2 = ((s2 << 17) ^ (b >> 12)) & 0x1fffffff;
    state2 = s2;

    /* Combine RNs into one */

    j = s1 ^ (s2 << 2);     /* align randoms at 29 bits */

    /* Adjust the range */

    if ( ir > 0 )
        return (j % ir);
    else
        return j;
}
