/*
 * Copyright (c) 1993 Martin Birgmeier
 * All rights reserved.
 *
 * You may redistribute unmodified or modified versions of this source
 * code provided that the above copyright notice and this and the
 * following conditions are retained.
 *
 * This software is provided ``as is'', and comes with no warranties
 * of any kind. I shall in no event be liable for anything that happens
 * to anyone/anything when using this software.
 *
 * $FreeBSD: release/9.1.0/lib/libc/gen/rand48.h 90045 2002-02-01 01:32:19Z obrien $
 */

#ifndef _RAND48_H_
#define _RAND48_H_

#ifndef LITTLE_ENDIAN
#define	LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#endif

#ifndef BIG_ENDIAN
#define	BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#endif

#ifndef BYTE_ORDER
#define	BYTE_ORDER	LITTLE_ENDIAN
#endif

#if (defined(WIN32) || defined(_WINDOWS)) && !defined(__CYGWIN__)
#else
#	include <sys/types.h>
#endif
#include <math.h>
#include <stdlib.h>

void BSD_dorand48(unsigned short[3]);
void BSD_srand48(long seed);
double BSD_drand48(void);
double BSD_erand48(unsigned short xseed[3]);
double BSD_ldexp(double x, int n);
long BSD_lrand48(void);

#define	RAND48_SEED_0	(0x330e)
#define	RAND48_SEED_1	(0xabcd)
#define	RAND48_SEED_2	(0x1234)
#define	RAND48_MULT_0	(0xe66d)
#define	RAND48_MULT_1	(0xdeec)
#define	RAND48_MULT_2	(0x0005)
#define	RAND48_ADD	(0x000b)

#endif /* _RAND48_H_ */
