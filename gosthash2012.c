/*
 * GOST R 34.11-2012 core functions.
 *
 * Copyright (c) 2013 Cryptocom LTD.
 * This file is distributed under the same license as OpenSSL.
 *
 * Author: Alexey Degtyarev <alexey@renatasystems.org>
 *
 */

#include "gosthash2012.h"

#if defined(_WIN32) || defined(_WINDOWS)
# define INLINE __inline
#else
# define INLINE inline
#endif

#define BSWAP64(x) \
    (((x & 0xFF00000000000000ULL) >> 56) | \
     ((x & 0x00FF000000000000ULL) >> 40) | \
     ((x & 0x0000FF0000000000ULL) >> 24) | \
     ((x & 0x000000FF00000000ULL) >>  8) | \
     ((x & 0x00000000FF000000ULL) <<  8) | \
     ((x & 0x0000000000FF0000ULL) << 24) | \
     ((x & 0x000000000000FF00ULL) << 40) | \
     ((x & 0x00000000000000FFULL) << 56))

/*
 * Initialize gost2012 hash context structure
 */
void init_gost2012_hash_ctx(gost2012_hash_ctx * CTX,
                            const unsigned int digest_size)
{
    memset(CTX, 0, sizeof(gost2012_hash_ctx));

    CTX->digest_size = digest_size;
    /*
     * IV for 512-bit hash should be 0^512
     * IV for 256-bit hash should be (00000001)^64
     *
     * It's already zeroed when CTX is cleared above, so we only
     * need to set it to 0x01-s for 256-bit hash.
     */
    if (digest_size == 256)
        memset(&CTX->h, 0x01, sizeof(uint512_u));
}

static INLINE void pad(gost2012_hash_ctx * CTX)
{
    memset(&(CTX->buffer[CTX->bufsize]), 0, sizeof(CTX->buffer) - CTX->bufsize);
    CTX->buffer[CTX->bufsize] = 1;

}

static INLINE void add512(union uint512_u * RESTRICT x,
                          const union uint512_u * RESTRICT y)
{
#ifndef __GOST3411_BIG_ENDIAN__
    unsigned int CF;
    unsigned int i;

    CF = 0;
    for (i = 0; i < 8; i++) {
	const unsigned long long left = x->QWORD[i];
	unsigned long long sum;

	sum = left + y->QWORD[i] + CF;
	/*
	 * (sum == left): is noop, because it's possible only
	 * when `left' is added with `0 + 0' or with `ULLONG_MAX + 1',
	 * in that case `CF' (carry) retain previous value, which is correct,
	 * because when `left + 0 + 0' there was no overflow (thus no carry),
	 * and when `left + ULLONG_MAX + 1' value is wrapped back to
	 * itself with overflow, thus creating carry.
	 *
	 * (sum != left):
	 * if `sum' is not wrapped (sum > left) there should not be carry,
	 * if `sum' is wrapped (sum < left) there should be carry.
	 */
	if (sum != left)
	    CF = (sum < left);
	x->QWORD[i] = sum;
    }
#else
    const unsigned char *yp;
    unsigned char *xp;
    unsigned int i;
    int buf;

    xp = (unsigned char *)&x[0];
    yp = (const unsigned char *)&y[0];

    buf = 0;
    for (i = 0; i < 64; i++) {
        buf = xp[i] + yp[i] + (buf >> 8);
        xp[i] = (unsigned char)buf & 0xFF;
    }
#endif
}

