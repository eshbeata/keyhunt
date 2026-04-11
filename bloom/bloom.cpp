/*
 *  Copyright (c) 2012-2019, Jyri J. Virkki
 *  All rights reserved.
 *
 *  This file is under BSD license. See LICENSE file.
 */

/*
 * Refer to bloom.h for documentation on the public interfaces.
 */

#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#ifndef _WIN64
#include <sys/mman.h>
#endif

#include "bloom.h"
#define XXH_INLINE_ALL
#include "../xxhash/xxhash.h"

#define MAKESTRING(n) STRING(n)
#define STRING(n) #n
#define BLOOM_MAGIC "libbloom2"
#define BLOOM_VERSION_MAJOR 2
#define BLOOM_VERSION_MINOR 201

inline static int test_bit_set_bit(uint8_t *bf, uint64_t bit, int set_bit)
{
  uint64_t byte = bit >> 3;
  uint8_t c = bf[byte];	 // expensive memory access
  uint8_t mask = 1 << (bit % 8);
  if (c & mask) {
    return 1;
  } else {
    if (set_bit) {
		bf[byte] = c | mask;
    }
    return 0;
  }
}

inline static int test_bit(uint8_t *bf, uint64_t bit)
{
  uint64_t byte = bit >> 3;
  uint8_t c = bf[byte];	 // expensive memory access
  uint8_t mask = 1 << (bit % 8);
  if (c & mask) {
    return 1;
  } else {
    return 0;
  }
}

static int bloom_check_add(struct bloom * bloom, const void * buffer, int len, int add)
{
  if (bloom->ready == 0) {
    printf("bloom at %p not initialized!\n", (void *)bloom);
    return -1;
  }

  // Single XXH3_128 call replaces two XXH64 calls (2x speedup on hashing)
  XXH128_hash_t h128 = XXH3_128bits_withSeed(buffer, (size_t)len, 0x59f2815b16f81798ULL);
  uint64_t a = h128.low64;
  uint64_t b = h128.high64;

  uint8_t hits = 0;
  uint64_t x;
  uint8_t i;

  if (add) {
    // For add: no need to prefetch, just set bits
    for (i = 0; i < bloom->hashes; i++) {
      x = bloom->use_mask ? ((a + b*(uint64_t)i) & bloom->bits_mask)
                          : ((a + b*(uint64_t)i) % bloom->bits);
      if (test_bit_set_bit(bloom->bf, x, 1)) {
        hits++;
      }
    }
    return (hits == bloom->hashes) ? 1 : 0;
  } else {
    // Two-phase prefetch for check: compute all positions, prefetch cache lines,
    // then test. Hides memory latency for large bloom filters.
    uint64_t positions[32]; // max supported hashes
    for (i = 0; i < bloom->hashes; i++) {
      positions[i] = bloom->use_mask ? ((a + b*(uint64_t)i) & bloom->bits_mask)
                                     : ((a + b*(uint64_t)i) % bloom->bits);
      __builtin_prefetch(bloom->bf + (positions[i] >> 3), 0, 1);
    }
    for (i = 0; i < bloom->hashes; i++) {
      uint8_t mask = (uint8_t)(1 << (positions[i] & 7));
      if (!(bloom->bf[positions[i] >> 3] & mask)) {
        return 0;
      }
    }
    return 1;
  }
}

// DEPRECATED - Please migrate to bloom_init2.
int bloom_init(struct bloom * bloom, uint64_t entries, long double error)
{
  return bloom_init2(bloom, entries, error);
}

