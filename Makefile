# Common flags
# x86-64 gets SSSE3 + AVX2 SIMD; ARM64 (Apple Silicon) uses native NEON via -march=native
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
  ARCH_FLAGS  = -m64 -mssse3 -mavx2
else ifeq ($(ARCH),amd64)
  ARCH_FLAGS  = -m64 -mssse3 -mavx2
else
  ARCH_FLAGS  =
endif
SIMD_FLAGS    = $(ARCH_FLAGS)
# Note: -march=native / -mtune=native omitted intentionally. Some virtualized
# environments (Docker / LXC / KVM) advertise host CPU features in /proc/cpuinfo
# that the guest cannot actually execute, causing silent hangs at startup.
# Explicit -mssse3 / -mavx2 from $(SIMD_FLAGS) covers the SIMD paths we rely on.
# For bare-metal builds where you want per-CPU tuning, prepend -march=native via:
#   make CFLAGS_COMMON="-march=native -mtune=native $(shell make -p | grep CFLAGS_COMMON)"
CFLAGS_COMMON = $(SIMD_FLAGS) -O3 -ffast-math -ftree-vectorize -flto -funroll-loops -fomit-frame-pointer
CFLAGS_WARN   = -Wall -Wextra -Wno-deprecated-copy
LDFLAGS       = -flto -lm -lpthread

default:
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c oldbloom/bloom.cpp -o oldbloom.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c bloom/bloom.cpp -o bloom.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -Wno-unused-parameter -c base58/base58.c -o base58.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -c rmd160/rmd160.c -o rmd160.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c sha3/sha3.c -o sha3.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c sha3/keccak.c -o keccak.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -c xxhash/xxhash.c -o xxhash.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c util.c -o util.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/Int.cpp -o Int.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/Point.cpp -o Point.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/SECP256K1.cpp -o SECP256K1.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/IntMod.cpp -o IntMod.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/Random.cpp -o Random.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/IntGroup.cpp -o IntGroup.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o hash/ripemd160.o -c hash/ripemd160.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o hash/sha256.o -c hash/sha256.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o hash/ripemd160_sse.o -c hash/ripemd160_sse.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o hash/sha256_sse.o -c hash/sha256_sse.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o keyhunt keyhunt.cpp base58.o rmd160.o hash/ripemd160.o hash/ripemd160_sse.o hash/sha256.o hash/sha256_sse.o bloom.o oldbloom.o xxhash.o util.o Int.o Point.o SECP256K1.o IntMod.o Random.o IntGroup.o sha3.o keccak.o $(LDFLAGS)
	rm -f *.o

# AVX-512 variant (x86-64 only: Ice Lake / Sapphire Rapids — may throttle on some Xeons)
avx512:
	$(MAKE) ARCH_FLAGS="-m64 -mssse3 -mavx2 -mavx512f -mavx512bw -mavx512vl"

# PGO step 1: build with instrumentation
pgo-generate:
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c oldbloom/bloom.cpp -o oldbloom.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c bloom/bloom.cpp -o bloom.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -Wno-unused-parameter -fprofile-generate -c base58/base58.c -o base58.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -fprofile-generate -c rmd160/rmd160.c -o rmd160.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c sha3/sha3.c -o sha3.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c sha3/keccak.c -o keccak.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -fprofile-generate -c xxhash/xxhash.c -o xxhash.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c util.c -o util.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c secp256k1/Int.cpp -o Int.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c secp256k1/Point.cpp -o Point.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c secp256k1/SECP256K1.cpp -o SECP256K1.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c secp256k1/IntMod.cpp -o IntMod.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c secp256k1/Random.cpp -o Random.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -c secp256k1/IntGroup.cpp -o IntGroup.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -o hash/ripemd160.o -c hash/ripemd160.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -o hash/sha256.o -c hash/sha256.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -o hash/ripemd160_sse.o -c hash/ripemd160_sse.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -o hash/sha256_sse.o -c hash/sha256_sse.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-generate -o keyhunt keyhunt.cpp base58.o rmd160.o hash/ripemd160.o hash/ripemd160_sse.o hash/sha256.o hash/sha256_sse.o bloom.o oldbloom.o xxhash.o util.o Int.o Point.o SECP256K1.o IntMod.o Random.o IntGroup.o sha3.o keccak.o $(LDFLAGS)
	rm -f *.o
	@echo "Run a representative workload now, e.g.:"
	@echo "  ./keyhunt -t 4 -m address -f addresses.txt -r 1:FFFFFFFF -n 2000000"
	@echo "Then run: make pgo-use"

