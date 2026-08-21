#include <stdio.h>
#include "erebus.h"
#include "scene.h"
#include "rt.h"
#include "opt.h"
#include "imago2.h"

int main(int argc, char **argv)
{
	unsigned long dur, start_time;

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if(!(tpool = tpool_create(opt.nthreads))) {
		fprintf(stderr, "failed to create thread pool\n");
		return -1;
	}

	if(load_scene(&scn, opt.infile ? opt.infile : "simple.lvl") == -1) {
		return 1;
	}
	fbsize(opt.width, opt.height);

	start_time = get_msec();


	dur = get_msec() - start_time;
	printf("Rendering took %.3f sec\n", (float)dur / 1000.0f);

	if(img_save_pixels(opt.outfile, fb.pixels, fb.width, fb.height, IMG_FMT_RGBAF) == -1) {
		fprintf(stderr, "failed to save output image to %s\n", opt.outfile);
		return -1;
	}

	destroy_scene(&scn);
	tpool_destroy(tpool);
	return 0;
}
