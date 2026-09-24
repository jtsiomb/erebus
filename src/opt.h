#ifndef OPT_H_
#define OPT_H_

enum opt_renderer {
	OPT_DEF_RENDERER,
	OPT_RAY_TRACER,
	OPT_PATH_TRACER
};

enum opt_tonemap {
	OPT_TONEMAP_NONE,
	OPT_TONEMAP_REINHARD,
	OPT_TONEMAP_ACES
};

enum {
	OPT_PROGRESS	= 1,
	OPT_WAIT		= 0x800000
};

struct options {
	int width, height;
	int nsamples;
	char *infile, *outfile;
	char *shm;

	int nthreads;
	int tilesz;
	int max_iter;
#ifdef USE_OIDN
	int denoise;
#endif

	enum opt_renderer renderer;
	unsigned int flags;
	float gamma;
	enum opt_tonemap tonemap;
};

extern struct options opt;

int parse_args(int argc, char **argv);

#endif	/* OPT_H_ */
