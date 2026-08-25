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

int shmfb_init(const char *path, int w, int h)
{
	int fd, create_shm;

	if(!(shmpath = strdup(path))) {
		fprintf(stderr, "shmfb_init: failed to allocate shm path buffer\n");
		return -1;
	}

	if((fd = shm_open(path, O_RDWR | O_CREAT, 0666)) == -1) {
		fprintf(stderr, "failed to open shared memory: %s: %s\n", path, strerror(errno));
		return -1;
	}

	if(w && h) {
		/* getting valid w and h implies we're creating and resizing a shared memory area */
		shmsz = CALC_SIZE(w, h);
		ftruncate(fd, shmsz);
		create_shm = 1;
	} else {
		/* otherwise map the header first, to get the size */
		shmsz = sizeof *shmfb;
		create_shm = 0;
	}

map:
	if((shmfb = mmap(0, shmsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (void*)-1) {
		fprintf(stderr, "failed to map shared memory %s (%d bytes): %s\n", path, shmsz,
				strerror(errno));
		shm_unlink(path);
		return -1;
	}

	if(create_shm) {
		/* it's a new shared memory area, we need to initialize things */
		shmfb->width = w;
		shmfb->height = h;

		sem_init(&shmfb->sem, 1, 1);

	} else {
		/* otherwise read size, and remap by jumping back */
		w = shmfb->width;
		h = shmfb->height;
		munmap(shmfb, shmsz);
		if(!w || !h) {
			fprintf(stderr, "shmfb_init: trying to map uninitialized shared memory area\n");
			shm_unlink(path);
			return -1;
		}
		shmsz = CALC_SIZE(w, h);
		goto map;
	}
	return 0;
}

void shm_destroy(void)
{
	if(!shmfb) return;

	sem_destroy(&shmfb->sem);

	munmap(shmfb, shmsz);

	if(shmpath) {
		shm_unlink(shmpath);
		shmpath = 0;
	}
	shmfb = 0;
	shmsz = 0;
}

void shmfb_start(int ntiles)
{
	sem_wait(&shmfb->sem);
	shmfb->done_tiles = 0;
	shmfb->total_tiles = ntiles;
	sem_post(&shmfb->sem);
}

void shmfb_donetile(void)
{
	sem_wait(&shmfb->sem);
	if(shmfb->done_tiles < shmfb->total_tiles) {
		shmfb->done_tiles++;
	}
	sem_post(&shmfb->sem);
}

int shmfb_rendering(void)
{
	return shmfb_pending();
}

int shmfb_pending(void)
{
	int res;
	sem_wait(&shmfb->sem);
	res = shmfb->total_tiles - shmfb->done_tiles;
	sem_post(&shmfb->sem);
	return res;
}

int shmfb_progress(void)
{
	int progr;
	sem_wait(&shmfb->sem);
	if(shmfb->total_tiles) {
		progr = shmfb->done_tiles / shmfb->total_tiles;
	} else {
		progr = 100;
	}
	sem_post(&shmfb->sem);
	return progr;
}
