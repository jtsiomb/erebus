#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "shmfb.h"

struct sharedfb *shmfb;

static int shmsz;
static char *shmpath;

#define CALC_SIZE(w, h)	(sizeof *shmfb + ((w) * (h) - 1) * sizeof *shmfb->pixels)

/* create a new shared memory area, sized for a wxh framebuffer, and initialize
 * everything in it.
 */
int shmfb_create(const char *path, int w, int h)
{
	int fd;

	if(!(shmpath = strdup(path))) {
		fprintf(stderr, "shmfb_create: failed to allocate shm path buffer\n");
		return -1;
	}

	if((fd = shm_open(path, O_RDWR | O_CREAT, 0666)) == -1) {
		fprintf(stderr, "shmfb_create: failed to open shared memory: %s: %s\n", path, strerror(errno));
		return -1;
	}

	shmsz = CALC_SIZE(w, h);
	ftruncate(fd, shmsz);

	if((shmfb = mmap(0, shmsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (void*)-1) {
		fprintf(stderr, "shmfb_create: failed to map shared memory %s (%d bytes): %s\n",
				path, shmsz, strerror(errno));
		shm_unlink(path);
		return -1;
	}

	shmfb->width = w;
	shmfb->height = h;
	shmfb->num_done = shmfb->num_tiles = 0;
	shmfb->done_list = -1;

	sem_init(&shmfb->sem, 1, 1);
	return 0;
}

/* map an existing shared memory framebuffer, first a small header to find out
 * the framebuffer dimensions, then remap the correct size.
 */
int shmfb_map(const char *path)
{
	int fd, width, height;

	if(!(shmpath = strdup(path))) {
		fprintf(stderr, "shmfb_map: failed to allocate shm path buffer\n");
		return -1;
	}

	if((fd = shm_open(path, O_RDWR, 0)) == -1) {
		fprintf(stderr, "shmfb_map: failed to open shared memory: %s: %s\n", path, strerror(errno));
		return -1;
	}

	shmsz = sizeof *shmfb;

	if((shmfb = mmap(0, shmsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (void*)-1) {
		fprintf(stderr, "shmfb_map: failed to map shared memory %s (%d bytes): %s\n",
				path, shmsz, strerror(errno));
		shm_unlink(path);
		return -1;
	}

	width = shmfb->width;
	height = shmfb->height;
	if(width <= 0 || height <= 0) {
		fprintf(stderr, "shmfb_map: shared memory header contains invalid dimensions\n");
		munmap(shmfb, shmsz);
		shm_unlink(path);
		return -1;
	}
	munmap(shmfb, shmsz);
	shmsz = CALC_SIZE(width, height);

	if((shmfb = mmap(0, shmsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (void*)-1) {
		fprintf(stderr, "shmfb_map: failed to map shared memory %s (%d bytes): %s\n",
				path, shmsz, strerror(errno));
		shm_unlink(path);
		return -1;
	}
	return 0;
}

void shmfb_unmap(void)
{
	if(!shmfb) return;

	munmap(shmfb, shmsz);
	if(shmpath) {
		shm_unlink(shmpath);
		shmpath = 0;
	}
	shmfb = 0;
	shmsz = 0;
}

void shmfb_destroy(void)
{
	if(!shmfb) return;

	sem_destroy(&shmfb->sem);
	shmfb_unmap();
}

void shmfb_lock(void)
{
	sem_wait(&shmfb->sem);
}

void shmfb_unlock(void)
{
	sem_post(&shmfb->sem);
}

void shmfb_start(int ntiles)
{
	sem_wait(&shmfb->sem);
	shmfb->num_done = 0;
	shmfb->num_tiles = ntiles;
	shmfb->done_list = -1;
	memset(shmfb->act_tiles, 0, sizeof shmfb->act_tiles);
	sem_post(&shmfb->sem);
}

static unsigned char zero;

void shmfb_tile_start(struct tile *tile)
{
	int tidx = tile - shmfb->tiles;
	int bmidx = tidx >> 5;
	int bit = tidx & 31;

	sem_wait(&shmfb->sem);
	shmfb->act_tiles[bmidx] |= 1 << bit;
	sem_post(&shmfb->sem);

	write(2, &zero, 1);
}

void shmfb_tile_done(struct tile *tile)
{
	int tidx = tile - shmfb->tiles;
	int bmidx = tidx >> 5;
	int bit = tidx & 31;

	sem_wait(&shmfb->sem);
	if(shmfb->num_done < shmfb->num_tiles) {
		shmfb->num_done++;

		tile->next = shmfb->done_list;
		shmfb->done_list = tile - shmfb->tiles;
	}
	shmfb->act_tiles[bmidx] &= ~(1 << bit);
	sem_post(&shmfb->sem);

	write(2, &zero, 1);
}

int shmfb_get_donelist(void)
{
	int list;
	sem_wait(&shmfb->sem);
	list = shmfb->done_list;
	shmfb->done_list = -1;
	sem_post(&shmfb->sem);
	return list;
}

int shmfb_rendering(void)
{
	return shmfb_pending();
}

int shmfb_pending(void)
{
	int res;
	sem_wait(&shmfb->sem);
	res = shmfb->num_tiles - shmfb->num_done;
	sem_post(&shmfb->sem);
	return res;
}

int shmfb_progress(void)
{
	int progr;
	sem_wait(&shmfb->sem);
	if(shmfb->num_tiles) {
		progr = (shmfb->num_done << 10) / shmfb->num_tiles;
	} else {
		progr = 1024;
	}
	sem_post(&shmfb->sem);
	return progr;
}
