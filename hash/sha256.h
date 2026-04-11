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

#ifndef SHA256_H
#define SHA256_H
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

void sha256(uint8_t *input,size_t length, uint8_t *digest);
void sha256_33(uint8_t *input, uint8_t *digest);
void sha256_65(uint8_t *input, uint8_t *digest);
void sha256_checksum(uint8_t *input, int length, uint8_t *checksum);
bool sha256_file(const char* file_name, uint8_t* checksum);
void sha256sse_1B(uint32_t *i0, uint32_t *i1, uint32_t *i2, uint32_t *i3,
  uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3);
void sha256sse_2B(uint32_t *i0, uint32_t *i1, uint32_t *i2, uint32_t *i3,
  uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3);
void sha256sse_checksum(uint32_t *i0, uint32_t *i1, uint32_t *i2, uint32_t *i3,
  uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3);
std::string sha256_hex(unsigned char *digest);
void sha256sse_test();

#ifdef __AVX2__
// 8-wide AVX2 SHA256 — processes 8 messages in parallel.
// i0..i7: pre-formatted uint32_t message word arrays (big-endian padded).
// d0..d7: 32-byte output digest buffers (must be 16-byte aligned for SSE stores).
// _1B variant: single block (33-byte compressed pubkey, padded to 64 bytes).
// _2B variant: two blocks (65-byte uncompressed pubkey, padded to 128 bytes).
void sha256avx2_1B(
  uint32_t *i0, uint32_t *i1, uint32_t *i2, uint32_t *i3,
  uint32_t *i4, uint32_t *i5, uint32_t *i6, uint32_t *i7,
  uint8_t  *d0, uint8_t  *d1, uint8_t  *d2, uint8_t  *d3,
  uint8_t  *d4, uint8_t  *d5, uint8_t  *d6, uint8_t  *d7);
void sha256avx2_2B(
  uint32_t *i0, uint32_t *i1, uint32_t *i2, uint32_t *i3,
  uint32_t *i4, uint32_t *i5, uint32_t *i6, uint32_t *i7,
  uint8_t  *d0, uint8_t  *d1, uint8_t  *d2, uint8_t  *d3,
  uint8_t  *d4, uint8_t  *d5, uint8_t  *d6, uint8_t  *d7);
#endif // __AVX2__

#endif