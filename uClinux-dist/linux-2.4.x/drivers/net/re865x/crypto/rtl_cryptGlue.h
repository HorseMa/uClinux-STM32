#ifndef _RTLCRYPTGLUE_H
#define _RTLCRYPTGLUE_H

#ifdef __KERNEL__
#define rtlglue_printf printk
#else// __KERNEL__
#define rtlglue_printf printf
#endif// __KERNEL__

void *rtlglue_malloc(unsigned int);
void rtlglue_free(void *APTR);

#endif
