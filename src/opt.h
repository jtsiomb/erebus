#ifndef OPT_H_
#define OPT_H_

enum {
	OPT_PROGRESS = 1
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

	unsigned int flags;
	float gamma;
};

extern struct options opt;

int parse_args(int argc, char **argv);

#endif	/* OPT_H_ */
