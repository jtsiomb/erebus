#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"
#include "treestor/treestor.h"

struct options opt = {
	1280, 720,
	10,					/* nsamples */
	0,					/* input file */
	"output.hdr",		/* output file */
	0,					/* shared memory path */
	0,					/* number of threads (0=auto) */
	32,					/* tile size */
	6,					/* max recursion depth */
#ifdef USE_OIDN
	1,					/* denoise */
#endif
	OPT_DEF_RENDERER,
	OPT_PROGRESS,
	2.2f,				/* gamma */
	OPT_TONEMAP_ACES
};

static const char *usage_text[] = {
	"Usage: %s [options] <scene file>\n",
	"Options:\n",
	" -o <filename>: output image file\n",
	" -s,-size <WxH>: output image resolution\n",
	" -r,-samples <N>: number of rays per pixel\n",
	" -t,-threads <N>: override number of threads\n",
	" -tile <N>: render tile size\n",
	" -d,-depth <N>: maximum recursion depth\n",
#ifdef USE_OIDN
	" -D,-denoise: toggle denoising\n",
#endif
	" -R,-renderer <renderer>: select renderer (rt, path)\n",
	" -np: disable progress bar\n",
	" -gamma <N>: gamma exponent for the output image\n",
	" -tmap <op>: select tone mapping operator (reinhard, aces)\n",
	" -h,-help: print usage information and exit\n\n",
	0
};

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
			} else if(strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "-renderer") == 0) {
				enum opt_renderer rsel;
				if(!argv[++i] || ((rsel = OPT_RAY_TRACER, strcmp(argv[i], "rt") != 0) &&
						(rsel = OPT_PATH_TRACER, strcmp(argv[i], "path") != 0))) {
					fprintf(stderr, "%s must be followed by the renderer (rt, path)\n", argv[i - 1]);
					return -1;
				}
				opt.renderer = rsel;

			} else if(strcmp(argv[i], "-np") == 0) {
				opt.flags ^= OPT_PROGRESS;

			} else if(strcmp(argv[i], "-gamma") == 0) {
				if(!argv[++i] || (opt.gamma = atof(argv[i])) <= 0.0f) {
					fprintf(stderr, "-gamma must be followed by a gamma value\n");
					return -1;
				}

			} else if(strcmp(argv[i], "-tmap") == 0) {
				enum opt_tonemap tmap;
				if(!argv[++i] || ((tmap = OPT_TONEMAP_REINHARD, strcmp(argv[i], "reinhard") != 0) &&
						(tmap = OPT_TONEMAP_ACES, strcmp(argv[i], "aces") != 0))) {
					fprintf(stderr, "%s must be followed by the renderer (reinhard, aces)\n", argv[i - 1]);
					return -1;
				}
				opt.tonemap = tmap;

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

			} else if(strcmp(argv[i], "-wait") == 0) {
				opt.flags |= OPT_WAIT;

			} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
				printf(usage_text[0], argv[0]);
				for(i=1; usage_text[i]; i++) {
					fputs(usage_text[i], stdout);
				}
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


static int bool_value(struct ts_attr *attr)
{
	switch(attr->val.type) {
	case TS_STRING:
		if(strcmp(attr->val.str, "true") == 0 ||
				strcmp(attr->val.str, "yes") == 0 ||
				strcmp(attr->val.str, "on") == 0) {
			return 1;
		}
		if(strcmp(attr->val.str, "false") == 0 ||
				strcmp(attr->val.str, "no") == 0 ||
				strcmp(attr->val.str, "off") == 0) {
			return 0;
		}
		break;

	case TS_NUMBER:
		if(attr->val.inum == 1) return 1;
		if(attr->val.inum == 0) return 0;
		break;

	default:
		break;
	}
	return -1;
}

#define EXPECT_INT(min)	\
	if(attr->val.type != TS_NUMBER || attr->val.inum < (min)) { \
		fprintf(stderr, "%s: %s: value must be a number\n", fname, attr->name); \
		goto next; \
	}


int read_options(const char *fname)
{
	int val, numopts = 0;
	struct ts_node *ts, *tsn;
	struct ts_attr *attr;

	if(!(ts = ts_load(fname)) || strcmp(ts->name, "erebus") != 0) {
		goto end;
	}
	if(!(tsn = ts_get_child(ts, "opt"))) {
		goto end;
	}

	attr = tsn->attr_list;
	while(attr) {
		if(strcmp(attr->name, "size") == 0) {
			if(sscanf(attr->val.str, "%dx%d", &opt.width, &opt.height) < 2) {
				fprintf(stderr, "%s: size: value must be <width>x<height>\n", fname);
				goto next;
			}

		} else if(strcmp(attr->name, "samples") == 0) {
			EXPECT_INT(1);
			opt.nsamples = attr->val.inum;

		} else if(strcmp(attr->name, "threads") == 0) {
			if(attr->val.type == TS_STRING && strcmp(attr->val.str, "auto") == 0) {
				opt.nthreads = 0;
			} else if(attr->val.type == TS_NUMBER && attr->val.inum >= 0) {
				opt.nthreads = attr->val.inum;
			} else {
				fprintf(stderr, "%s: threads: value must be a positive number, or \"auto\"\n", fname);
			}

		} else if(strcmp(attr->name, "tile") == 0) {
			EXPECT_INT(1);
			opt.tilesz = attr->val.inum;

		} else if(strcmp(attr->name, "depth") == 0) {
			EXPECT_INT(0);
			opt.max_iter = attr->val.inum;

		} else if(strcmp(attr->name, "denoise") == 0) {
			if((val = bool_value(attr)) == -1) {
				fprintf(stderr, "%s: denoise: value must be a boolean\n", fname);
				goto next;
			}
#ifdef USE_OIDN
			opt.denoise = val;
#else
			if(val) {
				fprintf(stderr, "%s: ignoring denoise enable, renderer not built with OIDN support\n", fname);
			}
#endif

		} else if(strcmp(attr->name, "renderer") == 0) {
			if(attr->val.type == TS_STRING) {
				if(strcmp(attr->val.str, "rt") == 0) {
					opt.renderer = OPT_RAY_TRACER;
					goto next;
				}
				if(strcmp(attr->val.str, "pt") == 0) {
					opt.renderer = OPT_PATH_TRACER;
					goto next;
				}
			}
			fprintf(stderr, "%s: renderer: value must be rt or pt\n", fname);

		} else if(strcmp(attr->name, "gamma") == 0) {
			if(attr->val.type != TS_NUMBER || attr->val.fnum <= 0.0f) {
				fprintf(stderr, "%s: gamma: value must be a positive number\n", fname);
				goto next;
			}

			opt.gamma = attr->val.fnum;

		} else if(strcmp(attr->name, "tonemap") == 0) {
			if(attr->val.type != TS_STRING) {
				if(strcmp(attr->val.str, "reinhard") == 0) {
					opt.tonemap = OPT_TONEMAP_REINHARD;
					goto next;
				}
				if(strcmp(attr->val.str, "aces") == 0) {
					opt.tonemap = OPT_TONEMAP_ACES;
					goto next;
				}
			}
			fprintf(stderr, "%s: tonemap: value must be \"reinhard\" or \"aces\"\n", fname);

		} else if(strcmp(attr->name, "output") == 0) {
			opt.outfile = strdup(attr->val.str);

		} else {
			fprintf(stderr, "%s: unknown option: %s\n", fname, attr->name);
		}
next:	attr = attr->next;
	}

	ts_free_tree(ts);
end:
	return numopts;
}
