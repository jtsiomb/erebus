/*
Tiny Mersenne Twister only 127 bit internal state

Copyright (C) 2011 Mutsuo Saito, Makoto Matsumoto,
Hiroshima University and The University of Tokyo.
All rights reserved.

The 3-clause BSD License
*/
#include "tinymt32.h"

#define MIN_LOOP 8
#define PRE_LOOP 8

/**
 * This function certificate the period of 2^127-1.
 * @param random tinymt state vector.
 */
static void period_certification(tinymt32_t * random) {
	if ((random->status[0] & TINYMT32_MASK) == 0 &&
			random->status[1] == 0 &&
			random->status[2] == 0 &&
			random->status[3] == 0) {
		random->status[0] = 'T';
		random->status[1] = 'I';
		random->status[2] = 'N';
		random->status[3] = 'Y';
	}
}

/**
 * This function initializes the internal state array with a 32-bit
 * unsigned integer seed.
 * @param random tinymt state vector.
 * @param seed a 32-bit unsigned integer used as a seed.
 */
void tinymt32_init(tinymt32_t * random, uint32_t seed)
{
	int i;
	random->status[0] = seed;
	random->status[1] = random->mat1;
	random->status[2] = random->mat2;
	random->status[3] = random->tmat;
	for (i = 1; i < MIN_LOOP; i++) {
		random->status[i & 3] ^= i + 1812433253u
			* (random->status[(i - 1) & 3]
					^ (random->status[(i - 1) & 3] >> 30));
	}
	period_certification(random);
	for (i = 0; i < PRE_LOOP; i++) {
		tinymt32_next_state(random);
	}
}
