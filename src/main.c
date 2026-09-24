#include <stdio.h>
#include <signal.h>
#include "erebus.h"
#include "scene.h"
#include "rt.h"
#include "mesh.h"
#include "opt.h"
#include "imago2.h"
#include "denoise.h"
#include "shmfb.h"
#include "util.h"
#include "postproc.h"

void cleanup(void);
void sighandler(int s);

volatile sig_atomic_t quit;

unsigned int progr_total_tiles;
ATOMIC_INT progr_done_tiles;

int main(int argc, char **argv)
{
	int i, npixels;
	unsigned long dur, start_time;
	cgm_vec4 *fbptr;
#ifdef USE_OIDN
	cgm_vec3 *alb, *nptr;
#endif

	if(parse_args(argc, argv) == -1) {
		return 1;
	}
	if(!opt.infile) {
		fprintf(stderr, "render what?\n");
		return 1;
	}

	setvbuf(stdout, 0, _IOLBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	signal(SIGINT, sighandler);

	if(opt.flags & OPT_WAIT) {
		raise(SIGSTOP);
	}

	if(opt.shm) {
		if(shmfb_map(opt.shm) == -1) {
			return 1;
		}
		opt.width = shmfb->width;
		opt.height = shmfb->height;

		fbsize(0, 0);
	} else {
		fbsize(opt.width, opt.height);
	}

	if(!(tpool = tpool_create(opt.nthreads))) {
		fprintf(stderr, "failed to create thread pool\n");
		return -1;
	}

	if(load_scene(&scn, opt.infile) == -1) {
		return 1;
	}

	/*dbg_dump_images();*/

	if(scn.camlist) {
		cgm_mcopy(view_xform, scn.camlist->node.matrix);
		set_fov(scn.camlist->fov);
	} else {
		cgm_mtranslation(view_xform, 0, 1.6, 0);
	}

	switch(opt.renderer) {
	case OPT_RAY_TRACER:
		rend = rt_renderer;
		break;

	case OPT_DEF_RENDERER:
	case OPT_PATH_TRACER:
		rend = pt_renderer;
		break;
	}
	printf("Renderer: %s\n", rend.name);

	printf("rendering %dx%d %d spp\n", opt.width, opt.height, opt.nsamples);
	start_time = get_msec();

	for(i=0; i<opt.nsamples; i++) {
		if(quit) goto end;
		render(i);
	}
	if(quit) goto end;

	npixels = fb.width * fb.height;
	fbptr = fb.pixels;
#ifdef USE_OIDN
	alb = fb.albedo;
	nptr = fb.normals;
#endif

#ifdef USE_OIDN
	if(opt.denoise) {
		for(i=0; i<npixels; i++) {
			float s = 1.0f / fbptr->w;
			alb->x *= s;
			alb->y *= s;
			alb->z *= s;
			nptr->x *= s;
			nptr->y *= s;
			nptr->z *= s;
			alb++;
			nptr++;
		}

		img_save_pixels("dbgnoise.ppm", fb.pixels, fb.width, fb.height, IMG_FMT_RGBAF);

		printf("denoising\n");
		denoise((float*)fb.pixels, (float*)fb.normals, (float*)fb.albedo, fb.width, fb.height);
	}
#endif

	dur = get_msec() - start_time;
	printf("Rendering took %.3f sec\n", (float)dur / 1000.0f);

	/* if we're operating on a shared memory framebuffer, all the post-processing
	 * and final saving of the image will be done by the front-end, so skip it.
	 */
	if(shmfb) goto end;

	/* divide by sample count */
#ifdef USE_OIDN
	if(!opt.denoise) {
#endif
		fbptr = fb.pixels;
		for(i=0; i<npixels; i++) {
			float s = 1.0f / fbptr->w;
			fbptr->x *= s;
			fbptr->y *= s;
			fbptr->z *= s;
			fbptr->w = 1.0f;
			fbptr++;
		}
#ifdef USE_OIDN
	}
#endif

	switch(opt.tonemap) {
	case OPT_TONEMAP_REINHARD:
		post_reinhard();
		break;

	case OPT_TONEMAP_ACES:
		post_acesfilm();
		break;

	default:
		break;
	}

	if(opt.gamma != 1.0f) {
		printf("gamma correction: %g\n", opt.gamma);
		post_gamma(opt.gamma);
	}

	if(img_save_pixels(opt.outfile, fb.pixels, fb.width, fb.height, IMG_FMT_RGBAF) == -1) {
		fprintf(stderr, "failed to save output image to %s\n", opt.outfile);
		goto end;
	}
#ifdef USE_OIDN
	/* dump denoise inputs */
	if(opt.denoise) {
		nptr = fb.normals;
		for(i=0; i<npixels; i++) {
			nptr->x = nptr->x * 0.5f + 0.5f;
			nptr->y = nptr->y * 0.5f + 0.5f;
			nptr->z = nptr->z * 0.5f + 0.5f;
			nptr++;
		}

		img_save_pixels("dbgnorm.ppm", fb.normals, fb.width, fb.height, IMG_FMT_RGBF);
		img_save_pixels("dbgalb.ppm", fb.albedo, fb.width, fb.height, IMG_FMT_RGBF);
	}
#endif

end:
	tpool_destroy(tpool);
	destroy_scene(&scn);
	if(shmfb) shmfb_unmap();
	return 0;
}

void sighandler(int s)
{
	printf("erebus received interrupt, shutting down...\n");
	quit = 1;
}
