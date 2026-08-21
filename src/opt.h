#ifndef OPT_H_
#define OPT_H_

struct options {
	int width, height;
	int nsamples;
	char *infile, *outfile;
	char *shm;

	int nthreads;
	int tilesz;
	int max_iter;
};

extern struct options opt;

int parse_args(int argc, char **argv);

#endif	/* OPT_H_ */
