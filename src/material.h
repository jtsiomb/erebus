#ifndef MATERIAL_H_
#define MATERIAL_H_

#include <cgmath/cgmath.h>

struct material {
	cgm_vec3 color;
	cgm_vec3 emission;
	float roughness;
	int metallic;

	struct material *next;
};

#endif	/* MATERIAL_H_ */
