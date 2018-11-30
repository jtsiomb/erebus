#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "surf.h"

int ray_surface(const union surface *surf, const cgm_ray *ray, struct surf_hit *hit)
{
	switch(surf->any.type) {
	case SURF_SPHERE:
		return ray_surf_sphere(&surf->sph, ray, hit);

	case SURF_AABOX:
		return ray_surf_aabox(&surf->box, ray, hit);

	case SURF_MESH:
		return ray_surf_mesh(&surf->mesh, ray, hit);

	default:
		assert(!"unknown surface type passed to ray_surface");
		break;
	}
	return 0;
}

int ray_surf_sphere(const struct surf_sphere *sph, const cgm_ray *ray, struct surf_hit *hit)
{
	float a, b, c, d, sqrt_d, t0, t1, t;
	cgm_ray lray = *ray;

	cgm_rmul_mr(&lray, sph->inv_xform);

	a = cgm_vdot(&lray.dir, &lray.dir);
	b = 2.0f * cgm_vdot(&lray.dir, &lray.origin);
	c = cgm_vdot(&lray.origin, &lray.origin) - 1.0f;

	d = b * b - 4.0f * a * c;
	if(d < 1e-5) return 0;

	sqrt_d = sqrt(d);
	t0 = (-b + sqrt_d) / (2.0f * a);
	t1 = (-b - sqrt_d) / (2.0f * a);

	if(t0 < 1e-5) t0 = t1;
	if(t1 < 1e-5) t1 = t0;
	t = t0 < t1 ? t0 : t1;
	if(t < 1e-5) return 0;

	if(hit) {
		hit->t = t;
		hit->surf = (void*)sph;
		hit->pos.x = hit->normal.x = ray->origin.x + ray->dir.x * t;
		hit->pos.y = hit->normal.y = ray->origin.y + ray->dir.y * t;
		hit->pos.z = hit->normal.z = ray->origin.z + ray->dir.z * t;
		cgm_vnormalize(&hit->normal);
	}
	return 1;
}

int ray_surf_aabox(const struct surf_aabox *box, const cgm_ray *ray, struct surf_hit *hit)
{
	int sign[3];
	float t, tmin, tmax, tymin, tymax, tzmin, tzmax, x, y, z;
	cgm_vec3 inv_dir, param[] = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
	cgm_ray lray = *ray;

	cgm_rmul_mr(&lray, box->inv_xform);

	cgm_vcons(&inv_dir, 1.0f / lray.dir.x, 1.0f / lray.dir.y, 1.0f / lray.dir.z);
	sign[0] = inv_dir.x < 0.0f ? 1 : 0;
	sign[1] = inv_dir.y < 0.0f ? 1 : 0;
	sign[2] = inv_dir.z < 0.0f ? 1 : 0;

	tmin = (param[sign[0]].x - lray.origin.x) * inv_dir.x;
	tmax = (param[1 - sign[0]].x - lray.origin.x) * inv_dir.x;
	tymin = (param[sign[1]].y - lray.origin.y) * inv_dir.y;
	tymax = (param[1 - sign[1]].y - lray.origin.y) * inv_dir.y;

	if(tmin > tymax || tymin > tmax) {
		return 0;
	}
	if(tymin > tmin) tmin = tymin;
	if(tymax < tmax) tmax = tymax;

	tzmin = (param[sign[2]].z - lray.origin.z) * inv_dir.z;
	tzmax = (param[1 - sign[2]].z - lray.origin.z) * inv_dir.z;

	if(tmin > tzmax || tzmin > tmax) {
		return 0;
	}
	if(tzmin > tmin) tmin = tzmin;
	if(tzmax < tmax) tmax = tzmax;

	t = tmin < 1e-5 ? tmax : tmin;
	if(t < 1e-5) return 0;

	if(hit) {
		hit->t = t;
		hit->surf = (void*)box;
		hit->pos.x = ray->origin.x + ray->dir.x * t;
		hit->pos.y = ray->origin.y + ray->dir.y * t;
		hit->pos.z = ray->origin.z + ray->dir.z * t;

		x = lray.origin.x + lray.dir.x * t;
		y = lray.origin.y + lray.dir.y * t;
		z = lray.origin.z + lray.dir.z * t;
		if(fabs(x) > fabs(y) && fabs(x) > fabs(z)) {
			cgm_vcons(&hit->normal, x > 0.0f ? 1.0f : -1.0f, 0, 0);
		} else if(fabs(y) > fabs(z)) {
			cgm_vcons(&hit->normal, 0, y > 0.0f ? 1.0f : -1.0f, 0);
		} else {
			cgm_vcons(&hit->normal, 0, 0, z > 0.0f ? 1.0f : -1.0f);
		}
	}
	return 1;
}

int ray_surf_mesh(const struct surf_mesh *mesh, const cgm_ray *ray, struct surf_hit *hit)
{
	int res = find_mesh_isect(&mesh->m, mesh->inv_xform, ray, hit);
	if(res && hit) {
		hit->surf = (void*)mesh;
	}
	return res;
}

union surface *create_sphere(float x, float y, float z, float rad)
{
	union surface *surf;

	if(rad == 0.0f) return 0;

	if(!(surf = calloc(1, sizeof *surf))) {
		return 0;
	}
	surf->sph.type = SURF_SPHERE;
	cgm_mscaling(surf->sph.xform, rad, rad, rad);
	cgm_mtranslate(surf->sph.xform, x, y, z);

	cgm_mcopy(surf->sph.inv_xform, surf->sph.xform);
	cgm_minverse(surf->sph.inv_xform);

	return surf;
}

union surface *create_aabox(float x, float y, float z, float xsz, float ysz, float zsz)
{
	union surface *surf;

	if(!(surf = calloc(1, sizeof *surf))) {
		return 0;
	}
	surf->box.type = SURF_AABOX;
	cgm_mscaling(surf->box.xform, xsz, ysz, zsz);
	cgm_mtranslate(surf->box.xform, x, y, z);

	cgm_mcopy(surf->box.inv_xform, surf->box.xform);
	cgm_minverse(surf->box.inv_xform);

	return surf;
}

union surface *create_mesh(void)
{
	union surface *surf;

	if(!(surf = calloc(1, sizeof *surf))) {
		return 0;
	}
	surf->mesh.type = SURF_MESH;
	cgm_midentity(surf->mesh.xform);
	cgm_midentity(surf->mesh.inv_xform);
	init_mesh(&surf->mesh.m);

	return surf;
}

void free_surface(union surface *surf)
{
	switch(surf->any.type) {
	case SURF_MESH:
		clear_mesh(&surf->mesh.m);
		break;

	default:
		break;
	}

	free(surf);
}

void calc_bounds(union surface *surf)
{
	switch(surf->any.type) {
	case SURF_SPHERE:
		cgm_vcons(&surf->sph.aabb.vmin, -1, -1, -1);
		cgm_vcons(&surf->sph.aabb.vmax, 1, 1, 1);
		break;

	case SURF_AABOX:
		cgm_vcons(&surf->sph.aabb.vmin, -0.5, -0.5, -0.5);
		cgm_vcons(&surf->sph.aabb.vmax, 0.5, 0.5, 0.5);
		break;

	case SURF_MESH:
		calc_mesh_bounds(&surf->mesh.m, &surf->mesh.aabb);
		break;

	default:
		break;
	}
}
