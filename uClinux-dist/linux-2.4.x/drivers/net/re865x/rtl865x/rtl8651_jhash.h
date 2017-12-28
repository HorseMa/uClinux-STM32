/*
	jhash implementation
*/

#ifndef RTL8651_JHASH_H
#define RTL8651_JHASH_H

#define __jhash_mix(a, b, c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/* The golden ration: an arbitrary value */
#define JHASH_GOLDEN_RATIO	0x9e3779b9


static __inline__ uint32 jhash(void *, uint32, uint32);
static __inline__ uint32 jhash2(uint32 *, uint32, uint32);
static __inline__ uint32 jhash_3words(uint32, uint32, uint32, uint32);
static __inline__ uint32 jhash_2words(uint32, uint32, uint32);
static __inline__ uint32 jhash_1word(uint32, uint32);


static __inline__ uint32 jhash(void *key, uint32 length, uint32 initval)
{
	uint32 a, b, c, len;
	uint8 *k = key;

	len = length;
	a = b = JHASH_GOLDEN_RATIO;
	c = initval;

	while (len >= 12) {
		a += (k[0] +((uint32)k[1]<<8) +((uint32)k[2]<<16) +((uint32)k[3]<<24));
		b += (k[4] +((uint32)k[5]<<8) +((uint32)k[6]<<16) +((uint32)k[7]<<24));
		c += (k[8] +((uint32)k[9]<<8) +((uint32)k[10]<<16)+((uint32)k[11]<<24));

		__jhash_mix(a,b,c);

		k += 12;
		len -= 12;
	}

	c += length;
	switch (len) {
	case 11: c += ((uint32)k[10]<<24);
	case 10: c += ((uint32)k[9]<<16);
	case 9 : c += ((uint32)k[8]<<8);
	case 8 : b += ((uint32)k[7]<<24);
	case 7 : b += ((uint32)k[6]<<16);
	case 6 : b += ((uint32)k[5]<<8);
	case 5 : b += k[4];
	case 4 : a += ((uint32)k[3]<<24);
	case 3 : a += ((uint32)k[2]<<16);
	case 2 : a += ((uint32)k[1]<<8);
	case 1 : a += k[0];
	};

	__jhash_mix(a,b,c);

	return c;
}

/* A special optimized version that handles 1 or more of u32s.
 * The length parameter here is the number of u32s in the key.
 */
static __inline__ uint32 jhash2(uint32 *k, uint32 length, uint32 initval)
{
	uint32 a, b, c, len;

	a = b = JHASH_GOLDEN_RATIO;
	c = initval;
	len = length;

	while (len >= 3) {
		a += k[0];
		b += k[1];
		c += k[2];
		__jhash_mix(a, b, c);
		k += 3; len -= 3;
	}

	c += length * 4;

	switch (len) {
	case 2 : b += k[1];
	case 1 : a += k[0];
	};

	__jhash_mix(a,b,c);

	return c;
}

/* A special ultra-optimized versions that knows they are hashing exactly
 * 3, 2 or 1 word(s).
 *
 * NOTE: In partilar the "c += length; __jhash_mix(a,b,c);" normally
 *       done at the end is not done here.
 */
static __inline__ uint32 jhash_3words(uint32 a, uint32 b, uint32 c, uint32 initval)
{
	a += JHASH_GOLDEN_RATIO;
	b += JHASH_GOLDEN_RATIO;
	c += initval;

	__jhash_mix(a, b, c);

	return c;
}

static __inline__ uint32 jhash_2words(uint32 a, uint32 b, uint32 initval)
{
	return jhash_3words(a, b, 0, initval);
}

static __inline__ uint32 jhash_1word(uint32 a, uint32 initval)
{
	return jhash_3words(a, 0, 0, initval);
}


#endif /* RTL8651_JHASH_H */
