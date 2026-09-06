#include "cgmath/cgmath.h"
#include "erebus.h"
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
	cgm_vec3 n, vdir, lpos, ldir, lcol = {1, 1, 1};
	cgm_ray sray;

	mtlattr_vec(color, hit->mtl, MATTR_EMIT, &hit->v.tex);

	mtlattr_vec(&col, hit->mtl, MATTR_COLOR, &hit->v.tex);
	cgm_vmul(&col, &scn.ambient);
	cgm_vadd(color, &col);

	if(cgm_vdot(&hit->ray.dir, &hit->v.norm) > 0.0f) {
		cgm_vneg(&hit->v.norm);
	}
	n = hit->v.norm;

	vdir = hit->ray.dir;
	cgm_vneg(&vdir);
	cgm_vnormalize(&vdir);

	/* ... for all lights ... */
	cgm_vcons(&lpos, 0, 1000, 0);
	ldir = lpos; cgm_vsub(&ldir, &hit->v.pos);

	sray.origin = hit->v.pos;
	sray.dir = ldir;
	if(!ray_scene(&sray, &scn, 1.0f, 0)) {
		cgm_vnormalize(&ldir);

		lambert_brdf.eval(&col, n, ldir, vdir, hit);
		cgm_vmul(&col, &lcol);
		cgm_vadd(&diffuse, &col);

		phong_brdf.eval(&col, n, ldir, vdir, hit);
		cgm_vmul(&col, &lcol);
		cgm_vadd(&specular, &col);
	}

	cgm_vadd(color, &diffuse);
	cgm_vadd(color, &specular);
}
