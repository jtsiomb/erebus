#ifndef BRDF_H_
#define BRDF_H_

#include "cgmath/cgmath.h"

struct rayhit;

struct brdf {
	void (*eval)(cgm_vec3 *col, cgm_vec3 n, cgm_vec3 l, cgm_vec3 v, struct rayhit *hit);
	void (*sample)(cgm_vec3 *dir, struct rayhit *hit);
};

extern struct brdf lambert_brdf;
extern struct brdf phong_brdf;

#endif	/* BRDF_H_ */
