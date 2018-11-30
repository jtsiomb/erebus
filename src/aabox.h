#ifndef AABOX_H_
#define AABOX_H_

#include <cgmath/cgmath.h>

struct aabox {
	cgm_vec3 vmin, vmax;
};

struct surf_hit;

int ray_aabox(const struct aabox *box, const cgm_ray *ray, struct surf_hit *hit);

#endif	/* AABOX_H_ */
