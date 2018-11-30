/* Tiny Mersenne Twister only 127 bit internal state
 *
 * Copyright (C) 2011 Mutsuo Saito, Makoto Matsumoto,
 * Hiroshima University and The University of Tokyo.
 * All rights reserved.
 */
#ifndef TINYMT_H_
#define TIMYMT_H_

#include <stdint.h>

typedef struct tinymt32 {
	uint32_t status[4];
	uint32_t mat1;
	uint32_t mat2;
	uint32_t tmat;
} tinymt32_t;

void tinymt32_init(tinymt32_t *random, uint32_t seed);
uint32_t tinymt32_generate_uint32(tinymt32_t *random);
float tinymt32_generate_float(tinymt32_t *random);

#endif	/* TINYMT_H_ */
