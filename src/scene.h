#ifndef SCENE_H_
#define SCENE_H_

#include "surf.h"

struct scene {
	cgm_vec3 sky_nadir, sky_horiz, sky_zenith;

	union surface *surfaces;
	union surface *emitters;
	struct material *materials;
};

void init_scene(struct scene *scn);
void clear_scene(struct scene *scn);

void add_surface(struct scene *scn, union surface *surf);
void add_material(struct scene *scn, struct material *mtl);

int ray_scene(const struct scene *scn, const cgm_ray *ray, struct surf_hit *hit);


#endif	/* SCENE_H_ */
