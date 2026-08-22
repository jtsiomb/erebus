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
	for(i=0; i<npixels; i++) {
		float s = 1.0f / fbptr->w;
		rgb->x = fbptr->x * s;
		rgb->y = fbptr->y * s;
		rgb->z = fbptr->z * s;
		fbptr++;
		rgb++;
	}

#ifdef USE_OIDN
	if(opt.denoise) {
		printf("denoising\n");
		denoise((float*)fb.pixels, fb.width, fb.height);
	}
#endif

	if(opt.gamma != 1.0f) {
		float inv_gamma = 1.0f / opt.gamma;

		printf("gamma correction: %g\n", opt.gamma);

		rgb = (cgm_vec3*)fb.pixels;
		for(i=0; i<npixels; i++) {
			rgb->x = pow(rgb->x, inv_gamma);
			rgb->y = pow(rgb->y, inv_gamma);
			rgb->z = pow(rgb->z, inv_gamma);
			rgb++;
		}
	}

	if(img_save_pixels(opt.outfile, fb.pixels, fb.width, fb.height, IMG_FMT_RGBF) == -1) {
		fprintf(stderr, "failed to save output image to %s\n", opt.outfile);
		return -1;
	}

	destroy_scene(&scn);
	tpool_destroy(tpool);
	return 0;
}
