/*
Tiny Mersenne Twister only 127 bit internal state

Copyright (C) 2011 Mutsuo Saito, Makoto Matsumoto,
Hiroshima University and The University of Tokyo.
All rights reserved.

The 3-clause BSD License
*/
#ifndef TINYMT32_H_
#define TINYMT32_H_

#include "util.h"

#define TINYMT32_MEXP 127
#define TINYMT32_SH0 1
#define TINYMT32_SH1 10
#define TINYMT32_SH8 8
#define TINYMT32_MASK 0x7fffffffu
#define TINYMT32_MUL (1.0f / 4294967296.0f)


typedef struct tinymt32 {
	uint32_t status[4];
	uint32_t mat1;
	uint32_t mat2;
	uint32_t tmat;
} tinymt32_t;


/**
 * This function changes internal state of tinymt32.
 * Users should not call this function directly.
 * @param random tinymt internal status
 */
static INLINE void tinymt32_next_state(tinymt32_t * random) {
	uint32_t x;
	uint32_t y;

	y = random->status[3];
	x = (random->status[0] & TINYMT32_MASK)
		^ random->status[1]
		^ random->status[2];
	x ^= (x << TINYMT32_SH0);
	y ^= (y >> TINYMT32_SH0) ^ x;
	random->status[0] = random->status[1];
	random->status[1] = random->status[2];
	random->status[2] = x ^ (y << TINYMT32_SH1);
	random->status[3] = y;
	random->status[1] ^= -((int32_t)(y & 1)) & random->mat1;
	random->status[2] ^= -((int32_t)(y & 1)) & random->mat2;
}

/**
 * This function outputs 32-bit unsigned integer from internal state.
 * Users should not call this function directly.
 * @param random tinymt internal status
 * @return 32-bit unsigned pseudorandom number
 */
static INLINE uint32_t tinymt32_temper(tinymt32_t * random) {
	uint32_t t0, t1;
	t0 = random->status[3];
	t1 = random->status[0]
		+ (random->status[2] >> TINYMT32_SH8);
	t0 ^= t1;
	t0 ^= -((int32_t)(t1 & 1)) & random->tmat;
	return t0;
}

/**
 * This function outputs 32-bit unsigned integer from internal state.
 * @param random tinymt internal status
 * @return 32-bit unsigned integer r (0 <= r < 2^32)
 */
static INLINE uint32_t tinymt32_generate_uint32(tinymt32_t * random) {
	tinymt32_next_state(random);
	return tinymt32_temper(random);
}

/**
 * This function outputs floating point number from internal state.
 * This function is implemented using multiplying by 1 / 2^32.
 * floating point multiplication is faster than using union trick in
 * my Intel CPU.
 * @param random tinymt internal status
 * @return floating point number r (0.0 <= r < 1.0)
 */
static INLINE float tinymt32_generate_float(tinymt32_t * random) {
	tinymt32_next_state(random);
	return tinymt32_temper(random) * TINYMT32_MUL;
}

void tinymt32_init(tinymt32_t *random, uint32_t seed);

#endif	/* TINYMT32_H_ */
