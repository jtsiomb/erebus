#ifndef SHMFB_H_
#define SHMFB_H_

#include <semaphore.h>
#include "cgmath/cgmath.h"
#include "rt.h"

#define MAX_SHM_TILES	4096

struct sharedfb {
	int width, height;

	sem_t sem;
	int done_tiles, num_tiles;

	struct tile *done_list;
	struct tile tiles[MAX_SHM_TILES];

	cgm_vec4 pixels[1];
};

extern struct sharedfb *shmfb;


int shmfb_create(const char *path, int w, int h);
void shmfb_destroy(void);

int shmfb_map(const char *path);
void shmfb_unmap(void);

void shmfb_start(int ntiles);
void shmfb_donetile(struct tile *tile);
struct tile *shmfb_get_done(void);

int shmfb_rendering(void);		/* non-zero if currently rendering */
int shmfb_pending(void);		/* returns number of pending tiles */
int shmfb_progress(void);		/* returns progress [0, 100] */

#endif	/* SHMFB_H_ */
