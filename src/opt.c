#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"

struct options opt = {
	1280, 720,
	10,					/* nsamples */
	0,					/* input file */
	"output.hdr",		/* output file */
	0,					/* shared memory path */
	0,					/* number of threads (0=auto) */
	16,					/* tile size */
	6,					/* max recursion depth */
#ifdef USE_OIDN
	1,					/* denoise */
#endif
	OPT_PROGRESS,
	1.0f				/* gamma */
};

static const char *usage_fmt = "Usage: %s [options] <scene file>\n"
	"Options:\n"
	" -o <filename>: output image file\n"
	" -s,-size <WxH>: output image resolution\n"
	" -r,-samples <N>: number of rays per pixel\n"
	" -t,-threads <N>: override number of threads\n"
	" -tile <N>: render tile size\n"
	" -d,-depth <N>: maximum recursion depth\n"
#ifdef USE_OIDN
	" -D,-denoise: toggle denoising\n"
#endif
	" -p: toggle progress bar\n"
	" -gamma <N>: gamma exponent for the output image\n"
	" -h,-help: print usage information and exit\n\n";

int parse_args(int argc, char **argv)
{
	int i;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-size") == 0) {
				if(!argv[++i] || sscanf(argv[i], "%dx%d", &opt.width, &opt.height) != 2) {
					fprintf(stderr, "%s must be followed by <width>x<height>\n", argv[i - 1]);
					return -1;
				}

			} else if(strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-samples") == 0) {
				if(!argv[++i] || (opt.nsamples = atoi(argv[i])) <= 0) {
					fprintf(stderr, "%s must be followed by the number of rays per pixel\n", argv[i - 1]);
					return -1;
				}

			} else if(strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "-threads") == 0) {
				if(!argv[++i] || (opt.nthreads = atoi(argv[i])) <= 0) {
					fprintf(stderr, "%s must be followed by the number of threads\n", argv[i - 1]);
					return -1;
				}

			} else if(strcmp(argv[i], "-tile") == 0) {
				if(!argv[++i] || (opt.tilesz = atoi(argv[i])) <= 0) {
					fprintf(stderr, "%s must be followed by the tile size\n", argv[i - 1]);
					return -1;
				}

			} else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-depth") == 0) {
				if(!argv[++i] || (opt.max_iter = atoi(argv[i])) <= 0) {
					fprintf(stderr, "%s must be followed by the maximum recursion depth\n", argv[i - 1]);
					return -1;
				}
#ifdef USE_OIDN
			} else if(strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "-denoise") == 0) {
				opt.denoise ^= 1;
#endif
			} else if(strcmp(argv[i], "-np") == 0) {
				opt.flags ^= OPT_PROGRESS;

			} else if(strcmp(argv[i], "-gamma") == 0) {
				if(!argv[++i] || (opt.gamma = atof(argv[i])) <= 0.0f) {
					fprintf(stderr, "-gamma must be followed by a gamma value\n");
					return -1;
				}

			} else if(strcmp(argv[i], "-o") == 0) {
				if(!argv[++i]) {
					fprintf(stderr, "-o must be followed by the output file\n");
					return -1;
				}
				opt.outfile = argv[i];

			} else if(strcmp(argv[i], "-shm") == 0) {
				if(!argv[++i]) {
					fprintf(stderr, "-shm must be followed by the front-end shared memory path\n");
					return -1;
				}
				opt.shm = argv[i];

			} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
				printf(usage_fmt, argv[0]);
				exit(0);

			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				return -1;
			}

		} else {
			if(opt.infile) {
				fprintf(stderr, "unexpected argument: %s\n", argv[i]);
				return -1;
			}
			opt.infile = argv[i];
		}
	}

	return 0;
}
