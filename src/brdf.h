#ifndef BRDF_H_
#define BRDF_H_

#include "cgmath/cgmath.h"

struct rayhit;

struct brdf_sample {
	cgm_vec3 dir, weight;
	float pdf;
	int transmit;
};

struct brdf {
	void (*eval)(cgm_vec3 *col, cgm_vec3 n, cgm_vec3 l, cgm_vec3 v, struct rayhit *hit);
	int (*sample)(struct brdf_sample *sample, struct rayhit *hit);
	float (*pdf)(cgm_vec3 dir, cgm_vec3 n, struct rayhit *hit);
};

extern struct brdf lambert_brdf;
extern struct brdf phong_brdf;
extern struct brdf ggx_brdf;

float ggx_g2_over_g1(float co, float ci, float rough);

#endif	/* BRDF_H_ */
