#ifndef SHMFB_H_
#define SHMFB_H_

#include <semaphore.h>
#include "cgmath/cgmath.h"

struct sharedfb {
	int width, height;

	sem_t sem;
	int done_tiles, total_tiles;

	cgm_vec4 pixels[1];
};

extern struct sharedfb *shmfb;


int shmfb_init(const char *path, int w, int h);
void shmfb_destroy(void);

void shmfb_start(int ntiles);
void shmfb_donetile(void);

int shmfb_rendering(void);		/* non-zero if currently rendering */
int shmfb_pending(void);		/* returns number of pending tiles */
int shmfb_progress(void);		/* returns progress [0, 100] */

#endif	/* SHMFB_H_ */
