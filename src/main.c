#include <stdio.h>
#include "erebus.h"
#include "scene.h"
#include "rt.h"
#include "opt.h"
#include "imago2.h"
#include "denoise.h"


int main(int argc, char **argv)
{
	int i, npixels;
	unsigned long dur, start_time;
	cgm_vec4 *fbptr;
	cgm_vec3 *rgb;
#ifdef USE_OIDN
	cgm_vec3 *alb, *nptr;
#endif

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if(!(tpool = tpool_create(opt.nthreads))) {
		fprintf(stderr, "failed to create thread pool\n");
		return -1;
	}

	if(load_scene(&scn, opt.infile ? opt.infile : "simple.erebus") == -1) {
		return 1;
	}
	fbsize(opt.width, opt.height);

	if(scn.camlist) {
		cgm_mcopy(view_xform, scn.camlist->node.matrix);
		set_fov(scn.camlist->fov);
	} else {
		cgm_mtranslation(view_xform, 0, 1.6, 0);
	}

	start_time = get_msec();

	for(i=0; i<opt.nsamples; i++) {
		render(i);
	}

	dur = get_msec() - start_time;
	printf("Rendering took %.3f sec\n", (float)dur / 1000.0f);

	npixels = fb.width * fb.height;
	fbptr = fb.pixels;
	rgb = (cgm_vec3*)fb.pixels;
#ifdef USE_OIDN
	alb = fb.albedo;
	nptr = fb.normals;
#endif
	for(i=0; i<npixels; i++) {
		float s = 1.0f / fbptr->w;
		rgb->x = fbptr->x * s;
		rgb->y = fbptr->y * s;
		rgb->z = fbptr->z * s;
		fbptr++;
		rgb++;

#ifdef USE_OIDN
		if(opt.denoise) {
			alb->x *= s;
			alb->y *= s;
			alb->z *= s;
			nptr->x *= s;
			nptr->y *= s;
			nptr->z *= s;
			alb++;
			nptr++;
		}
#endif
	}

#ifdef USE_OIDN
	if(opt.denoise) {
		printf("denoising\n");
		denoise((float*)fb.pixels, (float*)fb.normals, (float*)fb.albedo, fb.width, fb.height);
	}
#endif

	if(opt.gamma != 1.0f) {
		float inv_gamma = 1.0f / opt.gamma;

		printf("gamma correction: %g\n", opt.gamma);

		rgb = (cgm_vec3*)fb.pixels;
#ifdef USE_OIDN
		nptr = fb.normals;
#endif
		for(i=0; i<npixels; i++) {
			rgb->x = pow(rgb->x, inv_gamma);
			rgb->y = pow(rgb->y, inv_gamma);
			rgb->z = pow(rgb->z, inv_gamma);
			rgb++;
#ifdef USE_OIDN
			if(opt.denoise) {
				nptr->x = nptr->x * 0.5f + 0.5f;
				nptr->y = nptr->y * 0.5f + 0.5f;
				nptr->z = nptr->z * 0.5f + 0.5f;
				nptr++;
			}
#endif
		}
	}

	if(img_save_pixels(opt.outfile, fb.pixels, fb.width, fb.height, IMG_FMT_RGBF) == -1) {
		fprintf(stderr, "failed to save output image to %s\n", opt.outfile);
		return -1;
	}
#ifdef USE_OIDN
	if(opt.denoise) {
		img_save_pixels("dbgnorm.ppm", fb.normals, fb.width, fb.height, IMG_FMT_RGBF);
		img_save_pixels("dbgalb.ppm", fb.albedo, fb.width, fb.height, IMG_FMT_RGBF);
	}
#endif

	destroy_scene(&scn);
	tpool_destroy(tpool);
	return 0;
}
