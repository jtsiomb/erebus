#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>
#include "rt.h"
#include "erebus.h"
#include "shmfb.h"
#include "util.h"


struct framebuffer fb;
struct thread_pool *tpool;
float view_xform[16];
float zdist;		/* 1.0 / tan(fov / 2) */

struct renderer rend;

static struct tile *tiles;
static int num_tiles;

static void print_progress(int p, int sample);


int fbsize(int width, int height)
{
	int i, j, x, y, xtiles, ytiles;
	cgm_vec4 *fbptr;
#ifdef USE_OIDN
	cgm_vec3 *nptr = 0, *alb = 0;
#endif
	struct tile *tileptr;

	/* width and height will be 0 if we mapped a shared memory framebuffer
	 * and we shouldn't allocate memory for the pixels
	 */
	if(width > 0 && height > 0) {
		if(!(fbptr = malloc(width * height * sizeof *fb.pixels))) {
			return -1;
		}
	} else {
		fbptr = shmfb->pixels;
		width = shmfb->width;
		height = shmfb->height;
	}

#ifdef USE_OIDN
	if(opt.denoise) {
		if(!(nptr = malloc(width * height * sizeof *fb.normals))) {
			goto err;
		}
		if(!(alb = malloc(width * height * sizeof *fb.albedo))) {
			goto err;
		}
	}
#endif
	xtiles = (width + opt.tilesz - 1) / opt.tilesz;
	ytiles = (height + opt.tilesz - 1) / opt.tilesz;
	if(!(tileptr = malloc(xtiles * ytiles * sizeof *tiles))) {
		goto err;
	}

	fb.pixels = fbptr;
#ifdef USE_OIDN
	fb.albedo = alb;
	fb.normals = nptr;
#endif
	fb.width = width;
	fb.height = height;
	fb.aspect = (float)fb.width / (float)fb.height;

	tiles = tileptr;
	num_tiles = xtiles * ytiles;

	y = 0;
	for(i=0; i<ytiles; i++) {
		x = 0;
		for(j=0; j<xtiles; j++) {
			tileptr->x = x;
			tileptr->y = y;
			tileptr->width = width - x < opt.tilesz ? width - x : opt.tilesz;
			tileptr->height = height - y < opt.tilesz ? height - y : opt.tilesz;
			tileptr->fbptr = fbptr + x;
#ifdef USE_OIDN
			if(opt.denoise) {
				tileptr->albptr = alb + x;
				tileptr->nptr = nptr + x;
			} else {
				tileptr->albptr = tileptr->nptr = 0;
			}
#endif
			tileptr->sample = 0;
			tinymt32_init(&tileptr->rndstate, (i << 16) | j);
			tileptr++;

			x += opt.tilesz;
		}
		fbptr += width * opt.tilesz;
#ifdef USE_OIDN
		if(opt.denoise) {
			nptr += width * opt.tilesz;
			alb += width * opt.tilesz;
		}
#endif
		y += opt.tilesz;
	}

	return 0;
err:
	if(!shmfb) free(fbptr);
#ifdef USE_OIDN
	free(alb);
	free(nptr);
#endif
	return -1;
}

void set_fov(float fov)
{
	zdist = 1.0f / tan(fov * 0.5f);
}

void render(int samplenum)
{
	int i, ndone;

	if(zdist == 0.0f) {
		set_fov(CGM_PI / 4.0f);
	}

	if(!samplenum) {
		if(shmfb) {
			shmfb_start(num_tiles * opt.nsamples);
		} else if(opt.flags & OPT_PROGRESS) {
			atomic_int_zero(&progr_done_tiles);
			progr_total_tiles = num_tiles * opt.nsamples;
		}
	}

	for(i=0; i<num_tiles; i++) {
		tiles[i].sample = samplenum;
		tpool_enqueue(tpool, tiles + i, (tpool_callback)rend.render_tile, 0);
	}

	if(opt.flags & OPT_PROGRESS) {
		int progr, prev_progr = -1;
		do {
			tpool_timedwait(tpool, 500);
			progr = atomic_int_value(&progr_done_tiles) * 100 / progr_total_tiles;
			if(progr != prev_progr) {
				print_progress(progr, samplenum);
				prev_progr = progr;
			}
		} while(tpool_pending_jobs(tpool));

		if((ndone = atomic_int_value(&progr_done_tiles)) == progr_total_tiles) {
			progr = ndone * 100 / progr_total_tiles;
			print_progress(progr, samplenum);
			putchar('\n');
		}
	} else {
		tpool_wait(tpool);
	}
}


void ray_trace(cgm_vec3 *color, cgm_ray *ray, float energy, int max_iter)
{
	struct rayhit hit;

	if(max_iter && ray_scene(ray, &scn, FLT_MAX, &hit)) {
		rend.shade(color, &hit, energy, max_iter);
	} else {
		rend.bgcolor(color, ray);
	}
}

float fresnel(float costheta, float ior)
{
	float x, xsq, r0;

	r0 = (1.0f - ior) / (1.0f + ior);
	r0 *= r0;

	x = 1.0f - costheta;
	xsq = x * x;

	return r0 + (1.0f - r0) * (xsq * xsq * x);
}

float mtlattr_num(struct material *mtl, int attr, cgm_vec2 *uv)
{
	cgm_vec3 texel;

	if(mtl->attr[attr].tex) {
		tex_lookup(&texel, mtl->attr[attr].tex, uv->x, uv->y);
		return texel.x;
	}
	return mtl->attr[attr].value.x;
}

void mtlattr_vec(cgm_vec3 *res, struct material *mtl, int attr, cgm_vec2 *uv)
{
	if(mtl->attr[attr].tex) {
		tex_lookup(res, mtl->attr[attr].tex, uv->x, uv->y);
	} else {
		*res = mtl->attr[attr].value;
	}
}

void tex_lookup(cgm_vec3 *res, struct image *img, float u, float v)
{
	int tx, ty;

	v = 1.0f - v;

	if(img->ymask) {
		ty = (int)(v * img->height) & img->ymask;
	} else {
		ty = (int)(v * img->height) % img->height;
	}

	if(img->xmask) {
		tx = (int)(u * img->width) & img->xmask;
		*res = ((cgm_vec3*)img->pixels)[(ty << img->xshift) + tx];
	} else {
		tx = (int)(u * img->width) % img->width;
		*res = ((cgm_vec3*)img->pixels)[ty * img->width + tx];
	}
}


static void print_progress(int p, int sample)
{
	int i, nbar;

	printf("\r%-3d%% [", p);
	nbar = p >> 1;

	for(i=0; i<50; i++) {
		if(i < nbar) {
			putchar('=');
		} else if(i == nbar) {
			putchar('>');
		} else {
			putchar(' ');
		}
	}
	printf("] sample: %d/%d", sample + 1, opt.nsamples);
	fflush(stdout);
}
