#include "aabox.h"
#include "surf.h"

int ray_aabox(const struct aabox *box, const cgm_ray *ray, struct surf_hit *hit)
{
	int sign[3];
	float t, tmin, tmax, tymin, tymax, tzmin, tzmax, x, y, z;
	const cgm_vec3 *param = &box->vmin;
	cgm_vec3 inv_dir;

	cgm_vcons(&inv_dir, 1.0f / ray->dir.x, 1.0f / ray->dir.y, 1.0f / ray->dir.z);
	sign[0] = inv_dir.x < 0.0f ? 1 : 0;
	sign[1] = inv_dir.y < 0.0f ? 1 : 0;
	sign[2] = inv_dir.z < 0.0f ? 1 : 0;

	tmin = (param[sign[0]].x - ray->origin.x) * inv_dir.x;
	tmax = (param[1 - sign[0]].x - ray->origin.x) * inv_dir.x;
	tymin = (param[sign[1]].y - ray->origin.y) * inv_dir.y;
	tymax = (param[1 - sign[1]].y - ray->origin.y) * inv_dir.y;

	if(tmin > tymax || tymin > tmax) {
		return 0;
	}
	if(tymin > tmin) tmin = tymin;
	if(tymax < tmax) tmax = tymax;

	tzmin = (param[sign[2]].z - ray->origin.z) * inv_dir.z;
	tzmax = (param[1 - sign[2]].z - ray->origin.z) * inv_dir.z;

	if(tmin > tzmax || tzmin > tmax) {
		return 0;
	}
	if(tzmin > tmin) tmin = tzmin;
	if(tzmax < tmax) tmax = tzmax;

	t = tmin < 1e-5 ? tmax : tmin;
	if(t < 1e-5) return 0;

	if(hit) {
		hit->t = t;
		hit->surf = (union surface*)box;
		hit->pos.x = ray->origin.x + ray->dir.x * t;
		hit->pos.y = ray->origin.y + ray->dir.y * t;
		hit->pos.z = ray->origin.z + ray->dir.z * t;

		x = ray->origin.x + ray->dir.x * t;
		y = ray->origin.y + ray->dir.y * t;
		z = ray->origin.z + ray->dir.z * t;
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
