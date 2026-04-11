/*
 * This file is part of the VanitySearch distribution (https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "ripemd160.h"
#include <string.h>
#include <immintrin.h>

// Internal SSE RIPEMD-160 implementation.
namespace ripemd160sse {

#ifdef WIN64
  static const __declspec(align(16)) uint32_t _init[] = {
#else
  static const uint32_t _init[] __attribute__ ((aligned (16))) = {
#endif
      0x67452301ul,0x67452301ul,0x67452301ul,0x67452301ul,
      0xEFCDAB89ul,0xEFCDAB89ul,0xEFCDAB89ul,0xEFCDAB89ul,
      0x98BADCFEul,0x98BADCFEul,0x98BADCFEul,0x98BADCFEul,
      0x10325476ul,0x10325476ul,0x10325476ul,0x10325476ul,
      0xC3D2E1F0ul,0xC3D2E1F0ul,0xC3D2E1F0ul,0xC3D2E1F0ul
  };

//#define f1(x, y, z) (x ^ y ^ z)
//#define f2(x, y, z) ((x & y) | (~x & z))
//#define f3(x, y, z) ((x | ~y) ^ z)
//#define f4(x, y, z) ((x & z) | (~z & y))
//#define f5(x, y, z) (x ^ (y | ~z))

#define ROL(x,n) _mm_or_si128( _mm_slli_epi32(x, n) , _mm_srli_epi32(x, 32 - n) )

#ifdef WIN64

#define not(x) _mm_andnot_si128(x, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()))
#define f1(x,y,z) _mm_xor_si128(x, _mm_xor_si128(y, z))
#define f2(x,y,z) _mm_or_si128(_mm_and_si128(x,y),_mm_andnot_si128(x,z))
#define f3(x,y,z) _mm_xor_si128(_mm_or_si128(x,not(y)),z)
#define f4(x,y,z) _mm_or_si128(_mm_and_si128(x,z),_mm_andnot_si128(z,y))
#define f5(x,y,z) _mm_xor_si128(x,_mm_or_si128(y,not(z)))

#else

#define f1(x,y,z) _mm_xor_si128(x, _mm_xor_si128(y, z))
#define f2(x,y,z) _mm_or_si128(_mm_and_si128(x,y),_mm_andnot_si128(x,z))
#define f3(x,y,z) _mm_xor_si128(_mm_or_si128(x,~(y)),z)
#define f4(x,y,z) _mm_or_si128(_mm_and_si128(x,z),_mm_andnot_si128(z,y))
#define f5(x,y,z) _mm_xor_si128(x,_mm_or_si128(y,~(z)))

#endif


#define add3(x0, x1, x2 ) _mm_add_epi32(_mm_add_epi32(x0, x1), x2)
#define add4(x0, x1, x2, x3) _mm_add_epi32(_mm_add_epi32(x0, x1), _mm_add_epi32(x2, x3))

#define Round(a,b,c,d,e,f,x,k,r) \
  u = add4(a,f,x,_mm_set1_epi32(k)); \
  a = _mm_add_epi32(ROL(u, r),e); \
  c = ROL(c, 10);

#define R11(a,b,c,d,e,x,r) Round(a, b, c, d, e, f1(b, c, d), x, 0, r)
#define R21(a,b,c,d,e,x,r) Round(a, b, c, d, e, f2(b, c, d), x, 0x5A827999ul, r)
#define R31(a,b,c,d,e,x,r) Round(a, b, c, d, e, f3(b, c, d), x, 0x6ED9EBA1ul, r)
#define R41(a,b,c,d,e,x,r) Round(a, b, c, d, e, f4(b, c, d), x, 0x8F1BBCDCul, r)
#define R51(a,b,c,d,e,x,r) Round(a, b, c, d, e, f5(b, c, d), x, 0xA953FD4Eul, r)
#define R12(a,b,c,d,e,x,r) Round(a, b, c, d, e, f5(b, c, d), x, 0x50A28BE6ul, r)
#define R22(a,b,c,d,e,x,r) Round(a, b, c, d, e, f4(b, c, d), x, 0x5C4DD124ul, r)
#define R32(a,b,c,d,e,x,r) Round(a, b, c, d, e, f3(b, c, d), x, 0x6D703EF3ul, r)
#define R42(a,b,c,d,e,x,r) Round(a, b, c, d, e, f2(b, c, d), x, 0x7A6D76E9ul, r)
#define R52(a,b,c,d,e,x,r) Round(a, b, c, d, e, f1(b, c, d), x, 0, r)

#define LOADW(i) _mm_set_epi32(*((uint32_t *)blk[0]+i),*((uint32_t *)blk[1]+i),*((uint32_t *)blk[2]+i),*((uint32_t *)blk[3]+i))

  // Initialize RIPEMD-160 state
  void Initialize(__m128i *s) {
    memcpy(s, _init, sizeof(_init));
  }

  // Perform 4 RIPE in parallel using SSE2
  void Transform(__m128i *s, uint8_t *blk[4]) {

    __m128i a1 = _mm_load_si128(s + 0);
    __m128i b1 = _mm_load_si128(s + 1);
    __m128i c1 = _mm_load_si128(s + 2);
    __m128i d1 = _mm_load_si128(s + 3);
    __m128i e1 = _mm_load_si128(s + 4);
    __m128i a2 = a1;
    __m128i b2 = b1;
    __m128i c2 = c1;
    __m128i d2 = d1;
    __m128i e2 = e1;
    __m128i u;
    __m128i w[16];


    w[0] = LOADW(0);
    w[1] = LOADW(1);
    w[2] = LOADW(2);
    w[3] = LOADW(3);
    w[4] = LOADW(4);
    w[5] = LOADW(5);
    w[6] = LOADW(6);
    w[7] = LOADW(7);
    w[8] = LOADW(8);
    w[9] = LOADW(9);
    w[10] = LOADW(10);
    w[11] = LOADW(11);
    w[12] = LOADW(12);
    w[13] = LOADW(13);
    w[14] = LOADW(14);
    w[15] = LOADW(15);

    R11(a1, b1, c1, d1, e1, w[0], 11);
    R12(a2, b2, c2, d2, e2, w[5], 8);
    R11(e1, a1, b1, c1, d1, w[1], 14);
    R12(e2, a2, b2, c2, d2, w[14], 9);
    R11(d1, e1, a1, b1, c1, w[2], 15);
    R12(d2, e2, a2, b2, c2, w[7], 9);
    R11(c1, d1, e1, a1, b1, w[3], 12);
    R12(c2, d2, e2, a2, b2, w[0], 11);
    R11(b1, c1, d1, e1, a1, w[4], 5);
    R12(b2, c2, d2, e2, a2, w[9], 13);
    R11(a1, b1, c1, d1, e1, w[5], 8);
    R12(a2, b2, c2, d2, e2, w[2], 15);
    R11(e1, a1, b1, c1, d1, w[6], 7);
    R12(e2, a2, b2, c2, d2, w[11], 15);
    R11(d1, e1, a1, b1, c1, w[7], 9);
    R12(d2, e2, a2, b2, c2, w[4], 5);
    R11(c1, d1, e1, a1, b1, w[8], 11);
    R12(c2, d2, e2, a2, b2, w[13], 7);
    R11(b1, c1, d1, e1, a1, w[9], 13);
    R12(b2, c2, d2, e2, a2, w[6], 7);
    R11(a1, b1, c1, d1, e1, w[10], 14);
    R12(a2, b2, c2, d2, e2, w[15], 8);
    R11(e1, a1, b1, c1, d1, w[11], 15);
    R12(e2, a2, b2, c2, d2, w[8], 11);
    R11(d1, e1, a1, b1, c1, w[12], 6);
    R12(d2, e2, a2, b2, c2, w[1], 14);
    R11(c1, d1, e1, a1, b1, w[13], 7);
    R12(c2, d2, e2, a2, b2, w[10], 14);
    R11(b1, c1, d1, e1, a1, w[14], 9);
    R12(b2, c2, d2, e2, a2, w[3], 12);
    R11(a1, b1, c1, d1, e1, w[15], 8);
    R12(a2, b2, c2, d2, e2, w[12], 6);

    R21(e1, a1, b1, c1, d1, w[7], 7);
    R22(e2, a2, b2, c2, d2, w[6], 9);
    R21(d1, e1, a1, b1, c1, w[4], 6);
    R22(d2, e2, a2, b2, c2, w[11], 13);
    R21(c1, d1, e1, a1, b1, w[13], 8);
    R22(c2, d2, e2, a2, b2, w[3], 15);
    R21(b1, c1, d1, e1, a1, w[1], 13);
    R22(b2, c2, d2, e2, a2, w[7], 7);
    R21(a1, b1, c1, d1, e1, w[10], 11);
    R22(a2, b2, c2, d2, e2, w[0], 12);
    R21(e1, a1, b1, c1, d1, w[6], 9);
    R22(e2, a2, b2, c2, d2, w[13], 8);
    R21(d1, e1, a1, b1, c1, w[15], 7);
    R22(d2, e2, a2, b2, c2, w[5], 9);
    R21(c1, d1, e1, a1, b1, w[3], 15);
    R22(c2, d2, e2, a2, b2, w[10], 11);
    R21(b1, c1, d1, e1, a1, w[12], 7);
    R22(b2, c2, d2, e2, a2, w[14], 7);
    R21(a1, b1, c1, d1, e1, w[0], 12);
    R22(a2, b2, c2, d2, e2, w[15], 7);
    R21(e1, a1, b1, c1, d1, w[9], 15);
    R22(e2, a2, b2, c2, d2, w[8], 12);
    R21(d1, e1, a1, b1, c1, w[5], 9);
    R22(d2, e2, a2, b2, c2, w[12], 7);
    R21(c1, d1, e1, a1, b1, w[2], 11);
    R22(c2, d2, e2, a2, b2, w[4], 6);
    R21(b1, c1, d1, e1, a1, w[14], 7);
    R22(b2, c2, d2, e2, a2, w[9], 15);
    R21(a1, b1, c1, d1, e1, w[11], 13);
    R22(a2, b2, c2, d2, e2, w[1], 13);
    R21(e1, a1, b1, c1, d1, w[8], 12);
    R22(e2, a2, b2, c2, d2, w[2], 11);

    R31(d1, e1, a1, b1, c1, w[3], 11);
    R32(d2, e2, a2, b2, c2, w[15], 9);
    R31(c1, d1, e1, a1, b1, w[10], 13);
    R32(c2, d2, e2, a2, b2, w[5], 7);
    R31(b1, c1, d1, e1, a1, w[14], 6);
    R32(b2, c2, d2, e2, a2, w[1], 15);
    R31(a1, b1, c1, d1, e1, w[4], 7);
    R32(a2, b2, c2, d2, e2, w[3], 11);
    R31(e1, a1, b1, c1, d1, w[9], 14);
    R32(e2, a2, b2, c2, d2, w[7], 8);
    R31(d1, e1, a1, b1, c1, w[15], 9);
    R32(d2, e2, a2, b2, c2, w[14], 6);
    R31(c1, d1, e1, a1, b1, w[8], 13);
    R32(c2, d2, e2, a2, b2, w[6], 6);
    R31(b1, c1, d1, e1, a1, w[1], 15);
    R32(b2, c2, d2, e2, a2, w[9], 14);
    R31(a1, b1, c1, d1, e1, w[2], 14);
    R32(a2, b2, c2, d2, e2, w[11], 12);
    R31(e1, a1, b1, c1, d1, w[7], 8);
    R32(e2, a2, b2, c2, d2, w[8], 13);
    R31(d1, e1, a1, b1, c1, w[0], 13);
    R32(d2, e2, a2, b2, c2, w[12], 5);
    R31(c1, d1, e1, a1, b1, w[6], 6);
    R32(c2, d2, e2, a2, b2, w[2], 14);
    R31(b1, c1, d1, e1, a1, w[13], 5);
    R32(b2, c2, d2, e2, a2, w[10], 13);
    R31(a1, b1, c1, d1, e1, w[11], 12);
    R32(a2, b2, c2, d2, e2, w[0], 13);
    R31(e1, a1, b1, c1, d1, w[5], 7);
    R32(e2, a2, b2, c2, d2, w[4], 7);
    R31(d1, e1, a1, b1, c1, w[12], 5);
    R32(d2, e2, a2, b2, c2, w[13], 5);

    R41(c1, d1, e1, a1, b1, w[1], 11);
    R42(c2, d2, e2, a2, b2, w[8], 15);
    R41(b1, c1, d1, e1, a1, w[9], 12);
    R42(b2, c2, d2, e2, a2, w[6], 5);
    R41(a1, b1, c1, d1, e1, w[11], 14);
    R42(a2, b2, c2, d2, e2, w[4], 8);
    R41(e1, a1, b1, c1, d1, w[10], 15);
    R42(e2, a2, b2, c2, d2, w[1], 11);
    R41(d1, e1, a1, b1, c1, w[0], 14);
    R42(d2, e2, a2, b2, c2, w[3], 14);
    R41(c1, d1, e1, a1, b1, w[8], 15);
    R42(c2, d2, e2, a2, b2, w[11], 14);
    R41(b1, c1, d1, e1, a1, w[12], 9);
    R42(b2, c2, d2, e2, a2, w[15], 6);
    R41(a1, b1, c1, d1, e1, w[4], 8);
    R42(a2, b2, c2, d2, e2, w[0], 14);
    R41(e1, a1, b1, c1, d1, w[13], 9);
    R42(e2, a2, b2, c2, d2, w[5], 6);
    R41(d1, e1, a1, b1, c1, w[3], 14);
    R42(d2, e2, a2, b2, c2, w[12], 9);
    R41(c1, d1, e1, a1, b1, w[7], 5);
    R42(c2, d2, e2, a2, b2, w[2], 12);
    R41(b1, c1, d1, e1, a1, w[15], 6);
    R42(b2, c2, d2, e2, a2, w[13], 9);
    R41(a1, b1, c1, d1, e1, w[14], 8);
    R42(a2, b2, c2, d2, e2, w[9], 12);
    R41(e1, a1, b1, c1, d1, w[5], 6);
    R42(e2, a2, b2, c2, d2, w[7], 5);
    R41(d1, e1, a1, b1, c1, w[6], 5);
    R42(d2, e2, a2, b2, c2, w[10], 15);
    R41(c1, d1, e1, a1, b1, w[2], 12);
    R42(c2, d2, e2, a2, b2, w[14], 8);

    R51(b1, c1, d1, e1, a1, w[4], 9);
    R52(b2, c2, d2, e2, a2, w[12], 8);
    R51(a1, b1, c1, d1, e1, w[0], 15);
    R52(a2, b2, c2, d2, e2, w[15], 5);
    R51(e1, a1, b1, c1, d1, w[5], 5);
    R52(e2, a2, b2, c2, d2, w[10], 12);
    R51(d1, e1, a1, b1, c1, w[9], 11);
    R52(d2, e2, a2, b2, c2, w[4], 9);
    R51(c1, d1, e1, a1, b1, w[7], 6);
    R52(c2, d2, e2, a2, b2, w[1], 12);
    R51(b1, c1, d1, e1, a1, w[12], 8);
    R52(b2, c2, d2, e2, a2, w[5], 5);
    R51(a1, b1, c1, d1, e1, w[2], 13);
    R52(a2, b2, c2, d2, e2, w[8], 14);
    R51(e1, a1, b1, c1, d1, w[10], 12);
    R52(e2, a2, b2, c2, d2, w[7], 6);
    R51(d1, e1, a1, b1, c1, w[14], 5);
    R52(d2, e2, a2, b2, c2, w[6], 8);
    R51(c1, d1, e1, a1, b1, w[1], 12);
    R52(c2, d2, e2, a2, b2, w[2], 13);
    R51(b1, c1, d1, e1, a1, w[3], 13);
    R52(b2, c2, d2, e2, a2, w[13], 6);
    R51(a1, b1, c1, d1, e1, w[8], 14);
    R52(a2, b2, c2, d2, e2, w[14], 5);
    R51(e1, a1, b1, c1, d1, w[11], 11);
    R52(e2, a2, b2, c2, d2, w[0], 15);
    R51(d1, e1, a1, b1, c1, w[6], 8);
    R52(d2, e2, a2, b2, c2, w[3], 13);
    R51(c1, d1, e1, a1, b1, w[15], 5);
    R52(c2, d2, e2, a2, b2, w[9], 11);
    R51(b1, c1, d1, e1, a1, w[13], 6);
    R52(b2, c2, d2, e2, a2, w[11], 11);

    __m128i t = s[0];
    s[0] = add3(s[1],c1,d2);
    s[1] = add3(s[2],d1,e2);
    s[2] = add3(s[3],e1,a2);
    s[3] = add3(s[4],a1,b2);
    s[4] = add3(t,b1,c2);
  }

} // namespace ripemd160sse

#ifdef WIN64

#define DEPACK(d,i) \
((uint32_t *)d)[0] = s[0].m128i_u32[i]; \
((uint32_t *)d)[1] = s[1].m128i_u32[i]; \
((uint32_t *)d)[2] = s[2].m128i_u32[i]; \
((uint32_t *)d)[3] = s[3].m128i_u32[i]; \
((uint32_t *)d)[4] = s[4].m128i_u32[i];

#else

#define DEPACK(d,i) \
((uint32_t *)d)[0] = s0[i]; \
((uint32_t *)d)[1] = s1[i]; \
((uint32_t *)d)[2] = s2[i]; \
((uint32_t *)d)[3] = s3[i]; \
((uint32_t *)d)[4] = s4[i];

#endif

static const uint64_t sizedesc_32 = 32 << 3;
static const unsigned char pad[64] = { 0x80 };

void ripemd160sse_32(
  unsigned char *i0,
  unsigned char *i1,
  unsigned char *i2,
  unsigned char *i3,
  unsigned char *d0,
  unsigned char *d1,
  unsigned char *d2,
  unsigned char *d3) {

  __m128i s[5];
  uint8_t *bs[] = { i0,i1,i2,i3 };

  ripemd160sse::Initialize(s);
  memcpy(i0 + 32, pad, 24);
  memcpy(i0 + 56, &sizedesc_32, 8);
  memcpy(i1 + 32, pad, 24);
  memcpy(i1 + 56, &sizedesc_32, 8);
  memcpy(i2 + 32, pad, 24);
  memcpy(i2 + 56, &sizedesc_32, 8);
  memcpy(i3 + 32, pad, 24);
  memcpy(i3 + 56, &sizedesc_32, 8);

  ripemd160sse::Transform(s, bs);

#ifndef WIN64
  uint32_t *s0 = (uint32_t *)&s[0];
  uint32_t *s1 = (uint32_t *)&s[1];
  uint32_t *s2 = (uint32_t *)&s[2];
  uint32_t *s3 = (uint32_t *)&s[3];
  uint32_t *s4 = (uint32_t *)&s[4];
#endif

  DEPACK(d0,3);
  DEPACK(d1,2);
  DEPACK(d2,1);
  DEPACK(d3,0);

}

void ripemd160sse_test() {

  unsigned char h0[20];
  unsigned char h1[20];
  unsigned char h2[20];
  unsigned char h3[20];
  unsigned char ch0[20];
  unsigned char ch1[20];
  unsigned char ch2[20];
  unsigned char ch3[20];
  unsigned char m0[64];
  unsigned char m1[64];
  unsigned char m2[64];
  unsigned char m3[64];

  strcpy((char *)m0, "This is a test message to test01");
  strcpy((char *)m1, "This is a test message to test02");
  strcpy((char *)m2, "This is a test message to test03");
  strcpy((char *)m3, "This is a test message to test04");

  ripemd160_32(m0, ch0);
  ripemd160_32(m1, ch1);
  ripemd160_32(m2, ch2);
  ripemd160_32(m3, ch3);

  ripemd160sse_32(m0, m1, m2, m3, h0, h1, h2, h3);

  if ((ripemd160_hex(h0) != ripemd160_hex(ch0)) ||
    (ripemd160_hex(h1) != ripemd160_hex(ch1)) ||
    (ripemd160_hex(h2) != ripemd160_hex(ch2)) ||
    (ripemd160_hex(h3) != ripemd160_hex(ch3))) {

    printf("RIPEMD160() Results Wrong !\n");
    printf("RIP: %s\n", ripemd160_hex(ch0).c_str());
    printf("RIP: %s\n", ripemd160_hex(ch1).c_str());
    printf("RIP: %s\n", ripemd160_hex(ch2).c_str());
    printf("RIP: %s\n\n", ripemd160_hex(ch3).c_str());
    printf("SSE: %s\n", ripemd160_hex(h0).c_str());
    printf("SSE: %s\n", ripemd160_hex(h1).c_str());
    printf("SSE: %s\n", ripemd160_hex(h2).c_str());
    printf("SSE: %s\n\n", ripemd160_hex(h3).c_str());

  }

  printf("RIPE() Results OK !\n");

}

// ============================================================
// AVX2 8-wide RIPEMD160 implementation
// Processes 8 RIPEMD-160 hashes in parallel using 256-bit vectors.
// Requires: -mavx2 (auto-detected via __AVX2__ macro).
// ============================================================
#ifdef __AVX2__
#include <immintrin.h>

namespace ripemd160avx2 {

static const uint32_t _init[] __attribute__((aligned(32))) = {
    0x67452301ul,0x67452301ul,0x67452301ul,0x67452301ul,
    0x67452301ul,0x67452301ul,0x67452301ul,0x67452301ul,
    0xEFCDAB89ul,0xEFCDAB89ul,0xEFCDAB89ul,0xEFCDAB89ul,
    0xEFCDAB89ul,0xEFCDAB89ul,0xEFCDAB89ul,0xEFCDAB89ul,
    0x98BADCFEul,0x98BADCFEul,0x98BADCFEul,0x98BADCFEul,
    0x98BADCFEul,0x98BADCFEul,0x98BADCFEul,0x98BADCFEul,
    0x10325476ul,0x10325476ul,0x10325476ul,0x10325476ul,
    0x10325476ul,0x10325476ul,0x10325476ul,0x10325476ul,
    0xC3D2E1F0ul,0xC3D2E1F0ul,0xC3D2E1F0ul,0xC3D2E1F0ul,
    0xC3D2E1F0ul,0xC3D2E1F0ul,0xC3D2E1F0ul,0xC3D2E1F0ul
};

// AVX2 rotate left (no native instruction; emulate with shift+or)
#define B_ROL(x,n) _mm256_or_si256(_mm256_slli_epi32((x),(n)), _mm256_srli_epi32((x),32-(n)))

// NOT: AVX2 has no ~; use XOR with all-ones
#define B_NOT(x) _mm256_xor_si256((x), _mm256_set1_epi32(-1))

#define B_f1(x,y,z) _mm256_xor_si256((x), _mm256_xor_si256((y),(z)))
#define B_f2(x,y,z) _mm256_or_si256(_mm256_and_si256((x),(y)), _mm256_andnot_si256((x),(z)))
#define B_f3(x,y,z) _mm256_xor_si256(_mm256_or_si256((x), B_NOT(y)), (z))
#define B_f4(x,y,z) _mm256_or_si256(_mm256_and_si256((x),(z)), _mm256_andnot_si256((z),(y)))
#define B_f5(x,y,z) _mm256_xor_si256((x), _mm256_or_si256((y), B_NOT(z)))

#define B_add3(x0,x1,x2) _mm256_add_epi32(_mm256_add_epi32((x0),(x1)),(x2))
#define B_add4(x0,x1,x2,x3) _mm256_add_epi32(_mm256_add_epi32((x0),(x1)),_mm256_add_epi32((x2),(x3)))

#define B_Round(a,b,c,d,e,f,x,k,r) \
  u = B_add4(a,f,x,_mm256_set1_epi32(k)); \
  a = _mm256_add_epi32(B_ROL(u,(r)),(e)); \
  c = B_ROL((c),10);

#define B_R11(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f1(b,c,d),x,0,r)
#define B_R21(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f2(b,c,d),x,0x5A827999ul,r)
#define B_R31(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f3(b,c,d),x,0x6ED9EBA1ul,r)
#define B_R41(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f4(b,c,d),x,0x8F1BBCDCul,r)
#define B_R51(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f5(b,c,d),x,0xA953FD4Eul,r)
#define B_R12(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f5(b,c,d),x,0x50A28BE6ul,r)
#define B_R22(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f4(b,c,d),x,0x5C4DD124ul,r)
#define B_R32(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f3(b,c,d),x,0x6D703EF3ul,r)
#define B_R42(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f2(b,c,d),x,0x7A6D76E9ul,r)
#define B_R52(a,b,c,d,e,x,r) B_Round(a,b,c,d,e,B_f1(b,c,d),x,0,r)

// Load word i from 8 independent input buffers into a single __m256i.
// Lane mapping: blk[0] → lane7, blk[7] → lane0 (matching _mm256_set_epi32 order).
#define B_LOADW(i) _mm256_set_epi32( \
    *((uint32_t *)blk[0]+(i)), *((uint32_t *)blk[1]+(i)), \
    *((uint32_t *)blk[2]+(i)), *((uint32_t *)blk[3]+(i)), \
    *((uint32_t *)blk[4]+(i)), *((uint32_t *)blk[5]+(i)), \
    *((uint32_t *)blk[6]+(i)), *((uint32_t *)blk[7]+(i)))

  void Initialize(__m256i *s) {
    memcpy(s, _init, sizeof(_init));
  }

  void Transform(__m256i *s, uint8_t *blk[8]) {
    __m256i a1 = _mm256_load_si256(s + 0);
    __m256i b1 = _mm256_load_si256(s + 1);
    __m256i c1 = _mm256_load_si256(s + 2);
    __m256i d1 = _mm256_load_si256(s + 3);
    __m256i e1 = _mm256_load_si256(s + 4);
    __m256i a2 = a1, b2 = b1, c2 = c1, d2 = d1, e2 = e1;
    __m256i u;
    __m256i w[16];

    for (int i = 0; i < 16; i++) w[i] = B_LOADW(i);

    // Left rounds
    B_R11(a1,b1,c1,d1,e1,w[0],11);  B_R11(e1,a1,b1,c1,d1,w[1],14);
    B_R11(d1,e1,a1,b1,c1,w[2],15);  B_R11(c1,d1,e1,a1,b1,w[3],12);
    B_R11(b1,c1,d1,e1,a1,w[4],5);   B_R11(a1,b1,c1,d1,e1,w[5],8);
    B_R11(e1,a1,b1,c1,d1,w[6],7);   B_R11(d1,e1,a1,b1,c1,w[7],9);
    B_R11(c1,d1,e1,a1,b1,w[8],11);  B_R11(b1,c1,d1,e1,a1,w[9],13);
    B_R11(a1,b1,c1,d1,e1,w[10],14); B_R11(e1,a1,b1,c1,d1,w[11],15);
    B_R11(d1,e1,a1,b1,c1,w[12],6);  B_R11(c1,d1,e1,a1,b1,w[13],7);
    B_R11(b1,c1,d1,e1,a1,w[14],9);  B_R11(a1,b1,c1,d1,e1,w[15],8);

    B_R21(e1,a1,b1,c1,d1,w[7],7);   B_R21(d1,e1,a1,b1,c1,w[4],6);
    B_R21(c1,d1,e1,a1,b1,w[13],8);  B_R21(b1,c1,d1,e1,a1,w[1],13);
    B_R21(a1,b1,c1,d1,e1,w[10],11); B_R21(e1,a1,b1,c1,d1,w[6],9);
    B_R21(d1,e1,a1,b1,c1,w[15],7);  B_R21(c1,d1,e1,a1,b1,w[3],15);
    B_R21(b1,c1,d1,e1,a1,w[12],7);  B_R21(a1,b1,c1,d1,e1,w[0],12);
    B_R21(e1,a1,b1,c1,d1,w[9],15);  B_R21(d1,e1,a1,b1,c1,w[5],9);
    B_R21(c1,d1,e1,a1,b1,w[2],11);  B_R21(b1,c1,d1,e1,a1,w[14],7);
    B_R21(a1,b1,c1,d1,e1,w[11],13); B_R21(e1,a1,b1,c1,d1,w[8],12);

    B_R31(d1,e1,a1,b1,c1,w[3],11);  B_R31(c1,d1,e1,a1,b1,w[10],13);
    B_R31(b1,c1,d1,e1,a1,w[14],6);  B_R31(a1,b1,c1,d1,e1,w[4],7);
    B_R31(e1,a1,b1,c1,d1,w[9],14);  B_R31(d1,e1,a1,b1,c1,w[15],9);
    B_R31(c1,d1,e1,a1,b1,w[8],13);  B_R31(b1,c1,d1,e1,a1,w[1],15);
    B_R31(a1,b1,c1,d1,e1,w[2],14);  B_R31(e1,a1,b1,c1,d1,w[7],8);
    B_R31(d1,e1,a1,b1,c1,w[0],13);  B_R31(c1,d1,e1,a1,b1,w[6],6);
    B_R31(b1,c1,d1,e1,a1,w[13],5);  B_R31(a1,b1,c1,d1,e1,w[11],12);
    B_R31(e1,a1,b1,c1,d1,w[5],7);   B_R31(d1,e1,a1,b1,c1,w[12],5);

    B_R41(c1,d1,e1,a1,b1,w[1],11);  B_R41(b1,c1,d1,e1,a1,w[9],12);
    B_R41(a1,b1,c1,d1,e1,w[11],14); B_R41(e1,a1,b1,c1,d1,w[10],15);
    B_R41(d1,e1,a1,b1,c1,w[0],14);  B_R41(c1,d1,e1,a1,b1,w[8],15);
    B_R41(b1,c1,d1,e1,a1,w[12],9);  B_R41(a1,b1,c1,d1,e1,w[4],8);
    B_R41(e1,a1,b1,c1,d1,w[13],9);  B_R41(d1,e1,a1,b1,c1,w[3],14);
    B_R41(c1,d1,e1,a1,b1,w[7],5);   B_R41(b1,c1,d1,e1,a1,w[15],6);
    B_R41(a1,b1,c1,d1,e1,w[14],8);  B_R41(e1,a1,b1,c1,d1,w[5],6);
    B_R41(d1,e1,a1,b1,c1,w[6],5);   B_R41(c1,d1,e1,a1,b1,w[2],12);

    B_R51(b1,c1,d1,e1,a1,w[4],9);   B_R51(a1,b1,c1,d1,e1,w[0],15);
    B_R51(e1,a1,b1,c1,d1,w[5],5);   B_R51(d1,e1,a1,b1,c1,w[9],11);
    B_R51(c1,d1,e1,a1,b1,w[7],6);   B_R51(b1,c1,d1,e1,a1,w[12],8);
    B_R51(a1,b1,c1,d1,e1,w[2],13);  B_R51(e1,a1,b1,c1,d1,w[10],12);
    B_R51(d1,e1,a1,b1,c1,w[14],5);  B_R51(c1,d1,e1,a1,b1,w[1],12);
    B_R51(b1,c1,d1,e1,a1,w[3],13);  B_R51(a1,b1,c1,d1,e1,w[8],14);
    B_R51(e1,a1,b1,c1,d1,w[11],11); B_R51(d1,e1,a1,b1,c1,w[6],8);
    B_R51(c1,d1,e1,a1,b1,w[15],5);  B_R51(b1,c1,d1,e1,a1,w[13],6);

    // Right rounds
    B_R12(a2,b2,c2,d2,e2,w[5],8);   B_R12(e2,a2,b2,c2,d2,w[14],9);
    B_R12(d2,e2,a2,b2,c2,w[7],9);   B_R12(c2,d2,e2,a2,b2,w[0],11);
    B_R12(b2,c2,d2,e2,a2,w[9],13);  B_R12(a2,b2,c2,d2,e2,w[2],15);
    B_R12(e2,a2,b2,c2,d2,w[11],15); B_R12(d2,e2,a2,b2,c2,w[4],5);
    B_R12(c2,d2,e2,a2,b2,w[13],7);  B_R12(b2,c2,d2,e2,a2,w[6],7);
    B_R12(a2,b2,c2,d2,e2,w[15],8);  B_R12(e2,a2,b2,c2,d2,w[8],11);
    B_R12(d2,e2,a2,b2,c2,w[1],14);  B_R12(c2,d2,e2,a2,b2,w[10],14);
    B_R12(b2,c2,d2,e2,a2,w[3],12);  B_R12(a2,b2,c2,d2,e2,w[12],6);

    B_R22(e2,a2,b2,c2,d2,w[6],9);   B_R22(d2,e2,a2,b2,c2,w[11],13);
    B_R22(c2,d2,e2,a2,b2,w[3],15);  B_R22(b2,c2,d2,e2,a2,w[7],7);
    B_R22(a2,b2,c2,d2,e2,w[0],12);  B_R22(e2,a2,b2,c2,d2,w[13],8);
    B_R22(d2,e2,a2,b2,c2,w[5],9);   B_R22(c2,d2,e2,a2,b2,w[10],11);
    B_R22(b2,c2,d2,e2,a2,w[14],7);  B_R22(a2,b2,c2,d2,e2,w[15],7);
    B_R22(e2,a2,b2,c2,d2,w[8],12);  B_R22(d2,e2,a2,b2,c2,w[12],7);
    B_R22(c2,d2,e2,a2,b2,w[4],6);   B_R22(b2,c2,d2,e2,a2,w[9],15);
    B_R22(a2,b2,c2,d2,e2,w[1],13);  B_R22(e2,a2,b2,c2,d2,w[2],11);

    B_R32(d2,e2,a2,b2,c2,w[15],9);  B_R32(c2,d2,e2,a2,b2,w[5],7);
    B_R32(b2,c2,d2,e2,a2,w[1],15);  B_R32(a2,b2,c2,d2,e2,w[3],11);
    B_R32(e2,a2,b2,c2,d2,w[7],8);   B_R32(d2,e2,a2,b2,c2,w[14],6);
    B_R32(c2,d2,e2,a2,b2,w[6],6);   B_R32(b2,c2,d2,e2,a2,w[9],14);
    B_R32(a2,b2,c2,d2,e2,w[11],12); B_R32(e2,a2,b2,c2,d2,w[8],13);
    B_R32(d2,e2,a2,b2,c2,w[12],5);  B_R32(c2,d2,e2,a2,b2,w[2],14);
    B_R32(b2,c2,d2,e2,a2,w[10],13); B_R32(a2,b2,c2,d2,e2,w[0],13);
    B_R32(e2,a2,b2,c2,d2,w[4],7);   B_R32(d2,e2,a2,b2,c2,w[13],5);

    B_R42(c2,d2,e2,a2,b2,w[8],15);  B_R42(b2,c2,d2,e2,a2,w[6],5);
    B_R42(a2,b2,c2,d2,e2,w[4],8);   B_R42(e2,a2,b2,c2,d2,w[1],11);
    B_R42(d2,e2,a2,b2,c2,w[3],14);  B_R42(c2,d2,e2,a2,b2,w[11],14);
    B_R42(b2,c2,d2,e2,a2,w[15],6);  B_R42(a2,b2,c2,d2,e2,w[0],14);
    B_R42(e2,a2,b2,c2,d2,w[5],6);   B_R42(d2,e2,a2,b2,c2,w[12],9);
    B_R42(c2,d2,e2,a2,b2,w[2],12);  B_R42(b2,c2,d2,e2,a2,w[13],9);
    B_R42(a2,b2,c2,d2,e2,w[9],12);  B_R42(e2,a2,b2,c2,d2,w[7],5);
    B_R42(d2,e2,a2,b2,c2,w[10],15); B_R42(c2,d2,e2,a2,b2,w[14],8);

    B_R52(b2,c2,d2,e2,a2,w[12],8);  B_R52(a2,b2,c2,d2,e2,w[15],5);
    B_R52(e2,a2,b2,c2,d2,w[10],12); B_R52(d2,e2,a2,b2,c2,w[4],9);
    B_R52(c2,d2,e2,a2,b2,w[1],12);  B_R52(b2,c2,d2,e2,a2,w[5],5);
    B_R52(a2,b2,c2,d2,e2,w[8],14);  B_R52(e2,a2,b2,c2,d2,w[7],6);
    B_R52(d2,e2,a2,b2,c2,w[6],8);   B_R52(c2,d2,e2,a2,b2,w[2],13);
    B_R52(b2,c2,d2,e2,a2,w[13],6);  B_R52(a2,b2,c2,d2,e2,w[14],5);
    B_R52(e2,a2,b2,c2,d2,w[0],15);  B_R52(d2,e2,a2,b2,c2,w[3],13);
    B_R52(c2,d2,e2,a2,b2,w[9],11);  B_R52(b2,c2,d2,e2,a2,w[11],11);

    __m256i t = s[0];
    s[0] = B_add3(s[1], c1, d2);
    s[1] = B_add3(s[2], d1, e2);
    s[2] = B_add3(s[3], e1, a2);
    s[3] = B_add3(s[4], a1, b2);
    s[4] = B_add3(t,    b1, c2);
  }

} // namespace ripemd160avx2

static const uint64_t avx2_sizedesc_32 = 32 << 3;
static const unsigned char avx2_pad[64] = { 0x80 };

// Extract lane i (0=low word .. 7=high word) of a __m256i as uint32_t.
// Stores all 8 lanes and indexes; compiler will optimize single-use to vmovd+vpextrd.
static inline uint32_t avx2_ripe_lane(const __m256i v, int i) {
  uint32_t tmp[8] __attribute__((aligned(32)));
  _mm256_store_si256((__m256i*)tmp, v);
  return tmp[i];
}

void ripemd160avx2_32(
  uint8_t *i0, uint8_t *i1, uint8_t *i2, uint8_t *i3,
  uint8_t *i4, uint8_t *i5, uint8_t *i6, uint8_t *i7,
  uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3,
  uint8_t *d4, uint8_t *d5, uint8_t *d6, uint8_t *d7)
{
  __m256i s[5] __attribute__((aligned(32)));

  // Append padding to each 32-byte SHA256 output before RIPEMD-160
  memcpy(i0 + 32, avx2_pad, 24); memcpy(i0 + 56, &avx2_sizedesc_32, 8);
  memcpy(i1 + 32, avx2_pad, 24); memcpy(i1 + 56, &avx2_sizedesc_32, 8);
  memcpy(i2 + 32, avx2_pad, 24); memcpy(i2 + 56, &avx2_sizedesc_32, 8);
  memcpy(i3 + 32, avx2_pad, 24); memcpy(i3 + 56, &avx2_sizedesc_32, 8);
  memcpy(i4 + 32, avx2_pad, 24); memcpy(i4 + 56, &avx2_sizedesc_32, 8);
  memcpy(i5 + 32, avx2_pad, 24); memcpy(i5 + 56, &avx2_sizedesc_32, 8);
  memcpy(i6 + 32, avx2_pad, 24); memcpy(i6 + 56, &avx2_sizedesc_32, 8);
  memcpy(i7 + 32, avx2_pad, 24); memcpy(i7 + 56, &avx2_sizedesc_32, 8);

  uint8_t *bs[8] = { i0, i1, i2, i3, i4, i5, i6, i7 };
  ripemd160avx2::Initialize(s);
  ripemd160avx2::Transform(s, bs);

  // Depack: s[word] is a __m256i holding all 8 lane values for that word.
  // Lane mapping in _mm256_set_epi32: first arg = lane 7, last arg = lane 0.
  // So blk[0] → lane7, blk[7] → lane0.
  // Output assignments: d0←i0←lane7, d7←i7←lane0.
  uint8_t *dd[8] = { d0, d1, d2, d3, d4, d5, d6, d7 };
  int lane_map[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };

  for (int li = 0; li < 8; li++) {
    int lane = lane_map[li];
    uint32_t *out = (uint32_t *)dd[li];
    uint32_t tmp0[8] __attribute__((aligned(32)));
    uint32_t tmp1[8] __attribute__((aligned(32)));
    uint32_t tmp2[8] __attribute__((aligned(32)));
    uint32_t tmp3[8] __attribute__((aligned(32)));
    uint32_t tmp4[8] __attribute__((aligned(32)));
    _mm256_store_si256((__m256i*)tmp0, s[0]);
    _mm256_store_si256((__m256i*)tmp1, s[1]);
    _mm256_store_si256((__m256i*)tmp2, s[2]);
    _mm256_store_si256((__m256i*)tmp3, s[3]);
    _mm256_store_si256((__m256i*)tmp4, s[4]);
    out[0] = tmp0[lane];
    out[1] = tmp1[lane];
    out[2] = tmp2[lane];
    out[3] = tmp3[lane];
    out[4] = tmp4[lane];
  }
}

#undef B_ROL
#undef B_NOT
#undef B_f1
#undef B_f2
#undef B_f3
#undef B_f4
#undef B_f5
#undef B_add3
#undef B_add4
#undef B_Round
#undef B_R11
#undef B_R21
#undef B_R31
#undef B_R41
#undef B_R51
#undef B_R12
#undef B_R22
#undef B_R32
#undef B_R42
#undef B_R52
#undef B_LOADW

#endif // __AVX2__
