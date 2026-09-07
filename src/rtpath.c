#include <assert.h>
#include "erebus.h"
#include "rt.h"
#include "scene.h"


void pt_bgcolor(cgm_vec3 *color, cgm_ray *ray);
void pt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int max_iter);


struct renderer pt_renderer = {
	"path tracer",
	bgcolor,
	pt_shade
};


void pt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int max_iter)
{
	int transmit;
	cgm_vec3 v, n, out_n;
	float mrough, mtrans;
	float pdiff, pspec, rval;
	float fres;
	cgm_vec3 mcol, rcol;
	cgm_ray ray;
	struct material *mtl = hit->mtl;

	if(cgm_vdot(&hit->ray.dir, &hit->v.norm) > 0.0f) {
		cgm_vneg(&hit->v.norm);
	}
	n = hit->v.norm;

	mtlattr_vec(&mcol, hit->mtl, MATTR_COLOR, &hit->v.tex);
	mrough = mtlattr_num(hit->mtl, MATTR_ROUGHNESS, &hit->v.tex);
	mtrans = mtlattr_num(hit->mtl, MATTR_TRANSMIT, &hit->v.tex);

	mtlattr_vec(color, hit->mtl, MATTR_EMIT, &hit->v.tex);

#ifdef USE_OIDN
	if(!auxdata.valid) {
		auxdata.albedo = mcol;
		cgm_vadd(&auxdata.albedo, color);	/* add emitted light */
		auxdata.normal = n;
		auxdata.valid = 1;
	}
#endif

	rval = frand();

	pdiff = energy * mrough;
	pspec = energy * (1.0f - mrough);
	assert(pdiff + pspec <= 1.0f);

	if(rval <= pdiff) {
		cgm_vnormalize(&n);

		/* pick diffuse direction with a cosine-weighted probability */
		sphrand(&ray.dir, 0.98f);
		cgm_vadd(&ray.dir, &n);
		cgm_vnormalize(&ray.dir);

		if(cgm_vdot(&ray.dir, &n) < 0.0f) {
			ray.dir.x = -ray.dir.x;
			ray.dir.y = -ray.dir.y;
			ray.dir.z = -ray.dir.z;
		}

		ray.origin = hit->v.pos;
		ray_trace(&rcol, &ray, pdiff, max_iter - 1);

		color->x += rcol.x * mcol.x;
		color->y += rcol.y * mcol.y;
		color->z += rcol.z * mcol.z;

	} else if(rval <= pdiff + pspec) {
		cgm_vnormalize(&n);
		ray.dir = hit->ray.dir;

		if(!mtl->metal && (transmit = mtrans > 0.0f)) {
			/* calculate fresnel factor */
			fres = fresnel(-cgm_vdot(&hit->ray.dir, &n), mtl->ior);
			if(frand() < fres) {
				goto reflect;
			}

			/* calculate refraction direction */
			if(cgm_vrefract(&ray.dir, &n, mtl->ior) == -1) {
				transmit = 0;
			}
		} else {
reflect:	transmit = 0;
			/* calculate reflection direction */
			cgm_vreflect(&ray.dir, &n);
		}

		/* pick specular direction */
		if(mrough > 0.0f) {
			sphrand(&v, mrough);
			cgm_vadd(&ray.dir, &v);
		}
		cgm_vnormalize(&ray.dir);

		if(transmit) {
			cgm_vcons(&out_n, -n.x, -n.y, -n.z);
		} else {
			out_n = n;
		}
		if(cgm_vdot(&ray.dir, &out_n) > 0.0f) {
			/* only sample rays not crashing back into the surface */
			ray.origin = hit->v.pos;
			ray_trace(&rcol, &ray, pspec, max_iter - 1);

			if(mtl->metal) {
				color->x += rcol.x * mcol.x;
				color->y += rcol.y * mcol.y;
				color->z += rcol.z * mcol.z;
			} else {
				cgm_vadd(color, &rcol);
			}
		}
	}
}