static void g(union uint512_u *h, const union uint512_u *N,
              const unsigned char *m)
{
#ifdef __GOST3411_HAS_SSE2__
    __m128i xmm0, xmm2, xmm4, xmm6; /* XMMR0-quadruple */
    __m128i xmm1, xmm3, xmm5, xmm7; /* XMMR1-quadruple */
    unsigned int i;

    LOAD(N, xmm0, xmm2, xmm4, xmm6);
    XLPS128M(h, xmm0, xmm2, xmm4, xmm6);

    LOAD(m, xmm1, xmm3, xmm5, xmm7);
    XLPS128R(xmm0, xmm2, xmm4, xmm6, xmm1, xmm3, xmm5, xmm7);

    for (i = 0; i < 11; i++)
        ROUND128(i, xmm0, xmm2, xmm4, xmm6, xmm1, xmm3, xmm5, xmm7);

    XLPS128M((&C[11]), xmm0, xmm2, xmm4, xmm6);
    X128R(xmm0, xmm2, xmm4, xmm6, xmm1, xmm3, xmm5, xmm7);

    X128M(h, xmm0, xmm2, xmm4, xmm6);
    X128M(m, xmm0, xmm2, xmm4, xmm6);

    UNLOAD(h, xmm0, xmm2, xmm4, xmm6);

    /* Restore the Floating-point status on the CPU */
    _mm_empty();
#else
    union uint512_u Ki, data;
    unsigned int i;

    XLPS(h, N, (&data));

    /* Starting E() */
    Ki = data;
    XLPS((&Ki), ((const union uint512_u *)&m[0]), (&data));

    for (i = 0; i < 11; i++)
        ROUND(i, (&Ki), (&data));

    XLPS((&Ki), (&C[11]), (&Ki));
    X((&Ki), (&data), (&data));
    /* E() done */

    X((&data), h, (&data));
    X((&data), ((const union uint512_u *)&m[0]), h);
#endif
}

static INLINE void stage2(gost2012_hash_ctx * CTX, const unsigned char *data)
{
    union uint512_u m;

    memcpy(&m, data, sizeof(m));
    g(&(CTX->h), &(CTX->N), (const unsigned char *)&m);

    add512(&(CTX->N), &buffer512);
    add512(&(CTX->Sigma), &m);
}

static INLINE void stage3(gost2012_hash_ctx * CTX)
{
    ALIGN(16) union uint512_u buf;

    memset(&buf, 0x00, sizeof buf);
    memcpy(&buf, &(CTX->buffer), CTX->bufsize);
    memcpy(&(CTX->buffer), &buf, sizeof(uint512_u));

    memset(&buf, 0x00, sizeof buf);
#ifndef __GOST3411_BIG_ENDIAN__
    buf.QWORD[0] = CTX->bufsize << 3;
#else
    buf.QWORD[0] = BSWAP64(CTX->bufsize << 3);
#endif

    pad(CTX);

    g(&(CTX->h), &(CTX->N), (const unsigned char *)&(CTX->buffer));

    add512(&(CTX->N), &buf);
    add512(&(CTX->Sigma), (const union uint512_u *)&CTX->buffer[0]);

    g(&(CTX->h), &buffer0, (const unsigned char *)&(CTX->N));

    g(&(CTX->h), &buffer0, (const unsigned char *)&(CTX->Sigma));
}

/*
 * Hash block of arbitrary length
 *
 */
void gost2012_hash_block(gost2012_hash_ctx * CTX,
                         const unsigned char *data, size_t len)
{
    size_t chunksize;

    while (len > 63 && CTX->bufsize == 0) {
        stage2(CTX, data);

        data += 64;
        len -= 64;
    }

    while (len) {
        chunksize = 64 - CTX->bufsize;
        if (chunksize > len)
            chunksize = len;

        memcpy(&CTX->buffer[CTX->bufsize], data, chunksize);

        CTX->bufsize += chunksize;
        len -= chunksize;
        data += chunksize;

        if (CTX->bufsize == 64) {
            stage2(CTX, CTX->buffer);

            CTX->bufsize = 0;
        }
    }
}

/*
 * Compute hash value from current state of ctx
 * state of hash ctx becomes invalid and cannot be used for further
 * hashing.
 */
void gost2012_finish_hash(gost2012_hash_ctx * CTX, unsigned char *digest)
{
    stage3(CTX);

    CTX->bufsize = 0;

    if (CTX->digest_size == 256)
        memcpy(digest, &(CTX->h.QWORD[4]), 32);
    else
        memcpy(digest, &(CTX->h.QWORD[0]), 64);
}
