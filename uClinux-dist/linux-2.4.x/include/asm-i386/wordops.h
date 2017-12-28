#ifndef _I386_WORDOPS_H
#define _I386_WORDOPS_H

extern __inline__ int rotr32(__u32 x, int n)
{
	if (n == 32 || n == 0 || n == -32)
		return x;
	__asm__("rorl %1,%0" : "=r"(x) : "ir"(n));
	return x;
}

extern __inline__ int rotl32(__u32 x, int n)
{
	if (n == 32 || n == 0 || n == -32)
		return x;
	__asm__("roll %1,%0" : "=r"(x) : "ir"(n));
	return x;
}

#endif
