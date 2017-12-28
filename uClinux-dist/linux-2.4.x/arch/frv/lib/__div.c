/* More subroutines needed by GCC output code on some machines.  */
/* Compile this one with gcc.  */
/* Copyright (C) 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003  Free Software Foundation, Inc.

This file is based on selective parts of libgcc2.c

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

struct DWstruct {int high, low;};

typedef union
{
  struct DWstruct s;
  long long ll;
} DWunion;


#define add_ssaaaa(sh, sl, ah, al, bh, bl)				\
  __asm__ ("addcc %3,%5,%1,icc3\n\taddx %2,%4,%0,icc3"			\
	   : "=d" (sh), "=d" (sl)					\
	   : "d" (ah), "d" (al), "d" (bh), "d" (bl)			\
	   : "icc3");

#define sub_ddmmss(sh, sl, ah, al, bh, bl)				\
  __asm__ ("subcc %3,%5,%1,icc3\n\tsubx %2,%4,%0,icc3"			\
	   : "=d" (sh), "=d" (sl)					\
	   : "d" (ah), "d" (al), "d" (bh), "d" (bl)			\
	   : "icc3");

#define umul_ppmm(ph, pl, m0, m1) \
  ({union {								\
      unsigned long long __ll;						\
      struct {unsigned int __h, __l; } __i;				\
    } __xx;								\
  __xx.__ll = __umulsidi3(m0, m1);					\
  (ph) = __xx.__i.__h; (pl) = __xx.__i.__l;})

#define __umulsidi3(u, v) (((unsigned long long)(unsigned int)(u)) * ((unsigned long long)(unsigned int)(v)))


#define __ll_B ((unsigned) 1 << 16)
#define __ll_lowpart(t) ((unsigned) (t) & 0xffff)
#define __ll_highpart(t) ((unsigned) (t) >> 16)

#define __udiv_qrnnd_c(q, r, n1, n0, d) \
  do {									\
    unsigned __d1, __d0, __q1, __q0;					\
    unsigned __r1, __r0, __m;						\
    __d1 = __ll_highpart (d);						\
    __d0 = __ll_lowpart (d);						\
									\
    __r1 = (n1) % __d1;							\
    __q1 = (n1) / __d1;							\
    __m = (unsigned) __q1 * __d0;						\
    __r1 = __r1 * __ll_B | __ll_highpart (n0);				\
    if (__r1 < __m)							\
      {									\
	__q1--, __r1 += (d);						\
	if (__r1 >= (d)) /* i.e. we didn't get carry when adding to __r1 */\
	  if (__r1 < __m)						\
	    __q1--, __r1 += (d);					\
      }									\
    __r1 -= __m;							\
									\
    __r0 = __r1 % __d1;							\
    __q0 = __r1 / __d1;							\
    __m = (unsigned) __q0 * __d0;						\
    __r0 = __r0 * __ll_B | __ll_lowpart (n0);				\
    if (__r0 < __m)							\
      {									\
	__q0--, __r0 += (d);						\
	if (__r0 >= (d))						\
	  if (__r0 < __m)						\
	    __q0--, __r0 += (d);					\
      }									\
    __r0 -= __m;							\
									\
    (q) = (unsigned) __q1 * __ll_B | __q0;				\
    (r) = __r0;								\
  } while (0)


#define count_leading_zeros(count, x) \
  do {                                                                  \
  __asm__ ("scani %1,1,%0"                                              \
           : "=r" ((unsigned)(count))                                   \
           : "r" ((unsigned)(x)));					\
  } while (0)