int bloom_init2(struct bloom * bloom, uint64_t entries, long double error)
{
  memset(bloom, 0, sizeof(struct bloom));
  if (entries < 1000 || error <= 0 || error >= 1) {
    return 1;
  }
  bloom->entries = entries;
  bloom->error = error;

  long double num = -log(bloom->error);
  long double denom = 0.480453013918201; // ln(2)^2
  bloom->bpe = (num / denom);

  long double dentries = (long double)entries;
  long double allbits = dentries * bloom->bpe;
  bloom->bits = (uint64_t)allbits;

  // Round bits up to the next power of 2 so we can use bitwise AND
  // instead of modulo in the hot path (eliminates integer division).
  // Only round up when inflation is ≤ 12.5% (next power-of-2 is within 8/7 of current).
  // For large BSGS bloom filters (hundreds of MB each), unconditional rounding can
  // double memory usage and evict the filter from cache — worse than the modulo cost.
  bloom->use_mask = 0;
  bloom->bits_mask = 0;
  if (bloom->bits & (bloom->bits - 1)) {
    uint64_t p = 1;
    while (p < bloom->bits) p <<= 1;
    // Only accept the rounding if it inflates by at most 12.5%
    if (p <= bloom->bits + (bloom->bits >> 3)) {
      bloom->bits = p;
      bloom->bits_mask = bloom->bits - 1;
      bloom->use_mask  = 1;
    }
  } else {
    // Already a power of 2
    bloom->bits_mask = bloom->bits - 1;
    bloom->use_mask  = 1;
  }

  bloom->bytes = bloom->bits / 8;

  bloom->hashes = (uint8_t)ceil(0.693147180559945 * bloom->bpe);  // ln(2)
  if (bloom->hashes < 1) bloom->hashes = 1;

  bloom->bf = (uint8_t *)calloc(bloom->bytes, sizeof(uint8_t));
  if (bloom->bf == NULL) {                                   // LCOV_EXCL_START
    return 1;
  }                                                          // LCOV_EXCL_STOP

  bloom->ready = 1;
  bloom->mmap_flag = 0;
  bloom->major = BLOOM_VERSION_MAJOR;
  bloom->minor = BLOOM_VERSION_MINOR;
  return 0;
}

int bloom_check(struct bloom * bloom, const void * buffer, int len)
{
  if (bloom->ready == 0) {
    printf("bloom at %p not initialized!\n", (void *)bloom);
    return -1;
  }

  // Single XXH3_128 replaces two XXH64 calls
  XXH128_hash_t h128 = XXH3_128bits_withSeed(buffer, (size_t)len, 0x59f2815b16f81798ULL);
  uint64_t a = h128.low64;
  uint64_t b = h128.high64;

  // Two-phase: compute all bit positions, prefetch cache lines, then test
  uint64_t positions[32];
  uint8_t i;
  for (i = 0; i < bloom->hashes; i++) {
    positions[i] = bloom->use_mask ? ((a + b*(uint64_t)i) & bloom->bits_mask)
                                   : ((a + b*(uint64_t)i) % bloom->bits);
    __builtin_prefetch(bloom->bf + (positions[i] >> 3), 0, 1);
  }
  for (i = 0; i < bloom->hashes; i++) {
    uint8_t mask = (uint8_t)(1 << (positions[i] & 7));
    if (!(bloom->bf[positions[i] >> 3] & mask)) {
      return 0;
    }
  }
  return 1;
}


int bloom_add(struct bloom * bloom, const void * buffer, int len)
{
  return bloom_check_add(bloom, buffer, len, 1);
}

void bloom_print(struct bloom * bloom)
{
  printf("bloom at %p\n", (void *)bloom);
  if (!bloom->ready) { printf(" *** NOT READY ***\n"); }
  printf(" ->version = %d.%d\n", bloom->major, bloom->minor);
  printf(" ->entries = %" PRIu64 "\n", bloom->entries);
  printf(" ->error = %Lf\n", bloom->error);
  printf(" ->bits = %" PRIu64 "\n", bloom->bits);
  printf(" ->bits per elem = %f\n", bloom->bpe);
  printf(" ->bytes = %" PRIu64 "\n", bloom->bytes);
  unsigned int KB = bloom->bytes / 1024;
  unsigned int MB = KB / 1024;
  printf(" (%u KB, %u MB)\n", KB, MB);
  printf(" ->hash functions = %d\n", bloom->hashes);
}

void bloom_free(struct bloom * bloom)
{
  if (bloom->ready) {
#ifndef _WIN64
    if (bloom->mmap_flag) {
      munmap(bloom->bf, bloom->bytes);
    } else {
      free(bloom->bf);
    }
#else
    free(bloom->bf);
#endif
  }
  bloom->ready = 0;
  bloom->mmap_flag = 0;
}

