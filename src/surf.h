#ifndef SURF_H_
#define SURF_H_

#include <cgmath/cgmath.h>
#include "aabox.h"
#include "mesh.h"
#include "material.h"

union surface;

struct surf_hit {
	float t;
	cgm_vec3 pos;
	cgm_vec3 normal;
	cgm_vec3 tex;
	void *surf;
};

enum surf_type {
	SURF_UNKNOWN,
	SURF_SPHERE,
	SURF_AABOX,
	SURF_MESH
};

union surface;

#define COMMON_SURFACE_VARS \
	enum surf_type type; \
	float xform[16], inv_xform[16]; \
	struct material *mtl; \
	struct aabox aabb; \
	union surface *next, *emnext

struct surf_any {
	COMMON_SURFACE_VARS;
};

struct surf_sphere {
	COMMON_SURFACE_VARS;
};

struct surf_aabox {
	COMMON_SURFACE_VARS;
};

struct surf_mesh {
	COMMON_SURFACE_VARS;
	struct mesh m;
};

union surface {
	struct surf_any any;
	struct surf_sphere sph;
	struct surf_aabox box;
	struct surf_mesh mesh;
};

int ray_surface(const union surface *surf, const cgm_ray *ray, struct surf_hit *hit);
int ray_surf_sphere(const struct surf_sphere *sph, const cgm_ray *ray, struct surf_hit *hit);
int ray_surf_aabox(const struct surf_aabox *box, const cgm_ray *ray, struct surf_hit *hit);
int ray_surf_mesh(const struct surf_mesh *mesh, const cgm_ray *ray, struct surf_hit *hit);

union surface *create_sphere(float x, float y, float z, float rad);
union surface *create_aabox(float x, float y, float z, float xsz, float ysz, float zsz);
union surface *create_mesh(void);

void free_surface(union surface *surf);

void calc_bounds(union surface *surf);

#endif	/* SURF_H_ */
