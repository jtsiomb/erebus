#include <stdio.h>
#include "erebus.h"
#include "scene.h"
#include "rt.h"
#include "opt.h"
#include "imago2.h"

int main(int argc, char **argv)
{
	int i, npixels;
	unsigned long dur, start_time;
	cgm_vec4 *fbptr;

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

	if(0) {//scn.camlist) {
		cgm_mcopy(view_xform, scn.camlist->node.matrix);
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
	for(i=0; i<npixels; i++) {
		float s = 1.0f / fbptr->w;
		fbptr->x *= s;
		fbptr->y *= s;
		fbptr->z *= s;
		fbptr->w = 1.0f;
		fbptr++;
	}

	if(img_save_pixels(opt.outfile, fb.pixels, fb.width, fb.height, IMG_FMT_RGBAF) == -1) {
		fprintf(stderr, "failed to save output image to %s\n", opt.outfile);
		return -1;
	}

	destroy_scene(&scn);
	tpool_destroy(tpool);
	return 0;
}
