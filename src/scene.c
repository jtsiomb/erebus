#include <stdlib.h>
#include <float.h>
#include "scene.h"

void init_scene(struct scene *scn)
{
	memset(scn, 0, sizeof *scn);
}

void clear_scene(struct scene *scn)
{
	while(scn->surfaces) {
		union surface *s = scn->surfaces;
		scn->surfaces = scn->surfaces->any.next;
		free_surface(s);
	}

	while(scn->materials) {
		struct material *m = scn->materials;
		scn->materials = scn->materials->next;
		free(m);
	}

	scn->surfaces = 0;
	scn->emitters = 0;
	scn->materials = 0;
}

void add_surface(struct scene *scn, union surface *surf)
{
	surf->any.next = scn->surfaces;
	scn->surfaces = surf;

	if(surf->any.mtl && cgm_vlength_sq(&surf->any.mtl->emission) > 1e-4) {
		surf->any.emnext = scn->emitters;
		scn->emitters = surf;
	}
}

void add_material(struct scene *scn, struct material *mtl)
{
	mtl->next = scn->materials;
	scn->materials = mtl;
}

int ray_scene(const struct scene *scn, const cgm_ray *ray, struct surf_hit *hit)
{
	union surface *surf;
	struct surf_hit nearest;

	nearest.t = FLT_MAX;
	nearest.surf = 0;

	surf = scn->surfaces;
	while(surf) {
		struct surf_hit tmphit;
		if(ray_surface(surf, ray, &tmphit) && tmphit.t < nearest.t) {
			nearest = tmphit;
		}
		surf = surf->any.next;
	}

	if(nearest.surf) {
		if(hit) *hit = nearest;
		return 1;
	}
	return 0;
}