int bloom_reset(struct bloom * bloom)
{
  if (!bloom->ready) return 1;
  memset(bloom->bf, 0, bloom->bytes);
  return 0;
}
/*
int bloom_save(struct bloom * bloom, char * filename)
{
  if (filename == NULL || filename[0] == 0) {
    return 1;
  }

  int fd = open(filename, O_WRONLY | O_CREAT, 0644);
  if (fd < 0) {
    return 1;
  }

  ssize_t out = write(fd, BLOOM_MAGIC, strlen(BLOOM_MAGIC));
  if (out != strlen(BLOOM_MAGIC)) { goto save_error; }        // LCOV_EXCL_LINE

  uint16_t size = sizeof(struct bloom);
  out = write(fd, &size, sizeof(uint16_t));
  if (out != sizeof(uint16_t)) { goto save_error; }           // LCOV_EXCL_LINE

  out = write(fd, bloom, sizeof(struct bloom));
  if (out != sizeof(struct bloom)) { goto save_error; }       // LCOV_EXCL_LINE

  out = write(fd, bloom->bf, bloom->bytes);
  if (out != bloom->bytes) { goto save_error; }               // LCOV_EXCL_LINE

  close(fd);
  return 0;
                                                             // LCOV_EXCL_START
 save_error:
  close(fd);
  return 1;
                                                             // LCOV_EXCL_STOP
}


int bloom_load(struct bloom * bloom, char * filename)
{
  int rv = 0;

  if (filename == NULL || filename[0] == 0) { return 1; }
  if (bloom == NULL) { return 2; }

  memset(bloom, 0, sizeof(struct bloom));

  int fd = open(filename, O_RDONLY);
  if (fd < 0) { return 3; }

  char line[30];
  memset(line, 0, 30);
  ssize_t in = read(fd, line, strlen(BLOOM_MAGIC));

  if (in != strlen(BLOOM_MAGIC)) {
    rv = 4;
    goto load_error;
  }

  if (strncmp(line, BLOOM_MAGIC, strlen(BLOOM_MAGIC))) {
    rv = 5;
    goto load_error;
  }

  uint16_t size;
  in = read(fd, &size, sizeof(uint16_t));
  if (in != sizeof(uint16_t)) {
    rv = 6;
    goto load_error;
  }

  if (size != sizeof(struct bloom)) {
    rv = 7;
    goto load_error;
  }

  in = read(fd, bloom, sizeof(struct bloom));
  if (in != sizeof(struct bloom)) {
    rv = 8;
    goto load_error;
  }

  bloom->bf = NULL;
  if (bloom->major != BLOOM_VERSION_MAJOR) {
    rv = 9;
    goto load_error;
  }

  bloom->bf = (unsigned char *)malloc(bloom->bytes);
  if (bloom->bf == NULL) { rv = 10; goto load_error; }        // LCOV_EXCL_LINE

  in = read(fd, bloom->bf, bloom->bytes);
  if (in != bloom->bytes) {
    rv = 11;
    free(bloom->bf);
    bloom->bf = NULL;
    goto load_error;
  }

  close(fd);
  return rv;

 load_error:
  close(fd);
  bloom->ready = 0;
  return rv;
}
*/

// Binary file format for bloom_save_file / bloom_load_file / bloom_load_mmap:
//   [0..3]   magic "BLM2"
//   [4..11]  entries  (uint64_t, little-endian)
//   [12..19] bits     (uint64_t)
//   [20..27] bytes    (uint64_t)
//   [28]     hashes   (uint8_t)
//   [29]     use_mask (uint8_t)
//   [30..37] bits_mask (uint64_t)
//   [38..45] error    (double)
//   [46..47] padding (2 bytes)
//   [48..]   bf data  (bloom->bytes bytes)
#define BLOOM_FILE_MAGIC "BLM2"
#define BLOOM_FILE_HEADER_SIZE 48

int bloom_save_file(struct bloom *bloom, const char *filename)
{
  if (!bloom->ready || !filename) return 1;
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return 1;

  uint8_t header[BLOOM_FILE_HEADER_SIZE];
  memset(header, 0, sizeof(header));
  memcpy(header, BLOOM_FILE_MAGIC, 4);
  memcpy(header + 4,  &bloom->entries,   8);
  memcpy(header + 12, &bloom->bits,      8);
  memcpy(header + 20, &bloom->bytes,     8);
  header[28] = bloom->hashes;
  header[29] = bloom->use_mask;
  memcpy(header + 30, &bloom->bits_mask, 8);
  double err = (double)bloom->error;
  memcpy(header + 38, &err,              8);

  ssize_t out = write(fd, header, BLOOM_FILE_HEADER_SIZE);
  if (out != BLOOM_FILE_HEADER_SIZE) { close(fd); return 1; }
  out = write(fd, bloom->bf, bloom->bytes);
  if (out != (ssize_t)bloom->bytes) { close(fd); return 1; }
  close(fd);
  return 0;
}