static inline __attribute__ ((__always_inline__))
unsigned long long
__udivmoddi4 (unsigned long long n, unsigned long long d, unsigned long long *rp)
{
  const DWunion nn = {.ll = n};
  const DWunion dd = {.ll = d};
  DWunion rr;
  unsigned d0, d1, n0, n1, n2;
  unsigned q0, q1;
  unsigned b, bm;

  d0 = dd.s.low;
  d1 = dd.s.high;
  n0 = nn.s.low;
  n1 = nn.s.high;

  if (d1 == 0)
    {
      if (d0 > n1)
	{
	  /* 0q = nn / 0D */

	  count_leading_zeros (bm, d0);

	  if (bm != 0)
	    {
	      /* Normalize, i.e. make the most significant bit of the
		 denominator set.  */

	      d0 = d0 << bm;
	      n1 = (n1 << bm) | (n0 >> (32 - bm));
	      n0 = n0 << bm;
	    }

	  __udiv_qrnnd_c (q0, n0, n1, n0, d0);
	  q1 = 0;

	  /* Remainder in n0 >> bm.  */
	}
      else
	{
	  /* qq = NN / 0d */

	  if (d0 == 0)
	    d0 = 1 / d0;	/* Divide intentionally by zero.  */

	  count_leading_zeros (bm, d0);

	  if (bm == 0)
	    {
	      /* From (n1 >= d0) /\ (the most significant bit of d0 is set),
		 conclude (the most significant bit of n1 is set) /\ (the
		 leading quotient digit q1 = 1).

		 This special case is necessary, not an optimization.
		 (Shifts counts of W_TYPE_SIZE are undefined.)  */

	      n1 -= d0;
	      q1 = 1;
	    }
	  else
	    {
	      /* Normalize.  */

	      b = 32 - bm;

	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      __udiv_qrnnd_c (q1, n1, n2, n1, d0);
	    }

	  /* n1 != d0...  */

	  __udiv_qrnnd_c (q0, n0, n1, n0, d0);

	  /* Remainder in n0 >> bm.  */
	}

      if (rp != 0)
	{
	  rr.s.low = n0 >> bm;
	  rr.s.high = 0;
	  *rp = rr.ll;
	}
    }
  else
    {
      if (d1 > n1)
	{
	  /* 00 = nn / DD */

	  q0 = 0;
	  q1 = 0;

	  /* Remainder in n1n0.  */
	  if (rp != 0)
	    {
	      rr.s.low = n0;
	      rr.s.high = n1;
	      *rp = rr.ll;
	    }
	}
      else
	{
	  /* 0q = NN / dd */

	  count_leading_zeros (bm, d1);
	  if (bm == 0)
	    {
	      /* From (n1 >= d1) /\ (the most significant bit of d1 is set),
		 conclude (the most significant bit of n1 is set) /\ (the
		 quotient digit q0 = 0 or 1).

		 This special case is necessary, not an optimization.  */

	      /* The condition on the next line takes advantage of that
		 n1 >= d1 (true due to program flow).  */
	      if (n1 > d1 || n0 >= d0)
		{
		  q0 = 1;
		  sub_ddmmss (n1, n0, n1, n0, d1, d0);
		}
	      else
		q0 = 0;

	      q1 = 0;

	      if (rp != 0)
		{
		  rr.s.low = n0;
		  rr.s.high = n1;
		  *rp = rr.ll;
		}
	    }
	  else
	    {
	      unsigned m1, m0;
	      /* Normalize.  */

	      b = 32 - bm;

	      d1 = (d1 << bm) | (d0 >> b);
	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      __udiv_qrnnd_c (q0, n1, n2, n1, d1);
	      umul_ppmm (m1, m0, q0, d0);

	      if (m1 > n1 || (m1 == n1 && m0 > n0))
		{
		  q0--;
		  sub_ddmmss (m1, m0, m1, m0, d1, d0);
		}

	      q1 = 0;

	      /* Remainder in (n1n0 - m1m0) >> bm.  */
	      if (rp != 0)
		{
		  sub_ddmmss (n1, n0, n1, n0, m1, m0);
		  rr.s.low = (n1 << b) | (n0 >> bm);
		  rr.s.high = n1 >> bm;
		  *rp = rr.ll;
		}
	    }
	}
    }

  const DWunion ww = {{.low = q0, .high = q1}};
  return ww.ll;
}


unsigned long long
__udivdi3 (unsigned long long n, unsigned long long d)
{
  return __udivmoddi4 (n, d, (unsigned long long *) 0);
}

long long
__divll (long long u, long long v)
{
  int c = 0;
  DWunion uu = {.ll = u};
  DWunion vv = {.ll = v};
  long long w;

  if (uu.s.high < 0)
    c = ~c,
    uu.ll = -uu.ll;
  if (vv.s.high < 0)
    c = ~c,
    vv.ll = -vv.ll;

  w = __udivmoddi4 (uu.ll, vv.ll, (unsigned long long *) 0);
  if (c)
    w = -w;

  return w;
}
