#ifndef XEREBUS_H_
#define XEREBUS_H_

enum render_method { REND_PATH_TRACING, REND_RAY_TRACING };
enum tone_mapping { TONEMAP_NONE, TONEMAP_REINHARD, TONEMAP_ACESFILMIC };

struct render_opts {
	enum render_method rend;
	enum tone_mapping tmap;
	int xres, yres;
	int nsamples;
	int denoise;
	float gammaval;
};

extern struct render_opts ropt;

extern const char *glrend, *glvendor, *glver;

#endif	/* XEREBUS_H_ */
