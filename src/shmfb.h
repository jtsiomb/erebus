#ifndef SHMFB_H_
#define SHMFB_H_

#include <semaphore.h>
#include "cgmath/cgmath.h"
#include "rt.h"

#define MAX_SHM_TILES	4096
#define TILES_BM_LEN	((MAX_SHM_TILES + 31) / 32)

struct sharedfb {
	int width, height;

	sem_t sem;
	int cur_sample, total_samples;
	int total_done, total_tiles;	/* total over the whole render */

	struct tile tiles[MAX_SHM_TILES];

	int num_done, num_active;
	uint32_t done_tiles[TILES_BM_LEN];
	uint32_t act_tiles[TILES_BM_LEN];

	cgm_vec4 pixels[1];
};

extern struct sharedfb *shmfb;


int shmfb_create(const char *path, int w, int h);
void shmfb_destroy(void);

void shmfb_lock(void);
void shmfb_unlock(void);

int shmfb_map(const char *path);
void shmfb_unmap(void);

void shmfb_start(int ntiles);
void shmfb_tile_start(struct tile *tile);
void shmfb_tile_done(struct tile *tile);

int shmfb_rendering(void);		/* non-zero if currently rendering */
int shmfb_pending(void);		/* returns number of pending tiles */
int shmfb_progress(void);		/* returns progress [0, 100] */

#endif	/* SHMFB_H_ */
