#include "cgmath/cgmath.h"
#include "erebus.h"
#include "rt.h"
#include "scene.h"
#include "brdf.h"

void rt_render_tile(struct tile *tile);
void rt_bgcolor(cgm_vec3 *color, cgm_ray *ray);
void rt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int max_iter);

struct renderer rt_renderer = {
	"ray tracer",
	bgcolor,
	rt_shade
};

void rt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int max_iter)
{
	cgm_vec3 col, diffuse = {0}, specular = {0};
	cgm_vec3 ks;
	cgm_vec3 n, vdir, lpos, ldir;
	cgm_ray ray;
	float refl, trans, fres;
	struct material *mtl = hit->mtl;
	struct light *lt;

	mtlattr_vec(color, mtl, MATTR_EMIT, &hit->v.tex);

	mtlattr_vec(&col, mtl, MATTR_COLOR, &hit->v.tex);
	cgm_vmul(&col, &scn.ambient);
	cgm_vadd(color, &col);

	if(cgm_vdot(&hit->ray.dir, &hit->v.norm) > 0.0f) {
		cgm_vneg(&hit->v.norm);
	}
	n = hit->v.norm;

	vdir = hit->ray.dir;
	cgm_vneg(&vdir);
	cgm_vnormalize(&vdir);

	lt = scn.lightlist;
	while(lt) {
		if(lt->rad < 1e-5f) {
			lpos = lt->pos;
		} else {
			sphrand(&lpos, lt->rad);
			cgm_vadd(&lpos, &lt->pos);
		}
		ldir = lpos; cgm_vsub(&ldir, &hit->v.pos);

		ray.origin = hit->v.pos;
		ray.dir = ldir;
		if(!ray_scene(&ray, &scn, 1.0f, 0)) {
			cgm_vnormalize(&ldir);

			lambert_brdf.eval(&col, n, ldir, vdir, hit);
			cgm_vmul(&col, &lt->color);
			cgm_vadd(&diffuse, &col);

			phong_brdf.eval(&col, n, ldir, vdir, hit);
			cgm_vmul(&col, &lt->color);
			cgm_vadd(&specular, &col);
		}

		lt = lt->next;
	}

	mtlattr_vec(&ks, mtl, MATTR_SPECULAR, &hit->v.tex);

	if((refl = mtlattr_num(mtl, MATTR_REFLECT, &hit->v.tex)) > 1e-4f) {
		ray.origin = hit->v.pos;
		ray.dir = hit->ray.dir;
		cgm_vreflect(&ray.dir, &n);

		if(!mtl->metal) {
			fres = fresnel(cgm_vdot(&vdir, &n), mtl->ior);
			ray_trace(&col, &ray, refl * fres, max_iter - 1);
		} else {
			ray_trace(&col, &ray, refl, max_iter - 1);
		}

		cgm_vmul(&col, &ks);
		cgm_vadd(&specular, &col);
	}

	if((trans = mtlattr_num(mtl, MATTR_TRANSMIT, &hit->v.tex)) > 1e-4f) {
		ray.origin = hit->v.pos;
		ray.dir = hit->ray.dir;
		cgm_vrefract(&ray.dir, &n, mtl->ior);

		ray_trace(&col, &ray, trans, max_iter - 1);

		cgm_vmul(&col, &ks);
		cgm_vadd(&specular, &col);
	}

	cgm_vadd(color, &diffuse);
	cgm_vadd(color, &specular);
	cgm_vscale(color, energy);
}