# PGO step 2: rebuild using profile data
pgo-use:
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c oldbloom/bloom.cpp -o oldbloom.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c bloom/bloom.cpp -o bloom.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -Wno-unused-parameter -fprofile-use -fprofile-correction -c base58/base58.c -o base58.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -fprofile-use -fprofile-correction -c rmd160/rmd160.c -o rmd160.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c sha3/sha3.c -o sha3.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c sha3/keccak.c -o keccak.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -fprofile-use -fprofile-correction -c xxhash/xxhash.c -o xxhash.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c util.c -o util.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c secp256k1/Int.cpp -o Int.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c secp256k1/Point.cpp -o Point.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c secp256k1/SECP256K1.cpp -o SECP256K1.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c secp256k1/IntMod.cpp -o IntMod.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c secp256k1/Random.cpp -o Random.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -c secp256k1/IntGroup.cpp -o IntGroup.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -o hash/ripemd160.o -c hash/ripemd160.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -o hash/sha256.o -c hash/sha256.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -o hash/ripemd160_sse.o -c hash/ripemd160_sse.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -o hash/sha256_sse.o -c hash/sha256_sse.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -fprofile-use -fprofile-correction -o keyhunt keyhunt.cpp base58.o rmd160.o hash/ripemd160.o hash/ripemd160_sse.o hash/sha256.o hash/sha256_sse.o bloom.o oldbloom.o xxhash.o util.o Int.o Point.o SECP256K1.o IntMod.o Random.o IntGroup.o sha3.o keccak.o $(LDFLAGS)
	rm -f *.o

clean:
	rm -f keyhunt bsgsd *.o *.gcda *.gcno

legacy:
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -flto -c oldbloom/bloom.cpp -o oldbloom.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -flto -c bloom/bloom.cpp -o bloom.o
	gcc -march=native -mtune=native -Wno-unused-result -Ofast -ftree-vectorize -c base58/base58.c -o base58.o
	gcc -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c xxhash/xxhash.c -o xxhash.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c util.c -o util.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c sha3/sha3.c -o sha3.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c sha3/keccak.c -o keccak.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c hashing.c -o hashing.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c gmp256k1/Int.cpp -o Int.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c gmp256k1/Point.cpp -o Point.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c gmp256k1/GMP256K1.cpp -o GMP256K1.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -c gmp256k1/IntMod.cpp -o IntMod.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -flto -c gmp256k1/Random.cpp -o Random.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -flto -c gmp256k1/IntGroup.cpp -o IntGroup.o
	g++ -march=native -mtune=native -Wall -Wextra -Ofast -ftree-vectorize -o keyhunt keyhunt_legacy.cpp base58.o bloom.o oldbloom.o xxhash.o util.o Int.o Point.o GMP256K1.o IntMod.o IntGroup.o Random.o hashing.o sha3.o keccak.o -lm -lpthread -lcrypto -lgmp
	rm -f *.o

bsgsd:
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c oldbloom/bloom.cpp -o oldbloom.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c bloom/bloom.cpp -o bloom.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -Wno-unused-parameter -c base58/base58.c -o base58.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -c rmd160/rmd160.c -o rmd160.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c sha3/sha3.c -o sha3.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c sha3/keccak.c -o keccak.o
	gcc $(CFLAGS_COMMON) -Wall -Wextra -c xxhash/xxhash.c -o xxhash.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c util.c -o util.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/Int.cpp -o Int.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/Point.cpp -o Point.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/SECP256K1.cpp -o SECP256K1.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/IntMod.cpp -o IntMod.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/Random.cpp -o Random.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -c secp256k1/IntGroup.cpp -o IntGroup.o
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o hash/ripemd160.o -c hash/ripemd160.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o hash/sha256.o -c hash/sha256.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o hash/ripemd160_sse.o -c hash/ripemd160_sse.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o hash/sha256_sse.o -c hash/sha256_sse.cpp
	g++ $(CFLAGS_COMMON) $(CFLAGS_WARN) -o bsgsd bsgsd.cpp base58.o rmd160.o hash/ripemd160.o hash/ripemd160_sse.o hash/sha256.o hash/sha256_sse.o bloom.o oldbloom.o xxhash.o util.o Int.o Point.o SECP256K1.o IntMod.o Random.o IntGroup.o sha3.o keccak.o $(LDFLAGS)
	rm -f *.o