int bloom_load_file(struct bloom *bloom, const char *filename)
{
  if (!bloom || !filename) return 1;
  memset(bloom, 0, sizeof(struct bloom));
  int fd = open(filename, O_RDONLY);
  if (fd < 0) return 2;

  uint8_t header[BLOOM_FILE_HEADER_SIZE];
  if (read(fd, header, BLOOM_FILE_HEADER_SIZE) != BLOOM_FILE_HEADER_SIZE) { close(fd); return 3; }
  if (memcmp(header, BLOOM_FILE_MAGIC, 4) != 0) { close(fd); return 4; }

  memcpy(&bloom->entries,   header + 4,  8);
  memcpy(&bloom->bits,      header + 12, 8);
  memcpy(&bloom->bytes,     header + 20, 8);
  bloom->hashes   = header[28];
  bloom->use_mask = header[29];
  memcpy(&bloom->bits_mask, header + 30, 8);
  double err;
  memcpy(&err, header + 38, 8);
  bloom->error = (long double)err;

  bloom->bf = (uint8_t *)malloc(bloom->bytes);
  if (!bloom->bf) { close(fd); return 5; }
  if (read(fd, bloom->bf, bloom->bytes) != (ssize_t)bloom->bytes) {
    free(bloom->bf); bloom->bf = NULL; close(fd); return 6;
  }
  close(fd);
  bloom->ready = 1;
  bloom->mmap_flag = 0;
  bloom->major = BLOOM_VERSION_MAJOR;
  bloom->minor = BLOOM_VERSION_MINOR;
  return 0;
}

#ifndef _WIN64
int bloom_load_mmap(struct bloom *bloom, const char *filename)
{
  if (!bloom || !filename) return 1;
  memset(bloom, 0, sizeof(struct bloom));
  int fd = open(filename, O_RDONLY);
  if (fd < 0) return 2;

  uint8_t header[BLOOM_FILE_HEADER_SIZE];
  if (read(fd, header, BLOOM_FILE_HEADER_SIZE) != BLOOM_FILE_HEADER_SIZE) { close(fd); return 3; }
  if (memcmp(header, BLOOM_FILE_MAGIC, 4) != 0) { close(fd); return 4; }

  memcpy(&bloom->entries,   header + 4,  8);
  memcpy(&bloom->bits,      header + 12, 8);
  memcpy(&bloom->bytes,     header + 20, 8);
  bloom->hashes   = header[28];
  bloom->use_mask = header[29];
  memcpy(&bloom->bits_mask, header + 30, 8);
  double err;
  memcpy(&err, header + 38, 8);
  bloom->error = (long double)err;

  // mmap the bit array read-only.
  // MAP_POPULATE (Linux-only) pre-faults all pages into RAM; on other POSIX
  // systems we fall back to plain MAP_SHARED and advise random access.
#ifdef MAP_POPULATE
  int mmap_flags = MAP_SHARED | MAP_POPULATE;
#else
  int mmap_flags = MAP_SHARED;
#endif
  bloom->bf = (uint8_t *)mmap(NULL, bloom->bytes, PROT_READ,
                               mmap_flags, fd, BLOOM_FILE_HEADER_SIZE);
  if (bloom->bf == MAP_FAILED) {
    bloom->bf = NULL; close(fd); return 5;
  }
  // Advise the kernel this will be accessed randomly
  madvise(bloom->bf, bloom->bytes, MADV_RANDOM);

  close(fd);  // fd can be closed after mmap
  bloom->ready = 1;
  bloom->mmap_flag = 1;
  bloom->major = BLOOM_VERSION_MAJOR;
  bloom->minor = BLOOM_VERSION_MINOR;
  return 0;
}
#else
int bloom_load_mmap(struct bloom *bloom, const char *filename)
{
  // Fallback to regular load on Windows
  return bloom_load_file(bloom, filename);
}
#endif

const char * bloom_version()
{
  return MAKESTRING(BLOOM_VERSION);
}
