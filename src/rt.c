#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "rt.h"
#include "rend.h"
#include "tpool.h"

#define BLOCK_SIZE	32

static void render_block(void *bp);
static void done_block(void *bp);
static struct rt_block *alloc_block(void);
static void free_block(struct rt_block *blk);

static struct thread_pool *tpool;

static struct rt_block *donelist, *donelist_tail;
static pthread_mutex_t donelist_lock = PTHREAD_MUTEX_INITIALIZER;

static int debug;

int rt_init(int width, int height)
{
	char *env;

	if((env = getenv("RTW_DEBUG")) && atoi(env)) {
		debug = 1;
	}

	fbwidth = width;
	fbheight = height;
	if(!(fbpixels = calloc(width * height * 3, sizeof *fbpixels))) {
		return -1;
	}

	if(init_rend() == -1) {
		free(fbpixels);
		fbpixels = 0;
		return -1;
	}

	if(!(tpool = tpool_create(debug ? 1 : 0))) {
		destroy_rend();
		free(fbpixels);
		fbpixels = 0;
		return -1;
	}

	return 0;
}

void rt_cleanup(void)
{
	tpool_destroy(tpool);
	destroy_rend();
	free(fbpixels);
}

void rt_clear(void)
{
	cur_frame++;
	cur_sample = 0;

	memset(fbpixels, 0, fbwidth * fbheight * 3 * sizeof *fbpixels);
}

void rt_render(int nsamples)
{
	int i, j, k, x, y, num_xblk, num_yblk;

	num_xblk = (fbwidth + BLOCK_SIZE - 1) / BLOCK_SIZE;
	num_yblk = (fbheight + BLOCK_SIZE - 1) / BLOCK_SIZE;

	tpool_begin_batch(tpool);
	for(k=0; k<nsamples; k++) {
		cur_sample++;
		y = 0;
		for(i=0; i<num_yblk; i++) {
			int bh = fbheight - y > BLOCK_SIZE ? BLOCK_SIZE : fbheight - y;
			x = 0;
			for(j=0; j<num_xblk; j++) {
				struct rt_block *blk = alloc_block();
				if(!blk) abort();
				blk->frm = cur_frame;
				blk->sample = cur_sample;
				blk->x = x;
				blk->y = y;
				blk->w = fbwidth - x > BLOCK_SIZE ? BLOCK_SIZE : fbwidth - x;
				blk->h = bh;

				tpool_enqueue(tpool, blk, render_block, done_block);
				x += BLOCK_SIZE;
			}
			y += BLOCK_SIZE;
		}
	}
	tpool_end_batch(tpool);
}

static void render_block(void *bp)
{
	int i, j, px, py;
	cgm_ray ray;
	cgm_vec3 color;
	struct rt_block *blk = bp;
	float *fbptr = fbpixels + (blk->y * fbwidth + blk->x) * 3;

	if(blk->frm < cur_frame) {
		return;
	}

	for(i=0; i<blk->h; i++) {
		py = blk->y + i;
		for(j=0; j<blk->w; j++) {
			px = blk->x + j;

			if(debug && px == fbwidth / 2 && py == fbheight / 2) {
				asm("int $3");
			}
			primary_ray(&ray, px, py, blk->sample);
			trace_ray(&color, &ray, 0);

			*fbptr++ += color.x;
			*fbptr++ += color.y;
			*fbptr++ += color.z;
		}
		fbptr += (fbwidth - blk->w) * 3;
	}
}

static void done_block(void *bp)
{
	struct rt_block *blk = bp;
	pthread_mutex_lock(&donelist_lock);

	blk->next = 0;
	if(donelist) {
		donelist_tail->next = blk;
		donelist_tail = blk;
	} else {
		donelist = donelist_tail = blk;
	}
	pthread_mutex_unlock(&donelist_lock);
	redraw();
}

struct rt_block *rt_begin_update(void)
{
	pthread_mutex_lock(&donelist_lock);
	return donelist;
}

void rt_end_update(void)
{
	while(donelist) {
		struct rt_block *tmp = donelist;
		donelist = donelist->next;
		free_block(tmp);
	}
	donelist_tail = 0;
	pthread_mutex_unlock(&donelist_lock);
}

#define MAX_JOB_POOL_SIZE	4096
static struct rt_block *freeblocks;
static int freeblocks_size;
static pthread_mutex_t blockslock = PTHREAD_MUTEX_INITIALIZER;

static struct rt_block *alloc_block(void)
{
	struct rt_block *res = 0;

	pthread_mutex_lock(&blockslock);
	if(freeblocks) {
		res = freeblocks;
		freeblocks = freeblocks->next;
		freeblocks_size--;
	}
	pthread_mutex_unlock(&blockslock);

	if(!res && !(res = malloc(sizeof *res))) {
		return 0;
	}
	res->next = 0;
	return res;
}

static void free_block(struct rt_block *blk)
{
	pthread_mutex_lock(&blockslock);
	if(freeblocks_size >= MAX_JOB_POOL_SIZE) {
		pthread_mutex_unlock(&blockslock);
		free(blk);
	} else {
		blk->next = freeblocks;
		freeblocks = blk;
		freeblocks_size++;
		pthread_mutex_unlock(&blockslock);
	}
}
